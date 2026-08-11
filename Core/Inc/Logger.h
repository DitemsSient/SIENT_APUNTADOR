/**
 * @file    Logger.h
 * @brief   Serial logging over a dedicated UART (USART2, PA2/PA3).
 *
 * @details Provides Log_Print() for any driver or module to emit
 *          timestamped text messages to a terminal connected to USART2.
 *          Thread-safety: none — a RTOS mutex should be added later.
 *
 *          Usage:
 *            main.c  → call Log_Init() once after MX_USART2_UART_Init().
 *            others  → #include "Logger.h" and call Log_Print(TAG, msg).
 *
 * @date    July 03, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef LOGGER_H
#define LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"

/* ========================  CONFIGURATION  ================================= */

#define LOG_UART    (&huart1)   /**< UART dedicated to logs (USART2 PA2/PA3) */
#define LOG_TX_TIMEOUT_MS  100U /**< Blocking transmit timeout               */

/* ========================  API  =========================================== */

/**
 * @brief  Initializes the logger and binds it to the log UART.
 * @note   Call once after MX_USART2_UART_Init() in main.c.
 */
void Log_Init(void);

/**
 * @brief  Prints a tagged log message over the log UART.
 * @param  tag  Short module identifier, e.g. "BT", "GPS", "LORA".
 * @param  msg  Message string (NUL-terminated).
 *
 * Output format:  [TAG] msg\r\n
 */
void Log_Print(const char *tag, const char *msg);

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
