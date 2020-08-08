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


#include "ds3231.h"
#include "i2c.h"
#include "http_client.h"
#include "display.h"
#include "ws2812.h"
#include "list.h"
#include "clock.h"




/** @brief tag used for ESP serial console messages */
static const char TAG[] = "clock";

/** @brief flash memory namespace for the clock */
const char clock_nvs_namespace[] = "clock";

/** @brief storage for timestamp transitions */
static transition_t clock_transitions[CLOCK_MAX_TRANSITIONS];

/** @brief mutex to prevent several read/write to nvs at the same time */
static SemaphoreHandle_t clock_nvs_mutex = NULL;

time_t timestamp_utc;
time_t timestamp_local;
time_t timestamp_transitions_check = 0;
struct tm *clock_time_tm_ptr = NULL; /*used for localtime function which returns a static pointer */
struct tm clock_time_tm;

//static timezone_t clock_timezone;
static clock_config_t clock_config;

static QueueHandle_t clock_queue = NULL;
static bool time_set = false;

/** @brief List holding the sleep/wake events */
static list_t* clock_list_sleepevents = NULL;


/** @brief Save task handle to notify it of saving */
static TaskHandle_t clock_task_save_nvs = NULL;


time_t clock_get_current_time_utc(){
	return timestamp_utc;
}


/**
 * @brief task the will save the config in NVS when it is notified and it is safe to do so
 */
static void clock_save_config_task(void *pvParameter){

	ESP_LOGI(TAG, "clock_save_config_task started");
	for(;;){

		if(ulTaskNotifyTake(pdFALSE, portMAX_DELAY)){

			ESP_LOGI(TAG, "clock_save_config_task received notification");

			clock_config_t cfg = clock_get_config();
			if(clock_nvs_lock( portMAX_DELAY )){
				clock_save_config(&cfg);
				clock_nvs_unlock();
			}
		}
	}

	vTaskDelete( NULL );
}

static int comp_sleep_event(sleep_event_t *a, sleep_event_t *b){

	if(a && b){

		if(a->timestamp == b->timestamp) return 0;
		else if(a->timestamp < b->timestamp) return -1;
		else return 1;
	}
	else{
		return a - b;
	}
}

static void clock_build_new_sleepmodes(sleepmodes_t sleepmodes){

    time_t time_now_utc = clock_get_current_time_utc();
    struct tm tm_now;
    int offset = clock_config.timezone.offset;

    /* initialize the event */
    time_t time_now_local = time_now_utc + offset; /* apply local timezone then build the tm now object */
    gmtime_r(&time_now_local, &tm_now);
	/* gmtime_s(&tm_now, &time_now_local); on Windows */

	/* clear the previous events */
	list_clear(clock_list_sleepevents);

    if (sleepmodes.enable_sleepmode) {

        /* get current day of the week at midnight */
        time_t today_at_midight = time_now_local - (time_t)tm_now.tm_sec - ((time_t)tm_now.tm_min * 60) - ((time_t)tm_now.tm_hour * 3600);

        /* process all the sleepmodes */
        for (int i = 0; i < CLOCK_MAX_SLEEPMODES; i++) {

            /* to save some typing and code clarity */
            sleepmode_t sm = sleepmodes.sleepmode[i];

            /* skip if this one is disabled */
            if (!sm.enabled) continue;

            /* transform a sleepmode into UTC timestamp and add it to the list */

            int weekday = 1; /* the mask starts on mondays which is 1 on a struct tm */
            uint8_t days = sm.days;
            for (int j = 0; j < 7; j++) {
               
                if (days & (uint8_t)0x01) {
                    /* when is the next weekday matching ?
                        tm_wday = 1 (Monday), weekday = 1 => offset is 0
                        tm_wday = 1 (Monday), weekday = 2 => offset is 1 (tomorrow)
                        tm_wday = 3 (Wednesday), weekday = 1 (Next Monday) => offset is  (7-3 + 1) = 5 days
                    */
                    time_t day_offset;
                    if (tm_now.tm_wday <= weekday) {
                        day_offset = ((time_t)weekday - tm_now.tm_wday) * (time_t)86400; /* 24h * 3600 */
                    }
                    else {
                        day_offset = ((time_t)7 - tm_now.tm_wday + weekday) * (time_t)86400; /* 24h * 3600 */
                    }
 
                    /* build both events
                    if the TO hour is less than from, we consider it's for the next day and so add further offset */
                    sleep_event_t from = { .timestamp = today_at_midight + day_offset + sm.from, .action = SLEEP_ACTION_SLEEP };
                    sleep_event_t to = { .timestamp = today_at_midight + day_offset +  ((sm.to < sm.from)?(time_t)86400:0) + sm.to, .action = SLEEP_ACTION_WAKE };

					list_add_ordered(clock_list_sleepevents, from, &comp_sleep_event);
					list_add_ordered(clock_list_sleepevents, to, &comp_sleep_event);

					/* if the day is today we also build the sleep event for next week */
					if(day_offset == 0){
						from.timestamp += (time_t)86400*7;
						to.timestamp += (time_t)86400*7;
						list_add_ordered(clock_list_sleepevents, from, &comp_sleep_event);
						list_add_ordered(clock_list_sleepevents, to, &comp_sleep_event);
					}

					/* debug -- can be deleted in production code */
					struct tm debug;
					static char strftime_buf[64];
					localtime_r(&from.timestamp, &debug);
					strftime(strftime_buf, sizeof(strftime_buf), "%c", &debug);
					ESP_LOGI(TAG, "CLOCK WILL SLEEP AT: %s", strftime_buf);
					localtime_r(&to.timestamp, &debug);
					strftime(strftime_buf, sizeof(strftime_buf), "%c", &debug);
					ESP_LOGI(TAG, "CLOCK WILL WAKE AT: %s", strftime_buf);


                }

                days = days >> 1;
                weekday++; if (weekday == 7) weekday = 0; /* wrap around for sunday */

            } /* for (int j = 0; j < 7; j++) */
        } /* for (int i = 0; i < CLOCK_MAX_SLEEPMODES; i++) */
    } /* if (sleepmodes.enable_sleepmode) */
}


timezone_t clock_get_current_timezone(){
	//return clock_timezone;
	return clock_config.timezone;
}

void clock_notify_sta_got_ip(void *pvArgument){
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

void clock_notify_new_sleepmodes(sleepmodes_t sleepmodes){

	clock_queue_message_t msg;
	sleepmodes_t* sm = malloc(sizeof(sleepmodes_t));
	memset(sm, 0x00, sizeof(sleepmodes_t));
	*sm = sleepmodes;
	msg.message = CLOCK_MESSAGE_SLEEPMODE_CONFIG;
	msg.param = (void*)sm;

	xQueueSend(clock_queue, &msg, portMAX_DELAY);
}


void clock_notify_new_timezone(char* timezone){

	clock_queue_message_t msg;
	timezone_t* tz = malloc(sizeof(timezone_t));
	memset(tz, 0x00, sizeof(timezone_t));
	strncpy(  tz->name, timezone, sizeof(tz->name) - 1 );
	msg.message = CLOCK_MESSAGE_TIMEZONE;
	msg.param = (void*)tz;

	xQueueSend(clock_queue, &msg, portMAX_DELAY);
}


static void IRAM_ATTR gpio_isr_handler(void* arg){
    //uint32_t gpio_num = (uint32_t) arg;

	clock_queue_message_t msg;
	msg.message = CLOCK_MESSAGE_TICK;
	msg.param = NULL;
    xQueueSendFromISR(clock_queue, &msg, NULL);
	return;
}


void clock_transitions_shift_left(){

	int i = CLOCK_MAX_TRANSITIONS - 1;
	while(i > 1){
		clock_transitions[i - 1] = clock_transitions[i];
		i--;
	}
}

void clock_tick(){

	timezone_t* clock_timezone = &(clock_config.timezone);

	/* +1 second */
	timestamp_utc++;

	/* check for transitions */
	int new_offset = clock_timezone->offset;
	while(clock_transitions[0].timestamp && (timestamp_utc >= clock_transitions[0].timestamp)){
		/* save found offset */
		new_offset = clock_transitions[0].offset;

		/* shift transitions table left */
		clock_transitions_shift_left();
	}

	/* found a new timezone offset ? */
	if(new_offset != clock_timezone->offset){
		ESP_LOGI(TAG, "Saving new offset: %d vs old: %d", new_offset, clock_timezone->offset);
		clock_timezone->offset = new_offset;

		/* save new conf in memory */
		xTaskNotifyGive( clock_task_save_nvs );

		/* if a transition was processed it's a good idea to force a refresh soon */
		timestamp_transitions_check = timestamp_utc + (time_t)(60*60*24*15);
	}


	/* is it time to check for transitions? */
	if(timestamp_transitions_check && (timestamp_utc >= timestamp_transitions_check)){
		clock_queue_message_t msg;
		msg.message = CLOCK_MESSAGE_REQUEST_TRANSITIONS_API_CALL;
		msg.param = NULL;
		xQueueSend(clock_queue, &msg, portMAX_DELAY);
		timestamp_transitions_check = timestamp_utc + (time_t)(60*60*24*15); /* do another check in 15 days */
	}

	timestamp_local = timestamp_utc + clock_timezone->offset;

	/* check for sleep / wake event */
	bool process_sleep_event = false;
	sleep_event_t sleep_event = {
		.action = SLEEP_ACTION_UNKNOWN,
		.timestamp = 0x7fffffff
	};
	while(  (list_peek(clock_list_sleepevents, &sleep_event) == 0) &&  ( timestamp_local >= sleep_event.timestamp )  ){
		list_shift(clock_list_sleepevents, NULL);
		process_sleep_event = true;
	}
	if(process_sleep_event){

		clock_queue_message_t msg;
		msg.message = CLOCK_MESSAGE_SLEEP_EVENT;
		msg.param = (void*)sleep_event.action;
		xQueueSend(clock_queue, &msg, portMAX_DELAY);

		/* if a sleepevent was processed AND the count of the list is now 0, it means its time to refresh the list and
		 * recalculate new sleep events 
		 * Processing an item (eg sleep_event.action != SLEEP_ACTION_UNKNOWN) is important because the list can be empty
		 * simply because user has not set any sleepmode.
		 * */
		if(clock_list_sleepevents->count == 0){
			clock_build_new_sleepmodes(clock_config.sleepmodes);
		}
	}




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


esp_err_t clock_register_sqw_interrupt(){
	/* setup GPIO 4 as INTERRUPT on RISING EGDE */
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
	/* GPIO 4 bit mask */
	io_conf.pin_bit_mask = (1ULL<<GPIO_INPUT_IO_4);
	/* Input */
	io_conf.mode = GPIO_MODE_INPUT;
	/* No pull up since there is already a 10k external pull up */
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	//io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
	gpio_config(&io_conf);


	/* install ISR service */
	gpio_install_isr_service(ESP_INTR_FLAG_LEVEL2);
	return gpio_isr_handler_add(GPIO_INPUT_IO_4, gpio_isr_handler, (void*) GPIO_INPUT_IO_4);
}


esp_err_t clock_get_nvs_config(clock_config_t *conf){

	nvs_handle handle;
	esp_err_t esp_err;

	esp_err = nvs_open(clock_nvs_namespace, NVS_READONLY, &handle);
	if(esp_err == ESP_OK){
		ESP_LOGI(TAG, "Getting config from NVS memory");

		size_t sz = sizeof(clock_config_t);
		esp_err = nvs_get_blob(handle, "conf", conf, &sz);
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

			size_t sz = sizeof(clock_config_t);
			ESP_ERROR_CHECK(nvs_set_blob(handle, "conf", conf, sz));
			ESP_ERROR_CHECK(nvs_commit(handle));
			nvs_close(handle);
		}

	}

	return esp_err;
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


clock_config_t clock_get_config(){
	clock_config_t conf = clock_config;
	return conf;
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


esp_err_t clock_save_config(clock_config_t *conf){
	nvs_handle handle;
	esp_err_t esp_err;


	esp_err = nvs_open(clock_nvs_namespace, NVS_READWRITE, &handle);
	if(esp_err == ESP_OK){

		/* set timezone */
		size_t sz = sizeof(clock_config_t);
		esp_err = nvs_set_blob(handle, "conf", conf, sz);
		if(esp_err != ESP_OK){
			ESP_LOGE(TAG, "clock_save_config nvs_set_blob failed with error: %s", esp_err_to_name(esp_err) );
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
	memset(&clock_transitions, 0x00, sizeof(CLOCK_MAX_TRANSITIONS) * CLOCK_MAX_TRANSITIONS);
	clock_nvs_mutex = xSemaphoreCreateMutex();

	/* create the task that is used to save config in memory */
	xTaskCreate( &clock_save_config_task, "task_save_cfg", 4096, NULL, tskIDLE_PRIORITY+1, &clock_task_save_nvs );

	/* register clock queue */
	clock_queue = xQueueCreate(10, sizeof(clock_queue_message_t));

	/* initialized I2C */
	ESP_ERROR_CHECK(i2c_master_init());

	/* enabled 1Hz square wave */
	ESP_ERROR_CHECK(ds3231_enable_square_wave());

	/* HTTP client is needed for the clock task */
	ESP_ERROR_CHECK(http_client_init());

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

	/* initialize configuration */
	memset(&clock_config, 0x00, sizeof(clock_config));
	clock_config.timezone.offset = 0;
	strcpy(clock_config.timezone.name, "UTC");
	clock_config.sleepmodes.enable_sleepmode = false;
	ESP_ERROR_CHECK(clock_get_nvs_config(&clock_config));

	/* generate the list of sleep events */
	clock_list_sleepevents = list_create();
	clock_notify_new_sleepmodes(clock_config.sleepmodes);

	/* register interrupt on the 1Hz sqw signal coming from the DS3231 */
	ESP_ERROR_CHECK(clock_register_sqw_interrupt());

	clock_queue_message_t msg;

	for(;;) {
		if(xQueueReceive(clock_queue, &msg, pdMS_TO_TICKS(11001))) { /* portMAX_DELAY */

			switch(msg.message){
				case CLOCK_MESSAGE_STA_GOT_IP:
					ESP_LOGI(TAG, "CLOCK_MESSAGE_STA_GOT_IP");
					http_client_get_api_time(clock_config.timezone.name);
					break;
				case CLOCK_MESSAGE_TIMEZONE:
					ESP_LOGI(TAG, "CLOCK_MESSAGE_TIMEZONE");
					timezone_t* tz = (timezone_t*)msg.param;
					http_client_get_api_time(tz->name);
					free(tz);
					break;
				case CLOCK_MESSAGE_TICK:
					//ESP_LOGI(TAG, "CLOCK_MESSAGE_TICK");
					if(time_set){
						clock_tick();
						display_write_time(clock_time_tm_ptr);
						strftime(strftime_buf, sizeof(strftime_buf), "%c", clock_time_tm_ptr);
						ESP_LOGI(TAG, "TICK! date/time is: %s", strftime_buf);
					}
					break;
				case CLOCK_MESSAGE_REQUEST_TRANSITIONS_API_CALL:
					http_client_get_transitions(clock_config.timezone, timestamp_utc);
					break;
				case CLOCK_MESSAGE_RECEIVE_TRANSITIONS_API:{


					cJSON *json = (cJSON*)msg.param;

					if(json != NULL){
						char *json_str = cJSON_Print(json);
						ESP_LOGI(TAG, "%s", json_str);

						cJSON *transition = NULL;
						cJSON *transitions = cJSON_GetObjectItemCaseSensitive(json, "transitions");
						if(transitions){

							int i=0;
							cJSON_ArrayForEach(transition, transitions){
								if(transition){
									cJSON *transitionTimestamp = cJSON_GetObjectItemCaseSensitive(transition, "transitionTimestamp");
									cJSON *toOffset = cJSON_GetObjectItemCaseSensitive(transition, "toOffset");

									if (transitionTimestamp && toOffset && cJSON_IsNumber(transitionTimestamp) && cJSON_IsNumber(toOffset)){

										/* overflow protection */
										if(!(i >= CLOCK_MAX_TRANSITIONS)){
											/* add transitions */
											clock_transitions[i].offset = toOffset->valueint;
											clock_transitions[i].timestamp = (time_t)transitionTimestamp->valuedouble;
											i++;
										}
									}
								}
							}

							if(i==0){
								/* no transitions were processed. Run another check in a few days */
								timestamp_transitions_check = timestamp_utc + (time_t)(60*60*24*15);
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

						/* log response for debug */
						char *json_str = cJSON_Print(json);
						ESP_LOGI(TAG, "%s", json_str);

						/* extract time from json, and set it if necessary */
						cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(json, "timestamp");
						if(cJSON_IsNumber(timestamp)){
							time_t t = timestamp->valueint;
							clock_realign(t);
							time_set = true;
						}

						/* check timezone, save in memory if it's different than what was saved previously */
						cJSON *timezone = cJSON_GetObjectItemCaseSensitive(json, "timezone");
						if(cJSON_IsObject(timezone)){
							cJSON *timezoneName = cJSON_GetObjectItemCaseSensitive(timezone, "name");
							if(cJSON_IsString(timezoneName) && strcmp(clock_config.timezone.name, timezoneName->valuestring) != 0){
								/* realign clock_timezone */
								updateNVS = true;
								strcpy(clock_config.timezone.name, timezoneName->valuestring);
								ESP_LOGI(TAG, "Timezone set to: %s", clock_config.timezone.name);
							}

							cJSON *offset = cJSON_GetObjectItemCaseSensitive(timezone, "offset");
							if(cJSON_IsNumber(offset) && clock_config.timezone.offset != offset->valueint){
								/* realign offset if needed */
								updateNVS = true;
								clock_config.timezone.offset = offset->valueint;
								ESP_LOGI(TAG, "Offset set to: %d", clock_config.timezone.offset);
							}
						}

						/* memory clean up */
						free(json_str);
						cJSON_Delete(json);

						/* NVS needs to updated: notify the task that handles that */
						if(updateNVS){
							xTaskNotifyGive( clock_task_save_nvs );
						}

						/* finally, enqueue a timezone transitions call with the set timezone */
						clock_queue_message_t m;
						m.message = CLOCK_MESSAGE_REQUEST_TRANSITIONS_API_CALL;
						m.param = NULL;
						xQueueSend(clock_queue, &m, portMAX_DELAY);

					}
					break;
				case CLOCK_MESSAGE_SLEEPMODE_CONFIG:{
					ESP_LOGI(TAG, "CLOCK_MESSAGE_SLEEPMODE_CONFIG");
					sleepmodes_t* sleepmodes = (sleepmodes_t*)msg.param;
					clock_build_new_sleepmodes(*sleepmodes);

					/* if config needs to be updated then we do so and notify the task to save in flash 
						NOTE: we can do a simple memory comparison here because all the configurations objects
						are religiously bzero'd before using.
						This otherwise would be a problem because of struct alignments.
					*/
					if( memcmp( sleepmodes, &(clock_config.sleepmodes), sizeof(sleepmodes_t) ) != 0 ){
						clock_config.sleepmodes = *sleepmodes;
						xTaskNotifyGive( clock_task_save_nvs );
					}

					free(sleepmodes);
					}
					break;
				case CLOCK_MESSAGE_SLEEP_EVENT:{
					ESP_LOGI(TAG, "CLOCK_MESSAGE_SLEEP_EVENT");
					sleep_action_t a = (sleep_action_t)msg.param;
					if(a == SLEEP_ACTION_WAKE){
						display_turn_on();
					}
					else if(a == SLEEP_ACTION_SLEEP){
						display_turn_off();
					}

					}
					break;
				default:
					ESP_LOGE(TAG, "Unknown task message received: %d", msg.message);
					break;
			}

		}
		taskYIELD();
	}


	vTaskDelete( NULL );

}
