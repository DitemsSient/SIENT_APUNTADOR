/**
 * @file    SensorHall.c
 * @brief   Driver implementation for HW-484 Hall effect sensor (digital comparator output).
 *
 * @date    March 27, 2026
 * @author  César Pérez
 * @version 2.0.0
 */

#include "SensorHall.h"

/* ================================  API  =================================== */

/* Public functions declared in the .h */

bool HallSensor_IsPressed(void) {
    return (HAL_GPIO_ReadPin(HALL_GPIO_PORT, HALL_GPIO_PIN) == GPIO_PIN_SET);
}

/* ========================  SELF-TEST  ==================================== */

uint8_t HallSensor_Test(void)
{
    (void)HallSensor_IsPressed();
    return 1U;
}

/* ============================================================================
 * VERSIÓN ANTERIOR (ADC, HW-484 en modo analógico) — COMENTADA
 *
 * Se deja aquí de referencia por si volvemos a esta variante cuando ruteemos
 * un pin con ADC disponible. Ver nota equivalente en SensorHall.h.
 *
 * extern ADC_HandleTypeDef hadc1;
 *
 * static uint16_t HallSensor_DoConversion(void) {
 *     ADC_ChannelConfTypeDef cfg = {0};
 *
 *     cfg.Channel      = HALL_ADC_CHANNEL;
 *     cfg.Rank         = 1U;
 *     cfg.SamplingTime = ADC_SAMPLETIME_84CYCLES;
 *
 *     if (HAL_ADC_ConfigChannel(HALL_ADC_HANDLE, &cfg) != HAL_OK) {
 *         return 0U;
 *     }
 *
 *     HAL_ADC_Start(HALL_ADC_HANDLE);
 *
 *     if (HAL_ADC_PollForConversion(HALL_ADC_HANDLE, HALL_ADC_TIMEOUT_MS) != HAL_OK) {
 *         HAL_ADC_Stop(HALL_ADC_HANDLE);
 *         return 0U;
 *     }
 *
 *     uint16_t raw = (uint16_t)HAL_ADC_GetValue(HALL_ADC_HANDLE);
 *     HAL_ADC_Stop(HALL_ADC_HANDLE);
 *
 *     return raw;
 * }
 *
 * void HallSensor_Init(void) {
 *     (void)HallSensor_DoConversion();
 * }
 *
 * HAL_StatusTypeDef HallSensor_Read(HallData_t *data) {
 *     data->raw_value = 0U;
 *     data->state     = HALL_NO_FIELD;
 *     data->deviation = 0U;
 *
 *     uint16_t raw    = HallSensor_DoConversion();
 *     data->raw_value = raw;
 *
 *     data->deviation = (raw >= HALL_MIDPOINT) ? (raw - HALL_MIDPOINT)
 *                                              : (HALL_MIDPOINT - raw);
 *
 *     if (data->deviation > HALL_THRESHOLD) {
 *         data->state = (raw > HALL_MIDPOINT) ? HALL_NORTH_POLE : HALL_SOUTH_POLE;
 *     } else {
 *         data->state = HALL_NO_FIELD;
 *     }
 *
 *     return HAL_OK;
 * }
 *
 * uint16_t HallSensor_ReadRaw(void) {
 *     return HallSensor_DoConversion();
 * }
 * ============================================================================
 */
