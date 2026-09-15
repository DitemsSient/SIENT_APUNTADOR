/**
 * @file    Menu_Bluetooth.c
 * @brief   Bluetooth advertise screen — enters advertising, waits for connection.
 *
 * @details Protocolo real del modulo (firmware BL654 propio, confirmado
 *          7-sep-2026 -- $CONF reemplaza al marcador '*', ver Pendientes.md):
 *          - $CON\r          → arranca advertising (sin ack inmediato)
 *          - $NoCON\r        → pasaron 20s sin conexion, hay que reenviar $CON
 *          - $ACKCON\r       → alguien (Sensores) se conecto (antes $OK\r)
 *          - $CONF<datos>\r  → datos de juego separados por coma (ver
 *                              Bt_ParseExerciseData()), "CONF" (de
 *                              "configuracion"). Responde $ACKCONF\r si el
 *                              parseo fue exitoso. Vigilado de forma GLOBAL
 *                              en BluetoothTask (Tareas_Interrupciones.c,
 *                              14-sep-2026), no aqui -- necesario porque
 *                              Sensores puede reenviar el handshake completo
 *                              tras una reconexion BLE espontanea, en
 *                              cualquier pantalla, no solo BT_ESPERANDO.
 *                              Esta pantalla solo escucha el resultado via
 *                              Screen_Bluetooth_SetLinked()/NotifyConfError().
 *          - $<dato>\r       → cualquier dato posterior (fuera de este flujo)
 *          - $DSCON\r        → desconexion en cualquier momento (manejado
 *                              de forma global en BluetoothTask, no aqui)
 *          - $RUN\r          → arranca el modo Ejercicio (manejado de forma
 *                              global en BluetoothTask, no aqui)
 *
 *          Formato de $CONF<datos>\r (separado por comas):
 *          orden,lora,equipo,alias,vidas,balas,tiempo,mac
 *          mac: MAC hex completa (14 digitos) -- se guarda entera en
 *          g_exercise_data.mac, en pantalla (BT_MAIN) solo se muestran los
 *          ultimos 8 digitos.
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
 *            Espera $ACKCON\r (conectado) o $NoCON\r (timeout del modulo)
 *          ┌────┴────┐
 *          ▼         ▼
 *     BT_ESPERANDO  BT_SPLASH_ERR
 *     Espera a que BluetoothTask marque s_bt_linked (via $CONF<datos>\r global)
 *          │
 *          ▼
 *     BT_SPLASH_OK
 *     Conectado (3 s) → BT_MAIN
 *
 *          NOTA (14-sep-2026): s_bt_linked tambien puede volverse true fuera
 *          de este flujo (una reconexion BLE espontanea mientras Mira esta
 *          en cualquier otra pantalla) -- BT_MAIN ya lo refleja solo
 *          (`if (s_bt_linked)`), sin necesidad de pasar por BT_ESPERANDO.
 *
 * @date    June 26, 2026
 * @author  César Pérez
 * @version 7.0.0
 */

#include "Menu/Menu_Screens.h"
#include "Bluetooth.h"
#include "Inicializacion.h"
#include "Secuencias_LED.h"
#include "Logger.h"
#include "Display_Oled/Display_Comands.h"
#include "Display_Oled/Display_Fonts.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern Bt_Handle_t Bluetooth;

/* ========================  STATIC STATE  ================================== */

static volatile bool s_bt_linked = false;
static uint32_t s_led_tick   = 0U;
static bool     s_led_on     = false;

/* Banderas que BluetoothTask (Tareas_Interrupciones.c) usa para avisarle a
 * la pantalla de Bluetooth (si es que esta parada en BT_ESPERANDO) que un
 * $CONF<datos> ya se resolvio -- ver Screen_Bluetooth_SetLinked() /
 * Screen_Bluetooth_NotifyConfError() y su nota en el header. */
static volatile bool s_conf_fallo = false;

void Screen_Bluetooth_ResetLink(void)
{
    s_bt_linked = false;
}

void Screen_Bluetooth_SetLinked(bool linked)
{
    s_bt_linked      = linked;
    hmenu.bt_connected = linked;
}

void Screen_Bluetooth_NotifyConfError(void)
{
    s_conf_fallo = true;
}

/**
 * @brief  Parsea "orden,lora,equipo,alias,vidas,balas,tiempo,mac" en out.
 * @param  payload  Texto a partir de despues de "CONF" (sin el '$' ni el '\r').
 * @return true si los 8 campos se parsearon correctamente.
 */
bool Bt_ParseExerciseData(const char *payload, ExerciseGameData_t *out)
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
#define BT_LED_BLINK_MS     500U
#define BT_MAC_TIMEOUT_MS   20000U /**< Espera del $1<MAC>\r tras el $OK\r
                                        (handshake BLE + GATT puede tardar) */

#define BT_MACQUERY_TIMEOUT_MS   6000U  /**< Espera de $ACKMAC<mac>\r tras $MAC\r */
#define BT_MACQUERY_BUF_LEN        24U  /**< Suficiente para una MAC en texto     */

/* Buffer donde queda el texto de la MAC recibida (o vacio si fallo/timeout). */
static char s_mac_query[BT_MACQUERY_BUF_LEN] = { 0 };

/**
 * @brief  Manda "$MAC\r" y resetea el rx -- arranca la consulta.
 * @note   El modulo responde "$ACKMAC<mac>\r"; se procesa en BT_MAC_ESPERANDO.
 */
static void bt_mac_query_start(void)
{
    static const uint8_t cmd[] = "$MAC\r";

    s_mac_query[0] = '\0';
    Bt_ResetRx(&Bluetooth);
    Bt_Transmit(&Bluetooth, cmd, sizeof(cmd) - 1U);
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
                /* Se guarda la MAC completa (14 digitos), en pantalla solo
                 * caben/importan los ultimos 8. */
                size_t mac_len = strlen(g_exercise_data.mac);
                const char *mac_tail = (mac_len > 8U) ?
                    &g_exercise_data.mac[mac_len - 8U] : g_exercise_data.mac;

                char line[12U]; /* "Id:" + 8 digitos + NUL */
                snprintf(line, sizeof(line), "Id:%.8s", mac_tail);
                ssd1306_setCursor(0, 0);
                ssd1306_print(line, &Font5x7);

                int16_t y = 16;
                ssd1306_setCursor(0, y);
                ssd1306_print("> Salir", &Font5x7);
            } else {
                const char *opts[] = { "Anunciar", "MAC", "Salir" };
                for (uint8_t i = 0U; i < 3U; i++) {
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
            SecuenciasLED_ParpadeoNoBloqueanteReset(&s_led_tick, &s_led_on);
            h->needs_redraw = true;
            break;

        case BT_BUSCANDO:
            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("Buscando", 6, &Font5x7);
            ssd1306_printCentered(". . .",   18, &Font5x7);
            ssd1306_display();

            /* Parpadeo azul mientras se espera */
            SecuenciasLED_ParpadeoNoBloqueanteTick(RGB_BLUE, &s_led_tick, &s_led_on,
                                                    BT_LED_BLINK_MS);

            if (Bluetooth.rx_ready) {
                if (Bluetooth.rx_count >= 6U &&
                    strncmp((char *)Bluetooth.rx_buffer, "ACKCON", 6U) == 0) {
                    /* $ACKCON\r -- alguien se conecto. LED se queda fijo en
                     * azul mientras esperamos la MAC (ya no parpadea). */
                    Log_Print("BT", "ACKCON recibido -- conectado");
                    Bt_ResetRx(&Bluetooth);
                    Bt_ResetRawDebug(&Bluetooth);
                    SecuenciasLED_Fijo(RGB_BLUE);
                    h->sub_state   = (uint8_t)BT_ESPERANDO;
                    h->splash_tick = HAL_GetTick();
                } else {
                    /* $NoCON\r u otra cosa inesperada */
                    Bt_ResetRx(&Bluetooth);
                    SecuenciasLED_Apagar();
                    h->sub_state   = (uint8_t)BT_SPLASH_ERR;
                    h->splash_tick = HAL_GetTick();
                }
            } else if ((HAL_GetTick() - h->splash_tick) >= BT_ADVERTISE_TIMEOUT_MS) {
                SecuenciasLED_Apagar();
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

            /* LED se queda fijo en azul (ya no parpadea) desde que llego el $OK\r.
             * El $CONF<datos> en si YA NO se parsea aqui -- BluetoothTask lo
             * vigila de forma global (Tareas_Interrupciones.c) para que una
             * reconexion espontanea funcione sin importar la pantalla. Esta
             * pantalla solo espera a que s_bt_linked/s_conf_fallo cambien. */
            if (s_bt_linked) {
                SecuenciasLED_Apagar();
                h->sub_state    = (uint8_t)BT_SPLASH_OK;
                h->splash_tick  = HAL_GetTick();
            } else if (s_conf_fallo) {
                s_conf_fallo = false;
                SecuenciasLED_Apagar();
                h->sub_state   = (uint8_t)BT_SPLASH_ERR;
                h->splash_tick = HAL_GetTick();
            } else if ((HAL_GetTick() - h->splash_tick) >= BT_MAC_TIMEOUT_MS) {
                SecuenciasLED_Apagar();
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

        case BT_MAC_ESPERANDO:
            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("Consultando", 6, &Font5x7);
            ssd1306_printCentered("MAC . . .", 18, &Font5x7);
            ssd1306_display();

            if (Bluetooth.rx_ready) {
                if (Bluetooth.rx_count >= 6U &&
                    strncmp((char *)Bluetooth.rx_buffer, "ACKMAC", 6U) == 0) {

                    uint16_t len = (uint16_t)(Bluetooth.rx_count - 6U);
                    if (len >= sizeof(s_mac_query)) {
                        len = (uint16_t)(sizeof(s_mac_query) - 1U);
                    }
                    memcpy(s_mac_query, &Bluetooth.rx_buffer[6], len);
                    s_mac_query[len] = '\0';
                    Log_Printf("BT", "ACKMAC recibido -- mac=%s", s_mac_query);
                } else {
                    Log_Print("BT", "Respuesta inesperada esperando ACKMAC");
                }
                Bt_ResetRx(&Bluetooth);
                h->sub_state    = (uint8_t)BT_MAC_MOSTRAR;
                h->needs_redraw = true;
            } else if ((HAL_GetTick() - h->splash_tick) >= BT_MACQUERY_TIMEOUT_MS) {
                Log_Print("BT", "Timeout esperando ACKMAC");
                h->sub_state    = (uint8_t)BT_MAC_MOSTRAR;
            }
            h->needs_redraw = true;
            break;

        case BT_MAC_MOSTRAR:
            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);

            ssd1306_setCursor(0, 6);
            ssd1306_print(s_mac_query[0] != '\0' ? s_mac_query : "Sin respuesta", &Font5x7);

            ssd1306_setCursor(0, 25);
            ssd1306_print("> Salir", &Font4x6);
            ssd1306_display();
            break;

        default:
            break;
    }
}

/* ========================  INPUT  ======================================== */

void Screen_Bluetooth_OnButton(Menu_Handle_t *h, MenuButton_e btn)
{
    if (h->sub_state == (uint8_t)BT_MAC_MOSTRAR) {
        /* Unica opcion: Salir -> de vuelta a BT_MAIN */
        if (btn == BTN_ENTER) {
            h->sub_state    = (uint8_t)BT_MAIN;
            h->selected     = 0U;
            h->needs_redraw = true;
        }
        return;
    }

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

    /* Disconnected: Anunciar / MAC / Salir */
    if (btn == BTN_NAVIGATE) {
        h->selected++;
        if (h->selected >= 3U) {
            h->selected = 0U;
        }
        h->needs_redraw = true;

    } else if (btn == BTN_ENTER) {
        if (h->selected == 2U) {
            Menu_GoTo(h, SCREEN_MAIN_MENU);
        } else if (h->selected == 1U) {
            bt_mac_query_start();
            h->sub_state    = (uint8_t)BT_MAC_ESPERANDO;
            h->splash_tick  = HAL_GetTick();
            h->needs_redraw = true;
        } else {
            h->sub_state    = (uint8_t)BT_INICIANDO;
            h->needs_redraw = true;
        }
    }
}
