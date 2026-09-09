/**
 * @file    Logger.c
 * @brief   Serial logging implementation over USB CDC, con cola de FreeRTOS.
 *
 * @date    September 7, 2026
 * @author  César Pérez
 * @version 4.1.0
 */

#include "Logger.h"
#include "usbd_cdc_if.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

extern USBD_HandleTypeDef hUsbDeviceFS;

/* ========================  PRIVATE STATE  ================================= */

typedef struct {
    char     line[LOG_MAX_MSG_LEN];
    uint16_t len;
} LogEntry_t;

static bool               s_ready   = false;
static osMessageQueueId_t s_queue   = NULL;
static uint32_t           s_dropped = 0U;  /**< Lineas perdidas por cola llena */

/* ======================  STATIC FUNCTIONS  ================================ */

/**
 * @brief  Sends a buffer over USB CDC, retrying while el endpoint esta ocupado,
 *         y esperando a que el hardware termine de leerlo antes de regresar.
 * @param  data  Buffer to transmit.
 * @param  len   Number of bytes.
 * @note   Gives up silently after LOG_TX_TIMEOUT_MS — logging must never
 *         hang the app if nothing is connected on the other end.
 *
 *         Unica llamadora: LoggerTask (Log_Task), consumiendo la cola uno a
 *         la vez -- ya no hace falta mutex, no hay dos tareas transmitiendo
 *         al mismo tiempo (salvo el fallback pre-cola, tambien serializado
 *         porque solo hay un hilo de ejecucion corriendo en ese punto).
 *
 *         CDC_Transmit_FS() solo guarda el PUNTERO al buffer (USBD_CDC_SetTxBuffer),
 *         no copia los datos -- el hardware USB los sigue leyendo de forma
 *         asincrona (via interrupcion) despues de que la funcion ya regreso.
 *         Sin esperar aqui a que hcdc->TxState vuelva a 0, ese buffer se
 *         podia reutilizar (la siguiente vuelta de LoggerTask, o el stack
 *         del caller en el fallback) mientras el USB todavia lo transmitia,
 *         corrompiendo el log.
 */
static void Log_TransmitUSB(const uint8_t *data, uint16_t len) {
    uint32_t start = HAL_GetTick();
    uint8_t  usb_status;

    do {
        usb_status = CDC_Transmit_FS((uint8_t *)data, len);
    } while ((usb_status == USBD_BUSY) &&
             ((HAL_GetTick() - start) <= LOG_TX_TIMEOUT_MS));

    if (usb_status == USBD_OK) {
        /* Se logro arrancar la transmision -- esperar a que el hardware
         * termine de leer el buffer antes de regresar. */
        USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
        while ((hcdc != NULL) && (hcdc->TxState != 0U) &&
               ((HAL_GetTick() - start) <= LOG_TX_TIMEOUT_MS)) {
            /* esperando a que el USB termine de vaciar el buffer */
        }
    }
}

/**
 * @brief  Arma la entrada y la encola (no bloqueante); si la cola todavia
 *         no existe (pre-RTOS) o esta llena, cae al camino directo/descarta.
 * @param  line  Texto ya armado (con \r\n incluido).
 * @param  len   Longitud en bytes.
 */
static void Log_Enqueue(const char *line, uint16_t len) {
    if (s_queue == NULL) {
        /* Cola todavia no existe -- arranque bare-metal pre-RTOS, un solo
         * hilo de ejecucion corriendo, seguro transmitir directo. */
        Log_TransmitUSB((const uint8_t *)line, len);
        return;
    }

    LogEntry_t entry;
    if (len >= sizeof(entry.line)) {
        len = (uint16_t)(sizeof(entry.line) - 1U);
    }
    memcpy(entry.line, line, len);
    entry.len = len;

    /* timeout 0 -- NUNCA bloquea al llamante, ni siquiera desde ISR-adjacent
     * contexts. Si la cola esta llena (rafaga), se descarta y se cuenta. */
    if (osMessageQueuePut(s_queue, &entry, 0U, 0U) != osOK) {
        s_dropped++;
    }
}

/* ========================  PUBLIC FUNCTIONS  =============================== */

void Log_Init(void)
{
    s_ready = true;
}

void Log_InitQueue(void)
{
    if (s_queue == NULL) {
        s_queue = osMessageQueueNew(LOG_QUEUE_LEN, sizeof(LogEntry_t), NULL);
    }
}

void Log_Task(void *argument)
{
    (void)argument;

    LogEntry_t entry;
    for (;;) {
        if (osMessageQueueGet(s_queue, &entry, NULL, osWaitForever) == osOK) {
            Log_TransmitUSB((const uint8_t *)entry.line, entry.len);

            /* Cola vacia -- ya se consumio todo el lote pendiente. Se manda
             * un separador visual antes de volver a esperar (osWaitForever)
             * el siguiente lote, para que en la consola se note donde
             * termina una "tanda" de logs y empieza la siguiente. */
            if (osMessageQueueGetCount(s_queue) == 0U) {
                Log_TransmitUSB((const uint8_t *)"\r\n", 2U);
            }
        }
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

    Log_Enqueue(out, (uint16_t)len);
}

void Log_NewLine(void)
{
    if (!s_ready) {
        return;
    }
    Log_Enqueue("\r\n", 2U);
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
