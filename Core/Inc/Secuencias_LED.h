/**
 * @file    Secuencias_LED.h
 * @brief   Catalogo de secuencias de parpadeo del LED RGB con significado fijo.
 *
 * @details Centraliza los patrones de LED que antes estaban sueltos e
 *          inline en cada archivo (Bootloader.c, pruebas de EXTI, etc.),
 *          para que cada secuencia tenga un nombre y un significado unico
 *          y no haya que adivinar que parpadeo es cual. Como en la practica
 *          no siempre hay Logger a la mano (o no se ocupa), estas
 *          secuencias son la forma principal de comunicar estado.
 *
 *          Catalogo actual:
 *          - SecuenciasLED_FinEjercicio()      -- arcoiris x2 + rojo x3
 *          - SecuenciasLED_FinPorDesconexion() -- rojo x4 @ 800ms
 *
 *          NOTA: el parpadeo verde/rojo x3 del bootloader (Bootloader.c,
 *          Bootloader_BlinkLed()) NO se migro aqui a proposito -- es una
 *          ruta critica (indica si se entro a modo DFU) y no se quiso
 *          tocar ese archivo. Queda documentado aqui solo como referencia.
 *
 * @date    September 2, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#ifndef SECUENCIAS_LED_H
#define SECUENCIAS_LED_H

#include "stm32l4xx_hal.h"

/* ================================  API  =================================== */

/**
 * @brief  Secuencia de fin de ejercicio (temporizador general agotado, fin normal).
 * @note   Bloqueante (~8.5 s total). Arcoiris (6 colores, 500ms c/u), 2 vueltas,
 *         seguido de rojo encendido/apagado cada 1s, 3 veces. LED queda apagado.
 */
void SecuenciasLED_FinEjercicio(void);

/**
 * @brief  Secuencia de fin de ejercicio por desconexion de Bluetooth ($DSCON
 *         recibido en medio del ejercicio).
 * @note   Bloqueante (~6.4 s total). Rojo parpadeando cada 800ms, 4 veces.
 *         LED queda apagado.
 */
void SecuenciasLED_FinPorDesconexion(void);

#endif /* SECUENCIAS_LED_H */
