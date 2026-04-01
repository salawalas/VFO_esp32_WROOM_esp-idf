/*===========================================================================
 *  vfo_state.h — Globalny stan VFO (ESP-IDF 5.x / PlatformIO)
 *===========================================================================*/
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "config.h"

typedef enum {
    DISP_MODE_VFO = 0,      /* normalny tryb VFO          */
    DISP_MODE_MEM,           /* przeglad bankow pamieci    */
    DISP_MODE_LOCK,          /* zablokowany (nieuzywany)   */
    DISP_MODE_SAVE_OK,       /* komunikat: zapisano        */
    DISP_MODE_LOAD_OK,       /* komunikat: wczytano        */
    DISP_MODE_SAVE_PROMPT,   /* dialog: SAVE TO Mn?        */
    DISP_MODE_BAND_MENU,     /* menu wyboru pasma          */
    DISP_MODE_XTAL_CAL,      /* kalibracja kwarcu          */
    DISP_MODE_IF_OFFSET,     /* offset IF                  */
    DISP_MODE_SCAN,          /* tryb skanowania            */
} disp_mode_t;

typedef struct {
    uint32_t    freq;
    uint32_t    offset_freq;
    uint32_t    car_freq;
    bool        car_on;
    int         step_idx;
    int         mem_idx;
    uint32_t    mem_freq[VFO_MEM_COUNT];
    bool        locked;
    disp_mode_t disp_mode;
    int         band_sel;       /* aktualnie zaznaczone pasmo w menu */
    bool        f_freq_changed;
    bool        f_disp_changed;
    bool        f_autosave_arm;
    bool        f_rev;                     /* kierunek enkodera (z dial.c) */
    int32_t     rit_offset;                /* RIT offset [Hz] +/-9999 */
    bool        rit_enabled;               /* RIT on/off */
    int32_t     if_offset;                 /* IF offset [Hz] */
    int32_t     xtal_cal;                  /* kalibracja kwarcu [Hz] +/-5000 */
    uint8_t     brightness;                /* jasnosc podswietlenia 0-100% */
    bool        scan_active;               /* tryb skanowania */
    bool        mem_dirty[VFO_MEM_COUNT];  /* banki do zapisania w NVS */
    int         last_mem_idx;              /* ostatnio uzywany bank */
    SemaphoreHandle_t mutex;
} vfo_state_t;

extern vfo_state_t g_vfo;

void vfo_state_init(void);

#define VFO_LOCK()   xSemaphoreTake(g_vfo.mutex, portMAX_DELAY)
#define VFO_UNLOCK() xSemaphoreGive(g_vfo.mutex)
