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

@file http_client.c
@author Tony Pottier

Contains wrappers around the esp http client for the mclk.org time API.

*/

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "cJSON.h"

#include "clock.h"
#include "http_client.h"



static const char TAG[] = "HTTP_CLIENT";
static const char *HTTP_CLIENT_TIME_API_URL = "https://api.mclk.org/time";
static const char *HTTP_CLIENT_TRANSITIONS_API_URL = "https://api.mclk.org/transitions";



static QueueHandle_t http_client_queue = NULL;

/* used to keep track of request bodies that eventually need to be freed */
static char *http_client_body_str = NULL;

/* used to keep track of response */
static char *http_client_response_str = NULL;

static esp_http_client_handle_t http_client_handle = NULL;




void http_client_process_data(esp_http_client_event_t *evt){
	if(evt->user_data == (void*)HTTP_CLIENT_TIME_API_URL){

		/* process json answer */
		if(http_client_response_str){
			cJSON *json = cJSON_Parse(http_client_response_str);
			clock_notify_time_api_response(json);
		}

	}
	else if(evt->user_data == (void*)HTTP_CLIENT_TRANSITIONS_API_URL){

		/* process json answer */
		if(http_client_response_str){
			cJSON *json = cJSON_Parse(http_client_response_str);
			clock_notify_transitions_api_response(json);
		}
	}
}




void http_client_cleanup(esp_http_client_handle_t client){

	if(http_client_body_str){
		free(http_client_body_str);
		http_client_body_str = NULL;
	}

	if(http_client_response_str){
		free(http_client_response_str);
		http_client_response_str = NULL;
	}

	esp_http_client_cleanup(client);
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt){


	switch(evt->event_id) {
			case HTTP_EVENT_ERROR:
				ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
				break;
			case HTTP_EVENT_ON_CONNECTED:
				ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
				break;
			case HTTP_EVENT_HEADER_SENT:
				ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
				break;
			case HTTP_EVENT_ON_HEADER:
				ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
				break;
			case HTTP_EVENT_ON_DATA:
				ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
				if(evt->data_len > 0){
					if(http_client_response_str == NULL){
						/* allocate memory */
						int sz = esp_http_client_get_content_length(evt->client) + 1;
						http_client_response_str = (char*)malloc(sizeof(char) * sz);
						memset(http_client_response_str, '\0', sz);
					}
					/* copy data over */
					char* tmp = (char*)malloc(sizeof(char) * (evt->data_len + 1));
					memset(tmp, '\0', evt->data_len + 1);
					memcpy(tmp, evt->data, evt->data_len);
					strcat(http_client_response_str, tmp);
					free(tmp);
				}
				break;
			case HTTP_EVENT_ON_FINISH:
				ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
				http_client_process_data(evt);
				break;
			case HTTP_EVENT_DISCONNECTED:
				ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
				break;
		}

    return ESP_OK;
}


static void http_client_api_time_process(char *timezone){

	esp_err_t err;
	ESP_LOGI(TAG, "tz: %s", timezone);

	esp_http_client_config_t config = {
					.url = HTTP_CLIENT_TIME_API_URL,
					.event_handler = _http_event_handler,
					.is_async = true,
					.timeout_ms = 10000,
					.user_data = (void*)HTTP_CLIENT_TIME_API_URL
	};
	http_client_handle = esp_http_client_init(&config);


	if(timezone != NULL){
		cJSON *body = NULL;
		cJSON *tz = NULL;
		char* body_str = NULL;

		/* generate the request body */
		body = cJSON_CreateObject();
		tz = cJSON_CreateString(timezone);
		cJSON_AddItemToObject(body, "timezone", tz);
		body_str = cJSON_Print(body);
		cJSON_Delete(body);

		/* save pointer for later cleanup */
		http_client_body_str = body_str;

		/* set body */
		esp_http_client_set_post_field(http_client_handle, body_str, strlen(body_str));
	}


	for(;;) {
		err = esp_http_client_perform(http_client_handle);
		if (err != ESP_ERR_HTTP_EAGAIN) {
			break;
		}
		vTaskDelay( pdMS_TO_TICKS(10) ); /* avoid watchdog trigger */
	}
	if (err == ESP_OK) {
		ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %d",
				esp_http_client_get_status_code(http_client_handle),
				esp_http_client_get_content_length(http_client_handle));
	}
	else {
		ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
	}

	http_client_cleanup(http_client_handle);

}


static void http_client_api_transitions_process(void *pvParameter){
	timezone_t timezone = clock_get_config_timezone();
	time_t now = clock_get_current_time_utc();

	esp_http_client_config_t config = {
			.url = HTTP_CLIENT_TRANSITIONS_API_URL,
			.event_handler = _http_event_handler,
			.is_async = true,
			.timeout_ms = 10000,
			.user_data = (void*)HTTP_CLIENT_TRANSITIONS_API_URL
	};
	http_client_handle = esp_http_client_init(&config);


	if(timezone.name != NULL){
		cJSON *body = NULL;
		cJSON *tz = NULL;
		cJSON *from = NULL;
		cJSON *to = NULL;
		esp_err_t err;

		char* body_str = NULL;

		/* generate the request body */
		body = cJSON_CreateObject();

		/* timezone */
		tz = cJSON_CreateString(timezone.name);
		cJSON_AddItemToObject(body, "timezone", tz);

		/* from */
		time_t from_time = now - (60*60*24) ; /* -1 day back to avoid some weird edge cases by getting transitions strictly on now timestamp */
		from = cJSON_CreateNumber(from_time);
		cJSON_AddItemToObject(body, "from", from);

		/* to */
		time_t to_time = now + 60*60*24*365; /* +1 year */
		to = cJSON_CreateNumber(to_time);
		cJSON_AddItemToObject(body, "to", to);


		/* transform to json string then clean up cJSON object */
		body_str = cJSON_Print(body);
		cJSON_Delete(body);

		/* save pointer for later cleanup */
		http_client_body_str = body_str;

		/* set body */
		esp_http_client_set_post_field(http_client_handle, body_str, strlen(body_str));

		for(;;) {
			err = esp_http_client_perform(http_client_handle);
			if (err != ESP_ERR_HTTP_EAGAIN) {
				break;
			}
			vTaskDelay( pdMS_TO_TICKS(10) ); /* avoid watchdog trigger */
		}
		if (err == ESP_OK) {
			ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %d",
					esp_http_client_get_status_code(http_client_handle),
					esp_http_client_get_content_length(http_client_handle));
		}
		else {
			ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
		}

		http_client_cleanup(http_client_handle);
	}
}


/**
 * @brief freeRTOS task that processes URL requests. 
 * 
 * Because this is processed in a queue, it ensures only one call at a time can be made
 */
static void http_client_task(void *pvParameter){

	clock_queue_message_t msg;

	for(;;) {
		if(xQueueReceive(http_client_queue, &msg, portMAX_DELAY)) {

			switch(msg.message){

				case CLOCK_MESSAGE_REQUEST_TIME_API:{
					char* tz = (char*)msg.param;
					http_client_api_time_process( tz );
					if(tz != NULL){
						free(tz);
					}
					}
					break;

				case CLOCK_MESSAGE_REQUEST_TRANSITIONS_API_CALL:
					http_client_api_transitions_process( NULL );
					break;

				default:
					break;
			}
		}	
	}
}


esp_err_t http_client_init(){
	http_client_queue = xQueueCreate(10, sizeof(clock_queue_message_t));
	xTaskCreatePinnedToCore(&http_client_task, "http_client_task", 16384, NULL, CLOCK_TASK_PRIORITY-1, NULL, 1);
	return ESP_OK;
}


void http_client_get_transitions(timezone_t timezone, time_t now){
	clock_queue_message_t msg;
	msg.message = CLOCK_MESSAGE_REQUEST_TRANSITIONS_API_CALL;
	msg.param = NULL;
	xQueueSend(http_client_queue, &msg, portMAX_DELAY);
}


void http_client_get_api_time(char* timezone){
	clock_queue_message_t msg;
	size_t sz = strlen(timezone) + 1;
	char* param = malloc(sizeof(char) * sz);
	memset(param, 0x00, sz);
	strcpy(param, timezone);
	msg.message = CLOCK_MESSAGE_REQUEST_TIME_API;
	msg.param = (void*)param;
	xQueueSend(http_client_queue, &msg, portMAX_DELAY);
}

