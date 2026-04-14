/*===========================================================================
 *  main.c — VFO Controller (ESP-IDF 5.x / PlatformIO)
 *===========================================================================*/
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "driver/ledc.h"

#include "config.h"
#include "vfo_state.h"
#include "si5351.h"
#include "display.h"
#include "graph.h"
#include "dial.h"
#include "encoder.h"
#include "buttons.h"
#include "nvs_storage.h"
#include "ui_overlay.h"
#include "version.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

static void display_task(void *arg);
static void system_init(void);
static void tasks_start(void);
static void backlight_init(uint8_t pct);

/*===========================================================================
 *  app_main
 *===========================================================================*/
void app_main(void)
{
    ESP_LOGI(TAG_MAIN, "============================================");
    //ESP_LOGI(TAG_MAIN, "  %s  %s", FW_NAME, FW_VERSION);
    //ESP_LOGI(TAG_MAIN, "  %s", FW_AUTHOR);
    //ESP_LOGI(TAG_MAIN, "============================================");

    ESP_LOGI(TAG_MAIN, "%s %s", FW_NAME, FW_VERSION);
    ESP_LOGI(TAG_MAIN, "%s", FW_AUTHOR);
    ESP_LOGI(TAG_MAIN, "%s", BUILD_DATE);
    ESP_LOGI(TAG_MAIN, "commit %s", FW_COMMIT);
    ESP_LOGI(TAG_MAIN, "============================================");

    /* NVS flash init */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG_MAIN, "NVS: czyszczenie i reinicjalizacja...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG_MAIN, "NVS flash: OK");

    vfo_state_init();
    ESP_LOGI(TAG_MAIN, "Globalny stan VFO: OK");

    system_init();
    tasks_start();

    ESP_LOGI(TAG_MAIN, "Scheduler uruchomiony.");

    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI("CHIP", "Model: ESP32 rev %d", chip.revision);
    ESP_LOGI("CHIP", "Rdzenie CPU: %d", chip.cores);
    ESP_LOGI("CHIP", "Flash: %lu MB", (unsigned long)(flash_size / (1024*1024)));
    ESP_LOGI("CHIP", "WiFi: %s  BT: %s  BLE: %s",
        (chip.features & CHIP_FEATURE_WIFI_BGN) ? "TAK" : "NIE",
        (chip.features & CHIP_FEATURE_BT)       ? "TAK" : "NIE",
        (chip.features & CHIP_FEATURE_BLE)      ? "TAK" : "NIE");
    ESP_LOGI("CHIP", "RAM: ~520 KB SRAM (standard ESP32)");

    vTaskDelete(NULL);
}

/*===========================================================================
 *  system_init
 *===========================================================================*/
static void system_init(void)
{
    esp_err_t err;

    err = nvs_storage_init();
    if (err == ESP_OK) {
        uint32_t saved_freq = 0;
        if (nvs_load_last_freq(&saved_freq) == ESP_OK && saved_freq != 0) {
            VFO_LOCK();
            g_vfo.freq = saved_freq;
            ESP_LOGI(TAG_MAIN, "Wczytano czestotliwosc: %lu Hz",
                     (unsigned long)saved_freq);
            VFO_UNLOCK();
        }
        for (int i = 0; i < VFO_MEM_COUNT; i++) {
            uint32_t mf = 0;
            if (nvs_load_mem(i, &mf) == ESP_OK && mf != 0) {
                VFO_LOCK();
                g_vfo.mem_freq[i] = mf;
                VFO_UNLOCK();
            }
        }
        /* Pozostale parametry z NVS */
        int      step_idx = FREQ_STEP_DEFAULT;
        int32_t  xtal_cal = 0, if_off = 0, rit_off = 0;
        uint8_t  bright   = BRIGHTNESS_DEFAULT;
        int      last_mem = 0;

        if (nvs_load_step_idx(&step_idx) == ESP_OK) {
            if (step_idx < 0) step_idx = 0;
            if (step_idx >= FREQ_STEP_COUNT) step_idx = FREQ_STEP_COUNT - 1;
        }
        nvs_load_xtal_cal(&xtal_cal);
        nvs_load_if_offset(&if_off);
        nvs_load_brightness(&bright);
        if (bright < BRIGHTNESS_MIN || bright > BRIGHTNESS_MAX)
            bright = BRIGHTNESS_DEFAULT;
        nvs_load_last_mem(&last_mem);
        nvs_load_rit(&rit_off);

        VFO_LOCK();
        g_vfo.step_idx    = step_idx;
        g_vfo.xtal_cal    = xtal_cal;
        g_vfo.if_offset   = if_off;
        g_vfo.brightness  = bright;
        g_vfo.last_mem_idx = last_mem;
        g_vfo.rit_offset  = rit_off;
        VFO_UNLOCK();
    }

    /* Podswietlenie PWM */
    VFO_LOCK();
    uint8_t bright = g_vfo.brightness;
    VFO_UNLOCK();
    backlight_init(bright);
    ESP_LOGI(TAG_MAIN, "Podswietlenie LEDC: OK (%d%%)", (int)bright);

    err = si5351_init();
    ESP_ERROR_CHECK(err);
    VFO_LOCK();
    int32_t cal = g_vfo.xtal_cal;
    VFO_UNLOCK();
    if (cal != 0) si5351_set_xtal_cal(cal);
    ESP_LOGI(TAG_MAIN, "Si5351A: OK");

    err = display_init();
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG_MAIN, "Wyswietlacz ST7735: OK");

    dial_init();

    err = encoder_init();
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG_MAIN, "Enkoder PCNT: OK");

    err = buttons_init();
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG_MAIN, "Przyciski GPIO: OK");



    /* Splash screen */
    {
        char spl[48];
        gram_clear();

        /* Naglowek z ramka */
        boxfill(2, 108, NX - 3, NY - 2, 0x001a2a);
        draw_box(2, 108, NX - 3, NY - 2, 0x0066aa);
        disp_str16(FW_NAME, 22, 110, 0x00ccff);

        /* Separator 1 */
        draw_line(4, 106, NX - 5, 106, 0x224466);

        /* Wersja + commit hash w dwoch wierszach */
        disp_str8("ver", 4, 95, 0x778899);
        disp_str12(FW_VERSION, 30, 92, 0xffd080);
        disp_str8("git", 4, 81, 0x778899);
        snprintf(spl, sizeof(spl), "%s", FW_COMMIT);
        disp_str12(spl, 30, 78, 0xff8833);

        /* Data buildu */
        disp_str8(BUILD_DATE, 4, 68, 0x99aabb);

        /* Separator 2 */
        draw_line(4, 64, NX - 5, 64, 0x224433);

        /* Heap */
        uint32_t heap_kb = esp_get_free_heap_size() / 1024;
        disp_str8("Heap", 4, 52, 0x55aa55);
        snprintf(spl, sizeof(spl), "%lu KB free", (unsigned long)heap_kb);
        disp_str12(spl, 38, 49, 0x66ff66);

        /* CPU freq */
        disp_str8("CPU", 4, 36, 0x55aa55);
        snprintf(spl, sizeof(spl), "%d MHz", CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ);
        disp_str12(spl, 38, 33, 0x66ff66);

        /* IDF version */
        disp_str8("IDF", 4, 20, 0x55aa55);
        disp_str12(esp_get_idf_version(), 38, 18, 0x88ddaa);

        /* Separator 3 */
        draw_line(4, 13, NX - 5, 13, 0x1a2a1a);

        /* Autor + sprzet */
        disp_str8(FW_AUTHOR, 4, 2, 0x6699bb);
        disp_str8("ST7735 Si5351A", 70, 2, 0x6699bb);

        display_trans65k();
        display_transfer_image();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    /* Po splashu — wyczysc ekran i ustaw flagi dla display_task */
    gram_clear();
    display_trans65k();
    display_transfer_image();

    VFO_LOCK();
    g_vfo.f_freq_changed = true;
    g_vfo.f_disp_changed = true;  /* display_task odrysuje po starcie */
    VFO_UNLOCK();

    ESP_LOGI(TAG_MAIN, "system_init: KOMPLETNA");
}

/*===========================================================================
 *  tasks_start
 *===========================================================================*/
static void tasks_start(void)
{
    BaseType_t res;

    res = xTaskCreatePinnedToCore(encoder_task, "encoder",
            TASK_ENCODER_STACK, NULL, TASK_ENCODER_PRIO, NULL, TASK_ENCODER_CORE);
    configASSERT(res == pdPASS);

    res = xTaskCreatePinnedToCore(display_task, "display",
            TASK_DISPLAY_STACK, NULL, TASK_DISPLAY_PRIO, NULL, TASK_DISPLAY_CORE);
    configASSERT(res == pdPASS);

    res = xTaskCreatePinnedToCore(buttons_task, "buttons",
            TASK_BUTTONS_STACK, NULL, TASK_BUTTONS_PRIO, NULL, TASK_BUTTONS_CORE);
    configASSERT(res == pdPASS);

    res = xTaskCreatePinnedToCore(autosave_task, "autosave",
            TASK_AUTOSAVE_STACK, NULL, TASK_AUTOSAVE_PRIO, NULL, TASK_AUTOSAVE_CORE);
    configASSERT(res == pdPASS);

    ESP_LOGI(TAG_MAIN, "Wszystkie taski uruchomione.");
}

/*===========================================================================
 *  display_task — Core 0
 *===========================================================================*/
static void display_task(void *arg)
{
    (void)arg;
    char str[64];

    ESP_LOGI(TAG_MAIN, "display_task: start");

    while (1) {
        VFO_LOCK();
        bool        f_disp   = g_vfo.f_disp_changed;
        bool        f_freq   = g_vfo.f_freq_changed;
        uint32_t    freq     = g_vfo.freq;
        bool        car_on   = g_vfo.car_on;
        int         step_idx = g_vfo.step_idx;
        int         mem_idx  = g_vfo.mem_idx;
        bool        locked   = g_vfo.locked;
        disp_mode_t mode     = g_vfo.disp_mode;
        int         band_sel = g_vfo.band_sel;
        int32_t     xtal_cal = g_vfo.xtal_cal;
        if (f_disp) g_vfo.f_disp_changed = false;
        if (f_freq) g_vfo.f_freq_changed  = false;
        VFO_UNLOCK();

        if (f_freq) {
            si5351_set_xtal_cal(xtal_cal);   /* aktualizuj kalibracje przed synteza */
            si5351_set_freq(freq);
            si5351_set_car_freq(freq, car_on);
        }

        if (f_disp) {
            if (mode == DISP_MODE_BAND_MENU) {
                /* Pelnoekranowe menu — pomija dial_draw */
                ui_draw_band_menu(band_sel);

            } else if (mode == DISP_MODE_XTAL_CAL) {
                /* Pelnoekranowy ekran kalibracji — pomija dial_draw */
                ui_draw_xtal_cal(xtal_cal);

            } else if (mode == DISP_MODE_BRIGHTNESS) {
                /* Pelnoekranowy ekran regulacji jasnosci */
                VFO_LOCK();
                uint8_t br = g_vfo.brightness;
                VFO_UNLOCK();
                ui_draw_brightness(br);

            } else {
                /* Normalny widok VFO + overlaye */
                boxfill(0, 0, NX - 1, NY - 1, 0x000000);
                dial_draw(freq);
                draw_box(7, 100, 153, 126, 0xa0a0a0);
                draw_box(6,  99, 154, 127, 0xa0a0a0);

                /* Krok */
                const char *step_labels[] = {
                    "STEPS 10 Hz", "STEPS 100 Hz", "STEPS 1 kHz",
                    "STEPS 10 kHz", "STEPS 100 kHz", "STEPS 1 MHz"
                };
                if (step_idx >= 0 && step_idx < FREQ_STEP_COUNT) {
                    disp_str8(step_labels[step_idx], 50, 85, 0xffd080);
                }

                /* Bank pamięci */
                if (mem_idx == 0) {
                    disp_str8("VFO", 5, 85, 0x00ffff);
                } else {
                    snprintf(str, sizeof(str), "M%d", mem_idx);
                    disp_str8(str, 5, 85, 0xffd080);
                }

                /* Częstotliwość cyfrowa — ukryta gdy LOCK aktywny */
                if (!locked) {
                    snprintf(str, sizeof(str), "%3lu.%03lu,%02lu",
                        (unsigned long)(freq / 1000000UL),
                        (unsigned long)((freq / 1000UL) % 1000UL),
                        (unsigned long)((freq / 10UL)   % 100UL));
                    disp_str16(str, 17, 105, 0xffd080);
                    disp_str12("MHz", 120, 106, 0xffd080);
                }

                /* LOCK — napis na dole ekranu */
                ui_draw_lock_icon(locked);

                /* Overlaye na tarczy */
                if (mode == DISP_MODE_SAVE_OK) {
                    ui_draw_saved_confirm(mem_idx, freq);
                } else if (mode == DISP_MODE_LOAD_OK) {
                    ui_draw_loaded_confirm(mem_idx, freq);
                } else if (mode == DISP_MODE_SAVE_PROMPT) {
                    ui_draw_save_prompt(mem_idx);
                }
            }

            display_trans65k();
            display_transfer_image();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/*===========================================================================
 *  backlight_init / backlight_set — PWM podswietlenia (LEDC)
 *===========================================================================*/
static void backlight_init(uint8_t pct)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_BL,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz         = LEDC_FREQ_BL,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .gpio_num   = DISP_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_BL,
        .timer_sel  = LEDC_TIMER_BL,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch);

    /* Ustaw poczatkowe wypelnienie */
    uint32_t duty = (uint32_t)pct * 255 / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_BL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_BL);
}

void backlight_set(uint8_t pct)
{
    if (pct > BRIGHTNESS_MAX) pct = BRIGHTNESS_MAX;
    if (pct < BRIGHTNESS_MIN) pct = BRIGHTNESS_MIN;
    uint32_t duty = (uint32_t)pct * 255 / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_BL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_BL);
}
