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

@file clock.h
@author Tony Pottier
@brief Describes the main task driving the rest of the nixie clock

@see https://idyl.io
@see https://github.com/tonyp7/esp32-nixie-clock

For a comprehensive documentation on how the timezone strings work:
@see https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
@see ftp://ftp.iana.org/tz/tz-how-to.html
Unfortunately, it is broken on the ESP32. Things like CEST+2 simply do not work
which makes it unusable. The clock instead references everything from UTC and
apply offset manually to the timestamp.

*/


#ifndef MAIN_CLOCK_H_
#define MAIN_CLOCK_H_


#include "cJSON.h"

/** @brief Defines the task priority of the clock (main task).
 *  This should be the highest priority task unless very specific reason. Default: 10.
 */
#define CLOCK_TASK_PRIORITY				CONFIG_CLOCK_TASK_PRIORITY


/**
 * @brief Store the timezone in its readable way, such as "America/New_York"
 * At the time of writing this there are three 30 char long zones; which are:
 * 	America/Argentina/Buenos_Aires
 * 	America/Argentina/Rio_Gallegos
 * 	America/North_Dakota/New_Salem
 * 40 is rounded up to the nearest 10, to include the \0.
 */
#define CLOCK_MAX_TZ_STRING_LENGTH			40


/** in seconds, the maximum drift allowed before we force a refresh of the time */
#define CLOCK_MAX_ACCEPTABLE_TIME_DRIFT		60.0

/** number of transitions that will be stored in advance. Most timezones have 0 or 2 (summer time) so the default of 3 is plenty. */
#define CLOCK_MAX_TRANSITIONS				3


typedef enum clock_message_t{
	CLOCK_MESSAGE_NONE = 0,
	CLOCK_MESSAGE_TICK = 1,
	CLOCK_MESSAGE_STA_GOT_IP = 2,
	CLOCK_MESSAGE_STA_DISCONNECTED = 3,
	CLOCK_MESSAGE_RECEIVE_TIME_API = 4,
	CLOCK_MESSAGE_RECEIVE_TRANSITIONS_API = 5,
	CLOCK_MESSAGE_REQUEST_TRANSITIONS_API_CALL = 6,
	CLOCK_MESSAGE_MAX = 0x7fffffff
}clock_message_t;

/** @brief type that is processed by the clock queue */
typedef struct clock_queue_message_t{
	clock_message_t message;
	void *param;
}clock_queue_message_t;

typedef struct timezone_t{
	int32_t offset;
	char name[CLOCK_MAX_TZ_STRING_LENGTH];
}timezone_t;

typedef struct transition_t{
	int32_t offset;
	time_t timestamp;
}transition_t;


#define GPIO_INPUT_IO_4 				4
#define ESP_INTR_FLAG_DEFAULT 			0




void clock_notify_sta_got_ip(void* pvArgument);
void clock_notify_sta_disconnected();
void clock_notify_time_api_response(cJSON *json);
void clock_notify_transitions_api_response(cJSON *json);
void clock_tick();
void clock_task(void *pvParameter);
esp_err_t clock_register_sqw_interrupt();

time_t clock_get_current_time_utc();
timezone_t clock_get_current_timezone();

esp_err_t clock_save_timezone(timezone_t *tz);
bool clock_nvs_lock(TickType_t xTicksToWait);
void clock_nvs_unlock();
void clock_save_timezone_task(void *pvParameter);

void clock_change_timezone(timezone_t tz);
esp_err_t clock_get_nvs_timezone(timezone_t *tz);

bool clock_realign(time_t new_t);


#endif /* MAIN_CLOCK_H_ */
