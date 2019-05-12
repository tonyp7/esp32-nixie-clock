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

@file clock.c
@author Tony Pottier
@brief Describes the main task driving the rest of the nixie clock

@see https://idyl.io
@see https://github.com/tonyp7/esp32-nixie-clock

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "clock.h"
#include "ds3231.h"
#include "i2c.h"


#define CLOCK_SQW_INTERRUPT_GPIO


/* @brief tag used for ESP serial console messages */
static const char TAG[] = "clock";


struct tm *clock_time_tm;
time_t clock_time;
static QueueHandle_t clock_queue = NULL;


static void IRAM_ATTR gpio_isr_handler(void* arg){
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(clock_queue, &gpio_num, NULL);
}


/**
 * @brief this is the main RTOS task that controls everything in the clock
 */
void clock_task(void *pvParameter){

	/* initialized I2C */
	ESP_ERROR_CHECK(i2c_master_init());

	/* enabled squre wave */
	ESP_ERROR_CHECK(ds3231_enable_square_wave());

	/* register clock queue */
	clock_queue = xQueueCreate(10, sizeof(uint32_t));

	/* setup GPIO 4 as INTERRUPT on RISING EGDE */
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
	/* GPIO 4 bit mask */
	io_conf.pin_bit_mask = (1ULL<<GPIO_INPUT_IO_4);
	/* Input */
	io_conf.mode = GPIO_MODE_INPUT;
	/* No pull up since there is already a 10k external pull up */
	io_conf.pull_up_en = 0;
	gpio_config(&io_conf);


	/* install ISR service */
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	gpio_isr_handler_add(GPIO_INPUT_IO_4, gpio_isr_handler, (void*) GPIO_INPUT_IO_4);

	uint32_t io_num;
	for(;;) {
		if(xQueueReceive(clock_queue, &io_num, portMAX_DELAY)) {
			//printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
			ESP_LOGE(TAG, "tick");
		}
	}

	for(;;){

		ESP_LOGE(TAG, "temp: %.2f", ds3231_get_temperature());
		vTaskDelay(1000 / portTICK_PERIOD_MS);

	}

	vTaskDelete( NULL );

}
