/*
 * clock.h
 *
 *  Created on: 11 May 2019
 *      Author: tony
 */

#ifndef MAIN_CLOCK_H_
#define MAIN_CLOCK_H_

/** @brief Defines the task priority of the clock (main task).
 *  This should be the highest priority task unless very specific reason. Default: 10.
 */
#define CLOCK_TASK_PRIORITY				CONFIG_CLOCK_TASK_PRIORITY


#define GPIO_INPUT_IO_4 				4
#define ESP_INTR_FLAG_DEFAULT 			0

void clock_tick();
void clock_task(void *pvParameter);

#endif /* MAIN_CLOCK_H_ */
