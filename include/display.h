/*===========================================================================
 *  display.h — Sterownik ST7735 128x160, landscape (ESP-IDF 5.x / SPI)
 *
 *  Orientacja: landscape  →  Nx=160 (szerokosc), Ny=128 (wysokosc)
 *  Framebuffer: R_GRAM[Nx][Ny], G_GRAM[Nx][Ny], B_GRAM[Nx][Ny]
 *               GRAM65k[Nx][Ny]  — 16-bit RGB565 po konwersji
 *===========================================================================*/
#pragma once
#include "esp_err.h"
#include <stdint.h>

/* Wymiary fizyczne wyswietlacza */
#define XW  128   /* fizyczna szerokosc panelu [px] */
#define YW  160   /* fizyczna wysokosc  panelu [px] */

/* Wymiary framebuffera w orientacji landscape */
#define NX  YW    /* 160 — os X (szerokosc w landscape) */
#define NY  XW    /* 128 — os Y (wysokosc  w landscape) */

esp_err_t display_init(void);
void      display_transfer_image(void);
void      display_trans65k(void);
void      display_command(uint8_t cmd);
void      display_brightness_set(uint8_t pct);
