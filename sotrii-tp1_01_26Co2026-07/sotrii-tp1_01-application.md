# CESE - Sistemas Operativos de Tiempo Real

## Trabajo Práctico N°: 1 – Device Driver de FreeRTOS

### TP1 – Actividad 01 – Device Driver I2C de FreeRTOS

## Paso 01

Proyecto generado e importado. Compila en STM32CubeIDE.

---

## Paso 02

Archivo de entrega: `sotrii-tp1_01-application.md` (este documento).

---

## Paso 03: Analisis del codigo fuente base

Analizar y explicar (en español), el funcionamiento del codigo fuente contenido en los archivos adjuntos: `app.c`, `app_it.c`, `task_sender.c`, `task_receiver.c`, `task_i2c.c`, `task_i2c_interface.c`, `task_i2c_interface.h` y `freertos.c`.

---

### 1. Vision general de la arquitectura

El proyecto implementa una **demo de sistema disparado por eventos (ETS - Event-Triggered Systems)** sobre FreeRTOS en la NUCLEO-L4R5ZI. El objetivo del TP es construir un **device driver I2C** encapsulado como tareas del RTOS, de modo que el resto de la aplicacion no acceda directamente al periferico HAL.

La arquitectura se organiza en capas:

```
main.c
  └── app_init()                    (app.c)
        ├── xTaskCreate task_sender
        ├── xTaskCreate task_receiver
        ├── open_i2c(&hi2c1)         (task_i2c_interface.c)
        │     ├── xQueueCreate (TX y RX)
        │     ├── xTaskCreate task_i2c_tx
        │     └── xTaskCreate task_i2c_rx
        └── app_it_init()            (app_it.c)

Tareas de aplicacion (prioridad 1 = tskIDLE_PRIORITY + 1):
  - Task Sender    → write_i2c() → cola TX → Task I2C Tx → HAL_I2C_Master_Transmit
  - Task Receiver  → solo espera (stub)
  - Task I2C Tx    → consume cola y transmite por I2C1
  - Task I2C Rx    → toggle LED (stub, sin lectura I2C real aun)
```

**Punto de arranque** (`main.c`, linea 132): `app_init()` se invoca **antes** de `osKernelStart()`. Las tareas se crean pero no ejecutan hasta que el scheduler arranca. Esto es correcto en FreeRTOS: la creacion ocurre en contexto de inicializacion; la ejecucion, en contexto de tarea.

**Comparacion con baremetal**: en un programa baremetal tipico, `task_sender` llamaria directamente a `HAL_I2C_Master_Transmit()` dentro de su bucle. Aqui esa llamada esta **desacoplada** mediante una cola y una tarea dedicada (`task_i2c_tx`), lo que centraliza el acceso al bus I2C y evita que multiples tareas compitan por el mismo periferico sin coordinacion.

---

### 2. `app.c` — Inicializacion de la aplicacion

**Proposito**: punto central de arranque de la logica de aplicacion. Declara variables globales, crea las tareas de demo y abre el driver I2C.

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
6. **Abre el driver I2C** con `open_i2c(&hi2c1)` — referencia al handle generado por CubeMX en `main.c`.
7. **Inicializa interrupciones de aplicacion** con `app_it_init()`.
8. **Inicializa el cycle counter DWT** con `cycle_counter_init()` para medicion de tiempos en microsegundos.

Los comentarios sobre colas y semaforos (lineas 97-109) son **plantilla educativa**: en este codigo base aun no se crean colas ni semaforos en `app.c`; la cola del driver I2C se crea dentro de `open_i2c()`.

---

### 3. `app_it.c` — Interrupciones de aplicacion

**Proposito**: centralizar la configuracion de interrupciones propias de la aplicacion y los callbacks HAL de EXTI.

**`app_it_init()`** (lineas 57-65 de `app_it.c`):

- Deshabilita interrupciones con `CPSID i`, y las vuelve a habilitar con `CPSIE i`.
- El bloque interno esta vacio (`/* Init to be done */`): es un **esqueleto** para futura configuracion protegida de recursos compartidos.

**`HAL_GPIO_EXTI_Callback()`** (lineas 72-79 de `app_it.c`):

- Callback invocado por HAL cuando ocurre una interrupcion EXTI.
- Detecta si el pin fue `BTN_A_PIN` (boton de usuario de la placa).
- El cuerpo esta vacio (`/* Work to be done. */`): pendiente de implementar la logica de evento (por ejemplo, notificar a una tarea con `xSemaphoreGiveFromISR` o `xQueueSendFromISR`).

**Comparacion con baremetal**: en baremetal, el callback EXTI ejecutaria directamente la accion (toggle LED, setear flag). En RTOS, la practica recomendada es **hacer lo minimo en la ISR** (limpiar flag, senalar evento) y delegar el trabajo pesado a una tarea mediante un mecanismo de sincronizacion.

---

### 4. `task_sender.c` — Tarea emisora

**Proposito**: simular un productor de datos que escribe periodicamente en un dispositivo I2C (modulo LCD serial con PCF8574, direccion `0x27`).

**Flujo del bucle infinito** (lineas 85-97 de `task_sender.c`):

1. Incrementa `g_task_sender_cnt`.
2. Invierte `dev_data` con `~dev_data` (alterna entre `0x55` y `0xAA`).
3. Llama a `write_i2c(&hi2c1, dev_address, dev_data)` — **no accede al HAL directamente**.
4. Loguea la espera de 250 ms.
5. Se bloquea con `vTaskDelay(TASK_SENDER_DEL_MAX)` donde `TASK_SENDER_DEL_MAX = pdMS_TO_TICKS(250)`.

**Relacion con el driver**: `write_i2c()` encola la direccion y el dato en la cola TX del driver. La tarea emisora queda **desacoplada** del bus I2C fisico; solo conoce la interfaz publica del driver.

---

### 5. `task_receiver.c` — Tarea receptora

**Proposito**: placeholder de una tarea que eventualmente consumira datos (por ejemplo, lecturas I2C o eventos recibidos).

**Flujo del bucle infinito** (lineas 77-85 de `task_receiver.c`):

1. Incrementa `g_task_receiver_cnt`.
2. Loguea la espera de 250 ms.
3. Se bloquea con `vTaskDelay(TASK_RECEIVER_DEL_MAX)` (250 ms).

**Estado actual**: no llama a `read_i2c()` ni procesa ningun evento. Es un **stub** que demuestra la coexistencia de multiples tareas con la misma prioridad y periodo.

---

### 6. `task_i2c_interface.h` — Interfaz publica del driver I2C

**Proposito**: definir el contrato (API) del device driver, analogo a las operaciones `open/read/write/ioctl/release` de un driver en un SO convencional.

**Funciones declaradas** (lineas 52-58 de `task_i2c_interface.h`):

| Funcion | Rol |
|---------|-----|
| `open_i2c(h_i2c_device)` | Abrir/inicializar el driver: crear colas y tareas internas |
| `release_i2c(h_i2c_device)` | Cerrar el driver: eliminar colas y tareas |
| `write_i2c(h_i2c_device, address, data)` | Encolar una escritura I2C |
| `read_i2c(h_i2c_device)` | Lectura I2C (stub) |
| `ioctl_i2c(h_i2c_device)` | Control del dispositivo (stub) |

Las tareas de aplicacion (`task_sender`, etc.) solo incluyen este header; **no conocen** la implementacion interna (`task_i2c.c`, colas, handles).

---

### 7. `task_i2c_interface.c` — Implementacion del driver I2C

**Proposito**: implementar la API del driver. Gestiona la estructura de datos interna, las colas FreeRTOS y la creacion/destruccion de las tareas I2C.

**Estructura interna** (`task_i2c_dta_t`, definida en `task_i2c_attribute.h`):

```c
typedef struct {
    I2C_HandleTypeDef * device_id;
    TaskHandle_t       task_tx;
    QueueHandle_t      queue_tx;
    TaskHandle_t       task_rx;
    QueueHandle_t      queue_rx;
} task_i2c_dta_t;
```

Instancia unica estatica: `task_i2c_dta` (linea 57 de `task_i2c_interface.c`).

#### `open_i2c()` (lineas 67-96)

1. Asigna el puntero al handle HAL (`hi2c1`).
2. Crea `queue_tx`: capacidad **5** elementos de tipo `task_i2c_tx_dta_t` (direccion + dato).
3. Crea `queue_rx`: capacidad **10** elementos de tipo `uint8_t`.
4. Registra ambas colas con `vQueueAddToRegistry()` para depuracion.
5. Crea `task_i2c_tx` (prioridad 1, stack minimo) pasando `&task_i2c_dta` como parametro.
6. Crea `task_i2c_rx` (prioridad 1, stack minimo).
7. Verifica cada operacion con `configASSERT`.

#### `write_i2c()` (lineas 117-133)

1. Arma la estructura `task_i2c_tx_dta_t` con `address` y `data`.
2. Encola con `xQueueSend(..., portMAX_DELAY)` — la tarea llamadora **se bloquea** si la cola esta llena (maximo 5 pendientes).
3. Retorna cuando la cola acepta el elemento; la transmision fisica la ejecuta `task_i2c_tx` de forma asincrona.

#### `release_i2c()` (lineas 98-115)

- Desregistra y elimina colas, elimina tareas TX y RX. Implementado pero no invocado en el flujo actual.

#### `read_i2c()` y `ioctl_i2c()` (lineas 135-145)

- **Stubs vacios** (`UNUSED`). Pendientes de implementacion en pasos posteriores del TP.

---

### 8. `task_i2c.c` — Tareas internas del driver I2C

**Proposito**: contener la logica de bajo nivel del driver. Son tareas **privadas** del driver; no se crean desde `app.c` sino desde `open_i2c()`.

#### `task_i2c_tx()` (lineas 78-116)

Bucle infinito:

1. Incrementa contador `g_task_xxxx_tx_cnt`.
2. **Espera bloqueante** en `xQueueReceive(p_task_i2c_tx_dta->queue_tx, &task_i2c_tx_dta, portMAX_DELAY)` hasta recibir un pedido de escritura.
3. Mide tiempo con DWT: `cycle_counter_reset()` antes y `cycle_counter_get_time_us()` despues.
4. Ejecuta la transmision HAL:
   ```c
   HAL_I2C_Master_Transmit(device_id, (address << 1), &data, sizeof(data), HAL_MAX_DELAY);
   ```
   El desplazamiento `(address << 1)` convierte la direccion de 7 bits al formato de direccion I2C de 8 bits que espera HAL (bit R/W en LSB).
5. Guarda el tiempo de ejecucion en `g_task_xxxx_tx_runtime_us`.
6. Loguea y se bloquea 250 ms con `vTaskDelay`.

#### `task_i2c_rx()` (lineas 119-148)

Bucle infinito:

1. Incrementa contador `g_task_xxxx_rx_cnt`.
2. Mide tiempo con DWT.
3. Hace `HAL_GPIO_TogglePin(LED_A_PORT, LED_A_PIN)` — **no realiza lectura I2C**.
4. Loguea y se bloquea 250 ms.

**Estado actual**: `task_i2c_rx` es un **placeholder** que demuestra la existencia de la tarea RX pero aun no consume `queue_rx` ni llama a `HAL_I2C_Master_Receive`.

---

### 9. `freertos.c` (APP/src) — Hooks de FreeRTOS

**Proposito**: implementar los callback hooks que FreeRTOS invoca en momentos clave del kernel. El archivo en `APP/src/freertos.c` **sobreescribe** las versiones `__weak` vacias de `Core/Src/freertos.c`.

Configuracion verificada en `FreeRTOSConfig.h`:
- `configUSE_IDLE_HOOK = 1`
- `configUSE_TICK_HOOK = 1`
- `configCHECK_FOR_STACK_OVERFLOW = 1`

#### `vApplicationIdleHook()` (lineas 60-76)

- Se ejecuta cada vez que la tarea Idle corre (cuando no hay tareas listas de mayor o igual prioridad).
- Incrementa `g_task_idle_cnt`.
- **Restriccion critica**: no puede llamar APIs que bloqueen (`vTaskDelay`, `xQueueReceive`, etc.).

#### `vApplicationTickHook()` (lineas 78-89)

- Se ejecuta **desde la ISR del SysTick** en cada tick del RTOS.
- Incrementa `g_app_tick_cnt`.
- **Restriccion critica**: solo APIs seguras para ISR (sufijo `FromISR`). Debe ser muy breve.

#### `vApplicationStackOverflowHook()` (lineas 91-104)

- Invocado si el kernel detecta desbordamiento de stack de alguna tarea.
- Entra en seccion critica, ejecuta `configASSERT(0)` para detener la ejecucion (depuracion).
- Incrementa `g_app_stack_overflow_cnt`.

**Comparacion con baremetal**: en baremetal no existen hooks de idle/tick del kernel; un contador de ticks se implementaria manualmente en la ISR de SysTick y el "idle" seria simplemente el bucle principal vacio o `WFI`.

---

### 10. Flujo de ejecucion y timeline

Todas las tareas de aplicacion y del driver operan a **prioridad 1** (`tskIDLE_PRIORITY + 1`). Con prioridades iguales, FreeRTOS aplica **time-slicing** (round-robin por quantum de tick).

**Secuencia tipica de una iteracion de `task_sender`:**

```
1. task_sender (P=1, Ready)
   ├── write_i2c() → xQueueSend en queue_tx
   │     (si queue_tx estaba vacia, task_i2c_tx pasa a Ready)
   ├── LOGGER_INFO
   └── vTaskDelay(250ms) → task_sender pasa a Blocked

2. Scheduler elige entre tareas Ready de P=1 (round-robin):
   ├── task_i2c_tx (P=1, Ready) — recibio dato de la cola
   │     ├── HAL_I2C_Master_Transmit (bloqueante HAL)
   │     ├── LOGGER_INFO
   │     └── vTaskDelay(250ms) → Blocked
   ├── task_receiver (P=1, Ready)
   │     ├── LOGGER_INFO
   │     └── vTaskDelay(250ms) → Blocked
   └── task_i2c_rx (P=1, Ready)
         ├── HAL_GPIO_TogglePin
         ├── LOGGER_INFO
         └── vTaskDelay(250ms) → Blocked

3. Ninguna tarea de P=1 lista → Idle Task (P=0)
   └── vApplicationIdleHook() → g_task_idle_cnt++

4. Cada 1 ms (configTICK_RATE_HZ): SysTick ISR
   └── vApplicationTickHook() → g_app_tick_cnt++
```

**Nota sobre concurrencia I2C**: al centralizar la transmision en `task_i2c_tx`, solo **una tarea** accede al bus en un instante dado. Si `task_sender` y otra tarea llamaran a `write_i2c()` concurrentemente, los pedidos se encolarian y se procesarian secuencialmente — ventaja clave del patron device driver sobre acceso directo al HAL.

---

### 11. Elementos pendientes (codigo base incompleto)

| Componente | Estado | Observacion |
|------------|--------|-------------|
| Colas/semaforos en `app.c` | Comentario plantilla | No implementados aun |
| `HAL_GPIO_EXTI_Callback` | Stub | Sin logica para BTN_A |
| `read_i2c()` | Stub | Sin lectura I2C |
| `ioctl_i2c()` | Stub | Sin control de dispositivo |
| `task_i2c_rx()` | Parcial | Toggle LED, no usa `queue_rx` ni HAL Receive |
| `app_it_init()` | Esqueleto | Sin configuracion real |

Estos stubs son intencionales: el TP guia al alumno a completarlos en pasos posteriores.

---

### 12. Resumen

El codigo base implementa el **esqueleto de un device driver I2C sobre FreeRTOS** con separacion clara de responsabilidades:

- **`app.c`**: orquesta la creacion de tareas y abre el driver.
- **`task_i2c_interface.h/.c`**: expone la API tipo Unix (`open/write/read/ioctl/release`) y gestiona colas y tareas internas.
- **`task_i2c.c`**: ejecuta la transmision HAL de forma serializada desde una unica tarea.
- **`task_sender.c`**: productor de datos que usa el driver sin conocer el HAL.
- **`task_receiver.c`**: placeholder para futura logica de recepcion.
- **`app_it.c`**: punto de extension para interrupciones EXTI.
- **`freertos.c`**: hooks de monitoreo (idle, tick, stack overflow).

La arquitectura anticipa el objetivo del TP: que el acceso al periferico I2C quede encapsulado, sincronizado por colas, y sea transparente para las tareas de aplicacion — patron habitual en sistemas embebidos con RTOS en entornos industriales.

## Paso 06: Device Driver I2C de FreeRTOS (Polling + Sync)

Diseno, implementacion y uso del device driver I2C sobre FreeRTOS en la NUCLEO-L4R5ZI, con acelerometro ADXL345 conectado por I2C1. Se depreco el uso de Serial LCD I2C Module–PCF8574.

- Aplicacion: uso del driver con ADXL345; Inicio de dispositivo con seteo en registro POWER y despues lectura de registros de aceleracion tipo mapa.
- Patrón sincrono: El patron garantiza que la tarea que llama `write_i2c()` o `read_i2c()` espera hasta que el gatekeeper termino la operacion HAL antes de continuar.
- Tipo polling: Las tareas gatekeeper ejecutan las funciones HAL bloqueantes (`HAL_MAX_DELAY`):
- Almacenamiento en queue.
- Tareas Gatekeeper: Creadas en `open_i2c()` con prioridad `tskIDLE_PRIORITY + 1` y parametro `&task_i2c_dta`.
- Tipo de recepcion I2C: Lectura de mapeo.
- Estructura del dispositivo: Definida en `task_i2c_attribute.h` como `task_i2c_dta_t`:
- Cada mensaje en las colas es un `task_i2c_tx_rx_dta_t`
- Funciones de interfaz del driver: API publica en `task_i2c_interface.h` / `task_i2c_interface.c`:
- Desarrollo de `ioctl_i2c()`: No fue necesario el uso para este TP.


### WCET — medicion y registro

Medicion con DWT, STM32CubeIDE Live Expressions (NUCLEO-L4R5ZI, ADXL345):

| Medicion | Variable | WCET [us] |
|----------|----------|-----------|
| Gatekeeper TX | `g_task_xxxx_tx_runtime_us` | 301 |
| Gatekeeper RX | `g_task_xxxx_rx_runtime_us` | 851 |
| `read_i2c()` Sync | `g_read_i2c_wcet_us` | 978 |

Gatekeeper RX: cronometro despues de `xQueueReceive()` (no incluye espera en cola).
`read_i2c()` Sync: incluye encolado + semaforo + gatekeeper + `memcpy` (por eso > 851 us).
