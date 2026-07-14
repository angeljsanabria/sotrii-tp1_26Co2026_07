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

#ifndef TASK_ADC_ATTRIBUTE_H_
#define TASK_ADC_ATTRIBUTE_H_

/********************** CPP guard ********************************************/
#ifdef __cplusplus
extern "C" {
#endif

/********************** inclusions *******************************************/

/********************** macros ***********************************************/
#define TASK_ADC_DMA_TRANSFER_LENGTH	(1ul)

/********************** typedef **********************************************/
typedef enum
{
    ADC_MODE_DMA = 0,
    ADC_MODE_NOT_SUPPORTED,
    ADC_MODE_POLLING,
    ADC_MODE_INTERRUPT,
} adc_mode_hal_driver_t;

typedef enum
{
    ADC_PATTERN_LATEST_INPUT_ONLY = 0,
    ADC_PATTERN_NOT_SUPPORTED,
    ADC_PATTERN_SYNC,
    ADC_PATTERN_ASYNC,
} adc_pattern_driver_t;

/* Structure of Task */
typedef struct
{
	ADC_HandleTypeDef *device_id;

	TaskHandle_t		task_rx;
	QueueHandle_t		queue_rx_out;
    
	StaticQueue_t		queue_rx_out_struct;
	uint16_t			queue_rx_out_storage;

	SemaphoreHandle_t	sem_rx_dma_done;

	uint16_t			dma_sample;
	uint16_t			adc_dma_buff[TASK_ADC_DMA_TRANSFER_LENGTH];

	adc_mode_hal_driver_t	mode_use;
	adc_pattern_driver_t	pattern_use;
} task_adc_dta_t;

/********************** external data declaration ****************************/

/********************** external functions declaration ***********************/

/********************** End of CPP guard *************************************/
#ifdef __cplusplus
}
#endif

#endif /* TASK_ADC_ATTRIBUTE_H_ */

/********************** end of file ******************************************/
