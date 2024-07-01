#include <stdlib.h>
#include <stdio.h>

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

/* c = !ch ? (color % 5 ? color >> 2 : 0) : color % 4 + ch == 1 ? 0 : (ch - 2 + (color & 3)) % 3 + 1; */

static Uint8 blending[4][16] = {
	{0, 0, 0, 0, 1, 0, 1, 1, 2, 2, 0, 2, 3, 3, 3, 0},
	{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
	{1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1},
	{2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2}};

static void
screen_2bpp(Uint8 *layer, Uint8 *addr, Uint16 x1, Uint16 y1, Uint16 color, int fx, int fy)
{
	int row, w = uxn_screen.width, h = uxn_screen.height, opaque = (color % 5);
	Uint16 y, ymod = (fy < 0 ? 7 : 0), ymax = y1 + ymod + fy * 8;
	Uint16 x, xmod = (fx > 0 ? 7 : 0), xmax = x1 + xmod - fx * 8;
	for(y = y1 + ymod; y != ymax; y += fy, addr++) {
		int c = (addr[8] << 8) | addr[0];
		if(y < h)
			for(x = x1 + xmod, row = y * w; x != xmax; x -= fx, c >>= 1) {
				Uint8 ch = (c & 1) | ((c >> 7) & 2);
				if((opaque || ch) && x < w)
					layer[x + row] = blending[ch][color];
			}
	}
}

static void
screen_1bpp(Uint8 *layer, Uint8 *addr, Uint16 x1, Uint16 y1, Uint16 color, int fx, int fy)
{
	int row, w = uxn_screen.width, h = uxn_screen.height, opaque = (color % 5);
	Uint16 y, ymod = (fy < 0 ? 7 : 0), ymax = y1 + ymod + fy * 8;
	Uint16 x, xmod = (fx > 0 ? 7 : 0), xmax = x1 + xmod - fx * 8;
	for(y = y1 + ymod; y != ymax; y += fy) {
		int c = *addr++;
		if(y < h)
			for(x = x1 + xmod, row = y * w; x != xmax; x -= fx, c >>= 1) {
				Uint8 ch = c & 1;
				if((opaque || ch) && x < w)
					layer[x + row] = blending[ch][color];
			}
	}
}

int
screen_changed(void)
{
	if(uxn_screen.x1 < 0)
		uxn_screen.x1 = 0;
	else if(uxn_screen.x1 >= uxn_screen.width)
		uxn_screen.x1 = uxn_screen.width;
	if(uxn_screen.y1 < 0)
		uxn_screen.y1 = 0;
	else if(uxn_screen.y1 >= uxn_screen.height)
		uxn_screen.y1 = uxn_screen.height;
	if(uxn_screen.x2 < 0)
		uxn_screen.x2 = 0;
	else if(uxn_screen.x2 >= uxn_screen.width)
		uxn_screen.x2 = uxn_screen.width;
	if(uxn_screen.y2 < 0)
		uxn_screen.y2 = 0;
	else if(uxn_screen.y2 >= uxn_screen.height)
		uxn_screen.y2 = uxn_screen.height;
	return uxn_screen.x2 > uxn_screen.x1 && uxn_screen.y2 > uxn_screen.y1;
}

void
screen_change(int x1, int y1, int x2, int y2)
{
	if(x1 < uxn_screen.x1) uxn_screen.x1 = x1;
	if(y1 < uxn_screen.y1) uxn_screen.y1 = y1;
	if(x2 > uxn_screen.x2) uxn_screen.x2 = x2;
	if(y2 > uxn_screen.y2) uxn_screen.y2 = y2;
}

/* clang-format off */

static Uint8 icons[] = {
	0x00, 0x7c, 0x82, 0x82, 0x82, 0x82, 0x82, 0x7c, 0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 
	0x10, 0x00, 0x7c, 0x82, 0x02, 0x7c, 0x80, 0x80, 0xfe, 0x00, 0x7c, 0x82, 0x02, 0x1c, 0x02, 
	0x82, 0x7c, 0x00, 0x0c, 0x14, 0x24, 0x44, 0x84, 0xfe, 0x04, 0x00, 0xfe, 0x80, 0x80, 0x7c, 
	0x02, 0x82, 0x7c, 0x00, 0x7c, 0x82, 0x80, 0xfc, 0x82, 0x82, 0x7c, 0x00, 0x7c, 0x82, 0x02, 
	0x1e, 0x02, 0x02, 0x02, 0x00, 0x7c, 0x82, 0x82, 0x7c, 0x82, 0x82, 0x7c, 0x00, 0x7c, 0x82, 
	0x82, 0x7e, 0x02, 0x82, 0x7c, 0x00, 0x7c, 0x82, 0x02, 0x7e, 0x82, 0x82, 0x7e, 0x00, 0xfc, 
	0x82, 0x82, 0xfc, 0x82, 0x82, 0xfc, 0x00, 0x7c, 0x82, 0x80, 0x80, 0x80, 0x82, 0x7c, 0x00, 
	0xfc, 0x82, 0x82, 0x82, 0x82, 0x82, 0xfc, 0x00, 0x7c, 0x82, 0x80, 0xf0, 0x80, 0x82, 0x7c,
	0x00, 0x7c, 0x82, 0x80, 0xf0, 0x80, 0x80, 0x80 };
static Uint8 arrow[] = {
	0x00, 0x00, 0x00, 0xfe, 0x7c, 0x38, 0x10, 0x00 };

/* clang-format on */

static void
draw_byte(Uint8 b, Uint16 x, Uint16 y, Uint8 color)
{
	screen_1bpp(uxn_screen.fg, &icons[(b >> 4) << 3], x, y, color, 1, 1);
	screen_1bpp(uxn_screen.fg, &icons[(b & 0xf) << 3], x + 8, y, color, 1, 1);
	screen_change(x, y, x + 0x10, y + 0x8);
}

static void
screen_debugger(void)
{
	int i;
	for(i = 0; i < 0x08; i++) {
		Uint8 pos = uxn.wst.ptr - 4 + i;
		Uint8 color = i > 4 ? 0x01 : !pos ? 0xc :
			i == 4                        ? 0x8 :
											0x2;
		draw_byte(uxn.wst.dat[pos], i * 0x18 + 0x8, uxn_screen.height - 0x18, color);
	}
	for(i = 0; i < 0x08; i++) {
		Uint8 pos = uxn.rst.ptr - 4 + i;
		Uint8 color = i > 4 ? 0x01 : !pos ? 0xc :
			i == 4                        ? 0x8 :
											0x2;
		draw_byte(uxn.rst.dat[pos], i * 0x18 + 0x8, uxn_screen.height - 0x10, color);
	}
	screen_1bpp(uxn_screen.fg, &arrow[0], 0x68, uxn_screen.height - 0x20, 3, 1, 1);
	for(i = 0; i < 0x20; i++)
		draw_byte(uxn.ram[i], (i & 0x7) * 0x18 + 0x8, ((i >> 3) << 3) + 0x8, 1 + !!uxn.ram[i]);
}

void
screen_fill(Uint8 *layer, int color)
{
	int i, length = uxn_screen.width * uxn_screen.height;
	for(i = 0; i < length; i++)
		layer[i] = color;
}

void
screen_rect(Uint8 *layer, Uint16 x1, Uint16 y1, Uint16 x2, Uint16 y2, int color)
{
	int row, x, y, w = uxn_screen.width, h = uxn_screen.height;
	for(y = y1; y < y2 && y < h; y++)
		for(x = x1, row = y * w; x < x2 && x < w; x++)
			layer[x + row] = color;
}

void
screen_palette(void)
{
	int i, shift;
	for(i = 0, shift = 4; i < 4; ++i, shift ^= 4) {
		Uint8
			r = (uxn.dev[0x8 + i / 2] >> shift) & 0xf,
			g = (uxn.dev[0xa + i / 2] >> shift) & 0xf,
			b = (uxn.dev[0xc + i / 2] >> shift) & 0xf;
		uxn_screen.palette[i] = 0x0f000000 | r << 16 | g << 8 | b;
		uxn_screen.palette[i] |= uxn_screen.palette[i] << 4;
	}
	screen_change(0, 0, uxn_screen.width, uxn_screen.height);
}

void
screen_resize(Uint16 width, Uint16 height, int scale)
{
	Uint8 *bg, *fg;
	Uint32 *pixels = NULL;
	int dim_change = uxn_screen.width != width || uxn_screen.height != height;
	if(width < 0x8 || height < 0x8 || width >= 0x800 || height >= 0x800 || scale < 1 || scale >= 4)
		return;
	if(uxn_screen.width == width && uxn_screen.height == height && uxn_screen.scale == scale)
		return;
	if(dim_change) {
		bg = malloc(width * height), fg = malloc(width * height);
		if(bg && fg)
			pixels = realloc(uxn_screen.pixels, width * height * sizeof(Uint32) * scale * scale);
		if(!bg || !fg || !pixels) {
			free(bg), free(fg);
			return;
		}
		free(uxn_screen.bg), free(uxn_screen.fg);
	} else {
		bg = uxn_screen.bg, fg = uxn_screen.fg;
		pixels = realloc(uxn_screen.pixels, width * height * sizeof(Uint32) * scale * scale);
		if(!pixels)
			return;
	}
	uxn_screen.bg = bg, uxn_screen.fg = fg;
	uxn_screen.pixels = pixels;
	uxn_screen.width = width, uxn_screen.height = height, uxn_screen.scale = scale;
	if(dim_change)
		screen_fill(uxn_screen.bg, 0), screen_fill(uxn_screen.fg, 0);
	emu_resize(width, height);
	screen_change(0, 0, width, height);
}

void
screen_redraw(void)
{
	int i, x, y, k, l, s = uxn_screen.scale;
	Uint8 *fg = uxn_screen.fg, *bg = uxn_screen.bg;
	Uint16 w = uxn_screen.width, h = uxn_screen.height;
	Uint16 x1 = uxn_screen.x1, y1 = uxn_screen.y1;
	Uint16 x2 = uxn_screen.x2 > w ? w : uxn_screen.x2, y2 = uxn_screen.y2 > h ? h : uxn_screen.y2;
	Uint32 palette[16], *pixels = uxn_screen.pixels;
	uxn_screen.x1 = uxn_screen.y1 = 9000;
	uxn_screen.x2 = uxn_screen.y2 = 0;
	if(uxn.dev[0x0e])
		screen_debugger();
	for(i = 0; i < 16; i++)
		palette[i] = uxn_screen.palette[(i >> 2) ? (i >> 2) : (i & 3)];
	for(y = y1; y < y2; y++) {
		int ys = y * s;
		int o = y * w;
		for(x = x1, i = x1 + o; x < x2; x++, i++) {
			int c = palette[fg[i] << 2 | bg[i]];
			for(k = 0; k < s; k++) {
				int oo = ((ys + k) * w + x) * s;
				for(l = 0; l < s; l++)
					pixels[oo + l] = c;
			}
		}
	}
}

/* screen registers */

static int rX, rY, rA, rMX, rMY, rMA, rML, rDX, rDY;

Uint8
screen_dei(Uint8 addr)
{
	switch(addr) {
	case 0x22: return uxn_screen.width >> 8;
	case 0x23: return uxn_screen.width;
	case 0x24: return uxn_screen.height >> 8;
	case 0x25: return uxn_screen.height;
	case 0x28: return rX >> 8;
	case 0x29: return rX;
	case 0x2a: return rY >> 8;
	case 0x2b: return rY;
	case 0x2c: return rA >> 8;
	case 0x2d: return rA;
	default: return uxn.dev[addr];
	}
}

void
screen_deo(Uint8 addr)
{
	switch(addr) {
	case 0x23: screen_resize(PEEK2(&uxn.dev[0x22]), uxn_screen.height, uxn_screen.scale); return;
	case 0x25: screen_resize(uxn_screen.width, PEEK2(&uxn.dev[0x24]), uxn_screen.scale); return;
	case 0x26: rMX = uxn.dev[0x26] & 0x1, rMY = uxn.dev[0x26] & 0x2, rMA = uxn.dev[0x26] & 0x4, rML = uxn.dev[0x26] >> 4, rDX = rMX << 3, rDY = rMY << 2; return;
	case 0x28:
	case 0x29: rX = (uxn.dev[0x28] << 8) | uxn.dev[0x29], rX = twos(rX); return;
	case 0x2a:
	case 0x2b: rY = (uxn.dev[0x2a] << 8) | uxn.dev[0x2b], rY = twos(rY); return;
	case 0x2c:
	case 0x2d: rA = (uxn.dev[0x2c] << 8) | uxn.dev[0x2d]; return;
	case 0x2e: {
		Uint8 ctrl = uxn.dev[0x2e];
		Uint8 color = ctrl & 0x3;
		Uint8 *layer = ctrl & 0x40 ? uxn_screen.fg : uxn_screen.bg;
		/* fill mode */
		if(ctrl & 0x80) {
			Uint16 x1, y1, x2, y2;
			if(ctrl & 0x10)
				x1 = 0, x2 = rX;
			else
				x1 = rX, x2 = uxn_screen.width;
			if(ctrl & 0x20)
				y1 = 0, y2 = rY;
			else
				y1 = rY, y2 = uxn_screen.height;
			screen_rect(layer, x1, y1, x2, y2, color);
			screen_change(x1, y1, x2, y2);
		}
		/* pixel mode */
		else {
			Uint16 w = uxn_screen.width;
			if(rX >= 0 && rY >= 0 && rX < w && rY < uxn_screen.height)
				layer[rX + rY * w] = color;
			screen_change(rX, rY, rX + 1, rY + 1);
			if(rMX) rX++;
			if(rMY) rY++;
		}
		return;
	}
	case 0x2f: {
		Uint8 i;
		Uint8 ctrl = uxn.dev[0x2f];
		Uint8 twobpp = !!(ctrl & 0x80);
		Uint8 color = ctrl & 0xf;
		Uint8 *layer = ctrl & 0x40 ? uxn_screen.fg : uxn_screen.bg;
		int fx = ctrl & 0x10 ? -1 : 1, fy = ctrl & 0x20 ? -1 : 1;
		int x1, x2, y1, y2, x = rX, y = rY;
		int dxy = rDX * fy, dyx = rDY * fx, addr_incr = rMA << (1 + twobpp);
		if(twobpp)
			for(i = 0; i <= rML; i++, x += dyx, y += dxy, rA += addr_incr)
				screen_2bpp(layer, &uxn.ram[rA], x, y, color, fx, fy);
		else
			for(i = 0; i <= rML; i++, x += dyx, y += dxy, rA += addr_incr)
				screen_1bpp(layer, &uxn.ram[rA], x, y, color, fx, fy);
		if(fx < 0)
			x1 = x, x2 = rX;
		else
			x1 = rX, x2 = x;
		if(fy < 0)
			y1 = y, y2 = rY;
		else
			y1 = rY, y2 = y;
		screen_change(x1, y1, x2 + 8, y2 + 8);
		if(rMX) rX += rDX * fx;
		if(rMY) rY += rDY * fy;
		return;
	}
	}
}
