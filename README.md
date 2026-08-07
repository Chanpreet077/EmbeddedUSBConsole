# Embedded USB Developer Console

A handheld STM32-based device with a color display, rotary encoder navigation, and a Windows companion daemon — built to explore firmware fundamentals (SPI, I2C, UART, interrupts, state machines) end to end, from bare peripheral registers up through a working multi-screen UI and a bidirectional PC integration.

## Hardware

- STM32 Nucleo-F446RE
- ILI9341 SPI TFT display, 240x320, RGB565
- Rotary encoder with push-button (quadrature input)
- BMP180 I2C temperature/pressure sensor
- Onboard USER button (EXTI interrupt) for back-navigation
- USB (via ST-LINK virtual COM port) for PC communication

## Features

**Display**
- ILI9341 driver written from scratch: init sequence, address-window addressing, full-screen fill, 2x upscaled image rendering
- Programmatically generated 8x8 bitmap font, with a custom character and word-wrap string renderer
- Background images stored as compressed 120x160 RGB565 arrays, upscaled 2x at render time to fit the 512KB flash budget

**Navigation**
- Rotary-encoder-driven menu system with a cursor arrow, backed by a screen state machine (`AppScreen_t` enum + dispatch table)
- Generalized dirty-flag redraw system: a small queue of pending redraw callbacks (`MarkDirty()` / `ProcessDirtyQueue()`) replaces per-feature ad hoc flags, avoiding full-screen repaints (and the flicker that comes with them) on every input event
- USER button interrupt provides universal "back to menu" from any screen

**PC Connectivity (UART)**
- Interrupt-driven UART RX, line-buffered and parsed into a simple text protocol
- **Computer screen**: live CPU/RAM stats streamed from the PC once per second
- **Dev Tools screen**: STM32-triggered PC actions — BUILD (runs the project's build command), OPEN CODE (opens the project in VS Code), GIT STATUS (reports working-tree changes) — via a multithreaded Python daemon (`pyserial` + `psutil`)

**Sensors (I2C)**
- BMP180 driver implementing the full Bosch datasheet calibration/compensation algorithm from scratch (factory calibration read from sensor EEPROM, raw temperature/pressure conversion)
- **Environment screen**: on-demand sensor read (refreshes only when the screen is entered, not continuously)

## Architecture Notes

- **Why a callback queue instead of a framebuffer:** a full 240x320 RGB565 framebuffer would need 150KB — more RAM than the STM32F446RE has in total (128KB SRAM). Partial/dirty-region redraws were used instead of a full buffer for this reason.
- **Why interrupts for UART/USER button but polling for the encoder:** UART bytes and button presses can arrive at unpredictable times relative to the main loop, making interrupts the correct fit; the encoder is polled every loop pass instead, since quadrature decoding needs to sample both signal lines together at each step.
- **Why sensor init happens on screen entry, not at boot:** an early version initialized the I2C sensor during startup, before the display. A stuck I2C bus (missing pull-ups) froze the entire device before it could even show a UI. Moving sensor communication to only happen when the Environment screen is actually opened means a sensor fault can never block the rest of the device from working.

## Notable Bugs Found & Fixed

- **SPI CS-ordering bug**: address-window commands were being sent before the display's chip-select line was asserted, so they were silently ignored — screen stayed white despite "successful" SPI transfers.
- **DC/RST pin swap**: a wiring mixup swapped the display's data/command and reset lines, which looked like a firmware bug but was purely physical.
- **RGB565 endianness bug**: an image-drawing function cast a pixel buffer directly to bytes for a fast bulk SPI transfer, which sent each pixel's bytes in the STM32's little-endian storage order instead of the display's expected big-endian wire order — corrupting every color. Fixed by byte-swapping before buffering, preserving the bulk-transfer performance.
- **Missing HAL module enable**: enabling a new peripheral (UART) in code without enabling `HAL_UART_MODULE_ENABLED` in `stm32f4xx_hal_conf.h` (and later, without the driver files being present until regenerated via `.ioc`) produced a wall of "unknown type" errors that pointed to a config/build issue, not a logic bug.
- **I2C bus lockup**: `HAL_MAX_DELAY` timeouts combined with missing I2C pull-up resistors caused indefinite hangs at boot; fixed with bounded timeouts, moving sensor init off the boot path, and enabling internal pull-ups.

## PC Companion (`pc_console_daemon.py`)

- Requires `pyserial`, `psutil`
- Runs a background thread for reading STM32 commands (non-blocking relative to the stats-sending loop)
- Configurable project path / toolchain path for the BUILD action (STM32CubeIDE's bundled ARM toolchain is not on the system PATH by default)

## Not Yet Implemented

- DMA-based SPI transfers (currently blocking `HAL_SPI_Transmit`)
- RTOS (FreeRTOS was part of the original stretch goals)
- True ring buffer for UART RX (current buffer is a simple fixed-size line buffer)
- Non-volatile settings storage (on-chip flash)
- Formalized interrupt/timer-based debouncing (current button debounce is delay-based)
- Custom PCB (currently breadboard prototype)
- Animated boot screen, themes, notification center, RGB status LED

## Build & Run

**Firmware**: STM32CubeIDE, build and flash normally via ST-LINK.

**PC daemon**:
```
pip install pyserial psutil
python pc_console_daemon.py
```
Set `COM_PORT` and `PROJECT_FOLDER` at the top of the script to match your machine before running.