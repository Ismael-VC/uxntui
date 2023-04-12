#include <stdlib.h>

#include "../uxn.h"
#include "screen.h"

/*
Copyright (c) 2021-2023 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

UxnScreen uxn_screen;

static Uint8 blending[4][16] = {
	{0, 0, 0, 0, 1, 0, 1, 1, 2, 2, 0, 2, 3, 3, 3, 0},
	{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
	{1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1},
	{2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2}};

static int
clamp(int val, int min, int max)
{
	return (val >= min) ? (val <= max) ? val : max : min;
}

static void
screen_write(UxnScreen *p, Layer *layer, Uint16 x, Uint16 y, Uint8 color)
{
	if(x < p->width && y < p->height) {
		Uint32 i = x + y * p->width;
		if(color != layer->pixels[i]) {
			layer->pixels[i] = color;
			layer->changed = 1;
		}
	}
}

static void
screen_fill(UxnScreen *p, Layer *layer, Uint16 x1, Uint16 y1, Uint16 x2, Uint16 y2, Uint8 color)
{
	int v, h;
	for(v = y1; v < y2; v++)
		for(h = x1; h < x2; h++)
			screen_write(p, layer, h, v, color);
}

static void
screen_wipe(UxnScreen *p, Layer *layer, Uint16 x, Uint16 y)
{
	screen_fill(p, layer, x, y, x + 8, y + 8, 0);
}

static void
screen_blit(UxnScreen *p, Layer *layer, Uint16 x, Uint16 y, Uint8 *sprite, Uint8 color, Uint8 flipx, Uint8 flipy, Uint8 twobpp)
{
	int v, h, opaque = (color % 5) || !color;
	for(v = 0; v < 8; v++) {
		Uint16 c = sprite[v] | (twobpp ? (sprite[v + 8] << 8) : 0);
		for(h = 7; h >= 0; --h, c >>= 1) {
			Uint8 ch = (c & 1) | ((c >> 7) & 2);
			if(opaque || ch)
				screen_write(p,
					layer,
					x + (flipx ? 7 - h : h),
					y + (flipy ? 7 - v : v),
					blending[ch][color]);
		}
	}
}

void
screen_palette(UxnScreen *p, Uint8 *addr)
{
	int i, shift;
	for(i = 0, shift = 4; i < 4; ++i, shift ^= 4) {
		Uint8
			r = (addr[0 + i / 2] >> shift) & 0x0f,
			g = (addr[2 + i / 2] >> shift) & 0x0f,
			b = (addr[4 + i / 2] >> shift) & 0x0f;
		p->palette[i] = 0x0f000000 | r << 16 | g << 8 | b;
		p->palette[i] |= p->palette[i] << 4;
	}
	p->fg.changed = p->bg.changed = 1;
}

void
screen_resize(UxnScreen *p, Uint16 width, Uint16 height)
{
	Uint8
		*bg = realloc(p->bg.pixels, width * height),
		*fg = realloc(p->fg.pixels, width * height);
	Uint32
		*pixels = realloc(p->pixels, width * height * sizeof(Uint32));
	if(bg) p->bg.pixels = bg;
	if(fg) p->fg.pixels = fg;
	if(pixels) p->pixels = pixels;
	if(bg && fg && pixels) {
		p->width = width;
		p->height = height;
		screen_fill(p, &p->bg, 0, 0, p->width, p->height, 0);
		screen_fill(p, &p->fg, 0, 0, p->width, p->height, 0);
	}
}

void
screen_redraw(UxnScreen *p)
{
	Uint32 i, size = p->width * p->height, palette[16];
	for(i = 0; i < 16; i++)
		palette[i] = p->palette[(i >> 2) ? (i >> 2) : (i & 3)];
	for(i = 0; i < size; i++)
		p->pixels[i] = palette[p->fg.pixels[i] << 2 | p->bg.pixels[i]];
	p->fg.changed = p->bg.changed = 0;
}

Uint8
screen_dei(Uxn *u, Uint8 addr)
{
	switch(addr) {
	case 0x22: return uxn_screen.width >> 8;
	case 0x23: return uxn_screen.width;
	case 0x24: return uxn_screen.height >> 8;
	case 0x25: return uxn_screen.height;
	default: return u->dev[addr];
	}
}

void
screen_deo(Uint8 *ram, Uint8 *d, Uint8 port)
{
	switch(port) {
	case 0x3:
		screen_resize(&uxn_screen, clamp(PEEK2(d + 2), 1, 1024), uxn_screen.height);
		break;
	case 0x5:
		screen_resize(&uxn_screen, uxn_screen.width, clamp(PEEK2(d + 4), 1, 1024));
		break;
	case 0xe: {
		Uint16 x = PEEK2(d + 0x8), y = PEEK2(d + 0xa);
		Layer *layer = (d[0xf] & 0x40) ? &uxn_screen.fg : &uxn_screen.bg;
		if(d[0xe] & 0x80)
			screen_fill(&uxn_screen, layer, (d[0xe] & 0x10) ? 0 : x, (d[0xe] & 0x20) ? 0 : y, (d[0xe] & 0x10) ? x : uxn_screen.width, (d[0xe] & 0x20) ? y : uxn_screen.height, d[0xe] & 0x3);
		else {
			screen_write(&uxn_screen, layer, x, y, d[0xe] & 0x3);
			if(d[0x6] & 0x1) POKE2(d + 0x8, x + 1); /* auto x+1 */
			if(d[0x6] & 0x2) POKE2(d + 0xa, y + 1); /* auto y+1 */
		}
		break;
	}
	case 0xf: {
		Uint16 x = PEEK2(d + 0x8), y = PEEK2(d + 0xa), dx, dy, addr = PEEK2(d + 0xc);
		Uint8 i, n, twobpp = !!(d[0xf] & 0x80);
		Layer *layer = (d[0xf] & 0x40) ? &uxn_screen.fg : &uxn_screen.bg;
		n = d[0x6] >> 4;
		dx = (d[0x6] & 0x01) << 3;
		dy = (d[0x6] & 0x02) << 2;
		if(addr > 0x10000 - ((n + 1) << (3 + twobpp)))
			return;
		for(i = 0; i <= n; i++) {
			if(!(d[0xf] & 0xf))
				screen_wipe(&uxn_screen, layer, x + dy * i, y + dx * i);
			else {
				screen_blit(&uxn_screen, layer, x + dy * i, y + dx * i, &ram[addr], d[0xf] & 0xf, d[0xf] & 0x10, d[0xf] & 0x20, twobpp);
				addr += (d[0x6] & 0x04) << (1 + twobpp);
			}
		}
		if(d[0x6] & 0x1) POKE2(d + 0x8, x + dx); /* auto x+8 */
		if(d[0x6] & 0x2) POKE2(d + 0xa, y + dy); /* auto y+8 */
		if(d[0x6] & 0x4) POKE2(d + 0xc, addr);   /* auto addr+length */
		break;
	}
	}
}
