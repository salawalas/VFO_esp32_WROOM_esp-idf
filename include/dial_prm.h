/*===========================================================================
 *  dial_prm.h — Parametry tarczy analogowej (extern — def. w dial.c)
 *===========================================================================*/
#pragma once

/* Tryb wyswietlania: 0=normal, 1=obrot 90 stopni */
extern char f_dispmode;
/* Polozenie glownej tarczy: 0=wewnatrz, 1=na zewnatrz */
extern char f_main_outside;
/* Czcionka: 0, 1 lub 2 */
extern char f_FONT;
/* Kierunek obracania: 0=CW, 1=CCW */
extern char f_rev;

/* Widocznosc kresek glownych */
extern char f_maintick1;
extern char f_maintick5;
extern char f_maintick10;
extern char f_mainnum;

/* Widocznosc kresek pomocniczych */
extern char f_subtick1;
extern char f_subtick5;
extern char f_subtick10;
extern char f_subnum;

/* Czestotliwosc na podzialke glownej [Hz] */
extern int freq_tick_main;

/* Pozycja pionowa tarczy [px] */
extern int D_height;

/* Promien tarczy (jesli 45000 = skala liniowa) */
extern int D_R;

/* Odstep miedzy tarcza glowna a pomocnicza */
extern int Dial_space;

/* Interwal kresek */
extern int tick_pitch_main;
extern int tick_pitch;

/* Grubosc kresek */
extern int tick_width;

/* Dlugosc kresek glownych */
extern int tick_main1;
extern int tick_main5;
extern int tick_main10;

/* Dlugosc kresek pomocniczych */
extern int tick1;
extern int tick5;
extern int tick10;

/* Odstep liczba-kreska */
extern int TNCL_main;
extern int TNCL;

/* Wskaznik */
extern int DP_len;
extern int DP_width;
extern int DP_pos;

/* Kolory 0xRRGGBB */
extern unsigned long cl_BG;
extern unsigned long cl_POINTER;
extern unsigned long cl_TICK_main;
extern unsigned long cl_NUM_main;
extern unsigned long cl_TICK;
extern unsigned long cl_NUM;
extern unsigned long cl_DIAL_BG;
