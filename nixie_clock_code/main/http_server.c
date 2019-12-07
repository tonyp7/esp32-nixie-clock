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

@file http_server.c
@author Tony Pottier
@brief Defines all functions necessary for the HTTP server to run.

Contains the freeRTOS task for the HTTP listener and all necessary support
function to process requests, decode URLs, serve files, etc. etc.

@note http_server task cannot run without the wifi_manager task!
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/opt.h"
#include "lwip/memp.h"
#include "lwip/ip.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/priv/api_msg.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/priv/tcpip_priv.h"

#include "http_server.h"
#include "wifi_manager.h"
#include "ws2812.h"


//EventGroupHandle_t http_server_event_group = NULL;
//EventBits_t uxBits;

static const char TAG[] = "http_server";
static TaskHandle_t task_http_server = NULL;


/* embedded binary data */
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[]   asm("_binary_style_css_end");
extern const uint8_t jquery_gz_start[] asm("_binary_jquery_gz_start");
extern const uint8_t jquery_gz_end[] asm("_binary_jquery_gz_end");
extern const uint8_t code_js_start[] asm("_binary_code_js_start");
extern const uint8_t code_js_end[] asm("_binary_code_js_end");
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

/* clock specific */
extern const uint8_t iro_js_start[] asm("_binary_iro_js_start");
extern const uint8_t iro_js_end[] asm("_binary_iro_js_end");
extern const uint8_t clock_js_start[] asm("_binary_clock_js_start");
extern const uint8_t clock_js_end[] asm("_binary_clock_js_end");
extern const uint8_t clock_css_start[] asm("_binary_style_css_start");
extern const uint8_t clock_css_end[]   asm("_binary_style_css_end");


/* const http headers stored in ROM */
const static char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
const static char http_css_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/css\nCache-Control: public, max-age=31536000\n\n";
const static char http_js_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
const static char http_jquery_gz_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\nAccept-Ranges: bytes\nContent-Length: 29995\nContent-Encoding: gzip\n\n";
const static char http_400_hdr[] = "HTTP/1.1 400 Bad Request\nContent-Length: 0\n\n";
const static char http_404_hdr[] = "HTTP/1.1 404 Not Found\nContent-Length: 0\n\n";
const static char http_503_hdr[] = "HTTP/1.1 503 Service Unavailable\nContent-Length: 0\n\n";
const static char http_ok_json_no_cache_hdr[] = "HTTP/1.1 200 OK\nContent-type: application/json\nCache-Control: no-store, no-cache, must-revalidate, max-age=0\nPragma: no-cache\n\n";
const static char http_redirect_hdr_start[] = "HTTP/1.1 302 Found\nLocation: http://";
const static char http_redirect_hdr_end[] = "/\n\n";



void http_server_start(){
	if(task_http_server == NULL){
		xTaskCreate(&http_server, "http_server", 2048, NULL, WIFI_MANAGER_TASK_PRIORITY-1, &task_http_server);
	}
}

void http_server(void *pvParameters) {

	struct netconn *conn, *newconn;
	err_t err;
	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn, IP_ADDR_ANY, 80);
	netconn_listen(conn);
	ESP_LOGI(TAG, "HTTP Server listening on 80/tcp");
	do {
		err = netconn_accept(conn, &newconn);
		if (err == ERR_OK) {
			http_server_netconn_serve(newconn);
			netconn_delete(newconn);
		}
		taskYIELD();  /* allows the freeRTOS scheduler to take over if needed. */
	} while(err == ERR_OK);
	netconn_close(conn);
	netconn_delete(conn);

	vTaskDelete( NULL );
}


char* http_server_get_header(char *request, char *header_name, int *len) {
	*len = 0;
	char *ret = NULL;
	char *ptr = NULL;

	ptr = strstr(request, header_name);
	if (ptr) {
		ret = ptr + strlen(header_name);
		ptr = ret;
		while (*ptr != '\0' && *ptr != '\n' && *ptr != '\r') {
			(*len)++;
			ptr++;
		}
		return ret;
	}
	return NULL;
}


esp_err_t http_server_delete_request(http_request_t ** http_request) {

	http_request_t *req = (*http_request);

	if (req->body) free(req->body);
	if (req->request) free(req->request);

	if (req->headers_count > 0) {
		for (int i = 0; i<req->headers_count; i++) {
			if (req->headers[i].name) free(req->headers[i].name);
			if (req->headers[i].value) free(req->headers[i].value);
		}
		free(req->headers);
	}

	free(req);

	http_request = NULL;

	return ESP_OK;
}

esp_err_t http_server_parse_request(http_request_t ** http_request, const char* raw, const uint16_t raw_size) {

	http_request_t *req = (*http_request);

	req->content_length = 0;
	req->headers_count = 0;
	req->body = NULL;

	/* save body (if any) */
	const char body_separation_pattern[] = "\r\n\r\n";
	const char *body_start = NULL;
	const char *headers_start = NULL;
	const char *raw_end = (const char*)(raw + (int)raw_size - 1);
	body_start = strstr(raw, body_separation_pattern);


	const char *c = raw;

	/* first parse: header count and body len and snatch request while we're at it */
	while (c != raw_end) {

		if (c == body_start) {
			/* calculate size of content len */
			req->content_length = (int)raw_size - (int)(c - raw) - strlen(body_separation_pattern);
			if (req->content_length > 0) {
				req->body = (char*)malloc(sizeof(char) * (req->content_length + 1));
				memset(req->body, 0x00, req->content_length + 1);
				memcpy(req->body, body_start + strlen(body_separation_pattern), req->content_length);
			}
			break;
		}
		else if (*c == '\n' || *c == '\r') {

			/* ignore subsequent new lines or carrier return */
			do{
				c++;
				if (!(*c == '\n' || *c == '\r')) {
					break;
				}
			} while (c != raw_end);
			/* went all the way to the end of the request?? edge case for bogus requests */
			if (c == raw_end)
				break;

			/* request line is identified as header count is currently 0 */
			if (req->headers_count == 0) {
				req->request = (char*)malloc(sizeof(char) * (c - raw + 1));
				memset(req->request, 0x00, (c - raw) + 1);
				memcpy(req->request, raw, (c - raw));
				headers_start = c;
			}

			req->headers_count++;

		}

		c++;
	}

	/* allocate headers if any. */
	if (req->headers_count) {
		req->headers = (http_header_t*)malloc(sizeof(http_header_t) * req->headers_count);

		for (int i = 0; i<req->headers_count; i++) {
			req->headers[i].name = NULL;
			req->headers[i].value = NULL;
		}
	}

	/* copy each header individually */
	if (body_start == NULL) body_start = (raw + (int)raw_size - 1);
	c = headers_start;
	const char* str = headers_start;
	int current_header_index = 0;
	http_request_parser_state_t state = locating_header_name;
	while (c != body_start) {

		switch (state) {
			case locating_header_name:
				if (!isspace(*c)) {
					str = c; /* found the beginning of the header name*/
					state = reading_header_name;
					continue;
				}
				break;
			case reading_header_name:
				if (*c == ':') {
					/* found the end of the header name */
					int len = c - str;
					if (len > 0) {
						req->headers[current_header_index].name = (char*)malloc(sizeof(char) * len + 1);
						memset(req->headers[current_header_index].name, 0x00, len + 1);
						memcpy(req->headers[current_header_index].name, str, len);
						state = locating_header_value;
						str = c;
					}
					else {
						state = locating_header_name; /* edge case, should not have a header with no length*/
					}
				}
				break;
			case locating_header_value:
				if (!isspace(*c)) {
					str = c; /* found the beginning of the header value */
					state = reading_header_value;
					continue;
				}
				break;
			case reading_header_value:
				if (*c == '\r' || *c == '\n' || c == body_start /*last header*/) {
					/* found the end of the header value */
					int len = c - str;
					if (len > 0) {
						req->headers[current_header_index].value = (char*)malloc(sizeof(char) * len + 1);
						memset(req->headers[current_header_index].value, 0x00, len + 1);
						memcpy(req->headers[current_header_index].value, str, len);
						state = locating_header_name;
						current_header_index++;
						str = c;
					}
				}
				break;
			default:
				break;
		}

		c++;

	}

	return ESP_OK;
}

void http_server_netconn_serve(struct netconn *conn) {

	struct netbuf *inbuf;
	char *buf = NULL;
	u16_t buflen;
	err_t err;
	const char new_line[2] = "\n";
	http_request_t *http_request = NULL;

	err = netconn_recv(conn, &inbuf);
	if (err == ERR_OK) {

		/* get raw data and parse http request */
		netbuf_data(inbuf, (void**)&buf, &buflen);
		http_request = (http_request_t*)malloc(sizeof(http_request_t));
		http_server_parse_request(&http_request, buf, buflen);


		/* extract the first line of the request */
		char *save_ptr = buf;
		char *line = strtok_r(save_ptr, new_line, &save_ptr);

		if(line) {

			/* captive portal functionality: redirect to access point IP for HOST that are not the access point IP OR the STA IP */
			int lenH = 0;
			char *host = http_server_get_header(save_ptr, "Host: ", &lenH);
			/* determine if Host is from the STA IP address */
			wifi_manager_lock_sta_ip_string(portMAX_DELAY);
			bool access_from_sta_ip = lenH > 0?strstr(host, wifi_manager_get_sta_ip_string()):false;
			wifi_manager_unlock_sta_ip_string();

			if (lenH > 0 && !strstr(host, DEFAULT_AP_IP) && !access_from_sta_ip) {
				netconn_write(conn, http_redirect_hdr_start, sizeof(http_redirect_hdr_start) - 1, NETCONN_NOCOPY);
				netconn_write(conn, DEFAULT_AP_IP, sizeof(DEFAULT_AP_IP) - 1, NETCONN_NOCOPY);
				netconn_write(conn, http_redirect_hdr_end, sizeof(http_redirect_hdr_end) - 1, NETCONN_NOCOPY);
			}
			else{
				/* default page */
				if(strstr(line, "GET / ")) {
					netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
					netconn_write(conn, index_html_start, index_html_end - index_html_start, NETCONN_NOCOPY);
				}
				else if(strstr(line, "GET /jquery.js ")) {
					netconn_write(conn, http_jquery_gz_hdr, sizeof(http_jquery_gz_hdr) - 1, NETCONN_NOCOPY);
					netconn_write(conn, jquery_gz_start, jquery_gz_end - jquery_gz_start, NETCONN_NOCOPY);
				}
				else if(strstr(line, "GET /code.js ")) {
					netconn_write(conn, http_js_hdr, sizeof(http_js_hdr) - 1, NETCONN_NOCOPY);
					netconn_write(conn, code_js_start, code_js_end - code_js_start, NETCONN_NOCOPY);
				}
				else if(strstr(line, "GET /ap.json ")) {
					/* if we can get the mutex, write the last version of the AP list */
					if(wifi_manager_lock_json_buffer(( TickType_t ) 10)){
						netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY);
						char *buff = wifi_manager_get_ap_list_json();
						netconn_write(conn, buff, strlen(buff), NETCONN_NOCOPY);
						wifi_manager_unlock_json_buffer();
					}
					else{
						netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
						ESP_LOGD(TAG, "http_server_netconn_serve: GET /ap.json failed to obtain mutex");
					}
					/* request a wifi scan */
					wifi_manager_scan_async();
				}
				else if(strstr(line, "GET /style.css ")) {
					netconn_write(conn, http_css_hdr, sizeof(http_css_hdr) - 1, NETCONN_NOCOPY);
					netconn_write(conn, style_css_start, style_css_end - style_css_start, NETCONN_NOCOPY);
				}
				else if(strstr(line, "GET /status.json ")){
					if(wifi_manager_lock_json_buffer(( TickType_t ) 10)){
						char *buff = wifi_manager_get_ip_info_json();
						if(buff){
							netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY);
							netconn_write(conn, buff, strlen(buff), NETCONN_NOCOPY);
							wifi_manager_unlock_json_buffer();
						}
						else{
							netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
						}
					}
					else{
						netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
						ESP_LOGD(TAG, "http_server_netconn_serve: GET /status failed to obtain mutex");
					}
				}
				else if(strstr(line, "DELETE /connect.json ")) {
					ESP_LOGD(TAG, "http_server_netconn_serve: DELETE /connect.json");
					/* request a disconnection from wifi and forget about it */
					wifi_manager_disconnect_async();
					netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); /* 200 ok */
				}
				else if(strstr(line, "POST /connect.json ")) {
					ESP_LOGD(TAG, "http_server_netconn_serve: POST /connect.json");

					bool found = false;
					int lenS = 0, lenP = 0;
					char *ssid = NULL, *password = NULL;
					ssid = http_server_get_header(save_ptr, "X-Custom-ssid: ", &lenS);
					password = http_server_get_header(save_ptr, "X-Custom-pwd: ", &lenP);

					if(ssid && lenS <= MAX_SSID_SIZE && password && lenP <= MAX_PASSWORD_SIZE){
						wifi_config_t* config = wifi_manager_get_wifi_sta_config();
						memset(config, 0x00, sizeof(wifi_config_t));
						memcpy(config->sta.ssid, ssid, lenS);
						memcpy(config->sta.password, password, lenP);
						ESP_LOGD(TAG, "http_server_netconn_serve: wifi_manager_connect_async() call");
						wifi_manager_connect_async();
						netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); //200ok
						found = true;
					}

					if(!found){
						/* bad request the authentification header is not complete/not the correct format */
						netconn_write(conn, http_400_hdr, sizeof(http_400_hdr) - 1, NETCONN_NOCOPY);
					}

				}
				/* specific to the clock implementation */
				else if(strstr(line, "GET /clock.js ")) {
					netconn_write(conn, http_js_hdr, sizeof(http_js_hdr) - 1, NETCONN_NOCOPY);
					netconn_write(conn, clock_js_start, clock_js_end - clock_js_start, NETCONN_NOCOPY);
				}
				else if(strstr(line, "GET /iro.js ")) {
					netconn_write(conn, http_js_hdr, sizeof(http_js_hdr) - 1, NETCONN_NOCOPY);
					netconn_write(conn, iro_js_start, iro_js_end - iro_js_start, NETCONN_NOCOPY);
				}
				else if(strstr(line, "GET /clock.css ")) {
					netconn_write(conn, http_css_hdr, sizeof(http_css_hdr) - 1, NETCONN_NOCOPY);
					netconn_write(conn, clock_css_start, clock_css_end - clock_css_start, NETCONN_NOCOPY);
				}
				else if(strstr(line, "POST /color ")) {
					netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); //200 ok - no cache

					int lenR = 0, lenG = 0, lenB = 0;
					int r = 0,g = 0,b = 0;
					char *color_r = NULL, *color_g = NULL, *color_b = NULL;
					color_r = http_server_get_header(save_ptr, "X-Custom-R: ", &lenR);
					color_g = http_server_get_header(save_ptr, "X-Custom-G: ", &lenG);
					color_b = http_server_get_header(save_ptr, "X-Custom-B: ", &lenB);
					sscanf(color_r, "%d%*s", &r);
					sscanf(color_g, "%d%*s", &g);
					sscanf(color_b, "%d%*s", &b);
					rgb_t c =  ws2812_create_rgb(r, g, b);
					ws2812_set_backlight_color(c);


				}
				else{
					netconn_write(conn, http_400_hdr, sizeof(http_400_hdr) - 1, NETCONN_NOCOPY);
				}
			}
		}
		else{
			netconn_write(conn, http_404_hdr, sizeof(http_404_hdr) - 1, NETCONN_NOCOPY);
		}

		http_server_delete_request(&http_request);
	}

	/* free the buffer */
	netbuf_delete(inbuf);
}
