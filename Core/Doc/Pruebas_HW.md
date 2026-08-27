# Pruebas de Hardware — Tarjeta Mira

Lista de casos de prueba para validar cada módulo/driver en hardware real, uno por uno, directo desde `main.c` (fuera del Menu/Test HW todavía). Se marca el estado conforme se van confirmando en la tarjeta física.

Convención de estado: `Pendiente` / `En progreso` / `OK` / `Falla` (con nota de qué falló).

## Cómo se prueba

Cada prueba se agrega como bloque `PRUEBA N: <nombre>` dentro de `USER CODE 2` / `USER CODE WHILE` en `Core/Src/main.c`, uno a la vez o combinados cuando no interfieren entre sí. Se compila desde STM32CubeIDE, se flashea, y se confirma con el hardware real (LED a simple vista, UART con terminal serial, Programmer/debugger para leer registros, etc.).

---

## Lista de pruebas

| # | Módulo | Qué se prueba | Estado | Notas |
|---|--------|---------------|--------|-------|
| 1 | UART (Logger) + Modo Programación | Escribir por `huart1` vía `Log_Print()`, fijo en Modo MCU o Modo BT según la prueba | **OK** (BT y MCU ambos confirmados) | **Pendiente revisar a detalle el Modo MCU** — el mux no se tiene claro cómo está funcionando exactamente, hay que investigarlo con calma más adelante. |
| 2 | LedRGB | Ciclar R → G → B → apagado, 1 s cada uno (GPIO PB0/PB1/PB2) | **OK** | Confirmado a simple vista en la tarjeta. |
| 3 | Buzzer | Toca `ode_to_joy` completa una vez al boot (`Buzzer_PlayMelody`), TIM2 CH2 / PB3 | **OK** | Comparte `ARR`/`Prescaler` de `TIM2` con el láser — no probar ambas a la vez sin la reafirmación de `ARR` en el láser. |
| 4 | Transmision_Laser_IR | Transmite `"Cont1"`→`"Cont9"` (texto) cada 3 s, se reinicia después del 9. Mux CD4051B fijo en canal 6 (potencia IR). TIM2 CH3 / PA2 + TIM1 delay | En progreso | Receptor de prueba: Arduino + `Core/Doc/SNSOR_IR.txt` (Modo Decoder, filtra por prefijo `"Cont"` para descartar ruido de luz artificial). |
| 5 | SensorHall (Gatillo) | Leer `HallSensor_IsPressed()` en PA8 y confirmar que cambia al presionar/acercar el imán | Pendiente | Ya no es ADC — es lectura digital directa del comparador del HW-484. |
| 6 | Botones A / B | `BOTON_A` (PB11) presionado → LED rojo; `BOTON_B` (PB10) presionado → LED azul; ninguno → apagado | **OK** | Poll directo por `HAL_GPIO_ReadPin` en `main.c` (sin driver dedicado todavía), activo-bajo confirmado. |
| 7 | Bluetooth (BL654) - AT | Manda `"$AT\r"` cada 3 s por `huart1`, espera respuesta por poleo (`HAL_UART_Receive` bloqueante, 1000 ms) y la reporta por Logger | **Bloqueada** | El módulo BL654 no tiene firmware cargado todavía ("viene pelón"), así que no hay nada que responda `AT`. Código comentado en `main.c`, se retoma cuando el módulo tenga firmware. Reemplazada por la prueba 18 mientras tanto. |
| 8 | Flash (MX25L6445E) | Escribe `"ESTA ES UNA PRUEBA DE LA FLASH"` en `FLASH_TEST_ADDR` (0x001000) al boot; relee cada 3 s (no bloqueante) y lo manda por Logger, SPI2 + CS PA1 | **OK** | Se corrigió `SPI_DATASIZE_4BIT` → `8BIT` en `MX_SPI2_Init()` — ese era el bug que hacía que la lectura saliera vacía. |
| 9 | LSM6DSO32TR (IMU accel+gyro) | `LSM6DSO32TR_Init()` al boot + `LSM6DSO32TR_ReadAll()` cada 1 s (no bloqueante), imprime accel (g), gyro (dps) y temperatura por Logger, I2C1 (0x6A) | **OK** | Chip real es LSM6DS3 (`WHO_AM_I=0x69`), corregido en el driver. |
| 10 | MMC5983MA (Magnetómetro) | `MMC5983MA_Init()` al boot + `MMC5983MA_ReadAll()` cada 1 s (no bloqueante), imprime X/Y/Z en µT por Logger, I2C1 (0x30) | **OK (comunicación)** | Comunicación I2C confirmada (sin errores de status). **Pendiente:** algún eje da `-800 µT` (= `raw=0`, valor de piso/saturación, no una medición real) — revisar después si el modo continuo está midiendo de verdad. No bloquea el resto de las pruebas. |
| 11 | SensorLuz_TSL2571 | Imprimir por Logger cada lectura: `CH0`, `CH1`, `Lux`, `Sat` (saturación), I2C1 (0x39) | **OK** | Los canales cambian correctamente con luz ambiental. |
| 17 | Escaneo de bus I2C1 | Recorrer direcciones 1–126 con `HAL_I2C_IsDeviceReady()` cada 2 s, reportar cuáles responden ACK | **OK** | Causa real: el micro no se estaba reiniciando bien y dejaba el periférico I2C1 atorado (no era un corto de hardware). Ya responden los 5 dispositivos esperados: `0x30` (MMC5983MA), `0x39` (TSL2571), `0x55` (BQ27441), `0x6A` (LSM6DSO32TR), `0x3C` (SSD1306). |
| 12 | BatteryMonitor (BQ27441) | `BatGauge_Init()` al boot + `BatGauge_Update()` cada 3 s (no bloqueante), imprime voltaje, corriente, SOC, capacidad, SOH y estado de carga por Logger, I2C1 (0x55) | **OK** | La lectura ADC secundaria (divisor resistivo) queda fuera hasta que haya un ADC en el `.ioc`. |
| 13 | Display OLED (SSD1306) | `ssd1306_begin()` al boot + contador centrado 1→10 que se reinicia, cada 1 s (no bloqueante), I2C1 (0x3C) | **OK** | |
| 14 | Multiplexor_CD4051B | Ciclar los 8 canales (`MUX_SelectChannel`), uno cada 3 s (no bloqueante), confirmar con multímetro en el canal común | **OK** | Pines de selección A=PA7, B=PA4, C=PA3. Corre en paralelo con la prueba 6 (botones) en el mismo loop. |
| 15 | PowerManager | `PowerManager_SuspendAll()` / `WakeAll()` — confirmar que los periféricos manejados sí bajan/suben de consumo | Pendiente | Requiere que las pruebas 2, 9, 11, 13 ya funcionen individualmente primero. |
| 16 | Menu / Display_Oled + botones | Navegación completa del menú en la pantalla física con `BOTON_A`/`BOTON_B` | Pendiente | Depende de 6 y 13. |
| 18 | Programar el BT | Mux fijo en Modo BT (ya seteado en prueba 1) + LED parpadeando en rojo cada 2 s (no bloqueante) como testigo de que la tarjeta está viva | En progreso | El objetivo es que el compañero intente cargar algún firmware al BL654 por esta ruta, ya que viene sin nada de fábrica. |
| 19 | Bootloader (salto por software a USB DFU) | Mantener `BOTON_A` presionado al reset → LED rojo → salta a System Memory (`0x1FFF0000`); soltado → LED verde → boot normal. Verificar en PC que aparece un dispositivo DFU y que STM32CubeProgrammer lo detecta por USB | En progreso | Sin `BOOT0` expuesto en esta tarjeta, se usa el truco de salto por software (ver `Core/Inc/Bootloader.h`). Pendiente: agregar USB CDC (puerto COM) para cuando no se entra al bootloader. |
| — | Motovibrador | — | **No aplica** | Sin driver ni pin asignado en esta tarjeta todavía (ver `Menu_TestHW.c`, caso `MANUAL_VIBRADOR`). |

---

## Pendientes generales fuera de esta lista

- Agregar un ADC al `.ioc` si se retoma la lectura analógica de `SensorHall`/`BatteryMonitor`.
- Confirmar si se necesita un driver dedicado para `BOTON_A`/`BOTON_B`/`DISPARADOR` o si se maneja directo desde `Menu.c`.
