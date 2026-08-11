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
#include "Display_Oled/Display_Comands.h"
#include "Display_Oled/Display_Fonts.h"

/* ========================  CONSTANTS  ==================================== */

#define PROG_SPLASH_MS      3000U

/* Option indices — same for both modes (only label changes) */
#define PROG_OPT_SWITCH     0U   /* Bluetooth (from MCU) or MCU (from BT) */
#define PROG_OPT_SALIR      1U
#define PROG_OPT_COUNT      2U

/* ========================  STATIC FUNCTIONS  ============================== */

/**
 * @brief  Draws the menu view for MCU or BT mode.
 * @param  header    Top label ("Mode: MCU" or "Mode: BT").
 * @param  opt_label Label for the switch option ("Bluetooth" or "MCU").
 * @param  selected  Currently highlighted option index.
 */
static void prog_draw_menu(const char *header, const char *opt_label,
                            uint8_t selected)
{
    ssd1306_clearDisplay();
    ssd1306_setTextSize(1U);
    ssd1306_setTextColor(WHITE);

    ssd1306_printCentered(header, 0, &Font5x7);

    const char *labels[PROG_OPT_COUNT] = { opt_label, "Salir" };
    for (uint8_t i = 0U; i < PROG_OPT_COUNT; i++) {
        int16_t y = (int16_t)(16 + i * MENU_LINE_H);
        ssd1306_setCursor(0, y);
        ssd1306_print((i == selected) ? "> " : "  ", &Font5x7);
        ssd1306_print(labels[i], &Font5x7);
    }

    ssd1306_display();
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

        case PROG_MCU:
            prog_draw_menu("Mode: MCU", "Bluetoot", h->selected);
            break;

        case PROG_BT:
            prog_draw_menu("Mode: BT", "MCU", h->selected);
            break;

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

    if (btn == BTN_NAVIGATE) {
        h->selected++;
        if (h->selected >= PROG_OPT_COUNT) {
            h->selected = 0U;
        }
        h->needs_redraw = true;
        return;
    }

    if (btn == BTN_ENTER) {
        if (h->selected == PROG_OPT_SALIR) {
            Menu_GoTo(h, SCREEN_MAIN_MENU);
            return;
        }

        /* Switch option selected */
        if (h->sub_state == (uint8_t)PROG_MCU) {
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
