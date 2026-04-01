/*===========================================================================
 *  nvs_storage.h — Zapis/odczyt NVS (ESP-IDF 5.x)
 *===========================================================================*/
#pragma once
#include <stdint.h>
#include "esp_err.h"

esp_err_t nvs_storage_init(void);
esp_err_t nvs_load_last_freq(uint32_t *freq_hz);
esp_err_t nvs_save_last_freq(uint32_t freq_hz);
esp_err_t nvs_load_mem(int idx, uint32_t *freq_hz);
esp_err_t nvs_save_mem(int idx, uint32_t freq_hz);
void      autosave_task(void *arg);

esp_err_t nvs_save_step_idx(int idx);
esp_err_t nvs_load_step_idx(int *idx);
esp_err_t nvs_save_xtal_cal(int32_t cal);
esp_err_t nvs_load_xtal_cal(int32_t *cal);
esp_err_t nvs_save_if_offset(int32_t off);
esp_err_t nvs_load_if_offset(int32_t *off);
esp_err_t nvs_save_brightness(uint8_t pct);
esp_err_t nvs_load_brightness(uint8_t *pct);
esp_err_t nvs_save_last_mem(int idx);
esp_err_t nvs_load_last_mem(int *idx);
esp_err_t nvs_save_rit(int32_t off);
esp_err_t nvs_load_rit(int32_t *off);
