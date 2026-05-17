/*===========================================================================
 *  ui_overlay.c — Graficzne overlaye VFO
 *
 *  Wszystkie grafiki sa rysowane bezposrednio w framebufferze RGB.
 *  Kolory w formacie 0xRRGGBB.
 *
 *  Rozmieszczenie na ekranie 160x128 (landscape):
 *
 *   ┌─────────────────────────────────┐  y=0
 *   │  ramka czestotliwosci           │  y=0..49
 *   │  14.230,00 MHz                  │
 *   ├─────────────────────────────────┤  y=49
 *   │  VFO    STEPS 1 kHz             │  y=50..69
 *   ├─────────────────────────────────┤  y=70
 *   │                                 │
 *   │  [LOCK: ikona srodek, tekst dol]│  y=70..127
 *   │                                 │
 *   └─────────────────────────────────┘  y=127
 *===========================================================================*/

#include "ui_overlay.h"
#include "graph.h"
#include "display.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

/* Kolory overlayow */
#define COL_BLACK       0x000000
#define COL_DARK_AMBER  0x1a1000
#define COL_AMBER       0xffaa00
#define COL_YELLOW      0xffff00
#define COL_DARK_GREEN  0x001400
#define COL_GREEN       0x00cc00
#define COL_BRIGHT_GRN  0x00ff44
#define COL_DARK_RED    0x1a0000
#define COL_RED         0xff2222
#define COL_WHITE       0xffffff
#define COL_CYAN        0x00ffff
#define COL_GRAY        0x888888
#define COL_DARK_BLUE   0x000d1a
#define COL_BLUE_BORDER 0x4444aa
#define COL_SKY_BLUE    0x00ccff

/* Okna SAVE/LOAD sa przesuniete nizej fizycznie, poza ramke czestotliwosci. */
#define UI_MSG_Y_SHIFT  (-25)

/* -------------------------------------------------------------------------
 *  ui_rounded_box — prostokat z obramowaniem (2px border)
 * ---------------------------------------------------------------------- */
void ui_rounded_box(int x0, int y0, int x1, int y1,
                    uint32_t fill_color, uint32_t border_color)
{
    /* Wypelnienie */
    boxfill(x0 + 1, y0 + 1, x1 - 1, y1 - 1, fill_color);
    /* Obramowanie — 4 krawedzie */
    draw_line(x0 + 2, y0,     x1 - 2, y0,     border_color);
    draw_line(x0 + 2, y1,     x1 - 2, y1,     border_color);
    draw_line(x0,     y0 + 2, x0,     y1 - 2, border_color);
    draw_line(x1,     y0 + 2, x1,     y1 - 2, border_color);
    /* Narozniki */
    draw_line(x0 + 1, y0 + 1, x0 + 1, y0 + 1, border_color);
    draw_line(x1 - 1, y0 + 1, x1 - 1, y0 + 1, border_color);
    draw_line(x0 + 1, y1 - 1, x0 + 1, y1 - 1, border_color);
    draw_line(x1 - 1, y1 - 1, x1 - 1, y1 - 1, border_color);
}

/* -------------------------------------------------------------------------
 *  Ikona kłódki — duzy symbol na srodku ekranu
 *
 *  Kształt kłódki:
 *    - kabłąk: łuk U (2 pionowe + poziome na górze)
 *    - trzon:  prostokat
 *    - dziurka: kółko + kreska
 * ---------------------------------------------------------------------- */
void ui_draw_lock_icon(bool locked)
{
    if (!locked) return;

    uint32_t col = COL_RED;

    const int body_w = 46;
    const int body_h = 40;
    const int body_x = (NX - body_w) / 2;
    const int body_y2 = (NY / 2) + 1;
    const int body_y = body_y2 - body_h;
    const int body_x2 = body_x + body_w;

    const int shackle_x = body_x + 8;
    const int shackle_x2 = body_x2 - 8;
    const int shackle_y = body_y2 - 4;
    const int shackle_y2 = body_y2 + 24;
    const int shackle_thick = 4;

    /* Kabłąk — rysowany odwrotnie w framebufferze, bo panel odwraca os Y */
    boxfill(shackle_x, shackle_y,
            shackle_x + shackle_thick - 1, shackle_y2 - shackle_thick, col);
    boxfill(shackle_x2 - shackle_thick + 1, shackle_y,
            shackle_x2, shackle_y2 - shackle_thick, col);
    boxfill(shackle_x + shackle_thick, shackle_y2 - shackle_thick + 1,
            shackle_x2 - shackle_thick, shackle_y2, col);

    /* Wnetrze kabłąka — zostawia czytelny otwor */
    boxfill(shackle_x + shackle_thick, shackle_y + 2,
            shackle_x2 - shackle_thick, shackle_y2 - shackle_thick, COL_BLACK);

    /* Trzon kłódki */
    ui_rounded_box(body_x, body_y, body_x2, body_y2, COL_DARK_RED, col);
    draw_box(body_x + 3, body_y + 3, body_x2 - 3, body_y2 - 3, 0x660000);

    /* Dziurka */
    const int key_x = NX / 2;
    const int key_y = body_y2 - 12;
    boxfill(key_x - 5, key_y - 2, key_x + 5, key_y + 4, COL_BLACK);
    draw_line(key_x - 4, key_y - 3, key_x + 4, key_y - 3, col);
    draw_line(key_x - 6, key_y - 1, key_x - 6, key_y + 3, col);
    draw_line(key_x + 6, key_y - 1, key_x + 6, key_y + 3, col);
    draw_line(key_x - 4, key_y + 5, key_x + 4, key_y + 5, col);
    boxfill(key_x - 2, key_y - 14, key_x + 2, key_y - 3, COL_BLACK);
    draw_line(key_x - 3, key_y - 13, key_x - 3, key_y - 4, col);
    draw_line(key_x + 3, key_y - 13, key_x + 3, key_y - 4, col);
    draw_line(key_x - 3, key_y - 14, key_x + 3, key_y - 14, col);

    /* Napis LOCK — sam dol ekranu, czcionka 20px, wycentrowany.
     * Szerokosc: L=13 O=16 C=15 K=16 = 60px. */
    const int lock_text_w = 60;
    const int lock_box_w = 72;
    const int lock_box_h = 22;
    const int lock_box_x = (NX - lock_box_w) / 2;
    const int lock_box_y = 0;
    ui_rounded_box(lock_box_x, lock_box_y,
                   lock_box_x + lock_box_w, lock_box_y + lock_box_h,
                   COL_DARK_RED, col);
    disp_str20("LOCK", (NX - lock_text_w) / 2, lock_box_y + 2, col);
}

/* -------------------------------------------------------------------------
 *  Ikona dyskietki — 16x16 px
 *  Uzywana przy overlayach zapisu
 * ---------------------------------------------------------------------- */
static void draw_floppy_icon(int x, int y, uint32_t col)
{
    /* Obudowa dyskietki */
    ui_rounded_box(x, y, x + 15, y + 15, COL_BLACK, col);
    /* Okienko etykiety (gorna czesc) */
    boxfill(x + 2, y + 1, x + 13, y + 6, col);
    /* Szczelina na etykiete */
    draw_line(x + 10, y + 1, x + 10, y + 6, COL_BLACK);
    /* Otwor na nośnik (dolna czesc) */
    ui_rounded_box(x + 3, y + 9, x + 12, y + 14, COL_BLACK, col);
}

/* -------------------------------------------------------------------------
 *  Ikona check (fajka) — zielona
 * ---------------------------------------------------------------------- */
static void draw_check_icon(int x, int y, uint32_t col)
{
    /* Okrag */
    draw_line(x + 2,  y,      x + 10, y,      col);
    draw_line(x,      y + 2,  x,      y + 9,  col);
    draw_line(x + 12, y + 2,  x + 12, y + 9,  col);
    draw_line(x + 2,  y + 11, x + 10, y + 11, col);
    draw_line(x + 1,  y + 1,  x + 1,  y + 1,  col);
    draw_line(x + 11, y + 1,  x + 11, y + 1,  col);
    draw_line(x + 1,  y + 10, x + 1,  y + 10, col);
    draw_line(x + 11, y + 10, x + 11, y + 10, col);
    /* Fajka wewnatrz */
    draw_line(x + 2, y + 5,  x + 4,  y + 8,  COL_BRIGHT_GRN);
    draw_line(x + 3, y + 5,  x + 5,  y + 8,  COL_BRIGHT_GRN);
    draw_line(x + 4, y + 8,  x + 10, y + 3,  COL_BRIGHT_GRN);
    draw_line(x + 4, y + 9,  x + 10, y + 4,  COL_BRIGHT_GRN);
}

/* =========================================================================
 *  ui_draw_save_prompt — overlay "SAVE TO Mn?"
 *
 *  Overlay zajmuje srodkowa czesc ekranu (y=42..88)
 *  Layout:
 *    [ikona dyskietki]  SAVE TO M3?
 *    [BTN_MEM=YES]      [BTN_SAVE=NO]
 * ====================================================================== */
void ui_draw_save_prompt(int mem_idx)
{
    char str[32];

    /* Tlo overlaya */
    ui_rounded_box(12, 72 + UI_MSG_Y_SHIFT,
                   148, 118 + UI_MSG_Y_SHIFT, COL_DARK_AMBER, COL_AMBER);

    /* Ikona dyskietki */
    draw_floppy_icon(20, 78 + UI_MSG_Y_SHIFT, COL_AMBER);

    /* Tekst pytania */
    snprintf(str, sizeof(str), "SAVE TO M%d?", mem_idx);
    disp_str12(str, 42, 78 + UI_MSG_Y_SHIFT, COL_YELLOW);

    /* Linia separatora */
    draw_line(14, 97 + UI_MSG_Y_SHIFT, 146, 97 + UI_MSG_Y_SHIFT, COL_AMBER);

    /* Przycisk YES (BTN_MEM — lewy) */
    ui_rounded_box(18, 101 + UI_MSG_Y_SHIFT,
                   72, 115 + UI_MSG_Y_SHIFT, 0x003300, COL_GREEN);
    disp_str8("YES(MEM)", 22, 104 + UI_MSG_Y_SHIFT, COL_BRIGHT_GRN);

    /* Przycisk NO (BTN_SAVE — prawy) */
    ui_rounded_box(88, 101 + UI_MSG_Y_SHIFT,
                   142, 115 + UI_MSG_Y_SHIFT, COL_DARK_RED, COL_RED);
    disp_str8("NO(SAVE)", 92, 104 + UI_MSG_Y_SHIFT, COL_RED);
}

/* =========================================================================
 *  ui_draw_saved_confirm — overlay potwierdzenia zapisu
 * ====================================================================== */
void ui_draw_saved_confirm(int mem_idx, uint32_t freq_hz)
{
    char str[32];

    ui_rounded_box(12, 72 + UI_MSG_Y_SHIFT,
                   148, 118 + UI_MSG_Y_SHIFT, COL_DARK_GREEN, COL_GREEN);

    /* Ikona check */
    draw_check_icon(18, 80 + UI_MSG_Y_SHIFT, COL_GREEN);

    /* Napis SAVED */
    disp_str16("SAVED", 40, 76 + UI_MSG_Y_SHIFT, COL_BRIGHT_GRN);

    /* Linia */
    draw_line(14, 97 + UI_MSG_Y_SHIFT, 146, 97 + UI_MSG_Y_SHIFT, COL_GREEN);

    /* Czestotliwosc */
    snprintf(str, sizeof(str), "M%d = %lu.%03lu MHz",
             mem_idx,
             (unsigned long)(freq_hz / 1000000UL),
             (unsigned long)((freq_hz / 1000UL) % 1000UL));
    disp_str8(str, 16, 101 + UI_MSG_Y_SHIFT, COL_BRIGHT_GRN);
}

/* =========================================================================
 *  ui_draw_loaded_confirm — overlay potwierdzenia odczytu
 * ====================================================================== */
void ui_draw_loaded_confirm(int mem_idx, uint32_t freq_hz)
{
    char str[32];

    ui_rounded_box(12, 72 + UI_MSG_Y_SHIFT,
                   148, 118 + UI_MSG_Y_SHIFT, COL_DARK_BLUE, 0x4488ff);

    /* Strzalka wczytania (prosta grafika) */
    draw_line(18, 93 + UI_MSG_Y_SHIFT, 30, 93 + UI_MSG_Y_SHIFT, COL_CYAN);
    draw_line(18, 93 + UI_MSG_Y_SHIFT, 22, 89 + UI_MSG_Y_SHIFT, COL_CYAN);
    draw_line(18, 93 + UI_MSG_Y_SHIFT, 22, 97 + UI_MSG_Y_SHIFT, COL_CYAN);

    /* Napis LOADED */
    disp_str16("LOAD", 40, 76 + UI_MSG_Y_SHIFT, COL_CYAN);

    draw_line(14, 97 + UI_MSG_Y_SHIFT, 146, 97 + UI_MSG_Y_SHIFT, 0x4488ff);

    snprintf(str, sizeof(str), "M%d = %lu.%03lu MHz",
             mem_idx,
             (unsigned long)(freq_hz / 1000000UL),
             (unsigned long)((freq_hz / 1000UL) % 1000UL));
    disp_str8(str, 16, 101 + UI_MSG_Y_SHIFT, COL_CYAN);
}

/* =========================================================================
 *  ui_draw_band_menu — overlay menu wyboru pasma
 *
 *  Wyswietla liste 8 pasm z podswietlonym aktualnym wyborem.
 *  Pasma sa przewijane enkoderem, wybor zatwierdza BTN_MEM.
 *
 *  Layout (160x128, od y=0):
 *    ┌─────────────────────────┐  y=0
 *    │     SELECT BAND         │  y=0..14  naglowek
 *    ├─────────────────────────┤  y=14
 *    │  30m  10.100  MHz       │  y=15..27
 *    │ ►20m  14.000  MHz       │  y=27..39  <- wybrany (podswietlony)
 *    │  17m  18.068  MHz       │  y=39..51
 *    │  15m  21.000  MHz       │  y=51..63
 *    │  12m  24.940  MHz       │  y=63..75
 *    │  10m  28.000  MHz       │  y=75..87
 *    │   6m  50.000  MHz       │  y=87..99
 *    │   FM 100.000  MHz       │  y=99..111
 *    ├─────────────────────────┤  y=111
 *    │ MEM=OK  SAVE=CANCEL     │  y=112..127  podpowiedz
 *    └─────────────────────────┘  y=127
 * ====================================================================== */

/* Dane pasm — z config.h (VFO_BANDS) */
typedef struct { const char *name; } band_label_t;
static const band_label_t BAND_LABELS[] = {
    { "30m  10.100 MHz" },
    { "20m  14.000 MHz" },
    { "17m  18.068 MHz" },
    { "15m  21.000 MHz" },
    { "12m  24.940 MHz" },
    { "10m  28.000 MHz" },
    { " 6m  50.000 MHz" },
    { " FM 100.000 MHz" },
};
#define BAND_LABEL_COUNT  8

void ui_draw_band_menu(int selected_idx)
{
    const uint32_t band_text_color = COL_SKY_BLUE;
    const uint32_t band_selected_text_color = COL_CYAN;

    /* Tlo calego ekranu — ciemny granat */
    boxfill(0, 0, NX - 1, NY - 1, 0x00060F);

    /* Naglowek */
    ui_rounded_box(2, 2, NX - 3, 14, 0x001a33, 0x0088ff);
    disp_str8(">> SELECT BAND <<", 18, 4, band_text_color);

    /* Lista pasm */
    for (int i = 0; i < BAND_LABEL_COUNT; i++) {
        int y0 = 15 + i * 13;
        int y1 = y0 + 12;

        if (i == selected_idx) {
            /* Podswietlony wiersz */
            boxfill(4, y0, NX - 5, y1, 0x003300);
            draw_line(4,  y0, NX - 5, y0, band_selected_text_color);
            draw_line(4,  y1, NX - 5, y1, band_selected_text_color);
            /* Strzalka wskaznika */
            disp_str8(">", 6, y0 + 2, band_selected_text_color);
            disp_str8(BAND_LABELS[i].name, 16, y0 + 2, band_selected_text_color);
        } else {
            disp_str8(BAND_LABELS[i].name, 16, y0 + 2, band_text_color);
        }
    }

    /* Dolna belka — podpowiedz */
    boxfill(0, NY - 14, NX - 1, NY - 1, 0x000d00);
    draw_line(0, NY - 14, NX - 1, NY - 14, 0x004400);
    disp_str8("MEM=OK", 4,  NY - 11, 0x00aa00);
    disp_str8("SAVE=X", 88, NY - 11, 0x884400);
}

/* =========================================================================
 *  ui_draw_brightness — ekran regulacji jasnosci podswietlenia
 *
 *  Wyswietla:
 *    - naglowek BRIGHTNESS
 *    - pasek postępu (graficzny)
 *    - wartosc procentowa (duza czcionka)
 *    - podpowiedz: ENC=zmien  MEM/CAL=save  SAVE=cancel
 * ====================================================================== */
void ui_draw_brightness(uint8_t pct)
{
    char str[24];

    /* Tlo */
    boxfill(0, 0, NX - 1, NY - 1, 0x0a0808);

    /* Naglowek */
    ui_rounded_box(2, 2, NX - 3, 16, 0x1a0d00, 0xffaa00);
    disp_str8("BRIGHTNESS", 34, 5, 0xffd080);

    /* Wartosc procentowa */
    snprintf(str, sizeof(str), "%d%%", (int)pct);
    disp_str20(str, 52, 30, 0xffd080);

    /* Pasek graficzny (10 segmentow = 10..100%) */
    const int bar_x = 10;
    const int bar_y = 62;
    const int bar_w = NX - 20;   /* 140px */
    const int bar_h = 14;
    int segments = (pct - BRIGHTNESS_MIN) / BRIGHTNESS_STEP + 1;
    if (segments < 0)  segments = 0;
    if (segments > 10) segments = 10;

    /* Tlo paska */
    ui_rounded_box(bar_x - 2, bar_y - 2, bar_x + bar_w + 1, bar_y + bar_h + 1,
                   0x1a0d00, 0x886600);
    /* Wypelnione segmenty */
    for (int i = 0; i < 10; i++) {
        int sx = bar_x + i * 14;
        uint32_t col = (i < segments) ? 0xffd080 : 0x332200;
        boxfill(sx, bar_y, sx + 12, bar_y + bar_h, col);
    }

    /* Dolna belka — podpowiedz */
    boxfill(0, NY - 28, NX - 1, NY - 15, 0x0a0808);
    draw_line(0, NY - 28, NX - 1, NY - 28, 0x664400);
    disp_str8("ENC = zmien jasnosc", 6, NY - 25, 0x886600);
    boxfill(0, NY - 14, NX - 1, NY - 1, 0x100800);
    draw_line(0, NY - 14, NX - 1, NY - 14, 0x664400);
    disp_str8("MEM=SAVE", 4,  NY - 11, 0x00aa00);
    disp_str8("SAVE=X",   88, NY - 11, 0x884400);
}

/* =========================================================================
 *  ui_draw_xtal_cal — ekran kalibracji kwarcu
 *
 *  Caly ekran (160x128):
 *    Naglowek:  XTAL CALIBRATION
 *    Wartosc:   +XXXX Hz  (duza czcionka)
 *    XTAL eff:  24.999.XXX Hz
 *    Dolna belka: MEM=SAVE  SAVE=CANCEL
 * ====================================================================== */
void ui_draw_xtal_cal(int32_t cal)
{
    char str[32];

    /* Tlo */
    boxfill(0, 0, NX - 1, NY - 1, 0x000a0a);

    /* Naglowek */
    ui_rounded_box(2, 2, NX - 3, 16, 0x001a1a, 0x00aaaa);
    disp_str8("XTAL CALIBRATION", 10, 5, 0x00ffff);

    /* Wartosc korekty — duza czcionka */
    ui_rounded_box(10, 28, NX - 11, 60, 0x001010, 0x00aaaa);
    snprintf(str, sizeof(str), "%+ld Hz", (long)cal);
    disp_str16(str, 20, 35, 0x00ffff);

    /* Efektywna czestotliwosc XTAL */
    uint32_t xtal_eff = (uint32_t)((int32_t)SI5351_XTAL_FREQ + cal);
    snprintf(str, sizeof(str), "XTAL: %lu Hz", (unsigned long)xtal_eff);
    disp_str8(str, 14, 68, 0x888888);

    /* Zakres */
    snprintf(str, sizeof(str), "[%d..+%d Hz]", XTAL_CAL_MIN, XTAL_CAL_MAX);
    disp_str8(str, 14, 82, 0x555555);

    /* Podpowiedz obslugi */
    draw_line(0, NY - 28, NX - 1, NY - 28, 0x005555);
    disp_str8("ENC = zmien wartosc", 6, NY - 24, 0x007777);
    boxfill(0, NY - 14, NX - 1, NY - 1, 0x001010);
    draw_line(0, NY - 14, NX - 1, NY - 14, 0x005555);
    disp_str8("MEM=SAVE", 4,  NY - 11, 0x00aa00);
    disp_str8("SAVE=X",   88, NY - 11, 0x884400);
}
