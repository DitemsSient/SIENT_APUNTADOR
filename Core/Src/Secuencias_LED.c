/**
 * @file    Secuencias_LED.c
 * @brief   Implementacion del catalogo de secuencias de LED RGB.
 *
 * @date    September 2, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#include "Secuencias_LED.h"
#include "LedRGB.h"

/* ====================  DEVICE CONSTANTS  ================================== */

/* Arcoiris a 500ms por color (6 colores), 2 vueltas = 12 pasos. */
static const RGBStep_t s_patron_fin_ejercicio[] = {
    { { 1, 0, 0 }, 500U },     /* Rojo    */
    { { 1, 1, 0 }, 500U },     /* Amarillo*/
    { { 0, 1, 0 }, 500U },     /* Verde   */
    { { 0, 1, 1 }, 500U },     /* Cian    */
    { { 0, 0, 1 }, 500U },     /* Azul    */
    { { 1, 0, 1 }, 500U },     /* Magenta */
    { { 1, 0, 0 }, 500U },     /* Rojo    */
    { { 1, 1, 0 }, 500U },     /* Amarillo*/
    { { 0, 1, 0 }, 500U },     /* Verde   */
    { { 0, 1, 1 }, 500U },     /* Cian    */
    { { 0, 0, 1 }, 500U },     /* Azul    */
    { { 1, 0, 1 }, 500U },     /* Magenta */
};
#define PATRON_FIN_EJERCICIO_LEN   RGB_PATTERN_LEN(s_patron_fin_ejercicio)

/* ================================  API  =================================== */

void SecuenciasLED_FinEjercicio(void)
{
    LedRGB_PlayPattern(s_patron_fin_ejercicio, PATRON_FIN_EJERCICIO_LEN);
    LedRGB_Blink(RGB_RED, 3U, 1000U, 1000U);
}

void SecuenciasLED_FinPorDesconexion(void)
{
    LedRGB_Blink(RGB_RED, 4U, 800U, 800U);
}
