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

#include "cjson/cJSON.h"
#include "http_client.h"



static const char TAG[] = "HTTP_CLIENT";


/** @brief for the sake of simplicity you can only do one HTTP request at a time; strictly enforced by this mutex */
SemaphoreHandle_t http_client_mutex = NULL;


/* used to keep track of request bodies */
char *http_client_body_str = NULL;
cJSON *http_client_body = NULL;


bool http_client_lock(TickType_t xTicksToWait){
	if(http_client_mutex){
		if( xSemaphoreTake( http_client_mutex, xTicksToWait ) == pdTRUE ) {
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
void http_client_unlock(){
	xSemaphoreGive( http_client_mutex );
}


esp_err_t _http_event_handler(esp_http_client_event_t *evt){
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
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
                printf("%.*s", evt->data_len, (char*)evt->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            http_client_cleanup(evt->client);
            http_client_unlock();
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

void http_client_init(){

	if(http_client_mutex == NULL){
		http_client_mutex = xSemaphoreCreateMutex();
	}

}


void http_client_task(void *pvParameter){

	esp_err_t err;
	esp_http_client_handle_t client = (esp_http_client_handle_t)pvParameter;
	for(;;) {
		err = esp_http_client_perform(client);
		if (err != ESP_ERR_HTTP_EAGAIN) {
			break;
		}
	}
	if (err == ESP_OK) {
		ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %d",
				esp_http_client_get_status_code(client),
				esp_http_client_get_content_length(client));
	}
	else {
		ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
	}
	vTaskDelete( NULL );

}



void http_client_cleanup(esp_http_client_handle_t client){

	if(http_client_body){
		cJSON_Delete(http_client_body);
		free(http_client_body_str);
	}

	esp_http_client_cleanup(client);
}

void http_client_get_api_time(char* timezone){


	if(http_client_lock( pdMS_TO_TICKS(5000) )){
		 ESP_LOGI(TAG, "mutex ok");

		if(timezone != NULL){
			cJSON *tz = NULL;

			/* generate the request body */
			http_client_body = cJSON_CreateObject();
			tz = cJSON_CreateString(timezone);
			cJSON_AddItemToObject(http_client_body, "timezone", tz);
			http_client_body_str = cJSON_Print(http_client_body);
		}


		esp_http_client_config_t config = {
				.url = "https://api.mclk.org/time",
		        .event_handler = _http_event_handler,
				.is_async = true,
				.timeout_ms = 10000,
		};
		esp_http_client_handle_t client = esp_http_client_init(&config);


		xTaskCreate(&http_client_task, "http_client_task", 4096, (void*)client, 1, NULL);

	}

}


void http_rest()
{
    esp_http_client_config_t config = {
        //.url = "http://httpbin.org/get",
		.url = "https://api.mclk.org/time",
        .event_handler = _http_event_handler,
		.is_async = true,
		.timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    //esp_http_client_set_post_field(client, post_data, strlen(post_data));


    esp_err_t err;
    while (1) {
            err = esp_http_client_perform(client);
            if (err != ESP_ERR_HTTP_EAGAIN) {
                break;
            }
        }
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
        }


    // GET
        /*
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }*/

    esp_http_client_cleanup(client);
}

