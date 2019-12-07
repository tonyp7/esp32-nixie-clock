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

@file http_server.h
@author Tony Pottier
@brief Defines all functions necessary for the HTTP server to run.

Contains the freeRTOS task for the HTTP listener and all necessary support
function to process requests, decode URLs, serve files, etc. etc.

@note http_server task cannot run without the wifi_manager task!
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#ifndef HTTP_SERVER_H_INCLUDED
#define HTTP_SERVER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_SERVER_START_BIT_0	( 1 << 0 )

#define isspace(r) (r == ' ' || r == '\n' || r == '\r')


typedef struct http_header_t {
	char* name;
	char* value;
}http_header_t;

typedef struct http_request_t {
	char* request;
	http_header_t* headers;
	int headers_count;
	char* body;
	int content_length;
}http_request_t;


typedef enum http_request_parser_state_t{
	locating_header_name,
	reading_header_name,
	locating_header_value,
	reading_header_value,
	skip_beginning_value_spaces
}http_request_parser_state_t;



esp_err_t http_server_delete_request(http_request_t ** http_request);
esp_err_t http_server_parse_request(http_request_t ** http_request, const char* raw, const uint16_t raw_size);

void http_server(void *pvParameters);
void http_server_netconn_serve(struct netconn *conn);
//void http_server_set_event_start(); //TODO: delete
void http_server_start();

/**
 * @brief gets a char* pointer to the first occurence of header_name withing the complete http request request.
 *
 * For optimization purposes, no local copy is made. memcpy can then be used in coordination with len to extract the
 * data.
 *
 * @param request the full HTTP raw request.
 * @param header_name the header that is being searched.
 * @param len the size of the header value if found.
 * @return pointer to the beginning of the header value.
 */
char* http_server_get_header(char *request, char *header_name, int *len);

#ifdef __cplusplus
}
#endif

#endif
