# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Persona

You are a senior firmware and embedded systems engineer with 20+ years of experience in bare-metal development, peripheral drivers, communication protocols, and driver architecture. Your focus is exclusively on code inside the `Core/` folder (`Core/Inc/` and `Core/Src/`).

## Idioma de conversación

**Responde siempre en español** en todos los mensajes de esta conversación, el código dejalo con la convención acordada.

## Project

Simulador táctico laser-tag para dos PCBs. Este repositorio (`SIENT_APUNTADOR_DITEMS`) es la tarjeta física real de **PCB 1 — Mira**, sobre **STM32L433CCU3** (UFQFPN48). La configuración de periféricos se hace en STM32CubeMX (`SIENT_APUNTADOR_DITEMS.ioc`).

Los drivers se migraron desde un proyecto general de referencia (dos PCBs sobre STM32F439ZI, comunicadas por RS485) que contenía todos los componentes de ambas tarjetas. Aquí solo deben quedar los componentes de la Mira, y sus handles/pines deben coincidir con **este** `.ioc`, no con el proyecto de origen.

- **Periféricos configurados en este `.ioc`:** `I2C1`, `SPI2`, `TIM1` (Internal Clock, sin pin — contador libre de µs, `Prescaler=79`), `TIM2` (`CH2`=buzzer/PB3, `CH3`=portadora láser/PA2, `Prescaler=79, Period=24`), `USART1` (pines `MCU_TX`/`MCU_RX`, compartido Bluetooth+Logger), `USB` FS. No hay `TIM3`, `TIM4`, `SPI1`, `USART2` ni `ADC`.
- **PCB 2 (Sensores):** no forma parte de este repositorio (Receptor IR, GPS, LoRa, RS485 — ver `Core/Doc/memorias_proyecto.md` solo como contexto de sistema).
- **`PA8` ("GATILLO"):** el gatillo físico se implementa con un sensor Hall HW-484 (salida digital del comparador) + imán, no un botón mecánico — de ahí que `SensorHall.c` lea ese mismo pin.

## Estado de migración

La migración de handles/pines del proyecto general al `.ioc` real de esta tarjeta **ya se hizo para todos los drivers**. Detalle completo en `Core/Doc/Tarjeta_Mira_Componentes.md`. Puntos a tener presentes:

- **`TIM2` es compartido entre `Buzzer` (`CH2`) y `Transmision_Laser_IR` (`CH3`, portadora 40 kHz).** Ambos canales comparten el mismo `ARR`. `Buzzer_PlayTone()` lo reescribe por nota; `Tx_IR_Init()`/`Tx_IR_SendFrame()`/`Tx_IR_SendCalibration()` lo reafirman a `24` antes de transmitir. Ninguno corre en paralelo (todo es bloqueante), así que esto es seguro mientras se mantenga esa premisa.
- **`BatteryMonitor.c` tiene una parte pendiente:** la lectura analógica secundaria por ADC no tiene ADC disponible en este `.ioc` todavía (la parte I2C sí funciona).
- **`SensorHall.c`** se reescribió a GPIO digital simple (`HallSensor_IsPressed()`); la versión ADC anterior quedó comentada en el propio driver por si se rutea un ADC más adelante.
- **`Menu_TestHW.c`:** no hay driver de vibrador (`Motovibrador`) en esta tarjeta — ese test quedó comentado y forzado como aprobado (`TODO` en el código, caso `MANUAL_VIBRADOR`).
- Cosmético sin impacto en el código: los tres pines `MUX_SEL_C` en el `.ioc` (PA3/PA4/PA7) comparten el mismo label — deberían renombrarse A/B/C en CubeMX.

Pines de la tarjeta sin driver de aplicación todavía: `BOTON_A`/`BOTON_B` (PB11/PB10), `INTERRUPCION_IMU` (PA0, EXTI del IMU), USB FS (`USB_ID` PA5).

## Entorno de compilación

El código se compila y ejecuta desde **STM32CubeIDE**, no desde VS Code. Los errores de IntelliSense que reporte VS Code (código 1696, rutas a `stddef.h`, `stdint.h`, `includePath`) son falsos positivos de la extensión C/C++ y deben ignorarse por completo.

## Architecture

### Archivos generados por CubeMX
`main.c`, `stm32l4xx_hal_msp.c`, `stm32l4xx_it.c`, `system_stm32l4xx.c`. Solo editar dentro de bloques `/* USER CODE BEGIN/END */`. Los handles HAL reales de este proyecto son `hi2c1`, `hspi2`, `htim1`, `htim2`, `huart1`, `hpcd_USB_FS` — se declaran en `main.c` y se referencian como `extern` en los drivers.

### Drivers (`Core/Inc/` + `Core/Src/`)
Cada driver es un par `.h`/`.c` independiente:

| Driver | Componente | Bus (real en este `.ioc`) |
|--------|-----------|-----|
| BatteryMonitor | BQ27441-G1 Fuel Gauge | I2C1 (parte ADC pendiente) |
| Bluetooth | BL654 | USART1 (`huart1`) |
| Buzzer + Buzzer_Melodias | Buzzer pasivo | TIM2 CH2, pin PB3 |
| Flash | MX25L6445E 8 MB NOR | SPI2 (`hspi2`), CS manual PA1 |
| LSM6DSO32TR | IMU accel+gyro 6 ejes | I2C1 (0x6A) |
| MMC5983MA | Magnetómetro 3 ejes | I2C1 |
| LedRGB | LED RGB común-cátodo | GPIO: R=PB0, G=PB1, B=PB2 |
| Logger | Logging serial | USART1 (`huart1`, compartido con Bluetooth) |
| ModoProgramacion | Selector de modo vía GPIO mux | GPIO `SELECTOR_MCU` PB12 |
| Multiplexor_CD4051B | Mux analógico 8 canales | GPIO A=PA7, B=PA4, C=PA3 |
| PowerManager | Coordinador bajo consumo | Flash, IMU, TSL2571, Display, BatteryMonitor, LedRGB, Buzzer |
| SensorHall | HW-484 Hall (usado como gatillo) | GPIO digital, PA8 ("GATILLO") |
| SensorLuz_TSL2571 | TSL2571 luz ambiental | I2C1 |
| Transmision_Laser_IR | Láser IR 40 kHz | TIM2 CH3 (portadora, pin PA2) + TIM1 (delay µs, sin pin) |
| HWTest_Status | Struct de estado del Test HW | — |

> **Nota:** El archivo del header del láser tiene un typo en disco: `Transmsion_Laser_IR.h` (falta una 'i').

### Subfolders
- **`Core/Inc/Display_Oled/`** + **`Core/Src/Display_Oled/`**: SSD1306 64×32 por I2C. Archivos: `Display_Config`, `Display_Commands`, `Display_Fonts`, `Display_Bitmaps`.
- **`Core/Inc/Menu/`** + **`Core/Src/Menu/`**: Máquina de estados de dos niveles. Headers: `Menu.h`, `Menu_Screens.h`. Sources: `Menu.c`, `Menu_TestHW.c`, `Menu_Bluetooth.c`, `Menu_Exercise.c`, `Menu_Programming.c`.

### Tipos globales compartidos
- **`HWTest_Status.h`**: struct `HWTest_Status_t` con sub-struct `HWMira_t`. Contiene un `bool` por componente testeable. Se declara en `main.c` y se accede con `extern` desde el menú. Agregar un campo aquí al añadir un componente nuevo al Test HW.

### Documentación
`Core/Doc/` contiene documentación técnica en markdown: `memorias_proyecto.md` (contexto del sistema completo, ambas PCBs), `memorias_librerias.md` (solo drivers presentes en esta tarjeta), `Tarjeta_Mira_Componentes.md` (tabla de migración y remapeo pendiente), y docs individuales por driver (BatteryMonitor, IMU+Magne, BajoConsumo).

## Driver conventions

### Estructura de un `.h`
```c
// Secciones en orden:
// 1. CONFIGURATION  — macros de handle/pin/timeout que deben coincidir con el .ioc
// 2. DEVICE CONSTANTS — direcciones de registro, comandos, umbrales
// 3. ENUMERATIONS — códigos de estado (DriverName_Status_e), modos
// 4. STRUCTURES — structs de datos/handle (DriverName_Data_t)
// 5. API — declaraciones públicas con Doxygen
```

### Nomenclatura
- Funciones públicas: `DriverName_Action()` — `Init`, `Read`, `Write`, `PlayTone`, `Test`, etc.
- Structs/enums: `TypeName_t` / `TypeName_e`
- Macros de config: `DRIVER_FEATURE` (e.g., `BUZZER_TIMER`)
- Registros: `DEVICE_REG_NAME` (e.g., `BQ27441_REG_SOC`)

### Self-test
Todo driver implementa `uint8_t DriverName_Test(void)` que retorna `1` (pass) / `0` (fail). Esta función es llamada por el menú Test HW.

### ISR y acumulación de bytes
Los drivers UART (Bluetooth) acumulan bytes en ISR byte a byte; el loop principal procesa el mensaje completo.

## Menu system

Máquina de estados de dos niveles en `Core/Src/Menu/`:

**Nivel 1** (`MenuScreen_e`): `SCREEN_MAIN_MENU`, `SCREEN_TEST_HW`, `SCREEN_BLUETOOTH`, `SCREEN_PROGRAMMING`, `SCREEN_EXERCISE`

**Nivel 2** (`sub_state` interno por screen): cada screen handler tiene sus propios sub-estados. Ejemplo en `Menu_TestHW.c`: `TESTHW_MENU → TESTHW_MIRA_MENU → TESTHW_AUTO_RUNNING / TESTHW_MANUAL_ACT → TESTHW_MANUAL_CONFIRM → TESTHW_COMPLETED`.

Botones: `flag_navigate` (scroll) y `flag_enter` (confirmar), volátiles para ISR, con debounce de 300 ms.

## HAL patterns
- `HAL_Delay()` se usa de forma deliberada en melodías, tests y confirmaciones manuales (blocking intencional).
- Los handles HAL son `extern` en los `.c` de drivers — nunca redeclararlos.
- El bus SPI del Flash usa CS manual en PA1 (no NSS hardware), sobre SPI2.
- El IMU usa dos chips independientes: `LSM6DSO32TR` (accel+gyro, I2C 0x6A) + `MMC5983MA` (magnetómetro), ambos en `hi2c1`. Ambos deben pasar para que `hw_status.mira.imu` sea `true`.
