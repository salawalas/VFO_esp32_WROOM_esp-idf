/*===========================================================================
 *  graph.h — Framebuffer RGB + prymitywy graficzne (ESP-IDF 5.x)
 *===========================================================================*/
#pragma once
#include <stdint.h>
#include "display.h"

/* Framebuffer — 3 kanaly RGB + 16-bit dla SPI */
extern uint8_t  R_GRAM[NX][NY];
extern uint8_t  G_GRAM[NX][NY];
extern uint8_t  B_GRAM[NX][NY];
extern uint16_t GRAM65k[NX][NY];

/* Prymitywy */
void gram_clear(void);
void boxfill(int x0, int y0, int x1, int y1, uint32_t color);
void draw_line(int xs, int ys, int xe, int ye, uint32_t color);
void draw_box(int x0, int y0, int x1, int y1, uint32_t color);

/* Tekst — 4 rozmiary czcionek */
void disp_str8 (const char *s, int x, int y, uint32_t color);
void disp_str12(const char *s, int x, int y, uint32_t color);
void disp_str16(const char *s, int x, int y, uint32_t color);
void disp_str20(const char *s, int x, int y, uint32_t color);

/* Znaki pomocnicze (uzywane przez dial.c) */
int  disp_chr8 (char c, int x, int y, uint32_t color);
int  disp_chr12(char c, int x, int y, uint32_t color);
int  disp_chr16(char c, int x, int y, uint32_t color);
int  disp_chr20(char c, int x, int y, uint32_t color);
unsigned char bitrev8(unsigned char x);
