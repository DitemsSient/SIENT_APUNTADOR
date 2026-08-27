/**
 * @file    Test.c
 * @brief   Implementación del banco de pruebas de hardware de la tarjeta Mira.
 *
 * @date    August 27, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#include "Test.h"
#include "Logger.h"
#include "main.h"
#include <stdio.h>

/* ======================  EXTERNAL HAL HANDLES  ============================ */

extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

/* ======================  STATIC VARIABLES  ================================ */

/* Handles de driver que antes vivian en main.c — se centralizan aqui porque
 * varios drivers (LSM6DSO32TR_Test, TSL2571_Test, Bt_Test) los referencian
 * como "extern" con estos nombres exactos, y Menu_Exercise.c hace lo mismo
 * con laser_calibration_mode. */

/* ===========================================================================
 *  PRUEBA 1 — UART (Logger) + Modo Programación
 * ===========================================================================
 */
#include "ModoProgramacion.h"

void Test_UART_ModoProgramacion(bool modo_bt) {
    ModoProgramacion_Init();
    if (modo_bt) {
        ModoProgramacion_SetBT();
    } else {
        ModoProgramacion_SetMCU();
    }

    for (uint8_t i = 0U; i < 10U; i++) {
        Log_Printf("TEST", "UART OK #%u (modo %s)", i + 1U, modo_bt ? "BT" : "MCU");
        HAL_Delay(1000U);
    }
}

/* ===========================================================================
 *  PRUEBA 2 — LedRGB
 * ===========================================================================
 */
#include "LedRGB.h"

void Test_LedRGB(void) {
    LedRGB_Init();
    LedRGB_Test();
    Log_Print("TEST", "LedRGB: ciclo R->G->B completado");
}

/* ===========================================================================
 *  PRUEBA 3 — Buzzer
 * ===========================================================================
 */
#include "Buzzer_Melodias.h"

void Test_Buzzer(void) {
    Buzzer_PlayMelody(ode_to_joy, sizeof(ode_to_joy) / sizeof(ode_to_joy[0]), 120U);
    Log_Print("TEST", "Buzzer: melodia ode_to_joy completada");
}

/* ===========================================================================
 *  PRUEBA 4/15 — Transmision_Laser_IR (CONFIRMADO: alcance >= 40 m)
 * ===========================================================================
 */
#include "Transmsion_Laser_IR.h"
#include "Multiplexor_CD4051B.h"

bool laser_calibration_mode = false;
mux_handle_t mux;

void Test_LaserIR(uint8_t n_disparos) {
    HAL_TIM_Base_Start(&Tx_IR_DELAY_TIM_HANDLE);
    HAL_TIM_PWM_Start(&Tx_IR_TIM_HANDLE, Tx_IR_TIM_CHANNEL);
    Tx_IR_Init();

    MUX_Init(&mux);
    MUX_SelectChannel(&mux, MUX_CHANNEL_6);
    Log_Print("MUX", "Canal fijo en 6 (potencia IR)");

    uint8_t frame_data[1] = { 0xA5U };
    for (uint8_t i = 0U; i < n_disparos; i++) {
        Tx_IR_SendFrame(frame_data, sizeof(frame_data));
        Log_Printf("TEST", "Trama IR enviada #%u", i + 1U);
        HAL_Delay(1000U);
    }
}

/* ===========================================================================
 *  PRUEBA 5 — SensorHall (Gatillo)
 * ===========================================================================
 */
#include "SensorHall.h"

void Test_SensorHall(void) {
    uint8_t result = HallSensor_Test();
    Log_Printf("TEST", "SensorHall self-test: %s", result ? "PASS" : "FAIL");

    bool last_state = HallSensor_IsPressed();
    Log_Printf("HALL", "Estado inicial: %s", last_state ? "PRESIONADO" : "LIBRE");

    for (uint8_t i = 0U; i < 50U; i++) {
        bool state = HallSensor_IsPressed();
        if (state != last_state) {
            Log_Printf("HALL", "Cambio de estado: %s", state ? "PRESIONADO" : "LIBRE");
            last_state = state;
        }
        HAL_Delay(100U);
    }
}

/* ===========================================================================
 *  PRUEBA 6 — Botones A / B
 * ===========================================================================
 */

void Test_Botones(uint32_t duracion_ms) {
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < duracion_ms) {
        bool boton_a = (HAL_GPIO_ReadPin(BOTON_A_GPIO_Port, BOTON_A_Pin) == GPIO_PIN_RESET);
        bool boton_b = (HAL_GPIO_ReadPin(BOTON_B_GPIO_Port, BOTON_B_Pin) == GPIO_PIN_RESET);

        if (boton_a) {
            LedRGB_SetColor(RGB_RED);
        } else if (boton_b) {
            LedRGB_SetColor(RGB_BLUE);
        } else {
            LedRGB_Off();
        }
        HAL_Delay(50U);
    }
    LedRGB_Off();
    Log_Print("TEST", "Botones: prueba finalizada");
}

/* ===========================================================================
 *  PRUEBA 7 — Bluetooth (BL654) - AT
 * ===========================================================================
 */
#include "Bluetooth.h"

Bt_Handle_t hbt;

void Test_Bluetooth(void) {
    Bt_Init(&hbt);
    uint8_t result = Bt_Test();
    Log_Printf("TEST", "Bluetooth self-test (AT): %s", result ? "PASS" : "FAIL");
}

/* ===========================================================================
 *  PRUEBA 8 — Flash (MX25L6445E)
 * ===========================================================================
 */
#include "Flash.h"

void Test_Flash(void) {
    uint8_t result = Flash_Test();
    Log_Printf("TEST", "Flash self-test: %s", result ? "PASS" : "FAIL");
}

/* ===========================================================================
 *  PRUEBA 9 — LSM6DSO32TR (IMU accel+gyro, chip real LSM6DS3)
 * ===========================================================================
 */
#include "LSM6DSO32TR.h"

LSM6DSO32TR_t lsm;

void Test_IMU(uint8_t n_muestras) {
    if (LSM6DSO32TR_Init(&lsm) != LSM_OK) {
        Log_Print("TEST", "IMU: fallo de inicializacion");
        return;
    }

    LSM_Data_t data;
    for (uint8_t i = 0U; i < n_muestras; i++) {
        if (LSM6DSO32TR_ReadAll(&lsm, &data) == LSM_OK) {
            Log_Printf("IMU", "accel(g)=%.2f,%.2f,%.2f gyro(dps)=%.2f,%.2f,%.2f temp=%.1fC",
                       data.ax_g, data.ay_g, data.az_g,
                       data.gx_dps, data.gy_dps, data.gz_dps,
                       data.temp_c);
        } else {
            Log_Print("IMU", "Error de lectura");
        }
        HAL_Delay(1000U);
    }
}

/* ===========================================================================
 *  PRUEBA 10 — MMC5983MA (Magnetómetro)
 * ===========================================================================
 */
#include "MMC5983MA.h"

void Test_Magnetometro(uint8_t n_muestras) {
    if (MMC5983MA_Init() != MMC_OK) {
        Log_Print("TEST", "Magnetometro: fallo de inicializacion");
        return;
    }

    MMC_Data_t data;
    for (uint8_t i = 0U; i < n_muestras; i++) {
        if (MMC5983MA_ReadAll(&data) == MMC_OK) {
            Log_Printf("MAG", "X=%.1fuT Y=%.1fuT Z=%.1fuT", data.x_uT, data.y_uT, data.z_uT);
        } else {
            Log_Print("MAG", "Error de lectura");
        }
        HAL_Delay(1000U);
    }
}

/* ===========================================================================
 *  PRUEBA 11 — SensorLuz_TSL2571
 * ===========================================================================
 */
#include "SensorLuz_TSL2571.h"

TSL2571_t tsl;

void Test_SensorLuz(uint8_t n_muestras) {
    TSL2571_Attach(&tsl, &hi2c1, TSL2571_ADDR_7BIT, 100U);
    if (TSL2571_Begin(&tsl, 0xC0U, TSL2571_GAIN_1X) != HAL_OK) {
        Log_Print("TEST", "SensorLuz: fallo de inicializacion");
        return;
    }

    TSL2571_RawData_t raw;
    float lux;
    for (uint8_t i = 0U; i < n_muestras; i++) {
        if (TSL2571_ReadLux(&tsl, 1U, 200U, &lux, &raw) == HAL_OK) {
            Log_Printf("LUZ", "CH0=%u CH1=%u Lux=%.1f Sat=%u",
                       raw.ch0, raw.ch1, lux, raw.saturated);
        } else {
            Log_Print("LUZ", "Error de lectura");
        }
        HAL_Delay(1000U);
    }
}

/* ===========================================================================
 *  PRUEBA 12 — BatteryMonitor (BQ27441)
 * ===========================================================================
 */
#include "BatteryMonitor.h"

void Test_BatteryMonitor(uint8_t n_muestras) {
    if (BatGauge_Init() != HAL_OK) {
        Log_Print("TEST", "BatteryMonitor: fallo de inicializacion");
        return;
    }

    BatGauge_Data_t data;
    for (uint8_t i = 0U; i < n_muestras; i++) {
        BatGauge_Update(&data);
        if (data.is_ready) {
            Log_Printf("BAT", "V=%umV I=%dmA SOC=%u%% SOH=%u%% estado=%d",
                       data.voltage_mV, data.avg_current_mA, data.soc_pct,
                       data.soh_pct, (int)data.charge_state);
        } else {
            Log_Print("BAT", "Lectura no valida");
        }
        HAL_Delay(3000U);
    }
}

/* ===========================================================================
 *  PRUEBA 13 — Display OLED (SSD1306)
 * ===========================================================================
 */
#include "Display_Oled/Display_Comands.h"
#include "Display_Oled/Display_Fonts.h"

void Test_DisplayOled(void) {
    ssd1306_begin(SSD1306_SWITCHCAPVCC, 0x3CU);

    for (uint8_t i = 1U; i <= 10U; i++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", i);

        ssd1306_clearDisplay();
        ssd1306_printCenter(buf, &Font6x8);
        ssd1306_display();

        Log_Printf("TEST", "Display: mostrando %u", i);
        HAL_Delay(1000U);
    }
}

/* ===========================================================================
 *  PRUEBA 14 — Multiplexor_CD4051B
 * ===========================================================================
 */

void Test_Multiplexor(void) {
    MUX_Init(&mux);

    for (uint8_t ch = 0U; ch < (uint8_t)MUX_CHANNEL_TOTAL; ch++) {
        MUX_SelectChannel(&mux, (mux_channel_t)ch);
        Log_Printf("TEST", "Multiplexor: canal %u activo", ch);
        HAL_Delay(1000U);
    }
}

/* ===========================================================================
 *  PRUEBA 17 — Escaneo de bus I2C1
 * ===========================================================================
 */

void Test_I2CScan(void) {
    Log_Print("TEST", "Iniciando escaneo I2C1...");

    for (uint16_t addr = 1U; addr < 127U; addr++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 2U, 10U) == HAL_OK) {
            Log_Printf("I2C", "Dispositivo encontrado en 0x%02X", addr);
        }
    }

    Log_Print("TEST", "Escaneo I2C1 finalizado");
}

/* ===========================================================================
 *  PRUEBA 18 — Programar el BT
 * ===========================================================================
 */

void Test_ProgramarBT(void) {
    ModoProgramacion_Init();
    ModoProgramacion_SetBT();
    Log_Print("TEST", "Modo BT fijo -- listo para programar el BL654");

    while (1) {
        LedRGB_SetColor(RGB_RED);
        HAL_Delay(200U);
        LedRGB_Off();
        HAL_Delay(1800U);
    }
}
