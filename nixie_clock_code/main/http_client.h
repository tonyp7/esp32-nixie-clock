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

@file http_client.h
@author Tony Pottier

Contains wrappers around the esp http client for the mclk.org time API.

*/
#ifndef MAIN_INCLUDE_HTTP_CLIENT_H_
#define MAIN_INCLUDE_HTTP_CLIENT_H_

void http_client_init();
void http_client_cleanup(esp_http_client_handle_t client);
bool http_client_lock(TickType_t xTicksToWait);
void http_client_unlock();
void http_client_get_api_time(char* timezone);
void http_rest();
void http_client_task(void *pvParameter);


#endif /* MAIN_INCLUDE_HTTP_CLIENT_H_ */