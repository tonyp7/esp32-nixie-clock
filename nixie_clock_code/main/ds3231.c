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

@file ds3231.c
@author Tony Pottier
@brief Defines all functions necessary to query a DS3231 IC

@see https://idyl.io
@see https://github.com/tonyp7/esp32-nixie-clock

*/

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"


#include "i2c.h"
#include "ds3231.h"


/* @brief tag used for ESP serial console messages */
static const char TAG[] = "DS3231";


uint8_t ds3231_time_registers_values[DS3231_TIME_REGISTERS_COUNT];

/* @brief configures the ds3231 to output a square wave on its INT/SQW pin
 *
 * Default CONTROL_REGISTER (0x0E) values
 * 			BIT 7	BIT 6	BIT 5	BIT 4	BIT 3	BIT 2	BIT 1	BIT 0
	NAME:	EOSC	BBSQW	CONV	RS2		RS1		INTCN	A2IE	A1IE
	POR:	0		0		0		1		1		1		0		0
 *
 * */
esp_err_t ds3231_enable_square_wave(){

	/* to generate a 1Hz SQW, RS2=RS1=0, and INTCN needs to be set to 0 */
	const uint8_t register_value = 0x00;

	return i2c_write_byte(DS3231_ADDR, DS3231_CONTROL_REGISTER, register_value);



}

/**
 * @brief get the temperature currently read by the ds3231.
 * Temperature is split in two registers:
 * Address 0x11: MSB Bit[7];Sign Bits[6..0]:Data
 * Address 0x12: LSB Bits[7..6]:Data
 * Temperature Registers (11h–12h)
 * Temperature  is  represented  as  a  10-bit  code  with  a resolution of 0.25°C and is accessible at location 11h and
 * 12h. The  temperature  is  encoded  in  two’s  complement format. The upper 8 bits, the integer portion, are at loca
  * tion 11h and the lower 2 bits, the fractional portion, are in the upper nibble at location 12h. For example,
  * 00011001 01b = +25.25°C. Upon power reset, the registers are set  to  a  default  temperature  of  0°C  and  the
  * controller  starts  a temperature  conversion.  The  temperature  is  read  on  initial application of VCC or I2C access
  * on VBAT and once every  64  seconds  afterwards.  The  temperature  registers are  updated  after  each  user-initiated
  * conversion  and  on every  64-second  conversion.  The  temperature  registers are read-only.
  * Temp conversion time - Typical: 125ms. Max: 200ms.
 */
float ds3231_get_temperature(){

	union int16 {
	        int16_t i16;
	        uint8_t ui8[2];
	} rtc_temp;

	int ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, DS3231_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, DS3231_TEMP_MSB_REGISTER, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Error addressing ds3231 in func call ds3231_get_temperature");
		return 0.0f;
	}

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, DS3231_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
	i2c_master_read_byte(cmd, &rtc_temp.ui8[1], ACK_VAL); /* Order in which to store HIGH and LOW bytes depends on processor endianness. This code is esp32 specific*/
	i2c_master_read_byte(cmd, &rtc_temp.ui8[0], NACK_VAL);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM , cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);

	return (float)rtc_temp.i16 / 256.0f;


}




esp_err_t ds3231_set_time(const struct tm *timeinfo){



	ds3231_time_registers_values[DS3231_SECONDS_REGISTER] = ds3231_dec2bcd(timeinfo->tm_sec);
	ds3231_time_registers_values[DS3231_MINUTES_REGISTER] = ds3231_dec2bcd(timeinfo->tm_min);
	ds3231_time_registers_values[DS3231_HOURS_REGISTER] = ds3231_dec2bcd(timeinfo->tm_hour);
	ds3231_time_registers_values[DS3231_DAY_REGISTER] = timeinfo->tm_wday + 1; /* day is 1-7 in the ds3231, 0-6 in the time struct. Hence +1. 0=Sunday*/
	ds3231_time_registers_values[DS3231_DATE_REGISTER] = ds3231_dec2bcd(timeinfo->tm_mday);
	ds3231_time_registers_values[DS3231_MONTH_REGISTER] = ds3231_dec2bcd(timeinfo->tm_mon + 1); /* month is 1-12 in the ds3231, 0-11 in the time struct. Hence +1 */

	if(timeinfo->tm_year >= 100){
		ds3231_time_registers_values[DS3231_YEAR_REGISTER] = ds3231_dec2bcd(timeinfo->tm_year - 100);
		ds3231_time_registers_values[DS3231_MONTH_REGISTER] |= 0x80; /* set century bit => 2000s */
	}
	else{
		ds3231_time_registers_values[DS3231_YEAR_REGISTER] = ds3231_dec2bcd(timeinfo->tm_year);
	}

	esp_err_t ret = i2c_write_bytes(DS3231_ADDR, DS3231_SECONDS_REGISTER, ds3231_time_registers_values, DS3231_TIME_REGISTERS_COUNT);

	return ret;
}

esp_err_t ds3231_get_time(struct tm *timeinfo){

	esp_err_t ret = i2c_read_bytes(DS3231_ADDR, DS3231_SECONDS_REGISTER, ds3231_time_registers_values,DS3231_TIME_REGISTERS_COUNT);

	if(ret == ESP_OK){

		timeinfo->tm_sec = ds3231_bcd2dec(ds3231_time_registers_values[DS3231_SECONDS_REGISTER]);
		timeinfo->tm_min = ds3231_bcd2dec(ds3231_time_registers_values[DS3231_MINUTES_REGISTER]);
		timeinfo->tm_hour = ds3231_bcd2dec(ds3231_time_registers_values[DS3231_HOURS_REGISTER]);
		timeinfo->tm_wday = ds3231_time_registers_values[DS3231_DAY_REGISTER] - 1; /* day of the week, called DAY register in the DS3231 ranges from 1 to 7. WDAY is 0-6 format. 0=Sunday */
		timeinfo->tm_mday = ds3231_bcd2dec(ds3231_time_registers_values[DS3231_DATE_REGISTER]); /*day of the month 1-31, called the DATE register in the DS3231*/
		timeinfo->tm_mon = ds3231_bcd2dec(0x1f & ds3231_time_registers_values[DS3231_MONTH_REGISTER]) - 1; /* month is 1-12 in the ds3231, 0-11 in the time struct. Hence -1. 0x1f mask is to remove century information */
		timeinfo->tm_year = ds3231_bcd2dec(ds3231_time_registers_values[DS3231_YEAR_REGISTER]); /* year is 0-99, struct is from 1900 so 0 or 100 is added depending on century bit */
		timeinfo->tm_year += (0x80 & ds3231_time_registers_values[DS3231_MONTH_REGISTER])?100:0; /* century bit means we are in the 2000s*/
		timeinfo->tm_isdst = 0;

		ESP_LOGI(TAG, "READ: YEAR:%d MONTH:%d DAY:%d [WDAY:%d] - %d:%d:%d", timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_wday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	}

	return ret;

}




void ds3231_task(void *pvParameter){

	ds3231_i2c_init();

	for(;;){

		int ret;
		uint8_t data_h = 0, data_l = 0;

		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, DS3231_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
		i2c_master_write_byte(cmd, 0x00, ACK_CHECK_EN);
		i2c_master_stop(cmd);
		ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
		i2c_cmd_link_delete(cmd);
		if (ret != ESP_OK) {
			printf("ERROR\n");
			return;
		}
		vTaskDelay(1 / portTICK_RATE_MS);
		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, DS3231_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
		i2c_master_read_byte(cmd, &data_h, ACK_VAL);
		i2c_master_read_byte(cmd, &data_l, NACK_VAL);
		i2c_master_stop(cmd);
		ret = i2c_master_cmd_begin(I2C_MASTER_NUM , cmd, 1000 / portTICK_RATE_MS);
		i2c_cmd_link_delete(cmd);


		//printf("%d%d - ", ( (data_h>>4) & 0x0f),  (data_h & 0x0f));
		//printf("temp: %.2f\n", ds3231_get_temperature());
		ESP_LOGE(TAG, "temp: %.2f", ds3231_get_temperature());
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}



uint8_t ds3231_bcd2dec (uint8_t val) { return val - 6 * (val >> 4); }
uint8_t ds3231_dec2bcd (uint8_t val) { return val + 6 * (val / 10); }


