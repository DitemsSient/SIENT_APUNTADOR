/**
 * @file    Menu_Exercise.c
 * @brief   Exercise screen — BT status, preview, calibration mode.
 *
 * @details Flow:
 *          - EX_NO_BT: header "Bt Cnet" / "Bt NO Cnet" + Preview / Md Calibrar / Salir
 *          - EX_PREVIEW: no bloqueante, 2 vueltas balas/vidas (3s) + equipo/jugador (3s) → EX_NO_BT
 *          - EX_CALIBRAR: "Calibra tu / arma" with lines, Salir option → EX_NO_BT
 *          - EX_WAITING: bitmap + "Esperando..." (awaits LoRa)
 *          - EX_ACTIVE: static HUD with lives/ammo
 *
 * @date    June 26, 2026
 * @author  César Pérez
 * @version 2.0.0
 */

#include "Menu/Menu_Screens.h"
#include "Display_Oled/Display_Comands.h"
#include "Display_Oled/Display_Fonts.h"
#include "Display_Oled/Display_Bitmaps.h"
#include "Transmsion_Laser_IR.h"
#include "Inicializacion.h"
#include <stdio.h>
#include <ctype.h>

extern bool laser_calibration_mode;

/* ========================  CONSTANTS  ====================================== */

#define EX_MENU_OPTS        3U

static const char *ex_opts[EX_MENU_OPTS] = {
    "Preview",
    "Calibrar",
    "Salir"
};

static uint8_t ex_offset = 0U;

#define EX_VISIBLE_ITEMS    3U

void Screen_Exercise_ResetScroll(void) { ex_offset = 0U; }

/* ========================  ACTIVE HUD GEOMETRY (page 1)  =================== */

#define HUDACT_COL_W    (MENU_SCREEN_W / 3U)
#define HUDACT_COL1_X   0
#define HUDACT_COL2_X   HUDACT_COL_W
#define HUDACT_COL3_X   (HUDACT_COL_W * 2U)
#define HUDACT_COL3_W   (MENU_SCREEN_W - HUDACT_COL3_X)

static void HUD_DrawBigNumber(int16_t col_x, int16_t col_w, int16_t x_offset, uint16_t value)
{
    char buf[5];
    snprintf(buf, sizeof(buf), "%u", (value > 999U) ? 999U : value);

    uint16_t tw = ssd1306_getStringWidth(buf, &Font5x7);
    int16_t  x  = col_x + (col_w - (int16_t)tw) / 2 + x_offset;
    int16_t  y  = (int16_t)MENU_SCREEN_H - (int16_t)Font5x7.height;

    ssd1306_setCursor(x, y);
    ssd1306_print(buf, &Font5x7);
}

/* ========================  TEAM/PLAYER SCREEN (page 2)  ==================== */

#define EX_AVATAR_W   8U
#define EX_AVATAR_H   8U

static void HUD_CopyUpper(char *dst, const char *src, size_t maxlen)
{
    size_t i = 0U;
    for (; (src[i] != '\0') && (i < maxlen); i++) {
        dst[i] = (char)toupper((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

static void HUD_DrawTeamPage(void)
{
    uint16_t name_w  = ssd1306_getStringWidth(g_exercise_data.player_name, &Font5x7);
    uint16_t total_w = EX_AVATAR_W + 1U + name_w;
    int16_t  x       = ((int16_t)MENU_SCREEN_W - (int16_t)total_w) / 2;
    if (x < 0) x = 0;

    ssd1306_drawBitmap(x, 0, bitmap_Icon_User, (int16_t)EX_AVATAR_W, (int16_t)EX_AVATAR_H, WHITE);

    ssd1306_setCursor(x + (int16_t)EX_AVATAR_W + 1, ((int16_t)EX_AVATAR_H - (int16_t)Font5x7.height) / 2);
    ssd1306_print(g_exercise_data.player_name, &Font5x7);

    char team_upper[EX_TEAM_NAME_MAXLEN + 1U];
    HUD_CopyUpper(team_upper, g_exercise_data.team_name, EX_TEAM_NAME_MAXLEN);

    uint16_t team_w = ssd1306_getStringWidth(team_upper, &Font6x8);
    int16_t  team_x = ((int16_t)MENU_SCREEN_W - (int16_t)team_w) / 2;
    if (team_x < 0) team_x = 0;
    int16_t team_y    = (int16_t)MENU_SCREEN_H - (int16_t)Font6x8.height - 3;
    int16_t line_top  = team_y - 3;
    int16_t line_bot  = team_y + (int16_t)Font6x8.height + 2;
    int16_t line_x2   = team_x + (int16_t)team_w - 2;

    ssd1306_setCursor(team_x, team_y);
    ssd1306_print(team_upper, &Font6x8);

    ssd1306_drawLine(team_x, line_top, line_x2, line_top, WHITE);
    ssd1306_drawLine(team_x, line_bot, line_x2, line_bot, WHITE);
}

/* ========================  BATTERY BAR  ==================================== */

#define BAT_BAR_X   55
#define BAT_BAR_Y   2
#define BAT_BAR_W   4
#define BAT_BAR_H   16

/**
 * @brief  Cuantos renglones (de BAT_BAR_H) corresponden a un porcentaje 0-100.
 *         Cada renglon vale 100/BAT_BAR_H % (100/16 = 6.25% c/u), redondeado
 *         al mas cercano.
 */
static uint8_t HUD_BateryFilledRows(uint8_t pct)
{
    if (pct > 100U) { pct = 100U; }
    uint16_t filled = ((uint16_t)pct * BAT_BAR_H + 50U) / 100U;
    return (filled > BAT_BAR_H) ? (uint8_t)BAT_BAR_H : (uint8_t)filled;
}

/**
 * @brief  Dibuja la barra de bateria de un solo golpe (sin animar, sin
 *         llamar ssd1306_display()), llena desde abajo segun el porcentaje.
 * @note   Pensada para el refresco periodico de Exercise_DrawStatsPage().
 *         Al bajar el porcentaje se vacia de abajo hacia arriba (la carga
 *         restante se queda arriba, no abajo).
 */
static void HUD_DrawBateryLevel(uint8_t pct)
{
    uint8_t filled = HUD_BateryFilledRows(pct);
    for (uint8_t row = 0U; row < (uint8_t)BAT_BAR_H; row++) {
        bool encendido = row < filled;
        ssd1306_drawLine(BAT_BAR_X, BAT_BAR_Y + (int16_t)row,
                         BAT_BAR_X + BAT_BAR_W - 1, BAT_BAR_Y + (int16_t)row,
                         encendido ? WHITE : BLACK);
    }
}

/* COMENTADA -- animacion de vaciado/llenado de la pila. Ya no se usa en
 * EX_PREVIEW (ahora se muestra el valor real via HUD_DrawBateryLevel(),
 * sin animar), pero se deja aqui por si se vuelve a necesitar despues.
 *
 * static void HUD_BatDesvanecimiento(uint8_t pct)
 * {
 *     for (int16_t row = BAT_BAR_H - 1; row >= 0; row--) {
 *         ssd1306_drawLine(BAT_BAR_X, BAT_BAR_Y + row,
 *                          BAT_BAR_X + BAT_BAR_W - 1, BAT_BAR_Y + row, BLACK);
 *         ssd1306_display();
 *         HAL_Delay(180U);
 *     }
 *
 *     uint8_t filled = HUD_BateryFilledRows(pct);
 *     for (uint8_t i = 0U; i < filled; i++) {
 *         int16_t row = (int16_t)((uint8_t)BAT_BAR_H - 1U - i);
 *         ssd1306_drawLine(BAT_BAR_X, BAT_BAR_Y + row,
 *                          BAT_BAR_X + BAT_BAR_W - 1, BAT_BAR_Y + row, WHITE);
 *         ssd1306_display();
 *         HAL_Delay(180U);
 *     }
 * }
 */

/* ========================  PAGINAS COMPARTIDAS CON ExerciseTask  =========== */

void Exercise_DrawStatsPage(void)
{
    ssd1306_clearDisplay();
    ssd1306_setTextSize(1U);
    ssd1306_setTextColor(WHITE);

    ssd1306_drawBitmap(0, 0, bitmap_Modo_Ejercicio, MENU_SCREEN_W, MENU_SCREEN_H, WHITE);
    HUD_DrawBigNumber(HUDACT_COL1_X, (int16_t)HUDACT_COL_W, 0, g_exercise_data.ammo);
    HUD_DrawBigNumber(HUDACT_COL2_X, (int16_t)HUDACT_COL_W, 4, g_exercise_data.lives);
    HUD_DrawBateryLevel(g_exercise_data.lvBatery);

    ssd1306_display();
}

void Exercise_DrawTeamPage(void)
{
    ssd1306_clearDisplay();
    ssd1306_setTextSize(1U);
    ssd1306_setTextColor(WHITE);

    HUD_DrawTeamPage();

    ssd1306_display();
}

#define EX_PREVIEW_STEP_MS   3000U

/* ========================  DRAW  =========================================== */

void Screen_Exercise_Draw(Menu_Handle_t *h)
{
    /* EX_PREVIEW no bloqueante: mientras esperamos el timer de cada paso
     * no hay nada nuevo que dibujar. Sin este guard, el clear+display
     * incondicionales de abajo (pensados para el resto de los sub-estados,
     * que si dibujan todo en cada llamada) mandarian un buffer vacio a la
     * pantalla cada 20ms -- destello negro repetido, confirmado en pruebas. */
    if ((ExSubState_e)h->sub_state == EX_PREVIEW && h->selected != 0U &&
        (HAL_GetTick() - h->splash_tick) < EX_PREVIEW_STEP_MS) {
        h->needs_redraw = true;
        return;
    }

    ssd1306_clearDisplay();
    ssd1306_setTextSize(1U);
    ssd1306_setTextColor(WHITE);

    switch ((ExSubState_e)h->sub_state) {

    case EX_NO_BT: {
        ssd1306_setCursor(0, 0);
        ssd1306_print(h->bt_connected ? "Bt Cnet" : "Bt NO Cnt", &Font5x7);

        uint8_t end = ex_offset + EX_VISIBLE_ITEMS - 1U;
        if (end > EX_MENU_OPTS) { end = EX_MENU_OPTS; }

        for (uint8_t i = ex_offset; i < end; i++) {
            int16_t y = (int16_t)((i - ex_offset + 1U) * MENU_LINE_H);
            ssd1306_setCursor(0, y);
            ssd1306_print((i == h->selected) ? "> " : "  ", &Font5x7);
            ssd1306_print(ex_opts[i], &Font5x7);
        }

        break;
    }

    case EX_PREVIEW: {
        /* No bloqueante -- si MenuTask se suspende a medio ciclo (llega un
         * $RUN mientras estamos aqui), al reanudar cae limpio en el menu
         * principal (ya lo dejo puesto Menu_GoTo en ExerciseTask) en vez
         * de terminar una llamada bloqueada colgante de un ciclo viejo.
         * 2 vueltas de balas/vidas (3s) -> equipo/jugador (3s), y sale. */
        if (h->selected == 0U) {
            Inicializacion_PrintExerciseData();

            Exercise_DrawStatsPage();
            /* Animacion de vaciado/llenado de la pila -- COMENTADA. Ya no
             * hace falta, Exercise_DrawStatsPage() ya pinta el valor real
             * (g_exercise_data.lvBatery) via HUD_DrawBateryLevel(). Se deja
             * aqui por si se vuelve a necesitar mas adelante.
             * HUD_BatDesvanecimiento(g_exercise_data.lvBatery); */

            h->splash_tick = HAL_GetTick();
            h->selected    = 1U;

        } else if ((HAL_GetTick() - h->splash_tick) >= EX_PREVIEW_STEP_MS) {
            h->splash_tick = HAL_GetTick();
            h->selected++;

            switch (h->selected) {
                case 2U: Exercise_DrawTeamPage();  break;  /* vuelta 1: equipo  */
                case 3U: Exercise_DrawStatsPage(); break;  /* vuelta 2: balas   */
                case 4U: Exercise_DrawTeamPage();  break;  /* vuelta 2: equipo  */
                default:
                    h->sub_state = (uint8_t)EX_NO_BT;
                    h->selected  = 0U;
                    break;
            }
        }

        h->needs_redraw = true;
        break;
    }

    case EX_CALIBRAR: {
        const char *txt1 = "Calibrar";
        const char *txt2 = "Arma";

        uint16_t w1 = ssd1306_getStringWidth(txt1, &Font5x7);
        uint16_t w2 = ssd1306_getStringWidth(txt2, &Font5x7);
        uint16_t max_w = (w1 > w2) ? w1 : w2;

        int16_t lx1 = ((int16_t)MENU_SCREEN_W - (int16_t)max_w) / 2;
        int16_t lx2 = lx1 + (int16_t)max_w;

        int16_t y1 = 4;
        int16_t y2 = y1 + (int16_t)Font5x7.height + 1;

        int16_t line_top = y1 - 2;
        int16_t line_bot = y2 + (int16_t)Font5x7.height + 1;

        ssd1306_drawLine(lx1, line_top, lx2, line_top, WHITE);

        ssd1306_setCursor(((int16_t)MENU_SCREEN_W - (int16_t)w1) / 2, y1);
        ssd1306_print(txt1, &Font5x7);
        ssd1306_setCursor(((int16_t)MENU_SCREEN_W - (int16_t)w2) / 2, y2);
        ssd1306_print(txt2, &Font5x7);

        ssd1306_drawLine(lx1, line_bot, lx2, line_bot, WHITE);

        ssd1306_setCursor(0, 24);
        ssd1306_print("> Salir", &Font4x6);

        /* El disparo ya no es automatico -- se manda por el gatillo
         * (EXTI, ver HAL_GPIO_EXTI_Callback en Transmision_Laser_IR.c),
         * que solo dispara mientras h->screen == SCREEN_EXERCISE. */
        break;
    }

    case EX_WAITING:
        ssd1306_drawBitmap(0, 0, bitmap_Modo_Ejercicio,
                           MENU_SCREEN_W, MENU_SCREEN_H, WHITE);
        ssd1306_fillRect(0, 24, MENU_SCREEN_W, MENU_LINE_H, BLACK);
        ssd1306_printCentered("Esperando..", 24, &Font5x7);
        break;

    case EX_ACTIVE:
        HUD_DrawBigNumber(HUDACT_COL1_X, (int16_t)HUDACT_COL_W, 0, g_exercise_data.ammo);
        HUD_DrawBigNumber(HUDACT_COL2_X, (int16_t)HUDACT_COL_W, 4, g_exercise_data.lives);
        break;
    }

    ssd1306_display();
}

/* ========================  INPUT  ========================================== */

void Screen_Exercise_OnButton(Menu_Handle_t *h, MenuButton_e btn)
{
    switch ((ExSubState_e)h->sub_state) {

    case EX_NO_BT:
        if (btn == BTN_NAVIGATE) {
            h->selected++;
            if (h->selected >= EX_MENU_OPTS) {
                h->selected = 0U;
                ex_offset   = 0U;
            } else if (h->selected >= ex_offset + EX_VISIBLE_ITEMS - 1U) {
                ex_offset++;
            }
            h->needs_redraw = true;

        } else if (btn == BTN_ENTER) {
            switch (h->selected) {
                case 0U:    /* Preview */
                    h->sub_state    = (uint8_t)EX_PREVIEW;
                    h->selected     = 0U;
                    h->needs_redraw = true;
                    break;
                case 1U:    /* Md Calibrar */
                    laser_calibration_mode = true;
                    h->sub_state    = (uint8_t)EX_CALIBRAR;
                    h->needs_redraw = true;
                    break;
                case 2U:    /* Salir */
                    Menu_GoTo(h, SCREEN_MAIN_MENU);
                    break;
                default:
                    break;
            }
        }
        break;

    case EX_PREVIEW:
        h->sub_state    = (uint8_t)EX_NO_BT;
        h->selected     = 0U;
        ex_offset       = 0U;
        h->needs_redraw = true;
        break;

    case EX_CALIBRAR:
        if (btn == BTN_ENTER) {
            laser_calibration_mode = false;
            h->sub_state    = (uint8_t)EX_NO_BT;
            h->selected     = 0U;
            ex_offset       = 0U;
            h->needs_redraw = true;
        }
        break;

    case EX_WAITING:
        if (btn == BTN_ENTER) {
            h->sub_state    = (uint8_t)EX_ACTIVE;
            h->needs_redraw = true;
        } else if (btn == BTN_NAVIGATE) {
            Menu_GoTo(h, SCREEN_MAIN_MENU);
        }
        break;

    case EX_ACTIVE:
        if (btn == BTN_NAVIGATE) {
            Menu_GoTo(h, SCREEN_MAIN_MENU);
        }
        break;
    }
}
