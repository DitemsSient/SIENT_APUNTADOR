/**
 * @file    SensorHall.h
 * @brief   Driver for HW-484 Hall effect sensor (digital comparator output) on STM32L433.
 *
 * @details Reads the digital output (DO) of the HW-484 module's comparator via a
 *          plain GPIO input — no ADC involved. The comparator outputs a clean
 *          HIGH/LOW level instead of a raw analog reading.
 *          CubeMX configuration (this board):
 *          - PA8 ("GATILLO") as GPIO_Input, no pull (comparator drives the line).
 *
 * @date    March 27, 2026
 * @author  César Pérez
 * @version 2.0.0
 */

#ifndef SENSORHALL_H
#define SENSORHALL_H

#include "stm32l4xx_hal.h"
#include <stdbool.h>

/* ========================  CONFIGURATION  ================================= */

/* GPIO port/pin connected to the Hall sensor comparator output (DO) */

#define HALL_GPIO_PORT           GPIOA
#define HALL_GPIO_PIN            GPIO_PIN_8

/* ================================  API  =================================== */

/**
 * @brief  Reads the Hall sensor comparator output.
 * @return true  if the pin reads HIGH (magnet detected / trigger pressed).
 * @return false if the pin reads LOW (no magnet / trigger released).
 */
bool HallSensor_IsPressed(void);

/* ========================  SELF-TEST  ==================================== */

/**
 * @brief  Reads the pin once and reports success.
 * @return 1 always — this test only validates that the GPIO read executes;
 *         it does not require a magnet present (see HallSensor_IsPressed()
 *         for the actual state).
 */
uint8_t HallSensor_Test(void);

/* ============================================================================
 * VERSIÓN ANTERIOR (ADC, HW-484 en modo analógico) — COMENTADA
 *
 * Se deja aquí de referencia porque probablemente volvamos a esta variante
 * cuando ruteemos un pin con ADC disponible en el .ioc. Por ahora esta
 * tarjeta no tiene ningún ADC configurado, y el HW-484 que tenemos sí trae
 * comparador propio, así que usamos su salida digital (arriba) en su lugar.
 *
 * #define HALL_ADC_HANDLE         (&hadc1)
 * #define HALL_ADC_CHANNEL        ADC_CHANNEL_11
 *
 * #define HALL_ADC_MAX            4095U
 * #define HALL_MIDPOINT           2048U
 * #define HALL_THRESHOLD          200U
 * #define HALL_ADC_TIMEOUT_MS     10U
 *
 * typedef enum {
 *     HALL_NO_FIELD    = 0,
 *     HALL_NORTH_POLE  = 1,
 *     HALL_SOUTH_POLE  = 2
 * } HallState_e;
 *
 * typedef struct {
 *     uint16_t     raw_value;
 *     HallState_e  state;
 *     uint16_t     deviation;
 * } HallData_t;
 *
 * void HallSensor_Init(void);
 * HAL_StatusTypeDef HallSensor_Read(HallData_t *data);
 * uint16_t HallSensor_ReadRaw(void);
 * ============================================================================
 */

#endif /* SENSORHALL_H */
