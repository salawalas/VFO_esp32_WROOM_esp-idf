/*===========================================================================
 *  encoder.c — Enkoder obrotowy (PCNT v2, ESP-IDF 5.x)
 *
 *  PCNT v2 API (ESP-IDF >= 5.0):
 *    pcnt_new_unit()     zamiast pcnt_unit_config()
 *    pcnt_new_channel()  zamiast pcnt_channel_config()
 *    pcnt_unit_start()   zamiast pcnt_counter_resume()
 *
 *  Konfiguracja kanalow — identyczna z oryginaem Arduino:
 *    Kanal 0: pulse=A, ctrl=B, lctrl=REVERSE, hctrl=KEEP,  pos=INC, neg=DEC
 *    Kanal 1: pulse=B, ctrl=A, lctrl=KEEP,    hctrl=REVERSE,pos=INC, neg=DEC
 *    => 4x rozdzielczosc (quadrature x4)
 *
 *  Dynamiczny mnoznik kroku:
 *    Mierzony co ENC_TASK_PERIOD_MS (20ms).
 *    |impulsy| na cykl okreslaja mnoznik: x1 / x10 / x100 / x1000
 *    Mnoznik resetuje sie do x1 gdy enkoder stoi.
 *
 *  Przycisk SW enkodera:
 *    Krotkie nacisniecie: reset mnoznika do x1 (natychmiastowy powrot do slow)
 *    Dlugie nacisniecie: toggle kierunku f_rev
 *===========================================================================*/

#include "encoder.h"
#include "config.h"
#include "vfo_state.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*---------------------------------------------------------------------------
 *  Stale
 *--------------------------------------------------------------------------*/
#define ENC_TASK_PERIOD_MS  20      /* okres odczytu PCNT [ms]          */
#define ENC_GLITCH_NS       1000    /* filtr glitchy [ns]               */
#define ENC_COUNT_LIMIT     32767   /* limit licznika PCNT              */
#define ENC_SW_DEBOUNCE_MS  50      /* debouncing przycisku SW [ms]     */
#define ENC_SW_LONG_MS      800     /* prog dlugiego nacisniecia [ms]   */

/*---------------------------------------------------------------------------
 *  Zmienne modulowe
 *--------------------------------------------------------------------------*/
static pcnt_unit_handle_t   s_pcnt_unit = NULL;
static int                  s_accel_mul = 1;    /* aktualny mnoznik x1/10/100/1000 */

/*===========================================================================
 *  encoder_init — inicjalizacja PCNT v2
 *===========================================================================*/
esp_err_t encoder_init(void)
{
    esp_err_t err;

    /* --- Jednostka PCNT --- */
    pcnt_unit_config_t unit_cfg = {
        .low_limit  = -ENC_COUNT_LIMIT,
        .high_limit =  ENC_COUNT_LIMIT,
        .flags.accum_count = false,
    };
    err = pcnt_new_unit(&unit_cfg, &s_pcnt_unit);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_ENC, "pcnt_new_unit: %s", esp_err_to_name(err));
        return err;
    }

    /* --- Filtr glitchy --- */
    pcnt_glitch_filter_config_t flt = {
        .max_glitch_ns = ENC_GLITCH_NS,
    };
    err = pcnt_unit_set_glitch_filter(s_pcnt_unit, &flt);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_ENC, "glitch_filter: %s", esp_err_to_name(err));
        return err;
    }

    /* --- Kanal 0: pulse=A(17), ctrl=B(16) --- */
    pcnt_chan_config_t ch0_cfg = {
        .edge_gpio_num  = ENC_PIN_A,   /* GPIO17 */
        .level_gpio_num = ENC_PIN_B,   /* GPIO16 */
    };
    pcnt_channel_handle_t ch0 = NULL;
    err = pcnt_new_channel(s_pcnt_unit, &ch0_cfg, &ch0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_ENC, "pcnt_new_channel ch0: %s", esp_err_to_name(err));
        return err;
    }
    /* Zbocze narastajace: INC gdy ctrl=LOW(REVERSE), DEC gdy ctrl=HIGH(KEEP) */
    pcnt_channel_set_edge_action(ch0,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,   /* pos edge */
        PCNT_CHANNEL_EDGE_ACTION_DECREASE);  /* neg edge */
    pcnt_channel_set_level_action(ch0,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,      /* ctrl HIGH -> keep */
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);  /* ctrl LOW  -> reverse */

    /* --- Kanal 1: pulse=B(16), ctrl=A(17) --- */
    pcnt_chan_config_t ch1_cfg = {
        .edge_gpio_num  = ENC_PIN_B,   /* GPIO16 */
        .level_gpio_num = ENC_PIN_A,   /* GPIO17 */
    };
    pcnt_channel_handle_t ch1 = NULL;
    err = pcnt_new_channel(s_pcnt_unit, &ch1_cfg, &ch1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_ENC, "pcnt_new_channel ch1: %s", esp_err_to_name(err));
        return err;
    }
    pcnt_channel_set_edge_action(ch1,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(ch1,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE,   /* ctrl HIGH -> reverse */
        PCNT_CHANNEL_LEVEL_ACTION_KEEP);     /* ctrl LOW  -> keep    */

    /* --- Wlacz licznik --- */
    err = pcnt_unit_enable(s_pcnt_unit);
    if (err != ESP_OK) return err;
    err = pcnt_unit_clear_count(s_pcnt_unit);
    if (err != ESP_OK) return err;
    err = pcnt_unit_start(s_pcnt_unit);
    if (err != ESP_OK) return err;

    /* --- GPIO dla przycisku SW enkodera ---
     * ENC_PIN_SW=34 jest input-only — nie mozna ustawic pull-up programowo.
     * Wymaga zewnetrznego rezystora 10k do VCC.
     * Jesli uzywasz GPIO33 lub innego z pull-up: zmien na GPIO_PULLUP_ENABLE */
    gpio_config_t sw_cfg = {
        .pin_bit_mask = (1ULL << ENC_PIN_SW),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,   /* brak dla GPIO34 */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&sw_cfg);

    ESP_LOGI(TAG_ENC,
             "PCNT v2 OK — A=GPIO%d, B=GPIO%d, SW=GPIO%d | "
             "progi accel: x10@%d x100@%d x1000@%d imp/cykl",
             ENC_PIN_A, ENC_PIN_B, ENC_PIN_SW,
             ENC_ACCEL_THR1, ENC_ACCEL_THR2, ENC_ACCEL_THR3);
    return ESP_OK;
}

/*===========================================================================
 *  encoder_task — Core 0, priorytet 5
 *
 *  Cykl 20ms:
 *  1. Odczytaj i wyzeruj licznik PCNT
 *  2. Oblicz mnoznik dynamiczny na podstawie predkosci
 *  3. Aktualizuj czestotliwosc w g_vfo (z mutexem)
 *  4. Sprawdz przycisk SW
 *===========================================================================*/
void encoder_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG_ENC, "encoder_task start — rdzen %d", xPortGetCoreID());

    /* Zmienne lokalne przycisku SW */
    bool     sw_prev       = true;    /* HIGH = zwolniony (active LOW) */
    uint32_t sw_press_tick = 0;
    bool     sw_handled    = false;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(ENC_TASK_PERIOD_MS));

        /* ----------------------------------------------------------------
         *  1. Odczyt PCNT
         * -------------------------------------------------------------- */
        int raw = 0;
        pcnt_unit_get_count(s_pcnt_unit, &raw);
        pcnt_unit_clear_count(s_pcnt_unit);

        /* ----------------------------------------------------------------
         *  2. Przelicz surowe impulsy na detenty (skoki fizyczne)
         *     Akumulator przechowuje niepelne detenty miedzy cyklami.
         * -------------------------------------------------------------- */
        static int s_acc = 0;
        s_acc += raw;
        int detents = s_acc / ENC_PULSES_PER_DETENT;
        s_acc      %= ENC_PULSES_PER_DETENT;

        int abs_det = (detents < 0) ? -detents : detents;

        /* ----------------------------------------------------------------
         *  2b. Dynamiczny mnoznik kroku (progi w detentach/cykl)
         * -------------------------------------------------------------- */
        if (abs_det == 0) {
            /* Enkoder stoi — stopniowo zmniejsz mnoznik */
            if (s_accel_mul > 1) s_accel_mul /= 10;
        } else if (abs_det >= ENC_ACCEL_THR3) {
            s_accel_mul = 1000;
        } else if (abs_det >= ENC_ACCEL_THR2) {
            s_accel_mul = 100;
        } else if (abs_det >= ENC_ACCEL_THR1) {
            s_accel_mul = 10;
        } else {
            s_accel_mul = 1;
        }

        /* ----------------------------------------------------------------
         *  3. Aktualizacja czestotliwosci
         * -------------------------------------------------------------- */
        if (detents != 0) {
            VFO_LOCK();
            disp_mode_t cur_mode = g_vfo.disp_mode;

            if (!g_vfo.locked) {
                if (cur_mode == DISP_MODE_BAND_MENU) {
                    /* Menu pasm — przewijaj liste enkoderem */
                    int sel = g_vfo.band_sel + detents;
                    if (sel < 0) sel = BAND_COUNT - 1;
                    if (sel >= BAND_COUNT) sel = 0;
                    g_vfo.band_sel       = sel;
                    g_vfo.f_disp_changed = true;

                } else if (cur_mode == DISP_MODE_XTAL_CAL) {
                    /* Kalibracja kwarcu — krok 1 Hz (x10 przy szybkim obrocie) */
                    int32_t step    = (abs_det >= 5) ? 10 : 1;
                    int32_t new_cal = g_vfo.xtal_cal + (int32_t)detents * step;
                    if (new_cal >  XTAL_CAL_MAX) new_cal =  XTAL_CAL_MAX;
                    if (new_cal <  XTAL_CAL_MIN) new_cal =  XTAL_CAL_MIN;
                    g_vfo.xtal_cal       = new_cal;
                    g_vfo.f_freq_changed = true;
                    g_vfo.f_disp_changed = true;

                } else {
                    /* Normalny tryb VFO */
                    extern char f_rev;
                    int count     = (f_rev == 1) ? -detents : detents;
                    int32_t base_step = FREQ_STEPS[g_vfo.step_idx];
                    int32_t delta     = (int32_t)count * base_step * s_accel_mul;

                    int32_t new_freq = (int32_t)g_vfo.freq + delta;
                    if (new_freq < (int32_t)VFO_FREQ_MIN) new_freq = VFO_FREQ_MIN;
                    if (new_freq > (int32_t)VFO_FREQ_MAX) new_freq = VFO_FREQ_MAX;

                    g_vfo.freq           = (uint32_t)new_freq;
                    g_vfo.f_freq_changed = true;
                    g_vfo.f_disp_changed = true;
                    g_vfo.f_autosave_arm = true;
                    g_vfo.mem_idx        = 0;
                }
            }

            VFO_UNLOCK();
        }

        /* ----------------------------------------------------------------
         *  4. Przycisk SW enkodera
         *     GPIO34 jest active LOW (przycisk zwiera do GND)
         * -------------------------------------------------------------- */
        bool sw_now = gpio_get_level(ENC_PIN_SW);   /* LOW = wcisniety */

        if (sw_prev && !sw_now) {
            /* Zbocze opadajace — poczatek nacisniecia */
            sw_press_tick = xTaskGetTickCount();
            sw_handled    = false;
        }

        if (!sw_now && !sw_handled) {
            uint32_t held_ms = (xTaskGetTickCount() - sw_press_tick)
                               * portTICK_PERIOD_MS;

            if (held_ms >= ENC_SW_LONG_MS) {
                /* Dlugie nacisniecie — toggle kierunku */
                extern char f_rev;
                f_rev     = (f_rev == 0) ? 1 : 0;
                sw_handled = true;
                ESP_LOGI(TAG_ENC, "SW dlugie: f_rev=%d", (int)f_rev);

                VFO_LOCK();
                g_vfo.f_disp_changed = true;
                VFO_UNLOCK();
            }
        }

        if (!sw_prev && sw_now && !sw_handled) {
            /* Zbocze narastajace — zwolnienie po krotkim nacisnieciu */
            uint32_t held_ms = (xTaskGetTickCount() - sw_press_tick)
                               * portTICK_PERIOD_MS;

            if (held_ms >= ENC_SW_DEBOUNCE_MS && held_ms < ENC_SW_LONG_MS) {
                VFO_LOCK();
                if (g_vfo.disp_mode == DISP_MODE_BAND_MENU) {
                    /* Potwierdz wybor pasma */
                    int      bsel = g_vfo.band_sel;
                    uint32_t bfrq = VFO_BANDS[bsel].freq_hz;
                    g_vfo.freq           = bfrq;
                    g_vfo.mem_idx        = 0;
                    g_vfo.disp_mode      = DISP_MODE_VFO;
                    g_vfo.f_freq_changed = true;
                    g_vfo.f_disp_changed = true;
                    ESP_LOGI(TAG_ENC, "SW: BAND wybrany -> %s (%lu Hz)",
                             VFO_BANDS[bsel].label, (unsigned long)bfrq);
                } else {
                    /* Otworz menu od pierwszej pozycji */
                    g_vfo.band_sel  = 0;
                    g_vfo.disp_mode = DISP_MODE_BAND_MENU;
                    g_vfo.f_disp_changed = true;
                    ESP_LOGI(TAG_ENC, "SW: BAND menu otwarty");
                }
                VFO_UNLOCK();
            }
        }

        sw_prev = sw_now;
    }
}
