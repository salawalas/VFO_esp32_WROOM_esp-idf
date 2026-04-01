/*===========================================================================
 *  si5351.h — Sterownik Si5351A  (ESP-IDF 5.x, hardware I2C 400 kHz)
 *
 *  Architektura wyjsc:
 *    CLK0  — nosna glowna          (PLL_A, MS0, faza 0°)
 *    CLK1  — LO / VFO              (PLL_B, MS1, przestrajane enkoderem)
 *    CLK2  — nosna Q (90°)         (PLL_A, MS2, faza 90° = INV wzgledem CLK0)
 *
 *  PLL_A  →  MS0 (CLK0) + MS2 (CLK2)   nosna I/Q — wspolny PLL, latwa sync faz
 *  PLL_B  →  MS1 (CLK1)                 LO — niezalezny PLL, pelna swoboda strojenia
 *===========================================================================*/
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* Inicjalizacja — wywolac raz po starcie */
esp_err_t si5351_init(void);

/* Ustaw czestotliwosc LO (CLK1, PLL_B) — glowna czestotliwosc VFO */
void si5351_set_freq(uint32_t freq_hz);

/* Ustaw czestotliwosc nosnej (CLK0 + CLK2, PLL_A) */
void si5351_set_car_freq(uint32_t freq_hz, bool enable);

/* Wycisz / odcisz pojedyncze wyjscie (clk_num = 0, 1 lub 2) */
void si5351_output_enable(uint8_t clk_num, bool enable);

/* Kalibracja kwarcu — koryguje efektywna czestotliwosc XTAL [Hz], zakres +/-5000 */
void si5351_set_xtal_cal(int32_t cal);
