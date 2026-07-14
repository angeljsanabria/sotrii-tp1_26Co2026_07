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
#include "task_adc.h"
#include "task_adc_attribute.h"
#include "task_adc_interface.h"

/********************** macros and definitions *******************************/

/********************** internal data declaration ****************************/
task_adc_dta_t task_adc_dta;

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/

/********************** external functions definition ************************/
/* Interface functions */
void open_adc(ADC_HandleTypeDef *h_adc_device, adc_mode_hal_driver_t set_mode,
		adc_pattern_driver_t set_pattern)
{
	BaseType_t ret;
	task_adc_dta_t *p_task_adc_dta = &task_adc_dta;

	configASSERT(NULL != h_adc_device);
	p_task_adc_dta->device_id = h_adc_device;

	configASSERT(ADC_MODE_NOT_SUPPORTED >= set_mode);
	p_task_adc_dta->mode_use = set_mode;

	configASSERT(ADC_PATTERN_NOT_SUPPORTED >= set_pattern);
	p_task_adc_dta->pattern_use = set_pattern;

	p_task_adc_dta->dma_sample = 0;
	p_task_adc_dta->adc_dma_buff[0] = 0;

	if (HAL_ADCEx_Calibration_Start(p_task_adc_dta->device_id, ADC_SINGLE_ENDED) != HAL_OK)
	{
		LOGGER_INFO("ADC calibration error");
	}

	/* Before a queue is used it must be explicitly created.
	 * Check the queue was created successfully.
	 * Add queue to registry. */
	p_task_adc_dta->queue_rx_out = xQueueCreateStatic(1, sizeof(uint16_t),
			(uint8_t *)&p_task_adc_dta->queue_rx_out_storage,
			&p_task_adc_dta->queue_rx_out_struct);
	configASSERT(NULL != p_task_adc_dta->queue_rx_out);
	vQueueAddToRegistry(p_task_adc_dta->queue_rx_out, "Task ADC Rx Out Queue Handle");

	p_task_adc_dta->sem_rx_dma_done = xSemaphoreCreateBinary();
	configASSERT(NULL != p_task_adc_dta->sem_rx_dma_done);
	vQueueAddToRegistry(p_task_adc_dta->sem_rx_dma_done, "Semaforo binario DMA rx ADC");

	ret = xTaskCreate(task_adc, "Task ADC Rx", (2 * configMINIMAL_STACK_SIZE),
			(void *)p_task_adc_dta,
			(tskIDLE_PRIORITY + 1ul), &p_task_adc_dta->task_rx);
	configASSERT(pdPASS == ret);
}

void release_adc(ADC_HandleTypeDef *h_adc_device)
{
	task_adc_dta_t *p_task_adc_dta = &task_adc_dta;

	p_task_adc_dta->device_id = h_adc_device;
	p_task_adc_dta->mode_use = ADC_MODE_NOT_SUPPORTED;
	p_task_adc_dta->pattern_use = ADC_PATTERN_NOT_SUPPORTED;

	if (p_task_adc_dta->device_id == h_adc_device)
	{
		vQueueUnregisterQueue(p_task_adc_dta->queue_rx_out);
		vQueueDelete(p_task_adc_dta->queue_rx_out);

		vTaskDelete(p_task_adc_dta->task_rx);

		vSemaphoreDelete(p_task_adc_dta->sem_rx_dma_done);

		p_task_adc_dta->dma_sample = 0;
	}
}

void write_adc(ADC_HandleTypeDef *h_adc_device)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(h_adc_device);
}

void read_adc(ADC_HandleTypeDef *h_adc_device)
{
	task_adc_dta_t *p_task_adc_dta = &task_adc_dta;

	p_task_adc_dta->device_id = h_adc_device;

	if (p_task_adc_dta->device_id == h_adc_device)
	{
		if (p_task_adc_dta->mode_use == ADC_MODE_DMA)
		{
			if (p_task_adc_dta->pattern_use == ADC_PATTERN_LATEST_INPUT_ONLY)
			{
				/* LIO: no arranca HAL, el dato se obtiene con adc_get_rx_data() */
			}
			else
			{
				LOGGER_INFO("ADC Patron error");
			}
		}
		else
		{
			LOGGER_INFO("ADC Modo error");
		}
	}
}

BaseType_t adc_get_rx_data(uint16_t *sample, TickType_t ticks_to_wait)
{
	task_adc_dta_t *p_task_adc_dta = &task_adc_dta;

	if (xQueueReceive(p_task_adc_dta->queue_rx_out, sample, ticks_to_wait) == pdTRUE)
	{
		return pdTRUE;
	}

	*sample = p_task_adc_dta->dma_sample;
	return pdFALSE;
}

void ioctl_adc(ADC_HandleTypeDef *h_adc_device)
{
	/* Prevent unused argument(s) compilation warning */
	/* No es necesario para esta actividad */
	UNUSED(h_adc_device);
}

/********************** end of file ******************************************/
