/**
 * @file    Logger.h
 * @brief   Serial logging over the USB CDC virtual COM port.
 *
 * @details Provides Log_Print()/Log_Printf() for any driver or module to
 *          emit tagged text messages to a terminal connected to the board's
 *          USB CDC port (see USB_DEVICE/App/usbd_cdc_if.c, CDC_Transmit_FS).
 *
 *          Arquitectura (7-sep-2026, migrado de mutex a cola -- mismo patron
 *          validado en el proyecto hermano Sensores):
 *          Log_Print()/Log_Printf() arman la linea y la ENCOLAN
 *          (osMessageQueuePut, timeout 0 -- nunca bloquean al llamante) y
 *          regresan de inmediato. La unica que transmite de verdad por USB
 *          (bloqueante, esperando CDC_Transmit_FS + hcdc->TxState) es
 *          LoggerTask (cuerpo publico Log_Task()), corriendo en su propio
 *          hilo RTOS. Si la cola esta llena (rafaga de logs mas rapida que
 *          el consumidor), el mensaje se descarta y se cuenta -- ninguna
 *          tarea se queda esperando por un log, ni siquiera las de tiempo
 *          critico (disparo, etc.).
 *
 *          Mientras la cola no exista todavia (antes de Log_InitQueue(),
 *          en el arranque bare-metal pre-RTOS), Log_Print() transmite
 *          directo y bloqueante -- en ese punto solo hay un hilo de
 *          ejecucion corriendo, no hace falta la cola.
 *
 *          A diferencia del mutex viejo, encolar con timeout 0 SI es
 *          seguro entre Log_InitQueue() y osKernelStart() -- nunca
 *          bloquea. Ya NO aplica la regla de "nunca loguear en esa
 *          ventana" (esa regla sigue aplicando para I2C1Bus_Lock(), que
 *          si sigue siendo mutex).
 *
 *          Usage:
 *            main.c  → call Log_Init() once after MX_USB_DEVICE_Init()
 *                      (pre-RTOS, inside Inicializacion_Run()), then
 *                      Log_InitQueue() once after osKernelInitialize(), y
 *                      crear LoggerTask (osThreadNew(Log_Task, ...)) en
 *                      Tareas_CrearTareas() antes de osKernelStart().
 *                      Only reached when Bootloader_CheckAndEnter() did NOT
 *                      jump to the DFU bootloader (see Bootloader.h).
 *            others  → #include "Logger.h" and call Log_Print(TAG, msg).
 *
 * @date    September 7, 2026
 * @author  César Pérez
 * @version 4.0.0
 */

#ifndef LOGGER_H
#define LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"

/* ========================  CONFIGURATION  ================================= */

#define LOG_TX_TIMEOUT_MS   100U   /**< Max wait while CDC_Transmit_FS is busy */
#define LOG_MAX_MSG_LEN      160U  /**< "[TAG] msg\r\n" buffer size, truncates if longer */
#define LOG_QUEUE_LEN         16U  /**< Entradas de la cola -- rafaga que absorbe sin descartar */

/* ========================  API  =========================================== */

/**
 * @brief  Initializes the logger.
 * @note   Call once after MX_USB_DEVICE_Init() in main.c — and only on the
 *         path where the app did NOT jump to the bootloader.
 */
void Log_Init(void);

/**
 * @brief  Crea la cola interna del Logger.
 * @note   Llamar despues de osKernelInitialize() (no antes -- el kernel
 *         debe estar listo para crear objetos RTOS). Ver Tareas_Interrupciones.c.
 *         Mientras esta cola no exista, Log_Print()/Log_Printf() transmiten
 *         directo y bloqueante (fallback pre-RTOS).
 */
void Log_InitQueue(void);

/**
 * @brief  Cuerpo de LoggerTask -- consume la cola y transmite por USB CDC.
 * @param  argument  Sin uso (firma estandar de tarea CMSIS-RTOS v2).
 * @note   Crear con osThreadNew(Log_Task, NULL, &attr) en Tareas_CrearTareas(),
 *         despues de Log_InitQueue(). Es la UNICA que llama a la transmision
 *         bloqueante real -- todo lo demas solo encola.
 */
void Log_Task(void *argument);

/**
 * @brief  Prints a tagged log message over USB CDC.
 * @param  tag  Short module identifier, e.g. "BT", "I2C", "TEST".
 * @param  msg  Message string (NUL-terminated).
 *
 * Output format:  [TAG] msg\r\n
 * @note   No bloqueante una vez que existe la cola (Log_InitQueue() ya
 *         corrio) -- arma la linea y la encola, regresa de inmediato.
 */
void Log_Print(const char *tag, const char *msg);

/**
 * @brief  Prints a tagged, printf-style formatted log message over USB CDC.
 * @param  tag  Short module identifier, e.g. "BT", "I2C", "TEST".
 * @param  fmt  printf-style format string.
 * @note   Message is built in an internal fixed-size buffer via vsnprintf —
 *         truncates silently if the formatted message is longer.
 *
 * Output format:  [TAG] formatted message\r\n
 */
void Log_Printf(const char *tag, const char *fmt, ...);

/**
 * @brief  Prints a blank line (just "\r\n"), no tag.
 * @note   Usado como separador visual entre secciones del log.
 */
void Log_NewLine(void);

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
