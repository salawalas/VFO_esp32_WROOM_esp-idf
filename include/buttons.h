/*===========================================================================
 *  buttons.h — Przyciski tact switch (ESP-IDF 5.x)
 *
 *  Przyciski (active LOW, INPUT_PULLUP):
 *    BTN_STEP_DN (GPIO27) — krok w dol
 *    BTN_STEP_UP (GPIO26) — krok w gore
 *    BTN_MEM     (GPIO25) — pamiec / YES w dialogu zapisu
 *    BTN_SAVE    (GPIO32) — zapis / NO w dialogu
 *    BTN_BAND    (GPIO33) — menu wyboru pasma (5. przycisk)
 *
 *  Logika czasowa:
 *    Krotkie nacisniecie (<500ms): akcja podstawowa
 *    Dlugie  (>=1000ms):           akcja alternatywna
 *    LOCK: BTN_STEP_DN + BTN_STEP_UP trzymane >= 1s jednoczesnie
 *===========================================================================*/
#pragma once
#include "esp_err.h"
#include <stdbool.h>

/* GPIO 5. przycisku BAND — zmien gdy znany */
#define BTN_BAND    33    /* wbudowany pull-up dostepny */

esp_err_t buttons_init(void);
void      buttons_task(void *arg);
