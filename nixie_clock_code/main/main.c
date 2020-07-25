/*
Copyright (c) 2017-2019 Tony Pottier

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

@file main.c
@author Tony Pottier
@brief Entry point for the ESP32 application.
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/



#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "cJSON.h"


#include "wifi_manager.h"

#include "clock.h"
#include "i2c.h"
#include "ds3231.h"
#include "display.h"
#include "ws2812.h"
#include "webapp.h"



/* @brief tag used for ESP serial console messages */
static const char TAG[] = "main";

/**
 * @brief RTOS task that periodically prints the heap memory available.
 * @note Pure debug information, should not be ever started on production code! This is an example on how you can integrate your code with wifi-manager
 */
void monitoring_task(void *pvParameter)
{
	for(;;){
		ESP_LOGI(TAG, "free heap: %d",esp_get_free_heap_size());
		vTaskDelay( pdMS_TO_TICKS(30000) );
	}
}



void bitcode(uint8_t r, uint8_t g, uint8_t b, uint8_t* buffer){
  //order is G R B

  //0 => 100
  //1 => 110

  uint8_t spidata[9];
  uint32_t encoding = 0;

  memset(spidata, 0, 9);

  for(int i=0;i<8;i++){

    if(g & (uint8_t)0x80){
      encoding |= 0b110;
    }
    else{
      encoding |= 0b110;
    }

    encoding <<= 3;
    g <<= 1;
  }

  spidata[0] = ((encoding >> 16) & 0xff);
  spidata[1] = ((encoding >> 8) & 0xff);
  spidata[2] = (encoding & 0xff);

  encoding = 0;
  for(int i=0;i<8;i++){

    if(r & (uint8_t)0x80){
      encoding |= 0b110;
    }
    else{
      encoding |= 0b110;
    }

    encoding <<= 3;
    r <<= 1;
  }

  spidata[3] = ((encoding >> 16) & 0xff);
  spidata[4] = ((encoding >> 8) & 0xff);
  spidata[5] = (encoding & 0xff);

  encoding = 0;
  for(int i=0;i<8;i++){

    if(b & (uint8_t)0x80){
      encoding |= 0b110;
    }
    else{
      encoding |= 0b110;
    }

    encoding <<= 3;
    b <<= 1;
  }

  spidata[6] = ((encoding >> 16) & 0xff);
  spidata[7] = ((encoding >> 8) & 0xff);
  spidata[8] = (encoding & 0xff);

  memcpy(buffer, spidata, 9);
  

}

void rainbow_task2(void *pvParameter){

	//esp_err_t ret;

  spi_device_handle_t spi;
  spi_transaction_t t;

	/* setup all GPIOs */
	gpio_set_direction(WS2818_DATA_GPIO, GPIO_MODE_OUTPUT);

	spi_bus_config_t buscfg={
		.miso_io_num=-1, /* -1 == not used */
		.mosi_io_num=WS2818_DATA_GPIO,
		.sclk_io_num=-1,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1
	};
	spi_device_interface_config_t devcfg={
		.clock_speed_hz=3000000,					/* clock at 3Mhz => about 1000 frames per seconds! */
		.mode=0,                    			/* SPI mode 0 */
		.spics_io_num=-1,      /* CS pin CS GPIO pin for this device, or -1 if not used. */
		.queue_size=4                           /* Queue size is not really relevant for the nixie display so its kept low */
		//.pre_cb=ili_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
	};


	/* Initialize the SPI bus */
	ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, 2));

	/* Attach the display to the SPI bus */
	ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &spi));

	/* prepare transaction */

  const size_t buff_len = ((6 * 24 * 3)/8);
  uint8_t* buffer = malloc(  sizeof(uint8_t) *  buff_len);

	memset(&t, 0x00, sizeof(t));
  memset(buffer, 0x00, buff_len);

	t.length = 6 * 24 * 3;  /* 6 digits, 24 bits per digits, 3 bit needed for each bit */
	t.tx_buffer = buffer;
	t.user = NULL;



  bitcode(255, 0, 0, buffer);
  bitcode(0, 255, 0, &buffer[9]);
  bitcode(0, 0, 255, &buffer[18]);
  bitcode(255, 0, 0, &buffer[27]);
  bitcode(0, 255, 0, &buffer[36]);
  bitcode(0, 0, 255, &buffer[45]);

  for(;;){

    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));
    vTaskDelay(pdMS_TO_TICKS(5000));
  }



}


void test_task(void *pvParameter){

	display_init();

	uint16_t shift = 0;
	for(;;){

		uint16_t *vram = display_get_vram();
		vram[0] = (uint16_t)1<<shift;
		vram[1] = (uint16_t)1<<shift;
		vram[2] = (uint16_t)1<<shift;
		vram[3] = (uint16_t)1<<shift;
		vram[4] = (uint16_t)1<<shift;
		vram[5] = (uint16_t)1<<shift;

		shift++;
		if(shift == 10) shift = 0;

		display_write_vram();


		vTaskDelay( pdMS_TO_TICKS(250));
	}

}


void app_main()
{


	/* GPIO/RMT init for the WS2812 driver */
	ESP_ERROR_CHECK(ws2812_init());
	
	/* GPIO init for SPI transactions & GPIOs used to control the display */
	ESP_ERROR_CHECK(display_init());

	/* start the wifi manager */
	wifi_manager_start();
  webapp_register_handlers();

	/* register cb for internet connectivity */
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &clock_notify_sta_got_ip);

	/* clock task */
	//xTaskCreatePinnedToCore(&clock_task, "clock_task", 8192, NULL, CLOCK_TASK_PRIORITY, NULL, 1);
  xTaskCreatePinnedToCore(&clock_task, "clock_task", 16384, NULL, CLOCK_TASK_PRIORITY, NULL, 1);

	//xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);
}
