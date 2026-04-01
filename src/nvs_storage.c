/*===========================================================================
 *  nvs_storage.c — Zapis/odczyt NVS (ESP-IDF 5.x)
 *
 *  Namespace: "vfo"
 *  Klucze:
 *    "last_frq"  — uint32  ostatnia czestotliwosc VFO
 *    "mem0".."mem9" — uint32  banki pamieci
 *    "step"      — int32   indeks kroku
 *    "xtal_cal"  — int32   kalibracja kwarcu [Hz]
 *    "if_off"    — int32   IF offset [Hz]
 *    "bright"    — uint8   jasnosc podswietlenia [%]
 *    "last_mem"  — int32   ostatni bank pamieci
 *    "rit_off"   — int32   RIT offset [Hz]
 *
 *  autosave_task:
 *    Skanuje flage f_autosave_arm co AUTOSAVE_SCAN_MS.
 *    Gdy flaga jest ustawiona, startuje odliczanie AUTOSAVE_DELAY_MS.
 *    Kazde nowe ustawienie flagi resetuje odliczanie (activity reset).
 *    Po uplywie czasu — zapisuje freq, step_idx, last_mem_idx,
 *    i tylko te banki pamieci gdzie mem_dirty[i]=true.
 *===========================================================================*/

#include "nvs_storage.h"
#include "vfo_state.h"
#include "config.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AUTOSAVE_SCAN_MS    200     /* okres skanowania flagi [ms] */

/* Persystentny uchwyt NVS — otwarty raz w nvs_storage_init */
static nvs_handle_t s_nvs_handle = 0;
static bool         s_handle_open = false;

/*===========================================================================
 *  nvs_storage_init
 *===========================================================================*/
esp_err_t nvs_storage_init(void)
{
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NVS, "nvs_open(%s) blad: %s",
                 NVS_NAMESPACE, esp_err_to_name(err));
        s_handle_open = false;
        return err;
    }
    s_handle_open = true;
    ESP_LOGI(TAG_NVS, "NVS namespace \"%s\": OK", NVS_NAMESPACE);
    return ESP_OK;
}

/*===========================================================================
 *  nvs_load_last_freq
 *===========================================================================*/
esp_err_t nvs_load_last_freq(uint32_t *freq_hz)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_get_u32(s_nvs_handle, NVS_KEY_LAST_FREQ, freq_hz);
    if (err == ESP_OK) {
        ESP_LOGI(TAG_NVS, "Wczytano last_freq = %lu Hz", (unsigned long)*freq_hz);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG_NVS, "last_freq: brak wpisu (pierwszy start)");
    } else {
        ESP_LOGW(TAG_NVS, "nvs_get_u32(last_freq): %s", esp_err_to_name(err));
    }
    return err;
}

/*===========================================================================
 *  nvs_save_last_freq
 *===========================================================================*/
esp_err_t nvs_save_last_freq(uint32_t freq_hz)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_set_u32(s_nvs_handle, NVS_KEY_LAST_FREQ, freq_hz);
    if (err == ESP_OK) err = nvs_commit(s_nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_NVS, "Zapisano last_freq = %lu Hz", (unsigned long)freq_hz);
    } else {
        ESP_LOGW(TAG_NVS, "nvs_save_last_freq blad: %s", esp_err_to_name(err));
    }
    return err;
}

/*===========================================================================
 *  nvs_load_mem
 *===========================================================================*/
esp_err_t nvs_load_mem(int idx, uint32_t *freq_hz)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    if (idx < 0 || idx >= VFO_MEM_COUNT) return ESP_ERR_INVALID_ARG;

    char key[16];
    snprintf(key, sizeof(key), "%s%d", NVS_KEY_MEM_PREFIX, idx);

    esp_err_t err = nvs_get_u32(s_nvs_handle, key, freq_hz);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_NVS, "Wczytano M%d = %lu Hz", idx, (unsigned long)*freq_hz);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG_NVS, "nvs_get_u32(%s): %s", key, esp_err_to_name(err));
    }
    return err;
}

/*===========================================================================
 *  nvs_save_mem
 *===========================================================================*/
esp_err_t nvs_save_mem(int idx, uint32_t freq_hz)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    if (idx < 0 || idx >= VFO_MEM_COUNT) return ESP_ERR_INVALID_ARG;

    char key[16];
    snprintf(key, sizeof(key), "%s%d", NVS_KEY_MEM_PREFIX, idx);

    esp_err_t err = nvs_set_u32(s_nvs_handle, key, freq_hz);
    if (err == ESP_OK) err = nvs_commit(s_nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG_NVS, "Zapisano M%d = %lu Hz", idx, (unsigned long)freq_hz);
    } else {
        ESP_LOGW(TAG_NVS, "nvs_save_mem(%d) blad: %s", idx, esp_err_to_name(err));
    }
    return err;
}

/*===========================================================================
 *  nvs_save_step_idx / nvs_load_step_idx
 *===========================================================================*/
esp_err_t nvs_save_step_idx(int idx)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_set_i32(s_nvs_handle, NVS_KEY_STEP, (int32_t)idx);
    if (err == ESP_OK) err = nvs_commit(s_nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_NVS, "Zapisano step_idx = %d", idx);
    } else {
        ESP_LOGW(TAG_NVS, "nvs_save_step_idx blad: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t nvs_load_step_idx(int *idx)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    int32_t val = 0;
    esp_err_t err = nvs_get_i32(s_nvs_handle, NVS_KEY_STEP, &val);
    if (err == ESP_OK) {
        *idx = (int)val;
        ESP_LOGD(TAG_NVS, "Wczytano step_idx = %d", *idx);
    }
    return err;
}

/*===========================================================================
 *  nvs_save_xtal_cal / nvs_load_xtal_cal
 *===========================================================================*/
esp_err_t nvs_save_xtal_cal(int32_t cal)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_set_i32(s_nvs_handle, NVS_KEY_XTAL_CAL, cal);
    if (err == ESP_OK) err = nvs_commit(s_nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_NVS, "Zapisano xtal_cal = %ld Hz", (long)cal);
    } else {
        ESP_LOGW(TAG_NVS, "nvs_save_xtal_cal blad: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t nvs_load_xtal_cal(int32_t *cal)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_get_i32(s_nvs_handle, NVS_KEY_XTAL_CAL, cal);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_NVS, "Wczytano xtal_cal = %ld Hz", (long)*cal);
    }
    return err;
}

/*===========================================================================
 *  nvs_save_if_offset / nvs_load_if_offset
 *===========================================================================*/
esp_err_t nvs_save_if_offset(int32_t off)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_set_i32(s_nvs_handle, NVS_KEY_IF_OFFSET, off);
    if (err == ESP_OK) err = nvs_commit(s_nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_NVS, "Zapisano if_offset = %ld Hz", (long)off);
    } else {
        ESP_LOGW(TAG_NVS, "nvs_save_if_offset blad: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t nvs_load_if_offset(int32_t *off)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_get_i32(s_nvs_handle, NVS_KEY_IF_OFFSET, off);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_NVS, "Wczytano if_offset = %ld Hz", (long)*off);
    }
    return err;
}

/*===========================================================================
 *  nvs_save_brightness / nvs_load_brightness
 *===========================================================================*/
esp_err_t nvs_save_brightness(uint8_t pct)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_set_u8(s_nvs_handle, NVS_KEY_BRIGHTNESS, pct);
    if (err == ESP_OK) err = nvs_commit(s_nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_NVS, "Zapisano brightness = %d%%", (int)pct);
    } else {
        ESP_LOGW(TAG_NVS, "nvs_save_brightness blad: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t nvs_load_brightness(uint8_t *pct)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_get_u8(s_nvs_handle, NVS_KEY_BRIGHTNESS, pct);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_NVS, "Wczytano brightness = %d%%", (int)*pct);
    }
    return err;
}

/*===========================================================================
 *  nvs_save_last_mem / nvs_load_last_mem
 *===========================================================================*/
esp_err_t nvs_save_last_mem(int idx)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_set_i32(s_nvs_handle, NVS_KEY_LAST_MEM, (int32_t)idx);
    if (err == ESP_OK) err = nvs_commit(s_nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_NVS, "Zapisano last_mem = %d", idx);
    } else {
        ESP_LOGW(TAG_NVS, "nvs_save_last_mem blad: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t nvs_load_last_mem(int *idx)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    int32_t val = 0;
    esp_err_t err = nvs_get_i32(s_nvs_handle, NVS_KEY_LAST_MEM, &val);
    if (err == ESP_OK) {
        *idx = (int)val;
        ESP_LOGD(TAG_NVS, "Wczytano last_mem = %d", *idx);
    }
    return err;
}

/*===========================================================================
 *  nvs_save_rit / nvs_load_rit
 *===========================================================================*/
esp_err_t nvs_save_rit(int32_t off)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_set_i32(s_nvs_handle, NVS_KEY_RIT, off);
    if (err == ESP_OK) err = nvs_commit(s_nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_NVS, "Zapisano rit_offset = %ld Hz", (long)off);
    } else {
        ESP_LOGW(TAG_NVS, "nvs_save_rit blad: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t nvs_load_rit(int32_t *off)
{
    if (!s_handle_open) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_get_i32(s_nvs_handle, NVS_KEY_RIT, off);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_NVS, "Wczytano rit_offset = %ld Hz", (long)*off);
    }
    return err;
}

/*===========================================================================
 *  autosave_task — Core 1, priorytet 2
 *
 *  Logika:
 *    - skanuje f_autosave_arm co AUTOSAVE_SCAN_MS
 *    - gdy flaga jest ustawiona: zeruje ja i startuje/resetuje odliczanie
 *    - gdy odliczanie dobiegnie do zera: zapisuje freq, step_idx, last_mem_idx
 *      i tylko te banki gdzie mem_dirty[i]=true (potem zeruje dirty flag)
 *===========================================================================*/
void autosave_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG_NVS, "autosave_task start — rdzen %d", xPortGetCoreID());

    uint32_t countdown_ms = 0;     /* 0 = nieaktywny */

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(AUTOSAVE_SCAN_MS));

        /* Sprawdz flage */
        VFO_LOCK();
        bool armed = g_vfo.f_autosave_arm;
        if (armed) g_vfo.f_autosave_arm = false;
        VFO_UNLOCK();

        if (armed) {
            /* Nowa aktywnosc — resetuj odliczanie */
            countdown_ms = AUTOSAVE_DELAY_MS;
            continue;
        }

        if (countdown_ms == 0) {
            /* Brak aktywnosci — nic do roboty */
            continue;
        }

        /* Odliczanie aktywne */
        if (countdown_ms > AUTOSAVE_SCAN_MS) {
            countdown_ms -= AUTOSAVE_SCAN_MS;
        } else {
            countdown_ms = 0;

            /* Czas uplynal — pobierz dane pod mutexem */
            VFO_LOCK();
            uint32_t freq     = g_vfo.freq;
            int      step_idx = g_vfo.step_idx;
            int      last_mem = g_vfo.last_mem_idx;
            uint32_t mem_snap[VFO_MEM_COUNT];
            bool     dirty_snap[VFO_MEM_COUNT];
            for (int i = 0; i < VFO_MEM_COUNT; i++) {
                mem_snap[i]   = g_vfo.mem_freq[i];
                dirty_snap[i] = g_vfo.mem_dirty[i];
                if (dirty_snap[i]) g_vfo.mem_dirty[i] = false;  /* wyczysc */
            }
            VFO_UNLOCK();

            /* Zapisz czestotliwosc i parametry */
            ESP_LOGI(TAG_NVS, "Autozapis: freq=%lu Hz step=%d last_mem=%d",
                     (unsigned long)freq, step_idx, last_mem);
            nvs_save_last_freq(freq);
            nvs_save_step_idx(step_idx);
            nvs_save_last_mem(last_mem);

            /* Zapisz tylko brudne banki */
            for (int i = 0; i < VFO_MEM_COUNT; i++) {
                if (dirty_snap[i] && mem_snap[i] != 0) {
                    nvs_save_mem(i, mem_snap[i]);
                }
            }
        }
    }
}
