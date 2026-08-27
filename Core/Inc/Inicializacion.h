/**
 * @file    Inicializacion.h
 * @brief   Secuencia de arranque de la tarjeta: bootloader, USB CDC y Logger.
 *
 * @details Agrupa, en el orden correcto, lo que antes vivía suelto en
 *          main.c: el chequeo de entrada al bootloader por software (debe
 *          ir primero y puede no regresar), la inicialización del USB CDC
 *          (solo si no se entró al bootloader — el USB debe quedar libre
 *          para que el bootloader lo tome) y la inicialización del Logger
 *          sobre ese mismo USB.
 *
 *          Requiere que MX_GPIO_Init() ya se haya llamado (para LedRGB) y
 *          que Inicializacion_Run() se llame antes que cualquier otro driver
 *          o prueba en main().
 *
 * @date    August 27, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef INICIALIZACION_H
#define INICIALIZACION_H

#include "stm32l4xx_hal.h"

/* ================================  API  =================================== */

/**
 * @brief  Corre la secuencia de arranque completa: Bootloader_CheckAndEnter(),
 *         MX_USB_DEVICE_Init() y Log_Init(), en ese orden.
 * @note   Llamar una sola vez en main(), dentro de USER CODE 2, antes de
 *         cualquier prueba o driver del juego.
 */
void Inicializacion_Run(void);

#endif /* INICIALIZACION_H */
