						/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * core.c
 * Copyright (C) abhoriel 2010 <abhoriel@gmail.com>
 * 
 * gbem is free software copyrighted by abhoriel.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name ``abhoriel'' nor the name of any other
 *    contributor may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * gbem IS PROVIDED BY abhoriel ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL abhoriel OR ANY OTHER CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <assert.h>
#include "core.h"
#include "memory.h"
#include "debug.h"
#include "save.h"

#define	REG_A   (core.reg_af.b.h)
#define	REG_F   (core.reg_af.b.l) 	// must be set manually
#define	REG_B   (core.reg_bc.b.h)
#define	REG_C   (core.reg_bc.b.l)
#define	REG_D   (core.reg_de.b.h)
#define	REG_E   (core.reg_de.b.l)
#define	REG_H   (core.reg_hl.b.h)
#define	REG_L   (core.reg_hl.b.l)
#define	REG_AF  (core.reg_af.w)
#define	REG_BC  (core.reg_bc.w)
#define	REG_DE  (core.reg_de.w)
#define	REG_HL  (core.reg_hl.w)
#define	REG_SP  (core.reg_sp)
#define REG_PC  (core.reg_pc)
#define FLAG_Z  (core.flag_z)
#define FLAG_C  (core.flag_c)
#define FLAG_N  (core.flag_n)
#define FLAG_H  (core.flag_h)


static inline void handle_interrupts();
static inline void handle_interrupt(Byte interrupt, Word Vector, Byte reg_if);


/* arithmetic */
static inline Byte add_bbb(Byte a, Byte b);
static inline Word add_wwb(Word a, Byte b);
static inline Word add_www(Word a, Word b);
static inline Byte adc(Byte a, Byte b);
static inline Byte sub(Byte a, Byte b);
static inline Byte sbc(Byte a, Byte b);
static inline Byte inc_bb(Byte a);
static inline Word inc_ww(Word a);
static inline Byte dec_bb(Byte a);
static inline Word dec_ww(Word a);

/* binary */
static inline Byte and(Byte a, Byte b);
static inline Byte or(Byte a, Byte b);
static inline Byte xor(Byte a, Byte b);

/* stack */
static inline void push(Word a);
static inline Word pop();

/* other */
static inline Byte swap(Byte a);
static inline Byte daa(Byte a);
static inline Byte rlc(Byte a);
static inline Byte rl(Byte a);
static inline Byte rrc(Byte a);
static inline Byte rr(Byte a);
static inline Byte sla(Byte a);
static inline Byte sra(Byte a);
static inline Byte srl(Byte a);
static inline void bit(Byte a, Byte b);
static inline Byte set(Byte a, Byte b);
static inline Byte res(Byte a, Byte b);
static inline void jr(Byte a);
static inline void call(Word a);
static inline void rst(Byte a);
static inline void ret();

static inline void handle_interrupts();

extern int console;
extern int console_mode;

extern int sound_cycles;

CoreState core;
int debugging = 0;

int execute_cycles(int max_cycles) {
	int cycles = 0;
	int total_cycles = 0;
	Byte opcode;
	while (total_cycles < max_cycles) {
		cycles = 0;
		
		/* check for interrupts */
		handle_interrupts();

		if (core.is_halted == 1) {
/*
			if (core.ime == 0) {
				core.is_halted = 0;
			}
*/
			sound_cycles += max_cycles - total_cycles;
			return max_cycles;
		}

		if (debugging)
			disasm_exec(REG_PC);

		// switch opcode
		switch (readb(REG_PC++)) {
			/* 8bit loads: imm -> reg */
			case 0x06:  /* LD B, n */
				REG_B = readb(REG_PC++);
				cycles = 8;
				break;
			case 0x0E:  /* LD C, n */
				REG_C = readb(REG_PC++);
				cycles = 8;
				break;
			case 0x16:  /* LD D, n */
				REG_D = readb(REG_PC++);
				cycles = 8;
				break;
			case 0x1E:  /* LD E, n */
				REG_E = readb(REG_PC++);
				cycles = 8;
				break;
			case 0x26:  /* LD H, n */
				REG_H = readb(REG_PC++);
				cycles = 8;
				break;
			case 0x2E:  /* LD L, n */
				REG_L = readb(REG_PC++);
				cycles = 8;
				break;
			/* 8bit loads: reg -> reg */
			case 0x7F:  /* LD A, A */
				cycles = 4;
				break;
			case 0x78:  /* LD A, B */
				REG_A = REG_B;
				cycles = 4;
				break;
			case 0x79:  /* LD A, C */
				REG_A = REG_C;
				cycles = 4;
				break;
			case 0x7A:  /* LD A, D */
				REG_A = REG_D;
				cycles = 4;
				break;
			case 0x7B:  /* LD A, E */
				REG_A = REG_E;
				cycles = 4;
				break;
			case 0x7C:  /* LD A, H */
				REG_A = REG_H;
				cycles = 4;
				break;
			case 0x7D:  /* LD A, L */
				REG_A = REG_L;
				cycles = 4;
				break;
			case 0x7E:  /* LD A, (HL) */
				REG_A = readb(REG_HL);
				cycles = 8;
				break;
			case 0x40:  /* LD B, B */
				cycles = 4;
				break;
			case 0x41:  /* LD B, C */
				REG_B = REG_C;
				cycles = 4;
				break;
			case 0x42:  /* LD B, D */
				REG_B = REG_D;
				cycles = 4;
				break;
			case 0x43:  /* LD B, E */
				REG_B = REG_E;
				cycles = 4;
				break;
			case 0x44:  /* LD B, H */
				REG_B = REG_H;
				cycles = 4;
				break;
			case 0x45:  /* LD B, L */
				REG_B = REG_L;
				cycles = 4;
				break;
			case 0x46:  /* LD B, (HL) */
				REG_B = readb(REG_HL);
				cycles = 8;
				break;
			case 0x48:  /* LD C, B */
				REG_C = REG_B;
				cycles = 4;
				break;
			case 0x49:  /* LD C, C */
				cycles = 4;
				break;
			case 0x4A:  /* LD C, D */
				REG_C = REG_D;
				cycles = 4;
				break;
			case 0x4B:  /* LD C, E */
				REG_C = REG_E;
				cycles = 4;
				break;
			case 0x4C:  /* LD C, H */
				REG_C = REG_H;
				cycles = 4;
				break;
			case 0x4D:  /* LD C, L */
				REG_C = REG_L;
				cycles = 4;
				break;
			case 0x4E:  /* LD C, (HL) */
				REG_C = readb(REG_HL);
				cycles = 8;
				break;
			case 0x50:  /* LD D, B */
				REG_D = REG_B;
				cycles = 4;
				break;
			case 0x51:  /* LD D, C */
				REG_D = REG_C;
				cycles = 4;
				break;
			case 0x52:  /* LD D, D */
				cycles = 4;
				break;
			case 0x53:  /* LD D, E */
				REG_D = REG_E;
				cycles = 4;
				break;
			case 0x54:  /* LD D, H */
				REG_D = REG_H;
				cycles = 4;
				break;
			case 0x55:  /* LD D, L */
				REG_D = REG_L;
				cycles = 4;
				break;
			case 0x56:  /* LD D, (HL) */
				REG_D = readb(REG_HL);
				cycles = 8;
				break;
			case 0x58:  /* LD E, B */
				REG_E = REG_B;
				cycles = 4;
				break;
			case 0x59:  /* LD E, C */
				REG_E = REG_C;
				cycles = 4;
				break;
			case 0x5A:  /* LD E, D */
				REG_E = REG_D;
				cycles = 4;
				break;
			case 0x5B:  /* LD E, E */
				cycles = 4;
				break;
			case 0x5C:  /* LD E, H */
				REG_E = REG_H;
				cycles = 4;
				break;
			case 0x5D:  /* LD E, L */
				REG_E = REG_L;
				cycles = 4;
				break;
			case 0x5E:  /* LD E, (HL) */
				REG_E = readb(REG_HL);
				cycles = 8;
				break;
			case 0x60:  /* LD H, B */
				REG_H = REG_B;
				cycles = 4;
				break;
			case 0x61:  /* LD H, C */
				REG_H = REG_C;
				cycles = 4;
				break;
			case 0x62:  /* LD H, D */
				REG_H = REG_D;
				cycles = 4;
				break;
			case 0x63:  /* LD H, E */
				REG_H = REG_E;
				cycles = 4;
				break;
			case 0x64:  /* LD H, H */
				cycles = 4;
				break;
			case 0x65:  /* LD H, L */
				REG_H = REG_L;
				cycles = 4;
				break;
			case 0x66:  /* LD H, (HL) */
				REG_H = readb(REG_HL);
				cycles = 8;
				break;			
			case 0x68:  /* LD L, B */
				REG_L = REG_B;
				cycles = 4;
				break;
			case 0x69:  /* LD L, C */
				REG_L = REG_C;
				cycles = 4;
				break;
			case 0x6A:  /* LD L, D */
				REG_L = REG_D;
				cycles = 4;
				break;
			case 0x6B:  /* LD L, E */
				REG_L = REG_E;
				cycles = 4;
				break;
			case 0x6C:  /* LD L, H */
				REG_L = REG_H;
				cycles = 4;
				break;
			case 0x6D:  /* LD L, L */
				cycles = 4;
				break;
			case 0x6E:  /* LD L, (HL) */
				REG_L = readb(REG_HL);
				cycles = 8;
				break;
			/* 8bit loads: reg -> (HL) */
			case 0x70:  /* LD (HL), B */
				writeb(REG_HL, REG_B);
				cycles = 8;
				break;
			case 0x71:  /* LD (HL), C */
				writeb(REG_HL, REG_C);
				cycles = 8;
				break;
			case 0x72:  /* LD (HL), D */
				writeb(REG_HL, REG_D);
				cycles = 8;
				break;
			case 0x73:  /* LD (HL), E */
				writeb(REG_HL, REG_E);
				cycles = 8;
				break;
			case 0x74:  /* LD (HL), H */
				writeb(REG_HL, REG_H);
				cycles = 8;
				break;
			case 0x75:  /* LD (HL), L */
				writeb(REG_HL, REG_L);
				cycles = 8;
				break;
			case 0x36:  /* LD (HL), n */
				writeb(REG_HL, readb(REG_PC++));
				cycles = 12;
				break;
			case 0x0A:  /* LD A, (BC) */
				REG_A = readb(REG_BC);
				cycles = 8;
				break;
			case 0x1A:  /* LD A, (DE) */
				REG_A = readb(REG_DE);
				cycles = 8;
				break;
			case 0xFA:  /* LD A, (nn) */
				REG_A = readb(readw(REG_PC));
				REG_PC += 2;
				cycles = 16;
				break;
			case 0x3E:  /* LD A, n */
				REG_A = readb(REG_PC++);
				cycles = 8;
				break;
			case 0x47:  /* LD B, A */
				REG_B = REG_A;
				cycles = 4;
				break;
			case 0x4F:  /* LD C, A */
				REG_C = REG_A;
				cycles = 4;
				break;
			case 0x57:  /* LD D, A */
				REG_D = REG_A;
				cycles = 4;
				break;
			case 0x5F:  /* LD E, A */
				REG_E = REG_A;
				cycles = 4;
				break;
			case 0x67:  /* LD H, A */
				REG_H = REG_A;
				cycles = 4;
				break;
			case 0x6F:  /* LD L, A */
				REG_L = REG_A;
				cycles = 4;
				break;
			case 0x02:  /* LD (BC), A */
				writeb(REG_BC, REG_A);
				cycles = 8;
				break;
			case 0x12:  /* LD (DE), A */
				writeb(REG_DE, REG_A);
				cycles = 8;
				break;
			case 0x77:  /* LD (HL), A */
				writeb(REG_HL, REG_A);
				cycles = 8;
				break;
			case 0xEA:  /* LD (nn), A */
				writeb(readw(REG_PC), REG_A);
				REG_PC += 2;
				cycles = 16;
				break;
			case 0xF2:  /* LD A, (C) */
				REG_A = readb(REG_C + 0xFF00);
				cycles = 8;
				break;
			case 0xE2:  /* LD (C), A */
				writeb(REG_C + 0xFF00, REG_A);
				cycles = 8;
				
			break;
			/* 8bit loads/dec/inc */
			case 0x3A:  /* LDD A, (HL) */
				REG_A = readb(REG_HL--);
				cycles = 8;
				break;	
			case 0x32:  /* LDD (HL), A */
				writeb(REG_HL--, REG_A);
				cycles = 8;
				break;
			case 0x2A:  /* LDI A, (HL) */
				REG_A = readb(REG_HL++);
				cycles = 8;
				break;
			case 0x22:  /* LDI (HL), A */
				writeb(REG_HL++, REG_A);
				cycles = 8;
				break;
			case 0xE0:  /* LDH (n), A */
				writeb(readb(REG_PC++) + 0xFF00, REG_A);
				cycles = 12;
				break;
			case 0xF0:  /* LDH A, (n) */
				REG_A = readb(0xFF00 + readb(REG_PC++));
				cycles = 12;
				break;
			/* 16bit loads */
			case 0x01:  /* LD BC, nn */
				REG_BC = readw(REG_PC);
				REG_PC += 2;
				cycles = 12;
				break;
			case 0x11:  /* LD DE, nn */
				REG_DE = readw(REG_PC);
				REG_PC += 2;
				cycles = 12;
				break;
			case 0x21:  /* LD HL, nn */
				REG_HL = readw(REG_PC);
				REG_PC += 2;
				cycles = 12;
				break;
			case 0x31:  /* LD SP, nn */
				REG_SP = readw(REG_PC);
				REG_PC += 2;
				cycles = 12;
				break;
			case 0xF9:  /* LD SP, HL */
				REG_SP = REG_HL;
				cycles = 8;
				break;
			case 0xF8:  /* LDHL SP, n */
				REG_HL = add_wwb(REG_SP, readb(REG_PC++));
				cycles = 12;
				break;
			case 0x08: // LD (nn), SP
				writew(readw(REG_PC), REG_SP);
				cycles = 20;
				REG_PC += 2;
				break;
			case 0xF5:	// PUSH AF
				// Flags are stored in their own ints, not in REG_F, so we must
				// produce REG_F here. (This is for efficiency reasons, only 
				// PUSH AF and POP AF actually use REG_F/REG_AF)
				REG_F = (FLAG_C << 4) | (FLAG_H << 5) | (FLAG_N << 6) 
				              | (FLAG_Z << 7);
				push(REG_AF);
				cycles = 16;
				break;
			case 0xC5:	// PUSH BC
				push(REG_BC);
				cycles = 16;
				break;
			case 0xD5:	// PUSH DE
				push(REG_DE);
				cycles = 16;
				break;
			case 0xE5:	// PUSH HL
				push(REG_HL);
				cycles = 16;
				break;
			case 0xF1:	// POP AF
				// Flags are stored in their own ints, not in REG_F, so we must
				// produce the ints here. (This is for efficiency reasons, only 
				// PUSH AF and POP AF actually use REG_F/REG_AF)
				REG_AF = pop();
				FLAG_C = (REG_F & 0x10) >> 4; FLAG_H = (REG_F & 0x20) >> 5;
				FLAG_N = (REG_F & 0x40) >> 6; FLAG_Z = (REG_F & 0x80) >> 7;
				cycles = 12;
				break;
			case 0xC1:	// POP BC
				REG_BC = pop();
				cycles = 12;
				break;
			case 0xD1:	// POP DE
				REG_DE = pop();
				cycles = 12;
				break;
			case 0xE1:	// POP HL
				REG_HL = pop();
				cycles = 12;
				break;
			case 0x87:	// ADD A, A
				REG_A = add_bbb(REG_A, REG_A);
				cycles = 4;
				break;
			case 0x80:	// ADD A, B
				REG_A = add_bbb(REG_A, REG_B);
				cycles = 4;
				break;
			case 0x81:	// ADD A, C
				REG_A = add_bbb(REG_A, REG_C);
				cycles = 4;
				break;
			case 0x82:	// ADD A, D
				REG_A = add_bbb(REG_A, REG_D);
				cycles = 4;
				break;
			case 0x83:	// ADD A, E
				REG_A = add_bbb(REG_A, REG_E);
				cycles = 4;
				break;
			case 0x84:	// ADD A, H
				REG_A = add_bbb(REG_A, REG_H);
				cycles = 4;
				break;
			case 0x85:	// ADD A, L
				REG_A = add_bbb(REG_A, REG_L);
				cycles = 4;
				break;
			case 0x86:	// ADD A, (HL)
				REG_A = add_bbb(REG_A, readb(REG_HL));
				cycles = 8;
				break;
			case 0xC6:	// ADD A, n
				REG_A = add_bbb(REG_A, readb(REG_PC++));
				cycles = 8;
				break;
			case 0x8F:	// ADC A, A
				REG_A = adc(REG_A, REG_A);
				cycles = 4;
				break;
			case 0x88:	// ADC A, B
				REG_A = adc(REG_A, REG_B);
				cycles = 4;
				break;
			case 0x89:	// ADC A, C
				REG_A = adc(REG_A, REG_C);
				cycles = 4;
				break;
			case 0x8A:	// ADC A, D
				REG_A = adc(REG_A, REG_D);
				cycles = 4;
				break;
			case 0x8B:	// ADC A, E
				REG_A = adc(REG_A, REG_E);
				cycles = 4;
				break;
			case 0x8C:	// ADC A, H
				REG_A = adc(REG_A, REG_H);
				cycles = 4;
				break;
			case 0x8D:	// ADC A, L
				REG_A = adc(REG_A, REG_L);
				cycles = 4;
				break;
			case 0x8E:	// ADC A, (HL)
				REG_A = adc(REG_A, readb(REG_HL));
				cycles = 8;
				break;
			case 0xCE:	// ADC A, n
				REG_A = adc(REG_A, readb(REG_PC++));
				cycles = 8;
				break;
			case 0x97:	// SUB A, A
				REG_A = sub(REG_A, REG_A);
				cycles = 4;
				break;
			case 0x90:	// SUB A, B
				REG_A = sub(REG_A, REG_B);
				cycles = 4;
				break;
			case 0x91:	// SUB A, C
				REG_A = sub(REG_A, REG_C);
				cycles = 4;
				break;
			case 0x92:	// SUB A, D
				REG_A = sub(REG_A, REG_D);
				cycles = 4;
				break;
			case 0x93:	// SUB A, E
				REG_A = sub(REG_A, REG_E);
				cycles = 4;
				break;
			case 0x94:	// SUB A, H
				REG_A = sub(REG_A, REG_H);
				cycles = 4;
				break;
			case 0x95:	// SUB A, L
				REG_A = sub(REG_A, REG_L);
				cycles = 4;
				break;
			case 0x96:	// SUB A, (HL)
				REG_A = sub(REG_A, readb(REG_HL));
				cycles = 8;
				break;
			case 0xD6:	// SUB A, n
				REG_A = sub(REG_A, readb(REG_PC++));
				cycles = 8;
				break;
			case 0x9F:	// SBC A, A
				REG_A = sbc(REG_A, REG_A);
				cycles = 4;
				break;
			case 0x98:	// SBC A, B
				REG_A = sbc(REG_A, REG_B);
				cycles = 4;
				break;			
			case 0x99:	// SBC A, C
				REG_A = sbc(REG_A, REG_C);
				cycles = 4;
				break;
			case 0x9A:	// SBC A, D
				REG_A = sbc(REG_A, REG_D);
				cycles = 4;
				break;
			case 0x9B:	// SBC A, E
				REG_A = sbc(REG_A, REG_E);
				cycles = 4;
				break;
			case 0x9C:	// SBC A, H
				REG_A = sbc(REG_A, REG_H);
				cycles = 4;
				break;
			case 0x9D:	// SBC A, L
				REG_A = sbc(REG_A, REG_L);
				cycles = 4;
				break;
			case 0x9E:	// SBC A, (HL)
				REG_A = sbc(REG_A, readb(REG_HL));
				cycles = 8;
				break;
			case 0xDE:	// SBC A, n
				REG_A = sbc(REG_A, readb(REG_PC++));
				cycles = 8;
				break;
			case 0xA7:	// AND A
				REG_A = and(REG_A, REG_A);
				cycles = 4;
				break;
			case 0xA0:	// AND B
				REG_A = and(REG_A, REG_B);
				cycles = 4;
				break;
			case 0xA1:	// AND C
				REG_A = and(REG_A, REG_C);
				cycles = 4;
				break;
			case 0xA2:	// AND D
				REG_A = and(REG_A, REG_D);
				cycles = 4;
				break;
			case 0xA3:	// AND E
				REG_A = and(REG_A, REG_E);
				cycles = 4;
				break;
			case 0xA4:	// AND H
				REG_A = and(REG_A, REG_H);
				cycles = 4;
				break;
			case 0xA5:	// AND L
				REG_A = and(REG_A, REG_L);
				cycles = 4;
				break;
			case 0xA6:	// AND (HL)
				REG_A = and(REG_A, readb(REG_HL));
				cycles = 8;
				break;
			case 0xE6:	// AND n
				REG_A = and(REG_A, readb(REG_PC++));
				cycles = 8;
				break;
			case 0xB7:	// OR A
				REG_A = or(REG_A, REG_A);
				cycles = 4;
				break;
			case 0xB0:	// OR B
				REG_A = or(REG_A, REG_B);
				cycles = 4;
				break;
			case 0xB1:	// OR C
				REG_A = or(REG_A, REG_C);
				cycles = 4;
				break;
			case 0xB2:	// OR D
				REG_A = or(REG_A, REG_D);
				cycles = 4;
				break;
			case 0xB3:	// OR E
				REG_A = or(REG_A, REG_E);
				cycles = 4;
				break;
			case 0xB4:	// OR H
				REG_A = or(REG_A, REG_H);
				cycles = 4;
				break;
			case 0xB5:	// OR L
				REG_A = or(REG_A, REG_L);
				cycles = 4;
				break;
			case 0xB6:	// OR (HL)
				REG_A = or(REG_A, readb(REG_HL));
				cycles = 8;
				break;
			case 0xF6:	// OR n
				REG_A = or(REG_A, readb(REG_PC++));
				cycles = 8;
				break;
			case 0xAF:	// XOR A
				REG_A = xor(REG_A, REG_A);
				cycles = 4;
				break;
			case 0xA8:	// XOR B
				REG_A = xor(REG_A, REG_B);
				cycles = 4;
				break;
			case 0xA9:	// XOR C
				REG_A = xor(REG_A, REG_C);
				cycles = 4;
				break;
			case 0xAA:	// XOR D
				REG_A = xor(REG_A, REG_D);
				cycles = 4;
				break;
			case 0xAB:	// XOR E
				REG_A = xor(REG_A, REG_E);
				cycles = 4;
				break;
			case 0xAC:	// XOR H
				REG_A = xor(REG_A, REG_H);
				cycles = 4;
				break;
			case 0xAD:	// XOR L
				REG_A = xor(REG_A, REG_L);
				cycles = 4;
				break;
			case 0xAE:	// XOR (HL)
				REG_A = xor(REG_A, readb(REG_HL));
				cycles = 8;
				break;
			case 0xEE:	// XOR n
				REG_A = xor(REG_A, readb(REG_PC++));
				cycles = 8;
				break;
			case 0xBF:	// CP A
				sub(REG_A, REG_A);
				cycles = 4;
				break;
			case 0xB8:	// CP B
				sub(REG_A, REG_B);
				cycles = 4;
				break;
			case 0xB9:	// CP C
				sub(REG_A, REG_C);
				cycles = 4;
				break;
			case 0xBA:	// CP D
				sub(REG_A, REG_D);
				cycles = 4;
				break;
			case 0xBB:	// CP E
				sub(REG_A, REG_E);
				cycles = 4;
				break;
			case 0xBC:	// CP H
				sub(REG_A, REG_H);
				cycles = 4;
				break;
			case 0xBD:	// CP L
				sub(REG_A, REG_L);
				cycles = 4;
				break;
			case 0xBE:	// CP (HL)
				sub(REG_A, readb(REG_HL));
				cycles = 8;
				break;
			case 0xFE:	// CP n
				sub(REG_A, readb(REG_PC++));
				cycles = 8;
				break;
			case 0x3C:	// INC A
				REG_A = inc_bb(REG_A);
				cycles = 4;
				break;
			case 0x04:	// INC B
				REG_B = inc_bb(REG_B);
				cycles = 4;
				break;
			case 0x0C:	// INC C
				REG_C = inc_bb(REG_C);
				cycles = 4;
				break;
			case 0x14:	// INC D
				REG_D = inc_bb(REG_D);
				cycles = 4;
				break;
			case 0x1C:	// INC E
				REG_E = inc_bb(REG_E);
				cycles = 4;
				break;
			case 0x24:	// INC H
				REG_H = inc_bb(REG_H);
				cycles = 4;
				break;
			case 0x2C:	// INC L
				REG_L = inc_bb(REG_L);
				cycles = 4;
				break;
			case 0x34:	// INC (HL)
				writeb(REG_HL, inc_bb(readb(REG_HL)));
				cycles = 12;
				break;
			case 0x3D:	// DEC A
				REG_A = dec_bb(REG_A);
				cycles = 4;
				break;
			case 0x05:	// DEC B
				REG_B = dec_bb(REG_B);
				cycles = 4;
				break;
			case 0x0D:	// DEC C
				REG_C = dec_bb(REG_C);
				cycles = 4;
				break;
			case 0x15:	// DEC D
				REG_D = dec_bb(REG_D);
				cycles = 4;
				break;
			case 0x1D:	// DEC E
				REG_E = dec_bb(REG_E);
				cycles = 4;
				break;
			case 0x25:	// DEC H
				REG_H = dec_bb(REG_H);
				cycles = 4;
				break;
			case 0x2D:	// DEC L
				REG_L = dec_bb(REG_L);
				cycles = 4;
				break;
			case 0x35:	// DEC (HL)
				writeb(REG_HL, dec_bb(readb(REG_HL)));
				cycles = 12;
				break;
			case 0x09:	// ADD HL, BC
				REG_HL = add_www(REG_HL, REG_BC);
				cycles = 8;
				break;
			case 0x19:	// ADD HL, DE
				REG_HL = add_www(REG_HL, REG_DE);
				cycles = 8;
				break;
			case 0x29:	// ADD HL, HL
				REG_HL = add_www(REG_HL, REG_HL);
				cycles = 8;
				break;
			case 0x39:	// ADD HL, SP
				REG_HL = add_www(REG_HL, REG_SP);
				cycles = 8;
				break;
			case 0xE8:	// ADD SP, n
				REG_SP = add_wwb(REG_SP, readb(REG_PC++));
				cycles = 16;
				break;
			case 0x03:	// INC BC
				REG_BC = inc_ww(REG_BC);
				cycles = 8;
				break;
			case 0x13:	// INC DE
				REG_DE = inc_ww(REG_DE);
				cycles = 8;
				break;
			case 0x23:	// INC HL
				REG_HL = inc_ww(REG_HL);
				cycles = 8;
				break;
			case 0x33:	// INC SP
				REG_SP = inc_ww(REG_SP);
				cycles = 8;
				break;
			case 0x0B:	// DEC BC
				REG_BC = dec_ww(REG_BC);
				cycles = 8;
				break;
			case 0x1B:	// DEC DE
				REG_DE = dec_ww(REG_DE);
				cycles = 8;
				break;
			case 0x2B:	// DEC HL
				REG_HL = dec_ww(REG_HL);
				cycles = 8;
				break;
			case 0x3B:	// DEC SP
				REG_SP = dec_ww(REG_SP);
				cycles = 8;
				break;
			case 0xCB:	// Some two byte opcodes here.
				switch (readb(REG_PC++)) {
					case 0x37:	// SWAP A
						REG_A = swap(REG_A);
						cycles = 8;
						break;
					case 0x30:	// SWAP B
						REG_B = swap(REG_B);
						cycles = 8;
						break;
					case 0x31:	// SWAP C
						REG_C = swap(REG_C);
						cycles = 8;
						break;
					case 0x32:	// SWAP D
						REG_D = swap(REG_D);
						cycles = 8;
						break;
					case 0x33:	// SWAP E
						REG_E = swap(REG_E);
						cycles = 8;
						break;
					case 0x34:	// SWAP H
						REG_H = swap(REG_H);
						cycles = 8;
						break;
					case 0x35:	// SWAP L
						REG_L = swap(REG_L);
						cycles = 8;
						break;
					case 0x36:	// SWAP (HL)
						writeb(REG_HL, swap(readb(REG_HL)));
						cycles = 16;
						break;
					case 0x07:	// RLC A
						REG_A = rlc(REG_A);
						cycles = 8;
						break;
					case 0x00:	// RLC B
						REG_B = rlc(REG_B);
						cycles = 8;
						break;
					case 0x01:	// RLC C
						REG_C = rlc(REG_C);
						cycles = 8;
						break;
					case 0x02:	// RLC D
						REG_D = rlc(REG_D);
						cycles = 8;
						break;
					case 0x03:	// RLC E
						REG_E = rlc(REG_E);
						cycles = 8;
						break;
					case 0x04:	// RLC H
						REG_H = rlc(REG_H);
						cycles = 8;
						break;
					case 0x05:	// RLC L
						REG_L = rlc(REG_L);
						cycles = 8;
						break;
					case 0x06:	// RLC (HL)
						writeb(REG_HL, rlc(readb(REG_HL)));
						cycles = 16;
						break;
					case 0x17:	// RL A
						REG_A = rl(REG_A);
						cycles = 8;
						break;
					case 0x10:	// RL B
						REG_B = rl(REG_B);
						cycles = 8;
						break;
					case 0x11:	// RL C
						REG_C = rl(REG_C);
						cycles = 8;
						break;
					case 0x12:	// RL D
						REG_D = rl(REG_D);
						cycles = 8;
						break;
					case 0x13:	// RL E
						REG_E = rl(REG_E);
						cycles = 8;
						break;
					case 0x14:	// RL H
						REG_H = rl(REG_H);
						cycles = 8;
						break;
					case 0x15:	// RL L
						REG_L = rl(REG_L);
						cycles = 8;
						break;
					case 0x16:	// RL (HL)
						writeb(REG_HL, rl(readb(REG_HL)));
						cycles = 16;
						break;
					case 0x0F:	// RRC A
						REG_A = rrc(REG_A);
						cycles = 8;
						break;
					case 0x08:	// RRC B
						REG_B = rrc(REG_B);
						cycles = 8;
						break;
					case 0x09:	// RRC C
						REG_C = rrc(REG_C);
						cycles = 8;
						break;
					case 0x0A:	// RRC D
						REG_D = rrc(REG_D);
						cycles = 8;
						break;
					case 0x0B:	// RRC E
						REG_E = rrc(REG_E);
						cycles = 8;
						break;
					case 0x0C:	// RRC H
						REG_H = rrc(REG_H);
						cycles = 8;
						break;
					case 0x0D:	// RRC L
						REG_L = rrc(REG_L);
						cycles = 8;
						break;
					case 0x0E:	// RRC (HL)
						writeb(REG_HL, rrc(readb(REG_HL)));
						cycles = 16;
						break;
					case 0x1F:	// RR A
						REG_A = rr(REG_A);
						cycles = 8;
						break;
					case 0x18:	// RR B
						REG_B = rr(REG_B);
						cycles = 8;
						break;
					case 0x19:	// RR C
						REG_C = rr(REG_C);
						cycles = 8;
						break;
					case 0x1A:	// RR D
						REG_D = rr(REG_D);
						cycles = 8;
						break;
					case 0x1B:	// RR E
						REG_E = rr(REG_E);
						cycles = 8;
						break;
					case 0x1C:	// RR H
						REG_H = rr(REG_H);
						cycles = 8;
						break;
					case 0x1D:	// RR L
						REG_L = rr(REG_L);
						cycles = 8;
						break;
					case 0x1E:	// RR (HL)
						writeb(REG_HL, rr(readb(REG_HL)));
						cycles = 16;
						break;
					case 0x27:	// SLA A
						REG_A = sla(REG_A);
						cycles = 8;
						break;
					case 0x20:	// SLA B
						REG_B = sla(REG_B);
						cycles = 8;
						break;
					case 0x21:	// SLA C
						REG_C = sla(REG_C);
						cycles = 8;
						break;			
					case 0x22:	// SLA D
						REG_D = sla(REG_D);
						cycles = 8;
						break;
					case 0x23:	// SLA E
						REG_E = sla(REG_E);
						cycles = 8;
						break;
					case 0x24:	// SLA H
						REG_H = sla(REG_H);
						cycles = 8;
						break;
					case 0x25:	// SLA L
						REG_L = sla(REG_L);
						cycles = 8;
						break;
					case 0x26:	// SLA (HL)
						writeb(REG_HL, sla(readb(REG_HL)));
						cycles = 16;
						break;
					case 0x2F:	// SRA A
						REG_A = sra(REG_A);
						cycles = 8;
						break;
					case 0x28:	// SRA B
						REG_B = sra(REG_B);
						cycles = 8;
						break;
					case 0x29:	// SRA C
						REG_C = sra(REG_C);
						cycles = 8;
						break;			
					case 0x2A:	// SRA D
						REG_D = sra(REG_D);
						cycles = 8;
						break;
					case 0x2B:	// SRA E
						REG_E = sra(REG_E);
						cycles = 8;
						break;
					case 0x2C:	// SRA H
						REG_H = sra(REG_H);
						cycles = 8;
						break;
					case 0x2D:	// SRA L
						REG_L = sra(REG_L);
						cycles = 8;
						break;
					case 0x2E:	// SRA (HL)
						writeb(REG_HL, sra(readb(REG_HL)));
						cycles = 16;
						break;
					case 0x3F:	// SRL A
						REG_A = srl(REG_A);
						cycles = 8;
						break;
					case 0x38:	// SRL B
						REG_B = srl(REG_B);
						cycles = 8;
						break;
					case 0x39:	// SRL C
						REG_C = srl(REG_C);
						cycles = 8;
						break;			
					case 0x3A:	// SRL D
						REG_D = srl(REG_D);
						cycles = 8;
						break;
					case 0x3B:	// SRL E
						REG_E = srl(REG_E);
						cycles = 8;
						break;
					case 0x3C:	// SRL H
						REG_H = srl(REG_H);
						cycles = 8;
						break;
					case 0x3D:	// SRL L
						REG_L = srl(REG_L);
						cycles = 8;
						break;
					case 0x3E:	// SRL (HL)
						writeb(REG_HL, srl(readb(REG_HL)));
						cycles = 16;
						break;
					default:
						opcode = readb(REG_PC - 1);
						// Is this a BIT instruction?
						if ((opcode & 0xC0) == 0x40) {
							Byte b = (opcode & 0x38) >> 3;
							switch (opcode & 0x07) {
								case 0x00:	// BIT b, B
									bit(REG_B, b);
									cycles = 8;
									break;
								case 0x01:	// BIT b, C
									bit(REG_C, b);	
									cycles = 8;
									break;
								case 0x02:	// BIT b, D
									bit(REG_D, b);
									cycles = 8;
									break;
								case 0x03:	// BIT b, E
									bit(REG_E, b);
									cycles = 8;
									break;
								case 0x04:	// BIT b, H
									bit(REG_H, b);
									cycles = 8;
									break;
								case 0x05:	// BIT b, L
									bit(REG_L, b);
									cycles = 8;
									break;
								case 0x06:	// BIT b, (HL)
									bit(readb(REG_HL), b);
									cycles = 12;
									break;
								case 0x07:	// BIT b, A
									bit(REG_A, b);
									cycles = 8;
									break;
							}
						break;
						}
						// Is this a SET instruction?
						if ((opcode & 0xC0) == 0xC0) {
							Byte b = (opcode & 0x38) >> 3;
							switch (opcode & 0x07) {
								case 0x00:	// SET b, B
									REG_B = set(REG_B, b);
									cycles = 8;
									break;
								case 0x01:	// SET b, C
									REG_C = set(REG_C, b);
									cycles = 8;
									break;
								case 0x02:	// SET b, D
									REG_D = set(REG_D, b);
									cycles = 8;
									break;
								case 0x03:	// SET b, E
									REG_E = set(REG_E, b);
									cycles = 8;
									break;
								case 0x04:	// SET b, H
									REG_H = set(REG_H, b);
									cycles = 8;
									break;
								case 0x05:	// SET b, L
									REG_L = set(REG_L, b);
									cycles = 8;
									break;
								case 0x06:	// SET b, (HL)
									writeb(REG_HL, 
									           set(readb(REG_HL), b));
									cycles = 16;
									break;
								case 0x07:	// SET b, A
									REG_A = set(REG_A, b);
									cycles = 8;
									break;
							}
						break;
						}
						// Is this a RES instruction?
						if ((opcode & 0xC0) == 0x80) {
							Byte b = (opcode & 0x38) >> 3;
							switch (opcode & 0x07) {
								case 0x00:	// RES b, B
									REG_B = res(REG_B, b);
									cycles = 8;
									break;
								case 0x01:	// RES b, C
									REG_C = res(REG_C, b);
									cycles = 8;
									break;
								case 0x02:	// SET b, D
									REG_D = res(REG_D, b);
									cycles = 8;
									break;
								case 0x03:	// SET b, E
									REG_E = res(REG_E, b);
									cycles = 8;
									break;
								case 0x04:	// SET b, H
									REG_H = res(REG_H, b);
									cycles = 8;
									break;
								case 0x05:	// SET b, L
									REG_L = res(REG_L, b);
									cycles = 8;
									break;
								case 0x06:	// SET b, HL
									writeb(REG_HL, 
									           res(readb(REG_HL), b));
									cycles = 16;
									break;
								case 0x07:
									REG_A = res(REG_A, b);
									cycles = 8;
									break;
							}
						break;
						}
						printf("invalid opcode: cb%hhx ", readb(REG_PC - 1));
						printf("at %hx\n", REG_PC - 2);
						dump_state();
				}
				break;

			case 0x27:   // DAA
				REG_A = daa(REG_A);
				cycles = 4;
				break;
			case 0x2F:   // CPL
				REG_A = ~REG_A;
				FLAG_N = 1;
				FLAG_H = 1;
				cycles = 4;
				break;
			case 0x3F:   // CCF
				if (FLAG_C != 0)
					FLAG_C = 0;
				else
					FLAG_C = 1;
				FLAG_N = 0;
				FLAG_H = 0;
				cycles = 4;
				break;
			case 0x37:   // SCF
				FLAG_C = 1;
				FLAG_N = 0;
				FLAG_H = 0;
				cycles = 4;
				break;
			case 0x76:   // HALT
				core.is_halted = 1;
				cycles = 4;
				break;
			case 0x10:   // STOP
				++REG_PC;		/* skip over the 0x00 */
				/* has a speed switch been requested? */
				if ((read_io(HWREG_KEY1) & 0x01) && 
						(console_mode = MODE_GBC_ENABLED)) {
				fprintf(stderr, "speed switch");
					if (core.frequency == FREQ_NORMAL) {
						core.frequency = FREQ_DOUBLE;
						write_io(HWREG_KEY1, 0x80);
					}
					else if (core.frequency == FREQ_DOUBLE) {
						core.frequency = FREQ_NORMAL;
						write_io(HWREG_KEY1, 0x00);
					}
				} else
					core.is_stopped = 1;
				cycles = 4;
				break;
			case 0xF3:	// DI
				core.ime = 0;
				cycles = 4;
				break;
			case 0xFB:	// EI
				core.ei = 3;
				//ime_ = 1;
				cycles = 4;
				break;
			case 0x07:	// RLCA
				REG_A = rlc(REG_A);
				FLAG_Z = 0;
				cycles = 4;
				break;
			case 0x17:	// RLA
				REG_A = rl(REG_A);
				FLAG_Z = 0;
				cycles = 4;
				break;
			case 0x0F:	// RRCA
				REG_A = rrc(REG_A);
				FLAG_Z = 0;
				cycles = 4;
				break;
			case 0x1F:	// RRA
				REG_A = rr(REG_A);
				FLAG_Z = 0;
				cycles = 4;
				break;
			case 0xC3:   // JP imm
				REG_PC = readw(REG_PC);
				cycles = 16;
				break;
			case 0xC2: 	// JP NZ, nn
				if (FLAG_Z == 0) {
					REG_PC = readw(REG_PC);
					cycles = 16;
					break;
				}
				REG_PC += 2;
				cycles = 12;
				break;
			case 0xCA: 	// JP Z, nn
				if (FLAG_Z != 0) {
					REG_PC = readw(REG_PC);
					cycles = 16;
					break;
				}
				REG_PC += 2;
				cycles = 12;
				break;
			case 0xD2: 	// JP NC, nn
				if (FLAG_C == 0) {
					REG_PC = readw(REG_PC);
					cycles = 16;
					break;
				}
   				REG_PC += 2;
				cycles = 12;
				break;
			case 0xDA: 	// JP C, nn
				if (FLAG_C != 0) {
					REG_PC = readw(REG_PC);
					cycles = 16;
					break;
				}
   				REG_PC += 2;
				cycles = 12;
				break;
			case 0xE9:   // JP HL
				REG_PC = REG_HL;
				cycles = 4;
				break;
			case 0x18:   // JR n
				jr(readb(REG_PC));
                REG_PC += 1;
				cycles = 12;
				break;
			case 0x20:   // JR NZ, n
				if (FLAG_Z == 0) {
					jr(readb(REG_PC));
					++REG_PC;
					cycles = 12;
					break;
				}
				++REG_PC;
				cycles = 8;
				break;
			case 0x28:   // JR Z, n
				if (FLAG_Z != 0) {
					jr(readb(REG_PC));
					++REG_PC;
					cycles = 12;
					break;
				}
				++REG_PC;
				cycles = 8;
				break;
			case 0x30:   // JR NC, n
				if (FLAG_C == 0) {
					jr(readb(REG_PC));
					++REG_PC;
					cycles = 12;
					break;
				}
				++REG_PC;
				cycles = 8;
				break;
			case 0x38:   // JR C, n
				if (FLAG_C != 0) {
					jr(readb(REG_PC));
					++REG_PC;
					cycles = 12;
					break;
				}
				++REG_PC;
				cycles = 8;
				break;
			case 0xCD:	// CALL nn
				call(readw(REG_PC));
				cycles = 24;
				break;
			case 0xC4:	// CALL NZ, nn
				if (FLAG_Z == 0) {
					call(readw(REG_PC));
					cycles = 24;
					break;
				}
				cycles = 12;
				REG_PC += 2;
				break;
			case 0xCC:	// CALL Z, nn
				if (FLAG_Z != 0) {
					call(readw(REG_PC));
					cycles = 24;
					break;
				}
				cycles = 12;
				REG_PC += 2;
				break;
			case 0xD4:	// CALL NC, nn
				if (FLAG_C == 0) {
					call(readw(REG_PC));
					cycles = 24;
					break;
				}
				cycles = 12;
				REG_PC += 2;
				break;
			case 0xDC:	// CALL C, nn
				if (FLAG_C != 0) {
					call(readw(REG_PC));
					cycles = 24;
					break;
				}
				cycles = 12;
				REG_PC += 2;
				break;
			case 0xC7:	// RST 0x00
				rst(0x00);
				cycles = 16;
				break;
			case 0xCF:	// RST 0x08
				rst(0x08);
				cycles = 16;
				break;
			case 0xD7:	// RST 0x10
				rst(0x10);
				cycles = 16;
				break;
			case 0xDF:	// RST 0x18
				rst(0x18);
				cycles = 16;
				break;
			case 0xE7:	// RST 0x20
				rst(0x20);
				cycles = 16;
				break;
			case 0xEF:	// RST 0x28
				rst(0x28);
				cycles = 16;
				break;
			case 0xF7:	// RST 0x30
				rst(0x30);
				cycles = 16;
				break;
			case 0xFF:	// RST 0x38
				rst(0x38);
				cycles = 16;
				break;
			case 0xC9:	// RET
				ret();
				cycles = 16;
				break;
			case 0xC0:	// RET NZ
				if (FLAG_Z == 0) {
					ret();
					cycles = 20;
					break;
				}
				cycles = 8;
				break;
			case 0xC8:	// RET Z
				if (FLAG_Z != 0) {
					ret();
					cycles = 20;
					break;
				}
				cycles = 8;
				break;
			case 0xD0:	// RET NC
				if (FLAG_C == 0) {
					ret();
					cycles = 20;
					break;
				}
				cycles = 8;
				break;
			case 0xD8:	// RET C
				if (FLAG_C != 0) {
					ret();
					cycles = 20;
					break;
				}
				cycles = 8;
				break;
			case 0xD9:	// RETI
				ret();
				core.ime = 1;
				cycles = 16;
				break;
			case 0x00:  // NOP
				cycles = 4;
				break;
#if 0
			/* debugging instructions (ie. not on real gameboy) */
			case 0xD3:	// DUMP
				dumpState();
				break;
			case 0xDB:	// DMPB
				cout << hex << "DMPB: 0x" << readw(REG_PC) << ": 0x" 
				     << (unsigned int) readb(readw(REG_PC)) << dec << endl;
				REG_PC += 2;
				break;
			case 0xDD:	// DMPW
				cout << hex << "DMPW: 0x" << readw(REG_PC) << ": 0x"
				<< readw(readw(REG_PC)) << dec << endl;
				REG_PC += 2;
				break;
			case 0xE3:	// BRK
				exit(1);
#endif
			default:
				printf("invalid opcode: %hhx ", readb(REG_PC - 1));
				printf("at %hx\n", REG_PC - 1);
				dump_state();
				break;
		}

		// EI only enables ints after the next instruction.
		if (core.ei != 0) {
			--core.ei;
			if (core.ei == 1)
				core.ime = 1;
		}

		if (debugging)
			dump_state();
			
		total_cycles += cycles;
		sound_cycles += cycles;
		
	}

	return total_cycles;
}

void core_reset() {
	FLAG_Z = 1;
	FLAG_N = 0;
	FLAG_Z = 1;
	FLAG_C = 1;
	// TODO: Different AF values for other gameboys.
	// Set CPU registers to their default values

	REG_BC  = 0x0013;
	REG_DE  = 0x00D8;
	REG_HL  = 0x014D;
	REG_PC  = 0x0100;
	REG_SP  = 0xFFFE;

	/* different consoles have different initial values for A and B */
	switch(console) {
		case CONSOLE_DMG:
			REG_A = 0x01;
			break;
		case CONSOLE_POCKET:
			REG_A = 0xff;
			break;
		case CONSOLE_GBA:
			REG_B = 0x01;
			/* fall through */
		case CONSOLE_GBC:
			REG_A = 0x11;
			break;
		default:
			REG_A = 0x01;
			break;
	}

	core.is_halted = 0;
	core.is_stopped = 0;
	core.ime = 1;

	write_io(HWREG_P1,		0xFF);	// fairly arbitrary.
	write_io(HWREG_TIMA,	0x00);
	write_io(HWREG_TMA,		0x00);
	write_io(HWREG_TAC,		0x00);
/*
	write_io(HWREG_NR10,	0x80);
	write_io(HWREG_NR11,	0xBF);
	write_io(HWREG_NR12,	0xF3);
	write_io(HWREG_NR14,	0xBF);
	write_io(HWREG_NR21,	0x3F);
	write_io(HWREG_NR22,	0x00);
	write_io(HWREG_NR24,	0xBF);
	write_io(HWREG_NR30,	0x7F);
	write_io(HWREG_NR31,	0xFF);
	write_io(HWREG_NR32,	0x9F);
	write_io(HWREG_NR33,	0xBF);
	write_io(HWREG_NR41,	0xFF);
	write_io(HWREG_NR42,	0x00);
	write_io(HWREG_NR43,	0x00);
	write_io(HWREG_NR44,	0xBF);
	write_io(HWREG_NR50,	0x77);
	write_io(HWREG_NR51,	0xF3);
	write_io(HWREG_NR52,	0xF1); // TODO DIFFERENT FOR SGB
*/
	write_io(HWREG_LCDC,	0x91);
	write_io(HWREG_SCY, 	0x00);
	write_io(HWREG_SCX, 	0x00);
	write_io(HWREG_LYC, 	0x00);
	write_io(HWREG_BGP, 	0xFC);
	write_io(HWREG_OBP0,	0xFF);
	write_io(HWREG_OBP1,	0xFF);
	write_io(HWREG_WY, 		0x00);
	write_io(HWREG_WX, 		0x00);
	write_io(HWREG_IE, 		0x00);

	write_io(HWREG_KEY1, 	0x00);
	write_io(HWREG_SVBK, 	0x00);
	write_io(HWREG_HDMA5, 	0xff);
	
	core.frequency = FREQ_NORMAL;
}

void dump_state() {
	printf("\t\tregs: ");
	printf("A: %02x\t", (unsigned int) REG_A);
	printf("B: %02x\t", (unsigned int) REG_B);
	printf("C: %02x\t", (unsigned int) REG_C);
	printf("D: %02x\t", (unsigned int) REG_D);
	printf("E: %02x\t", (unsigned int) REG_E);
	printf("H: %02x\t", (unsigned int) REG_H);
	printf("L: %02x\t", (unsigned int) REG_L);
	printf("PC: %04x\t", (unsigned int) REG_PC);
	printf("SP: %04x\t", (unsigned int) REG_SP);
	
	printf("set flags: ");
	if (FLAG_Z != 0)
		printf("Z");
	else
		printf(" ");
	if (FLAG_N != 0)
		printf("N");
	else
		printf(" ");
	if (FLAG_H != 0)
		printf("H");
	else
		printf(" ");
	if (FLAG_C != 0)
		printf("C");
	else
		printf(" ");

	printf("\tIE: %04x\tIF: %04x\tIME:%d\n", (unsigned int)readb(HWREG_IE), (unsigned int) readb(HWREG_IF), core.ime);
	
	
	/*
	cout << "\tstack:\t";
	for (int i = 0; i < 10; i += 2) {
		cout << REG_SP + i << " : " << readw(REG_SP + i) << "\t";
	}
*/
//	cout << endl << dec;
}


static inline void handle_interrupts() {
	Byte reg_if, reg_ie;
	reg_if = readb(HWREG_IF);
	reg_ie = readb(HWREG_IE);
	if (reg_if == 0)
		return;
	// check for vblank interrupt, and check if it is enabled
	if ((reg_if & INT_VBLANK) && (reg_ie & INT_VBLANK)) {
		handle_interrupt(INT_VBLANK, MEM_INT_VBLANK, reg_if);
		return;
	}
	// check for stat interrupt, and check if it is enabled
	if ((reg_if & INT_STAT) && (reg_ie & INT_STAT)) {
		handle_interrupt(INT_STAT, MEM_INT_STAT, reg_if);
		return;
	}
	// check for timer interrupt, and check if it is enabled
	if ((reg_if & INT_TIMER) && (reg_ie & INT_TIMER)) {
		handle_interrupt(INT_TIMER, MEM_INT_TIMER, reg_if);
		return;
	}
	// check for serial interrupt, and check if it is enabled
	if ((reg_if & INT_SERIAL)  && (reg_ie & INT_SERIAL)) {
		handle_interrupt(INT_SERIAL, MEM_INT_SERIAL, reg_if);
		return;
	}
	// check for button interrupt, and check if it is enabled
	if ((reg_if & INT_BUTTON) && (reg_ie & INT_BUTTON)) {
		handle_interrupt(INT_BUTTON, MEM_INT_BUTTON, reg_if);
		return;
	}
}

static inline void handle_interrupt(Byte interrupt, Word Vector, Byte reg_if) {
	// handle only if interrupts are actually enabled
	if (core.ime != 0) {
		writeb(HWREG_IF, reg_if & ~(interrupt));
		core.ime = 0;
		push(REG_PC);
		REG_PC = Vector;
	}
	// core is unhalted regardless of whether interrupts are enabled
	core.is_halted = 0;
}

// ADD
static inline Byte add_bbb(Byte a, Byte b) {
	Byte temp = a + b;
	// will it overflow? If so, set carry flag.
	if (0xFF - a < b)
		FLAG_C = 1;
	else
		FLAG_C = 0;
	// will the lower nibble overflow? If so, set half carry flag.
	if (0x0F - (a & 0x0F) < (b & 0x0F))
		FLAG_H = 1;
	else
		FLAG_H = 0;
	// if applicable, set zero flag. This is only changed in 8bit adds.
	if (temp == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	// set subtract flag to 0.
	FLAG_N = 0;
	return temp;
}

static inline Word add_www(Word a, Word b) {
	Word temp = a + b;
	// will it overflow? If so, set carry flag.
	if (0xFFFF - a < b)
		FLAG_C = 1;
	else
		FLAG_C = 0;
	// will the lower nibble overflow? If so, set half carry flag.
	if (0x0FFF - (a & 0x0FFF) < (b & 0x0FFF))
		FLAG_H = 1;
	else
		FLAG_H = 0;
	// set subtract flag to 0.
	FLAG_N = 0;
	return temp;
}

static inline Word add_wwb(Word a, Byte b) {
	// we must get the C compiler to sign extend b.
	SWord bsx = (SWord)((SByte)b);
	Word temp = a + (Word)bsx;
	// will it overflow? If so, set carry flag.
	if (0xFF - (a & 0x00ff) < b)
		FLAG_C = 1;
	else
		FLAG_C = 0;
	// will the lower nibble overflow? If so, set half carry flag.
	if (0x0F - (a & 0x0F) < (b & 0x0F))
		FLAG_H = 1;
	else
		FLAG_H = 0;
	// set subtract flag to 0.
	FLAG_N = 0;
	FLAG_Z = 0;
	return temp;
}

/* for this, we check for carry and half carry after each of the two add
 * operations
 */
static inline Byte adc(Byte a, Byte b) {
	Byte temp, temp2;
	unsigned carry_set = 0;
	unsigned half_set = 0;
	
	temp = a + FLAG_C;

	// will the lower nibble overflow? If so, set half carry flag.
	if (0x0F - (a & 0x0F) < (FLAG_C & 0x0F))
		half_set = 1;
	// will it overflow? If so, set carry flag.
	if (0xFF - a < FLAG_C)
		carry_set = 1;

	temp2 = temp + b;
	// will the lower nibble overflow? If so, set half carry flag.
	if (0x0F - (temp & 0x0F) < (b & 0x0F))
		half_set = 1;
	// will it overflow? If so, set carry flag.
	if (0xFF - temp < b)
		carry_set = 1;	

	if (half_set)
		FLAG_H = 1;
	else
		FLAG_H = 0;
		
	if (carry_set)
		FLAG_C = 1;
	else
		FLAG_C = 0;
		
	// if applicable, set zero flag.
	if (temp2 == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	// set subtract flag to 0.
	FLAG_N = 0;
	return temp2;
}


static inline Byte sub(Byte a, Byte b) {
	Byte temp;
	// will there be a borrow? if so, no carry
	if (a < b)
		FLAG_C = 1;
	else
		FLAG_C = 0;
	// will the lower nibble borrow? if so, no carry
	if ((a & 0x0F) < (b & 0x0F))
		FLAG_H = 1;
	else
		FLAG_H = 0;
	temp = a - b;
	// if applicable, set zero flag.
	if (temp == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	// set subtract flag to 1.
	FLAG_N = 1;
	return temp;
}

static inline Byte sbc(Byte a, Byte b) {
	Byte temp, temp2;
	unsigned carry_set = 0;
	unsigned half_set = 0;
	
	temp = a - FLAG_C;
	
	// will there be a borrow? if so, no carry
	if (a < FLAG_C)
		carry_set = 1;
	// will the lower nibble borrow? if so, no carry
	if ((a & 0x0F) < (FLAG_C & 0x0F))
		half_set = 1;

	temp2 = temp - b;
	
	// will there be a borrow? if so, no carry
	if (temp < b)
		carry_set = 1;
	// will the lower nibble borrow? if so, no carry
	if ((temp & 0x0F) < (b & 0x0F))
		half_set = 1;

	if (half_set)
		FLAG_H = 1;
	else
		FLAG_H = 0;
		
	if (carry_set)
		FLAG_C = 1;
	else
		FLAG_C = 0;

	// if applicable, set zero flag.
	if (temp2 == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	// set subtract flag to 1.
	FLAG_N = 1;
	return temp2;
}

static inline Byte inc_bb(Byte a) {
	++a;
	// has the lower nibble overflowed? If so, set half carry flag.
	if ((a & 0x0F) == 0)
		FLAG_H = 1;
	else
		FLAG_H = 0;
	// if applicable, set zero flag.
	if (a == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	// set subtract flag to 0.
	FLAG_N = 0;
	return a;
}


static inline Word inc_ww(Word a) {
	return ++a;
}


static inline Byte dec_bb(Byte a) {
	--a;
	// has the lower nibble borrowed? if so, no carry
	if ((a & 0x0F) == 0x0F)
		FLAG_H = 1;
	else
		FLAG_H = 0;
	// if applicable, set zero flag.
	if (a == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	// set subtract flag to 1.
	FLAG_N = 1;
	return a;
}


static inline Word dec_ww(Word a) {
	return --a;
}

// binary operations.

static inline Byte and(Byte a, Byte b) {
	Byte temp = a & b;
	FLAG_N = 0;
	FLAG_H = 1;
	FLAG_C = 0;
	if (temp == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	return temp;	
}

static inline Byte or(Byte a, Byte b) {
	Byte temp = a | b;
	FLAG_N = 0;
	FLAG_H = 0;
	FLAG_C = 0;
	if (temp == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	return temp;
}


static inline Byte xor(Byte a, Byte b) {
	Byte temp = a ^ b;
	FLAG_N = 0;
	FLAG_H = 0;
	FLAG_C = 0;
	if (temp == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	return temp;
}

static inline Byte swap(Byte a) {
		Byte temp;
		temp = a & 0x0F;
		a >>= 4;
		a |= (temp << 4);

		if (a == 0)
			FLAG_Z = 1;
		else
			FLAG_Z = 0;
		FLAG_N = 0;
		FLAG_H = 0;
		FLAG_C = 0;

		// a = ((a & 0xF0) ^ (a & 0x0F)) & 0xF0;
		// a = ((a & 0xF0) ^ (a & 0x0F)) & 0x0F;
		// a = ((a & 0xF0) ^ (a & 0x0F)) & 0xF0;
		return a;
}

static inline void push(Word a) {
	REG_SP -= 2;
	writew(REG_SP, a);
	return;
}

static inline Word pop() {
	REG_SP += 2;
	return readw(REG_SP - 2);
}

static inline Byte daa(Byte a) {
	unsigned int temp = a;
	if (!FLAG_N) {
		if (FLAG_H || ((temp & 0x0f) > 9))
			temp += 6;
		if (FLAG_C || (temp > 0x9f))
			temp += 0x60;
	} else {
		if (FLAG_H)
			temp = (temp - 6) & 0xff;
		if (FLAG_C)
			temp -= 0x60;
	}

	if (temp & 0x100)
		FLAG_C = 1;

	FLAG_H = 0;

	temp &= 0xff;

	if (temp == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	
	return temp;
}


static inline Byte rlc(Byte a) {
	FLAG_C = (a & 0x80) >> 7;
	a = (a << 1) + FLAG_C;
	if (a == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	FLAG_N = 0;
	FLAG_H = 0;
	return a;
}

static inline Byte rl(Byte a) {
	int temp = FLAG_C;
	FLAG_C = (a & 0x80) >> 7;
	a = (a << 1) + temp;
	if (a == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	FLAG_N = 0;
	FLAG_H = 0;
	return a;
}

static inline Byte rrc(Byte a) {
	FLAG_C = a & 0x01;
	a = (a >> 1) + (FLAG_C << 7);
	if (a == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	FLAG_N = 0;
	FLAG_H = 0;
	return a;
}

static inline Byte rr(Byte a) {
	int temp = FLAG_C;
	FLAG_C = a & 0x01;
	a = (a >> 1) + (temp << 7);
	if (a == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	FLAG_N = 0;
	FLAG_H = 0;
	return a;
}

static inline Byte sla(Byte a) {
	FLAG_C = (a & 0x80) >> 7;
	a <<= 1;
	if (a == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	FLAG_N = 0;
	FLAG_H = 0;
	return a;
}

static inline Byte sra(Byte a) {
	FLAG_C = a & 0x01;
	a >>= 1;
	// We must preserve bit 7 in this instruction	
	a |= ((a & 0x40) << 1);
	if (a == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	FLAG_N = 0;
	FLAG_H = 0;
	return a;
}

static inline Byte srl(Byte a) {
	FLAG_C = a & 0x01;
	a >>= 1;
	if (a == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	FLAG_N = 0;
	FLAG_H = 0;
	return a;
}

static inline void bit(Byte a, Byte b) {
	if ((a & (0x01 << b)) == 0)
		FLAG_Z = 1;
	else
		FLAG_Z = 0;
	// FLAG_Z = (~(a & (0x01 << b))) & 0x01;
	FLAG_N = 0;
	FLAG_H = 1;
}

static inline Byte set(Byte a, Byte b) {
	return a |= (1 << b);
}

static inline Byte res(Byte a, Byte b) {
	return a &= ~(1 << b);
}

static inline void jr(Byte a) {
	// is a negative?
	if ((a & 0x80) == 0x80) {
		// remove two's complement
		--a; a = ~a;
		REG_PC -= a;
	} else {
		REG_PC += a;
	}
}

static inline void call(Word a) {
	push(REG_PC + 2);
	REG_PC = readw(REG_PC);
}

static inline void rst(Byte a) {
	push(REG_PC);
	REG_PC = 0x0000 + a;
}

static inline void ret() {
	REG_PC = pop();
}

void core_save() {
	save_byte("reg_a", core.reg_af.b.h);
	save_byte("reg_f", core.reg_af.b.l);
	save_byte("reg_b", core.reg_bc.b.h);
	save_byte("reg_c", core.reg_bc.b.l);
	save_byte("reg_d", core.reg_de.b.h);
	save_byte("reg_e", core.reg_de.b.l);
	save_byte("reg_h", core.reg_hl.b.h);
	save_byte("reg_l", core.reg_hl.b.l);
	save_word("reg_sp", core.reg_sp);
	save_word("reg_pc", core.reg_pc);
	save_int("flag_z", core.flag_z);
	save_int("flag_n", core.flag_n);
	save_int("flag_h", core.flag_h);
	save_int("flag_c", core.flag_c);
	save_int("ei", core.ei);
	save_int("is_halted", core.is_halted);
	save_int("is_stopped", core.is_stopped);
	save_int("ime", core.ime);
	save_uint("frequency", core.frequency);
	
	save_int("console", console);
	save_int("console_mode", console_mode);
}

void core_load() {
	core.reg_af.b.h = load_byte("reg_a");
	core.reg_af.b.l = load_byte("reg_f");
	core.reg_bc.b.h = load_byte("reg_b");
	core.reg_bc.b.l = load_byte("reg_c");
	core.reg_de.b.h = load_byte("reg_d");
	core.reg_de.b.l = load_byte("reg_e");
	core.reg_hl.b.h = load_byte("reg_h");
	core.reg_hl.b.l = load_byte("reg_l");
	core.reg_sp = load_word("reg_sp");
	core.reg_pc = load_word("reg_pc");
	core.flag_z = load_int("flag_z");
	core.flag_n = load_int("flag_n");
	core.flag_h = load_int("flag_h");
	core.flag_c = load_int("flag_c");
	core.ei = load_int("ei");
	core.is_halted = load_int("is_halted");
	core.is_stopped = load_int("is_stopped");
	core.ime = load_int("ime");
	core.frequency = load_uint("frequency");
	
	console = load_int("console");
	console_mode = load_int("console_mode");
}


