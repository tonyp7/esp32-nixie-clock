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

@file list.h
@author Tony Pottier
@brief Simple C linked list

@see https://idyl.io
@see https://github.com/tonyp7/esp32-nixie-clock

*/

#ifndef MAIN_LIST_H_
#define MAIN_LIST_H_

struct node_t {
    void* data;
    struct node_t* next;
};
typedef struct node_t node_t;

typedef struct list_t {
    unsigned int count;
    node_t* first;
    node_t* last;
}list_t;


/**
 * @brief generate a list object
 */
list_t* list_create();

/**
 * @brief frees all memory allocated to the list and its elements
 */
void list_free(list_t* list);


/**
 * @brief add data to the (end) tail of the list
 */
void list_push(list_t* list, void* data);


/**
 * @brief add data in the list as per the comparison function provided
 */
void list_add_ordered(list_t* list, void* data, int (*comp)(void*, void*));


/**
 * @brief Remove an item from the beginning of the list
 */
void* list_shift(list_t* list);


/**
 * @brief Remove an item at the end of the list
 */
void* list_pop(list_t* list);


#endif