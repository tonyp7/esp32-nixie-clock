/*
 * display.h
 *
 *  Created on: 28 May 2019
 *      Author: tony
 */

#ifndef MAIN_DISPLAY_H_
#define MAIN_DISPLAY_H_

#define	DISPLAY_SPI_CS_GPIO				15
#define DISPLAY_SPI_MOSI_GPIO			13
#define DISPLAY_SPI_SCLK_GPIO			14
#define DISPLAY_OE_GPIO					27
#define DISPLAY_HVEN_GPIO				26


esp_err_t display_init();


#endif /* MAIN_DISPLAY_H_ */
