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

- Aplicacion: `task_receiver` se encarga de consumir los datos del ADC. 
- Tipo DMA (Hardware): Configurado via CubeMX con `ContinuousConvMode = ENABLE`, `DMAContinuousRequests = ENABLE` y `OVR_DATA_OVERWRITTEN`. El DMA1_Channel1 opera en modo Circular, HalfWord, con MemInc DISABLE para sobrescribir siempre la misma direccion de memoria.
- 