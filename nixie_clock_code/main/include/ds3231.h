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

@file ds3231.h
@author Tony Pottier
@brief Defines all functions necessary to query a DS3231 IC

@see https://idyl.io
@see https://github.com/tonyp7/esp32-nixie-clock

*/

#ifndef MAIN_DS3231_H_
#define MAIN_DS3231_H_

#include <esp_err.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DS3231_ADDR						   	0x68


#define DS3231_SECONDS_REGISTER				0x00
#define DS3231_MINUTES_REGISTER				0x01
#define DS3231_HOURS_REGISTER				0x02
#define DS3231_DAY_REGISTER					0x03
#define DS3231_DATE_REGISTER				0x04
#define DS3231_MONTH_REGISTER				0x05
#define DS3231_YEAR_REGISTER				0x06
#define DS3231_TIME_REGISTERS_COUNT			7			/* 7 registers from 0x00 to 0x06 */

#define DS3231_CONTROL_REGISTER				0x0E
#define DS3231_CONTROL_STATUS_REGISTER		0x0F
#define DS3231_AGING_OFFSET_REGISTER		0x10
#define DS3231_TEMP_MSB_REGISTER			0x11
#define DS3231_TEMP_LSB_REGISTER			0x12



void ds3231_task(void *pvParameter);

esp_err_t ds3231_enable_square_wave();

float ds3231_get_temperature();
void ds3231_i2c_init();
int ds3231_gettimeofday();
void ds3231_set_datetime(time_t datetime);

esp_err_t ds3231_get_time(struct tm *timeinfo);
esp_err_t ds3231_set_time(const struct tm *timeinfo);
uint8_t ds3231_bcd2dec (uint8_t val);
uint8_t ds3231_dec2bcd (uint8_t val);


#ifdef __cplusplus
}
#endif


#endif /* MAIN_DS3231_H_ */
