/*===========================================================================
 *  graph.c — Framebuffer RGB + prymitywy graficzne (ESP-IDF 5.x)
 *
 *  Port z oryginalnego graph.cpp JF3HZB — bez zmian algorytmicznych,
 *  tylko usunieto #include <arduino.h> i poprawiono typy.
 *===========================================================================*/

#include "graph.h"
#include "display.h"
#include "font.h"
#include <string.h>

/*--- Framebuffer ---*/
uint8_t  R_GRAM[NX][NY];
uint8_t  G_GRAM[NX][NY];
uint8_t  B_GRAM[NX][NY];
uint16_t GRAM65k[NX][NY];

/* -------------------------------------------------------------------------
 *  Pomocnik — rozloz kolor 0xRRGGBB na bajty R, G, B
 * ---------------------------------------------------------------------- */
typedef union {
    uint32_t      lw;
    uint8_t       byte[4];
} color_u;

/* =========================================================================
 *  gram_clear
 * ====================================================================== */
void gram_clear(void)
{
    memset(R_GRAM, 0, sizeof(R_GRAM));
    memset(G_GRAM, 0, sizeof(G_GRAM));
    memset(B_GRAM, 0, sizeof(B_GRAM));
}

/* =========================================================================
 *  boxfill
 * ====================================================================== */
void boxfill(int x0, int y0, int x1, int y1, uint32_t color)
{
    color_u c; c.lw = color;
    if (x0 < 0)   x0 = 0;
    if (y0 < 0)   y0 = 0;
    if (x1 >= NX) x1 = NX - 1;
    if (y1 >= NY) y1 = NY - 1;
    for (int x = x0; x <= x1; x++)
        for (int y = y0; y <= y1; y++) {
            R_GRAM[x][y] = c.byte[2];
            G_GRAM[x][y] = c.byte[1];
            B_GRAM[x][y] = c.byte[0];
        }
}

/* =========================================================================
 *  draw_line — Bresenham
 * ====================================================================== */
void draw_line(int xs, int ys, int xe, int ye, uint32_t color)
{
    color_u c; c.lw = color;
    int dx = (xe > xs) ? xe - xs : xs - xe;
    int dy = (ye > ys) ? ye - ys : ys - ye;
    int sx = (xe > xs) ? 1 : -1;
    int sy = (ye > ys) ? 1 : -1;

#define SET_PX(xx, yy) \
    if ((xx) >= 0 && (xx) < NX && (yy) >= 0 && (yy) < NY) { \
        R_GRAM[xx][yy] = c.byte[2]; \
        G_GRAM[xx][yy] = c.byte[1]; \
        B_GRAM[xx][yy] = c.byte[0]; }

    if (dx == 0 && dy == 0) {
        SET_PX(xs, ys);
    } else if (dx == 0) {
        int s = (sy > 0) ? ys : ye;
        int e = (sy > 0) ? ye : ys;
        for (int j = s; j <= e; j++) { SET_PX(xs, j); }
    } else if (dy == 0) {
        int s = (sx > 0) ? xs : xe;
        int e = (sx > 0) ? xe : xs;
        for (int j = s; j <= e; j++) { SET_PX(j, ys); }
    } else if (dx >= dy) {
        int t = -(dx >> 1);
        int xx = xs, yy = ys;
        while (1) {
            SET_PX(xx, yy);
            if (xx == xe) break;
            xx += sx; t += dy;
            if (t >= 0) { yy += sy; t -= dx; }
        }
    } else {
        int t = -(dy >> 1);
        int xx = xs, yy = ys;
        while (1) {
            SET_PX(xx, yy);
            if (yy == ye) break;
            yy += sy; t += dx;
            if (t >= 0) { xx += sx; t -= dy; }
        }
    }
#undef SET_PX
}

/* =========================================================================
 *  draw_box
 * ====================================================================== */
void draw_box(int x0, int y0, int x1, int y1, uint32_t color)
{
    draw_line(x0, y0, x1, y0, color);
    draw_line(x0, y1, x1, y1, color);
    draw_line(x0, y0, x0, y1, color);
    draw_line(x1, y0, x1, y1, color);
}

/* =========================================================================
 *  bitrev8
 * ====================================================================== */
unsigned char bitrev8(unsigned char x)
{
    x = ((x & 0xAA) >> 1) | ((x & 0x55) << 1);
    x = ((x & 0xCC) >> 2) | ((x & 0x33) << 2);
    x = (x >> 4) | (x << 4);
    return x;
}

/* =========================================================================
 *  disp_chr8 — czcionka 5x8 px
 * ====================================================================== */
int disp_chr8(char c, int x, int y, uint32_t color)
{
    color_u cl; cl.lw = color;
    if (c == '\\') c = ' ';
    for (int k = 0; k < 5; k++) {
        uint8_t f8 = bitrev8((uint8_t)font[(uint8_t)c - 0x20][k]);
        if (x >= 0 && x < NX) {
            for (int j = 0; j < 8; j++) {
                if (f8 & 0x01) {
                    int yy = y + j;
                    if (yy >= 0 && yy < NY) {
                        R_GRAM[x][yy] = cl.byte[2];
                        G_GRAM[x][yy] = cl.byte[1];
                        B_GRAM[x][yy] = cl.byte[0];
                    }
                }
                f8 >>= 1;
            }
        }
        x++;
    }
    return x;
}

/* =========================================================================
 *  disp_chr12
 * ====================================================================== */
int disp_chr12(char c, int x, int y, uint32_t color)
{
    color_u cl; cl.lw = color;
    if (c == '\\') c = ' ';
    for (int k = 0; k < 13; k++) {
        uint16_t f12 = (uint16_t)font12[(uint8_t)c - 0x20][k];
        if (f12 == 0x0FFF) break;
        if (x >= 0 && x < NX) {
            for (int j = 0; j < 12; j++) {
                int yy = y + j;
                if ((f12 & 0x0001) && yy >= 0 && yy < NY) {
                    R_GRAM[x][yy] = cl.byte[2];
                    G_GRAM[x][yy] = cl.byte[1];
                    B_GRAM[x][yy] = cl.byte[0];
                }
                f12 >>= 1;
            }
        }
        x++;
    }
    return x + 1;
}

/* =========================================================================
 *  disp_chr16
 * ====================================================================== */
int disp_chr16(char c, int x, int y, uint32_t color)
{
    color_u cl; cl.lw = color;
    if (c == '\\') c = ' ';
    for (int k = 0; k < 21; k++) {
        uint16_t f16 = (uint16_t)font16[(uint8_t)c - 0x20][k];
        if (f16 == 0xFFFF) break;
        if (x >= 0 && x < NX) {
            for (int j = 0; j < 16; j++) {
                int yy = y + j;
                if ((f16 & 0x0001) && yy >= 0 && yy < NY) {
                    R_GRAM[x][yy] = cl.byte[2];
                    G_GRAM[x][yy] = cl.byte[1];
                    B_GRAM[x][yy] = cl.byte[0];
                }
                f16 >>= 1;
            }
        }
        x++;
    }
    return x + 1;
}

/* =========================================================================
 *  disp_chr20
 * ====================================================================== */
int disp_chr20(char c, int x, int y, uint32_t color)
{
    color_u cl; cl.lw = color;
    if (c == '\\') c = ' ';
    for (int k = 0; k < 23; k++) {
        uint32_t f20 = (uint32_t)font20[(uint8_t)c - 0x20][k];
        if (f20 == 0xFFFFF) break;
        if (x >= 0 && x < NX) {
            for (int j = 0; j < 20; j++) {
                int yy = y + j;
                if ((f20 & 0x00001) && yy >= 0 && yy < NY) {
                    R_GRAM[x][yy] = cl.byte[2];
                    G_GRAM[x][yy] = cl.byte[1];
                    B_GRAM[x][yy] = cl.byte[0];
                }
                f20 >>= 1;
            }
        }
        x++;
    }
    return x + 1;
}

/* =========================================================================
 *  disp_str* — wyswietl ciag znakow
 * ====================================================================== */
void disp_str8(const char *s, int x, int y, uint32_t color)
{
    for (int k = 0; k < 24 && s[k]; k++) {
        x = disp_chr8(s[k], x, y, color);
        x += 1;
    }
}

void disp_str12(const char *s, int x, int y, uint32_t color)
{
    for (int k = 0; k < 24 && s[k]; k++) {
        x = disp_chr12(s[k], x, y, color);
        x += 1;
    }
}

void disp_str16(const char *s, int x, int y, uint32_t color)
{
    for (int k = 0; k < 24 && s[k]; k++) {
        x = disp_chr16(s[k], x, y, color);
        x += 1;
    }
}

void disp_str20(const char *s, int x, int y, uint32_t color)
{
    for (int k = 0; k < 24 && s[k]; k++) {
        x = disp_chr20(s[k], x, y, color);
        x += 1;
    }
}
