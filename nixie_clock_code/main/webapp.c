/**
Copyright (c) 2020 Tony Pottier

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

@file webapp.c
@author Tony Pottier

Contains page logic handling web app functionalities

*/

#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <sys/param.h> /* for the MIN macro */
#include <esp_err.h>
#include <cJSON.h>
#include <http_app.h>

#include "ws2812.h"
#include "webapp.h"
#include "clock.h"


/**
 * @brief embedded binary data.
 * @see file "CMakeLists.txt"
 * @see https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#embedding-binary-data
 */
extern const uint8_t clock_css_start[] asm("_binary_clock_css_start");
extern const uint8_t clock_css_end[]   asm("_binary_clock_css_end");
extern const uint8_t clock_js_start[] asm("_binary_clock_js_start");
extern const uint8_t clock_js_end[] asm("_binary_clock_js_end");
extern const uint8_t clock_html_start[] asm("_binary_clock_html_start");
extern const uint8_t clock_html_end[] asm("_binary_clock_html_end");
extern const uint8_t iro_js_start[] asm("_binary_iro_js_start");
extern const uint8_t iro_js_end[] asm("_binary_iro_js_end");


/* const httpd related values stored in ROM */
const static char http_200_hdr[] = "200 OK";
const static char http_400_hdr[] = "400 Bad Request";
//const static char http_503_hdr[] = "503 Service Unavailable";
const static char http_content_type_html[] = "text/html";
const static char http_content_type_js[] = "text/javascript";
const static char http_content_type_css[] = "text/css";
const static char http_content_type_json[] = "application/json";
const static char http_cache_control_hdr[] = "Cache-Control";
const static char http_cache_control_no_cache[] = "no-store, no-cache, must-revalidate, max-age=0";
const static char http_cache_control_cache[] = "public, max-age=31536000";
const static char http_pragma_hdr[] = "Pragma";
const static char http_pragma_no_cache[] = "no-cache";

const static char TAG[] = "webapp";



/**
 * @brief transform a sleepmodes_t struct into its json string representation
 */
static char* webapp_get_sleepmodes_json(sleepmodes_t sleepmodes){

	char* str_json = NULL;

	/* format as following:

	{
    "enabled":true,
    "data":[
        {"enabled":true,"days":3,"from":50400,"to":50460},
        {"enabled":false,"days":0,"from":0,"to":0},
        {"enabled":false,"days":0,"from":0,"to":0},
        {"enabled":false,"days":0,"from":0,"to":0}
	}
	*/

	cJSON *root = cJSON_CreateObject();
	cJSON *data = cJSON_CreateArray();

	cJSON_AddBoolToObject( root, "enabled", sleepmodes.enable_sleepmode );
	for(int i=0; i < CLOCK_MAX_SLEEPMODES; i++){

		cJSON *sleepmode = cJSON_CreateObject();
		cJSON_AddBoolToObject( sleepmode, "enabled", sleepmodes.sleepmode[i].enabled );
		cJSON_AddNumberToObject( sleepmode, "days", sleepmodes.sleepmode[i].days );
		cJSON_AddNumberToObject( sleepmode, "from", sleepmodes.sleepmode[i].from );
		cJSON_AddNumberToObject( sleepmode, "to", sleepmodes.sleepmode[i].to );

		cJSON_AddItemToArray( data, sleepmode);
	}
	cJSON_AddItemToObject(root, "data", data);

	str_json = cJSON_Print(root);
	cJSON_Delete(root);

	return str_json;
}

static esp_err_t webapp_get_hander(httpd_req_t *req){

    if(strcmp(req->uri, "/") == 0){

        httpd_resp_set_status(req, http_200_hdr);
		httpd_resp_set_type(req, http_content_type_html);
		httpd_resp_send(req, (char*)clock_html_start, clock_html_end - clock_html_start);
    }
    else if(strcmp(req->uri, "/clock.js") == 0){

        httpd_resp_set_status(req, http_200_hdr);
		httpd_resp_set_type(req, http_content_type_js);
        httpd_resp_set_hdr(req, http_cache_control_hdr, http_cache_control_cache);
		httpd_resp_send(req, (char*)clock_js_start, clock_js_end - clock_js_start);
    }
    else if(strcmp(req->uri, "/iro.min.js") == 0){

        httpd_resp_set_status(req, http_200_hdr);
		httpd_resp_set_type(req, http_content_type_js);
        httpd_resp_set_hdr(req, http_cache_control_hdr, http_cache_control_cache);
		httpd_resp_send(req, (char*)iro_js_start, iro_js_end - iro_js_start);
    }
    else if(strcmp(req->uri, "/clock.css") == 0){

        httpd_resp_set_status(req, http_200_hdr);
		httpd_resp_set_type(req, http_content_type_css);
        httpd_resp_set_hdr(req, http_cache_control_hdr, http_cache_control_cache);
		httpd_resp_send(req, (char*)clock_css_start, clock_css_end - clock_css_start);
    }
    else if(strcmp(req->uri, "/sleepmode/") == 0){

        clock_config_t conf = clock_get_config();
        char* str_json = webapp_get_sleepmodes_json( conf.sleepmodes );

        if(str_json){

            httpd_resp_set_status(req, http_200_hdr);
            httpd_resp_set_type(req, http_content_type_json);
            httpd_resp_set_hdr(req, http_cache_control_hdr, http_cache_control_no_cache);
            httpd_resp_set_hdr(req, http_pragma_hdr, http_pragma_no_cache);
            httpd_resp_send(req, str_json, strlen(str_json));

            free(str_json);

            return ESP_OK;
        }
        else{
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }
    else{

        httpd_resp_send_404(req);
    }

    return ESP_OK;
}

/**
 * @brief Parses a json string into a sleepmodes_t struct 
 */
static esp_err_t webapp_parse_sleemodes(char *content, sleepmodes_t *sleepmodes){

    sleepmodes_t sm;
    memset(&sm, 0x00, sizeof(sleepmodes_t));
    
    cJSON *json = cJSON_Parse(content);
    if(json == NULL){

        const char *error_ptr = cJSON_GetErrorPtr();
        ESP_LOGE(TAG, "cJSON parsing error: %s", error_ptr);
        return ESP_FAIL;
    }

    cJSON *enable_sleepmode = cJSON_GetObjectItemCaseSensitive(json, "enabled");
    sm.enable_sleepmode = (bool)enable_sleepmode->valueint;

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    cJSON *sleepmode;
    int i = 0;

    cJSON_ArrayForEach(sleepmode, data){
        cJSON *enabled = cJSON_GetObjectItemCaseSensitive(sleepmode, "enabled");
        cJSON *days = cJSON_GetObjectItemCaseSensitive(sleepmode, "days");
        cJSON *from = cJSON_GetObjectItemCaseSensitive(sleepmode, "from");
        cJSON *to = cJSON_GetObjectItemCaseSensitive(sleepmode, "to");

        sm.sleepmode[i].enabled = (bool)enabled->valueint;
        sm.sleepmode[i].days = days->valueint;
        sm.sleepmode[i].from = from->valueint;
        sm.sleepmode[i].to = to->valueint;
        i++;
        if(i >= CLOCK_MAX_SLEEPMODES) break;
    }

    cJSON_Delete(json);
    *sleepmodes = sm;

    return ESP_OK;
}

static esp_err_t webapp_post_handler(httpd_req_t *req){


    if(strcmp(req->uri, "/sleepmode/") == 0){


        const size_t max_buffer_size = 511;
        sleepmodes_t sleepmodes;
        esp_err_t ret = ESP_OK;
        
        /** read body buffer. The message should be in this format:
            {
                "enabled":	false,
                "data":	[{
                        "enabled":	false,
                        "days":	255,
                        "from":	99999,
                        "to":	99999
                    }, {
                        "enabled":	false,
                        "days":	255,
                        "from":	99999,
                        "to":	99999
                    }, {
                        "enabled":	false,
                        "days":	255,
                        "from":	99999,
                        "to":	99999
                    }, {
                        "enabled":	false,
                        "days":	255,
                        "from":	99999,
                        "to":	99999
                    }]
            }
         * that is to say never more than 400 chars.
         * because this is pretty big to be on the stack
         * it is heap allocated
         */

        /* Truncate if content length larger than the buffer */
        size_t recv_size = MIN(req->content_len, max_buffer_size);
        char* content = malloc(sizeof(char) * (recv_size+1));
        memset(content, 0x00, (recv_size+1));


        int read_count = httpd_req_recv(req, content, recv_size);
        if (read_count <= 0) {  /* 0 return value indicates connection closed */
            /* Check if timeout occurred */
            if (read_count == HTTPD_SOCK_ERR_TIMEOUT) {
                /* In case of timeout one can choose to retry calling
                    * httpd_req_recv(), but to keep it simple, here we
                    * respond with an HTTP 408 (Request Timeout) error */
                httpd_resp_send_408(req);
            }
            else {
                httpd_resp_send_500(req);
            }

            free(content);
            return ESP_FAIL;
        }

        
        /* processing -- build the sleepomodes_t object */
        ret = webapp_parse_sleemodes(content, &sleepmodes);
        if(ret == ESP_OK){

            /* send sleepmode over to the clock */
            clock_notify_new_sleepmodes(sleepmodes);

            /* web answer */
            httpd_resp_set_status(req, http_200_hdr);
            httpd_resp_set_type(req, http_content_type_json);
            httpd_resp_set_hdr(req, http_cache_control_hdr, http_cache_control_no_cache);
            httpd_resp_set_hdr(req, http_pragma_hdr, http_pragma_no_cache);
            httpd_resp_send(req, content, strlen(content));
            free(content);
            return ESP_OK;
        }
        else{
            httpd_resp_send_408(req);
            free(content);
            return ret;
        }

    }
    else if(strcmp(req->uri, "/backlights/") == 0){

        /* read body buffer. The message should be { r: 123, g: 123, b: 123 } that is to say never more than 30 chars. */
        const size_t buffer_size = 30;
        char content[buffer_size];
        esp_err_t ret = ESP_OK;
        memset(content, 0x00, buffer_size);

        /* Truncate if content length larger than the buffer */
        size_t recv_size = MIN(req->content_len, sizeof(content));

        int read_count = httpd_req_recv(req, content, recv_size);
        if (read_count <= 0) {  /* 0 return value indicates connection closed */
            /* Check if timeout occurred */
            if (read_count == HTTPD_SOCK_ERR_TIMEOUT) {
                /* In case of timeout one can choose to retry calling
                    * httpd_req_recv(), but to keep it simple, here we
                    * respond with an HTTP 408 (Request Timeout) error */
                httpd_resp_send_408(req);
            }
            else {
                httpd_resp_send_500(req);
            }
            return ESP_FAIL;
        }


        /* avoids a buffer overflow parsing the string if the content is bigger than the buffer size */
        content[buffer_size - 1] = '\0';

        /* parse content to json */
        rgb_t rgb = {.num = (uint32_t)0};
        cJSON *json = cJSON_Parse(content);
        const cJSON *r = NULL;
        const cJSON *g = NULL;
        const cJSON *b = NULL;

        if(json == NULL){

            const char *error_ptr = cJSON_GetErrorPtr();
            ESP_LOGE(TAG, "cJSON parsing error: %s", error_ptr);

            /* bad request */
            httpd_resp_set_status(req, http_400_hdr);
            httpd_resp_send(req, NULL, 0);
            return ESP_FAIL;

        }

        r = cJSON_GetObjectItemCaseSensitive(json, "r");
        g = cJSON_GetObjectItemCaseSensitive(json, "g");
        b = cJSON_GetObjectItemCaseSensitive(json, "b");

        if(r != NULL && g != NULL && b != NULL && cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b)){

            rgb.r = (uint8_t)r->valueint;
            rgb.g = (uint8_t)g->valueint;
            rgb.b = (uint8_t)b->valueint;

            /* free json object */
            cJSON_Delete(json);

            ret = ws2812_set_backlight_color(rgb);
            if(ret == ESP_OK){

                httpd_resp_set_status(req, http_200_hdr);
                httpd_resp_send(req, NULL, 0);
                return ESP_OK;
            }
            else{
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }
        }
        else{

            /* free json object */
            cJSON_Delete(json);

            /* bad request */
            httpd_resp_set_status(req, http_400_hdr);
            httpd_resp_send(req, NULL, 0);
            return ESP_FAIL;
        }

    } /* if(strcmp(req->uri, "/backlights/") == 0) */

    return ESP_OK;
}

esp_err_t webapp_register_handlers(){

    esp_err_t ret;
    ret = http_app_set_handler_hook(HTTP_GET, &webapp_get_hander);

    if(ret != ESP_OK){
        return ret;
    }

    ret = http_app_set_handler_hook(HTTP_POST, &webapp_post_handler);
    if(ret != ESP_OK){
        http_app_set_handler_hook(HTTP_POST, NULL);
        return ret;
    }

    return ESP_OK;
}

