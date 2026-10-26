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

/* Ciclo Rojo-Verde-Azul-Magenta a 300ms por color, 3 vueltas = 12 pasos.
 * Homogenizado con la tarjeta Sensores (Leds_ParpadeoFinJuego(),
 * LEDS_FIN_JUEGO_MS=300, LEDS_FIN_JUEGO_VUELTAS=3, 8-sep-2026) -- mismo
 * color y tiempo exacto para que el evento se vea igual en ambas tarjetas. */
static const RGBStep_t s_patron_fin_ejercicio[] = {
    { { 1, 0, 0 }, 300U },     /* Rojo    */
    { { 0, 1, 0 }, 300U },     /* Verde   */
    { { 0, 0, 1 }, 300U },     /* Azul    */
    { { 1, 0, 1 }, 300U },     /* Magenta */
    { { 1, 0, 0 }, 300U },     /* Rojo    */
    { { 0, 1, 0 }, 300U },     /* Verde   */
    { { 0, 0, 1 }, 300U },     /* Azul    */
    { { 1, 0, 1 }, 300U },     /* Magenta */
    { { 1, 0, 0 }, 300U },     /* Rojo    */
    { { 0, 1, 0 }, 300U },     /* Verde   */
    { { 0, 0, 1 }, 300U },     /* Azul    */
    { { 1, 0, 1 }, 300U },     /* Magenta */
};
#define PATRON_FIN_EJERCICIO_LEN   RGB_PATTERN_LEN(s_patron_fin_ejercicio)

/* ==========================  HELPERS GENERICOS  ============================ */

void SecuenciasLED_Apagar(void)
{
    LedRGB_Off();
}

void SecuenciasLED_Fijo(RGBColor_t color)
{
    LedRGB_SetColor(color);
}

void SecuenciasLED_ParpadeoNoBloqueanteReset(uint32_t *tick, bool *on)
{
    *tick = HAL_GetTick();
    *on   = false;
    LedRGB_Off();
}

void SecuenciasLED_ParpadeoNoBloqueanteTick(RGBColor_t color, uint32_t *tick,
                                             bool *on, uint16_t period_ms)
{
    if ((HAL_GetTick() - *tick) >= period_ms) {
        *tick = HAL_GetTick();
        *on   = !(*on);
        LedRGB_SetColor(*on ? color : RGB_OFF);
    }
}

/**
 * @brief  Alterna dos colores `veces` veces cada uno, `ms` por paso.
 * @note   Bloqueante. LED queda apagado al terminar.
 */
static void Led_Alternar2(RGBColor_t a, RGBColor_t b, uint8_t veces, uint16_t ms)
{
    for (uint8_t i = 0U; i < veces; i++) {
        LedRGB_SetColor(a);
        HAL_Delay(ms);
        LedRGB_SetColor(b);
        HAL_Delay(ms);
    }
    LedRGB_Off();
}

/* ====================  CATALOGO DE SECUENCIAS (bloqueantes)  ============== */

void SecuenciasLED_FinEjercicio(void)
{
    LedRGB_PlayPattern(s_patron_fin_ejercicio, PATRON_FIN_EJERCICIO_LEN);
}

void SecuenciasLED_FinPorDesconexion(void)
{
    LedRGB_Blink(RGB_CYAN, 5U, 400U, 400U);
}

void SecuenciasLED_FinDetenido(void)
{
    LedRGB_Blink(RGB_RED, 5U, 400U, 400U);
}

void SecuenciasLED_FinGanaste(void)
{
    Led_Alternar2(RGB_GREEN, RGB_BLUE, 5U, 400U);
}

void SecuenciasLED_FinPerdiste(void)
{
    /* Sin naranja real en el HW (RGB binario de 3 canales) -- amarillo es
     * lo mas cercano, decidido 15-oct-2026. */
    Led_Alternar2(RGB_RED, RGB_YELLOW, 5U, 400U);
}

void SecuenciasLED_FinEmpate(void)
{
    Led_Alternar2(RGB_MAGENTA, RGB_CYAN, 5U, 400U);
}
