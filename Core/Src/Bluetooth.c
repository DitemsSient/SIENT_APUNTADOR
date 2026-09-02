/**
 * @file    Bluetooth.c
 * @brief   Bluetooth UART module driver implementation for STM32F4xx.
 *
 * @details Uses HAL_UART_Transmit / HAL_UART_Receive for blocking transfers.
 *          Interrupt-driven reception is supported via Bt_StoreByte().
 *
 * @date    April 8, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#include "Bluetooth.h"
#include <string.h>

/* ========================  EXTERNAL HAL HANDLES  ========================== */

extern UART_HandleTypeDef huart1;


/* ========================  PUBLIC FUNCTIONS  =============================== */

BtStatus_e Bt_Init(Bt_Handle_t *h)
{
    if (h == NULL) {
        return BT_ERR_PARAM;
    }

    h->huart           = BT_UART;
    h->rx_count        = 0U;
    h->rx_byte         = 0U;
    h->rx_ready        = false;
    h->raw_debug_count = 0U;
    memset(h->tx_buffer, 0, BT_TX_BUFFER_SIZE);
    memset(h->rx_buffer, 0, BT_RX_BUFFER_SIZE);
    memset(h->raw_debug, 0, BT_RAW_DEBUG_LEN);

    /* Arm interrupt reception for the first byte */
    HAL_UART_Receive_IT(h->huart, &h->rx_byte, 1U);

    return BT_OK;
}

BtStatus_e Bt_Transmit(Bt_Handle_t *h, const uint8_t *data, uint16_t len)
{
    if (h == NULL || data == NULL || len == 0U) {
        return BT_ERR_PARAM;
    }

    HAL_StatusTypeDef hal = HAL_UART_Transmit(h->huart, data, len, BT_TX_TIMEOUT_MS);
    if (hal != HAL_OK) {
        return (hal == HAL_TIMEOUT) ? BT_ERR_TIMEOUT : BT_ERR_UART;
    }

    return BT_OK;
}

BtStatus_e Bt_Receive(Bt_Handle_t *h, uint8_t *data, uint16_t len)
{
    if (h == NULL || data == NULL || len == 0U) {
        return BT_ERR_PARAM;
    }

    HAL_StatusTypeDef hal = HAL_UART_Receive(h->huart, data, len, BT_RX_TIMEOUT_MS);
    if (hal != HAL_OK) {
        return (hal == HAL_TIMEOUT) ? BT_ERR_TIMEOUT : BT_ERR_UART;
    }

    return BT_OK;
}

void Bt_StoreByte(Bt_Handle_t *h)
{
    if (h == NULL) {
        return;
    }

    /* Captura cruda para diagnostico -- ring buffer, solo memoria, nada de
     * Logger/USB aqui (esto corre en ISR). Guarda los ultimos N bytes,
     * asi siempre se ve la cola mas reciente aunque haya mucho texto de
     * debug del modulo antes del frame real. */
    h->raw_debug[h->raw_debug_count % BT_RAW_DEBUG_LEN] = h->rx_byte;
    h->raw_debug_count++;

    if (h->rx_byte == '$') {
        h->rx_count = 0U;
        h->rx_ready = false;
        h->rx_buffer[0] = '\0';
    } else if (h->rx_byte == '\r') {
        if (h->rx_count > 0U) {
            h->rx_ready = true;
        }
    } else if (h->rx_count < BT_RX_BUFFER_SIZE - 1U) {
        h->rx_buffer[h->rx_count] = h->rx_byte;
        h->rx_count++;
        h->rx_buffer[h->rx_count] = '\0';
    }

    /* Re-arm interrupt for next byte */
    HAL_UART_Receive_IT(h->huart, &h->rx_byte, 1U);
}

void Bt_ResetRx(Bt_Handle_t *h)
{
    if (h == NULL) {
        return;
    }

    h->rx_count = 0U;
    h->rx_ready = false;
    memset(h->rx_buffer, 0, BT_RX_BUFFER_SIZE);
}

void Bt_ResetRawDebug(Bt_Handle_t *h)
{
    if (h == NULL) {
        return;
    }

    h->raw_debug_count = 0U;
}

/**
 * @brief  Enruta la recepción por IT de huart1 hacia Bt_StoreByte().
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    extern Bt_Handle_t Bluetooth;

    if (huart->Instance == USART1) {
        Bt_StoreByte(&Bluetooth);
    }
}

/* ========================  ADVERTISE API  ================================ */

BtStatus_e Bt_SendAdvertise(Bt_Handle_t *h)
{
    if (h == NULL) { return BT_ERR_PARAM; }

    static const uint8_t cmd[] = "$CON\r";
    Bt_ResetRx(h);
    return Bt_Transmit(h, cmd, sizeof(cmd) - 1U);
}

/* ========================  SELF-TEST  ==================================== */

uint8_t Bt_Test(void)
{
    extern Bt_Handle_t Bluetooth;

    static const uint8_t cmd[] = BT_CMD_TEST;

    Bt_Init(&Bluetooth);
    Bt_ResetRx(&Bluetooth);

    if (Bt_Transmit(&Bluetooth, cmd, sizeof(cmd) - 1U) != BT_OK) { return 0U; }

    /* La recepcion es por IT (armada en Bt_Init) -- esperar rx_ready en vez
     * de HAL_UART_Receive() bloqueante, que chocaria con la IT ya armada. */
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < BT_RX_TIMEOUT_MS) {
        if (Bluetooth.rx_ready) {
            uint8_t ok = (strstr((char *)Bluetooth.rx_buffer, "00") != NULL) ? 1U : 0U;
            Bt_ResetRx(&Bluetooth);
            return ok;
        }
        HAL_Delay(5U);
    }

    return 0U;
}
