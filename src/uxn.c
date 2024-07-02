#include "uxn.h"

/*
Copyright (u) 2022-2024 Devine Lu Linvega, Andrew Alderwick, Andrew Richards

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

/* Registers
[ Z ][ Y ][ X ][ L ][ N ][ T ] <
[ . ][ . ][ . ][   H2   ][ . ] <
[   L2   ][   N2   ][   T2   ] <
*/

#define FLP { s = ins & 0x40 ? &uxn.wst : &uxn.rst; }
#define JMP(x) { if(m2) pc = (x); else pc += (Sint8)(x); }
#define PO1(o) { o = s->dat[--*sp]; }
#define PO2(o) { o = s->dat[--*sp] | (s->dat[--*sp] << 0x8); }
#define POx(o) { if(m2) { PO2(o) } else PO1(o) }
#define PU1(y) { s->dat[s->ptr++] = (y); }
#define PU2(y) { tt = (y); s->dat[s->ptr++] = tt >> 0x8; s->dat[s->ptr++] = tt; }
#define PUx(y) { if(m2) { PU2(y) } else PU1(y) }
#define PEK(o, x, r) { if(m2) { r = (x); o = uxn.ram[r++] << 8 | uxn.ram[r]; } else o = uxn.ram[(x)]; }
#define POK(x, y, r) { if(m2) { r = (x); uxn.ram[r++] = y >> 8; uxn.ram[r] = y; } else uxn.ram[(x)] = (y); }
#define DEI(o, p) { if(m2) { o = (emu_dei(p) << 8) | emu_dei(p + 1); } else o = emu_dei(p); }
#define DEO(p, y) { if(m2) { emu_deo(p, y >> 8); emu_deo(p + 1, y); } else emu_deo(p, y); }

#define OPC(x, body) { case x: body break; }

int
uxn_eval(Uint16 pc)
{
	if(!pc || uxn.dev[0x0f]) return 0;
	for(;;) {
		Uint16 tt, a, b, c;
		Uint8 t, kp, *sp, ins = uxn.ram[pc++];
		/* 2 */ Uint8 m2 = ins & 0x20;
		/* r */ Stack *s = ins & 0x40 ? &uxn.rst : &uxn.wst;
		/* k */ if(ins & 0x80) kp = s->ptr, sp = &kp; else sp = &s->ptr;
		switch(ins & 0x1f) {
		case 0x00:
		switch(ins) {
			/* BRK */ case 0x00: return 1;
			/* JCI */ case 0x20: PO1(b) if(!b) { pc += 2; break; } a = uxn.ram[pc++] << 8 | uxn.ram[pc++]; pc += a; break;
			/* JMI */ case 0x40: a = uxn.ram[pc++] << 8 | uxn.ram[pc++]; pc += a; break;
			/* JSI */ case 0x60: PU2(pc + 2) a = uxn.ram[pc++] << 8 | uxn.ram[pc++]; pc += a; break;
			/* LIT2 */ case 0xa0: case 0xe0: PU1(uxn.ram[pc++]) PU1(uxn.ram[pc++]) break;
			/* LIT  */ case 0x80: case 0xc0: PU1(uxn.ram[pc++]) break;
		} break;
		/* INC */ OPC(0x01, POx(a) PUx(a + 1))
		/* POP */ OPC(0x02, POx(a))
		/* NIP */ OPC(0x03, POx(a) POx(b) PUx(a))
		/* SWP */ OPC(0x04, POx(a) POx(b) PUx(a) PUx(b))
		/* ROT */ OPC(0x05, POx(a) POx(b) POx(c) PUx(b) PUx(a) PUx(c))
		/* DUP */ OPC(0x06, POx(a) PUx(a) PUx(a))
		/* OVR */ OPC(0x07, POx(a) POx(b) PUx(b) PUx(a) PUx(b))
		/* EQU */ OPC(0x08, POx(a) POx(b) PU1(b == a))
		/* NEQ */ OPC(0x09, POx(a) POx(b) PU1(b != a))
		/* GTH */ OPC(0x0a, POx(a) POx(b) PU1(b > a))
		/* LTH */ OPC(0x0b, POx(a) POx(b) PU1(b < a))
		/* JMP */ OPC(0x0c, POx(a) JMP(a))
		/* JCN */ OPC(0x0d, POx(a) PO1(b) if(b) JMP(a))
		/* JSR */ OPC(0x0e, POx(a) FLP PU2(pc) JMP(a))
		/* STH */ OPC(0x0f, POx(a) FLP PUx(a))
		/* LDZ */ OPC(0x10, PO1(a) PEK(b, a, t) PUx(b))
		/* STZ */ OPC(0x11, PO1(a) POx(b) POK(a, b, t))
		/* LDR */ OPC(0x12, PO1(a) PEK(b, pc + (Sint8)a, tt) PUx(b))
		/* STR */ OPC(0x13, PO1(a) POx(b) POK(pc + (Sint8)a, b, tt))
		/* LDA */ OPC(0x14, PO2(a) PEK(b, a, tt) PUx(b))
		/* STA */ OPC(0x15, PO2(a) POx(b) POK(a, b, tt))
		/* DEI */ OPC(0x16, PO1(a) DEI(b, a) PUx(b))
		/* DEO */ OPC(0x17, PO1(a) POx(b) DEO(a, b))
		/* ADD */ OPC(0x18, POx(a) POx(b) PUx(b + a))
		/* SUB */ OPC(0x19, POx(a) POx(b) PUx(b - a))
		/* MUL */ OPC(0x1a, POx(a) POx(b) PUx(b * a))
		/* DIV */ OPC(0x1b, POx(a) POx(b) PUx(a ? b / a : 0))
		/* AND */ OPC(0x1c, POx(a) POx(b) PUx(b & a))
		/* ORA */ OPC(0x1d, POx(a) POx(b) PUx(b | a))
		/* EOR */ OPC(0x1e, POx(a) POx(b) PUx(b ^ a))
		/* SFT */ OPC(0x1f, PO1(a) POx(b) PUx(b >> (a & 0xf) << (a >> 4)))
		}
	}
}
