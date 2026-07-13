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
#include "task_uart_attribute.h"

/********************** macros and definitions *******************************/
#define G_TASK_XXXX_CNT_INI	0
#define G_TASK_XXXX_RUNTIME_US_INI	0

#define TASK_XXXX_DEL_ZERO	(pdMS_TO_TICKS(0))
#define TASK_XXXX_DEL_MAX	(pdMS_TO_TICKS(250))

/********************** internal data declaration ****************************/

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/
void task_uart_tx(void *parameters);
void task_uart_rx(void *parameters);

/********************** internal data definition *****************************/
const char *p_task_uart_rx_wait_250mS	= "   ==> Task UART RX - Wait:   250mS";

/********************** external data declaration ****************************/
uint32_t g_task_xxxx_tx_cnt;
uint32_t g_task_xxxx_tx_runtime_us;

uint32_t g_task_xxxx_rx_cnt;
uint32_t g_task_xxxx_rx_runtime_us;

/********************** external functions definition ************************/
/* Task UART TX thread */
void task_uart_tx(void *parameters)
{
	
	task_uart_dta_t *p_task_uart_tx_dta = (task_uart_dta_t *)parameters;

	g_task_xxxx_tx_cnt = G_TASK_XXXX_CNT_INI;
	g_task_xxxx_tx_runtime_us = G_TASK_XXXX_RUNTIME_US_INI;
	HAL_StatusTypeDef ret = HAL_ERROR;

	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - Tick [mS] = %3d", pcTaskGetName(NULL), (int)xTaskGetTickCount());

	for (;;)
	{
		g_task_xxxx_tx_cnt++;

		xQueueReceive(p_task_uart_tx_dta->queue_tx, &p_task_uart_tx_dta->active_tx, portMAX_DELAY);

		cycle_counter_reset();

		if (p_task_uart_tx_dta->mode_use == UART_MODE_INTERRUPT)
		{
			ret = HAL_UART_Transmit_IT(p_task_uart_tx_dta->device_id,
					p_task_uart_tx_dta->active_tx.buffer,
					p_task_uart_tx_dta->active_tx.len);

			if (ret == HAL_OK)
			{
				xSemaphoreTake(p_task_uart_tx_dta->sem_tx_it_done, portMAX_DELAY);
			}
			else
			{
				LOGGER_INFO("UART TX IT error");
			}

			vPortFree(p_task_uart_tx_dta->active_tx.buffer);
			p_task_uart_tx_dta->active_tx.buffer = NULL;
			p_task_uart_tx_dta->active_tx.len = 0;
		}
		else
		{
			LOGGER_INFO("UART mode error");
			vPortFree(p_task_uart_tx_dta->active_tx.buffer);
			p_task_uart_tx_dta->active_tx.buffer = NULL;
			p_task_uart_tx_dta->active_tx.len = 0;
		}

		g_task_xxxx_tx_runtime_us = cycle_counter_get_time_us();

    	/* Print out: Wait 250mS */
		//LOGGER_INFO(p_task_uart_tx_wait_250mS);
		//vTaskDelay(TASK_XXXX_DEL_MAX);
	}
}

/* Task UART RX thread */
void task_uart_rx(void *parameters)
{
	
	/*  Declare & Initialize Task Function variables */
	HAL_StatusTypeDef ret = HAL_ERROR;
	task_uart_dta_t *p_task_uart_rx_dta = (task_uart_dta_t *)parameters;
	task_uart_spooler_tx_rx_dta_t rx_req;
	task_uart_spooler_tx_rx_dta_t rx_out;

	g_task_xxxx_rx_cnt = G_TASK_XXXX_CNT_INI;
	g_task_xxxx_rx_runtime_us = G_TASK_XXXX_RUNTIME_US_INI;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - Tick [mS] = %3d", pcTaskGetName(NULL), (int)xTaskGetTickCount());

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
	{
		/* Update Task Counter */
		g_task_xxxx_rx_cnt++;

		xQueueReceive(p_task_uart_rx_dta->queue_rx, &rx_req, portMAX_DELAY);

		cycle_counter_reset();

		p_task_uart_rx_dta->active_rx.buffer = (uint8_t *)pvPortMalloc(UART_IN_OUT_MAX_SIZE);
		configASSERT(NULL != p_task_uart_rx_dta->active_rx.buffer);
		p_task_uart_rx_dta->active_rx.len = 0;
		p_task_uart_rx_dta->is_valid_answer = false;

		if (p_task_uart_rx_dta->mode_use == UART_MODE_INTERRUPT)
		{
			while (p_task_uart_rx_dta->active_rx.len < UART_IN_OUT_MAX_SIZE)
			{
				ret = HAL_UART_Receive_IT(p_task_uart_rx_dta->device_id, &p_task_uart_rx_dta->active_rx.buffer[p_task_uart_rx_dta->active_rx.len], 1);

				if (ret != HAL_OK)
				{
					break;
				}

				if (xSemaphoreTake(p_task_uart_rx_dta->sem_rx_it_done, pdMS_TO_TICKS(UART_RX_TIMEOUT_MS)) != pdTRUE)
				{
					(void)HAL_UART_AbortReceive_IT(p_task_uart_rx_dta->device_id);
					break;
				}

				p_task_uart_rx_dta->active_rx.len++;

				if (UART_RX_END_CHAR == p_task_uart_rx_dta->active_rx.buffer[p_task_uart_rx_dta->active_rx.len - 1])
				{
					p_task_uart_rx_dta->is_valid_answer = true;
					break;
				}
			}

			if ((p_task_uart_rx_dta->active_rx.len > 1) && (p_task_uart_rx_dta->is_valid_answer))		// Mensaje con length valido (sin contar /n)
			{
				rx_out.buffer = p_task_uart_rx_dta->active_rx.buffer;
				rx_out.len = p_task_uart_rx_dta->active_rx.len;
				xQueueSend(p_task_uart_rx_dta->queue_rx_out, &rx_out, portMAX_DELAY);
				p_task_uart_rx_dta->active_rx.buffer = NULL;
			}
			else
			{
				vPortFree(p_task_uart_rx_dta->active_rx.buffer);
				p_task_uart_rx_dta->active_rx.buffer = NULL;
			}

			p_task_uart_rx_dta->is_valid_answer = false;
			p_task_uart_rx_dta->active_rx.len = 0;
		}
		else
		{
			LOGGER_INFO("UART mode error");
			vPortFree(p_task_uart_rx_dta->active_rx.buffer);
			p_task_uart_rx_dta->active_rx.buffer = NULL;
			p_task_uart_rx_dta->is_valid_answer = false;
			p_task_uart_rx_dta->active_rx.len = 0;
		}

		g_task_xxxx_rx_runtime_us = cycle_counter_get_time_us();

    	/* Print out: Wait 250mS */
		//LOGGER_INFO(p_task_uart_rx_wait_250mS);
		//vTaskDelay(TASK_XXXX_DEL_MAX);
	}
}

/********************** end of file ******************************************/
