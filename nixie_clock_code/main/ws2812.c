#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <soc/rmt_struct.h>
#include <soc/dport_reg.h>
#include <driver/gpio.h>
#include <soc/gpio_sig_map.h>
#include <esp_intr.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <driver/rmt.h>
#include <math.h>

#include "ws2812.h"

#define ETS_RMT_CTRL_INUM	18
#define ESP_RMT_CTRL_DISABLE	ESP_RMT_CTRL_DIABLE /* Typo in esp_intr.h */

#define DIVIDER		4 /* Above 4, timings start to deviate*/
#define DURATION	12.5 /* minimum time of a single RMT duration
				in nanoseconds based on clock */

#define PULSE_T0H						(  350 / (DURATION * DIVIDER));
#define PULSE_T1H						(  900 / (DURATION * DIVIDER));
#define PULSE_T0L						(  900 / (DURATION * DIVIDER));
#define PULSE_T1L						(  350 / (DURATION * DIVIDER));
#define PULSE_TRS						(50000 / (DURATION * DIVIDER));

#define MAX_PULSES						32

#define WS2812_RMT_CHANNEL				0

typedef union {
  struct {
    uint32_t duration0:15;
    uint32_t level0:1;
    uint32_t duration1:15;
    uint32_t level1:1;
  };
  uint32_t val;
} rmt_pulse_pair_t;

static uint8_t *ws2812_buffer = NULL;
static unsigned int ws2812_pos, ws2812_len, ws2812_half;
static xSemaphoreHandle ws2812_sem = NULL;
static intr_handle_t rmt_intr_handle = NULL;
static rmt_pulse_pair_t ws2812_bits[2];



float clamp(float d, float min, float max) {
	const float t = d < min ? min : d;
	return t > max ? max : t;
}



float impulse( float k, float x ){
    float h = k*x;
    return h*expf(1.0f-h);
}

/**
 * @brief exponential step interp. Typical k: 10.0f. Typical n: 1.0f
 */
float exp_step(float x, float k, float n){
	return expf( -k * powf(x, n) );
}

/**
 * @brief smoothstep performs smooth Hermite interpolation between 0 and 1 when edge0 < x < edge1.
 * This is useful in cases where a threshold function with a smooth transition is desired.
 */
float smoothstep(const float edge0, const float edge1, const float x){
	const float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}


void ws2812_init_rmt_channel(int rmt_channel)
{
  RMT.apb_conf.fifo_mask = 1;  //enable memory access, instead of FIFO mode.
  RMT.apb_conf.mem_tx_wrap_en = 1; //wrap around when hitting end of buffer
  RMT.conf_ch[rmt_channel].conf0.div_cnt = DIVIDER;
  RMT.conf_ch[rmt_channel].conf0.mem_size = 1;
  RMT.conf_ch[rmt_channel].conf0.carrier_en = 0;
  RMT.conf_ch[rmt_channel].conf0.carrier_out_lv = 1;
  RMT.conf_ch[rmt_channel].conf0.mem_pd = 0;

  RMT.conf_ch[rmt_channel].conf1.rx_en = 0;
  RMT.conf_ch[rmt_channel].conf1.mem_owner = 0;
  RMT.conf_ch[rmt_channel].conf1.tx_conti_mode = 0;    /* loop back mode */
  RMT.conf_ch[rmt_channel].conf1.ref_always_on = 1;    /* use APB 80Mhz clock */
  RMT.conf_ch[rmt_channel].conf1.idle_out_en = 1;
  RMT.conf_ch[rmt_channel].conf1.idle_out_lv = 0;

  return;
}

void ws2812_copy()
{
  unsigned int i, j, offset, len, bit;


  offset = ws2812_half * MAX_PULSES;
  ws2812_half = !ws2812_half;

  len = ws2812_len - ws2812_pos;
  if (len > (MAX_PULSES / 8))
    len = (MAX_PULSES / 8);

  if (!len) {
    for (i = 0; i < MAX_PULSES; i++)
      RMTMEM.chan[WS2812_RMT_CHANNEL].data32[i + offset].val = 0;
    return;
  }

  for (i = 0; i < len; i++) {
    bit = ws2812_buffer[i + ws2812_pos];
    for (j = 0; j < 8; j++, bit <<= 1) {
      RMTMEM.chan[WS2812_RMT_CHANNEL].data32[j + i * 8 + offset].val =
	ws2812_bits[(bit >> 7) & 0x01].val;
    }
    if (i + ws2812_pos == ws2812_len - 1)
      RMTMEM.chan[WS2812_RMT_CHANNEL].data32[7 + i * 8 + offset].duration1 = PULSE_TRS;
  }

  for (i *= 8; i < MAX_PULSES; i++)
    RMTMEM.chan[WS2812_RMT_CHANNEL].data32[i + offset].val = 0;

  ws2812_pos += len;
  return;
}

void ws2812_handle_interrupt(void *arg){
	BaseType_t task_awoken = 0;

  if (RMT.int_st.ch0_tx_thr_event) {
    ws2812_copy();
    RMT.int_clr.ch0_tx_thr_event = 1;
  }
  else if (RMT.int_st.ch0_tx_end && ws2812_sem) {
    xSemaphoreGiveFromISR(ws2812_sem, &task_awoken);
    RMT.int_clr.ch0_tx_end = 1;
  }

  return;
}

void ws2812_init(){
	DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_RMT_CLK_EN);
	DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_RMT_RST);

	rmt_set_pin((rmt_channel_t)WS2812_RMT_CHANNEL, RMT_MODE_TX, (gpio_num_t)WS2818_DATA_GPIO);

	ws2812_init_rmt_channel(WS2812_RMT_CHANNEL);

	RMT.tx_lim_ch[WS2812_RMT_CHANNEL].limit = MAX_PULSES;
	RMT.int_ena.ch0_tx_thr_event = 1;
	RMT.int_ena.ch0_tx_end = 1;

	ws2812_bits[0].level0 = 1;
	ws2812_bits[0].level1 = 0;
	ws2812_bits[0].duration0 = PULSE_T0H;
	ws2812_bits[0].duration1 = PULSE_T0L;
	ws2812_bits[1].level0 = 1;
	ws2812_bits[1].level1 = 0;
	ws2812_bits[1].duration0 = PULSE_T1H;
	ws2812_bits[1].duration1 = PULSE_T1L;

	esp_intr_alloc(ETS_RMT_INTR_SOURCE, 0, ws2812_handle_interrupt, NULL, &rmt_intr_handle);

	return;
}

void ws2812_set_colors(unsigned int length, rgb_t *array){
	unsigned int i;


	ws2812_len = (length * 3) * sizeof(uint8_t);
	ws2812_buffer = malloc(ws2812_len);

	for (i = 0; i < length; i++) {
		ws2812_buffer[i * 3 + 0] = array[i].g;
		ws2812_buffer[i * 3 + 1] = array[i].r;
		ws2812_buffer[i * 3 + 2] = array[i].b;
	}

	ws2812_pos = 0;
	ws2812_half = 0;

	ws2812_copy();

	if (ws2812_pos < ws2812_len)
	ws2812_copy();

	ws2812_sem = xSemaphoreCreateBinary();

	RMT.conf_ch[WS2812_RMT_CHANNEL].conf1.mem_rd_rst = 1;
	RMT.conf_ch[WS2812_RMT_CHANNEL].conf1.tx_start = 1;

	xSemaphoreTake(ws2812_sem, portMAX_DELAY);
	vSemaphoreDelete(ws2812_sem);
	ws2812_sem = NULL;

	free(ws2812_buffer);

	return;
}
