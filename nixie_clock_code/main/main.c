/*
Copyright (c) 2017-2019 Tony Pottier

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

@file main.c
@author Tony Pottier
@brief Entry point for the ESP32 application.
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/



#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "cJSON.h"


#include "wifi_manager.h"

#include "clock.h"
#include "i2c.h"
#include "ds3231.h"
#include "display.h"



/* @brief tag used for ESP serial console messages */
static const char TAG[] = "main";

/**
 * @brief RTOS task that periodically prints the heap memory available.
 * @note Pure debug information, should not be ever started on production code! This is an example on how you can integrate your code with wifi-manager
 */
void monitoring_task(void *pvParameter)
{
	for(;;){
		ESP_LOGI(TAG, "free heap: %d",esp_get_free_heap_size());
		vTaskDelay( pdMS_TO_TICKS(30000) );
	}
}


void test_task(void *pvParameter){

	display_init();

	uint16_t shift = 0;
	for(;;){

		uint16_t *vram = display_get_vram();
		vram[0] = (uint16_t)1<<shift;
		vram[1] = (uint16_t)1<<shift;
		vram[2] = (uint16_t)1<<shift;
		vram[3] = (uint16_t)1<<shift;
		vram[4] = (uint16_t)1<<shift;
		vram[5] = (uint16_t)1<<shift;

		shift++;
		if(shift == 10) shift = 0;

		display_write_vram();


		vTaskDelay( pdMS_TO_TICKS(250));
	}

}


void app_main()
{


	/* GPIO init for SPI transactions & GPIOs used to control the display */
	ESP_ERROR_CHECK(display_init());

//	xTaskCreate(&test_task, "test_task", 8192, NULL, CLOCK_TASK_PRIORITY, NULL);

	/* start the wifi manager */
	wifi_manager_start();

	/* register cb for internet connectivity */
	wifi_manager_set_callback(EVENT_STA_GOT_IP, &clock_notify_sta_got_ip);

	/* clock */
	xTaskCreate(&clock_task, "clock_task", 8192, NULL, CLOCK_TASK_PRIORITY, NULL);

	/* your code should go here. Here we simply create a task on core 2 that monitors free heap memory */
	xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);


}
