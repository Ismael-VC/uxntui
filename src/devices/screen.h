/*
Copyright (c) 2021 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

typedef struct UxnScreen {
	int width, height, x1, y1, x2, y2, scale;
	Uint32 palette[4], *pixels;
	Uint8 *fg, *bg;
} UxnScreen;

extern UxnScreen uxn_screen;
extern int emu_resize(int width, int height);
int screen_changed(void);
void screen_change(int x1, int y1, int x2, int y2);
void screen_fill(Uint8 *layer, int color);
void screen_rect(Uint8 *layer, Uint16 x1, Uint16 y1, Uint16 x2, Uint16 y2, int color);
void screen_palette(void);
void screen_resize(Uint16 width, Uint16 height, int scale);
void screen_redraw();

Uint8 screen_dei(Uint8 addr);
void screen_deo(Uint8 addr);

#define twos(v) (v & 0x8000 ? (int)v - 0x10000 : (int)v)
