/*
Copyright (c) 2019 Tony Pottier

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

typedef union {
  struct __attribute__ ((packed)) {
    uint8_t r, g, b;
  };
  uint32_t num;
} rgb_t;

extern esp_err_t ws2812_init();
extern void ws2812_set_colors(unsigned int length, rgb_t *array);

inline rgb_t ws2812_create_rgb(uint8_t r, uint8_t g, uint8_t b){
	rgb_t v;

	v.r = r;
	v.g = g;
	v.b = b;
	return v;
}




#endif /* MAIN_WS2812_H_ */
