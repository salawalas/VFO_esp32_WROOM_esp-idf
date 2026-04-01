/*===========================================================================
 *  buttons.c — Przyciski tact switch (ESP-IDF 5.x)
 *
 *  Architektura:
 *    Task skanuje 5 przyciskow co BTN_SCAN_MS (10ms).
 *    Debouncing: stan musi byc stabilny przez BTN_DEBOUNCE_MS (50ms).
 *    Detekcja krotkie/dlugie przez pomiar czasu trzymania.
 *    LOCK: jednoczesne trzymanie STEP_DN + STEP_UP przez BTN_LOCK_HOLD_MS.
 *
 *  Akcje:
 *    BTN_STEP_DN krotkie  -> step_idx--
 *    BTN_STEP_UP krotkie  -> step_idx++
 *    BTN_MEM     krotkie  -> w trybie VFO: kolejny bank M0..M9
 *                         -> w trybie SAVE_PROMPT: YES (zapisz)
 *    BTN_MEM     dlugie   -> zaladuj czestotliwosc z aktualnego banku
 *    BTN_SAVE    krotkie  -> w trybie SAVE_PROMPT: NO (anuluj)
 *    BTN_SAVE    dlugie   -> otworz dialog zapisu do aktualnego banku
 *    BTN_BAND    krotkie  -> otworz/zamknij menu wyboru pasma
 *    BTN_BAND    dlugie   -> nie uzywane (rezerwa)
 *    STEP_DN+UP  >= 1s    -> toggle LOCK
 *===========================================================================*/

#include "buttons.h"
#include "config.h"
#include "vfo_state.h"
#include "nvs_storage.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

/*---------------------------------------------------------------------------
 *  Stale
 *--------------------------------------------------------------------------*/
#define BTN_SCAN_MS         10
#define BTN_COUNT            5

/* Indeksy przyciskow */
#define IDX_STEP_DN          0
#define IDX_STEP_UP          1
#define IDX_MEM              2
#define IDX_SAVE             3
#define IDX_BAND             4

/* GPIO przyciskow */
static const int BTN_GPIOS[BTN_COUNT] = {
    BTN_STEP_DN,   /* 27 */
    BTN_STEP_UP,   /* 26 */
    BTN_MEM,       /* 25 */
    BTN_SAVE,      /* 32 */
    BTN_BAND,      /* 33 */
};

/* Czas trzymania przycisku w ms */
#define HOLD_SHORT_MIN_MS    BTN_DEBOUNCE_MS    /* 50ms  */
#define HOLD_SHORT_MAX_MS    BTN_SHORT_MAX_MS   /* 500ms */
#define HOLD_LONG_MS         BTN_LONG_MIN_MS    /* 1000ms*/

/* Pasma amatorskie — z config.h (VFO_BANDS[BAND_COUNT]) */

/*---------------------------------------------------------------------------
 *  Stan wewnetrzny przyciskow
 *--------------------------------------------------------------------------*/
typedef struct {
    bool     raw;          /* aktualny stan GPIO (LOW=wcisniety) */
    bool     stable;       /* stan po debouncingu */
    bool     prev_stable;  /* poprzedni stan stabilny */
    uint32_t debounce_ms;  /* licznik debouncingu */
    uint32_t hold_ms;      /* czas trzymania */
    bool     handled;      /* czy akcja juz obsluzona */
} btn_state_t;

static btn_state_t s_btn[BTN_COUNT];

/* Stan menu BAND */
static bool     s_band_menu_open = false;
static int      s_band_sel       = 0;   /* aktualnie zaznaczony indeks */

/*---------------------------------------------------------------------------
 *  Prototypy wewnetrzne
 *--------------------------------------------------------------------------*/
static void handle_short(int idx);
static void handle_long(int idx);
static void check_lock_combo(void);
static void band_menu_draw(void);

/*===========================================================================
 *  buttons_init
 *===========================================================================*/
esp_err_t buttons_init(void)
{
    /* Konfiguracja GPIO — wszystkie active LOW z pull-up */
    for (int i = 0; i < BTN_COUNT; i++) {
        gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << BTN_GPIOS[i]),
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        /* GPIO34/35 sa input-only bez pull-up HW — jesli BTN_BAND=34/35
         * uzytkownik musi dodac zewnetrzny rezystor 10k do VCC */
        if (BTN_GPIOS[i] >= 34 && BTN_GPIOS[i] <= 39) {
            cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        }
        gpio_config(&cfg);
    }

    memset(s_btn, 0, sizeof(s_btn));
    for (int i = 0; i < BTN_COUNT; i++) {
        s_btn[i].stable      = true;   /* HIGH = zwolniony */
        s_btn[i].prev_stable = true;
    }

    ESP_LOGI(TAG_BTN,
             "OK — STEP_DN=%d STEP_UP=%d MEM=%d SAVE=%d BAND=%d",
             BTN_STEP_DN, BTN_STEP_UP, BTN_MEM, BTN_SAVE, BTN_BAND);
    return ESP_OK;
}

/*===========================================================================
 *  band_menu_draw — odswiezenie wyswietlacza menu BAND
 *  (ustawia flagi — display_task wykona faktyczny rendering)
 *===========================================================================*/
static void band_menu_draw(void)
{
    VFO_LOCK();
    g_vfo.disp_mode   = DISP_MODE_BAND_MENU;
    g_vfo.band_sel    = s_band_sel;
    g_vfo.f_disp_changed = true;
    VFO_UNLOCK();
}

/*===========================================================================
 *  handle_short — akcja krotkie nacisniecie
 *===========================================================================*/
static void handle_short(int idx)
{
    VFO_LOCK();
    bool locked   = g_vfo.locked;
    disp_mode_t mode = g_vfo.disp_mode;
    VFO_UNLOCK();

    /* W trybie LOCK — tylko BTN_BAND moze przerwac (toggle LOCK przez combo) */
    if (locked && idx != IDX_BAND) return;

    switch (idx) {

    case IDX_STEP_DN:
        VFO_LOCK();
        if (g_vfo.step_idx > 0) {
            g_vfo.step_idx--;
            g_vfo.f_disp_changed = true;
            ESP_LOGI(TAG_BTN, "STEP- -> idx=%d (%ld Hz)",
                     g_vfo.step_idx, (long)FREQ_STEPS[g_vfo.step_idx]);
        }
        VFO_UNLOCK();
        break;

    case IDX_STEP_UP:
        VFO_LOCK();
        if (g_vfo.step_idx < FREQ_STEP_COUNT - 1) {
            g_vfo.step_idx++;
            g_vfo.f_disp_changed = true;
            ESP_LOGI(TAG_BTN, "STEP+ -> idx=%d (%ld Hz)",
                     g_vfo.step_idx, (long)FREQ_STEPS[g_vfo.step_idx]);
        }
        VFO_UNLOCK();
        break;

    case IDX_MEM:
        if (mode == DISP_MODE_SAVE_PROMPT) {
            /* YES w dialogu zapisu */
            VFO_LOCK();
            int    midx = g_vfo.mem_idx;
            uint32_t frq = g_vfo.freq;
            VFO_UNLOCK();

            nvs_save_mem(midx, frq);
            VFO_LOCK();
            g_vfo.mem_freq[midx] = frq;
            g_vfo.disp_mode      = DISP_MODE_SAVE_OK;
            g_vfo.f_disp_changed = true;
            VFO_UNLOCK();
            ESP_LOGI(TAG_BTN, "SAVED M%d = %lu Hz", midx, (unsigned long)frq);

        } else if (mode == DISP_MODE_BAND_MENU) {
            /* Wybor pasma w menu */
            uint32_t bfrq = VFO_BANDS[s_band_sel].freq_hz;
            s_band_menu_open = false;
            VFO_LOCK();
            g_vfo.freq        = bfrq;
            g_vfo.mem_idx     = 0;
            g_vfo.disp_mode   = DISP_MODE_VFO;
            g_vfo.f_freq_changed = true;
            g_vfo.f_disp_changed = true;
            VFO_UNLOCK();
            ESP_LOGI(TAG_BTN, "BAND -> %s (%lu Hz)",
                     VFO_BANDS[s_band_sel].label, (unsigned long)bfrq);

        } else {
            /* Przewijanie bankow pamieci */
            VFO_LOCK();
            g_vfo.mem_idx = (g_vfo.mem_idx + 1) % VFO_MEM_COUNT;
            g_vfo.f_disp_changed = true;
            VFO_UNLOCK();
        }
        break;

    case IDX_SAVE:
        if (mode == DISP_MODE_SAVE_PROMPT) {
            /* NO — anuluj zapis */
            VFO_LOCK();
            g_vfo.disp_mode      = DISP_MODE_VFO;
            g_vfo.f_disp_changed = true;
            VFO_UNLOCK();

        } else if (mode == DISP_MODE_BAND_MENU) {
            /* Zamknij menu BAND bez wyboru */
            s_band_menu_open = false;
            VFO_LOCK();
            g_vfo.disp_mode      = DISP_MODE_VFO;
            g_vfo.f_disp_changed = true;
            VFO_UNLOCK();
        }
        break;

    case IDX_BAND:
        if (mode == DISP_MODE_BAND_MENU) {
            /* Nastepna pozycja w menu */
            s_band_sel = (s_band_sel + 1) % (int)BAND_COUNT;
            band_menu_draw();
        } else {
            /* Otworz menu */
            s_band_menu_open = true;
            s_band_sel       = 0;
            band_menu_draw();
            ESP_LOGI(TAG_BTN, "BAND menu otwarty");
        }
        break;
    }
}

/*===========================================================================
 *  handle_long — akcja dlugie nacisniecie
 *===========================================================================*/
static void handle_long(int idx)
{
    VFO_LOCK();
    bool locked = g_vfo.locked;
    VFO_UNLOCK();
    if (locked) return;

    switch (idx) {

    case IDX_MEM: {
        /* Zaladuj czestotliwosc z aktualnego banku */
        VFO_LOCK();
        int      midx = g_vfo.mem_idx;
        uint32_t mfrq = g_vfo.mem_freq[midx];
        VFO_UNLOCK();

        if (mfrq == 0) {
            /* Pusta komorka — sprobuj wczytac z NVS */
            nvs_load_mem(midx, &mfrq);
        }
        if (mfrq > 0) {
            VFO_LOCK();
            g_vfo.freq           = mfrq;
            g_vfo.disp_mode      = DISP_MODE_LOAD_OK;
            g_vfo.f_freq_changed = true;
            g_vfo.f_disp_changed = true;
            VFO_UNLOCK();
            ESP_LOGI(TAG_BTN, "LOADED M%d = %lu Hz", midx, (unsigned long)mfrq);
        }
        break;
    }

    case IDX_SAVE: {
        /* Otworz dialog zapisu */
        VFO_LOCK();
        g_vfo.disp_mode      = DISP_MODE_SAVE_PROMPT;
        g_vfo.f_disp_changed = true;
        VFO_UNLOCK();
        ESP_LOGI(TAG_BTN, "SAVE dialog otwarty dla M%d", 0);
        break;
    }

    default:
        break;
    }
}

/*===========================================================================
 *  check_lock_combo — sprawdz jednoczesne trzymanie STEP_DN + STEP_UP
 *===========================================================================*/
static void check_lock_combo(void)
{
    static uint32_t combo_start_ms = 0;
    static bool     combo_handled  = false;

    bool dn_held = !s_btn[IDX_STEP_DN].stable;
    bool up_held = !s_btn[IDX_STEP_UP].stable;

    if (dn_held && up_held) {
        if (combo_start_ms == 0) {
            combo_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            combo_handled  = false;
        } else if (!combo_handled) {
            uint32_t held = xTaskGetTickCount() * portTICK_PERIOD_MS
                            - combo_start_ms;
            if (held >= BTN_LOCK_HOLD_MS) {
                VFO_LOCK();
                g_vfo.locked         = !g_vfo.locked;
                g_vfo.f_disp_changed = true;
                ESP_LOGI(TAG_BTN, "LOCK toggle -> %s",
                         g_vfo.locked ? "ON" : "OFF");
                VFO_UNLOCK();
                combo_handled = true;
            }
        }
    } else {
        combo_start_ms = 0;
    }
}

/*===========================================================================
 *  buttons_task — Core 1, priorytet 4
 *===========================================================================*/
void buttons_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG_BTN, "buttons_task start — rdzen %d", xPortGetCoreID());

    /* Czas pokazywania komunikatow SAVED/LOADED/SAVE_PROMPT [ms] */
    uint32_t msg_timer_ms  = 0;
    bool     msg_active    = false;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(BTN_SCAN_MS));
        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* ----------------------------------------------------------------
         *  Auto-zamkniecie komunikatow po 2 sekundach
         * -------------------------------------------------------------- */
        VFO_LOCK();
        disp_mode_t cur_mode = g_vfo.disp_mode;
        VFO_UNLOCK();

        if (cur_mode == DISP_MODE_SAVE_OK || cur_mode == DISP_MODE_LOAD_OK) {
            if (!msg_active) {
                msg_active   = true;
                msg_timer_ms = now_ms;
            } else if (now_ms - msg_timer_ms >= 2000) {
                msg_active = false;
                VFO_LOCK();
                g_vfo.disp_mode      = DISP_MODE_VFO;
                g_vfo.f_disp_changed = true;
                VFO_UNLOCK();
            }
        } else {
            msg_active = false;
        }

        /* ----------------------------------------------------------------
         *  Skanowanie i debouncing przyciskow
         * -------------------------------------------------------------- */
        for (int i = 0; i < BTN_COUNT; i++) {
            bool raw = gpio_get_level(BTN_GPIOS[i]);   /* HIGH=zwolniony */
            s_btn[i].raw = raw;

            if (raw != s_btn[i].stable) {
                s_btn[i].debounce_ms += BTN_SCAN_MS;
                if (s_btn[i].debounce_ms >= BTN_DEBOUNCE_MS) {
                    s_btn[i].prev_stable  = s_btn[i].stable;
                    s_btn[i].stable       = raw;
                    s_btn[i].debounce_ms  = 0;

                    if (!raw) {
                        /* Zbocze opadajace — poczatek nacisniecia */
                        s_btn[i].hold_ms   = 0;
                        s_btn[i].handled   = false;
                    }
                }
            } else {
                s_btn[i].debounce_ms = 0;
            }

            /* Licznik czasu trzymania */
            if (!s_btn[i].stable) {
                s_btn[i].hold_ms += BTN_SCAN_MS;
            }

            /* Dlugie nacisniecie — obsluż raz po przekroczeniu progu */
            if (!s_btn[i].stable && !s_btn[i].handled
                    && s_btn[i].hold_ms >= HOLD_LONG_MS) {
                handle_long(i);
                s_btn[i].handled = true;
            }

            /* Krotkie nacisniecie — obsluż przy zwolnieniu */
            if (s_btn[i].prev_stable == false && s_btn[i].stable == true
                    && !s_btn[i].handled
                    && s_btn[i].hold_ms >= HOLD_SHORT_MIN_MS
                    && s_btn[i].hold_ms <  HOLD_SHORT_MAX_MS) {
                handle_short(i);
                s_btn[i].handled = true;
            }
        }

        /* ----------------------------------------------------------------
         *  Sprawdz kombinacje LOCK
         * -------------------------------------------------------------- */
        check_lock_combo();
    }
}
