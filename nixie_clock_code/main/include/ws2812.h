/**
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

@file ws2812.h
@author Tony Pottier
@brief Provides a WS2812 "neopixel" software driver

@see https://idyl.io
@see https://github.com/tonyp7/esp32-nixie-clock

This is based of FozzTexx's public domain code on WS2812
@see https://github.com/FozzTexx/ws2812-demo

*/

#ifndef MAIN_WS2812_H_
#define MAIN_WS2812_H_

#include <stdint.h>

#define WS2818_DATA_GPIO		23
#define WS2812_STRIP_SIZE		6


/**
 * structure holding a RGB 24 bit colors information in a 32 bit int
 */
typedef union {
  struct __attribute__ ((packed)) {
    uint8_t r, g, b;
  };
  uint32_t num;
} rgb_t;


/**
 * @brief defines the type of message to be sent to the queue.
 * The WS2812 queue can only process color information for simplicity
 */
typedef struct ws2812_message_t {
	rgb_t rgb;
} ws2812_message_t;

esp_err_t ws2812_init();



/**
 * @brief Helper to generate a rgb_t based on r, g, b parameters
 */
inline rgb_t ws2812_create_rgb(uint8_t r, uint8_t g, uint8_t b){
	rgb_t v;

	v.r = r;
	v.g = g;
	v.b = b;
	return v;
}

/**
 * @brief Sends the message to the ws2812 queue to process a color change
 * @return ESP_OK if success, ESP_FAIL if the queue was full or any other error
 */
esp_err_t ws2812_set_backlight_color(rgb_t c);

/**
 * @brief helper for ws2812_set_backlight_color
 * @see ws2812_set_backlight_color
 */
inline esp_err_t ws2812_set_backlight_color_rgb(uint8_t r, uint8_t g, uint8_t b){
	return ws2812_set_backlight_color(ws2812_create_rgb(r, g, b));
}


void ws2812_set_colors(unsigned int length, rgb_t *array);




#endif /* MAIN_WS2812_H_ */
