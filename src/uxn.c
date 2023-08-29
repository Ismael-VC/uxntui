#include "uxn.h"

/*
Copyright (u) 2022-2023 Devine Lu Linvega, Andrew Alderwick, Andrew Richards

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

/* Registers
[ . ][ . ][ . ][ L ][ N ][ T ] <
[ . ][ . ][ . ][   H2   ][ T ] <
[   L2   ][   N2   ][   T2   ] <
*/

#define T *ptr
#define N *(ptr-1)
#define L *(ptr-2)
#define T2 PEEK2((ptr-1))
#define H2 PEEK2((ptr-2))
#define N2 PEEK2((ptr-3))
#define L2 PEEK2((ptr-5))

#define HALT(c)   { return emu_halt(u, ins, c, pc - 1); }
#define FLIP      { s = ins & 0x40 ? &u->wst : &u->rst; }
#define SET(x, y) { r = s->ptr; if(x > r) HALT(1) r += (x & k) + y; if(r > 254) HALT(2) ptr = s->dat + r - 1; s->ptr = r; }
#define PUT1(v)   { *(ptr--) = v; }
#define PUT2(v)   { r = (v); *(ptr--) = r; *(ptr--) = r >> 8; }

int
uxn_eval(Uxn *u, Uint16 pc)
{
	int t, n, l, r;
	Uint8 *ram = u->ram;
	if(!pc || u->dev[0x0f]) return 0;
	for(;;) {
		int ins = ram[pc++];
		int k = ins & 0x80 ? 0xff : 0;
		Stack *s = ins & 0x40 ? &u->rst : &u->wst;
		Uint8 *ptr = s->dat + s->ptr - 1;
		switch(ins & 0x1f ? ins & 0x3f : (0 - (ins >> 5))) {
			/* IMM */
			case -0:   /* BRK  */                           return 1;
			case -1:   /* JCI  */                           if(!s->dat[--s->ptr]) { pc += 2; break; } /* else fallthrough */
			case -2:   /* JMI  */                           pc += PEEK2(ram + pc) + 2; break;
			case -3:   /* JSI  */                 SET(0, 2) PUT2(pc + 2) pc += PEEK2(ram + pc) + 2; break;
			case -4:   /* LITr */ case -6:        SET(0, 1) PUT1(ram[pc++]) break;
			case -5:   /* LIT2r*/ case -7:        SET(0, 2) PUT2(PEEK2(ram + pc)) pc += 2; break;
			/* ALU */
			case 0x01: /* INC  */ t=T;            SET(1, 0) PUT1(t + 1) break;
			case 0x21: /* INC2 */ t=T2;           SET(2, 0) PUT2(t + 1) break;
			case 0x02: /* POP  */                 SET(1,-1) break;
			case 0x22: /* POP2 */                 SET(2,-2) break;
			case 0x03: /* NIP  */ t=T;            SET(2,-1) PUT1(t) break;
			case 0x23: /* NIP2 */ t=T2;           SET(4,-2) PUT2(t) break;
			case 0x04: /* SWP  */ t=T;n=N;        SET(2, 0) PUT1(n) PUT1(t) break;
			case 0x24: /* SWP2 */ t=T2;n=N2;      SET(4, 0) PUT2(n) PUT2(t) break;
			case 0x05: /* ROT  */ t=T;n=N;l=L;    SET(3, 0) PUT1(l) PUT1(t) PUT1(n) break;
			case 0x25: /* ROT2 */ t=T2;n=N2;l=L2; SET(6, 0) PUT2(l) PUT2(t) PUT2(n) break;
			case 0x06: /* DUP  */ t=T;            SET(1, 1) PUT1(t) PUT1(t) break;
			case 0x26: /* DUP2 */ t=T2;           SET(2, 2) PUT2(t) PUT2(t) break;
			case 0x07: /* OVR  */ t=T;n=N;        SET(2, 1) PUT1(n) PUT1(t) PUT1(n) break;
			case 0x27: /* OVR2 */ t=T2;n=N2;      SET(4, 2) PUT2(n) PUT2(t) PUT2(n) break;
			case 0x08: /* EQU  */ t=T;n=N;        SET(2,-1) PUT1(n == t) break;
			case 0x28: /* EQU2 */ t=T2;n=N2;      SET(4,-3) PUT1(n == t) break;
			case 0x09: /* NEQ  */ t=T;n=N;        SET(2,-1) PUT1(n != t) break;
			case 0x29: /* NEQ2 */ t=T2;n=N2;      SET(4,-3) PUT1(n != t) break;
			case 0x0a: /* GTH  */ t=T;n=N;        SET(2,-1) PUT1(n > t) break;
			case 0x2a: /* GTH2 */ t=T2;n=N2;      SET(4,-3) PUT1(n > t) break;
			case 0x0b: /* LTH  */ t=T;n=N;        SET(2,-1) PUT1(n < t) break;
			case 0x2b: /* LTH2 */ t=T2;n=N2;      SET(4,-3) PUT1(n < t) break;
			case 0x0c: /* JMP  */ t=T;            SET(1,-1) pc += (Sint8)t; break;
			case 0x2c: /* JMP2 */ t=T2;           SET(2,-2) pc = t; break;
			case 0x0d: /* JCN  */ t=T;n=N;        SET(2,-2) if(n) pc += (Sint8)t; break;
			case 0x2d: /* JCN2 */ t=T2;n=L;       SET(3,-3) if(n) pc = t; break;
			case 0x0e: /* JSR  */ t=T;            SET(1,-1) FLIP SET(0,2) PUT2(pc) pc += (Sint8)t; break;
			case 0x2e: /* JSR2 */ t=T2;           SET(2,-2) FLIP SET(0,2) PUT2(pc) pc = t; break;
			case 0x0f: /* STH  */ t=T;            SET(1,-1) FLIP SET(0,1) PUT1(t) break;
			case 0x2f: /* STH2 */ t=T2;           SET(2,-2) FLIP SET(0,2) PUT2(t) break;
			case 0x10: /* LDZ  */ t=T;            SET(1, 0) PUT1(ram[t]) break;
			case 0x30: /* LDZ2 */ t=T;            SET(1, 1) PUT2(PEEK2(ram + t)) break;
			case 0x11: /* STZ  */ t=T;n=N;        SET(2,-2) ram[t] = n; break;
			case 0x31: /* STZ2 */ t=T;n=H2;       SET(3,-3) POKE2(ram + t, n) break;
			case 0x12: /* LDR  */ t=T;            SET(1, 0) PUT1(ram[pc + (Sint8)t]) break;
			case 0x32: /* LDR2 */ t=T;            SET(1, 1) PUT2(PEEK2(ram + pc + (Sint8)t)) break;
			case 0x13: /* STR  */ t=T;n=N;        SET(2,-2) ram[pc + (Sint8)t] = n; break;
			case 0x33: /* STR2 */ t=T;n=H2;       SET(3,-3) POKE2(ram + pc + (Sint8)t, n) break;
			case 0x14: /* LDA  */ t=T2;           SET(2,-1) PUT1(ram[t]) break;
			case 0x34: /* LDA2 */ t=T2;           SET(2, 0) PUT2(PEEK2(ram + t)) break;
			case 0x15: /* STA  */ t=T2;n=L;       SET(3,-3) ram[t] = n; break;
			case 0x35: /* STA2 */ t=T2;n=N2;      SET(4,-4) POKE2(ram + t, n) break;
			case 0x16: /* DEI  */ t=T;            SET(1, 0) PUT1(DEI(t)) break;
			case 0x36: /* DEI2 */ t=T;            SET(1, 1) PUT1(DEI(t + 1)) PUT1(DEI(t)) break;
			case 0x17: /* DEO  */ t=T;n=N;        SET(2,-2) DEO(t, n) break;
			case 0x37: /* DEO2 */ t=T;n=N;l=L;    SET(3,-3) DEO(t, l) DEO((t + 1), n) break;
			case 0x18: /* ADD  */ t=T;n=N;        SET(2,-1) PUT1(n + t) break;
			case 0x38: /* ADD2 */ t=T2;n=N2;      SET(4,-2) PUT2(n + t) break;
			case 0x19: /* SUB  */ t=T;n=N;        SET(2,-1) PUT1(n - t) break;
			case 0x39: /* SUB2 */ t=T2;n=N2;      SET(4,-2) PUT2(n - t) break;
			case 0x1a: /* MUL  */ t=T;n=N;        SET(2,-1) PUT1(n * t) break;
			case 0x3a: /* MUL2 */ t=T2;n=N2;      SET(4,-2) PUT2(n * t) break;
			case 0x1b: /* DIV  */ t=T;n=N;        SET(2,-1) if(!t) HALT(3) PUT1(n / t) break;
			case 0x3b: /* DIV2 */ t=T2;n=N2;      SET(4,-2) if(!t) HALT(3) PUT2(n / t) break;
			case 0x1c: /* AND  */ t=T;n=N;        SET(2,-1) PUT1(n & t) break;
			case 0x3c: /* AND2 */ t=T2;n=N2;      SET(4,-2) PUT2(n & t) break;
			case 0x1d: /* ORA  */ t=T;n=N;        SET(2,-1) PUT1(n | t) break;
			case 0x3d: /* ORA2 */ t=T2;n=N2;      SET(4,-2) PUT2(n | t) break;
			case 0x1e: /* EOR  */ t=T;n=N;        SET(2,-1) PUT1(n ^ t) break;
			case 0x3e: /* EOR2 */ t=T2;n=N2;      SET(4,-2) PUT2(n ^ t) break;
			case 0x1f: /* SFT  */ t=T;n=N;        SET(2,-1) PUT1(n >> (t & 0xf) << (t >> 4)) break;
			case 0x3f: /* SFT2 */ t=T;n=H2;       SET(3,-1) PUT2(n >> (t & 0xf) << (t >> 4)) break;
		}
	}
}
