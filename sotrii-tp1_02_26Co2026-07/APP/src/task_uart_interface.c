/*
 * Copyright (c) 2026 Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @author : Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>
 */

/********************** inclusions *******************************************/
/* Project includes */
#include "main.h"
#include "cmsis_os.h"

/* Demo includes */
#include "logger.h"
#include "dwt.h"

/* Application & Tasks includes */
#include "board.h"
#include "app.h"
#include "app_it.h"
#include "task_uart.h"
#include "task_uart_attribute.h"
#include "task_uart_interface.h"

/********************** macros and definitions *******************************/

/********************** internal data declaration ****************************/
task_uart_dta_t task_uart_dta;

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/

/********************** external functions definition ************************/
/* Interface functions */
void open_uart(UART_HandleTypeDef *h_uart_device, uart_mode_hal_driver_t set_mode,
		uart_pattern_driver_t set_pattern)
{
	BaseType_t ret;
	task_uart_dta_t *p_task_uart_dta = &task_uart_dta;

	p_task_uart_dta->device_id = h_uart_device;

	configASSERT(UART_MODE_NOT_SUPPORTED >= set_mode);
	p_task_uart_dta->mode_use = set_mode;

	configASSERT(UART_PATTERN_NOT_SUPPORTED >= set_pattern);
	p_task_uart_dta->pattern_use = set_pattern;

	p_task_uart_dta->active_tx.buffer = NULL;
	p_task_uart_dta->active_tx.len = 0U;
	p_task_uart_dta->active_rx.buffer = NULL;
	p_task_uart_dta->active_rx.len = 0U;

	p_task_uart_dta->queue_tx = xQueueCreate(5, sizeof(task_uart_spooler_tx_rx_dta_t));
	configASSERT(NULL != p_task_uart_dta->queue_tx);
	vQueueAddToRegistry(p_task_uart_dta->queue_tx, "Task UART Tx Queue Handle");

	p_task_uart_dta->queue_rx = xQueueCreate(10, sizeof(task_uart_spooler_tx_rx_dta_t));
	configASSERT(NULL != p_task_uart_dta->queue_rx);
	vQueueAddToRegistry(p_task_uart_dta->queue_rx, "Task UART Rx Queue Handle");

	p_task_uart_dta->queue_rx_out = xQueueCreate(10, sizeof(task_uart_spooler_tx_rx_dta_t));
	configASSERT(NULL != p_task_uart_dta->queue_rx_out);
	vQueueAddToRegistry(p_task_uart_dta->queue_rx_out, "Task UART Rx Out Queue Handle");

	p_task_uart_dta->sem_tx_it_done = xSemaphoreCreateBinary();
	configASSERT(NULL != p_task_uart_dta->sem_tx_it_done);
	vQueueAddToRegistry(p_task_uart_dta->sem_tx_it_done, "Semaforo binario IT tx UART");

	p_task_uart_dta->sem_rx_it_done = xSemaphoreCreateBinary();
	configASSERT(NULL != p_task_uart_dta->sem_rx_it_done);
	vQueueAddToRegistry(p_task_uart_dta->sem_rx_it_done, "Semaforo binario IT rx UART");

	ret = xTaskCreate(task_uart_tx, "Task UART Tx", (configMINIMAL_STACK_SIZE),
			(void *)p_task_uart_dta,
			(tskIDLE_PRIORITY + 1ul), &p_task_uart_dta->task_tx);
	configASSERT(pdPASS == ret);

	ret = xTaskCreate(task_uart_rx, "Task UART Rx", (configMINIMAL_STACK_SIZE),
			(void *)p_task_uart_dta,
			(tskIDLE_PRIORITY + 1ul), &p_task_uart_dta->task_rx);
	configASSERT(pdPASS == ret);
}

void release_uart(UART_HandleTypeDef *h_uart_device)
{
	task_uart_dta_t *p_task_uart_dta = &task_uart_dta;

	p_task_uart_dta->device_id = h_uart_device;
	p_task_uart_dta->mode_use = UART_MODE_NOT_SUPPORTED;
	p_task_uart_dta->pattern_use = UART_PATTERN_NOT_SUPPORTED;

	if (p_task_uart_dta->device_id == h_uart_device)
	{
		vQueueUnregisterQueue(p_task_uart_dta->queue_tx);
		vQueueDelete(p_task_uart_dta->queue_tx);

		vQueueUnregisterQueue(p_task_uart_dta->queue_rx);
		vQueueDelete(p_task_uart_dta->queue_rx);

		vQueueUnregisterQueue(p_task_uart_dta->queue_rx_out);
		vQueueDelete(p_task_uart_dta->queue_rx_out);

		vTaskDelete(p_task_uart_dta->task_tx);
		vTaskDelete(p_task_uart_dta->task_rx);

		vSemaphoreDelete(p_task_uart_dta->sem_tx_it_done);
		vSemaphoreDelete(p_task_uart_dta->sem_rx_it_done);

		p_task_uart_dta->active_tx.buffer = NULL;
		p_task_uart_dta->active_tx.len = 0U;
		p_task_uart_dta->active_rx.buffer = NULL;
		p_task_uart_dta->active_rx.len = 0U;
	}
}

void write_uart(UART_HandleTypeDef *h_uart_device, task_uart_spooler_tx_rx_dta_t *tx_data)
{
	UNUSED(h_uart_device);
	UNUSED(tx_data);
}

void read_uart(UART_HandleTypeDef *h_uart_device)
{
	UNUSED(h_uart_device);
}

BaseType_t uart_get_rx_data(task_uart_spooler_tx_rx_dta_t *rx_data, TickType_t ticks_to_wait)
{
	UNUSED(rx_data);
	UNUSED(ticks_to_wait);
	return pdFALSE;
}

void ioctl_uart(UART_HandleTypeDef *h_uart_device)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(h_uart_device);
}

/********************** end of file ******************************************/
