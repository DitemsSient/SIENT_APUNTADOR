# Memorias del Proyecto

## Contexto General

Sistema táctico que simula gotcha mediante impactos por transmisión láser (IR). El sistema está compuesto por dos PCBs que trabajan en conjunto, montadas sobre el usuario, y se comunican entre sí por Bluetooth.

> **Este repositorio (`SIENT_APUNTADOR_DITEMS`) es la tarjeta Mira real**, sobre **STM32L433CCU3** (no STM32F439ZI — ese MCU corresponde solo al proyecto general de referencia del que se migraron los drivers). El contenido de este archivo describe el sistema completo (ambas PCBs) como contexto; el código de `Core/` de este repo cubre únicamente PCB 1 — Mira.

---

## PCB 1 — Mira (Frontal)

Función principal: **transmitir un mensaje a través de un LED IR** con el ID del atacante, simulando el disparo/bala.

Componentes y funciones clave:
- **LED IR**: Emisor del "disparo". Transmite un paquete con el ID del jugador atacante.
- **Pantalla OLED**: Proyecta diferentes menús antes y durante el ejercicio (configuración, estado del juego, HUD).
- **Bluetooth**: Comunicación con la PCB 2 (sensores/recepción).
- **Sensores y actuadores complementarios**: LEDs RGB, vibrador, sensor de luz, IMU, entre otros. No críticos para la funcionalidad principal.

---

## PCB 2 — Sensores / Recepción (Trasera)

Función principal: **recibir impactos IR, geolocalizar al usuario y transmitir datos en tiempo real**.

Componentes y funciones clave:
- **Sensor IR**: Recibe el impacto láser y decodifica el ID del atacante.
- **Módulo GPS (L86-M33)**: Ubicación exacta del usuario en tiempo real.
- **Módulo LoRa**: Transmite información del juego a base de datos remota.
- **Módulo Bluetooth**: Comunicación con la PCB 1.
- **RS485**: Comunicación con PCB de respaldo trasera (impactos por espalda, micro independiente).

---

## Sistema de Menús OLED (PCB 1 — Mira)

Estado: **boceto / diseño de pantallas**. Solo estructura visual y navegación. La lógica de ejecución real se implementará después.

Navegación: 2 botones físicos — **Navigate** (desplazar cursor) y **Enter** (seleccionar).
Font: 5x7 (mayúsculas). Display: OLED con librería Display_Oled existente.
Arquitectura: máquina de estados de 2 niveles (pantalla global + sub-estado interno por pantalla).

### Menú Principal (4 opciones)

Título centrado de cada opción con indicador visual de scroll (arriba/abajo). El botón Navigate recorre las opciones cíclicamente, Enter entra a la pantalla seleccionada.

| # | Opción | Pantalla destino |
|---|--------|-----------------|
| 1 | TEST HARDWARE | SCREEN_TEST_HW |
| 2 | CONEXION BLUETOOTH | SCREEN_BLUETOOTH |
| 3 | MODO PROGRAMACION | SCREEN_PROGRAMMING |
| 4 | INICIAR EJERCICIO | SCREEN_EXERCISE |

### Pantalla: Test Hardware

Menú de **dos niveles** para diagnosticar hardware por PCB. Extensible: agregar una PCB nueva
es solo añadir una entrada en el nivel 1 e implementar su submenu.

**Nivel 1 — Lista de PCBs** (`sub_state = TESTHW_MENU`):

| Opción | Acción al Enter |
|--------|----------------|
| Mira | Entra al submenu de la PCB Mira |
| Salir | Regresa al menú principal |

**Nivel 2 — Suite Mira** (`sub_state = TESTHW_MIRA_MENU`):

Menú deslizante de 4 ítems visibles con indicadores `^`/`v` en esquinas. Cursor gestionado
por variable estática `mira_selected` + `mira_offset`.

| Opción | Tipo | Comportamiento |
|--------|------|----------------|
| Sensores | Automático | Ejecuta 5 tests en secuencia, muestra nombre + "..." + resultado, finaliza con "Completado" 2 s y regresa solo |
| Buzzer | Manual | Tono A4 continuo 5 s → pantalla "Funciono? SI/NO" → "Completado!" 2 s |
| Vibrador | Manual | Motor ON 5 s → pantalla "Funciono? SI/NO" → "Completado!" 2 s (⚠️ no existe driver `Motovibrador` en este repo) |
| RGB | Manual | Ciclo R→G→B × 2 (1.5 s/color = 9 s) → "Funciono? SI/NO" → "Completado!" 2 s (esta tarjeta usa `LedRGB` por GPIO, no LP55231) |
| Salir | — | Regresa al nivel 1 |

**Suite Sensores (automática):** Flash → Hall → Luz Amb. → RS485 → Bluetooth.
Cada test muestra nombre + "...", espera 1 s, ejecuta la función `_Test()` del driver,
muestra "Correct" o "Fail" 1 s. Las llamadas reales están comentadas; `ok = 1U` es simulado
hasta conectar el hardware.

**Confirmación manual (SI/NO):** `BTN_NAVIGATE` alterna entre SI y NO. `BTN_ENTER` confirma
y transiciona a "Completado!" automático.

**Sub-states internos:**

| Valor | Nombre | Descripción |
|-------|--------|-------------|
| 0 | TESTHW_MENU | Lista nivel 1 |
| 1 | TESTHW_MIRA_MENU | Lista nivel 2 |
| 2 | TESTHW_AUTO_RUNNING | Ejecutando sensores (bloqueante) |
| 3 | TESTHW_MANUAL_ACT | Activando dispositivo manual (bloqueante) |
| 4 | TESTHW_MANUAL_CONFIRM | Esperando confirmación SI/NO |
| 5 | TESTHW_COMPLETED | "Completado!" 2 s, regreso automático |

### Pantalla: Conexión Bluetooth

Pantalla condicional — cambia según el estado de `bt_connected`:

**Si NO está conectado:**

| Opción | Acción |
|--------|--------|
| CONECTAR | Placeholder: "AUX CONECTANDO" |
| BACK | Regresa al menú principal |

**Si YA está conectado:**

| Opción | Acción |
|--------|--------|
| CONECTADO: [ID] | (solo informativo, no seleccionable) |
| NUEVA CONEXION | Desconecta y reconecta. Placeholder: "AUX RECONECTANDO" |
| BACK | Regresa al menú principal |

Alcance futuro: pairing BT real, mostrar ID del dispositivo emparejado, gestión de desconexión.

### Pantalla: Modo Programación

Pantalla con mecanismo de seguridad. Al entrar se muestra un **bitmap de candado cerrado** centrado arriba y las opciones abajo. Para acceder a las opciones, se debe ingresar una clave usando los 2 botones (Navigate = cambiar dígito, Enter = confirmar dígito). Si la clave es correcta, el candado se abre (bitmap abierto) y las opciones se desbloquean. Al salir, el candado se cierra automáticamente.

| Opción | Acción (solo si desbloqueado) |
|--------|-------------------------------|
| MICROCONTROLADOR | Placeholder: "AUX MICRO" |
| BLUETOOTH | Placeholder: "AUX BT PROG" |
| BACK | Regresa al menú principal (cierra candado) |

Alcance futuro: modo DFU/programación del micro, configuración avanzada del BT, actualización de firmware.

### Pantalla: Iniciar Ejercicio

Pantalla condicional — requiere `bt_connected == true` para funcionar.

**Si BT NO está conectado:**
Muestra centrado: "ESTABLECE CONEXION BLUETOOTH PRIMERO". Solo permite regresar con Navigate + Enter en Back implícito.

**Si BT está conectado:**
- Sub-estado ESPERANDO: bitmap de ejercicio centrado + texto "ESPERANDO..." abajo. Espera señal de inicio por LoRa.
- Sub-estado ACTIVO (al recibir señal LoRa): HUD del juego con vidas, balas, porcentaje de batería. El porcentaje de batería se muestra desde el estado de espera.

Alcance futuro: recepción de señal LoRa para inicio, actualización en vivo de vidas/balas, lógica de game over.
