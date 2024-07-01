#include "../uxn.h"
#include "controller.h"

/*
Copyright (c) 2021-2023 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

void
controller_down(Uint8 mask)
{
	if(mask) {
		uxn.dev[0x82] |= mask;
		uxn_eval(PEEK2(&uxn.dev[0x80]));
	}
}

void
controller_up(Uint8 mask)
{
	if(mask) {
		uxn.dev[0x82] &= (~mask);
		uxn_eval(PEEK2(&uxn.dev[0x80]));
	}
}

void
controller_key(Uint8 key)
{
	if(key) {
		uxn.dev[0x83] = key;
		uxn_eval(PEEK2(&uxn.dev[0x80]));
		uxn.dev[0x83] = 0;
	}
}
