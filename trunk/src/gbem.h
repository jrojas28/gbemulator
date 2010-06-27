/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of jonny nor the name of any other
 *    contributor may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY jonny AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL jonny OR ANY OTHER
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 
#include <stdint.h>
#include "config.h"

#ifndef _GBEM_H
#define _GBEM_H

typedef uint8_t Byte;
typedef uint16_t Word;
typedef int8_t SByte;
typedef int16_t SWord;

typedef uint16_t GbAddr;


union UWord {
	Word w;
	struct {
#ifdef WORDS_BIGENDIAN
		Byte h, l;
#else
		Byte l, h;
#endif
	} b;
};


#define MEM_ROM_BANK_0				0x0000
#define MEM_ROM_BANK_SW				0x4000
#define MEM_VIDEO					0x8000
#define MEM_RAM_BANK_SW				0xA000
#define MEM_INTERNAL_0				0xC000
#define MEM_INTERNAL_SW				0xD000
#define MEM_INTERNAL_ECHO			0xE000
#define MEM_OAM						0xFE00
#define MEM_EMPTY_UNUSABLE_0		0xFEA0
#define MEM_IO						0xFF00
#define MEM_EMPTY_UNUSABLE_1		0xFF4C
#define MEM_INTERNAL_1				0xFF80

#define SIZE_ROM_BANK_0				0x4000
#define SIZE_ROM_BANK_SW			0x4000
#define SIZE_VIDEO					0x2000
#define SIZE_RAM_BANK_SW			0x2000
#define SIZE_INTERNAL_0				0x2000
#define SIZE_INTERNAL_SW			0x1000
#define SIZE_INTERNAL_ECHO			0x1E00
#define SIZE_OAM					0x00A0
#define SIZE_EMPTY_UNUSABLE_0		0x0060
#define SIZE_IO						0x004C
#define SIZE_EMPTY_UNUSABLE_1		0x0034
#define SIZE_INTERNAL_1				0x0080

#define HWREG_P1				0xFF00
#define HWREG_SB				0xFF01
#define HWREG_SC				0xFF02
#define HWREG_DIV				0xFF04
#define HWREG_TIMA				0xFF05
#define HWREG_TMA				0xFF06
#define HWREG_TAC				0xFF07
#define HWREG_IF				0xFF0F
#define HWREG_NR10				0xFF10
#define HWREG_NR11				0xFF11
#define HWREG_NR12				0xFF12
#define HWREG_NR13				0xFF13
#define HWREG_NR14				0xFF14
#define HWREG_NR21				0xFF16
#define HWREG_NR22				0xFF17
#define HWREG_NR23				0xFF18
#define HWREG_NR24				0xFF19
#define HWREG_NR30				0xFF1A
#define HWREG_NR31				0xFF1B
#define HWREG_NR32				0xFF1C
#define HWREG_NR33				0xFF1D
#define HWREG_NR34				0xFF1E
#define HWREG_NR41				0xFF20
#define HWREG_NR42				0xFF21
#define HWREG_NR43				0xFF22
#define HWREG_NR44				0xFF23
#define HWREG_NR50				0xFF24
#define HWREG_NR51				0xFF25
#define HWREG_NR52				0xFF26
#define HWREG_LCDC				0xFF40
#define HWREG_STAT				0xFF41
#define HWREG_SCY				0xFF42
#define HWREG_SCX				0xFF43
#define HWREG_LY				0xFF44
#define HWREG_LYC				0xFF45
#define HWREG_DMA				0xFF46
#define HWREG_BGP				0xFF47
#define HWREG_OBP0				0xFF48
#define HWREG_OBP1				0xFF49
#define HWREG_WY				0xFF4A
#define HWREG_WX				0xFF4B
#define HWREG_KEY1				0xFF4D
#define HWREG_RP				0xFF56
#define HWREG_SVBK				0xFF70
#define HWREG_IE				0xFFFF

#define MEM_INT_VBLANK				0x0040
#define MEM_INT_STAT				0x0048
#define MEM_INT_TIMER				0x0050
#define MEM_INT_SERIAL				0x0058
#define MEM_INT_BUTTON				0x0060

enum Console {DMG, POCKET, GBC, SGB, GBA, AUTO};
enum ConsoleMode {NORMAL, DMG_EMU, GBC_ENABLED};

#endif /* _GBEM_H */
