#include <stdio.h>
#include <stdlib.h>

#include "../uxn.h"
#include "system.h"

/*
Copyright (c) 2022-2023 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

static const char *errors[] = {
	"underflow",
	"overflow",
	"division by zero"};

static void
system_print(Stack *s, char *name)
{
	Uint8 i;
	fprintf(stderr, "<%s>", name);
	for(i = 0; i < s->ptr; i++)
		fprintf(stderr, " %02x", s->dat[i]);
	if(!i)
		fprintf(stderr, " empty");
	fprintf(stderr, "\n");
}

void
system_inspect(Uxn *u)
{
	system_print(u->wst, "wst");
	system_print(u->rst, "rst");
}

/* RAM */

void
system_cmd(Uint8 *ram, Uint16 addr)
{
	Uint16 a = addr, i = 0;
	Uint8 o = ram[a++];
	if(o == 1) {
		Uint16 length = (ram[a++] << 8) + ram[a++];
		Uint16 src_page = ((ram[a++] << 8) + ram[a++]) % 16, src_addr = (ram[a++] << 8) + ram[a++];
		Uint16 dst_page = ((ram[a++] << 8) + ram[a++]) % 16, dst_addr = (ram[a++] << 8) + ram[a];
		for(i = 0; i < length; i++)
			ram[dst_page * 0x10000 + dst_addr + i] = ram[src_page * 0x10000 + src_addr + i];
	}
}

int
system_load(Uxn *u, char *filename)
{
	int l, i = 0;
	FILE *f = fopen(filename, "rb");
	if(!f)
		return 0;
	l = fread(&u->ram[PAGE_PROGRAM], 0x10000 - PAGE_PROGRAM, 1, f);
	while(l && ++i < RAM_PAGES)
		l = fread(u->ram + 0x10000 * i, 0x10000, 1, f);
	fclose(f);
	return 1;
}

/* IO */

void
system_deo(Uxn *u, Uint8 *d, Uint8 port)
{
	Uint16 a;
	switch(port) {
	case 0x3:
		PEKDEV(a, 0x2);
		system_cmd(u->ram, a);
		break;
	case 0xe:
		if(u->wst->ptr || u->rst->ptr) system_inspect(u);
		break;
	}
}

/* Error */

int
uxn_halt(Uxn *u, Uint8 instr, Uint8 err, Uint16 addr)
{
	Uint8 *d = &u->dev[0x00];
	Uint16 handler = GETVEC(d);
	if(handler) {
		u->wst->ptr = 4;
		u->wst->dat[0] = addr >> 0x8;
		u->wst->dat[1] = addr & 0xff;
		u->wst->dat[2] = instr;
		u->wst->dat[3] = err;
		return uxn_eval(u, handler);
	} else {
		system_inspect(u);
		fprintf(stderr, "%s %s, by %02x at 0x%04x.\n", (instr & 0x40) ? "Return-stack" : "Working-stack", errors[err - 1], instr, addr);
	}
	return 0;
}
