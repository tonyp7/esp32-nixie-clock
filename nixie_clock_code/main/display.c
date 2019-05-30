/*
 * display.c
 *
 *  Created on: 28 May 2019
 *      Author: tony
 */

#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"


#include "display.h"


static uint16_t *display_vram;
static spi_device_handle_t spi;
static spi_transaction_t t;

esp_err_t display_init(){

	esp_err_t ret;

	display_vram = (uint16_t*)malloc(sizeof(uint16_t) * 6);

	gpio_set_direction(DISPLAY_SPI_CS_GPIO, GPIO_MODE_OUTPUT);

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
	ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
	assert(ret==ESP_OK);

	/* Attach the display to the SPI bus */
	ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);

	assert(ret==ESP_OK);


	/* prepare transaction */
	memset(&t, 0x00, sizeof(t));
	t.length= 6 * 16; /* 6 digits, 16 bits per digits */
	t.tx_buffer = display_vram;
	t.user=NULL;

	return ret;
}
