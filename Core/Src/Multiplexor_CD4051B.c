/**
 * @file    Multiplexor_CD4051B.c
 * @brief   Driver implementation for CD4051B 8-channel analog multiplexer.
 *
 * @date    March 06, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#include "Multiplexor_CD4051B.h"

/* ======================  STATIC FUNCTIONS  ================================ */

/* Forward declarations */

static void WriteSelectionPins(mux_channel_t channel);
static bool IsValidChannel(mux_channel_t channel);

/**
 * @brief  Drives the A, B, C GPIO pins to the binary encoding of the channel.
 * @param  channel  Target channel (bits 0, 1, 2 map to pins A, B, C).
 */
static void WriteSelectionPins(mux_channel_t channel) {
    HAL_GPIO_WritePin(MUX_PIN_A_PORT, MUX_PIN_A_PIN,
                      ((channel >> 0U) & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(MUX_PIN_B_PORT, MUX_PIN_B_PIN,
                      ((channel >> 1U) & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(MUX_PIN_C_PORT, MUX_PIN_C_PIN,
                      ((channel >> 2U) & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  Checks whether a channel index is within the valid range.
 * @param  channel  Channel to validate.
 * @note   Returns true if channel < MUX_CHANNEL_TOTAL.
 */
static bool IsValidChannel(mux_channel_t channel) {
    return (channel < MUX_CHANNEL_TOTAL);
}

/* ================================  API  =================================== */

/* Public functions declared in the .h */

/**
 * @brief  Clears the handle, drives all selection pins LOW (channel 0),
 *         then sets initialized = true.
 */
mux_status_t MUX_Init(mux_handle_t *hmux) {
    if (hmux == NULL) {
        return MUX_ERR_NULL_PARAM;
    }

    hmux->initialized    = false;
    hmux->active_channel = MUX_CHANNEL_0;

    WriteSelectionPins(MUX_CHANNEL_0);

    hmux->initialized = true;
    return MUX_OK;
}

/**
 * @brief  Validates the handle and channel, calls WriteSelectionPins(),
 *         then updates active_channel in the handle.
 */
mux_status_t MUX_SelectChannel(mux_handle_t *hmux, mux_channel_t channel) {
    if (hmux == NULL)           return MUX_ERR_NULL_PARAM;
    if (!hmux->initialized)     return MUX_ERR_NOT_INIT;
    if (!IsValidChannel(channel)) return MUX_ERR_INVALID_CH;

    WriteSelectionPins(channel);
    hmux->active_channel = channel;

    return MUX_OK;
}

/**
 * @brief  Returns hmux->active_channel, or MUX_CHANNEL_0 if hmux is NULL.
 */
mux_channel_t MUX_GetActiveChannel(const mux_handle_t *hmux) {
    if (hmux == NULL) {
        return MUX_CHANNEL_0;
    }
    return hmux->active_channel;
}

/**
 * @brief  Returns MUX_OK if the handle is valid and initialized,
 *         or the appropriate error code otherwise.
 */
mux_status_t MUX_GetStatus(const mux_handle_t *hmux) {
    if (hmux == NULL)       return MUX_ERR_NULL_PARAM;
    if (!hmux->initialized) return MUX_ERR_NOT_INIT;
    return MUX_OK;
}
