/*===========================================================================
 *  encoder.h — Enkoder obrotowy (PCNT v2, ESP-IDF 5.x)
 *
 *  Piny:
 *    ENC_PIN_A = GPIO17  (PULSE_INPUT)
 *    ENC_PIN_B = GPIO16  (PULSE_CTRL)
 *    ENC_PIN_SW = GPIO XX (przycisk push enkodera — do ustalenia)
 *
 *  Dynamiczny krok:
 *    Predkosc obrotu enkodera (impulsy/cykl) mapuje sie na mnoznik kroku.
 *    Przy wolnym kreceniu:  krok bazowy (np. 1 kHz)
 *    Przy srednim:          krok x10
 *    Przy szybkim:          krok x100
 *    Przy bardzo szybkim:   krok x1000
 *
 *  Przycisk SW enkodera:
 *    Krotkie (<500ms): reset dynamicznego mnoznika do x1
 *    Dlugie  (>=1s):   przelaczenie kierunku (f_rev)
 *===========================================================================*/
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/* GPIO przycisku SW enkodera — zmien na wlasciwy pin */
#define ENC_PIN_SW      34    /* input-only, wymaga zewn. pull-up 10k do VCC */

/* Liczba impulsow PCNT na jeden fizyczny skok (detent) enkodera.
 * Dla quadrature x4 z tym enkoderem = 2 imp/detent. */
#define ENC_PULSES_PER_DETENT  2

/* Progi predkosci (detenty na cykl 20ms) dla dynamicznego kroku */
#define ENC_ACCEL_THR1   2    /* >= 2  detenty/cykl -> mnoznik x10   */
#define ENC_ACCEL_THR2   5    /* >= 5  detenty/cykl -> mnoznik x100  */
#define ENC_ACCEL_THR3  12    /* >= 12 detenty/cykl -> mnoznik x1000 */

esp_err_t encoder_init(void);
void      encoder_task(void *arg);
