/**
 * @file    Logger.c
 * @brief   Serial logging implementation over USB CDC.
 *
 * @date    July 03, 2026
 * @author  César Pérez
 * @version 2.0.0
 */

#include "Logger.h"
#include "usbd_cdc_if.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

/* ========================  PRIVATE STATE  ================================= */

static bool s_ready = false;
static osMutexId_t s_mutex = NULL;
static const osMutexAttr_t s_mutex_attr = { .name = "LogMutex" };

/* ======================  STATIC FUNCTIONS  ================================ */

/**
 * @brief  Sends a buffer over USB CDC, retrying while the endpoint is busy.
 * @param  data  Buffer to transmit.
 * @param  len   Number of bytes.
 * @note   Gives up silently after LOG_TX_TIMEOUT_MS — logging must never
 *         hang the app if nothing is connected on the other end. Protegido
 *         por mutex si ya existe (ver Log_InitMutex).
 */
static void Log_TransmitUSB(uint8_t *data, uint16_t len) {
    if (s_mutex != NULL) {
        osMutexAcquire(s_mutex, osWaitForever);
    }

    uint32_t start = HAL_GetTick();
    while (CDC_Transmit_FS(data, len) == USBD_BUSY) {
        if ((HAL_GetTick() - start) > LOG_TX_TIMEOUT_MS) {
            break;
        }
    }

    if (s_mutex != NULL) {
        osMutexRelease(s_mutex);
    }
}

/* ========================  PUBLIC FUNCTIONS  =============================== */

void Log_Init(void)
{
    s_ready = true;
}

void Log_InitMutex(void)
{
    if (s_mutex == NULL) {
        s_mutex = osMutexNew(&s_mutex_attr);
    }
}

void Log_Print(const char *tag, const char *msg)
{
    if (!s_ready || tag == NULL || msg == NULL) {
        return;
    }

    char out[LOG_MAX_MSG_LEN];
    int len = snprintf(out, sizeof(out), "[%s] %s\r\n", tag, msg);

    if (len <= 0) {
        return;
    }
    if ((size_t)len >= sizeof(out)) {
        len = (int)sizeof(out) - 1;
    }

    Log_TransmitUSB((uint8_t *)out, (uint16_t)len);
}

void Log_NewLine(void)
{
    if (!s_ready) {
        return;
    }
    Log_TransmitUSB((uint8_t *)"\r\n", 2U);
}

void Log_Printf(const char *tag, const char *fmt, ...)
{
    if (!s_ready || tag == NULL || fmt == NULL) {
        return;
    }

    char msg[LOG_MAX_MSG_LEN];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    Log_Print(tag, msg);
}
