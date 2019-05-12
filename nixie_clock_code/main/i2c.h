/*
 * i2c.h
 *
 *  Created on: 11 May 2019
 *      Author: tony
 */

#ifndef MAIN_I2C_H_
#define MAIN_I2C_H_


#define I2C_MASTER_SCL_IO					22               /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO					21               /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM             			I2C_NUM_0        /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE  			0                /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 	 		0                /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ      	   		400000           /*!< I2C master clock frequency */



#define WRITE_BIT                          	I2C_MASTER_WRITE	/*!< I2C master write */
#define READ_BIT                           	I2C_MASTER_READ 	/*!< I2C master read */
#define ACK_CHECK_EN                       	0x1             	/*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                      	0x0              	/*!< I2C master will not check ack from slave */
#define ACK_VAL                            	0x0             	/*!< I2C ack value */
#define NACK_VAL 							0x1 				/*!< I2C nack value */




esp_err_t i2c_master_init();




/**
 * @brief I2C helper function to write a single byte to a register
 *
 */
esp_err_t i2c_write_byte(const uint8_t slave_address, const uint8_t register_address, const uint8_t value);

/**
 * @brief I2C helper function to read a single byte from a register
 *
 */
esp_err_t i2c_read_byte(const uint8_t slave_address, const uint8_t register_address, uint8_t *value);


/**
 * @brief I2C helper function to read bytes starting from specified register
 * @warning: it is the caller responsability to ensure data buffer is big enough to store data_len bytes.
 */
esp_err_t i2c_read_bytes(const uint8_t slave_address, const uint8_t register_address, uint8_t *data, size_t data_len);

#endif /* MAIN_I2C_H_ */
