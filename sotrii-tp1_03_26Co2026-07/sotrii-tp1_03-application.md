# CESE - Sistemas Operativos de Tiempo Real

## Trabajo Práctico N°: 1 – Device Driver de FreeRTOS

### TP1 – Actividad 03 – Device Driver ADC de FreeRTOS

## Paso 01

Proyecto generado e importado. Compila en STM32CubeIDE.

---

## Paso 02

Archivo de entrega creado.

---

## Paso 03: Analisis del codigo fuente base

Analizar y explicar (en español), el funcionamiento del codigo fuente contenido en los archivos adjuntos: `app.c`, `app_it.c`, `task_receiver.c`, `task_adc.c`, `task_adc_interface.c`, `task_adc_interface.h` y `freertos.c`.

---

### 1. Vision general de la arquitectura

El proyecto proporciona el codigo base para implementar un **device driver de ADC** sobre FreeRTOS en la placa NUCLEO-L4R5ZI. Sigue una arquitectura de sistema disparado por eventos (ETS) donde el acceso al periferico ADC estara encapsulado en tareas dedicadas (gatekeepers) y la comunicacion se realizara mediante colas. Esto aísla a las tareas de aplicacion del acceso directo al hardware (HAL).

### 2. `app.c` — Inicializacion de la aplicacion

Es el punto de entrada de la logica de usuario (invocado antes de iniciar el scheduler).
- Inicializa contadores globales de la aplicacion.
- Crea la tarea `task_receiver` con prioridad 1 (`tskIDLE_PRIORITY + 1ul`).
- Llama a `open_adc(&hadc1)` para inicializar el driver del ADC.
- Inicializa las interrupciones (`app_it_init()`) y el contador de ciclos DWT (`cycle_counter_init()`) para medir tiempos de ejecucion (WCET).

### 3. `app_it.c` — Interrupciones de aplicacion

Centraliza las rutinas de servicio de interrupcion (ISR) a nivel de aplicacion.
- Contiene `HAL_GPIO_EXTI_Callback` para manejar eventos de botones externos.
- Contiene `HAL_ADC_ConvCpltCallback`, que es invocada por el HAL cuando finaliza una conversion ADC (util para modos de interrupcion o DMA). Actualmente, esta ISR levanta un flag (`hal_xxxx_callback_flag`), incrementa un contador y registra el tiempo usando el DWT. En un driver completo, aqui se enviaria una notificacion o dato a una cola hacia el gatekeeper.

### 4. `task_receiver.c` — Tarea de aplicacion

Actua como la tarea consumidora de los datos del ADC.
- Actualmente es un **stub** (plantilla) que se ejecuta en un bucle infinito, incrementa un contador, imprime un log y se bloquea por 250 ms usando `vTaskDelay`.
- En el futuro, esta tarea llamara a `read_adc()` para obtener las muestras procesadas por el driver de forma transparente.

### 5. `task_adc.c` — Tarea interna del driver (Gatekeeper)

Contiene la implementacion de la tarea `task_adc_rx`, que actuara como **gatekeeper de recepcion** del ADC.
- Por ahora es un esqueleto que hace parpadear un LED (`LED_A_PORT`), mide su tiempo de ejecucion con el DWT y se bloquea 250 ms.
- Su proposito final sera gestionar las lecturas del ADC (ya sea por polling o esperando notificaciones de la ISR) y enviar los datos a la cola de recepcion del driver, siendo la unica tarea autorizada a interactuar con el periferico.

### 6. `task_adc_interface.c` y `task_adc_interface.h` — Interfaz del Driver

Definen e implementan la API publica del device driver con semantica tipo UNIX/POSIX: `open_adc`, `release_adc`, `write_adc`, `read_adc` e `ioctl_adc`.
- En este codigo base, todas las funciones son **stubs vacios** (solo contienen la macro `UNUSED()`).
- El objetivo del TP es completar estas funciones para que gestionen las colas, configuren el modo de operacion (Polling, Interrupt, DMA) y sincronicen a las tareas de aplicacion con la tarea gatekeeper.

### 7. `freertos.c` — Hooks del RTOS

Implementa las funciones de callback (hooks) del kernel de FreeRTOS:
- `vApplicationIdleHook`: Se ejecuta cuando no hay tareas listas. Incrementa un contador de idle.
- `vApplicationTickHook`: Se ejecuta en cada tick del sistema desde la ISR del SysTick. Incrementa un contador de ticks.
- `vApplicationStackOverflowHook`: Detiene la ejecucion (`configASSERT(0)`) si detecta que una tarea desbordo su pila, fundamental para la depuracion del sistema.


## Paso 06: Device Driver ADC de FreeRTOS (DMA + Latest Input Only)

Diseno, implementacion y uso del device driver ADC sobre FreeRTOS en la NUCLEO-L4R5ZI, con `ADC1` (`hadc1`) en el pin **PC0** (ADC1_IN1, Single-ended).

- Aplicacion: `task_receiver` cada 250 ms llama a `read_adc()` (no-op en patron LIO) y luego `adc_get_rx_data()` para obtener el ultimo valor disponible, y lo imprime por `LOGGER_INFO`.
- Patron asincrono: **Latest Input Only (LIO)**. El gatekeeper `task_adc` dispara una conversion DMA por ciclo (sin demora) y sobrescribe el ultimo valor en una cola de tamaño 1 con `xQueueOverwrite()`; la tarea de aplicacion nunca bloquea al productor y solo recibe el dato mas reciente disponible.
- Tipo DMA (Hardware): Configurado via CubeMX con `ContinuousConvMode = DISABLE`, `DMAContinuousRequests = DISABLE`, `NbrOfConversion = 1` y `Overrun = ADC_OVR_DATA_OVERWRITTEN`. El DMA1_Channel1 opera en modo **Normal** (no Circular), HalfWord, con `MemInc = DISABLE` (siempre la misma direccion de memoria). Cada `HAL_ADC_Start_DMA(..., Length = TASK_ADC_DMA_TRANSFER_LENGTH = 1)` dispara exactamente una conversion; el gatekeeper hace `Start_DMA` / `Stop_DMA` en cada ciclo (no hay conversion continua en hardware).
- Calibracion: `HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED)` se ejecuta una vez en `open_adc()`, antes de crear la cola y la tarea gatekeeper.
- Interrupciones NVIC: `DMA1_Channel1_IRQn` y `ADC1_IRQn` en prioridad **6** (por debajo de `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5`, apto para llamar funciones `FromISR` de FreeRTOS). `ADC1_IRQn` se habilita explicitamente porque en STM32L4, con DMA y `Overrun = OVERWRITTEN`, el flag de overrun (`ADC_FLAG_OVR`) se atiende por `HAL_ADC_IRQHandler()`, no por el handler de DMA.
- Sincronizacion ISR -> gatekeeper: `HAL_ADC_ConvCpltCallback()` (en `app_it.c`) hace `xSemaphoreGiveFromISR(sem_rx_dma_done, ...)` seguido de `portYIELD_FROM_ISR()`. El gatekeeper `task_adc` espera con `xSemaphoreTake(sem_rx_dma_done, portMAX_DELAY)` tras cada `Start_DMA`.
- Estructura del dispositivo: Definida en `task_adc_attribute.h` como `task_adc_dta_t` (`device_id`, `task_rx`, `queue_rx_out` + `queue_rx_out_struct`/`queue_rx_out_storage` para allocation estatica, `sem_rx_dma_done`, `dma_sample`, `adc_dma_buff[]`, `mode_use`, `pattern_use`).
- Almacenamiento en queue, allocation estatica: `xQueueCreateStatic(1, sizeof(uint16_t), &queue_rx_out_storage, &queue_rx_out_struct)` en `open_adc()`; sin `pvPortMalloc`/`vPortFree` en el camino de datos del ADC.
- Tarea Gatekeeper: **Una sola**, de recepcion (`task_adc`, ADC es periferico de entrada, no hay gatekeeper de transmision). Creada en `open_adc()` con prioridad `tskIDLE_PRIORITY + 1` y parametro `&task_adc_dta` (contiene la referencia al canal ADC en `device_id`).
- Funciones de interfaz del driver: API publica en `task_adc_interface.h` / `task_adc_interface.c` (`open_adc`, `release_adc`, `write_adc`, `read_adc`, `adc_get_rx_data`, `ioctl_adc`).
- Desarrollo de `write_adc()` e `ioctl_adc()`: No fue necesario su uso para este TP (ADC es de solo lectura). Ambas son funciones stub que solo evitan warnings de argumento sin uso.

### WCET — medicion y registro

Medicion con DWT, STM32CubeIDE Live Expressions (NUCLEO-L4R5ZI):

| Medicion | Variable | WCET [us] |
|----------|----------|-----------|
| Gatekeeper RX (`task_adc`) | `g_task_xxxx_rx_runtime_us` | 32 |
| `read_adc()` + `adc_get_rx_data()` LIO | `g_read_adc_wcet_us` | 6 |


### Video del tp funcionando
[Video / archivo en Drive](https://drive.google.com/file/d/1NTRfU5eaeDBt7i3yezerCBRWDXLiqsH3/view?usp=drive_link)


### Muestra de logs IDE

```text
[info]  
[info] app_init is running - Tick [mS] = 0
[info]  app is a RTOS - Event-Triggered Systems (ETS)
[info]  app is a sotrii-tp1_03-application: Demo Code
[info]  app is a (Source => CESE - Sistemas Operativos de Tiempo Real)
[info]  
[info] Task ADC Rx is running - Tick [mS] =   0
[info]  
[info]   Task Receiver is running - Tick [mS] = 1
[info]    ==> Task RECEIVER - ADC: 226
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 229
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 210
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 204
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 190
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 174
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 161
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 143
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 127
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 109
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 103
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 116
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 105
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 105
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 104
[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task RECEIVER - ADC: 104
```