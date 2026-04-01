/*===========================================================================
 *  vfo_state.c — Inicjalizacja globalnego stanu VFO
 *===========================================================================*/
#include "vfo_state.h"
#include "config.h"
#include <string.h>

vfo_state_t g_vfo;

void vfo_state_init(void)
{
    memset(&g_vfo, 0, sizeof(g_vfo));

    g_vfo.mutex = xSemaphoreCreateMutex();
    configASSERT(g_vfo.mutex != NULL);

    g_vfo.freq        = VFO_FREQ_INIT;
    g_vfo.offset_freq = 0;
    g_vfo.car_freq    = 0;
    g_vfo.car_on      = true;
    g_vfo.step_idx    = FREQ_STEP_DEFAULT;
    g_vfo.mem_idx     = 0;
    g_vfo.locked      = false;
    g_vfo.disp_mode   = DISP_MODE_VFO;

    /* Domyslne czestotliwosci bankow pamieci */
    g_vfo.mem_freq[0] = VFO_MEM_0;
    g_vfo.mem_freq[1] = VFO_MEM_1;
    g_vfo.mem_freq[2] = VFO_MEM_2;
    g_vfo.mem_freq[3] = VFO_MEM_3;
    g_vfo.mem_freq[4] = VFO_MEM_4;
    g_vfo.mem_freq[5] = VFO_MEM_5;
    g_vfo.mem_freq[6] = VFO_MEM_6;
    g_vfo.mem_freq[7] = VFO_MEM_7;
    g_vfo.mem_freq[8] = VFO_MEM_8;
    g_vfo.mem_freq[9] = VFO_MEM_9;

    g_vfo.f_freq_changed = true;
    g_vfo.f_disp_changed = true;
    g_vfo.f_autosave_arm = false;

    g_vfo.f_rev       = false;
    g_vfo.rit_offset  = 0;
    g_vfo.rit_enabled = false;
    g_vfo.if_offset   = 0;
    g_vfo.xtal_cal    = 0;
    g_vfo.brightness  = BRIGHTNESS_DEFAULT;
    g_vfo.scan_active = false;
    g_vfo.last_mem_idx = 0;
    memset(g_vfo.mem_dirty, 0, sizeof(g_vfo.mem_dirty));
}
