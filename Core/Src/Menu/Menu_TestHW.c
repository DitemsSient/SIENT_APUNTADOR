/**
 * @file    Menu_TestHW.c
 * @brief   Test Hardware screen — menú de dos niveles con tests automáticos y manuales.
 *
 * @details Nivel 1 (TESTHW_MENU): selección de PCB a testear (Mira / Salir).
 *          Nivel 2 (TESTHW_MIRA_MENU): suite de la PCB Mira
 *            (Autotest / Sensores / Buzzer / Vibrador / RGB / Salir).
 *
 *          Tests automáticos (Sensores): secuencia bloqueante con HAL_Delay.
 *            Al finalizar muestra "Completado" 2 s y regresa automáticamente.
 *          Tests manuales (Buzzer 5 s, Vibrador 5 s, RGB 9 s): activan el
 *            dispositivo durante el tiempo indicado y transicionan solos a la
 *            pantalla de confirmación SI / NO.
 *          Al confirmar: muestra "Completado" 2 s y regresa automáticamente.
 *
 *          Autotest: ejecuta sensores + manuales en secuencia bloqueante.
 *            Polling de botones vía Menu_Poll() dentro del loop de confirmación.
 *            Al finalizar muestra "Todo bien" o páginas de módulos fallidos (3/página, 2 s c/u).
 *
 *          Para agregar una nueva PCB: añadir entrada en testhw_l1_labels y
 *            manejar su sub-estado en Draw / OnButton.
 *
 * @date    May 2026
 * @author  César Pérez
 * @version 0.4.0
 */

#include "Menu/Menu_Screens.h"
#include "Logger.h"
#include "Inicializacion.h"
#include "Display_Oled/Display_Comands.h"
#include "Display_Oled/Display_Fonts.h"
#include "Flash.h"
#include "SensorHall.h"
#include "SensorLuz_TSL2571.h"
#include "Bluetooth.h"
#include "Buzzer.h"
#include "LedRGB.h"
#include "HWTest_Status.h"
#include "LSM6DSO32TR.h"
#include "MMC5983MA.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <string.h>

extern HWTest_Status_t hw_status;
extern LSM6DSO32TR_t   Imu;
extern Bt_Handle_t     Bluetooth;

/* ========================  SUB-STATES  =================================== */

#define TESTHW_MENU             0U  /* Nivel 1: Mira / Salir                          */
#define TESTHW_MIRA_MENU        1U  /* Nivel 2: Autotest / Sensores / Buzzer / ...     */
#define TESTHW_AUTO_RUNNING     2U  /* Ejecutando auto-tests de sensores               */
#define TESTHW_MANUAL_ACT       3U  /* Activando dispositivo manual                    */
#define TESTHW_MANUAL_CONFIRM   4U  /* Esperando confirmacion SI / NO                  */
#define TESTHW_COMPLETED        5U  /* "Completado" 2 s — regresa al menú             */
#define TESTHW_AUTOTEST_START   6U  /* Autotest: secuencia bloqueante completa         */
#define TESTHW_SENSORES_NO_BT  7U  /* "Bluetooth / No conectado" splash 3 s           */
#define TESTHW_SENS_ESPERANDO  8U  /* "Esperando..." — sends $TEST\r, waits response  */
#define TESTHW_SENS_SHOWING    9U  /* Showing sensor results one by one                */
#define TESTHW_SENS_MANUAL_ACT 10U /* Activating manual device (BUZ/RGB) via BT        */
#define TESTHW_SENS_MANUAL_CFM 11U /* Waiting user confirm SI/NO                       */
#define TESTHW_SENS_RESULT     12U /* Final report screen                              */

/* ========================  NIVEL 1 — LISTA DE PCBs  ====================== */

#define TESTHW_L1_COUNT     3U

static const char *testhw_l1_labels[TESTHW_L1_COUNT] = {
    "Mira",
    "Sensores",
    "Salir"
};

#define TESTHW_SENSORES_SPLASH_MS  3000U

/* ========================  NIVEL 2 — SUITE MIRA  ========================= */

#define TESTHW_MIRA_COUNT   5U

static const char *testhw_mira_labels[TESTHW_MIRA_COUNT] = {
    "Autotest",
    "Sensores",
    "Buzzer",
    "RGB",
    "Salir"
};

static uint8_t mira_selected = 0U;
static uint8_t mira_offset   = 0U;

#define MENU_VISIBLE_ITEMS  4U

/* ========================  AUTO-TEST TABLE  =============================== */

typedef struct {
    const char *name;
    uint8_t     passed;
} AutoTest_t;

static AutoTest_t sensor_tests[] = {
    { "Flash",     0U },
    { "Luz Amb.",  0U },
    { "Bluetooth", 0U },
    { "Magnet.",   0U },
    { "Girosc.",   0U },
    { "Bateria",   0U },
};

#define SENSOR_TEST_COUNT   ((uint8_t)(sizeof(sensor_tests) / sizeof(sensor_tests[0])))

static uint8_t sensor_pass_count = 0U;

/* ========================  MANUAL TEST STATE  ============================= */

#define MANUAL_BUZZER       0U
#define MANUAL_RGB          1U

static uint8_t manual_id          = 0U;
static uint8_t manual_confirm_sel = 0U;  /* 0 = SI, 1 = NO */

/* ========================  SENSORES (REMOTE VIA BT)  ===================== */

#define SENS_MANUAL_BUZZER  0U
#define SENS_MANUAL_RGB     1U
#define SENS_BUZZER_MS      7000U
#define SENS_TEST_TIMEOUT_MS 15000U

static uint8_t sens_manual_id       = 0U;
static uint8_t sens_confirm_sel     = 0U;

static const char *sens_auto_names[] = { "GPS", "LORA", "FLASH", "IMU" };
#define SENS_AUTO_COUNT  4U

static bool sens_auto_results[SENS_AUTO_COUNT];

#define RESULT_PAGE_ITEMS  3U

/* ========================  HELPERS  ====================================== */

static void show_step(const char *name, const char *status)
{
    ssd1306_clearDisplay();
    ssd1306_setTextSize(1U);
    ssd1306_setTextColor(WHITE);
    ssd1306_printCentered(name,   8,  &Font5x7);
    ssd1306_printCentered(status, 20, &Font5x7);
    ssd1306_display();

    /* Solo resultados, no pasos intermedios ("...", "Sonando...", etc.) */
    if (strstr(status, "...") == NULL) {
        Log_Printf("TESTHW", "%s: %s", name, status);
    }
}

static void draw_list(const char **labels, uint8_t count, uint8_t selected, uint8_t offset)
{
    ssd1306_clearDisplay();
    ssd1306_setTextSize(1U);
    ssd1306_setTextColor(WHITE);

    uint8_t end = offset + MENU_VISIBLE_ITEMS;
    if (end > count) { end = count; }

    for (uint8_t i = offset; i < end; i++) {
        int16_t y = (int16_t)((i - offset) * MENU_LINE_H);
        ssd1306_setCursor(0, y);
        ssd1306_print((i == selected) ? "> " : "  ", &Font5x7);
        ssd1306_print(labels[i], &Font5x7);
    }

    /* Indicadores de scroll */
    if (offset > 0U) {
        ssd1306_setCursor(MENU_SCREEN_W - 5, 0);
        ssd1306_print("^", &Font5x7);
    }
    if (end < count) {
        ssd1306_setCursor(MENU_SCREEN_W - 5, 24);
        ssd1306_print("v", &Font5x7);
    }

    ssd1306_display();
}

/* ========================  SENSORES FUNCTIONS  ============================ */

static void sens_send_cmd(const char *cmd)
{
    Bt_ResetRx(&Bluetooth);
    uint8_t buf[16];
    uint8_t len = 0U;
    buf[len++] = '$';
    while (*cmd) { buf[len++] = (uint8_t)*cmd++; }
    buf[len++] = '\r';
    Bt_Transmit(&Bluetooth, buf, len);
}

static void sens_parse_flags(void)
{
    for (uint8_t i = 0U; i < SENS_AUTO_COUNT; i++) {
        sens_auto_results[i] = (i < Bluetooth.rx_count) && (Bluetooth.rx_buffer[i] == '1');
    }
    hw_status.sensores.gps   = sens_auto_results[0];
    hw_status.sensores.lora  = sens_auto_results[1];
    hw_status.sensores.flash = sens_auto_results[2];
    hw_status.sensores.imu   = sens_auto_results[3];
}

static void sens_show_results_sequence(void)
{
    for (uint8_t i = 0U; i < SENS_AUTO_COUNT; i++) {
        show_step(sens_auto_names[i], "...");
        HAL_Delay(1000U);
        show_step(sens_auto_names[i], sens_auto_results[i] ? "Correct" : "Fail");
        HAL_Delay(1000U);
    }
}

static void sens_draw_confirm(void)
{
    ssd1306_clearDisplay();
    ssd1306_setTextSize(1U);
    ssd1306_setTextColor(WHITE);
    ssd1306_printCentered("Funciono?", 4, &Font5x7);
    ssd1306_setCursor(8,  20);
    ssd1306_print(sens_confirm_sel == 0U ? "> SI" : "  SI", &Font5x7);
    ssd1306_setCursor(36, 20);
    ssd1306_print(sens_confirm_sel == 1U ? "> NO" : "  NO", &Font5x7);
    ssd1306_display();
}

static void sens_show_report(void)
{
    const char *fail_names[6];
    uint8_t     fail_count = 0U;

    if (!hw_status.sensores.gps)    fail_names[fail_count++] = "GPS";
    if (!hw_status.sensores.lora)   fail_names[fail_count++] = "LORA";
    if (!hw_status.sensores.flash)  fail_names[fail_count++] = "Flash";
    if (!hw_status.sensores.imu)    fail_names[fail_count++] = "IMU";
    if (!hw_status.sensores.buzzer) fail_names[fail_count++] = "Buzzer";
    if (!hw_status.sensores.rgb)    fail_names[fail_count++] = "RGB";

    if (fail_count == 0U) {
        Log_Print("TESTHW", "Sensores (BT): todo bien");
        show_step("Todo bien", ":)");
        HAL_Delay(2000U);
        return;
    }
    Log_Printf("TESTHW", "Sensores (BT): %u fallo(s)", fail_count);

    uint8_t pages = (uint8_t)((fail_count + RESULT_PAGE_ITEMS - 1U) / RESULT_PAGE_ITEMS);

    for (uint8_t p = 0U; p < pages; p++) {
        ssd1306_clearDisplay();
        ssd1306_setTextSize(1U);
        ssd1306_setTextColor(WHITE);
        ssd1306_printCentered("Fallo:", 0, &Font5x7);

        uint8_t start = (uint8_t)(p * RESULT_PAGE_ITEMS);
        uint8_t end   = (uint8_t)(start + RESULT_PAGE_ITEMS);
        if (end > fail_count) { end = fail_count; }

        for (uint8_t i = start; i < end; i++) {
            ssd1306_setCursor(0, (int16_t)(10 + (i - start) * 8));
            ssd1306_print("- ", &Font5x7);
            ssd1306_print(fail_names[i], &Font5x7);
        }

        ssd1306_display();
        HAL_Delay(2000U);
    }
}

/* ========================  MIRA CONFIRM  ================================= */

static void draw_confirm(void)
{
    ssd1306_clearDisplay();
    ssd1306_setTextSize(1U);
    ssd1306_setTextColor(WHITE);
    ssd1306_printCentered("Funciono?", 4, &Font5x7);
    ssd1306_setCursor(8,  20);
    ssd1306_print(manual_confirm_sel == 0U ? "> SI" : "  SI", &Font5x7);
    ssd1306_setCursor(36, 20);
    ssd1306_print(manual_confirm_sel == 1U ? "> NO" : "  NO", &Font5x7);
    ssd1306_display();
}

/* ========================  AUTOTEST RESULT  =============================== */

static void show_autotest_result(void)
{
    const char *fail_names[8];
    uint8_t     fail_count = 0U;

    if (!hw_status.mira.flash)          fail_names[fail_count++] = "Flash";
    if (!hw_status.mira.luz_ambiental)  fail_names[fail_count++] = "Luz Amb.";
    if (!hw_status.mira.bluetooth)      fail_names[fail_count++] = "Bluetooth";
    if (!hw_status.mira.magnetometro)   fail_names[fail_count++] = "Magnet.";
    if (!hw_status.mira.giroscopio)     fail_names[fail_count++] = "Girosc.";
    if (!hw_status.mira.batterymonitor) fail_names[fail_count++] = "Bateria";
    if (!hw_status.mira.buzzer)         fail_names[fail_count++] = "Buzzer";
    if (!hw_status.mira.rgb_driver)     fail_names[fail_count++] = "RGB";

    if (fail_count == 0U) {
        Log_Print("TESTHW", "Autotest Mira: todo bien");
        show_step("Todo bien", ":)");
        HAL_Delay(2000U);
        return;
    }
    Log_Printf("TESTHW", "Autotest Mira: %u fallo(s)", fail_count);

    uint8_t pages = (uint8_t)((fail_count + RESULT_PAGE_ITEMS - 1U) / RESULT_PAGE_ITEMS);

    for (uint8_t p = 0U; p < pages; p++) {
        ssd1306_clearDisplay();
        ssd1306_setTextSize(1U);
        ssd1306_setTextColor(WHITE);
        ssd1306_printCentered("Fallo:", 0, &Font5x7);

        uint8_t start = (uint8_t)(p * RESULT_PAGE_ITEMS);
        uint8_t end   = (uint8_t)(start + RESULT_PAGE_ITEMS);
        if (end > fail_count) { end = fail_count; }

        for (uint8_t i = start; i < end; i++) {
            ssd1306_setCursor(0, (int16_t)(10 + (i - start) * 8));
            ssd1306_print("- ", &Font4x6);
            ssd1306_print(fail_names[i], &Font4x6);
        }

        ssd1306_display();
        HAL_Delay(2000U);
    }
}

/* ========================  AUTO-TESTS  =================================== */

static void run_sensors_only(void)
{
    sensor_pass_count = 0U;
    uint8_t ok;

    /* Resultados reales, tomados del diagnostico de Inicializacion_Run()
     * (arranque del sistema) en vez de valores fijos. */

    /* --- Flash MX25L6445E --- */
    show_step(sensor_tests[0].name, "...");
    HAL_Delay(1000U);
    ok = Diagnostico.flash ? 1U : 0U;
    sensor_tests[0].passed = ok;
    hw_status.mira.flash = (ok == 1U);
    if (ok) { sensor_pass_count++; }
    show_step(sensor_tests[0].name, ok ? "Correct" : "Fail");
    HAL_Delay(1000U);

    /* --- Sensor de luz ambiental --- */
    show_step(sensor_tests[1].name, "...");
    HAL_Delay(1000U);
    ok = Diagnostico.sensorluz ? 1U : 0U;
    sensor_tests[1].passed = ok;
    hw_status.mira.luz_ambiental = (ok == 1U);
    if (ok) { sensor_pass_count++; }
    show_step(sensor_tests[1].name, ok ? "Correct" : "Fail");
    HAL_Delay(1000U);

    /* --- Bluetooth BL654 --- */
    show_step(sensor_tests[2].name, "...");
    HAL_Delay(1000U);
    ok = Diagnostico.bluetooth ? 1U : 0U;
    sensor_tests[2].passed = ok;
    hw_status.mira.bluetooth = (ok == 1U);
    if (ok) { sensor_pass_count++; }
    show_step(sensor_tests[2].name, ok ? "Correct" : "Fail");
    HAL_Delay(1000U);

    /* --- Magnetometro MMC5983MA --- */
    show_step(sensor_tests[3].name, "...");
    HAL_Delay(1000U);
    ok = Diagnostico.magnetometro ? 1U : 0U;
    sensor_tests[3].passed = ok;
    hw_status.mira.magnetometro = (ok == 1U);
    if (ok) { sensor_pass_count++; }
    show_step(sensor_tests[3].name, ok ? "Correct" : "Fail");
    HAL_Delay(1000U);

    /* --- Giroscopio LSM6DSO32TR (accel + gyro) --- */
    show_step(sensor_tests[4].name, "...");
    HAL_Delay(1000U);
    ok = Diagnostico.imu ? 1U : 0U;
    sensor_tests[4].passed = ok;
    hw_status.mira.giroscopio = (ok == 1U);
    if (ok) { sensor_pass_count++; }
    show_step(sensor_tests[4].name, ok ? "Correct" : "Fail");
    HAL_Delay(1000U);

    /* --- BatteryMonitor BQ27441 --- */
    show_step(sensor_tests[5].name, "...");
    HAL_Delay(1000U);
    ok = Diagnostico.batterymonitor ? 1U : 0U;
    sensor_tests[5].passed = ok;
    hw_status.mira.batterymonitor = (ok == 1U);
    if (ok) { sensor_pass_count++; }
    show_step(sensor_tests[5].name, ok ? "Correct" : "Fail");
    HAL_Delay(1000U);
}

static void run_sensor_tests(void)
{
    Log_Print("TESTHW", "==== TEST Autodiagnostico ====");
    run_sensors_only();
    show_step("Completado", "");
    HAL_Delay(2000U);
}

/* ========================  MANUAL-TESTS  ================================= */

static void run_manual_act(void)
{
    switch (manual_id) {
        case MANUAL_BUZZER:
            show_step("Buzzer", "Sonando...");
            Buzzer_Test();
            break;

        case MANUAL_RGB:
            show_step("RGB", "Ciclando...");
            LedRGB_Test();
            break;

        default:
            break;
    }
}

/* ========================  AUTOTEST SEQUENCE  ============================= */

static void autotest_wait_confirm(Menu_Handle_t *h, uint8_t id)
{
    manual_id          = id;
    manual_confirm_sel = 0U;

    run_manual_act();

    draw_confirm();
    h->flag_enter    = false;
    h->flag_navigate = false;

    while (!h->flag_enter) {
        Menu_Poll(h);
        if (h->flag_navigate) {
            h->flag_navigate = false;
            manual_confirm_sel ^= 1U;
            draw_confirm();
        }
        HAL_Delay(10U);
    }
    h->flag_enter = false;

    bool result = (manual_confirm_sel == 0U);
    switch (id) {
        case MANUAL_BUZZER:
            hw_status.mira.buzzer = result;
            Log_Printf("TESTHW", "Buzzer: %s", result ? "Correct" : "Fail");
            break;
        case MANUAL_RGB:
            hw_status.mira.rgb_driver = result;
            Log_Printf("TESTHW", "RGB: %s", result ? "Correct" : "Fail");
            break;
        default: break;
    }
}

static void run_autotest_sequence(Menu_Handle_t *h)
{
    Log_Print("TESTHW", "==== TEST Autodiagnostico ====");
    run_sensors_only();
    autotest_wait_confirm(h, MANUAL_BUZZER);
    autotest_wait_confirm(h, MANUAL_RGB);
    show_autotest_result();
}

/* ========================  DRAW  ========================================= */

void Screen_TestHW_Draw(Menu_Handle_t *h)
{
    switch (h->sub_state) {

        case TESTHW_MENU:
            draw_list(testhw_l1_labels, TESTHW_L1_COUNT, h->selected, 0U);
            break;

        case TESTHW_MIRA_MENU:
            draw_list(testhw_mira_labels, TESTHW_MIRA_COUNT, mira_selected, mira_offset);
            break;

        case TESTHW_AUTO_RUNNING:
            run_sensor_tests();
            mira_selected   = 0U;
            mira_offset     = 0U;
            h->sub_state    = TESTHW_MIRA_MENU;
            draw_list(testhw_mira_labels, TESTHW_MIRA_COUNT, mira_selected, mira_offset);
            break;

        case TESTHW_MANUAL_ACT:
            run_manual_act();
            manual_confirm_sel = 0U;
            h->sub_state    = TESTHW_MANUAL_CONFIRM;
            draw_confirm();
            break;

        case TESTHW_MANUAL_CONFIRM:
            draw_confirm();
            break;

        case TESTHW_COMPLETED:
            show_step("Completado", "");
            HAL_Delay(2000U);
            mira_selected   = 0U;
            mira_offset     = 0U;
            h->sub_state    = TESTHW_MIRA_MENU;
            draw_list(testhw_mira_labels, TESTHW_MIRA_COUNT, mira_selected, mira_offset);
            break;

        case TESTHW_AUTOTEST_START:
            run_autotest_sequence(h);
            mira_selected   = 0U;
            mira_offset     = 0U;
            h->sub_state    = TESTHW_MIRA_MENU;
            draw_list(testhw_mira_labels, TESTHW_MIRA_COUNT, mira_selected, mira_offset);
            break;

        case TESTHW_SENSORES_NO_BT:
            show_step("Bluetooth", "No conectado");
            HAL_Delay(TESTHW_SENSORES_SPLASH_MS);
            h->selected     = 0U;
            h->sub_state    = TESTHW_MENU;
            h->needs_redraw = true;
            break;

        case TESTHW_SENS_ESPERANDO:
            show_step("Esperando", "...");
            if (Bluetooth.rx_ready) {
                sens_parse_flags();
                Bt_ResetRx(&Bluetooth);
                sens_show_results_sequence();
                sens_manual_id  = SENS_MANUAL_BUZZER;
                sens_send_cmd("BUZ");
                h->sub_state    = TESTHW_SENS_MANUAL_ACT;
                h->needs_redraw = true;
            } else if ((HAL_GetTick() - h->splash_tick) >= SENS_TEST_TIMEOUT_MS) {
                show_step("Falla", "Comunicacion");
                HAL_Delay(3000U);
                h->selected     = 0U;
                h->sub_state    = TESTHW_MENU;
                h->needs_redraw = true;
            }
            h->needs_redraw = true;
            break;

        case TESTHW_SENS_MANUAL_ACT:
            if (sens_manual_id == SENS_MANUAL_BUZZER) {
                show_step("Buzzer", "Sonando...");
                HAL_Delay(SENS_BUZZER_MS);
            } else {
                show_step("RGB", "Ciclando...");
                HAL_Delay(SENS_BUZZER_MS);
            }
            sens_confirm_sel = 0U;
            h->sub_state     = TESTHW_SENS_MANUAL_CFM;
            sens_draw_confirm();
            break;

        case TESTHW_SENS_MANUAL_CFM:
            sens_draw_confirm();
            break;

        case TESTHW_SENS_RESULT:
            sens_show_report();
            h->selected     = 0U;
            h->sub_state    = TESTHW_MENU;
            h->needs_redraw = true;
            break;

        default:
            break;
    }
}

/* ========================  INPUT  ======================================== */

void Screen_TestHW_OnButton(Menu_Handle_t *h, MenuButton_e btn)
{
    switch (h->sub_state) {

        case TESTHW_MENU:
            if (btn == BTN_NAVIGATE) {
                h->selected++;
                if (h->selected >= TESTHW_L1_COUNT) { h->selected = 0U; }
                h->needs_redraw = true;
            } else if (btn == BTN_ENTER) {
                switch (h->selected) {
                    case 0U:  /* Mira */
                        mira_selected   = 0U;
                        mira_offset     = 0U;
                        h->sub_state    = TESTHW_MIRA_MENU;
                        h->needs_redraw = true;
                        break;
                    case 1U:  /* Sensores */
                        if (h->bt_connected) {
                            sens_send_cmd("TEST");
                            h->sub_state    = TESTHW_SENS_ESPERANDO;
                            h->splash_tick  = HAL_GetTick();
                            h->needs_redraw = true;
                        } else {
                            h->sub_state    = TESTHW_SENSORES_NO_BT;
                            h->needs_redraw = true;
                        }
                        break;
                    case 2U:  /* Salir */
                        Menu_GoTo(h, SCREEN_MAIN_MENU);
                        break;
                    default:
                        break;
                }
            }
            break;

        case TESTHW_MIRA_MENU:
            if (btn == BTN_NAVIGATE) {
                mira_selected++;
                if (mira_selected >= TESTHW_MIRA_COUNT) {
                    mira_selected = 0U;
                    mira_offset   = 0U;
                } else if (mira_selected >= mira_offset + MENU_VISIBLE_ITEMS) {
                    mira_offset++;
                }
                h->needs_redraw = true;
            } else if (btn == BTN_ENTER) {
                switch (mira_selected) {
                    case 0U:  /* Autotest */
                        h->sub_state    = TESTHW_AUTOTEST_START;
                        h->needs_redraw = true;
                        break;
                    case 1U:  /* Sensores */
                        h->sub_state    = TESTHW_AUTO_RUNNING;
                        h->needs_redraw = true;
                        break;
                    case 2U:  /* Buzzer */
                        manual_id       = MANUAL_BUZZER;
                        h->sub_state    = TESTHW_MANUAL_ACT;
                        h->needs_redraw = true;
                        break;
                    case 3U:  /* RGB */
                        manual_id       = MANUAL_RGB;
                        h->sub_state    = TESTHW_MANUAL_ACT;
                        h->needs_redraw = true;
                        break;
                    case 4U:  /* Salir — vuelve al nivel 1 */
                        h->selected     = 0U;
                        h->sub_state    = TESTHW_MENU;
                        h->needs_redraw = true;
                        break;
                    default:
                        break;
                }
            }
            break;

        case TESTHW_MANUAL_CONFIRM:
            if (btn == BTN_NAVIGATE) {
                manual_confirm_sel = (manual_confirm_sel == 0U) ? 1U : 0U;
                h->needs_redraw = true;
            } else if (btn == BTN_ENTER) {
                bool result = (manual_confirm_sel == 0U); /* 0=SI→true, 1=NO→false */
                switch (manual_id) {
                    case MANUAL_BUZZER:
                        hw_status.mira.buzzer = result;
                        Log_Printf("TESTHW", "Buzzer: %s", result ? "Correct" : "Fail");
                        break;
                    case MANUAL_RGB:
                        hw_status.mira.rgb_driver = result;
                        Log_Printf("TESTHW", "RGB: %s", result ? "Correct" : "Fail");
                        break;
                    default: break;
                }
                h->sub_state    = TESTHW_COMPLETED;
                h->needs_redraw = true;
            }
            break;

        case TESTHW_SENS_MANUAL_CFM:
            if (btn == BTN_NAVIGATE) {
                sens_confirm_sel = (sens_confirm_sel == 0U) ? 1U : 0U;
                h->needs_redraw = true;
            } else if (btn == BTN_ENTER) {
                bool result = (sens_confirm_sel == 0U);
                if (sens_manual_id == SENS_MANUAL_BUZZER) {
                    hw_status.sensores.buzzer = result;
                    sens_manual_id = SENS_MANUAL_RGB;
                    sens_send_cmd("RGB");
                    h->sub_state    = TESTHW_SENS_MANUAL_ACT;
                    h->needs_redraw = true;
                } else {
                    hw_status.sensores.rgb = result;
                    h->sub_state    = TESTHW_SENS_RESULT;
                    h->needs_redraw = true;
                }
            }
            break;

        default:
            break;
    }
}
