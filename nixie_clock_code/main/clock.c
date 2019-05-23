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
#include "esp_system.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"
#include "esp_http_client.h"

#include "clock.h"
#include "ds3231.h"
#include "i2c.h"
#include "http_client.h"
#include "cjson/cJSON.h"


#define CLOCK_SQW_INTERRUPT_GPIO


/* @brief tag used for ESP serial console messages */
static const char TAG[] = "clock";

/* @brief flash memory namespace for the clock */
const char clock_nvs_namespace[] = "clock";


//char clock_tz_string[CLOCK_MAX_TZ_STRING_LENGTH];

time_t timestamp_utc;
time_t timestamp_local;
struct tm *clock_time_tm_ptr; /*used for localtime function which returns a static pointer */
struct tm clock_time_tm;
timezone_t timezone;

static QueueHandle_t clock_queue = NULL;



void clock_notify_sta_got_ip(void* pvArgument){
	if(clock_queue){
		uint32_t message = CLOCK_MESSAGE_STA_GOT_IP;
		xQueueSend(clock_queue, (void*)&message, portMAX_DELAY);
		//xQueueSendFromISR(clock_queue, &message, NULL);
	}
}
void clock_notify_sta_disconnected(){
	if(clock_queue){
		xQueueSend(clock_queue, (void*)CLOCK_MESSAGE_STA_DISCONNECTED, portMAX_DELAY);
	}
}



void clock_change_timezone(timezone_t tz){

}


static void initialize_sntp(void){
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, CLOCK_SNTP_SERVER);
    sntp_init();
}



static void obtain_time(void){
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

}





static void IRAM_ATTR gpio_isr_handler(void* arg){
    //uint32_t gpio_num = (uint32_t) arg;
    uint32_t message = CLOCK_MESSAGE_TICK;
    xQueueSendFromISR(clock_queue, &message, NULL);
	return;

}


void clock_tick(){
	/* +1 second */
	timestamp_local = ++timestamp_utc + timezone.offset;
	clock_time_tm_ptr = localtime(&timestamp_local);
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



esp_err_t clock_get_timezone(timezone_t *tz){
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

/**
 * @brief this is the main RTOS task that controls everything in the clock
 */
void clock_task(void *pvParameter){


	//vTaskDelay( pdMS_TO_TICKS(5000));

	char strftime_buf[64];

	/* register clock queue */
	clock_queue = xQueueCreate(10, sizeof(uint32_t));

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

	/* time variables */
	timestamp_utc = mktime(&clock_time_tm);
	clock_time_tm_ptr = localtime(&timestamp_utc);

	/* init timezone and attempt to get what was saved in nvs */
	timezone.offset = 0;
	strcpy(timezone.name, "UTC");
	ESP_ERROR_CHECK(clock_get_timezone(&timezone));

	/*
	const char* json_string = "{\"unixEpoch\":1558439438,\"timeString\":\"Tuesday, 21-May-2019 07:50:38 EDT\",\"timezone\":{\"timezoneString\":\"America\\/New_York\",\"offset\":-14400,\"offsetPretty\":\"UTC-04:00\",\"abbr\":\"EDT\"}}";
	const cJSON *name = NULL;
	cJSON *json = cJSON_Parse(json_string);
	name = cJSON_GetObjectItemCaseSensitive(json, "timeString");
	if (cJSON_IsString(name) && (name->valuestring != NULL))
	{
		printf("Checking monitor \"%s\"\n", name->valuestring);
	}*/

/*
	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now, &timeinfo);
	// Is time set? If not, tm_year will be (1970 - 1900).
	if (timeinfo.tm_year < (2016 - 1900)) {
		ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
		obtain_time();
		// update 'now' variable with current time
		time(&now);
	}

	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "The current date/time in UTC is: %s", strftime_buf);


	ds3231_get_time(&timeinfo);*/



	//for(;;){
	//	vTaskDelay( pdMS_TO_TICKS(100));
	//}










	/* register interrupt on the 1Hz sqw signal coming from the DS3231 */
	clock_register_sqw_interrupt();





	uint32_t msg;
	for(;;) {
		if(xQueueReceive(clock_queue, &msg, portMAX_DELAY)) { /* portMAX_DELAY */

			switch(msg){
				case CLOCK_MESSAGE_STA_GOT_IP:
					ESP_LOGE(TAG, "******************************* CONNECTED");
					//http_rest();
					http_client_get_api_time("Asia/Singapore");
					break;
				case CLOCK_MESSAGE_TICK:
					clock_tick();
					strftime(strftime_buf, sizeof(strftime_buf), "%c", clock_time_tm_ptr);
					ESP_LOGE(TAG, "TICK! date/time is: %s", strftime_buf);
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
