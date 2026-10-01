/**
 * @file    Multiplexor_CD4051B.h
 * @brief   Driver for CD4051B 8-channel analog multiplexer on STM32F4xx.
 *
 * @details The CD4051B is an 8-channel analog multiplexer/demultiplexer.
 *          Channel selection is performed via three digital GPIO pins (A, B, C)
 *          that form a 3-bit binary code. Configure MUX_PIN_x_PORT/PIN macros
 *          to match the GPIO outputs assigned in CubeMX.
 *          Tarjeta nueva (30-sep-2026): B se movio de PA4 a PA8, C se movio
 *          de PA3 a PC13 (PA4 ahora es GATILLO, PA3 ahora es Buzzer/TIM15_CH2).
 *
 * @date    March 06, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef MULTIPLEXOR_CD4051B_H
#define MULTIPLEXOR_CD4051B_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================  CONFIGURATION  ================================= */

/* GPIO port and pin for each selection bit — update to match CubeMX .ioc */

#define MUX_PIN_A_PORT      GPIOA          /**< Selection pin A port (bit 0) */
#define MUX_PIN_A_PIN       GPIO_PIN_7      /**< Selection pin A              */

#define MUX_PIN_B_PORT      GPIOA           /**< Selection pin B port (bit 1) */
#define MUX_PIN_B_PIN       GPIO_PIN_8      /**< Selection pin B              */

#define MUX_PIN_C_PORT      GPIOC           /**< Selection pin C port (bit 2) */
#define MUX_PIN_C_PIN       GPIO_PIN_13     /**< Selection pin C              */

/* ========================  ENUMERATIONS  ================================== */

/* Available multiplexer channels (CBA = 000 to 111) */

typedef enum {
    MUX_CHANNEL_0 = 0,  /**< C=0, B=0, A=0 — 10  kΩ channel resistance  */
    MUX_CHANNEL_1,      /**< C=0, B=0, A=1 — 15  kΩ channel resistance  */
    MUX_CHANNEL_2,      /**< C=0, B=1, A=0 — 20  kΩ channel resistance  */
    MUX_CHANNEL_3,      /**< C=0, B=1, A=1 — 30  kΩ channel resistance  */
    MUX_CHANNEL_4,      /**< C=1, B=0, A=0 — 45  kΩ channel resistance  */
    MUX_CHANNEL_5,      /**< C=1, B=0, A=1 — 51  kΩ channel resistance  */
    MUX_CHANNEL_6,      /**< C=1, B=1, A=0 — 100 kΩ channel resistance  */
    MUX_CHANNEL_7,      /**< C=1, B=1, A=1 — 150 kΩ channel resistance  */
    MUX_CHANNEL_TOTAL   /**< Total number of channels                    */
} mux_channel_t;

/* Driver status / error codes */

typedef enum {
    MUX_OK              = 0x00U, /**< Operation successful                 */
    MUX_ERR_NULL_PARAM  = 0x01U, /**< NULL pointer received                */
    MUX_ERR_INVALID_CH  = 0x02U, /**< Channel index out of range           */
    MUX_ERR_NOT_INIT    = 0x03U, /**< Driver not initialized               */
} mux_status_t;

/* ============================  STRUCTURES  ================================ */

/* Driver context — holds active channel and initialization state */

typedef struct {
    mux_channel_t   active_channel;     /**< Currently selected channel    */
    bool            initialized;        /**< Initialization flag           */
} mux_handle_t;

/* ================================  API  =================================== */

/**
 * @brief  Configures the multiplexer and selects channel 0 by default.
 * @param  hmux  Pointer to the multiplexer handle.
 * @note   GPIO pins must be configured as outputs in CubeMX before calling.
 *         Returns MUX_OK on success, error code otherwise.
 */
mux_status_t MUX_Init(mux_handle_t *hmux);

/**
 * @brief  Sets the active channel by driving the A/B/C selection pins.
 * @param  hmux     Pointer to the multiplexer handle.
 * @param  channel  Target channel (MUX_CHANNEL_0 … MUX_CHANNEL_7).
 * @note   Returns MUX_OK on success, error code otherwise.
 */
mux_status_t MUX_SelectChannel(mux_handle_t *hmux, mux_channel_t channel);

/**
 * @brief  Returns the currently active channel stored in the handle.
 * @param  hmux  Pointer to the multiplexer handle.
 */
mux_channel_t MUX_GetActiveChannel(const mux_handle_t *hmux);

/**
 * @brief  Returns the last recorded status code (useful for debugging).
 * @param  hmux  Pointer to the multiplexer handle.
 */
mux_status_t MUX_GetStatus(const mux_handle_t *hmux);

#ifdef __cplusplus
}
#endif

#endif /* MULTIPLEXOR_CD4051B_H */
