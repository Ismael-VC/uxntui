#include <stdio.h>
#include <stdlib.h>

#include "../uxn.h"
#include "console.h"

/*
Copyright (c) 2022-2024 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

int
console_input(Uint8 c, int type)
{
	uxn.dev[0x12] = c;
	uxn.dev[0x17] = type;
	return uxn_eval(PEEK2(&uxn.dev[0x10]));
}

void
console_listen(int i, int argc, char **argv)
{
	for(; i < argc; i++) {
		char *p = argv[i];
		while(*p) console_input(*p++, CONSOLE_ARG);
		console_input('\n', i == argc - 1 ? CONSOLE_END : CONSOLE_EOA);
	}
}

void
console_deo(Uint8 addr)
{
	FILE *fd;
	switch(addr) {
	case 0x18: fd = stdout, fputc(uxn.dev[0x18], fd), fflush(fd); break;
	case 0x19: fd = stderr, fputc(uxn.dev[0x19], fd), fflush(fd); break;
	}
}
