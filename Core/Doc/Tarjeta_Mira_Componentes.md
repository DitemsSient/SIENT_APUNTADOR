# Tarjeta Mira — Componentes migrados

Listado de drivers movidos al proyecto `SIENT_APUNTADOR_DITEMS`, la tarjeta física real de la Mira (**STM32L433CCU3**, no el STM32F439ZI del proyecto general de referencia). Sirve como base para las pruebas de funcionamiento general.

> **MCU real confirmado por el `.ioc`:** STM32L433CCU3 (UFQFPN48). Periféricos disponibles: `I2C1`, `SPI2`, `TIM1`, `TIM2`, `USART1`, `USB`. No hay `TIM3`, `TIM4`, `SPI1`, `USART2` ni `ADC`.
>
> **Migración de pines/handles: completa.** Todos los drivers de la tabla ya apuntan a los handles y pines reales de este `.ioc`. Único pendiente real: `MUX_SEL_C` sigue repetido como label de tres pines distintos en el `.ioc` (PA3/PA4/PA7) — cosmético, no bloquea nada, pero vale la pena renombrarlos A/B/C en CubeMX para que no confunda después.

## Carpetas

| Carpeta | Contenido | Componente físico |
|---|---|---|
| `Display_Oled/` | Display_Config, Display_Commands, Display_Fonts, Display_Bitmaps | OLED SSD1306 64×32 (I2C) |
| `Menu/` | Menu, Menu_TestHW, Menu_Bluetooth, Menu_Exercise, Menu_Programming | Máquina de estados del menú |

## Drivers individuales

| Archivo | Componente físico | Bus/Periférico real usado | Estado |
|---|---|---|---|
| `BatteryMonitor.c` | BQ27441-G1 Fuel Gauge | I2C1 (`hi2c1`); lectura analógica secundaria por ADC | ⚠️ parte I2C OK; parte ADC pendiente (no hay ADC en el `.ioc`) |
| `Bluetooth.c` | BL654 | USART1 (`huart1`) | ✅ OK |
| `Buzzer.c` | Buzzer pasivo | TIM2 CH2 (`htim2`), pin PB3 | ✅ remapeado — comparte `TIM2`/`ARR` con el láser (ver `Transmision_Laser_IR.c`) |
| `Flash.c` | MX25L6445E 8 MB NOR | SPI2 (`hspi2`), CS manual PA1 | ✅ remapeado |
| `LedRGB.c` | LED RGB común-cátodo | GPIO: R=PB0, G=PB1, B=PB2 | ✅ remapeado |
| `Logger.c` | Logging serial | USART1 (`huart1`, compartido con Bluetooth) | ✅ remapeado |
| `LSM6DSO32TR.c` | IMU accel+gyro 6 ejes | I2C1 (0x6A) | ✅ OK |
| `MMC5983MA.c` | Magnetómetro 3 ejes | I2C1 | ✅ OK |
| `ModoProgramacion.c` | Selector de modo vía GPIO mux | GPIO `SELECTOR_MCU` PB12 | ✅ remapeado |
| `Multiplexor_CD4051B.c` | Mux analógico 8 canales | GPIO A=PA7, B=PA4, C=PA3 | ✅ remapeado (labels `MUX_SEL_C` x3 en el `.ioc` siguen iguales, cosmético) |
| `PowerManager.c` | Coordinador bajo consumo | Flash, IMU, TSL2571, Display, BatteryMonitor, LedRGB, Buzzer | ✅ limpiado — se quitaron GPS/ICM-20948/LP55231/LoRa |
| `SensorHall.c` | HW-484 Hall (usado como gatillo) | GPIO digital, PA8 ("GATILLO") | ✅ remapeado a GPIO simple; versión ADC queda comentada por si se rutea un ADC después |
| `SensorLuz_TSL2571.c` | TSL2571 luz ambiental | I2C1 | ✅ OK |
| `Transmision_Laser_IR.c` | Láser IR 40 kHz (transmisor) | TIM2 CH3 (`htim2`, pin PA2) portadora + TIM1 (`htim1`, internal clock) delay µs | ✅ remapeado — `ARR` compartido con el buzzer, el driver lo reafirma en `Init`/`SendFrame`/`SendCalibration` |

> **Nota:** el header de este driver tiene un typo en disco: `Transmsion_Laser_IR.h` (falta una 'i').
>
> Pendiente aparte (no de migración): `Menu_TestHW.c` no tiene driver de vibrador (`Motovibrador`) en esta tarjeta — el test correspondiente quedó comentado y forzado como aprobado (`TODO` en el código, ver caso `MANUAL_VIBRADOR`).

## Archivos generados / de proyecto

- `main.h`
- `stm32l4xx_hal_conf.h`
- `stm32l4xx_it.h`

## Pendiente

- Decidir si se agrega un `ADC` al `.ioc` para la lectura analógica de `BatteryMonitor`.
- Renombrar en CubeMX los tres pines `MUX_SEL_C` (PA3/PA4/PA7) a labels A/B/C distintos — puramente cosmético en el `.ioc`, el código ya usa el pin correcto para cada uno.
- Decidir si se agrega un driver de vibrador o se quita esa opción del Test HW.
