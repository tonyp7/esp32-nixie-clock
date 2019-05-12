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
static const char TAG[] = "ds3231";


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

void ds3231_i2c_init(){
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_MASTER_RX_BUF_DISABLE,
					   I2C_MASTER_TX_BUF_DISABLE, 0);
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


uint8_t ds3231_dec2bcd(uint8_t i){
	 return i + 6 * (i / 10);
}
uint8_t ds3231_bcd2dec(uint8_t i){
	return i - 6 * (i >> 4);
}
