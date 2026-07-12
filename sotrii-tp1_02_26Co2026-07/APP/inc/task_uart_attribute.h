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

#ifndef TASK_UART_ATTRIBUTE_H_
#define TASK_UART_ATTRIBUTE_H_

/********************** CPP guard ********************************************/
#ifdef __cplusplus
extern "C" {
#endif

/********************** inclusions *******************************************/
#include <stdbool.h>

/********************** macros ***********************************************/
#define UART_IN_OUT_MAX_SIZE  128
#define UART_RX_TIMEOUT_MS    100    // Tiempo de espera de respuesta del otro dispositivo
#define UART_RX_END_CHAR      0x0A   // Fin de linea (LF) o /n

/********************** typedef **********************************************/
typedef enum
{
    UART_MODE_INTERRUPT = 0,
    UART_MODE_NOT_SUPPORTED,
    UART_MODE_POLLING,
    UART_MODE_DMA,
} uart_mode_hal_driver_t;

typedef enum
{
    UART_PATTERN_ASYNC = 0,
    UART_PATTERN_NOT_SUPPORTED,
    UART_PATTERN_SYNC,
    UART_PATTERN_LATEST_INPUT_ONLY,
} uart_pattern_driver_t;

typedef struct
{
    uint8_t  *buffer;
    uint16_t  len;
} task_uart_spooler_tx_rx_dta_t;

/* Structure of Task */
typedef struct
{
	UART_HandleTypeDef *device_id;

	TaskHandle_t		task_tx;
	QueueHandle_t		queue_tx;

	TaskHandle_t		task_rx;
	QueueHandle_t		queue_rx;

	QueueHandle_t		queue_rx_out;

	SemaphoreHandle_t	sem_tx_it_done;
	SemaphoreHandle_t	sem_rx_it_done;

	bool 				is_valid_answer;

    task_uart_spooler_tx_rx_dta_t  active_tx;
    task_uart_spooler_tx_rx_dta_t  active_rx;

	uart_mode_hal_driver_t	mode_use;
	uart_pattern_driver_t	pattern_use;
} task_uart_dta_t;

/* Structure of UART Tx */


/********************** external data declaration ****************************/

/********************** external functions declaration ***********************/

/********************** End of CPP guard *************************************/
#ifdef __cplusplus
}
#endif

#endif /* TASK_UART_ATTRIBUTE_H_ */

/********************** end of file ******************************************/
