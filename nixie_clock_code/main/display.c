/*
Copyright (c) 2020 Tony Pottier

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

@file display.c
@author Tony Pottier

Contains functions to control the nixie clock display over SPI

*/

#include <stdlib.h>
#include <string.h>
#include <byteswap.h>

#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_intr_alloc.h>

#include "display.h"


static uint16_t *display_vram;
static spi_device_handle_t spi;
static spi_transaction_t t;



static void IRAM_ATTR gpio_usb_power_isr_handler(void* arg){

	/* disable display when USB is on as a measure of security for the whole board. USB powering HV power supply after going through a tiny diode = bad idea */
	gpio_set_level(DISPLAY_OE_GPIO, 1);
	return;
}


void display_turn_on(){
	/* turn on does not do anything if USB power is connected */
	if(gpio_get_level(DEBUG_USB_POWER_ON_GPIO) == 0){
		gpio_set_level(DISPLAY_HVEN_GPIO, 1);
		gpio_set_level(DISPLAY_OE_GPIO, 0);
	}
}

void display_turn_off(){
	gpio_set_level(DISPLAY_OE_GPIO, 1);
	gpio_set_level(DISPLAY_HVEN_GPIO, 0);
}


esp_err_t display_register_usb_power_interrupt(){

	/* setup DEBUG_USB_POWER_ON_GPIO as INTERRUPT on RISING EGDE */
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
	/* GPIO 4 bit mask */
	io_conf.pin_bit_mask = (1ULL << DEBUG_USB_POWER_ON_GPIO);
	/* Input */
	io_conf.mode = GPIO_MODE_INPUT;
	/* Disable pu/pd as this is tied to a voltage divider from 5V rail */
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	gpio_config(&io_conf);

	/* install ISR service */
	gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
	return gpio_isr_handler_add(DEBUG_USB_POWER_ON_GPIO, gpio_usb_power_isr_handler, (void*)DEBUG_USB_POWER_ON_GPIO);
}



esp_err_t display_init(){

	esp_err_t ret;

	display_vram = (uint16_t*)malloc(sizeof(uint16_t) * DISPLAY_DIGIT_COUNT);
	memset(display_vram, 0x00, sizeof(uint16_t) * DISPLAY_DIGIT_COUNT);

	/* setup all GPIOs */
	gpio_set_direction(DISPLAY_SPI_CS_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_direction(DISPLAY_OE_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_direction(DISPLAY_HVEN_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_direction(DEBUG_USB_POWER_ON_GPIO, GPIO_MODE_INPUT);

	/* by default display is off but HV power supply is enabled, idling */
	gpio_set_level(DISPLAY_OE_GPIO, 1); /* output enable is reversed logic. This effectively has no effect since the pin is physically pulled-up to 3v3 */
	gpio_set_level(DISPLAY_HVEN_GPIO, 1);

	/* register interrupt on USB power */
	//ret = display_register_usb_power_interrupt();
	//if(ret != ESP_OK) return ret;

	spi_bus_config_t buscfg={
		.miso_io_num=-1, /* -1 == not used */
		.mosi_io_num=DISPLAY_SPI_MOSI_GPIO,
		.sclk_io_num=DISPLAY_SPI_SCLK_GPIO,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1
	};
	spi_device_interface_config_t devcfg={
		.clock_speed_hz=100000,					/* clock at 100Khz => about 1000 frames per seconds! */
		.mode=0,                    			/* SPI mode 0 */
		.spics_io_num=DISPLAY_SPI_CS_GPIO,      /* CS pin CS GPIO pin for this device, or -1 if not used. */
		.queue_size=4                           /* Queue size is not really relevant for the nixie display so its kept low */
		//.pre_cb=ili_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
	};


	/* Initialize the SPI bus */
	ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
	if(ret!=ESP_OK) return ret;

	/* Attach the display to the SPI bus */
	ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
	if(ret!=ESP_OK) return ret;

	/* prepare transaction */
	memset(&t, 0x00, sizeof(t));
	t.length = DISPLAY_DIGIT_COUNT * 16; /* 6 digits, 16 bits per digits */
	t.tx_buffer = display_vram;
	t.user = NULL;

	return ret;
}



uint16_t* display_get_vram(){
	return display_vram;
}

esp_err_t display_write_vram(){
	esp_err_t ret;


	/* due to esp32 endianess, we need to swipe bytes. The hardware assumes big endian but the esp is little endian. */
	for(int i=0;i<6;i++){
		display_vram[i] = __bswap_16(display_vram[i]);
	}


	/* safe guard. Enable display only if USB is disconnected */
	if(gpio_get_level(DEBUG_USB_POWER_ON_GPIO) == 0){
		gpio_set_level(DISPLAY_OE_GPIO, 0);
	}

	gpio_set_level(DISPLAY_SPI_CS_GPIO, 0);
	ret=spi_device_transmit(spi, &t);
	gpio_set_level(DISPLAY_SPI_CS_GPIO, 1);

	return ret;
}

esp_err_t display_write_time(struct tm *time){

	if(time){

		/* seconds */
		display_vram[0] =  (uint16_t) (1 << (time->tm_sec % 10));
		display_vram[1] =  (uint16_t) (1 << (time->tm_sec / 10));

		/* minutes */
		display_vram[2] =  (uint16_t) (1 << (time->tm_min % 10));
		display_vram[3] =  (uint16_t) (1 << (time->tm_min / 10));

		/* hours */
		display_vram[4] =  (uint16_t) (1 << (time->tm_hour % 10));
		display_vram[5] =  (uint16_t) (1 << (time->tm_hour / 10));


		if(time->tm_sec % 2 == 0){
			display_vram[2] |= DISPLAY_TOP_DOT_MASK;
			display_vram[2] |= DISPLAY_BOTTOM_DOT_MASK;
			display_vram[4] |= DISPLAY_TOP_DOT_MASK;
			display_vram[4] |= DISPLAY_BOTTOM_DOT_MASK;
		}


		return display_write_vram();
	}
	else{

		display_vram[0] = (uint16_t) (1 << 0);
		display_vram[1] = (uint16_t) (1 << 0);
		display_vram[2] = (uint16_t) (1 << 0);
		display_vram[3] = (uint16_t) (1 << 0);
		display_vram[4] = (uint16_t) (1 << 0);
		display_vram[5] = (uint16_t) (1 << 0);

		return display_write_vram();
	}


}
