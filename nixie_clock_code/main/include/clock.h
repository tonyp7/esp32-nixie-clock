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


#include <stdint.h> /* for int32_t, etc. */
#include <time.h> /* for time_t */
#include <stdbool.h> /* for bool */
#include <esp_err.h> /* for esp_err_t */
#include "freertos/FreeRTOS.h" /* TickType_t */
#include "cJSON.h" /* for cJSON */


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

/** maximum numbers of sleepmodes a user can set */
#define CLOCK_MAX_SLEEPMODES				4

typedef enum clock_message_t{
	CLOCK_MESSAGE_NONE = 0,
	CLOCK_MESSAGE_TICK = 1,
	CLOCK_MESSAGE_STA_GOT_IP = 2,
	CLOCK_MESSAGE_STA_DISCONNECTED = 3,
	CLOCK_MESSAGE_RECEIVE_TIME_API = 4,
	CLOCK_MESSAGE_RECEIVE_TRANSITIONS_API = 5,
	CLOCK_MESSAGE_REQUEST_TRANSITIONS_API_CALL = 6,
	CLOCK_MESSAGE_REQUEST_TIME_API = 7,
	CLOCK_MESSAGE_SLEEPMODE_CONFIG = 8,
	CLOCK_MESSAGE_TIMEZONE = 9,
	CLOCK_MESSAGE_SLEEP_EVENT = 10,
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

/**
 * @brief defines a structure holding sleepmode information: which days to apply, and from what time to what time
 */
typedef struct sleepmode_t{
	bool enabled;
	uint8_t days;
	time_t from;
	time_t to;
}sleepmode_t;


typedef struct sleepmodes_t{
	bool enable_sleepmode;
	sleepmode_t sleepmode[CLOCK_MAX_SLEEPMODES];
}sleepmodes_t;

typedef enum sleep_action_t{
	SLEEP_ACTION_UNKNOWN = 0,
	SLEEP_ACTION_WAKE = 1,
	SLEEP_ACTION_SLEEP = 2,
	SLEEP_ACTION_MAX = 0x7fffffff
}sleep_action_t;

typedef struct sleep_event_t{
	time_t timestamp;
	sleep_action_t action;
}sleep_event_t;


/**
 * @brief defines a structure holding the gloabl configuration of the clock */
typedef struct clock_config_t{
	timezone_t timezone;
	sleepmodes_t sleepmodes;
}clock_config_t;


#define GPIO_INPUT_IO_4 				4


void clock_notify_new_timezone(char* timezone);
void clock_notify_new_sleepmodes(sleepmodes_t sleepmodes);
void clock_notify_sta_got_ip(void* pvArgument);
void clock_notify_sta_disconnected();
void clock_notify_time_api_response(cJSON *json);
void clock_notify_transitions_api_response(cJSON *json);
void clock_tick();
void clock_task(void *pvParameter);
esp_err_t clock_register_sqw_interrupt();

time_t clock_get_current_time_utc();
timezone_t clock_get_current_timezone();


/**
 * @brief saves the current configuration to NVS memory
 */
esp_err_t clock_save_config(clock_config_t *conf);


clock_config_t clock_get_config();



/**
 * @brief Retrieves the configuration from NVS memory
 */
esp_err_t clock_get_nvs_config(clock_config_t *conf);

/**
 * Mutex lock to avoid resource sharing issues when accessing the NVS
 */
bool clock_nvs_lock(TickType_t xTicksToWait);

/**
 * @see clock_nvs_lock
 */
void clock_nvs_unlock();


esp_err_t clock_save_timezone(timezone_t *tz);


//void clock_save_timezone_task(void *pvParameter);

void clock_change_timezone(timezone_t tz);
esp_err_t clock_get_nvs_timezone(timezone_t *tz);

bool clock_realign(time_t new_t);


#endif /* MAIN_CLOCK_H_ */
