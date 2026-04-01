# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

ESP32-based VFO (Variable Frequency Oscillator) controller for amateur radio. Built on ESP-IDF 5.x, managed via PlatformIO. Generates RF signals via Si5351A and displays an analog dial on an ST7735 LCD.

## Build Commands

```bash
pio run                  # build
pio run -t upload        # flash to ESP32
pio device monitor       # serial monitor (115200 baud, exception decoder active)
pio run -t clean         # clean build artifacts
```

No tests exist in this project.

## Architecture

### FreeRTOS Task Structure

Four concurrent tasks — all share `g_vfo` state protected by a binary semaphore:

| Task | Core | Priority | Period | Role |
|------|------|----------|--------|------|
| `encoder` | 0 | 5 | 20ms | Reads PCNT counter, applies dynamic acceleration, updates `g_vfo.freq` |
| `display` | 0 | 3 | 10ms | Renders UI → graph → ST7735 via SPI; calls `si5351_set_freq()` |
| `buttons` | 1 | 4 | 10ms | GPIO debounce (50ms), short/long press logic, mode transitions |
| `autosave` | 1 | 2 | event | Writes frequency + memory banks to NVS after 3s idle |

State access pattern: always `VFO_LOCK()` / `VFO_UNLOCK()` macros around `g_vfo` reads/writes.

### Signal Generation (Si5351A)

Dual-PLL strategy to avoid glitches during tuning:
- **PLL_A** (fixed): CLK0 = Carrier I (0°), CLK2 = Carrier Q (90° inverted)
- **PLL_B** (variable): CLK1 = LO/VFO output — this is the tunable output

Register writes are cached; only changed registers are sent over I2C (addr `0x60`, 400 kHz on GPIO 21/22).

### Display Pipeline

`dial.c` / `graph.c` → RGB framebuffers (`R_GRAM`, `G_GRAM`, `B_GRAM`, 160×128) → `display_transfer_image()` converts to RGB565 and pushes to ST7735 via SPI (GPIO 18/23/5/2/15, 27 MHz).

`ui_overlay.c` draws dialogs on top of the dial framebuffer (lock icon, save/load prompts, band menu).

### Key Configuration (`include/config.h`)

All hardware pin assignments, frequency limits (100 kHz – 225 MHz), step sizes (10 Hz – 1 MHz, 6 levels), default memory banks, task stack/priority constants, and autosave delay are defined here. Crystal calibration: `24,999,250 Hz`.

### Display Modes (`disp_mode_t` in `vfo_state.h`)

`DISP_MODE_VFO` → `DISP_MODE_MEM` → `DISP_MODE_SAVE_PROMPT` → `DISP_MODE_SAVE_OK` / `DISP_MODE_LOAD_OK` / `DISP_MODE_BAND_MENU` / `DISP_MODE_LOCK`

Mode transitions are driven by button logic in `buttons.c` and read by `display_task` in `main.c`.

### Encoder Acceleration (`encoder.c`)

Dynamic multiplier applied to the current step size based on PCNT pulses per 20ms cycle:
- ≥12 pulses/cycle → ×1000
- ≥5 → ×100
- ≥2 → ×10
- else → ×1

Short press on encoder button resets multiplier; long press toggles direction.

## ESP-IDF Notes

- Uses PCNT v2 API (ESP-IDF 5.x) for quadrature decoding — not the legacy `pcnt_unit_config_t` API.
- FreeRTOS tick rate is 1000 Hz (set in `sdkconfig.defaults`) — critical for 50ms debounce accuracy.
- NVS namespace: `"vfo"` — stores `last_freq` and `mem_0` through `mem_9`.
