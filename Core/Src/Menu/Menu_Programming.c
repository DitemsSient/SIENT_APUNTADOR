/**
 * @file    Menu_Programming.c
 * @brief   Programming mode screen — switches the mux between MCU and BT paths.
 *
 * @details Layout (64x32, Font5x7):
 *
 *          PROG_MCU / PROG_BT (menu view):
 *            y= 0:  "Mode: MCU" or "Mode: BT"  (centered)
 *            y=16:  [>] Bluetooth  /  [>] MCU
 *            y=24:  [ ] Salir      /  [ ] Salir
 *
 *          PROG_SPLASH_BT / PROG_SPLASH_MCU (timed, 3 s):
 *            y= 8:  "Modo BT" or "Modo MCU"  (centered)
 *            y=18:  "activo"                  (centered)
 *
 * @date    June 01, 2026
 * @author  César Pérez
 * @version 2.0.0
 */

#include "Menu/Menu_Screens.h"
#include "ModoProgramacion.h"
#include "Logger.h"
#include "Bluetooth.h"
#include "Display_Oled/Display_Comands.h"
#include "Display_Oled/Display_Fonts.h"
#include <string.h>

extern Bt_Handle_t Bluetooth;

/* ========================  CONSTANTS  ==================================== */

#define PROG_SPLASH_MS      3000U

/* Option indices — el boton RUN solo existe en modo MCU */
#define PROG_OPT_SWITCH     0U   /* Bluetooth (from MCU) or MCU (from BT) */
#define PROG_OPT_RUN        1U   /* Solo en PROG_MCU                      */
#define PROG_OPT_SALIR_MCU  2U
#define PROG_OPT_COUNT_MCU  3U

#define PROG_OPT_SALIR_BT   1U
#define PROG_OPT_COUNT_BT   2U

/* Comando que hace que el BL654 corra su programa cargado ("Apuntador"),
 * ya que todavia no tenemos el autorun configurado en el modulo. */
#define PROG_CMD_RUN        "AT+RUN \"Apuntador\"\r\n"

/* ========================  STATIC FUNCTIONS  ============================== */

/**
 * @brief  Draws the menu view for MCU or BT mode.
 * @param  header    Top label ("Mode: MCU" or "Mode: BT").
 * @param  labels    Array of option labels to show.
 * @param  count     Number of options in labels.
 * @param  selected  Currently highlighted option index.
 */
static void prog_draw_menu(const char *header, const char *const *labels,
                            uint8_t count, uint8_t selected)
{
    ssd1306_clearDisplay();
    ssd1306_setTextSize(1U);
    ssd1306_setTextColor(WHITE);

    ssd1306_printCentered(header, 0, &Font5x7);

    /* Lista pegada justo debajo del header -- con 3 opciones (modo MCU)
     * empezar en y=16 sacaba la ultima ("Salir") fuera de la pantalla
     * (MENU_SCREEN_H=32). */
    for (uint8_t i = 0U; i < count; i++) {
        int16_t y = (int16_t)(MENU_LINE_H + i * MENU_LINE_H);
        ssd1306_setCursor(0, y);
        ssd1306_print((i == selected) ? "> " : "  ", &Font5x7);
        ssd1306_print(labels[i], &Font5x7);
    }

    ssd1306_display();
}

/**
 * @brief  Manda PROG_CMD_RUN al BL654 y muestra "Cod BLE Corriendo" 2s.
 * @note   El comando hace que el modulo corra el programa "Apuntador" ya
 *         cargado, sin necesidad de JTAG (todavia no hay autorun). El
 *         modulo no regresa respuesta a este comando (confirmado en
 *         pruebas), asi que no se espera nada por UART.
 */
static void prog_send_run(void)
{
    static const uint8_t cmd[] = PROG_CMD_RUN;

    Bt_ResetRx(&Bluetooth);
    Bt_Transmit(&Bluetooth, cmd, sizeof(cmd) - 1U);

    ssd1306_clearDisplay();
    ssd1306_setTextSize(1U);
    ssd1306_setTextColor(WHITE);
    ssd1306_printCentered("Cod BLE",   10, &Font5x7);
    ssd1306_printCentered("Corriendo", 20, &Font5x7);
    ssd1306_display();
    HAL_Delay(2000U);
}

/**
 * @brief  Draws the timed splash screen.
 * @param  line1  First centered line (e.g. "Modo BT").
 */
static void prog_draw_splash(const char *line1)
{
    ssd1306_clearDisplay();
    ssd1306_setTextSize(1U);
    ssd1306_setTextColor(WHITE);
    ssd1306_printCentered(line1, 8, &Font5x7);
    ssd1306_printCentered("activo", 18, &Font5x7);
    ssd1306_display();
}

/* ========================  DRAW  ========================================= */

void Screen_Programming_Draw(Menu_Handle_t *h)
{
    switch ((ProgSubState_e)h->sub_state) {

        case PROG_MCU: {
            const char *labels[PROG_OPT_COUNT_MCU] = { "Bluetoot", "RUN", "Salir" };
            prog_draw_menu("Mode: MCU", labels, PROG_OPT_COUNT_MCU, h->selected);
            break;
        }

        case PROG_BT: {
            const char *labels[PROG_OPT_COUNT_BT] = { "MCU", "Salir" };
            prog_draw_menu("Mode: BT", labels, PROG_OPT_COUNT_BT, h->selected);
            break;
        }

        case PROG_SPLASH_BT:
            prog_draw_splash("Modo BT");
            if ((HAL_GetTick() - h->splash_tick) >= PROG_SPLASH_MS) {
                h->sub_state = (uint8_t)PROG_BT;
                h->selected  = 0U;
            }
            h->needs_redraw = true;   /* keep polling until timer fires */
            break;

        case PROG_SPLASH_MCU:
            prog_draw_splash("Modo MCU");
            if ((HAL_GetTick() - h->splash_tick) >= PROG_SPLASH_MS) {
                h->sub_state = (uint8_t)PROG_MCU;
                h->selected  = 0U;
            }
            h->needs_redraw = true;   /* keep polling until timer fires */
            break;

        default:
            break;
    }
}

/* ========================  INPUT  ======================================== */

void Screen_Programming_OnButton(Menu_Handle_t *h, MenuButton_e btn)
{
    /* Ignore buttons during splash transitions */
    if (h->sub_state == (uint8_t)PROG_SPLASH_BT ||
        h->sub_state == (uint8_t)PROG_SPLASH_MCU) {
        return;
    }

    bool is_mcu = (h->sub_state == (uint8_t)PROG_MCU);
    uint8_t opt_count = is_mcu ? PROG_OPT_COUNT_MCU : PROG_OPT_COUNT_BT;
    uint8_t opt_salir = is_mcu ? PROG_OPT_SALIR_MCU : PROG_OPT_SALIR_BT;

    if (btn == BTN_NAVIGATE) {
        h->selected++;
        if (h->selected >= opt_count) {
            h->selected = 0U;
        }
        h->needs_redraw = true;
        return;
    }

    if (btn == BTN_ENTER) {
        if (h->selected == opt_salir) {
            Menu_GoTo(h, SCREEN_MAIN_MENU);
            return;
        }

        if (is_mcu && h->selected == PROG_OPT_RUN) {
            prog_send_run();
            h->needs_redraw = true;
            return;
        }

        /* Switch option selected */
        Log_Print("PROG", "Opcion de cambio de modo seleccionada");
        if (is_mcu) {
            ModoProgramacion_SetBT();
            h->sub_state    = (uint8_t)PROG_SPLASH_BT;
        } else {
            ModoProgramacion_SetMCU();
            h->sub_state    = (uint8_t)PROG_SPLASH_MCU;
        }
        h->selected     = 0U;
        h->splash_tick  = HAL_GetTick();
        h->needs_redraw = true;
    }
}
