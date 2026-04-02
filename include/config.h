/*===========================================================================
 *  config.h — VFO Controller (ESP-IDF 5.x / PlatformIO)
 *===========================================================================*/
#pragma once

#include <stdint.h>
#include "esp_err.h"

/*---------------------------------------------------------------------------
 *  GPIO — Enkoder obrotowy
 *--------------------------------------------------------------------------*/
#define ENC_PIN_A       17
#define ENC_PIN_B       16

/*---------------------------------------------------------------------------
 *  GPIO — Przyciski (active LOW, INPUT_PULLUP)
 *--------------------------------------------------------------------------*/
#define BTN_STEP_DN     26
#define BTN_STEP_UP     27
#define BTN_MEM         25
#define BTN_SAVE        32
#define BTN_BAND        33      /* 5. przycisk — wybor pasma, GPIO z pull-up */

/*---------------------------------------------------------------------------
 *  GPIO — Si5351A (hardware I2C)
 *--------------------------------------------------------------------------*/
#define SI5351_SDA          21
#define SI5351_SCL          22
#define SI5351_I2C_ADDR     0x60
#define SI5351_I2C_FREQ     400000
#define SI5351_XTAL_FREQ    24999250UL

/*---------------------------------------------------------------------------
 *  GPIO — Wyswietlacz ST7735 128x160 (SPI)
 *--------------------------------------------------------------------------*/
#define DISP_SCLK       18
#define DISP_MOSI       23
#define DISP_CS          5
#define DISP_DC          2
#define DISP_RST        15
#define DISP_SPI_FREQ   27000000

/*---------------------------------------------------------------------------
 *  Wymiary wyswietlacza
 *--------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
 *  Czestotliwosci VFO
 *--------------------------------------------------------------------------*/
#define VFO_FREQ_INIT   14230000UL
#define VFO_FREQ_MAX    225000000UL
#define VFO_FREQ_MIN       100000UL

/*---------------------------------------------------------------------------
 *  Si5351 CLK outputs
 *--------------------------------------------------------------------------*/
#define SI5351_CLK_LO    1   /* CLK1 = LO (VFO, PLL_B) */
#define SI5351_CLK_CAR_I 0   /* CLK0 = nosna glowna I (PLL_A, 0 deg) */
#define SI5351_CLK_CAR_Q 2   /* CLK2 = nosna Q (PLL_A, 90 deg, INV) */

/*---------------------------------------------------------------------------
 *  Kroki czestotliwosci — makra zamiast tablic (zero problemow z ODR)
 *--------------------------------------------------------------------------*/
#define FREQ_STEP_COUNT   6
#define FREQ_STEP_DEFAULT 2

#define FREQ_STEP_0       10
#define FREQ_STEP_1       100
#define FREQ_STEP_2       1000
#define FREQ_STEP_3       10000
#define FREQ_STEP_4       100000
#define FREQ_STEP_5       1000000

/* Tablica krokow — dostepna w runtime (static = jedna kopia na TU) */
static const int32_t FREQ_STEPS[FREQ_STEP_COUNT] = {
    FREQ_STEP_0, FREQ_STEP_1, FREQ_STEP_2,
    FREQ_STEP_3, FREQ_STEP_4, FREQ_STEP_5
};

/*---------------------------------------------------------------------------
 *  Pamiec VFO
 *--------------------------------------------------------------------------*/
#define VFO_MEM_COUNT   10

#define VFO_MEM_0   14230000UL
#define VFO_MEM_1    3730000UL
#define VFO_MEM_2    3853100UL
#define VFO_MEM_3    5450000UL
#define VFO_MEM_4    5505000UL
#define VFO_MEM_5    6070000UL
#define VFO_MEM_6    7130000UL
#define VFO_MEM_7    7878100UL
#define VFO_MEM_8    8957000UL
#define VFO_MEM_9   10100000UL

/*---------------------------------------------------------------------------
 *  Dynamiczny enkoder
 *--------------------------------------------------------------------------*/
#define ENC_ACCEL_FAST_THR   5
#define ENC_ACCEL_VFAST_THR  15

/*---------------------------------------------------------------------------
 *  Przyciski — czasy [ms]
 *--------------------------------------------------------------------------*/
#define BTN_DEBOUNCE_MS      50
#define BTN_SHORT_MAX_MS    500
#define BTN_LONG_MIN_MS    1000
#define BTN_LOCK_HOLD_MS   1000

/*---------------------------------------------------------------------------
 *  NVS
 *--------------------------------------------------------------------------*/
#define NVS_NAMESPACE       "vfo"
#define NVS_KEY_LAST_FREQ   "last_frq"
#define NVS_KEY_MEM_PREFIX  "mem"

/*---------------------------------------------------------------------------
 *  Autozapis [ms]
 *--------------------------------------------------------------------------*/
#define AUTOSAVE_DELAY_MS   10000

/*---------------------------------------------------------------------------
 *  Tagi logowania
 *--------------------------------------------------------------------------*/
#define TAG_MAIN    "VFO_MAIN"
#define TAG_SI5351  "SI5351"
#define TAG_DISP    "DISPLAY"
#define TAG_ENC     "ENCODER"
#define TAG_BTN     "BUTTONS"
#define TAG_NVS     "NVS"

/*---------------------------------------------------------------------------
 *  Priorytety taskow FreeRTOS
 *--------------------------------------------------------------------------*/
#define TASK_ENCODER_CORE   0
#define TASK_ENCODER_PRIO   5
#define TASK_ENCODER_STACK  3072

#define TASK_DISPLAY_CORE   0
#define TASK_DISPLAY_PRIO   3
#define TASK_DISPLAY_STACK  4096

#define TASK_BUTTONS_CORE   1
#define TASK_BUTTONS_PRIO   4
#define TASK_BUTTONS_STACK  2048

#define TASK_AUTOSAVE_CORE  1
#define TASK_AUTOSAVE_PRIO  2
#define TASK_AUTOSAVE_STACK 2048

/*---------------------------------------------------------------------------
 *  Wersja firmware
 *--------------------------------------------------------------------------*/
//#define FW_NAME     "VFO System"
//#define FW_VERSION  "Ver. 2.00"
//#define FW_AUTHOR   "Marcin / ESP-IDF port"

/*---------------------------------------------------------------------------
 *  Ujednolicona struktura pasm (zastepuje BANDS[] w buttons.c i BAND_LABELS[] w ui_overlay.c)
 *--------------------------------------------------------------------------*/
typedef struct { const char *label; uint32_t freq_hz; } band_entry_t;
#define BAND_COUNT  8
static const band_entry_t VFO_BANDS[BAND_COUNT] = {
    { "30m  10.100 MHz", 10100000UL },
    { "20m  14.000 MHz", 14000000UL },
    { "17m  18.068 MHz", 18068000UL },
    { "15m  21.000 MHz", 21000000UL },
    { "12m  24.940 MHz", 24940000UL },
    { "10m  28.000 MHz", 28000000UL },
    { " 6m  50.000 MHz", 50000000UL },
    { " FM 100.000 MHz", 100000000UL},
};

/*---------------------------------------------------------------------------
 *  Podswietlenie PWM (LEDC)
 *--------------------------------------------------------------------------*/
#define DISP_BL             4    /* GPIO dla podswietlenia ST7735 — podlacz GPIO4 do BL */
#define LEDC_CHANNEL_BL     LEDC_CHANNEL_0
#define LEDC_TIMER_BL       LEDC_TIMER_0
#define LEDC_FREQ_BL        5000
#define BRIGHTNESS_DEFAULT  80
#define BRIGHTNESS_MIN      10
#define BRIGHTNESS_MAX     100
#define BRIGHTNESS_STEP     10

/*---------------------------------------------------------------------------
 *  Nowe klucze NVS
 *--------------------------------------------------------------------------*/
#define NVS_KEY_STEP        "step"
#define NVS_KEY_XTAL_CAL    "xtal_cal"
#define NVS_KEY_IF_OFFSET   "if_off"
#define NVS_KEY_BRIGHTNESS  "bright"
#define NVS_KEY_LAST_MEM    "last_mem"
#define NVS_KEY_RIT         "rit_off"

/*---------------------------------------------------------------------------
 *  Nowe stale funkcji
 *--------------------------------------------------------------------------*/
#define XTAL_CAL_MAX        5000
#define XTAL_CAL_MIN       -5000
#define RIT_MAX_HZ          9999
#define RIT_MIN_HZ         -9999
#define SCAN_STEP_MS         300
