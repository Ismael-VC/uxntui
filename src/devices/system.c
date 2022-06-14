#include <stdio.h>

#include "../uxn.h"
#include "system.h"

/*
Copyright (c) 2022 Devine Lu Linvega, Andrew Alderwick

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

int
uxn_halt(Uxn *u, Uint8 instr, Uint8 err, Uint16 addr)
{
	Uint8 *d = &u->dev[0x00];
	if(instr & 0x40)
		u->rst->err = err;
	else
		u->wst->err = err;
	if(GETVEC(d))
		uxn_eval(u, GETVEC(d));
	else {
		system_inspect(u);
		fprintf(stderr, "%s %s, by %02x at 0x%04x.\n", (instr & 0x40) ? "Return-stack" : "Working-stack", errors[err - 1], instr, addr);
	}
	return 0;
}

/* IO */

void
system_deo(Uxn *u, Uint8 *d, Uint8 port)
{
	switch(port) {
	case 0x2: u->wst = (Stack *)(u->ram + (d[port] ? (d[port] * 0x100) : 0x10000)); break;
	case 0x3: u->rst = (Stack *)(u->ram + (d[port] ? (d[port] * 0x100) : 0x10100)); break;
	case 0xe:
		if(u->wst->ptr || u->rst->ptr) system_inspect(u);
		break;
	}
}
