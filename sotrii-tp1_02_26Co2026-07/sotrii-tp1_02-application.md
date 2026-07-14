# CESE - Sistemas Operativos de Tiempo Real

## Trabajo Práctico N°: 1 – Device Driver de FreeRTOS

### TP1 – Actividad 02 – Device Driver UART de FreeRTOS

## Paso 01

Proyecto generado e importado. Compila en STM32CubeIDE.

---

## Paso 02

Archivo de entrega creado.

---

## Paso 03: Analisis del codigo fuente base

Analizar y explicar (en español), el funcionamiento del codigo fuente contenido en los archivos adjuntos: `app.c`, `app_it.c`, `task_sender.c`, `task_receiver.c`, `task_uart.c`, `task_uart_interface.c` y `freertos.c`.

---

### 1. Vision general de la arquitectura

El proyecto implementa una **demo de sistema disparado por eventos (ETS - Event-Triggered Systems)** sobre FreeRTOS en la NUCLEO-L4R5ZI. El objetivo del TP es construir un **device driver UART** encapsulado como tareas del RTOS, de modo que el resto de la aplicacion no acceda directamente al periferico HAL.

La arquitectura prevista se organiza en capas, en forma analoga a la Actividad 01 (I2C):

```
main.c
  └── app_init()                         (app.c)
        ├── xTaskCreate task_sender
        ├── xTaskCreate task_receiver
        ├── open_uart(&huart2)           (task_uart_interface.c)  ← stub
        │     ├── (pendiente) colas TX/RX
        │     ├── (pendiente) xTaskCreate task_uart_tx
        │     └── (pendiente) xTaskCreate task_uart_rx
        └── app_it_init()                (app_it.c)

Tareas de aplicacion (prioridad 1 = tskIDLE_PRIORITY + 1):
  - Task Sender    → (pendiente) write_uart()
  - Task Receiver  → (pendiente) read_uart()

Tareas gatekeeper del driver (definidas, aun no creadas):
  - Task UART Tx   → (pendiente) HAL_UART_Transmit / IT / DMA
  - Task UART Rx   → (pendiente) HAL_UART_Receive / IT / DMA
```

**Periferico UART**: `USART2` (`huart2`), configurado en CubeMX a **57600 baud**, 8N1, modo TX/RX. En la NUCLEO-L4R5ZI suele corresponder al **Virtual COM Port** (depuracion por serial USB).

**Punto de arranque** (`main.c`, linea 125): `app_init()` se invoca **antes** de `osKernelStart()`. Las tareas se crean pero no ejecutan hasta que el scheduler arranca. Esto es correcto en FreeRTOS.

**Estado actual del codigo base**: respecto de la Actividad 01 en su Paso 03, este proyecto UART esta **mas incompleto**: `open_uart()` no crea colas ni tareas gatekeeper, y `task_uart_tx` / `task_uart_rx` existen como codigo fuente pero **no se ejecutan** porque nadie las crea con `xTaskCreate`.

**Comparacion con baremetal**: en un programa baremetal tipico, `task_sender` llamaria directamente a `HAL_UART_Transmit()` sobre `huart2`. Aqui se anticipa el **desacoplamiento** mediante colas y tareas gatekeeper (`task_uart_tx` / `task_uart_rx`), igual que en el driver I2C del TP anterior.

---

### 2. `app.c` — Inicializacion de la aplicacion

**Proposito**: punto central de arranque de la logica de aplicacion. Declara variables globales, crea las tareas de demo e invoca la apertura del driver UART.

**Variables globales relevantes** (lineas 67-79 de `app.c`):

| Variable | Tipo | Uso |
|----------|------|-----|
| `g_app_tick_cnt` | `volatile uint32_t` | Contador de ticks (actualizado en tick hook) |
| `g_task_idle_cnt` | `uint32_t` | Contador de ejecuciones del idle hook |
| `g_app_stack_overflow_cnt` | `uint32_t` | Contador de desbordes de stack detectados |
| `h_task_sender` | `TaskHandle_t` | Handle de la tarea emisora |
| `h_task_receiver` | `TaskHandle_t` | Handle de la tarea receptora |

**Funcion `app_init()`** (lineas 82-154 de `app.c`):

1. **Inicializa contadores** a cero.
2. **Log de arranque** via `LOGGER_INFO` con el tick actual (`xTaskGetTickCount()`). En este momento el scheduler aun no corre, por lo que el tick sera 0.
3. **Crea `task_sender`** con:
   - Prioridad: `tskIDLE_PRIORITY + 1ul` → **prioridad 1**
   - Stack: `2 * configMINIMAL_STACK_SIZE` palabras
   - Parametro: `NULL`
4. **Crea `task_receiver`** con la misma prioridad y stack.
5. **Consulta heap libre** con `xPortGetFreeHeapSize()` (solo informativo; el valor se descarta en `ret`).
6. **Abre el driver UART** con `open_uart(&huart2)` — referencia al handle generado por CubeMX en `main.c` (`MX_USART2_UART_Init`).
7. **Inicializa interrupciones de aplicacion** con `app_it_init()`.
8. **Inicializa el cycle counter DWT** con `cycle_counter_init()` para medicion de tiempos en microsegundos (WCET en pasos posteriores).

Los comentarios sobre colas y semaforos (lineas 97-109) son **plantilla educativa**: en este codigo base aun no se crean colas ni semaforos de aplicacion en `app.c`; la infraestructura del driver UART debera agregarse en `open_uart()` en pasos posteriores.

**Nota sobre `app.h`**: el header declara `h_task_uart2_tx` (linea 64 de `app.h`), pero `app.c` usa `h_task_sender` y `h_task_receiver`. Es una inconsistencia de la plantilla base que se resolvera cuando se implemente por completo el driver UART.

---

### 3. `app_it.c` — Interrupciones y callbacks HAL de aplicacion

**Proposito**: centralizar la configuracion de interrupciones propias de la aplicacion y los callbacks HAL de EXTI y UART.

**Variables globales** (lineas 57-59 de `app_it.c`):

| Variable | Uso |
|----------|-----|
| `hal_xxxx_callback_flag` | Flag seteado desde ISR de fin de TX UART |
| `hal_xxxx_callback_cnt` | Contador de invocaciones del callback TX |
| `hal_xxxx_callback_runtime_us` | Tiempo medido con DWT al completar TX |

**`app_it_init()`** (lineas 62-74 de `app_it.c`):

- Deshabilita interrupciones con `CPSID i`, inicializa las variables anteriores, y las vuelve a habilitar con `CPSIE i`.
- El bloque interno es minimo: prepara contadores para medicion cuando se use UART por interrupcion.

**`HAL_GPIO_EXTI_Callback()`** (lineas 81-88 de `app_it.c`):

- Callback invocado por HAL cuando ocurre una interrupcion EXTI.
- Detecta si el pin fue `BTN_A_PIN` (boton de usuario de la placa).
- El cuerpo esta vacio (`/* Work to be done. */`): pendiente de implementar logica de evento (por ejemplo, `xSemaphoreGiveFromISR`).

**`HAL_UART_TxCpltCallback()`** (lineas 96-106 de `app_it.c`):

- Callback HAL invocado al **completar una transmision UART** (modo interrupcion o DMA).
- Filtra por `huart->Instance == USART2`.
- Incrementa contadores y setea `hal_xxxx_callback_flag = true`.
- Guarda tiempo de ejecucion con `cycle_counter_get_time_us()`.

Este callback es **clave para la Actividad 02**: anticipa el modo **IT/DMA** del driver UART. En polling puro no se invoca; cuando el gatekeeper use `HAL_UART_Transmit_IT` o `HAL_UART_Transmit_DMA`, la finalizacion de la transferencia llegara por esta ISR.

**Comparacion con baremetal**: en baremetal, el callback TX haria directamente la accion post-envio (liberar buffer, setear flag). En RTOS, la practica recomendada es hacer **lo minimo en la ISR** (contador, flag, `GiveFromISR`) y delegar el trabajo a una tarea gatekeeper o a la tarea que esperaba el fin de TX.

---

### 4. `task_sender.c` — Tarea emisora

**Proposito**: simular un **productor** de datos que eventualmente escribira periodicamente por UART (equivalente al rol de `task_sender` en la demo I2C).

**Flujo del bucle infinito** (lineas 77-85 de `task_sender.c`):

1. Incrementa `g_task_sender_cnt`.
2. Loguea la espera de 250 ms.
3. Se bloquea con `vTaskDelay(TASK_SENDER_DEL_MAX)` donde `TASK_SENDER_DEL_MAX = pdMS_TO_TICKS(250)`.

**Estado actual**: no llama a `write_uart()`. Incluye `task_uart_interface.h` pero aun no usa la API del driver. Es un **stub** que demuestra coexistencia de tareas con la misma prioridad y periodo.

**Relacion prevista con el driver**: en pasos posteriores, `write_uart(&huart2, ...)` encolara bytes en la cola TX del driver; la tarea emisora quedara **desacoplada** del UART fisico.

---

### 5. `task_receiver.c` — Tarea receptora

**Proposito**: placeholder de una tarea que eventualmente **consumira** datos recibidos por UART.

**Flujo del bucle infinito** (lineas 77-85 de `task_receiver.c`):

1. Incrementa `g_task_receiver_cnt`.
2. Loguea la espera de 250 ms.
3. Se bloquea con `vTaskDelay(TASK_RECEIVER_DEL_MAX)` (250 ms).

**Estado actual**: no llama a `read_uart()` ni procesa ningun dato. Es un **stub**, analogo a `task_receiver` del codigo base I2C en el Paso 03 de la Actividad 01.

---

### 6. `task_uart_interface.h` / `task_uart_interface.c` — Interfaz publica del driver UART

**Proposito**: definir e implementar el contrato (API) del device driver UART, analogo a las operaciones `open/read/write/ioctl/release` de un driver en un SO convencional (mismo patron que `task_i2c_interface` de la Actividad 01).

**Funciones declaradas** (lineas 52-58 de `task_uart_interface.h`):

| Funcion | Rol previsto |
|---------|--------------|
| `open_uart(h_uart_device)` | Abrir/inicializar el driver: crear colas, semaforos y tareas gatekeeper |
| `release_uart(h_uart_device)` | Cerrar el driver: eliminar colas y tareas |
| `write_uart(h_uart_device)` | Encolar una escritura UART |
| `read_uart(h_uart_device)` | Solicitar una lectura UART |
| `ioctl_uart(h_uart_device)` | Control del dispositivo (modo, baudrate, etc.) |

Las tareas de aplicacion (`task_sender`, `task_receiver`) incluyen este header; **no deben** conocer la implementacion interna (`task_uart.c`, colas, handles).

**Implementacion actual** (`task_uart_interface.c`, lineas 66-94):

Todas las funciones son **stubs vacios** que solo ejecutan `UNUSED(h_uart_device)` para evitar warnings de compilacion:

```c
void open_uart(UART_HandleTypeDef *h_uart_device)
{
    UNUSED(h_uart_device);
}
```

**Diferencia importante respecto del codigo base I2C (Actividad 01, Paso 03)**: en I2C, `open_i2c()` ya creaba colas, tareas gatekeeper y registraba recursos en el kernel. En UART, **`open_uart()` aun no hace nada**; la estructura interna (`task_uart_dta_t`) tampoco esta definida en `task_uart_attribute.h` (archivo vacio, lineas 48-52).

---

### 7. `task_uart.c` — Tareas internas del driver UART (gatekeepers)

**Proposito**: contener la logica de bajo nivel del driver. Son tareas **privadas** del driver; deberian crearse desde `open_uart()`, no desde `app.c`.

**Variables de medicion** (lineas 70-74 de `task_uart.c`):

| Variable | Uso |
|----------|-----|
| `g_task_xxxx_tx_cnt` | Contador de iteraciones de `task_uart_tx` |
| `g_task_xxxx_tx_runtime_us` | WCET de una iteracion TX (DWT) |
| `g_task_xxxx_rx_cnt` | Contador de iteraciones de `task_uart_rx` |
| `g_task_xxxx_rx_runtime_us` | WCET de una iteracion RX (DWT) |

#### `task_uart_tx()` (lineas 78-107 de `task_uart.c`)

Bucle infinito:

1. Incrementa contador.
2. Mide tiempo con DWT (`cycle_counter_reset()` / `cycle_counter_get_time_us()`).
3. Hace `HAL_GPIO_TogglePin(LED_A_PORT, LED_A_PIN)` — **no realiza transmision UART**.
4. Loguea y se bloquea 250 ms con `vTaskDelay`.

El parametro `parameters` se marca `UNUSED`: la tarea aun no recibe el puntero al contexto del driver (`task_uart_dta_t`, por definir).

#### `task_uart_rx()` (lineas 110-139 de `task_uart.c`)

Bucle identico al TX: toggle LED, medicion DWT, log, delay 250 ms. **No consume cola RX ni llama a HAL Receive**.

**Estado actual**: ambas gatekeepers son **placeholders** (como `task_i2c_rx` con toggle LED en la Actividad 01). Ademas, al no ser creadas por `open_uart()`, **no llegan a ejecutarse** en el firmware actual.

**Comportamiento previsto** (pasos posteriores del TP):

```
task_uart_tx:
  xQueueReceive(queue_tx, ...) → HAL_UART_Transmit / _IT / _DMA → sync/async

task_uart_rx:
  xQueueReceive(queue_rx, ...) → HAL_UART_Receive / _IT / _DMA → devolver datos
```

---

### 8. `freertos.c` (APP/src) — Hooks de FreeRTOS

**Proposito**: implementar los callback hooks que FreeRTOS invoca en momentos clave del kernel. El archivo en `APP/src/freertos.c` **sobreescribe** las versiones `__weak` vacias de `Core/Src/freertos.c`.

Configuracion verificada en `FreeRTOSConfig.h`:

- `configUSE_IDLE_HOOK = 1`
- `configUSE_TICK_HOOK = 1`
- `configCHECK_FOR_STACK_OVERFLOW = 1`

#### `vApplicationIdleHook()` (lineas 60-76 de `freertos.c`)

- Se ejecuta cada vez que la tarea Idle corre (cuando no hay tareas listas de mayor o igual prioridad).
- Incrementa `g_task_idle_cnt`.
- **Restriccion critica**: no puede llamar APIs que bloqueen (`vTaskDelay`, `xQueueReceive`, etc.).

#### `vApplicationTickHook()` (lineas 78-89 de `freertos.c`)

- Se ejecuta **desde la ISR del SysTick** en cada tick del RTOS.
- Incrementa `g_app_tick_cnt`.
- **Restriccion critica**: solo APIs seguras para ISR (sufijo `FromISR`). Debe ser muy breve.

#### `vApplicationStackOverflowHook()` (lineas 91-104 de `freertos.c`)

- Invocado si el kernel detecta desbordamiento de stack de alguna tarea.
- Entra en seccion critica, ejecuta `configASSERT(0)` para detener la ejecucion (depuracion).
- Incrementa `g_app_stack_overflow_cnt`.

**Comparacion con baremetal**: en baremetal no existen hooks de idle/tick del kernel; un contador de ticks se implementaria manualmente en la ISR de SysTick y el "idle" seria el bucle principal vacio o `WFI`.

---

### 9. Flujo de ejecucion y timeline (codigo base actual)

En el estado actual del proyecto, solo ejecutan **`task_sender`** y **`task_receiver`**, ambas a **prioridad 1** (`tskIDLE_PRIORITY + 1`). Con prioridades iguales, FreeRTOS aplica **time-slicing** (round-robin por quantum de tick).

**Secuencia tipica de una iteracion:**

```
1. task_sender (P=1, Ready)
   ├── LOGGER_INFO ("Wait 250mS")
   └── vTaskDelay(250ms) → Blocked

2. task_receiver (P=1, Ready)
   ├── LOGGER_INFO ("Wait 250mS")
   └── vTaskDelay(250ms) → Blocked

3. Ninguna tarea de P=1 lista → Idle Task (P=0)
   └── vApplicationIdleHook() → g_task_idle_cnt++

4. Cada 1 ms (configTICK_RATE_HZ): SysTick ISR
   └── vApplicationTickHook() → g_app_tick_cnt++
```

**Nota**: `task_uart_tx` y `task_uart_rx` **no participan** en este timeline porque `open_uart()` no las crea. Cuando se implemente el driver, el flujo incluira encolado desde sender/receiver, procesamiento serializado en gatekeepers, y posiblemente callbacks en `HAL_UART_TxCpltCallback` para modos IT/DMA.

**Nota sobre concurrencia UART**: al centralizar TX/RX en gatekeepers (diseno objetivo), solo **una tarea** accedera al UART en un instante dado, evitando corrupcion de buffers compartidos — ventaja clave del patron device driver frente a llamadas directas a HAL desde multiples tareas.

---

### 10. Elementos pendientes (codigo base incompleto)

| Componente | Estado | Observacion |
|------------|--------|-------------|
| `task_uart_attribute.h` | Vacio | Falta `task_uart_dta_t`, colas, modos, patrones |
| `open_uart()` | Stub | No crea colas, semaforos ni tareas gatekeeper |
| `write_uart()` / `read_uart()` | Stub | Sin encolado ni SYNC/ASYNC |
| `ioctl_uart()` / `release_uart()` | Stub | Sin implementacion |
| `task_uart_tx()` / `task_uart_rx()` | Placeholder | Toggle LED; no usan HAL UART ni colas |
| Creacion de `task_uart_tx/rx` | Ausente | Funciones definidas pero no invocadas con `xTaskCreate` |
| `task_sender` / `task_receiver` | Stub | No llaman a la API UART |
| `HAL_GPIO_EXTI_Callback` | Stub | Sin logica para BTN_A |
| `HAL_UART_TxCpltCallback` | Parcial | Cuenta eventos; falta integrar con driver (GiveFromISR, etc.) |
| Colas/semaforos en `app.c` | Comentario plantilla | No implementados aun |
| `app.h` vs `app.c` | Inconsistente | `h_task_uart2_tx` declarado pero no usado en `app.c` |

Estos stubs son intencionales: el TP guia al alumno a completarlos en pasos posteriores, reutilizando el patron aprendido en la Actividad 01 (I2C).

---

### 11. Resumen

El codigo base implementa el **esqueleto de un device driver UART sobre FreeRTOS** con separacion clara de responsabilidades (misma filosofia que el driver I2C):

- **`app.c`**: orquesta la creacion de tareas de aplicacion e invoca `open_uart(&huart2)`.
- **`task_uart_interface.h/.c`**: expone la API tipo Unix (`open/write/read/ioctl/release`); hoy todas las funciones son stubs.
- **`task_uart.c`**: define las gatekeepers TX/RX como placeholders (toggle LED + delay); aun no conectadas al scheduler.
- **`task_sender.c` / `task_receiver.c`**: tareas de demo que coexisten a prioridad 1; pendientes de usar `write_uart` / `read_uart`.
- **`app_it.c`**: punto de extension para EXTI y **callback de fin de TX UART** (`HAL_UART_TxCpltCallback`), preparado para modos IT/DMA.
- **`freertos.c`**: hooks de monitoreo (idle, tick, stack overflow).

La arquitectura anticipa el objetivo del TP: que el acceso a **USART2** quede encapsulado, sincronizado por colas y semaforos, y sea transparente para las tareas de aplicacion — patron habitual en sistemas embebidos con RTOS en entornos industriales.

**Paralelo con Actividad 01**: I2C tenia `open_i2c` funcional (colas + tareas); UART replicara ese esquema adaptado a transmision serial, con la particularidad de que UART admite **polling, interrupcion y DMA**, y el callback `HAL_UART_TxCpltCallback` ya esta esbozado en `app_it.c` para soportar los modos no bloqueantes.

---


## Paso 06: Device Driver UART de FreeRTOS (Interrupt + Async)

Diseno, implementacion y uso del device driver UART sobre FreeRTOS en la NUCLEO-L4R5ZI, con `USART2` (`huart2`) en **PD5/PD6** (57600 baud, 8N1). Comunicacion con terminal serial **Docklight**. 

- Aplicacion: `task_sender` rota comandos `"CMD1\r\n"`, `"CMD2\r\n"`, `"CMD3\r\n"` via `write_uart()`; `task_receiver` solicita lectura con `read_uart()` y obtiene la respuesta con `uart_get_rx_data()` (`"OK\r\n"` / `"ERROR\r\n"` simuladas desde Docklight).
- Patron asincrono: `write_uart()` y `read_uart()` encolan en el driver y retornan de inmediato; la tarea de aplicacion consume el resultado RX desde `queue_rx_out` con timeout configurable.
- Tipo interrupt: Las tareas gatekeeper usan `HAL_UART_Transmit_IT()` y `HAL_UART_Receive_IT()` (1 byte por IT); los callbacks en `app_it.c` liberan `sem_tx_it_done` / `sem_rx_it_done` con `xSemaphoreGiveFromISR()`.
- Almacenamiento en queue con spooler dinamico: buffers TX/RX con `pvPortMalloc()` en la tarea de aplicacion o gatekeeper RX; liberados con `vPortFree()` al finalizar la operacion.
- Tareas Gatekeeper: Creadas en `open_uart()` con prioridad `tskIDLE_PRIORITY + 1` y parametro `&task_uart_dta` (`task_uart_tx`, `task_uart_rx`).
- Tipo de recepcion UART: Lectura byte a byte hasta `UART_RX_END_CHAR` (`0x0A`); timeout `UART_RX_TIMEOUT_MS` con `HAL_UART_AbortReceive_IT()`; entrega valida si `len > 1` e `is_valid_answer == true`.
- Estructura del dispositivo: Definida en `task_uart_attribute.h` como `task_uart_dta_t` (`device_id`, `queue_tx`, `queue_rx`, `queue_rx_out`, `sem_*_it_done`, `active_tx`, `active_rx`, `is_valid_answer`, `mode_use`, `pattern_use`).
- Cada mensaje en las colas es un `task_uart_spooler_tx_rx_dta_t` (`buffer`, `len`).
- Funciones de interfaz del driver: API publica en `task_uart_interface.h` / `task_uart_interface.c` (`open_uart`, `release_uart`, `write_uart`, `read_uart`, `uart_get_rx_data`, `ioctl_uart`).
- Desarrollo de `ioctl_uart()`: No fue necesario el uso para este TP.

### WCET — medicion y registro

Medicion con DWT, STM32CubeIDE Live Expressions (NUCLEO-L4R5ZI, Docklight - terminal serial -):

| Medicion | Variable | WCET [us] |
|----------|----------|-----------|
| Gatekeeper TX | `g_task_xxxx_tx_runtime_us` | 1069 |
| Gatekeeper RX | `g_task_xxxx_rx_runtime_us` | 100122 |
| `read_uart()` + `uart_get_rx_data()` Async | `g_read_uart_wcet_us` | 99980 |

Gatekeeper TX/RX: cronometro en `task_uart.c` despues de `xQueueReceive()` (no incluye espera en cola vacia).
Gatekeeper RX WCET ~100 ms: dominado por `UART_RX_TIMEOUT_MS` (100 ms) al agotarse el timeout byte-a-byte sin recibir `\n`.
`read_uart()` + `uart_get_rx_data()` Async: cronometro en `task_receiver.c` desde `read_uart()` hasta retorno de `uart_get_rx_data()`; incluye encolado, gatekeeper RX y espera en `queue_rx_out` (valor similar al gatekeeper RX en el peor caso de timeout).


### Video del tp funcionando
[Video / archivo en Drive](https://drive.google.com/file/d/1fqSU3TrkTfsJqmvgtzP6yJ8GMEykkGbu/view?usp=drive_link)



### Muestra de logs IDE

```text
[info]  
[info] app_init is running - Tick [mS] = 0
[info]  app is a RTOS - Event-Triggered Systems (ETS)
[info]  app is a sotrii-tp1_02-application: Demo Code
[info]  app is a (Source => CESE - Sistemas Operativos de Tiempo Real)
[info] 

[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task SENDER - TX: CMD1


[info]    ==> Task SENDER - Wait:   250mS
[info]    ==> Task RECEIVER - RX: ERROR


[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task SENDER - TX: CMD2


[info]    ==> Task SENDER - Wait:   250mS
[info]    ==> Task RECEIVER - RX: OK


[info]    ==> Task RECEIVER - Wait:   250mS
[info]    ==> Task SENDER - TX: CMD3


[info]    ==> Task SENDER - Wait:   250mS
[info]    ==> Task RECEIVER - RX: OK
```

### Muestra de logs Docklight (terminal serial)

```text
12/7/2026 14:08:35.770 [RX] - CMD1

12/7/2026 14:08:35.778 [TX] - OK

12/7/2026 14:08:37.288 [RX] - CMD2

12/7/2026 14:08:37.292 [TX] - OK

12/7/2026 14:08:38.806 [RX] - CMD3

12/7/2026 14:08:38.820 [TX] - ERROR
```