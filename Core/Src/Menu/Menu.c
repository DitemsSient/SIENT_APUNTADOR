/**
 * @file    Menu.c
 * @brief   Central menu dispatch, button handling, and main menu screen.
 *
 * @date    April 13, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#include "Menu/Menu.h"
#include "Menu/Menu_Screens.h"
#include "ModoProgramacion.h"
#include "Bluetooth.h"
#include "Logger.h"
#include "Display_Oled/Display_Comands.h"
#include "Display_Oled/Display_Fonts.h"
#include "Display_Oled/Display_Bitmaps.h"
#include <string.h>

/**
 * @brief  Nombre corto de la pantalla, para logging.
 */
static const char *Menu_ScreenName(MenuScreen_e screen) {
    switch (screen) {
        case SCREEN_MAIN_MENU:   return "MAIN_MENU";
        case SCREEN_TEST_HW:     return "TEST_HW";
        case SCREEN_BLUETOOTH:   return "BLUETOOTH";
        case SCREEN_PROGRAMMING: return "PROGRAMMING";
        case SCREEN_EXERCISE:    return "EXERCISE";
        default:                 return "?";
    }
}

extern Bt_Handle_t Bluetooth;

/* ========================  CONSTANTS  ==================================== */

#define MAIN_MENU_COUNT     4U
#define ICON_SIZE           16U

static const char *main_menu_labels[MAIN_MENU_COUNT] = {
    "Test Hw",
    "Bluetooth",
    "Programar",
    "Ejercicio"
};

static const MenuScreen_e main_menu_targets[MAIN_MENU_COUNT] = {
    SCREEN_TEST_HW,
    SCREEN_BLUETOOTH,
    SCREEN_PROGRAMMING,
    SCREEN_EXERCISE
};

static const unsigned char *main_menu_icons[MAIN_MENU_COUNT] = {
    bitmap_Icon_Wrench,
    bitmap_Icon_Bluetooth,
    bitmap_Icon_Key,
    bitmap_Icon_Crosshair
};

/* ========================  MAIN MENU SCREEN  ============================= */

/**
 * @brief  Draws the main menu: icon (16x16) centered top, label below,
 *         with up/down arrows on the sides.
 *
 *         Layout (64x32):
 *         y=0   Icon 16x16 centered (x=24)
 *         y=18  Label centered (Font5x7)
 *         y=26  "v" (Font4x6) right side    (if not last)
 *         Left side: "^" (Font4x6)           (if not first)
 */
void Screen_MainMenu_Draw(Menu_Handle_t *h)
{
    ssd1306_clearDisplay();
    ssd1306_setTextSize(1U);
    ssd1306_setTextColor(WHITE);

    /* Icon centered horizontally */
    int16_t icon_x = (MENU_SCREEN_W - ICON_SIZE) / 2;
    ssd1306_drawBitmap(icon_x, 0, main_menu_icons[h->selected],
                       ICON_SIZE, ICON_SIZE, WHITE);

    /* Label below icon */
    ssd1306_printCentered(main_menu_labels[h->selected], 18, &Font5x7);

    /* Up arrow — left side */
    if (h->selected > 0U) {
        ssd1306_setCursor(0, 0);
        ssd1306_print("<", &Font4x6);
    }

    /* Down arrow — right side */
    if (h->selected < (MAIN_MENU_COUNT - 1U)) {
        ssd1306_setCursor(MENU_SCREEN_W - 5, 0);
        ssd1306_print(">", &Font4x6);
    }

    ssd1306_display();
}

/**
 * @brief  Handles button presses on the main menu.
 */
void Screen_MainMenu_OnButton(Menu_Handle_t *h, MenuButton_e btn)
{
    if (btn == BTN_NAVIGATE) {
        h->selected++;
        if (h->selected >= MAIN_MENU_COUNT) {
            h->selected = 0U;
        }
        h->needs_redraw = true;
    } else if (btn == BTN_ENTER) {
        Menu_GoTo(h, main_menu_targets[h->selected]);
    }
}

/* ========================  PUBLIC API  =================================== */

void Menu_Init(Menu_Handle_t *h)
{
    memset(h, 0, sizeof(Menu_Handle_t));
    h->screen       = SCREEN_MAIN_MENU;
    h->needs_redraw = true;
    Log_Printf("MENU", "Screen: %s", Menu_ScreenName(h->screen));
}

void Menu_GoTo(Menu_Handle_t *h, MenuScreen_e screen)
{
    h->screen       = screen;
    h->selected     = 0U;
    h->sub_state    = 0U;
    h->needs_redraw = true;

    if (screen == SCREEN_EXERCISE) {
        Screen_Exercise_ResetScroll();
    }

    /* Programming enters at the correct mode (MCU or BT) */
    if (screen == SCREEN_PROGRAMMING) {
        h->sub_state = ModoProgramacion_IsBluetoothMode() ? (uint8_t)PROG_BT
                                                          : (uint8_t)PROG_MCU;
    }

    Log_Printf("MENU", "Screen: %s", Menu_ScreenName(screen));
}

void Menu_OnButton(Menu_Handle_t *h, MenuButton_e btn)
{
    if (btn == BTN_NAVIGATE) {
        h->flag_navigate = true;
    } else if (btn == BTN_ENTER) {
        h->flag_enter = true;
    }
}

void Menu_Poll(Menu_Handle_t *h)
{
    uint32_t now = HAL_GetTick();

    if ((now - h->last_btn_tick) < BTN_DEBOUNCE_MS) {
        return;
    }

    if (HAL_GPIO_ReadPin(BTN_NAVIGATE_PORT, BTN_NAVIGATE_PIN) == GPIO_PIN_RESET) {
        h->flag_navigate = true;
        h->last_btn_tick = now;
    }

    if (HAL_GPIO_ReadPin(BTN_ENTER_PORT, BTN_ENTER_PIN) == GPIO_PIN_RESET) {
        h->flag_enter = true;
        h->last_btn_tick = now;
    }
}

void Menu_Update(Menu_Handle_t *h)
{
    /* Check for BT disconnection message ($DSCON\r) from any screen.
     * >= 5 en vez de == 5: tolera ruido/bytes extra al final del frame. */
    if (Bluetooth.rx_ready && Bluetooth.rx_count >= 5U &&
        Bluetooth.rx_buffer[0] == 'D' && Bluetooth.rx_buffer[1] == 'S' &&
        Bluetooth.rx_buffer[2] == 'C' && Bluetooth.rx_buffer[3] == 'O' &&
        Bluetooth.rx_buffer[4] == 'N') {
        Bt_ResetRx(&Bluetooth);
        h->bt_connected = false;
        Screen_Bluetooth_ResetLink();

        ssd1306_clearDisplay();
        ssd1306_setTextSize(1U);
        ssd1306_setTextColor(WHITE);
        ssd1306_printCentered("Bluetooth",    6, &Font5x7);
        ssd1306_printCentered("Desconect.", 18, &Font5x7);
        ssd1306_display();
        HAL_Delay(3000U);

        Menu_GoTo(h, SCREEN_MAIN_MENU);
        return;
    }

    /* Process pending button flags */
    MenuButton_e btn = BTN_NONE;

    if (h->flag_navigate) {
        h->flag_navigate = false;
        btn = BTN_NAVIGATE;
    } else if (h->flag_enter) {
        h->flag_enter = false;
        btn = BTN_ENTER;
    }

    /* Dispatch button to active screen */
    if (btn != BTN_NONE) {
        switch (h->screen) {
            case SCREEN_MAIN_MENU:   Screen_MainMenu_OnButton(h, btn);   break;
            case SCREEN_TEST_HW:     Screen_TestHW_OnButton(h, btn);     break;
            case SCREEN_BLUETOOTH:   Screen_Bluetooth_OnButton(h, btn);  break;
            case SCREEN_PROGRAMMING: Screen_Programming_OnButton(h, btn); break;
            case SCREEN_EXERCISE:    Screen_Exercise_OnButton(h, btn);   break;
        }
    }

    /* Dispatch draw to active screen.
     * Clear needs_redraw BEFORE calling Draw so timed screens can re-set it. */
    if (h->needs_redraw) {
        h->needs_redraw = false;
        switch (h->screen) {
            case SCREEN_MAIN_MENU:   Screen_MainMenu_Draw(h);    break;
            case SCREEN_TEST_HW:     Screen_TestHW_Draw(h);      break;
            case SCREEN_BLUETOOTH:   Screen_Bluetooth_Draw(h);   break;
            case SCREEN_PROGRAMMING: Screen_Programming_Draw(h); break;
            case SCREEN_EXERCISE:    Screen_Exercise_Draw(h);    break;
        }
    }
}
