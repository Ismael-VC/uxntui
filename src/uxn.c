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
	case 0x80|opc: {enum{_2=0,_r=0};k=uxn.wst.ptr;init;uxn.wst.ptr= k;body;} break;\
	case 0xa0|opc: {enum{_2=1,_r=0};k=uxn.wst.ptr;init;uxn.wst.ptr= k;body;} break;\
	case 0xc0|opc: {enum{_2=0,_r=1};k=uxn.rst.ptr;init;uxn.rst.ptr= k;body;} break;\
	case 0xe0|opc: {enum{_2=1,_r=1};k=uxn.rst.ptr;init;uxn.rst.ptr= k;body;} break;\
}

/* Microcode */

#define REM if(_r) uxn.rst.ptr -= 1 + _2; else uxn.wst.ptr -= 1 + _2;
#define INC(s) uxn.s.dat[uxn.s.ptr++]
#define DEC(s) uxn.s.dat[--uxn.s.ptr]
#define POP(o) if(_r) o = DEC(rst); else o = DEC(wst);
#define PUx(y) if(_2) { tt = (y); PU1(tt >> 8) PU1(tt) } else PU1(y)
#define PU1(y) if(_r) INC(rst) = y; else INC(wst) = y;
#define PUR(y) if(_r) INC(wst) = y; else INC(rst) = y;
#define PUT(y,z) PU1(y) if(_2) PU1(z)
#define JMI pc += uxn.ram[pc++] << 8 | uxn.ram[pc++];
#define JMP(j) if(_2) pc = (j); else pc += (Sint8)(j);
#define DEI(i,o,p) o =emu_dei(i); if(_2) p =emu_dei(i + 1);
#define DEO(i,y,z) emu_deo(i, y); if(_2) emu_deo(i + 1, z);
#define PEK(i,o,p,r) o = uxn.ram[i]; if(_2) r = i + 1, p = uxn.ram[r];
#define POK(i,y,z,r) uxn.ram[i] = y; if(_2) r = i + 1, uxn.ram[r] = z;

/* stack-registers */

#define A if(_2) { A2 } else A1
#define A1 POP(a)
#define A2 if(_r) a = DEC(rst) | (DEC(rst) << 8); else a = DEC(wst) | (DEC(wst) << 8);
#define B if(_2) { B2 } else B1
#define B1 POP(b)
#define B2 if(_r) b = DEC(rst) | (DEC(rst) << 8); else b = DEC(wst) | (DEC(wst) << 8);

/* memory-registers */

#define X if(_2) POP(xx) POP(x)
#define Y if(_2) POP(yy) POP(y)
#define Z if(_2) POP(zz) POP(z)

int
uxn_eval(Uint16 pc)
{
	int a,b,c,x,xx,y,yy,z,zz,k;
	Uint8 t;
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
		/* NIP */ OPC(0x03, X REM, PUT(x,xx))
		/* SWP */ OPC(0x04, X Y, PUT(x,xx) PUT(y,yy))
		/* ROT */ OPC(0x05, X Y Z, PUT(y,yy) PUT(x,xx) PUT(z,zz))
		/* DUP */ OPC(0x06, X, PUT(x,xx) PUT(x,xx))
		/* OVR */ OPC(0x07, X Y, PUT(y,yy) PUT(x,xx) PUT(y,yy))
		/* EQU */ OPC(0x08, A B, PU1(b == a))
		/* NEQ */ OPC(0x09, A B, PU1(b != a))
		/* GTH */ OPC(0x0a, A B, PU1(b > a))
		/* LTH */ OPC(0x0b, A B, PU1(b < a))
		/* JMP */ OPC(0x0c, A, JMP(a))
		/* JCN */ OPC(0x0d, A B1, if(b) JMP(a))
		/* JSR */ OPC(0x0e, A, PUR(pc >> 8) PUR(pc) JMP(a))
		/* STH */ OPC(0x0f, X, PUR(x) if(_2) PUR(xx))
		/* LDZ */ OPC(0x10, A1, PEK(a,x,xx,t) PUT(x,xx))
		/* STZ */ OPC(0x11, A1 Y, POK(a,y,yy,t))
		/* LDR */ OPC(0x12, A1, PEK(pc+(Sint8)a,x,xx,tt) PUT(x,xx))
		/* STR */ OPC(0x13, A1 Y, POK(pc+(Sint8)a,y,yy,tt))
		/* LDA */ OPC(0x14, A2, PEK(a,x,xx,tt) PUT(x,xx))
		/* STA */ OPC(0x15, A2 Y, POK(a,y,yy,tt))
		/* DEI */ OPC(0x16, A1, DEI(a,x,xx) PUT(x,xx))
		/* DEO */ OPC(0x17, A1 Y, DEO(a,y,yy))
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
