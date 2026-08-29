/**
 * @file    Logger.h
 * @brief   Serial logging over the USB CDC virtual COM port.
 *
 * @details Provides Log_Print()/Log_Printf() for any driver or module to
 *          emit tagged text messages to a terminal connected to the board's
 *          USB CDC port (see USB_DEVICE/App/usbd_cdc_if.c, CDC_Transmit_FS).
 *
 *          Thread-safety: Log_TransmitUSB() (the actual CDC_Transmit_FS()
 *          call) is protected by an internal RTOS mutex so two tasks can't
 *          interleave their output on the shared USB endpoint. The mutex
 *          must be created with Log_InitMutex() AFTER osKernelInitialize()
 *          — see main.c, USER CODE BEGIN RTOS_MUTEX. Until that runs, calls
 *          are unlocked (safe, since only single-threaded code — bootloader/
 *          Inicializacion_Run() — logs before the scheduler exists).
 *
 *          Usage:
 *            main.c  → call Log_Init() once after MX_USB_DEVICE_Init()
 *                      (pre-RTOS, inside Inicializacion_Run()), then
 *                      Log_InitMutex() once after osKernelInitialize().
 *                      Only reached when Bootloader_CheckAndEnter() did NOT
 *                      jump to the DFU bootloader (see Bootloader.h).
 *            others  → #include "Logger.h" and call Log_Print(TAG, msg).
 *
 * @date    July 03, 2026
 * @author  César Pérez
 * @version 3.0.0
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

/* ========================  API  =========================================== */

/**
 * @brief  Initializes the logger.
 * @note   Call once after MX_USB_DEVICE_Init() in main.c — and only on the
 *         path where the app did NOT jump to the bootloader.
 */
void Log_Init(void);

/**
 * @brief  Crea el mutex interno del Logger.
 * @note   Llamar despues de osKernelInitialize() (no antes -- el kernel
 *         debe estar listo para crear objetos RTOS). Ver Tareas_Interrupciones.c.
 */
void Log_InitMutex(void);

/**
 * @brief  Prints a tagged log message over USB CDC.
 * @param  tag  Short module identifier, e.g. "BT", "I2C", "TEST".
 * @param  msg  Message string (NUL-terminated).
 *
 * Output format:  [TAG] msg\r\n
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
