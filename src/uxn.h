/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

/* clang-format off */

#define POKE2(d, v) { *(d) = (v) >> 8; (d)[1] = (v); }
#define PEEK2(d) (*(d) << 8 | (d)[1])
#define DEI(p) (u->dei_masks[p] ? emu_dei(u, (p)) : u->dev[(p)])
#define DEO(p, v) { u->dev[p] = v; if(u->deo_masks[p]) emu_deo(u, p); }

/* clang-format on */

#define PAGE_PROGRAM 0x0100

typedef unsigned char Uint8;
typedef signed char Sint8;
typedef unsigned short Uint16;
typedef signed short Sint16;
typedef unsigned int Uint32;

typedef struct {
	Uint8 dat[0x100], ptr;
} Stack;

typedef struct Uxn {
	Uint8 *ram, dev[0x100], dei_masks[0x100], deo_masks[0x100];
	Stack wst, rst;
} Uxn;

/* required functions */

extern Uint8 emu_dei(Uxn *u, Uint8 addr);
extern void emu_deo(Uxn *u, Uint8 addr);
extern int emu_halt(Uxn *u, Uint8 instr, Uint8 err, Uint16 addr);
extern Uint16 dev_vers[0x10], dei_mask[0x10], deo_mask[0x10];

/* built-ins */

int uxn_eval(Uxn *u, Uint16 pc);
