#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "../uxn.h"

/*
Copyright (c) 2023 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

pthread_t threads[8];
static int tids[8];
static Uint16 link_vectors[8];
static Uint8 *link_ram;

static void *
link_eval(void *x)
{
	int tid = *((int *)x);
	Uxn u;
	Uint16 vector = link_vectors[tid];
	u.ram = link_ram;
	u.wst.ptr = u.rst.ptr = 0;
	uxn_eval(&u, link_vectors[tid]);
	threads[tid] = 0;
	link_vectors[tid] = 0;
	return NULL;
}

static void
link_init(Uint8 *ram, int id, Uint16 vector)
{
	tids[id] = id;
	link_vectors[id] = vector;
	link_ram = ram;
	pthread_create(&threads[id], NULL, link_eval, (void *)&tids[id]);
}

static void
link_wait(int id)
{
	pthread_join(threads[id], NULL);
	threads[id] = 0;
}

Uint8
link_dei(Uxn *u, Uint8 addr)
{
	/* TODO: return non-zero if active */
	return 0;
}

void
link_deo(Uint8 *ram, Uint8 *d, Uint8 port)
{
	Uint8 id = port >> 0x1;
	Uint16 vector = PEEK2(d + port - 1);
	if(threads[id])
		link_wait(id);
	if(vector)
		link_init(ram, id, vector);
}
