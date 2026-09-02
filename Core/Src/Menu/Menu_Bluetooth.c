/**
 * @file    Menu_Bluetooth.c
 * @brief   Bluetooth advertise screen — enters advertising, waits for connection.
 *
 * @details Protocolo real del modulo (firmware BL654 propio, confirmado
 *          28-ago-2026):
 *          - $CON\r          → arranca advertising (sin ack inmediato)
 *          - $NoCON\r        → pasaron 20s sin conexion, hay que reenviar $CON
 *          - $OK\r           → alguien (Sensores) se conecto
 *          - $*<datos>\r     → primer dato GATT tras conectar: datos de
 *                              juego separados por coma (ver bt_parse_
 *                              exercise_data), el '*' marca que hay que
 *                              parsear y guardar en g_exercise_data.
 *                              Responde $ACKOK\r si el parseo fue exitoso.
 *          - $<dato>\r       → cualquier dato posterior (fuera de este flujo)
 *          - $DSCON\r        → desconexion en cualquier momento (manejado
 *                              de forma global en Menu.c, no aqui)
 *
 *          Formato de $*<datos>\r (separado por comas):
 *          orden,lora,equipo,alias,vidas,balas,tiempo,mac
 *
 *          State flow:
 *
 *          BT_MAIN (disconnected)          BT_MAIN (connected)
 *            > Anunciar                      Id: XXXXXX
 *              Salir                         > Salir
 *               │ Enter "Anunciar"
 *               ▼
 *          BT_INICIANDO
 *            Sends "$CON\r" via UART
 *            Shows "Anunciando..."
 *               │ auto
 *               ▼
 *          BT_BUSCANDO  (LED azul parpadea 1s)
 *            Espera $OK\r (conectado) o $NoCON\r (timeout del modulo)
 *          ┌────┴────┐
 *          ▼         ▼
 *     BT_ESPERANDO  BT_SPLASH_ERR
 *     Espera $*<datos>\r, parsea y responde $ACKOK\r
 *          │
 *          ▼
 *     BT_SPLASH_OK
 *     Conectado (3 s) → BT_MAIN
 *
 * @date    June 26, 2026
 * @author  César Pérez
 * @version 6.0.0
 */

#include "Menu/Menu_Screens.h"
#include "Bluetooth.h"
#include "Inicializacion.h"
#include "Logger.h"
#include "LedRGB.h"
#include "Display_Oled/Display_Comands.h"
#include "Display_Oled/Display_Fonts.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern Bt_Handle_t Bluetooth;

/* ========================  STATIC STATE  ================================== */

static bool     s_bt_linked  = false;
static uint32_t s_led_tick   = 0U;
static bool     s_led_on     = false;

void Screen_Bluetooth_ResetLink(void)
{
    s_bt_linked = false;
}

/**
 * @brief  Parsea "orden,lora,equipo,alias,vidas,balas,tiempo,mac" en out.
 * @param  payload  Texto a partir de despues del '*' (sin el '$' ni el '\r').
 * @return true si los 8 campos se parsearon correctamente.
 */
static bool bt_parse_exercise_data(const char *payload, ExerciseGameData_t *out)
{
    char buf[BT_RX_BUFFER_SIZE];
    strncpy(buf, payload, sizeof(buf) - 1U);
    buf[sizeof(buf) - 1U] = '\0';

    char *saveptr;
    char *tok = strtok_r(buf, ",", &saveptr);
    if (tok == NULL) { return false; }
    out->orden = (uint8_t)strtoul(tok, NULL, 10);

    tok = strtok_r(NULL, ",", &saveptr);
    if (tok == NULL) { return false; }
    out->lora = (uint8_t)strtoul(tok, NULL, 10);

    tok = strtok_r(NULL, ",", &saveptr);
    if (tok == NULL) { return false; }
    strncpy(out->team_name, tok, EX_TEAM_NAME_MAXLEN);
    out->team_name[EX_TEAM_NAME_MAXLEN] = '\0';

    tok = strtok_r(NULL, ",", &saveptr);
    if (tok == NULL) { return false; }
    strncpy(out->player_name, tok, EX_PLAYER_NAME_MAXLEN);
    out->player_name[EX_PLAYER_NAME_MAXLEN] = '\0';

    tok = strtok_r(NULL, ",", &saveptr);
    if (tok == NULL) { return false; }
    out->lives = (uint8_t)strtoul(tok, NULL, 10);

    tok = strtok_r(NULL, ",", &saveptr);
    if (tok == NULL) { return false; }
    out->ammo = (uint16_t)strtoul(tok, NULL, 10);

    tok = strtok_r(NULL, ",", &saveptr);
    if (tok == NULL) { return false; }
    out->tiempo = (uint32_t)strtoul(tok, NULL, 10);

    tok = strtok_r(NULL, ",", &saveptr);
    if (tok == NULL) { return false; }
    strncpy(out->mac, tok, EX_MAC_MAXLEN);
    out->mac[EX_MAC_MAXLEN] = '\0';

    return true;
}

/* ========================  CONSTANTS  ==================================== */

#define BT_SPLASH_MS        3000U
#define BT_LED_BLINK_MS     700U
#define BT_MAC_TIMEOUT_MS   20000U /**< Espera del $1<MAC>\r tras el $OK\r
                                        (handshake BLE + GATT puede tardar) */

/**
 * @brief  Imprime en hex + texto todo lo que se haya capturado en
 *         raw_debug, sin filtrar (para diagnostico).
 */
static void bt_log_raw(void)
{
    if (Bluetooth.raw_debug_count == 0U) {
        Log_Print("BT-RAW", "0 bytes capturados");
        return;
    }

    char hex[BT_RAW_DEBUG_LEN * 3U + 1U];
    char txt[BT_RAW_DEBUG_LEN + 1U];

    uint16_t total = Bluetooth.raw_debug_count;
    uint16_t n     = (total < BT_RAW_DEBUG_LEN) ? total : BT_RAW_DEBUG_LEN;
    uint16_t start = (total < BT_RAW_DEBUG_LEN) ? 0U : (total % BT_RAW_DEBUG_LEN);

    for (uint16_t i = 0U; i < n; i++) {
        uint8_t b = Bluetooth.raw_debug[(start + i) % BT_RAW_DEBUG_LEN];
        snprintf(&hex[i * 3U], 4U, "%02X ", b);
        txt[i] = ((b >= 32U) && (b < 127U)) ? (char)b : '.';
    }
    txt[n] = '\0';

    Log_Printf("BT-RAW", "%u bytes (total visto: %u): %s", n, total, hex);
    Log_Printf("BT-RAW", "texto: \"%s\"", txt);
}

/* ========================  DRAW  ========================================= */

void Screen_Bluetooth_Draw(Menu_Handle_t *h)
{
    switch ((BtSubState_e)h->sub_state) {

        case BT_MAIN:
            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);

            if (s_bt_linked) {
                char line[MENU_MAX_CHARS + 1U];
                snprintf(line, sizeof(line), "Id:%.7s", g_exercise_data.mac);
                ssd1306_setCursor(0, 0);
                ssd1306_print(line, &Font5x7);

                int16_t y = 16;
                ssd1306_setCursor(0, y);
                ssd1306_print("> Salir", &Font5x7);
            } else {
                const char *opts[] = { "Anunciar", "Salir" };
                for (uint8_t i = 0U; i < 2U; i++) {
                    int16_t y = (int16_t)(8 + i * MENU_LINE_H);
                    ssd1306_setCursor(0, y);
                    ssd1306_print((i == h->selected) ? "> " : "  ", &Font5x7);
                    ssd1306_print(opts[i], &Font5x7);
                }
            }
            ssd1306_display();
            break;

        case BT_INICIANDO:
            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("Anunciando", 6, &Font5x7);
            ssd1306_printCentered(". . .",     18, &Font5x7);
            ssd1306_display();

            Bt_ResetRawDebug(&Bluetooth);
            Bt_SendAdvertise(&Bluetooth);
            h->sub_state   = (uint8_t)BT_BUSCANDO;
            h->splash_tick = HAL_GetTick();
            s_led_tick      = HAL_GetTick();
            s_led_on        = false;
            LedRGB_Off();
            h->needs_redraw = true;
            break;

        case BT_BUSCANDO:
            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("Buscando", 6, &Font5x7);
            ssd1306_printCentered(". . .",   18, &Font5x7);
            ssd1306_display();

            /* Parpadeo azul cada 1s mientras se espera */
            if ((HAL_GetTick() - s_led_tick) >= BT_LED_BLINK_MS) {
                s_led_tick = HAL_GetTick();
                s_led_on   = !s_led_on;
                LedRGB_SetColor(s_led_on ? RGB_BLUE : RGB_OFF);
            }

            if (Bluetooth.rx_ready) {
                Log_Printf("BT", "Recibido (BUSCANDO): \"%s\"", (char *)Bluetooth.rx_buffer);
                bt_log_raw();
                if (Bluetooth.rx_count == 2U &&
                    Bluetooth.rx_buffer[0] == 'O' && Bluetooth.rx_buffer[1] == 'K') {
                    /* $OK\r -- alguien se conecto. LED se queda fijo en azul
                     * mientras esperamos la MAC (ya no parpadea). */
                    Bt_ResetRx(&Bluetooth);
                    Bt_ResetRawDebug(&Bluetooth);
                    LedRGB_SetColor(RGB_BLUE);
                    h->sub_state   = (uint8_t)BT_ESPERANDO;
                    h->splash_tick = HAL_GetTick();
                } else {
                    /* $NoCON\r u otra cosa inesperada */
                    Bt_ResetRx(&Bluetooth);
                    LedRGB_Off();
                    h->sub_state   = (uint8_t)BT_SPLASH_ERR;
                    h->splash_tick = HAL_GetTick();
                }
            } else if ((HAL_GetTick() - h->splash_tick) >= BT_ADVERTISE_TIMEOUT_MS) {
                Log_Print("BT", "Timeout (BUSCANDO): no llego nada");
                bt_log_raw();
                LedRGB_Off();
                h->sub_state   = (uint8_t)BT_SPLASH_ERR;
                h->splash_tick = HAL_GetTick();
            }
            h->needs_redraw = true;
            break;

        case BT_ESPERANDO:
            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("Enlazando", 6, &Font5x7);
            ssd1306_printCentered(". . .",    18, &Font5x7);
            ssd1306_display();

            /* LED se queda fijo en azul (ya no parpadea) desde que llego el $OK\r */

            if (Bluetooth.rx_ready) {
                Log_Printf("BT", "Recibido (ESPERANDO): \"%s\"", (char *)Bluetooth.rx_buffer);
                bt_log_raw();
                if (Bluetooth.rx_count > 0U && Bluetooth.rx_buffer[0] == '*' &&
                    bt_parse_exercise_data((char *)&Bluetooth.rx_buffer[1], &g_exercise_data)) {
                    Inicializacion_PrintExerciseData();

                    static const uint8_t ack[] = "$ACKOK\r";
                    Bt_Transmit(&Bluetooth, ack, sizeof(ack) - 1U);

                    LedRGB_Off();
                    s_bt_linked     = true;
                    h->bt_connected = true;
                    h->sub_state    = (uint8_t)BT_SPLASH_OK;
                } else {
                    Log_Print("BT", "Datos de ejercicio invalidos");
                    LedRGB_Off();
                    h->sub_state = (uint8_t)BT_SPLASH_ERR;
                }
                Bt_ResetRx(&Bluetooth);
                h->splash_tick = HAL_GetTick();
            } else if ((HAL_GetTick() - h->splash_tick) >= BT_MAC_TIMEOUT_MS) {
                Log_Print("BT", "Timeout (ESPERANDO MAC): no llego nada");
                bt_log_raw();
                LedRGB_Off();
                h->sub_state   = (uint8_t)BT_SPLASH_ERR;
                h->splash_tick = HAL_GetTick();
            }
            h->needs_redraw = true;
            break;

        case BT_SPLASH_OK:
            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("Conectado", 12, &Font5x7);
            ssd1306_display();

            if ((HAL_GetTick() - h->splash_tick) >= BT_SPLASH_MS) {
                h->sub_state = (uint8_t)BT_MAIN;
                h->selected  = 0U;
            }
            h->needs_redraw = true;
            break;

        case BT_SPLASH_ERR:
            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("Falla de",  6, &Font5x7);
            ssd1306_printCentered("conexion", 18, &Font5x7);
            ssd1306_display();

            if ((HAL_GetTick() - h->splash_tick) >= BT_SPLASH_MS) {
                s_bt_linked     = false;
                h->bt_connected = false;
                h->sub_state    = (uint8_t)BT_MAIN;
                h->selected     = 0U;
            }
            h->needs_redraw = true;
            break;

        default:
            break;
    }
}

/* ========================  INPUT  ======================================== */

void Screen_Bluetooth_OnButton(Menu_Handle_t *h, MenuButton_e btn)
{
    if (h->sub_state != (uint8_t)BT_MAIN) {
        return;
    }

    if (s_bt_linked) {
        /* Only option is Salir */
        if (btn == BTN_ENTER) {
            Menu_GoTo(h, SCREEN_MAIN_MENU);
        }
        return;
    }

    /* Disconnected: Anunciar / Salir */
    if (btn == BTN_NAVIGATE) {
        h->selected = (h->selected == 0U) ? 1U : 0U;
        h->needs_redraw = true;

    } else if (btn == BTN_ENTER) {
        if (h->selected == 1U) {
            Menu_GoTo(h, SCREEN_MAIN_MENU);
        } else {
            h->sub_state    = (uint8_t)BT_INICIANDO;
            h->needs_redraw = true;
        }
    }
}
