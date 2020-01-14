/*
 * display.c
 *
 *  Created on: 28 May 2019
 *      Author: tony
 */

#include <stdlib.h>
#include <string.h>
#include <byteswap.h>

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"


#include "display.h"


static uint16_t *display_vram;
static spi_device_handle_t spi;
static spi_transaction_t t;





esp_err_t display_init(){

	esp_err_t ret;

	display_vram = (uint16_t*)malloc(sizeof(uint16_t) * DISPLAY_DIGIT_COUNT);
	memset(display_vram, 0x00, sizeof(uint16_t) * DISPLAY_DIGIT_COUNT);

	gpio_set_direction(DISPLAY_SPI_CS_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_direction(DISPLAY_OE_GPIO, GPIO_MODE_OUTPUT);

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


	/* due to esp32 endianess, we need to swipe bytes */
	for(int i=0;i<6;i++){
		display_vram[i] = __bswap_16(display_vram[i]);
	}

	gpio_set_level(DISPLAY_OE_GPIO, 0);
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
		return ESP_ERR_INVALID_ARG;
	}


}
