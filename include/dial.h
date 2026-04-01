/*===========================================================================
 *  dial.h — Analogowa tarcza czestotliwosci (ESP-IDF 5.x)
 *===========================================================================*/
#pragma once
#include <stdint.h>

void dial_init(void);
void dial_draw(uint32_t freq_hz);
