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
#include "task_i2c_interface.h"
#include "adxl345_data.h"

/********************** macros and definitions *******************************/
#define G_TASK_RECEIVER_CNT_INI	0ul

#define TASK_RECEIVER_DEL_ZERO		(pdMS_TO_TICKS(0ul))
#define TASK_RECEIVER_DEL_MAX		(pdMS_TO_TICKS(250ul))

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/
const char *p_task_receiver_wait_250mS		= "   ==> Task RECEIVER - Wait:   250mS";

/********************** external data declaration ****************************/
uint32_t g_task_receiver_cnt;
uint32_t g_read_i2c_wcet_us;	/* WCET read_i2c() Sync: Live Expression en CubeIDE */

/********************** external functions definition ************************/
/* Task thread */
void task_receiver(void *parameters)
{
	/*  Declare & Initialize Task Function variables */
	g_task_receiver_cnt = G_TASK_RECEIVER_CNT_INI;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());

	task_i2c_tx_rx_dta_t rx;

	rx.address = ADXL345_ADDRESS;

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
    {
		/* Update Task Counter */
		g_task_receiver_cnt++;

		if (adxl345_data.initialized == false)
		{
			// Inicacion de dispositivo
			xSemaphoreTake(h_sem_adxl_init_write_done, portMAX_DELAY);

			rx.read_add = ADXL345_REG_POWER_CTL;
			rx.rx_type  = I2C_RX_MAP_REG;
			rx.len      = 1;
			cycle_counter_reset();
			read_i2c(&hi2c1, &rx);
			g_read_i2c_wcet_us = cycle_counter_get_time_us();

			if (rx.len > 0){
				if (rx.buffer[0] == ADXL345_REG_POWER_CTL_SET_IN_MEASURE)
				{
					adxl345_data.initialized = true;
					LOGGER_INFO("   ==> ADXL345 init OK (POWER_CTL = 0x%02X)", rx.buffer[0]);
				}
			}
		}
		else
		{
			// Lectura de ejes si esta iniciado
			// xSemaphoreTake(h_sem_adxl_init_write_done, portMAX_DELAY);

			rx.read_add = ADXL345_BASE_REG_DATA;
			rx.rx_type  = I2C_RX_MAP_REG;
			rx.len      = ADXL345_DATA_LENGTH;
			cycle_counter_reset();
			read_i2c(&hi2c1, &rx);
			g_read_i2c_wcet_us = cycle_counter_get_time_us();

			if (rx.len > 0){
				adxl345_data.sample.x = (int16_t)((uint16_t)rx.buffer[0] | ((uint16_t)rx.buffer[1] << 8));
				adxl345_data.sample.y = (int16_t)((uint16_t)rx.buffer[2] | ((uint16_t)rx.buffer[3] << 8));
				adxl345_data.sample.z = (int16_t)((uint16_t)rx.buffer[4] | ((uint16_t)rx.buffer[5] << 8));
				adxl345_data.is_valid_sample = true;
			}
		}

		if(adxl345_data.is_valid_sample){
			LOGGER_INFO("   ==> ADXL345 X=%d Y=%d Z=%d", adxl345_data.sample.x, adxl345_data.sample.y, adxl345_data.sample.z);
			adxl345_data.is_valid_sample = false;
		}

    	/* Print out: Wait 250mS */
		LOGGER_INFO(p_task_receiver_wait_250mS);
		vTaskDelay(TASK_RECEIVER_DEL_MAX);
	}
}

/********************** end of file ******************************************/
