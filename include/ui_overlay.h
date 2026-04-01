/*===========================================================================
 *  ui_overlay.h — Graficzne overlaye VFO
 *  - Pytanie o zapis do pamieci (SAVE TO Mn?)
 *  - Potwierdzenie zapisu (SAVED / LOADED)
 *  - Kłódka LOCK w narozu ramki czestotliwosci
 *===========================================================================*/
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Narysuj ikonke kłódki w prawym gornym rogu ramki czestotliwosci */
void ui_draw_lock_icon(bool locked);

/* Narysuj overlay pytania o zapis: "SAVE TO Mn?" z YES/NO */
void ui_draw_save_prompt(int mem_idx);

/* Narysuj overlay potwierdzenia zapisu */
void ui_draw_saved_confirm(int mem_idx, uint32_t freq_hz);

/* Narysuj overlay potwierdzenia odczytu */
void ui_draw_loaded_confirm(int mem_idx, uint32_t freq_hz);

/* Pomocnik: narysuj wypelniony prostokat z zaokraglonymi rogami (symulacja) */
void ui_rounded_box(int x0, int y0, int x1, int y1,
                    uint32_t fill_color, uint32_t border_color);

/* Narysuj menu wyboru pasma amatorskiego */
void ui_draw_band_menu(int selected_idx);
