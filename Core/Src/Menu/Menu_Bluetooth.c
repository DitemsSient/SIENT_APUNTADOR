/**
 * @file    Menu_Bluetooth.c
 * @brief   Bluetooth advertise screen — enters advertising, waits for connection.
 *
 * @details State flow:
 *
 *          BT_MAIN (disconnected)          BT_MAIN (connected)
 *            > Anunciar                      Id: XXXXXX
 *              Salir                         > Salir
 *               │ Enter "Anunciar"
 *               ▼
 *          BT_INICIANDO
 *            Sends "CON\r" via UART
 *            Shows "Anunciando..."
 *               │ auto
 *               ▼
 *          BT_BUSCANDO
 *            Waits for UART response:
 *            "1<ID>\r" = connected
 *            "0\r"     = failed
 *          ┌────┴────┐
 *          ▼         ▼
 *     BT_SPLASH_OK  BT_SPLASH_ERR
 *     Conectado     Falla de
 *       (2 s)       conexion (2 s)
 *          │             │
 *          └── BT_MAIN ──┘
 *
 * @date    June 26, 2026
 * @author  César Pérez
 * @version 3.0.0
 */

#include "Menu/Menu_Screens.h"
#include "Bluetooth.h"
#include "Display_Oled/Display_Comands.h"
#include "Display_Oled/Display_Fonts.h"
#include <stdio.h>
#include <string.h>

extern Bt_Handle_t Bluetooth;

/* ========================  STATIC STATE  ================================== */

static bool s_bt_linked = false;
static char s_bt_id_str[BT_RX_BUFFER_SIZE];

void Screen_Bluetooth_ResetLink(void)
{
    s_bt_linked    = false;
    s_bt_id_str[0] = '\0';
}

/* ========================  CONSTANTS  ==================================== */

#define BT_SPLASH_MS        3000U

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
                snprintf(line, sizeof(line), "Id:%.7s", s_bt_id_str);
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

            Bt_SendAdvertise(&Bluetooth);
            h->sub_state   = (uint8_t)BT_BUSCANDO;
            h->splash_tick = HAL_GetTick();
            h->needs_redraw = true;
            break;

        case BT_BUSCANDO:
            ssd1306_clearDisplay();
            ssd1306_setTextSize(1U);
            ssd1306_setTextColor(WHITE);
            ssd1306_printCentered("Buscando", 6, &Font5x7);
            ssd1306_printCentered(". . .",   18, &Font5x7);
            ssd1306_display();

            if (Bluetooth.rx_ready) {
                if (Bluetooth.rx_count == 2U &&
                    Bluetooth.rx_buffer[0] == 'O' && Bluetooth.rx_buffer[1] == 'K') {
                    Bt_ResetRx(&Bluetooth);
                    h->sub_state = (uint8_t)BT_ESPERANDO;
                } else {
                    h->sub_state   = (uint8_t)BT_SPLASH_ERR;
                    h->splash_tick = HAL_GetTick();
                }
            } else if ((HAL_GetTick() - h->splash_tick) >= BT_ADVERTISE_TIMEOUT_MS) {
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

            if (Bluetooth.rx_ready) {
                if (Bluetooth.rx_count > 0U && Bluetooth.rx_buffer[0] == '1') {
                    uint16_t id_len = Bluetooth.rx_count - 1U;
                    if (id_len >= sizeof(s_bt_id_str)) {
                        id_len = sizeof(s_bt_id_str) - 1U;
                    }
                    memcpy(s_bt_id_str, &Bluetooth.rx_buffer[1], id_len);
                    s_bt_id_str[id_len] = '\0';

                    s_bt_linked     = true;
                    h->bt_connected = true;
                    h->sub_state    = (uint8_t)BT_SPLASH_OK;
                } else {
                    h->sub_state = (uint8_t)BT_SPLASH_ERR;
                }
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
