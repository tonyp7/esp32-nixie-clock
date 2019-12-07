/*
 * display.h
 *
 *  Created on: 28 May 2019
 *      Author: tony
 */

#ifndef MAIN_DISPLAY_H_
#define MAIN_DISPLAY_H_

#include "time.h"
#include "ws2812.h"

#define	DISPLAY_SPI_CS_GPIO				15
#define DISPLAY_SPI_MOSI_GPIO			13
#define DISPLAY_SPI_SCLK_GPIO			14
#define DISPLAY_OE_GPIO					27
#define DISPLAY_HVEN_GPIO				26

#define DISPLAY_DIGIT_COUNT				6

#define DISPLAY_TOP_DOT_MASK			(uint16_t)(1<<10)
#define DISPLAY_BOTTOM_DOT_MASK			(uint16_t)(1<<11)


typedef enum display_leading_zero_t{
	DISPLAY_LEADING_ZERO_HIDE = 0,
	DISPLAY_LEADING_ZERO_SHOW = 1
}display_leading_zero_t;

typedef enum display_dot_mode_t{
	DISPLAY_DOT_MODE_BLINK_ON_EVEN_SECONDS = 0,
	DISPLAY_DOT_MODE_BLINK_ON_ODDS_SECONDS = 1,
	DISPLAY_DOT_MODE_BLINK_LEFT_RIGHT = 2,
	DISPLAY_DOT_MODE_BLINK_WHEEL_CW = 3,
	DISPLAY_DOT_MODE_PM_INDICATOR = 4,
	DISPLAY_DOT_MODE_PM_INDICATOR_BLINK = 5,
	DISPLAY_DOT_MODE_OFF = 0x7fffffff
}display_dot_mode_t;

typedef struct display_config_t{
	display_dot_mode_t dot_mode;
	display_leading_zero_t leading_zero;
	float led_brightness;
	rgb_t led_color;
}display_config_t;

esp_err_t display_init();
esp_err_t display_write_time(struct tm *time);
uint16_t* display_get_vram();
esp_err_t display_write_vram();



#endif /* MAIN_DISPLAY_H_ */
