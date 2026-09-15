/**
 * @file    Inicializacion.c
 * @brief   Implementación de la secuencia de arranque del firmware real.
 *
 * @date    August 27, 2026
 * @author  César Pérez
 * @version 1.0.0
 */

#include "Inicializacion.h"
#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* ===========================================================================
 *  BOOTLOADER + USB CDC + LOGGER  (siempre activos, ver Inicializacion.h)
 * ===========================================================================
 */
#include "Bootloader.h"
#include "LedRGB.h"
#include "Logger.h"
#include "usb_device.h"

/* ======================  EXTERNAL HAL HANDLES  ============================ */

extern I2C_HandleTypeDef hi2c1;

/* ======================  STATIC VARIABLES  ================================ */

/* Dispositivos I2C1 esperados en esta tarjeta (ver Core/Doc/Pruebas_HW.md,
 * prueba 17). Direcciones en formato 7-bit. */

typedef struct {
    uint8_t     addr_7bit;
    const char *nombre;
} Inicializacion_I2CDevice_t;

static const Inicializacion_I2CDevice_t s_i2c_esperados[] = {
    { 0x30U, "MMC5983MA (Magnetometro)" },
    { 0x39U, "TSL2571 (SensorLuz)"      },
    { 0x3CU, "SSD1306 (Display/LCD)"    },
    { 0x55U, "BQ27441 (BatteryMonitor)" },
    { 0x6AU, "LSM6DSO32TR (IMU)"        },
};
#define I2C_ESPERADOS_LEN  (sizeof(s_i2c_esperados) / sizeof(s_i2c_esperados[0]))

/* ===========================================================================
 *  Modo Programación (mux BT/MCU)
 * ===========================================================================
 */
#if INIT_MODOPROGRAMACION_ENABLE
#include "ModoProgramacion.h"
#endif

/* ===========================================================================
 *  Multiplexor de resistencias (CD4051B)
 * ===========================================================================
 */
#if INIT_MULTIPLEXOR_ENABLE
#include "Multiplexor_CD4051B.h"
mux_handle_t Mux_Laser;
#endif

/* ===========================================================================
 *  Flash SPI (MX25L6445E)
 * ===========================================================================
 */
#if INIT_FLASH_ENABLE
#include "Flash.h"
#endif

/* ===========================================================================
 *  Buzzer
 * ===========================================================================
 */
#if INIT_BUZZER_ENABLE
#include "Buzzer_Melodias.h"
#endif

/* ===========================================================================
 *  Bluetooth (BL654)
 * ===========================================================================
 */
#if INIT_BLUETOOTH_ENABLE
#include "Bluetooth.h"
Bt_Handle_t Bluetooth;
#endif

/* ===========================================================================
 *  IMU (LSM6DSO32TR / chip real LSM6DS3)
 * ===========================================================================
 */
#if INIT_IMU_ENABLE
#include "LSM6DSO32TR.h"
LSM6DSO32TR_t Imu;
LSM_Data_t Imu_UltimaLectura;
#endif

/* ===========================================================================
 *  Magnetómetro (MMC5983MA)
 * ===========================================================================
 */
#if INIT_MAGNETOMETRO_ENABLE
#include "MMC5983MA.h"
MMC_Data_t Magnetometro_UltimaLectura;
#endif

/* ========================  MENU / ESTADO DE JUEGO  ========================= */

Menu_Handle_t hmenu;
HWTest_Status_t hw_status;
Diagnostico_t Diagnostico;

/* Datos base -- se muestran en la Preview mientras no se haya conectado
 * el Bluetooth (nada real que mostrar todavia). Se sobreescriben con los
 * datos reales del ejercicio al parsear un frame $*<csv>\r valido, y se
 * regresan a estos mismos valores al desconectarse (ver Menu.c, $DSCON). */
ExerciseGameData_t g_exercise_data = {
    .lives       = 1U,
    .ammo        = 2U,
    .team_name   = "EQUIPO x",
    .player_name = "USER x",
    .lvBatery    = 100U
};

/* laser_calibration_mode: extern declarada en Transmsion_Laser_IR.h,
 * definida aqui porque Menu_Exercise.c y Test.c la referencian por nombre. */
bool laser_calibration_mode = false;

/* ejercicio_disparo_habilitado: idem, la controla ExerciseTask. */
volatile bool ejercicio_disparo_habilitado = false;

/* ===========================================================================
 *  Sensor de luz (TSL2571)
 * ===========================================================================
 */
#if INIT_SENSORLUZ_ENABLE
#include "SensorLuz_TSL2571.h"
TSL2571_t SensorLuz;
TSL2571_RawData_t SensorLuz_UltimaLectura;
float SensorLuz_UltimoLux;
#endif

/* ===========================================================================
 *  Fuel gauge / BatteryMonitor (BQ27441)
 * ===========================================================================
 */
#if INIT_BATTERYMONITOR_ENABLE
#include "BatteryMonitor.h"
BatGauge_Data_t Bateria;
#endif

/* ===========================================================================
 *  Display OLED (SSD1306)
 * ===========================================================================
 */
#if INIT_DISPLAY_ENABLE
#include "Display_Oled/Display_Comands.h"
#include "Display_Oled/Display_Fonts.h"
#include "Display_Oled/Display_Bitmaps.h"
#endif

/* ===========================================================================
 *  Transmision Laser IR
 * ===========================================================================
 */
#if INIT_LASERIR_ENABLE
#include "Transmsion_Laser_IR.h"
#endif

/* ===========================================================================
 *  SensorHall (Gatillo)
 * ===========================================================================
 */
#if INIT_SENSORHALL_ENABLE
#include "SensorHall.h"
#endif

/* ======================  STATIC FUNCTIONS  ================================ */

/**
 * @brief  Escanea todo el bus I2C1 (1-126), imprime cada dirección que
 *         responde, y luego compara contra s_i2c_esperados[].
 */
static void Inicializacion_ScanI2C(void) {
    bool encontrado[128] = { false };
    bool todos_ok = true;

    Log_Print("I2C", "Escaneando bus I2C1...");
    for (uint16_t addr = 1U; addr < 127U; addr++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 2U, 10U) == HAL_OK) {
            encontrado[addr] = true;
            Log_Printf("I2C", "0x%02X responde", addr);
        }
    }

    Log_Print("I2C", "Comparando contra dispositivos esperados...");
    for (uint8_t i = 0U; i < I2C_ESPERADOS_LEN; i++) {
        uint8_t     addr   = s_i2c_esperados[i].addr_7bit;
        const char *nombre = s_i2c_esperados[i].nombre;

        if (encontrado[addr]) {
            Log_Printf("I2C", "0x%02X %s encontrado", addr, nombre);
        } else {
            Log_Printf("I2C", "0x%02X %s NO encontrado", addr, nombre);
            todos_ok = false;
        }
    }

    Diagnostico.i2c_completo = todos_ok;
}

/**
 * @brief  Imprime un separador de seccion tipo "==== titulo ====".
 */
static void Inicializacion_PrintSeparador(const char *titulo) {
    Log_Print("SIENT", "========================================");
    Log_Printf("SIENT", "  %s", titulo);
    Log_Print("SIENT", "========================================");
}

/* ================================  API  =================================== */

/* Funciones públicas declaradas en el .h */

void Inicializacion_PrintBanner(void) {
    Log_Print("SIENT", "========================================");
    Log_Print("SIENT", "  Proyecto SIENT_APUNTADOR v1");
    Log_Print("SIENT", "========================================");
    Log_Print("SIENT", "USB configurado en modo Logger correctamente.");
    Log_Print("SIENT", "Modo normal activado.");
}

void Inicializacion_PrintDiagnostico(void) {
    Log_Print("DIAG", "---- Estado de Diagnostico ----");
    Log_Printf("DIAG", "ModoProgramacion: %s", Diagnostico.modoprogramacion ? "OK" : "FALLO");
    Log_Printf("DIAG", "Multiplexor:      %s", Diagnostico.multiplexor     ? "OK" : "FALLO");
    Log_Printf("DIAG", "Flash:            %s", Diagnostico.flash           ? "OK" : "FALLO");
    Log_Printf("DIAG", "Buzzer:           %s", Diagnostico.buzzer          ? "OK" : "FALLO");
    Log_Printf("DIAG", "Bluetooth:        %s", Diagnostico.bluetooth       ? "OK" : "FALLO");
    Log_Printf("DIAG", "IMU:              %s", Diagnostico.imu             ? "OK" : "FALLO");
    Log_Printf("DIAG", "Magnetometro:     %s", Diagnostico.magnetometro    ? "OK" : "FALLO");
    Log_Printf("DIAG", "SensorLuz:        %s", Diagnostico.sensorluz       ? "OK" : "FALLO");
    Log_Printf("DIAG", "BatteryMonitor:   %s", Diagnostico.batterymonitor  ? "OK" : "FALLO");
    Log_Printf("DIAG", "LaserIR:          %s", Diagnostico.laserir         ? "OK" : "FALLO");
    Log_Printf("DIAG", "SensorHall:       %s", Diagnostico.sensorhall      ? "OK" : "FALLO");
    Log_Printf("DIAG", "Display:          %s", Diagnostico.display         ? "OK" : "FALLO");
    Log_Printf("DIAG", "I2C completo:     %s", Diagnostico.i2c_completo    ? "OK" : "FALLO");
    Log_Print("DIAG", "--------------------------------");
}

void Inicializacion_PrintExerciseData(void) {
    Log_Print("GAME", "---- Datos de ejercicio ----");
    Log_Printf("GAME", "Orden: %u", g_exercise_data.orden);
    Log_Printf("GAME", "Lora: %u", g_exercise_data.lora);
    Log_Printf("GAME", "Equipo: %s", g_exercise_data.team_name);
    Log_Printf("GAME", "Alias: %s", g_exercise_data.player_name);
    Log_Printf("GAME", "Vidas: %u", g_exercise_data.lives);
    Log_Printf("GAME", "Balas: %u", g_exercise_data.ammo);
    Log_Printf("GAME", "Tiempo: %lu", (unsigned long)g_exercise_data.tiempo);
    Log_Printf("GAME", "MAC: %s", g_exercise_data.mac);
    Log_Print("GAME", "-----------------------------");
}

void Inicializacion_Run(void) {
#if INIT_BOOTLOADER_ENABLE
    /* Revisamos si entramos en modo bootloader o normal*/
    LedRGB_Init();
    Bootloader_CheckAndEnter();
#endif

#if INIT_USB_LOGGER_ENABLE
    /* Damos un pequeño margen de 5s para conectarnos a la terminal y ver todos los mensajes. */
    MX_USB_DEVICE_Init();
    HAL_Delay(5000U);

    Log_Init();
    Inicializacion_PrintSeparador("INICIALIZACION");
    Log_NewLine();

    Inicializacion_PrintBanner();
    Log_NewLine();
    HAL_Delay(500U);

    Inicializacion_ScanI2C();
    Log_NewLine();
    HAL_Delay(500U);
#endif

#if INIT_MODOPROGRAMACION_ENABLE
    ModoProgramacion_Init();
    Diagnostico.modoprogramacion = true;
    Log_NewLine();
    HAL_Delay(500U);
#endif

#if INIT_MULTIPLEXOR_ENABLE
    /* Canal inicial de arranque -- LuzMuxTask (Tareas_Interrupciones.c) lo
     * reajusta cada 5s segun la luz ambiental en cuanto arranca el RTOS. */
    Diagnostico.multiplexor = (MUX_Init(&Mux_Laser) == MUX_OK);
    Diagnostico.multiplexor = (MUX_SelectChannel(&Mux_Laser, MUX_CHANNEL_6) == MUX_OK) && Diagnostico.multiplexor;
    Log_Print("MUX", "Multiplexor inicializado en canal 6 (valor inicial)");
    Log_NewLine();
    HAL_Delay(500U);
#endif

#if INIT_FLASH_ENABLE
    Log_Print("FLASH", "Inicializando Flash SPI...");
    if (Flash_Init() == FLASH_OK) {
        Log_Print("FLASH", "Init OK, corriendo self-test...");
        if (Flash_Test()) {
            Log_Print("FLASH", "Test inicial verificado");
            Diagnostico.flash = true;
        } else {
            Log_Print("FLASH", "Self-test FALLO");
        }
    } else {
        Log_Print("FLASH", "Init FALLO");
    }
    Log_NewLine();
    HAL_Delay(500U);
#endif

#if INIT_BUZZER_ENABLE
    Buzzer_Init();
    Log_Print("BUZZER", "Sonando...");
    Buzzer_PlayMelody(alert, MELODY_LEN(alert), 160U);
    HAL_Delay(1000);
    Buzzer_PlayMelody(alert, MELODY_LEN(alert), 160U);
    Log_Print("BUZZER", "Buzzer inicializado");
    Diagnostico.buzzer = true;
    Log_NewLine();
    HAL_Delay(500U);
#endif

#if INIT_IMU_ENABLE
    Log_Print("IMU", "Inicializando IMU...");
    if (LSM6DSO32TR_Init(&Imu) == LSM_OK) {
        LSM6DSO32TR_ReadAll(&Imu, &Imu_UltimaLectura);
        Log_Printf("IMU", "accel(g)=%.2f,%.2f,%.2f gyro(dps)=%.2f,%.2f,%.2f",
                   Imu_UltimaLectura.ax_g, Imu_UltimaLectura.ay_g, Imu_UltimaLectura.az_g,
                   Imu_UltimaLectura.gx_dps, Imu_UltimaLectura.gy_dps, Imu_UltimaLectura.gz_dps);
        Diagnostico.imu = true;
    } else {
        Log_Print("IMU", "Init FALLO");
    }
    Log_NewLine();
    HAL_Delay(500U);
#endif

#if INIT_MAGNETOMETRO_ENABLE
    Log_Print("MAG", "Inicializando magnetometro...");
    if (MMC5983MA_Init() == MMC_OK) {
        HAL_Delay(150U); /* espera la primera conversion (ODR 10Hz ~100ms) */
        MMC5983MA_ReadAll(&Magnetometro_UltimaLectura);
        Log_Printf("MAG", "X=%.1fuT Y=%.1fuT Z=%.1fuT", Magnetometro_UltimaLectura.x_uT,
                   Magnetometro_UltimaLectura.y_uT, Magnetometro_UltimaLectura.z_uT);
        Diagnostico.magnetometro = true;
    } else {
        Log_Print("MAG", "Init FALLO");
    }
    Log_NewLine();
    HAL_Delay(500U);
#endif

#if INIT_SENSORLUZ_ENABLE
    Log_Print("LUZ", "Inicializando sensor de luz...");
    TSL2571_Attach(&SensorLuz, &hi2c1, TSL2571_ADDR_7BIT, 100U);
    if (TSL2571_Begin(&SensorLuz, 0xC0U, TSL2571_GAIN_1X) == HAL_OK) {
        TSL2571_ReadLux(&SensorLuz, 1U, 200U, &SensorLuz_UltimoLux, &SensorLuz_UltimaLectura);
        Log_Printf("LUZ", "CH0=%u CH1=%u Lux=%.1f", SensorLuz_UltimaLectura.ch0,
                   SensorLuz_UltimaLectura.ch1, SensorLuz_UltimoLux);
        Diagnostico.sensorluz = true;
    } else {
        Log_Print("LUZ", "Init FALLO");
    }
    Log_NewLine();
    HAL_Delay(500U);
#endif

#if INIT_BATTERYMONITOR_ENABLE
    Log_Print("BAT", "Inicializando BatteryMonitor...");
    if (BatGauge_Init() == HAL_OK) {
        Diagnostico.batterymonitor = true;
        BatGauge_Update(&Bateria);
        if (Bateria.is_ready) {
            Log_Printf("BAT", "V=%umV I=%dmA SOC=%u%%", Bateria.voltage_mV,
                       Bateria.avg_current_mA, Bateria.soc_pct);
        } else {
            Log_Print("BAT", "Lectura no valida (sin bateria conectada)");
        }
    } else {
        Log_Print("BAT", "Init FALLO");
    }
    Log_NewLine();
    HAL_Delay(500U);
#endif

#if INIT_LASERIR_ENABLE
    /* TIM1 arranca como base de tiempo libre para los delays en us del
     * protocolo (MARK/SPACE). TIM2 CH3 arranca el PWM de 40kHz que sera
     * la portadora del laser. Tx_IR_Init() deja PA2 en idle-low y reafirma
     * ARR=24 (compartido con el Buzzer). Nada de esto dispara un tiro --
     * solo dispara el hardware, listo para Tx_IR_SendFrame(). */
    HAL_TIM_Base_Start(&Tx_IR_DELAY_TIM_HANDLE);
    HAL_TIM_PWM_Start(&Tx_IR_TIM_HANDLE, Tx_IR_TIM_CHANNEL);
    Tx_IR_Init();
    Log_Print("LASER", "Laser IR listo (idle)");
    Diagnostico.laserir = true;
    Log_NewLine();
    HAL_Delay(500U);
#endif

#if INIT_SENSORHALL_ENABLE
    Log_Print("HALL", "Periferico inicializado para leer el sensor de efecto Hall");
    Diagnostico.sensorhall = true;
    Log_NewLine();
    HAL_Delay(500U);
#endif

#if INIT_BLUETOOTH_ENABLE
    Log_Print("BT", "Probando comunicacion AT...");
    if (Bt_Test()) {
        Log_Print("BT", "Respuesta OK");
        Diagnostico.bluetooth = true;
    } else {
        Log_Print("BT", "Sin respuesta");
    }

    /* Corre el programa "Apuntador" ya cargado en el modulo -- siempre, sin
     * importar el resultado del test de arriba (14-sep-2026, antes era un
     * boton "RunBLE" en la pantalla de Bluetooth; ahora se manda una sola
     * vez aqui, como parte de la inicializacion). */
    Bt_SendRunBLE(&Bluetooth);
    Log_Print("BT", "RunBLE enviado");

    Log_NewLine();
    HAL_Delay(500U);
#endif

#if INIT_DISPLAY_ENABLE
    Log_Print("LCD", "Inicializando display OLED...");
    ssd1306_begin(SSD1306_SWITCHCAPVCC, 0x3CU);
    Diagnostico.display = true;
    ssd1306_clearDisplay();
    ssd1306_drawBitmap(0, 0, bitmap_Logo_SIENT, 64, 32, WHITE);
    ssd1306_display();
    Log_Print("LCD", "Logo mostrado, esperando boton A/B...");
    Log_NewLine();
    Inicializacion_PrintDiagnostico();
    Log_NewLine();

    while ((HAL_GPIO_ReadPin(BOTON_A_GPIO_Port, BOTON_A_Pin) == GPIO_PIN_SET) &&
           (HAL_GPIO_ReadPin(BOTON_B_GPIO_Port, BOTON_B_Pin) == GPIO_PIN_SET)) {
        HAL_Delay(20U);
    }

    ssd1306_clearDisplay();
    ssd1306_drawRect(0, 0, SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT, WHITE);
    ssd1306_drawRect(1, 1, SSD1306_LCDWIDTH - 2, SSD1306_LCDHEIGHT - 2, WHITE);
    ssd1306_printCenter("DITEMS", &Font6x8);
    ssd1306_display();
    Log_Print("LCD", "Boton detectado, mostrando DITEMS");
    HAL_Delay(2000U);

    /* Animacion de salida: borra columna por columna hasta dejar la
     * pantalla en negro. */
    for (uint8_t x = 0U; x < SSD1306_LCDWIDTH; x++) {
        ssd1306_fillRect((int16_t)x, 0, 1, SSD1306_LCDHEIGHT, BLACK);
        ssd1306_display();
        HAL_Delay(15U);
    }
    Log_NewLine();
    HAL_Delay(500U);
#endif

    Inicializacion_PrintSeparador("MENU");
    Menu_Init(&hmenu);
}
