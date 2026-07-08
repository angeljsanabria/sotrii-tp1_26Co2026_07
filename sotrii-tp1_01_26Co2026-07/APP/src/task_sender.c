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
#define G_TASK_SENDER_CNT_INI	0ul

#define TASK_SENDER_DEL_ZERO	(pdMS_TO_TICKS(0ul))
#define TASK_SENDER_DEL_MAX		(pdMS_TO_TICKS(250ul))

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/
const char *p_task_sender_wait_250mS		= "   ==> Task SENDER - Wait:   250mS";
adxl345_dta_t adxl345_data = {0};
/********************** external data declaration ****************************/
uint32_t g_task_sender_cnt;

/********************** external functions definition ************************/
/* Task thread */
void task_sender(void *parameters)
{
	/*  Declare & Initialize Task Function variables */
	g_task_sender_cnt = G_TASK_SENDER_CNT_INI;

	/* Serial LCD I2C Module–PCF8574  -> DEPRECATED 
	 * No tengo ese IC -> Se usa el acelerometro digital de 3 ejes (X, Y, Z)
	 * https://alselectro.wordpress.com/2016/05/12/serial-lcd-i2c-module-pcf8574/
	 * https://www.ti.com/product/PCF8574
 	 * dev_address = (address base | jumper less address)
	 * -------------------------------------------------------------------------------------
	 * Acelerometro digital de 3 ejes (X, Y, Z)
	 * Link: https://www.analog.com/media/en/technical-documentation/data-sheets/adxl345.pdf
	 * Funcionamiento simple:
	 * Configurar el registro de Power CTL en:
	 * Table 26. Register 0x2D:  
	 * - Bit 3: Measure = 1  -> ESTE!
	 * - Bit 2: Self Test = 0
	 * - Bit 1: Interrupt = 0
	 * - Bit 0: Link = 0
	 * Data en: 0x08; // Activa el bit "Measure"
	 * Una vez activada: data in the DATAX, DATAY, and DATAZ registers (Address 0x32 to Address 0x37).
	 * leer los datos de los registros de datos (X, Y, Z)
	 * Table 3. Register 0x32 a 0x37:
	 * - 16 bits: X data, Y data, Z data  (Data 0 y Data 1 de cada eje)
 	 */
	adxl345_data.i2c_addr = ADXL345_ADDRESS_SHIFTED;
	adxl345_data.initialized = false;
	adxl345_data.is_valid_sample = false;
	LOGGER_INFO("adxl345 INICIADO");

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());

	task_i2c_tx_rx_dta_t tx;

	tx.address = ADXL345_ADDRESS;

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
	{
		/* Update Task Counter */
		g_task_sender_cnt++;

		/* I2C Device Driver init: write POWER_CTL, avisar a task_receiver */
		if (adxl345_data.initialized == false)
		{
			tx.len = 2;
			tx.buffer[0] = ADXL345_REG_POWER_CTL;
			tx.buffer[1] = ADXL345_REG_POWER_CTL_SET_IN_MEASURE;
			write_i2c(&hi2c1, &tx);
			xSemaphoreGive(h_sem_adxl_init_write_done);
		}


    	/* Print out: Wait 250mS */
		LOGGER_INFO(p_task_sender_wait_250mS);
		vTaskDelay(TASK_SENDER_DEL_MAX);
	}
}

/********************** end of file ******************************************/
