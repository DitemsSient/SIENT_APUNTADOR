/**
 * @file    Bluetooth.h
 * @brief   Driver for UART Bluetooth module on STM32F4xx.
 *
 * @details Provides basic UART transmit/receive interface for a Bluetooth
 *          module (e.g., HC-05, HC-06, HM-10, or similar).
 *          CubeMX configuration:
 *          - UART peripheral in Asynchronous mode (commonly 9600 or 38400 baud).
 *          - No hardware flow control required.
 *
 * @date    April 8, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ========================  CONFIGURATION  ================================= */

/* UART handle — update to match CubeMX .ioc */

#define BT_UART                 (&huart1)

/* Timeouts */

#define BT_TX_TIMEOUT_MS        500U    /**< Transmit timeout in ms          */
#define BT_RX_TIMEOUT_MS        500U    /**< Receive timeout in ms           */

/* Buffer sizes */

#define BT_TX_BUFFER_SIZE       256U    /**< Max transmit payload bytes      */
#define BT_RX_BUFFER_SIZE       256U    /**< Max receive payload bytes       */
#define BT_RAW_DEBUG_LEN        32U     /**< Bytes crudos capturados para diagnostico */

/* Advertise protocol */

#define BT_ADVERTISE_TIMEOUT_MS 21000U  /**< Margen local sobre el timeout de 20s
                                             del modulo (manda $NoCON si nadie
                                             se conecta antes de eso)          */

/* Comandos AT del BL654 (app "AT Interface" de Laird/Ezurio, FW 29.5.7.2
 * confirmado en pruebas). Terminan en '\r' unicamente (sin '\n'), tokens
 * separados por espacio. Exito = "00" en la respuesta; error = "01<TAB>Exxx". */

#define BT_CMD_TEST         "AT\r"      /**< Test basico -> "00"                    */
#define BT_CMD_VERSION      "AT I 3\r"  /**< Version FW -> "10\t3\t<version>\r00"   */

/* Comando que hace que el BL654 corra su programa cargado ("Apuntador"),
 * ya que todavia no tenemos el autorun configurado en el modulo. Se manda
 * una sola vez, siempre, como parte de la inicializacion del modulo (ver
 * Bt_SendRunBLE() / Inicializacion.c) -- ya no es un boton del menu
 * (14-sep-2026). El modulo no regresa respuesta a este comando (confirmado
 * en pruebas), asi que no se espera nada por UART. */
#define BT_CMD_RUNBLE       "AT+RUN \"Apuntador\"\r\n"

/* TODO: AT+DIR -- lista los archivos cargados en el modulo. Pendiente de
 * probar y documentar el formato de respuesta. */

/* ========================  ENUMERATIONS  ================================== */

/* Driver status / error codes */

typedef enum {
    BT_OK               = 0,    /**< Operation successful                    */
    BT_ERR_PARAM,               /**< Invalid or NULL parameter               */
    BT_ERR_UART,                /**< UART communication error                */
    BT_ERR_TIMEOUT,             /**< Operation timed out                     */
    BT_ERR_BUSY,                /**< Module busy                             */
    BT_ERR_OVERFLOW             /**< Buffer overflow                         */
} BtStatus_e;

/* ============================  STRUCTURES  ================================ */

/* Bluetooth driver control handle */

typedef struct {
    UART_HandleTypeDef *huart;          /**< CubeMX-generated UART handle    */
    uint8_t             tx_buffer[BT_TX_BUFFER_SIZE];   /**< Transmit buffer */
    uint8_t             rx_buffer[BT_RX_BUFFER_SIZE];   /**< Receive buffer  */
    uint16_t            rx_count;       /**< Bytes accumulated in rx_buffer  */
    uint8_t             rx_byte;        /**< Last byte from UART interrupt   */
    bool                rx_ready;       /**< true when data is available     */
    uint8_t             raw_debug[BT_RAW_DEBUG_LEN]; /**< Ring buffer: ultimos BT_RAW_DEBUG_LEN bytes crudos */
    uint16_t            raw_debug_count;              /**< Total acumulado (no capado) -- usar %BT_RAW_DEBUG_LEN */
} Bt_Handle_t;

/* ================================  API  =================================== */

/**
 * @brief  Initializes the Bluetooth handle and binds it to the configured UART.
 * @param  h  Pointer to the Bluetooth handle.
 */
BtStatus_e Bt_Init(Bt_Handle_t *h);

/**
 * @brief  Transmits a data buffer through the Bluetooth module.
 * @param  h     Pointer to the Bluetooth handle.
 * @param  data  Pointer to the data to send.
 * @param  len   Number of bytes to send.
 */
BtStatus_e Bt_Transmit(Bt_Handle_t *h, const uint8_t *data, uint16_t len);

/**
 * @brief  Receives data from the Bluetooth module (blocking).
 * @param  h     Pointer to the Bluetooth handle.
 * @param  data  Destination buffer.
 * @param  len   Number of bytes to receive.
 */
BtStatus_e Bt_Receive(Bt_Handle_t *h, uint8_t *data, uint16_t len);

/**
 * @brief  Accumulates one received byte into the internal buffer.
 * @param  h  Pointer to the Bluetooth handle.
 * @note   Call from the UART receive ISR (HAL_UART_RxCpltCallback).
 */
void Bt_StoreByte(Bt_Handle_t *h);

/**
 * @brief  Resets the receive buffer and byte counter.
 * @param  h  Pointer to the Bluetooth handle.
 */
void Bt_ResetRx(Bt_Handle_t *h);

/**
 * @brief  Limpia el buffer de diagnostico de bytes crudos (raw_debug).
 * @param  h  Pointer to the Bluetooth handle.
 * @note   Llamar antes de iniciar una espera para capturar solo lo nuevo.
 */
void Bt_ResetRawDebug(Bt_Handle_t *h);

/* ========================  ADVERTISE API  ================================ */

/**
 * @brief  Sends "CON\r" to enter advertising mode and resets the rx buffer.
 * @param  h  Pointer to the Bluetooth handle.
 */
BtStatus_e Bt_SendAdvertise(Bt_Handle_t *h);

/**
 * @brief  Manda BT_CMD_RUNBLE ("AT+RUN \"Apuntador\"\r\n") y resetea el rx.
 * @param  h  Pointer to the Bluetooth handle.
 * @note   Llamar una sola vez durante la inicializacion del modulo (ver
 *         Inicializacion.c). No espera respuesta -- el modulo no contesta
 *         a este comando.
 */
BtStatus_e Bt_SendRunBLE(Bt_Handle_t *h);

/* ========================  SELF-TEST  ==================================== */

/**
 * @brief  Sends BT_CMD_TEST ("AT\r") and checks for "00" in the response.
 * @return 1 if the module replies with "00", 0 on timeout o error.
 */
uint8_t Bt_Test(void);

#ifdef __cplusplus
}
#endif

#endif /* BLUETOOTH_H */
