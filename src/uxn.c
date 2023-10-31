#include "uxn.h"

/*
Copyright (u) 2022-2023 Devine Lu Linvega, Andrew Alderwick, Andrew Richards

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define FLIP       { s = ins & 0x40 ? &u->wst : &u->rst; }
#define JUMP(x)    { if(m2) pc = (x); else pc += (Sint8)(x); }
#define POKE(x, y) { if(m2) { tp = &ram[x]; POKE2(tp, y) } else ram[(x)] = (y); }
#define PEEK(o, x) { if(m2) { tp = &ram[x]; o = PEEK2(tp); } else o = ram[(x)]; }
#define DEVR(o, p) { if(m2) { o = (emu_dei(u, p) << 8) | emu_dei(u, p + 1); } else o = emu_dei(u, p); }
#define DEVW(p, y) { if(m2) { emu_deo(u, p, y >> 8); emu_deo(u, p + 1, y); } else emu_deo(u, p, y); }
#define PUSH1(y)   { s->dat[s->ptr++] = (y); }
#define PUSH2(y)   { t = (y); s->dat[s->ptr++] = t >> 0x8; s->dat[s->ptr++] = t; }
#define PUSHx(y)   { if(m2) { PUSH2(y) } else PUSH1(y) }
#define POP1(o)    { o = s->dat[--*sp]; }
#define POP2(o)    { o = s->dat[--*sp] | (s->dat[--*sp] << 0x8); }
#define POPx(o)    { if(m2) { POP2(o) } else POP1(o) }

int
uxn_eval(Uxn *u, Uint16 pc)
{
	Uint8 ksp, *sp, *tp, *ram = u->ram;
	Uint16 a, b, c, t;
	if(!pc || u->dev[0x0f]) return 0;
	for(;;) {
		Uint8 ins = ram[pc++], opc = ins & 0x1f;
		Uint8 m2 = ins & 0x20;
		Stack *s = ins & 0x40 ? &u->rst : &u->wst;
		if(ins & 0x80) {
			ksp = s->ptr;
			sp = &ksp;
		} else
			sp = &s->ptr;
		/* Opcodes */
		switch(opc - (!opc * (ins >> 5))) {
		/* Immediate */
		case -0x0: /* BRK   */ return 1;
		case -0x1: /* JCI   */ POP1(b) if(!b) { pc += 2; break; } /* else fallthrough */
		case -0x2: /* JMI   */ pc += PEEK2(ram + pc) + 2; break;
		case -0x3: /* JSI   */ PUSH2(pc + 2) pc += PEEK2(ram + pc) + 2; break;
		case -0x4: /* LIT   */
		case -0x6: /* LITr  */ PUSH1(ram[pc++]) break;
		case -0x5: /* LIT2  */
		case -0x7: /* LIT2r */ PUSH2(PEEK2(ram + pc)) pc += 2; break;
		/* ALU */
		case 0x01: /* INC */ POPx(a) PUSHx(a + 1) break;
		case 0x02: /* POP */ POPx(a) break;
		case 0x03: /* NIP */ POPx(a) POPx(b) PUSHx(a) break;
		case 0x04: /* SWP */ POPx(a) POPx(b) PUSHx(a) PUSHx(b) break;
		case 0x05: /* ROT */ POPx(a) POPx(b) POPx(c) PUSHx(b) PUSHx(a) PUSHx(c) break;
		case 0x06: /* DUP */ POPx(a) PUSHx(a) PUSHx(a) break;
		case 0x07: /* OVR */ POPx(a) POPx(b) PUSHx(b) PUSHx(a) PUSHx(b) break;
		case 0x08: /* EQU */ POPx(a) POPx(b) PUSH1(b == a) break;
		case 0x09: /* NEQ */ POPx(a) POPx(b) PUSH1(b != a) break;
		case 0x0a: /* GTH */ POPx(a) POPx(b) PUSH1(b > a) break;
		case 0x0b: /* LTH */ POPx(a) POPx(b) PUSH1(b < a) break;
		case 0x0c: /* JMP */ POPx(a) JUMP(a) break;
		case 0x0d: /* JCN */ POPx(a) POP1(b) if(b) JUMP(a) break;
		case 0x0e: /* JSR */ POPx(a) FLIP PUSH2(pc) JUMP(a) break;
		case 0x0f: /* STH */ POPx(a) FLIP PUSHx(a) break;
		case 0x10: /* LDZ */ POP1(a) PEEK(b, a) PUSHx(b) break;
		case 0x11: /* STZ */ POP1(a) POPx(b) POKE(a, b) break;
		case 0x12: /* LDR */ POP1(a) PEEK(b, pc + (Sint8)a) PUSHx(b) break;
		case 0x13: /* STR */ POP1(a) POPx(b) POKE(pc + (Sint8)a, b) break;
		case 0x14: /* LDA */ POP2(a) PEEK(b, a) PUSHx(b) break;
		case 0x15: /* STA */ POP2(a) POPx(b) POKE(a, b) break;
		case 0x16: /* DEI */ POP1(a) DEVR(b, a) PUSHx(b) break;
		case 0x17: /* DEO */ POP1(a) POPx(b) DEVW(a, b) break;
		case 0x18: /* ADD */ POPx(a) POPx(b) PUSHx(b + a) break;
		case 0x19: /* SUB */ POPx(a) POPx(b) PUSHx(b - a) break;
		case 0x1a: /* MUL */ POPx(a) POPx(b) PUSHx((Uint32)b * a) break;
		case 0x1b: /* DIV */ POPx(a) POPx(b) PUSHx(a ? b / a : 0) break;
		case 0x1c: /* AND */ POPx(a) POPx(b) PUSHx(b & a) break;
		case 0x1d: /* ORA */ POPx(a) POPx(b) PUSHx(b | a) break;
		case 0x1e: /* EOR */ POPx(a) POPx(b) PUSHx(b ^ a) break;
		case 0x1f: /* SFT */ POP1(a) POPx(b) PUSHx(b >> (a & 0xf) << (a >> 4)) break;
		}
	}
}
