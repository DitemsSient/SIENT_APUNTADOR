# Memorias de Librerías

Cada sección corresponde a un driver. El título (`## nombre`) es el hint que se pasa
entre paréntesis en `/new-rama <rama> (nombre)`.

---

## buzzer

Controla un buzzer pasivo para generar tonos y melodías musicales mediante PWM. Permite reproducir notas individuales, secuencias completas con tempo configurable y silencios. Es la capa de retroalimentación sonora del sistema (alertas, confirmaciones, boot).

**Archivo:** `Buzzer.h` / `Buzzer.c` · **Versión:** 1.0.0  
**Componente:** Buzzer pasivo via PWM  
**Timer:** `htim4`, canal `TIM_CHANNEL_1`  
**Clock efectivo:** `BUZZER_TIMER_CLK = 1 000 000 Hz` (APB1 / (prescaler+1))

Estructura de nota: `BuzzerNote_t { int16_t note [Hz], int16_t duration }`.  
Duration negativo = nota con puntillo (×1.5).  
Macro de longitud: `MELODY_LEN(arr)`.

**API:**
- `Buzzer_Init()` — arranca PWM, llamar tras `MX_TIM4_Init()`
- `Buzzer_PlayTone(freq)` — tono continuo (0 = silencio)
- `Buzzer_Stop()` — silencia inmediatamente
- `Buzzer_PlayMelody(melody, length, bpm)` — bloqueante, usa `HAL_Delay`
- `Buzzer_Test()` — tono A4 continuo 5 s (para uso del sistema TestHW)

**Melodías predefinidas** (en `Buzzer_Melodias.h`): `pink_panther`, `ode_to_joy`,
`alert`, `boot_ok`. Uso: `Buzzer_PlayMelody(boot_ok, MELODY_LEN(boot_ok), 140)`.

Notas disponibles: C4–B5 + C6/D6/E6 + `SILENCE`. Duraciones: `WHOLE`, `HALF`,
`QUARTER`, `EIGHTH`, `SIXTEENTH`.

---

## LedRGB

Maneja un LED RGB de cátodo común con tres pines GPIO independientes. Ofrece colores directos, parpadeo configurable y reproducción de patrones de secuencia. Se usa como indicador visual de estado del sistema (error, OK, standby, advertencia).

**Archivo:** `LedRGB.h` / `LedRGB.c` · **Versión:** 1.0.0  
**Componente:** LED RGB cátodo común via GPIO  
**Pines:** R→PG5, G→PG7, B→PG8

Tipo base: `RGBColor_t { r, g, b }` (0/1 por canal).  
Paleta lista: `RGB_OFF/RED/GREEN/BLUE/YELLOW/CYAN/MAGENTA/WHITE`.  
Patrones: `RGBStep_t { RGBColor_t, duration_ms }`. Macro: `RGB_PATTERN_LEN(arr)`.

**Patrones predefinidos:** `RGB_PATTERN_ERROR`, `_OK`, `_STANDBY`, `_RAINBOW`,
`_WARNING`, `_FLASH` (con sus `_LEN`).

**API:**
- `LedRGB_Init()` — configura GPIOs y apaga el LED
- `LedRGB_SetColor(color)` — color directo
- `LedRGB_On()` / `LedRGB_Off()` / `LedRGB_Toggle()`
- `LedRGB_Blink(color, times, on_ms, off_ms)` — bloqueante
- `LedRGB_PlayPattern(pattern, length)` — bloqueante, LED off al terminar
- `LedRGB_Test()` — cicla R→G→B × 2 (1.5 s/color = 9 s); reservado para test del LED discreto

---

## SensorHall

Lee la salida digital del comparador del sensor Hall HW-484 (usado como gatillo: imán + sensor). No usa ADC — el módulo trae su propio comparador y entrega un nivel HIGH/LOW limpio.

**Archivo:** `SensorHall.h` / `SensorHall.c` · **Versión:** 2.0.0  
**Componente:** Sensor Hall HW-484 (salida digital del comparador)  
**Pin:** `PA8` ("GATILLO"), GPIO_Input

**API:**
- `HallSensor_IsPressed()` → `bool` — `true` si el pin está en HIGH (imán detectado / gatillo presionado), `false` si está en LOW
- `HallSensor_Test()` → `uint8_t` — siempre 1, solo valida que la lectura del GPIO se ejecute (TestHW)

> La versión anterior (ADC, HW-484 en modo analógico con `HallState_e`/`HallData_t`/umbral por polo) quedó comentada al final de `SensorHall.h`/`.c` — se retomará si se rutea un pin con ADC disponible en el `.ioc` (actualmente esta tarjeta no tiene ningún ADC configurado).

---

## SensorLuz

Driver para el sensor de luz ambiente TSL2571 via I2C. Mide la iluminación en lux a partir de dos canales ADC internos (broadband e IR). Permite ajustar ganancia y tiempo de integración, y promediar N lecturas para mayor estabilidad.

**Archivo:** `SensorLuz_TSL2571.h` / `SensorLuz_TSL2571.c` · **Versión:** 1.0.0  
**Componente:** TSL2571 sensor de luz ambiente  
**Bus:** I2C, dirección 7-bit `0x39`

Handle: `TSL2571_t { hi2c, addr, timeout_ms, atime, gain }`.  
Raw: `TSL2571_RawData_t { ch0 (broadband), ch1 (IR), saturated }`.  
Ganancias: `TSL2571_GAIN_1X/8X/16X/120X`.

**API:**
- `TSL2571_Attach(dev, hi2c, addr, timeout_ms)` — binding
- `TSL2571_Begin(dev, atime, gain)` — power on + ALS enable
- `TSL2571_ReadLux(dev, nSamples, gapMs, &lux, &raw)` — promedio + conversión a lux
- `TSL2571_ReadRawChannels(dev, &raw)` — lectura directa CH0/CH1
- `TSL2571_SetGain / SetATime / Enable / Disable / WriteReg / ReadReg`
- `TSL2571_Test()` → `uint8_t` — I2C ACK + ch0 > 0 (TestHW; usa `extern TSL2571_t tsl`)

Fórmula integración: `Tint = 2.7296 × (256 − ATIME) ms`.

---

## Multiplexor

Controla el multiplexor analógico CD4051B de 8 canales mediante tres pines GPIO de selección (A/B/C). Permite conectar dinámicamente uno de ocho canales analógicos a la línea común, útil para leer múltiples sensores con un solo ADC.

**Archivo:** `Multiplexor_CD4051B.h` / `Multiplexor_CD4051B.c` · **Versión:** 1.0.0  
**Componente:** CD4051B multiplexor analógico 8 canales  
**Pines de selección:** A→PC5, B→PC6, C→PC8

8 canales (MUX_CHANNEL_0..7). Selección por código binario CBA en GPIO.  
Handle: `mux_handle_t { active_channel, initialized }`.  
Status: `MUX_OK / ERR_NULL_PARAM / ERR_INVALID_CH / ERR_NOT_INIT`.

**API:**
- `MUX_Init(hmux)` — init y selecciona canal 0
- `MUX_SelectChannel(hmux, channel)` — activa canal CBA
- `MUX_GetActiveChannel(hmux)` / `MUX_GetStatus(hmux)`

---

## IR_Laser

Transmite tramas IR a 40 kHz usando PWM por timer. Es el "cañón" del sistema: envía el ID del atacante codificado en hasta 8 bytes con checksum XOR. Simula el disparo en el juego táctico.

**Archivo:** `Transmsion_Laser_IR.h` / `Transmsion_Laser_IR.c` · **Versión:** 1.0.0  
**Componente:** Transmisor IR por laser (LED IR a 40 kHz)  
**Timer PWM:** `htim1`, canal `TIM_CHANNEL_1`, ARR=24, CCR=12 (50 % duty)  
**Timer delay µs:** `htim3`  
**Pin IR:** PE9

Protocolo (µs): MARK=500, SPACE0=600, SPACE1=1400, INTER=2500, SYNC=4000.  
Trama: hasta 8 bytes de datos + 1 byte XOR checksum.

**API:**
- `Tx_IR_Init()` — fuerza PE9 a GPIO_Output LOW (estado idle)
- `Tx_IR_SendFrame(pData, len)` — transmite trama completa con checksum XOR

---

## Bluetooth

Interfaz UART para módulos Bluetooth serie (HC-05/HC-06/HM-10). Permite transmitir y recibir datos de forma bloqueante, con acumulación byte a byte desde ISR. Se usa para la comunicación entre PCB1 (mira) y PCB2 (sensores).

**Archivo:** `Bluetooth.h` / `Bluetooth.c` · **Versión:** 1.0.0  
**Componente:** Módulo Bluetooth UART (HC-05/HC-06/HM-10)  
**UART:** `huart1` (deshabilitado en test — compartido con GPS)

Buffers TX/RX: 256 bytes. Handle: `Bt_Handle_t { huart, tx_buf, rx_buf, rx_count, rx_byte, rx_ready }`.

**API:**
- `Bt_Init(h)` — binding UART
- `Bt_Transmit(h, data, len)` — bloqueante
- `Bt_Receive(h, data, len)` — bloqueante
- `Bt_StoreByte(h)` — llamar desde ISR UART
- `Bt_ResetRx(h)` — limpia buffer de recepción
- `Bt_Test()` → `uint8_t` — envía `AT\r\n`, busca "OK" en respuesta con timeout (TestHW)

---

## Flash

Driver para memoria NOR Flash SPI MX25L6445E (8 MB). Ofrece tres niveles de API: comandos directos, operaciones seguras con manejo automático de páginas/Write Enable, y utilidades de diagnóstico. Se usa para almacenamiento persistente de configuración o logs.

**Archivo:** `Flash.h` / `Flash.c` · **Versión:** 1.0.0  
**Componente:** NOR Flash MX25L6445EZNI (Macronix 64 Mbit = 8 MB) via SPI  
**SPI:** `hspi1` · **CS:** PA8  
**JEDEC ID:** Manufacturer `0xC2`, Device `0x2017`

Geometría: página 256 B, sector 4 KB, bloque 32/64 KB. Rango: `0x000000–0x7FFFFF`.  
Regla crítica: Write solo vuelca 1→0; para volver 0→1 hay que borrar sector primero.

**Issue conocido:** velocidad SPI alta causa corrupción en lecturas. Usar `/16` o `/32
como prescaler para testing.

**API — Nivel 3 (utilidades):**  
`Flash_Init / ReadID / IsBusy / WaitBusy / UnprotectAll / PowerDown / WakeUp`

**API — Nivel 1 (comandos directos):**  
`Flash_ReadRaw / PageProgram / EraseSector / EraseBlock32K / EraseBlock64K / EraseChip`

**API — Nivel 2 (operaciones seguras):**  
`Flash_Read / Flash_Write` (maneja fragmentación de páginas)  
`Flash_ModifySector` — read-patch-erase-rewrite dentro de un sector (requiere 4 KB RAM)

**Self-test:**  
`Flash_Test()` → `uint8_t` — erase sector `0x7FF000` → write 16 × `0xA5` → read → compara → erase; retorna 1 si coincide

---

## LSM6DSO32TR

Driver para el IMU de 6 ejes ST LSM6DSO32TR (acelerómetro + giroscopio) via I2C. Entrega los seis ejes en dos transacciones burst de 6 bytes cada una, aplica calibración de bias del giroscopio y cuenta con recuperación automática por SW reset ante errores I2C. Soporta modo power-down (ODR=0) para bajo consumo.

**Archivo:** `LSM6DSO32TR.h` / `LSM6DSO32TR.c` · **Versión:** 1.0.0  
**Componente:** IMU 6 ejes ST LSM6DSO32TR (accel + gyro)  
**Bus:** I2C1 (`hi2c1`) · **Dirección:** `0x6A` (SA0=GND)

Configuración fija: accel ±16 g / gyro ±500 dps, ODR 104 Hz HP, BDU habilitado.  
WHO_AM_I esperado: `0x6C`.

Handle: `LSM6DSO32TR_t { cal, initialized, consecutive_errors, total_recoveries }`.  
Datos: `LSM_Data_t { ax/ay/az_raw, gx/gy/gz_raw, ax/ay/az_g, gx/gy/gz_dps, temp_c }`.  
Calibración: `LSM_Cal_t { gx/gy/gz_bias_dps, calibrated }`. Bias aplicado automáticamente en `ReadAll`.

**Recuperación automática I2C:** a los 3 errores consecutivos ejecuta SW reset + reconfiguración, preserva calibración.  
Bajo consumo: `PowerDown` escribe ODR=0 en CTRL1_XL y CTRL2_G; `PowerOn` restaura la configuración original.

**API:**
- `LSM6DSO32TR_Init(dev)` — SW reset + WHO_AM_I + configura accel/gyro/BDU
- `LSM6DSO32TR_WhoAmI(dev, &id)` — retorna 0x6C
- `LSM6DSO32TR_CalibrateGyroBias(dev)` — 200 muestras × 10 ms ≈ 2 s en reposo
- `LSM6DSO32TR_ReadAll(dev, &out)` — burst gyro + burst accel + temperatura, bias corregido
- `LSM6DSO32TR_Recover(dev)` — SW reset + reconfig, preserva cal
- `LSM6DSO32TR_PowerDown(dev)` — ODR=0 en ambos sensores (~5 µA)
- `LSM6DSO32TR_PowerOn(dev)` — restaura ODR 104 HP
- `LSM6DSO32TR_Test(dev)` — WHO_AM_I + una lectura completa

---

## MMC5983MA

Driver para el magnetómetro de 3 ejes MEMSIC MMC5983MA via I2C. Opera en modo continuo a 10 Hz con auto SET/RESET activado por hardware en cada medición. Entrega 18 bits de resolución por eje, detecta saturación automáticamente y emite un SET pulse de recuperación cuando es necesario.

**Archivo:** `MMC5983MA.h` / `MMC5983MA.c` · **Versión:** 1.0.0  
**Componente:** Magnetómetro 3 ejes MEMSIC MMC5983MA  
**Bus:** I2C1 (`hi2c1`) · **Dirección:** `0x30` (SA0=GND)

ODR activo: 10 Hz (definido en `MMC_ODR_ACTIVE`). Auto SET/RESET habilitado (CTRL0 bit 5).  
Resolución: 18 bits por eje (0–262143); cero de campo en `131072` (2^17). Sensibilidad: 163.84 counts/µT.  
Product ID esperado: `0x30`.

Datos: `MMC_Data_t { x/y/z_uT, x/y/z_raw }`.

**SET pulse:** aplicado en `Init` y automáticamente en cada muestra (modo auto). Si se detecta saturación en cualquier eje durante `ReadAll`, se emite un SET adicional y se relee.

**API:**
- `MMC5983MA_Init()` — SET pulse + verifica ID + activa modo continuo con auto SR
- `MMC5983MA_ReadAll(&out)` — burst de 7 bytes, ensambla 18 bits, convierte a µT; SET automático si satura
- `MMC5983MA_Set()` — SET pulse manual (desgausado), bloquea 1 ms
- `MMC5983MA_WhoAmI(&id)` — retorna 0x30
- `MMC5983MA_Test()` — WHO_AM_I + lectura + validación de rango ±800 µT

---

## BatteryMonitor

Monitorea el estado de la batería Li-Ion mediante el gauge BQ27441-G1 (I2C), que reporta voltaje, corriente, SOC, capacidad y SOH. Incluye también una lectura analógica secundaria via ADC conectada a un divisor resistivo.

**Archivo:** `BatteryMonitor.h` / `BatteryMonitor.c` · **Versión:** 1.0.0  
**Componente:** Gauge BQ27441-G1 (I2C) + lectura analógica ADC  
**I2C:** `hi2c1`, dirección 7-bit `0x55` · **ADC pot:** `hadc1`, `ADC_CHANNEL_0`  
**Batería diseño:** 400 mAh / 1480 mWh / terminate 3000 mV

`BatGauge_Data_t { voltage_mV, avg_current_mA, soc_pct, remaining_mAh, full_cap_mAh, soh_pct, temp_c10, flags }`.

**API:**
- `BatGauge_Init()` — verifica device type `0x0421`
- `BatGauge_ReadAll(&data)` — todos los registros clave
- `BatGauge_ReadReg(reg, &val)` — registro individual
- `BatGauge_Control(subcmd, &val)` — sub-comandos de control
- `BatGauge_SoftReset()` — reset suave (exit CFGUPDATE si queda atascado)
- `BatGauge_Configure()` — escribe parámetros de batería en NVM (solo una vez)
- `BatAdc_ReadVoltage_mV()` / `BatAdc_ReadRaw()` — lectura ADC del divisor

---

## Menu

Máquina de estados de 2 niveles para la interfaz de usuario del OLED 64×32. Gestiona 5 pantallas (menú principal, test HW, Bluetooth, programación, ejercicio) con 2 botones físicos. Incluye bloqueo por PIN para la pantalla de programación.

**Archivo:** `Menu.h` / `Menu.c` (+ `Menu_Screens.h`) · **Versión:** 1.0.0  
**Display:** SSD1306 OLED 64×32  
**Botones:** Navigate→PB11, Enter→PB12 (activo LOW), debounce 300 ms

Máquina de estados de 2 niveles:
- Nivel 1: `MenuScreen_e { SCREEN_MAIN_MENU, TEST_HW, BLUETOOTH, PROGRAMMING, EXERCISE }`
- Nivel 2: `sub_state` por pantalla (cada `.c` de pantalla gestiona el suyo)

Handle: `Menu_Handle_t { screen, selected, needs_redraw, sub_state, bt_connected, bt_device_id, lock_open, code_input[3], code_pos, splash_tick, last_btn_tick, flag_navigate (volatile), flag_enter (volatile) }`.

PIN de bloqueo (pantalla Programming): `MENU_LOCK_CODE = {1,2,3}`.

**API:**
- `Menu_Init(h)` — init handle y dibuja menú principal
- `Menu_Update(h)` — llamar en cada iteración del while; procesa flags y despacha pantalla activa
- `Menu_Poll(h)` — polling GPIO con debounce (modo test; producción usa EXTI ISR)
- `Menu_OnButton(h, btn)` — inyecta evento de botón desde ISR o polling
- `Menu_GoTo(h, screen)` — navega directamente a una pantalla, resetea sub_state

**Pantalla Test HW (`Menu_TestHW.c`):** implementada con sub-menú de dos niveles.
Ver `memorias_proyecto.md` → sección "Pantalla: Test Hardware" para la arquitectura completa
de sub_states, flujo automático/manual y menú deslizante.

---

## PowerManager

Coordinador de bajo consumo del sistema. Pone en modo sleep o despierta todos los periféricos que tienen un modo de bajo consumo, en el orden correcto. Actúa como capa de orquestación por encima de los drivers individuales.

**Archivo:** `PowerManager.h` / `PowerManager.c` · **Versión:** 1.1.0  
**Documentación:** `Core/Doc/BajoConsumo_Referencia.md`  
**Rama de desarrollo:** `feature/BajoConsumo`

ICs gestionados en esta tarjeta: Flash MX25L6445E, LSM6DSO32TR (IMU), TSL2571, SSD1306, BQ27441.  
BL654 Bluetooth: auto-sleep, sin comando.  
Actuadores (LED, Buzzer): se apagan incondicionalmente en Suspend.

> Se quitaron las referencias a GPS, ICM-20948, LP55231 (RGB I2C) y LoRa — no forman parte de esta tarjeta (Mira). El RGB aquí es `LedRGB` por GPIO; el IMU real es `LSM6DSO32TR` + `MMC5983MA` (el magnetómetro no tiene modo bajo consumo gestionado aún por `PowerManager`).

Handles registrados vía `PowerManager_Init()`: IMU (`LSM6DSO32TR_t`), TSL2571 (`TSL2571_t`).  
Flash, SSD1306, BQ27441, LedRGB, Buzzer usan handles internos de sus módulos.

**API:**
- `PowerManager_Init(himu, hlight)` — registra handles; llamar una vez tras todos los Init()
- `PowerManager_SuspendAll(&result)` — suspende todo; retorna `PM_OK` o `PM_ERR_PARTIAL`
- `PowerManager_WakeAll(&result)` — despierta todo en orden inverso
- `PowerManager_MCUSleep()` — **STUB — NO IMPLEMENTADO**

`PM_Result_t` tiene un bool por cada IC: `flash_ok, imu_ok, light_ok, display_ok, gauge_ok`.

**Pendientes:**
1. Implementar `PowerManager_MCUSleep()`: Stop mode + wakeup EXTI en USART1 RX (PA10). Requiere cambio en `.ioc` primero.
2. Evaluar si `MMC5983MA` (magnetómetro) necesita entrar también en bajo consumo dentro de `SuspendAll`/`WakeAll`.

---

## Display

Capa de dibujo sobre framebuffer para el OLED SSD1306 64×32 via I2C, basada en Adafruit GFX. Provee primitivas gráficas (píxeles, líneas, rectángulos, círculos, bitmaps) y funciones de texto con múltiples fuentes. El contenido se acumula en el buffer interno y se envía al display con `ssd1306_display()`.

**Archivos:** `Display_Comands.h`, `Display_Config.h`, `Display_Fonts.h`, `Display_Bitmaps.h`  
**Versión:** 1.0.0  
**Componente:** SSD1306 OLED 64×32 via I2C (basado en Adafruit GFX)

Framebuffer interno; `ssd1306_display()` flushea al hardware.  
Fonts disponibles: `Font3x5`, `Font4x6`, `Font5x7`, `Font6x8`.  
Colores: `BLACK(0)`, `WHITE(1)`, `INVERSE(2)`.

**API clave:**
- `ssd1306_begin(vccstate, i2caddr)` — init, usar `SSD1306_SWITCHCAPVCC`, `SSD1306_I2C_ADDRESS`
- `ssd1306_clearDisplay()` / `ssd1306_display()` — limpiar y flush
- `ssd1306_drawPixel / drawLine / drawRect / fillRect / drawCircle / fillCircle`
- `ssd1306_drawBitmap(x, y, bitmap[], w, h, color)` — bitmap 1-bit row-major MSB first
- `ssd1306_print(str, &font)` — escribe desde cursor actual
- `ssd1306_printCentered(str, y, &font)` — centrado horizontal en fila y
- `ssd1306_printCenter(str, &font)` — centrado horizontal y vertical
- `ssd1306_setCursor / setTextSize / setTextColor / setTextColorBg`
- `ssd1306_getStringWidth(str, &font)` — ancho en píxeles para calcular posiciones

Bitmaps disponibles en `Display_Bitmaps.h`: `bitmap_Logo_SIENT`, `bitmap_Candado_Abierto`,
`bitmap_Candado_Cerrado`, `bitmap_Modo_Ejercicio`.
