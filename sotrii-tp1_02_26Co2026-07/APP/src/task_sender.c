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
#include <string.h>

#include "board.h"
#include "app.h"
#include "task_uart_interface.h"

/********************** macros and definitions *******************************/
#define G_TASK_SENDER_CNT_INI	0ul

#define TASK_SENDER_DEL_ZERO	(pdMS_TO_TICKS(0ul))
#define TASK_SENDER_DEL_MAX		(pdMS_TO_TICKS(250ul))

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/
static const char * const p_task_sender_cmd_msg_1 = "CMD1\r\n";
static const char * const p_task_sender_cmd_msg_2 = "CMD2\r\n";
static const char * const p_task_sender_cmd_msg_3 = "CMD3\r\n";
static const uint8_t p_task_sender_cmd_msg_count_max = 3;

const char *p_task_sender_wait_250mS		= "   ==> Task SENDER - Wait:   250mS";

/********************** external data declaration ****************************/
uint32_t g_task_sender_cnt;

/********************** external functions definition ************************/
/* Task thread */
void task_sender(void *parameters)
{
	/*  Declare & Initialize Task Function variables */
	uint8_t cmd_idx = 0;
	uint16_t tx_len = 0;
	task_uart_spooler_tx_rx_dta_t tx;

	UNUSED(parameters);

	g_task_sender_cnt = G_TASK_SENDER_CNT_INI;

	// _TODO Bajar la prioridad a la misma que el receiver
	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
	{
		const char *p_cmd = NULL;

		g_task_sender_cnt++;

		if (cmd_idx == 0)
		{
			p_cmd = p_task_sender_cmd_msg_1;
		}
		else if (cmd_idx == 1)
		{
			p_cmd = p_task_sender_cmd_msg_2;
		}
		else
		{
			p_cmd = p_task_sender_cmd_msg_3;
		}

		tx_len = (uint16_t)strlen(p_cmd);
		tx.buffer = (uint8_t *)pvPortMalloc(tx_len);
		if (tx.buffer != NULL)
		{
			(void)memcpy(tx.buffer, p_cmd, tx_len);
			tx.len = tx_len;
			write_uart(&huart2, &tx);
			LOGGER_INFO("   ==> Task SENDER - TX: %s", p_cmd);
		}else{
			LOGGER_INFO("   ==> Task SENDER - Fail alloc ");
		}

		// loop msgs
		cmd_idx++;
		if (cmd_idx >= p_task_sender_cmd_msg_count_max)		cmd_idx = 0;

		LOGGER_INFO(p_task_sender_wait_250mS);
		vTaskDelay(TASK_SENDER_DEL_MAX);
	}
}

/********************** end of file ******************************************/
