#include <stdio.h>
#include <stdlib.h>

#include "uxn.h"
#include "devices/system.h"
#include "devices/file.h"
#include "devices/datetime.h"

/*
Copyright (c) 2021-2023 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define SUPPORT 0x1c03 /* devices mask */

static int
emu_error(char *msg, const char *err)
{
	fprintf(stderr, "Error %s: %s\n", msg, err);
	return 0;
}

static int
console_input(Uxn *u, char c)
{
	Uint8 *d = &u->dev[0x10];
	d[0x02] = c;
	return uxn_eval(u, GETVEC(d));
}

static void
console_deo(Uint8 *d, Uint8 port)
{
	FILE *fd = port == 0x8 ? stdout : port == 0x9 ? stderr
												  : 0;
	if(fd) {
		fputc(d[port], fd);
		fflush(fd);
	}
}

static Uint8
emu_dei(Uxn *u, Uint8 addr)
{
	Uint8 p = addr & 0x0f, d = addr & 0xf0;
	switch(d) {
	case 0xa0: return file_dei(0, &u->dev[d], p);
	case 0xb0: return file_dei(1, &u->dev[d], p);
	case 0xc0: return datetime_dei(&u->dev[d], p);
	}
	return u->dev[addr];
}

static void
emu_deo(Uxn *u, Uint8 addr, Uint8 v)
{
	Uint8 p = addr & 0x0f, d = addr & 0xf0;
	Uint16 mask = 0x1 << (d >> 4);
	u->dev[addr] = v;
	switch(d) {
	case 0x00: system_deo(u, &u->dev[d], p); break;
	case 0x10: console_deo(&u->dev[d], p); break;
	case 0xa0: file_deo(0, u->ram, &u->dev[d], p); break;
	case 0xb0: file_deo(1, u->ram, &u->dev[d], p); break;
	}
	if(p == 0x01 && !(SUPPORT & mask))
		fprintf(stderr, "Warning: Incompatible emulation, device: %02x.\n", d);
}

int
main(int argc, char **argv)
{
	Uxn u;
	int i;
	Mmu mmu;
	if(argc < 2)
		return emu_error("Usage", "uxncli game.rom args");
	if(!uxn_boot(&u, mmu_init(&mmu, 16), emu_dei, emu_deo))
		return emu_error("Boot", "Failed");
	if(!load_rom(&u, argv[1]))
		return emu_error("Load", "Failed");
	if(!uxn_eval(&u, PAGE_PROGRAM))
		return emu_error("Init", "Failed");
	for(i = 2; i < argc; i++) {
		char *p = argv[i];
		while(*p) console_input(&u, *p++);
		console_input(&u, '\n');
	}
	while(!u.dev[0x0f]) {
		int c = fgetc(stdin);
		if(c != EOF)
			console_input(&u, (Uint8)c);
	}
	return 0;
}
