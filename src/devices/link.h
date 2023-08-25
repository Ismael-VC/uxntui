/*
Copyright (c) 2022 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define LINK_VERSION 1
#define LINK_DEIMASK 0x0000
#define LINK_DEOMASK 0xaaaa

Uint8 link_dei(Uxn *u, Uint8 addr);
void link_deo(Uint8 *ram, Uint8 *d, Uint8 port);
