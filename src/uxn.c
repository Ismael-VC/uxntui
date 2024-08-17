#include "uxn.h"

/*
Copyright (u) 2022-2024 Devine Lu Linvega, Andrew Alderwick, Andrew Richards

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define OPC(opc, init, body) {\
	case 0x00|opc: {enum{_2=0,_r=0};init;body;} break;\
	case 0x20|opc: {enum{_2=1,_r=0};init;body;} break;\
	case 0x40|opc: {enum{_2=0,_r=1};init;body;} break;\
	case 0x60|opc: {enum{_2=1,_r=1};init;body;} break;\
	case 0x80|opc: {enum{_2=0,_r=0};int k=uxn.wst.ptr;init;uxn.wst.ptr= k;body;} break;\
	case 0xa0|opc: {enum{_2=1,_r=0};int k=uxn.wst.ptr;init;uxn.wst.ptr= k;body;} break;\
	case 0xc0|opc: {enum{_2=0,_r=1};int k=uxn.rst.ptr;init;uxn.rst.ptr= k;body;} break;\
	case 0xe0|opc: {enum{_2=1,_r=1};int k=uxn.rst.ptr;init;uxn.rst.ptr= k;body;} break;\
}

/* Microcode */
#define JMI pc += uxn.ram[pc++] << 8 | uxn.ram[pc++];
#define JMP(i) if(_2) pc = (i); else pc += (Sint8)(i);
/* stack-registers */
#define REM if(_r) uxn.rst.ptr -= 1 + _2; else uxn.wst.ptr -= 1 + _2;
#define INC(s) uxn.s.dat[uxn.s.ptr++]
#define DEC(s) uxn.s.dat[--uxn.s.ptr]
#define POP(o) if(_r) o = DEC(rst); else o = DEC(wst);
#define PUx(i) if(_2) { tt = (i); PU1(tt >> 8) PU1(tt) } else PU1(i)
#define PU1(i) if(_r) INC(rst) = i; else INC(wst) = i;
#define RP1(i) if(_r) INC(wst) = i; else INC(rst) = i;
#define A if(_2) { A2 } else A1
#define A1 POP(a)
#define A2 if(_r) a = DEC(rst) | (DEC(rst) << 8); else a = DEC(wst) | (DEC(wst) << 8);
#define B if(_2) { B2 } else B1
#define B1 POP(b)
#define B2 if(_r) b = DEC(rst) | (DEC(rst) << 8); else b = DEC(wst) | (DEC(wst) << 8);
/* memory-registers */
#define PUT(i) PU1(i[0]) if(_2) PU1(i[1])
#define DEI(i,o) o[0] =emu_dei(i); if(_2) o[1] =emu_dei(i + 1);
#define DEO(i,j) emu_deo(i, j[0]); if(_2) emu_deo(i + 1, j[1]);
#define PEK(i,o,m) o[0] = uxn.ram[i]; if(_2) o[1] = uxn.ram[(i + 1) & m];
#define POK(i,j,m) uxn.ram[i] = j[0]; if(_2) uxn.ram[(i + 1) & m] = j[1];
#define X if(_2) POP(x[1]) POP(x[0])
#define Y if(_2) POP(y[1]) POP(y[0])
#define Z if(_2) POP(z[1]) POP(z[0])

int
uxn_eval(Uint16 pc)
{
	int a,b,x[2],y[2],z[2];
	Uint16 tt;
	if(!pc || uxn.dev[0x0f]) return 0;
	for(;;) {
		switch(uxn.ram[pc++]) {
		/* BRK */ case 0x00: return 1;
		/* JCI */ case 0x20: if(DEC(wst)) { JMI break; } pc += 2; break;
		/* JMI */ case 0x40: JMI break;
		/* JSI */ case 0x60: tt = pc + 2; INC(rst) = tt >> 8; INC(rst) = tt; JMI break;
		/* LI2 */ case 0xa0: INC(wst) = uxn.ram[pc++];
		/* LIT */ case 0x80: INC(wst) = uxn.ram[pc++]; break;
		/* L2r */ case 0xe0: INC(rst) = uxn.ram[pc++];
		/* LIr */ case 0xc0: INC(rst) = uxn.ram[pc++]; break;
		/* INC */ OPC(0x01, A, PUx(a + 1))
		/* POP */ OPC(0x02, REM, 0)
		/* NIP */ OPC(0x03, X REM, PUT(x))
		/* SWP */ OPC(0x04, X Y, PUT(x) PUT(y))
		/* ROT */ OPC(0x05, X Y Z, PUT(y) PUT(x) PUT(z))
		/* DUP */ OPC(0x06, X, PUT(x) PUT(x))
		/* OVR */ OPC(0x07, X Y, PUT(y) PUT(x) PUT(y))
		/* EQU */ OPC(0x08, A B, PU1(b == a))
		/* NEQ */ OPC(0x09, A B, PU1(b != a))
		/* GTH */ OPC(0x0a, A B, PU1(b > a))
		/* LTH */ OPC(0x0b, A B, PU1(b < a))
		/* JMP */ OPC(0x0c, A, JMP(a))
		/* JCN */ OPC(0x0d, A B1, if(b) JMP(a))
		/* JSR */ OPC(0x0e, A, RP1(pc >> 8) RP1(pc) JMP(a))
		/* STH */ OPC(0x0f, X, RP1(x[0]) if(_2) RP1(x[1]))
		/* LDZ */ OPC(0x10, A1, PEK(a, x, 0xff) PUT(x))
		/* STZ */ OPC(0x11, A1 Y, POK(a, y, 0xff))
		/* LDR */ OPC(0x12, A1, PEK(pc+(Sint8)a, x, 0xffff) PUT(x))
		/* STR */ OPC(0x13, A1 Y, POK(pc+(Sint8)a, y, 0xffff))
		/* LDA */ OPC(0x14, A2, PEK(a, x, 0xffff) PUT(x))
		/* STA */ OPC(0x15, A2 Y, POK(a, y, 0xffff))
		/* DEI */ OPC(0x16, A1, DEI(a, x) PUT(x))
		/* DEO */ OPC(0x17, A1 Y, DEO(a, y))
		/* ADD */ OPC(0x18, A B, PUx(b + a))
		/* SUB */ OPC(0x19, A B, PUx(b - a))
		/* MUL */ OPC(0x1a, A B, PUx(b * a))
		/* DIV */ OPC(0x1b, A B, PUx(a ? b / a : 0))
		/* AND */ OPC(0x1c, A B, PUx(b & a))
		/* ORA */ OPC(0x1d, A B, PUx(b | a))
		/* EOR */ OPC(0x1e, A B, PUx(b ^ a))
		/* SFT */ OPC(0x1f, A1 B, PUx(b >> (a & 0xf) << (a >> 4)))
		}
	}
}
