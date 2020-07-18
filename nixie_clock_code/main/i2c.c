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

@file i2c.c
@author Tony Pottier

Contains helper functions to communicate over the I2C bus

*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"


#include "i2c.h"


esp_err_t i2c_master_init(){
	esp_err_t ret;
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    ret = i2c_param_config(i2c_master_port, &conf);
    if(ret == ESP_OK){
    	return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    }
    return ret;

}


esp_err_t i2c_write_bytes(const uint8_t slave_address, const uint8_t register_address, uint8_t *data, size_t data_len){
	esp_err_t ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, slave_address << 1 | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, register_address, ACK_CHECK_EN);

	for(int i=0; i<data_len; i++){
		i2c_master_write_byte(cmd, data[i], ACK_CHECK_EN);
	}

	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS( 1000 ) );
	i2c_cmd_link_delete(cmd);
	return ret;
}


esp_err_t i2c_read_bytes(const uint8_t slave_address, const uint8_t register_address, uint8_t *data, size_t data_len){

	/* I don't know why you'd ever call read_bytes with only one byte to read but this is an edge case we can easily handle
	 * by re-routing this call.
	 */
	if(data_len == 1) return i2c_read_byte(slave_address, register_address, &data[0]);

	esp_err_t ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, slave_address << 1 | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, register_address, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS( 1000 ) );
	i2c_cmd_link_delete(cmd);
	if (ret != ESP_OK) {
		return ret;
	}

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, slave_address << 1 | READ_BIT, ACK_CHECK_EN);
	for(int i=0; i < data_len-1; i++){
		i2c_master_read_byte(cmd, &data[i], ACK_VAL);
	}
	i2c_master_read_byte(cmd, &data[data_len-1], NACK_VAL);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM , cmd, pdMS_TO_TICKS( 1000 ) );
	i2c_cmd_link_delete(cmd);

	return ret;

}

esp_err_t i2c_read_byte(const uint8_t slave_address, const uint8_t register_address, uint8_t *value){

	esp_err_t ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, slave_address << 1 | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, register_address, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS( 1000 ) );
	i2c_cmd_link_delete(cmd);
	if (ret != ESP_OK) {
		return ret;
	}

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, slave_address << 1 | READ_BIT, ACK_CHECK_EN);
	i2c_master_read_byte(cmd, value, NACK_VAL);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM , cmd, pdMS_TO_TICKS( 1000 ) );
	i2c_cmd_link_delete(cmd);

	return ret;

}


esp_err_t i2c_write_byte(const uint8_t slave_address, const uint8_t register_address, const uint8_t value){
	esp_err_t ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, slave_address << 1 | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, register_address, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS( 1000 ) );
	i2c_cmd_link_delete(cmd);
	return ret;
}
