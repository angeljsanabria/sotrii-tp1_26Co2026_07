# CESE - Sistemas Operativos de Tiempo Real

## Trabajo Práctico N°: 1 – Device Driver de FreeRTOS

### TP1 – Actividad 03 – Device Driver ADC de FreeRTOS

## Paso 01

Proyecto generado e importado. Compila en STM32CubeIDE.

---

## Paso 02

Archivo de entrega: `sotrii-tp1_03-application.md` (este documento).

---

## Paso 03: Analisis del codigo fuente base


## Paso 06
Paso 06: Diseñar , Implementar y Usar Device Driver ADC de FreeRTOS . con las siguientes características: 
- Estructura de datos que representen al dispositivo, incluyendo: device_id , device_queue , etc. 
- Funciones de Interfaz: open () , release () , write () , read () , ioctl () , etc. 
Patrón de diseño: Latest Input Only (NO Synchronous ni tampoco Asynchronous )
- Gestión del periférico:  DMA  (NO Polling , ni tampoco InterruptAcceso) 
- Acceso al periférico: API HAL 
- Una función de dispositivo para instanciar tareas del tipo: Gatekeeper (para recibir), que reciben como parámetro referencia al canal ADC del MCU sobre el que el Device Driver opera. 
- - Almacenamiento de datos: Queue , Queue with Input and Output Data Spooler allocation: Static , (NO Dynamic)
- Compilar / depurar/observar comportamiento/ Medir WCET de las Funciones de Interfaz del driver , asentar lo observado en el archivo sotrii-tp1_03-application.md .