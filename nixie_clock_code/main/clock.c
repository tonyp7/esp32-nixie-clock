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
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"
#include "esp_http_client.h"
#include "cJSON.h"

#include "clock.h"
#include "ds3231.h"
#include "i2c.h"
#include "http_client.h"



#define CLOCK_SQW_INTERRUPT_GPIO



/* @brief tag used for ESP serial console messages */
static const char TAG[] = "clock";

/* @brief flash memory namespace for the clock */
const char clock_nvs_namespace[] = "clock";

/* @brief storage for timestamp transitions */
static transition_t transitions[CLOCK_MAX_TRANSITIONS];

/* @brief mutex to prevent several read/write to nvs at the same time */
static SemaphoreHandle_t clock_nvs_mutex = NULL;

time_t timestamp_utc;
time_t timestamp_local;
struct tm *clock_time_tm_ptr; /*used for localtime function which returns a static pointer */
struct tm clock_time_tm;
static timezone_t clock_timezone;

static QueueHandle_t clock_queue = NULL;
static bool time_set = false;


time_t clock_get_current_time_utc(){
	return timestamp_utc;
}

timezone_t clock_get_current_timezone(){
	return clock_timezone;
}

void clock_notify_sta_got_ip(void* pvArgument){
	if(clock_queue){
		clock_queue_message_t msg;
		msg.message = CLOCK_MESSAGE_STA_GOT_IP;
		xQueueSend(clock_queue, &msg, portMAX_DELAY);
		//xQueueSendFromISR(clock_queue, &message, NULL);
	}
}
void clock_notify_sta_disconnected(){
	if(clock_queue){
		clock_queue_message_t msg;
		msg.message = CLOCK_MESSAGE_STA_DISCONNECTED;
		xQueueSend(clock_queue, &msg, portMAX_DELAY);
	}
}

void clock_notify_time_api_response(cJSON *json){
	if(clock_queue){
		clock_queue_message_t msg;
		msg.message = CLOCK_MESSAGE_RECEIVE_TIME_API;
		msg.param = (void*)json;
		xQueueSend(clock_queue, &msg, portMAX_DELAY);
	}
}


void clock_notify_transitions_api_response(cJSON *json){
	if(clock_queue){
		clock_queue_message_t msg;
		msg.message = CLOCK_MESSAGE_RECEIVE_TRANSITIONS_API;
		msg.param = (void*)json;
		xQueueSend(clock_queue, &msg, portMAX_DELAY);
	}
}


void clock_change_timezone(timezone_t tz){

}





static void IRAM_ATTR gpio_isr_handler(void* arg){
    //uint32_t gpio_num = (uint32_t) arg;

	clock_queue_message_t msg;
	msg.message = CLOCK_MESSAGE_TICK;

    xQueueSendFromISR(clock_queue, &msg, NULL);
	return;

}


void clock_tick(){
	/* +1 second */
	timestamp_local = ++timestamp_utc + clock_timezone.offset;
	clock_time_tm_ptr = localtime(&timestamp_local);
}



bool clock_realign(time_t new_t){

	double d = fabs(difftime(timestamp_utc, new_t));
	if(d > CLOCK_MAX_ACCEPTABLE_TIME_DRIFT){
		ESP_LOGI(TAG, "Re-alignment of the clock");
		timestamp_utc = new_t;

		/* set time on RTC */
		ESP_ERROR_CHECK(ds3231_set_time(localtime(&timestamp_utc)));

		return true;
	}

	return false;

}


void clock_register_sqw_interrupt(){
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
}



esp_err_t clock_get_nvs_timezone(timezone_t *tz){
	nvs_handle handle;
	esp_err_t esp_err;

	esp_err = nvs_open(clock_nvs_namespace, NVS_READONLY, &handle);
	if(esp_err == ESP_OK){
		ESP_LOGI(TAG, "Getting timezone from NVS memory");

		/* timezone name */
		size_t sz = sizeof(timezone_t);
		esp_err = nvs_get_blob(handle, "tz", tz, &sz);
		if(esp_err != ESP_OK){
			return esp_err;
		}

		nvs_close(handle);
	}
	else if(esp_err == ESP_ERR_NVS_NOT_FOUND){
		/* first time launching the program! */
		ESP_LOGI(TAG, "Cannot find clock nvs namespace. Attempt to create it");
		nvs_close(handle);

		esp_err = nvs_open(clock_nvs_namespace, NVS_READWRITE, &handle);
		if(esp_err == ESP_OK){

			size_t sz = sizeof(timezone_t);
			ESP_ERROR_CHECK(nvs_set_blob(handle, "tz", tz, sz));
			ESP_ERROR_CHECK(nvs_commit(handle));
			nvs_close(handle);
		}

	}

	return esp_err;

}



void clock_save_timezone_task(void *pvParameter){
	if(clock_nvs_lock( pdMS_TO_TICKS( 10000 ) )){
		clock_save_timezone(&clock_timezone);
		clock_nvs_unlock();
	}

	vTaskDelete( NULL );
}


bool clock_nvs_lock(TickType_t xTicksToWait){
	if(clock_nvs_mutex){
		if( xSemaphoreTake( clock_nvs_mutex, xTicksToWait ) == pdTRUE ) {
			return true;
		}
		else{
			return false;
		}
	}
	else{
		return false;
	}

}
void clock_nvs_unlock(){
	xSemaphoreGive( clock_nvs_mutex );
}

esp_err_t clock_save_timezone(timezone_t *tz){
	nvs_handle handle;
	esp_err_t esp_err;


	esp_err = nvs_open(clock_nvs_namespace, NVS_READWRITE, &handle);
	if(esp_err == ESP_OK){

		/* set timezone */
		size_t sz = sizeof(timezone_t);
		esp_err = nvs_set_blob(handle, "tz", tz, sz);
		if(esp_err != ESP_OK){
			ESP_LOGE(TAG, "nvs_set_blob failed with error: %d", esp_err);
			return esp_err;
		}

		esp_err = nvs_commit(handle);
		if (esp_err != ESP_OK) return esp_err;

		nvs_close(handle);

	}
	else{
		ESP_LOGE(TAG, "Could not get a handle to NVS as NVS_READWRITE");
	}

	return esp_err;
}

/**
 * @brief this is the main RTOS task that controls everything in the clock
 */
void clock_task(void *pvParameter){


	/* debug, should be cut in prod code */
	char strftime_buf[64];

	/* memory init */
	memset(&transitions, 0x00, sizeof(CLOCK_MAX_TRANSITIONS) * CLOCK_MAX_TRANSITIONS);
	clock_nvs_mutex = xSemaphoreCreateMutex();

	/* register clock queue */
	clock_queue = xQueueCreate(10, sizeof(clock_queue_message_t));

	/* initialized I2C */
	ESP_ERROR_CHECK(i2c_master_init());

	/* enabled squre wave */
	ESP_ERROR_CHECK(ds3231_enable_square_wave());

	/* HTTP client is needed for the clock task */
	http_client_init();

	/* get RTC time */
	memset(&clock_time_tm, 0x00, sizeof(struct tm));
	ESP_ERROR_CHECK(ds3231_get_time(&clock_time_tm));
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &clock_time_tm);
	ESP_LOGI(TAG, "The current RTC time is: %s", strftime_buf);
	if(clock_time_tm.tm_year < 71){ /* 1971 */
		/* time is not available: first run or battery of the RTC was dead */
		timestamp_utc = 1;
	}
	else{
		/* otherwise: we are good to go! */
		timestamp_utc = mktime(&clock_time_tm);
		time_set = true;
	}
	clock_time_tm_ptr = localtime(&timestamp_utc);


	/* init timezone and attempt to get what was saved in nvs */
	clock_timezone.offset = 0;
	strcpy(clock_timezone.name, "UTC");
	ESP_ERROR_CHECK(clock_get_nvs_timezone(&clock_timezone));

	/* register interrupt on the 1Hz sqw signal coming from the DS3231 */
	clock_register_sqw_interrupt();


	clock_queue_message_t msg;

	for(;;) {
		if(xQueueReceive(clock_queue, &msg, pdMS_TO_TICKS(11100))) { /* portMAX_DELAY */

			switch(msg.message){
				case CLOCK_MESSAGE_STA_GOT_IP:
					http_client_get_api_time("Europe/Paris");
					break;
				case CLOCK_MESSAGE_TICK:
					if(time_set){
						clock_tick();
						//strftime(strftime_buf, sizeof(strftime_buf), "%c", clock_time_tm_ptr);
						//ESP_LOGE(TAG, "TICK! date/time is: %s", strftime_buf);
					}
					break;
				case CLOCK_MESSAGE_REQUEST_TRANSITIONS_API_CALL:
					http_client_get_transitions(clock_timezone, timestamp_utc);
					break;
				case CLOCK_MESSAGE_RECEIVE_TRANSITIONS_API:{


					cJSON *json = (cJSON*)msg.param;

					if(json != NULL){
						char *json_str = cJSON_Print(json);
						ESP_LOGI(TAG, "%s", json_str);

						cJSON *transition = NULL;
						cJSON *transitions = cJSON_GetObjectItemCaseSensitive(json, "transitions");
						if(transitions){
							cJSON_ArrayForEach(transition, transitions){
								if(transition){
									cJSON *transitionUnixEpoch = cJSON_GetObjectItemCaseSensitive(transition, "transitionUnixEpoch");
									cJSON *toOffset = cJSON_GetObjectItemCaseSensitive(transition, "toOffset");

									if (transitionUnixEpoch && toOffset && cJSON_IsNumber(transitionUnixEpoch) && cJSON_IsNumber(toOffset)){
										printf("%d - %d\n", transitionUnixEpoch->valueint, toOffset->valueint);
									}
								}
							}
						}

						free(json_str);
						cJSON_Delete(json);

					}
					}
					break;
				case CLOCK_MESSAGE_RECEIVE_TIME_API:
					;cJSON *json = (cJSON*)msg.param;
					bool updateNVS = false;
					if(json){
						char *json_str = cJSON_Print(json);
						ESP_LOGI(TAG, "%s", json_str);

						/* get time, and set it if necessary */
						cJSON *unixEpoch = cJSON_GetObjectItemCaseSensitive(json, "unixEpoch");
						if(cJSON_IsNumber(unixEpoch)){
							time_t t = unixEpoch->valueint;
							clock_realign(t);
							time_set = true;
						}

						/* check timezone */
						cJSON *timezone = cJSON_GetObjectItemCaseSensitive(json, "timezone");
						if(cJSON_IsObject(timezone)){
							cJSON *timezoneString = cJSON_GetObjectItemCaseSensitive(timezone, "timezoneString");
							if(cJSON_IsString(timezoneString) && strcmp(clock_timezone.name, timezoneString->valuestring) != 0){
								/* realign clock_timezone */
								updateNVS = true;
								strcpy(clock_timezone.name, timezoneString->valuestring);
								ESP_LOGI(TAG, "Timezone set to: %s", clock_timezone.name);
							}

							cJSON *offset = cJSON_GetObjectItemCaseSensitive(timezone, "offset");
							if(cJSON_IsNumber(offset) && clock_timezone.offset != offset->valueint){
								/* realign offset if needed */
								updateNVS = true;
								clock_timezone.offset = offset->valueint;
								ESP_LOGI(TAG, "Offset set to: %d", clock_timezone.offset);
							}
						}

						/* memory clean up */
						free(json_str);
						cJSON_Delete(json);

						/* NVS needs to updated: spawn a low priority task that'll take care of it when possible */
						if(updateNVS){
							xTaskCreate(&clock_save_timezone_task, "ck_save_tz", 4096, NULL, 1, NULL);
						}

						/* finally, enqueue a transitions with this timezone */
						clock_queue_message_t m;
						m.message = CLOCK_MESSAGE_REQUEST_TRANSITIONS_API_CALL;
						m.param = NULL;
						xQueueSend(clock_queue, &m, portMAX_DELAY);




					}
					break;
				default:
					break;
			}

		}
		taskYIELD();
	}

	for(;;){

		ESP_LOGE(TAG, "temp: %.2f", ds3231_get_temperature());
		vTaskDelay(1000 / portTICK_PERIOD_MS);

	}

	vTaskDelete( NULL );

}
