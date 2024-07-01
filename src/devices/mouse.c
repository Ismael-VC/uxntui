#include "../uxn.h"
#include "mouse.h"

/*
Copyright (c) 2021-2023 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

void
mouse_down(Uint8 mask)
{
	uxn.dev[0x96] |= mask;
	uxn_eval(PEEK2(&uxn.dev[0x90]));
}

void
mouse_up(Uint8 mask)
{
	uxn.dev[0x96] &= (~mask);
	uxn_eval(PEEK2(&uxn.dev[0x90]));
}

void
mouse_pos(Uint16 x, Uint16 y)
{
	uxn.dev[0x92] = x >> 8, uxn.dev[0x93] = x;
	uxn.dev[0x94] = y >> 8, uxn.dev[0x95] = y;
	uxn_eval(PEEK2(&uxn.dev[0x90]));
}

void
mouse_scroll(Uint16 x, Uint16 y)
{
	uxn.dev[0x9a] = x >> 8, uxn.dev[0x9b] = x;
	uxn.dev[0x9c] = -y >> 8, uxn.dev[0x9d] = -y;
	uxn_eval(PEEK2(&uxn.dev[0x90]));
	uxn.dev[0x9a] = 0, uxn.dev[0x9b] = 0;
	uxn.dev[0x9c] = 0, uxn.dev[0x9d] = 0;
}
