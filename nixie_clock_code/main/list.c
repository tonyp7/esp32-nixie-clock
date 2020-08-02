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

@file list.c
@author Tony Pottier
@brief Simple C linked list

@see https://idyl.io
@see https://github.com/tonyp7/esp32-nixie-clock

*/

#include <stdlib.h>
#include "list.h"

list_t* list_create()
{

	list_t* list = malloc(sizeof(list_t));

	list->first = NULL;
	list->last = NULL;
	list->count = 0;

	return list;
}


void list_free(list_t* list) 
{

	node_t* next;
	node_t* curr = list->first;

	while (curr != NULL) {
		next = curr->next;
		free(curr);
		curr = next;
	}

	free(list);
}


void list_push(list_t* list, void* data) 
{
	node_t* node = malloc(sizeof(node_t));
	node->data = data;
	node->next = NULL; /* sent to the end so there is no next */

	if ( list->last != NULL) {
		node_t* last = list->last;
		last->next = node;
		list->last = node;
	}
	else {

		list->first = node;
		list->last = node;
	}

	list->count++;
}



void list_add_ordered(list_t* list, void* data, int (*comp)(void*, void*))
{

	if (list->count == 0) return list_push(list, data);

	node_t* current = list->first;
	node_t* previous = NULL;

	/* create the node to add to the list */
	node_t* node = malloc(sizeof(node_t));
	node->data = data;
	

	/* find spot in list to add the new element */
	while (current != NULL && current->data != NULL && comp(current->data, data) < 0) {
		previous = current;
		current = current->next;
	}
	node->next = current;

	if (previous == NULL) {
		/* added at the beginning of the list */
		list->first = node;

		/* in case this was the first element to be added */
		if (list->last == NULL) {
			list->last = node;
		}
	}
	else {

		/* the end of the list is the newly inserted node ? */
		if (list->last == previous) {
			list->last = node;
		}
		previous->next = node;
	}


	list->count++;

}


void* list_shift(list_t* list) 
{
	if (list->first) {
		node_t* first = list->first;
		void* shift_data = first->data;
		
		if (list->last == list->first) { /*edge case where list contains one element */
			list->first = NULL;
			list->last = NULL;
		}
		else {
			list->first = first->next;
		}

		free(first);
		list->count--;

		return shift_data;
	}
	else {
		return NULL;
	}
}

void* list_pop(list_t* list)
{
	if (list->last) {

		node_t* last = list->last;
		void* pop_data = last->data;

		if (list->last == list->first) { /*edge case where list contains one element */
			list->first = NULL;
			list->last = NULL;
		}
		else {
			node_t* current = list->first;
			while (current != NULL && current->next != list->last) {
				current = current->next;
			}
			current->next = NULL;
			list->last = current;
		}

		free(last);
		list->count--;
		return pop_data;

	}
	else {
		return NULL;
	}
}