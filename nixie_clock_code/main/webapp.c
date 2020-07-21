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
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_err.h"

#include "http_app.h"

#include "webapp.h"


static esp_err_t webapp_get_hander(httpd_req_t *req){
    return ESP_OK;
}

static esp_err_t webapp_post_handler(httpd_req_t *req){
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
        http_app_set_handler_hook(HTTP_GET, NULL);
        return ret;
    }

    return ESP_OK;
}

