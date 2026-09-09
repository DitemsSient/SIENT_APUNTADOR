/**
 * @file    Secuencias_LED.h
 * @brief   Catalogo de secuencias de parpadeo del LED RGB con significado fijo.
 *
 * @details Centraliza TODO el manejo del LED RGB de la tarjeta -- tanto las
 *          secuencias de un solo tiro con significado fijo (fin de
 *          ejercicio, desconexion, etc.) como los parpadeos no bloqueantes
 *          que otras pantallas necesitan mientras hacen polling (ej.
 *          Bluetooth esperando conexion). Ningun otro archivo (fuera de
 *          Bootloader.c, ver nota abajo) debe llamar a LedRGB_* directo --
 *          todo pasa por aqui, para no tener parpadeos sueltos e
 *          inconsistentes regados por el codigo (8-sep-2026).
 *
 *          Catalogo de secuencias (bloqueantes, un solo tiro):
 *          - SecuenciasLED_FinEjercicio()      -- ciclo R-V-A-Magenta x3 @ 300ms
 *          - SecuenciasLED_FinPorDesconexion() -- cyan x5 @ 400ms
 *          - SecuenciasLED_FinPorAdmin()       -- rojo x5 @ 400ms
 *
 *          NOTA (8-sep-2026): colores/tiempos de las 3 de arriba homogenizados
 *          a proposito con el catalogo de LEDs de la tarjeta Sensores
 *          (Leds_Parpadeo*.c) -- mismo evento debe verse igual en ambas
 *          tarjetas.
 *
 *          Helpers no bloqueantes (para pantallas con su propio loop de
 *          polling, ej. Menu_Bluetooth.c mientras espera conexion):
 *          - SecuenciasLED_Fijo() / SecuenciasLED_Apagar()
 *          - SecuenciasLED_ParpadeoNoBloqueanteReset() / ...Tick()
 *
 *          NOTA: el parpadeo verde/rojo x3 del bootloader (Bootloader.c,
 *          Bootloader_BlinkLed()) NO se migro aqui a proposito -- es una
 *          ruta critica (indica si se entro a modo DFU) y no se quiso
 *          tocar ese archivo. Tampoco se migraron los usos sueltos de
 *          LedRGB_* en Test.c (banco de pruebas de bring-up, valida el
 *          driver directo a proposito, no es parte del flujo real).
 *
 * @date    September 2, 2026
 * @author  César Pérez
 * @version 2.0.0
 */

#ifndef SECUENCIAS_LED_H
#define SECUENCIAS_LED_H

#include "stm32l4xx_hal.h"
#include "LedRGB.h"
#include <stdbool.h>

/* ==========================  HELPERS GENERICOS  ============================ */

/**
 * @brief  Apaga el LED RGB.
 */
void SecuenciasLED_Apagar(void);

/**
 * @brief  Pone el LED RGB en un color fijo (sin parpadeo).
 * @param  color  Color a mostrar (usar las constantes RGB_* de LedRGB.h).
 */
void SecuenciasLED_Fijo(RGBColor_t color);

/**
 * @brief  Reinicia el estado de un parpadeo no bloqueante (lo deja apagado
 *         y arranca el timer de referencia).
 * @param  tick  Puntero al tick de referencia del caller (se sobreescribe).
 * @param  on    Puntero al estado on/off del caller (se pone en false).
 * @note   El caller es dueno de `tick`/`on` (normalmente estaticos de su
 *         propio archivo) -- esta funcion solo los inicializa.
 */
void SecuenciasLED_ParpadeoNoBloqueanteReset(uint32_t *tick, bool *on);

/**
 * @brief  Revisa si ya paso `period_ms` desde el ultimo toggle y, si es el
 *         caso, invierte el estado on/off y actualiza el LED.
 * @param  color     Color a mostrar cuando el estado esta "encendido".
 * @param  tick      Puntero al tick de referencia del caller (se actualiza
 *                   al togglear).
 * @param  on        Puntero al estado on/off del caller (se invierte al
 *                   togglear).
 * @param  period_ms Periodo entre toggles, en ms.
 * @note   No bloqueante -- llamar en cada vuelta del loop del caller
 *         (ej. Screen_*_Draw() de una pantalla que hace polling).
 */
void SecuenciasLED_ParpadeoNoBloqueanteTick(RGBColor_t color, uint32_t *tick,
                                             bool *on, uint16_t period_ms);

/* ====================  CATALOGO DE SECUENCIAS (bloqueantes)  ============== */

/**
 * @brief  Secuencia de fin de ejercicio (temporizador general agotado, fin normal).
 * @note   Bloqueante (~3.6 s total). Ciclo Rojo-Verde-Azul-Magenta, 300ms c/u,
 *         3 vueltas. LED queda apagado. Homogenizado con Sensores.
 */
void SecuenciasLED_FinEjercicio(void);

/**
 * @brief  Secuencia de fin de ejercicio por desconexion de Bluetooth ($DSCON,
 *         en medio del ejercicio o fuera de uno via Menu_HandleDisconnect()).
 * @note   Bloqueante (~4 s total). Cyan parpadeando cada 400ms, 5 veces.
 *         LED queda apagado.
 */
void SecuenciasLED_FinPorDesconexion(void);

/**
 * @brief  Secuencia de fin de ejercicio por orden del administrador ($END_S
 *         recibido en medio del ejercicio).
 * @note   Bloqueante (~4 s total). Rojo parpadeando cada 400ms, 5 veces.
 *         LED queda apagado.
 */
void SecuenciasLED_FinPorAdmin(void);

#endif /* SECUENCIAS_LED_H */
