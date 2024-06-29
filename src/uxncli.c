#include <stdio.h>
#include <stdlib.h>

#include "uxn.h"
#include "devices/system.h"
#include "devices/console.h"
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

Uxn uxn;

Uint8
emu_dei(Uint8 addr)
{
	switch(addr & 0xf0) {
	case 0x00: return system_dei(&uxn, addr);
	case 0x10: return console_dei(&uxn, addr);
	case 0xc0: return datetime_dei(&uxn, addr);
	}
	return uxn.dev[addr];
}

void
emu_deo(Uint8 addr, Uint8 value)
{
	Uint8 p = addr & 0x0f, d = addr & 0xf0;
	uxn.dev[addr] = value;
	switch(d) {
	case 0x00: system_deo(&uxn, &uxn.dev[d], p); break;
	case 0x10: console_deo(&uxn, &uxn.dev[d], p); break;
	case 0xa0: file_deo(0, uxn.ram, &uxn.dev[d], p); break;
	case 0xb0: file_deo(1, uxn.ram, &uxn.dev[d], p); break;
	}
}

static void
emu_run(Uxn *u)
{
	while(!u->dev[0x0f]) {
		int c = fgetc(stdin);
		if(c == EOF) {
			console_input(u, 0x00, CONSOLE_END);
			break;
		}
		console_input(u, (Uint8)c, CONSOLE_STD);
	}
}

static int
emu_end(Uxn *u)
{
	free(u->ram);
	return u->dev[0x0f] & 0x7f;
}

int
main(int argc, char **argv)
{
	int i = 1;
	if(i == argc)
		return system_error("usage", "uxncli [-v] file.rom [args..]");
	/* Read flags */
	if(argv[i][0] == '-' && argv[i][1] == 'v')
		return system_version("Uxncli - Console Varvara Emulator", "29 Jun 2024");
	if(!system_boot(&uxn, (Uint8 *)calloc(0x10000 * RAM_PAGES, sizeof(Uint8)), argv[i++]))
		return system_error("Init", "Failed to initialize uxn.");
	/* Game Loop */
	uxn.dev[0x17] = argc - i;
	if(uxn_eval(&uxn, PAGE_PROGRAM) && PEEK2(uxn.dev + 0x10)) {
		console_listen(&uxn, i, argc, argv);
		emu_run(&uxn);
	}
	return emu_end(&uxn);
}
