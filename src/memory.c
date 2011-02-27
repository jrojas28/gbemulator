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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "core.h"
#include "cart.h"
#include "display.h"
#include "joypad.h"
#include "sound.h"
#include "save.h"

#define ADDRESS_SPACE		0x10000
#define VT_ENTRIES 			(ADDRESS_SPACE / VT_GRANULARITY)
#define VT_SIZE 			(VT_ENTRIES * sizeof(Byte*))
#define SIZE_HIMEM 			(SIZE_IO + SIZE_INTERNAL_1)

Byte *internal0 = NULL;
Byte** vector_table = NULL;
Byte* himem = NULL;

unsigned int iram_bank = 1;

extern int console;
extern int console_mode;

unsigned mem_map[256];

void memory_init(void) {
	himem = malloc (sizeof(Byte) * SIZE_HIMEM);
	vector_table = malloc(sizeof(Byte*) * VT_ENTRIES);
}

void memory_reset(void) {
	if (internal0 != NULL)
		free(internal0);

	internal0 = malloc(sizeof(Byte) * IMEM_SIZE_GBC);
	memset(internal0, 0, IMEM_SIZE_GBC);

	memset(vector_table, 0, VT_SIZE);
	memset(himem, 0, SIZE_HIMEM);

	iram_bank = 1;

	set_vector_block(MEM_INTERNAL_0, internal0, SIZE_INTERNAL_0);
	set_vector_block(MEM_INTERNAL_SW, internal0 + (iram_bank * 0x1000), SIZE_INTERNAL_SW);
	set_vector_block(MEM_INTERNAL_ECHO, internal0, SIZE_INTERNAL_0);
	set_vector_block(MEM_INTERNAL_ECHO + SIZE_INTERNAL_0, internal0 + (iram_bank * 0x1000), SIZE_INTERNAL_ECHO - SIZE_INTERNAL_0);
	set_vector_block(MEM_IO, himem, SIZE_HIMEM);
}

void memory_fini(void) {
	free(internal0);
	free(himem);
	free(vector_table);
}

void writeb(Word address, Byte value) {
	// cartridge rom area
	if (address < MEM_VIDEO) {
		write_rom(address, value);
	}
	// video ram area
	else if (address < MEM_VIDEO + SIZE_VIDEO) {
		write_vram(address, value);
		return;
	}
	// cartridge ram area
	else if (address < MEM_RAM_BANK_SW + SIZE_RAM_BANK_SW) {
		write_cart_ram(address - MEM_RAM_BANK_SW, value);
		return;
	}
	// internal ram area 0
	else if (address < MEM_INTERNAL_0 + SIZE_INTERNAL_0) {
		internal0[address - MEM_INTERNAL_0] = value;
		return;
	}
	// internal ram area switchable
	else if (address < MEM_INTERNAL_SW + SIZE_INTERNAL_SW) {
		internal0[(iram_bank * 0x1000) + address - MEM_INTERNAL_SW] = value;
		return;
	}	
	// echo of internal ram area 0
	else if (address < MEM_INTERNAL_ECHO + SIZE_INTERNAL_ECHO) {
		fprintf(stderr, "ECHO %hx\n", address);
		if (address < MEM_INTERNAL_ECHO + SIZE_INTERNAL_0)
			internal0[address - MEM_INTERNAL_ECHO] = value;
		else
			internal0[(iram_bank * 0x1000) + address - MEM_INTERNAL_ECHO - SIZE_INTERNAL_0] = value;
		return;
	}
	// sprite attrib (oam) ram
	else if (address < MEM_OAM + SIZE_OAM) {
		write_oam(address, value);
		return;
	}
	// unusable memory
	else if (address < MEM_IO) {
		//printf("Bad memory write to unusable location (%hx)!\n", address);
		return;
	}
	// i/o memory
	else if (address < MEM_IO + SIZE_IO) {
	if (address == HWREG_KEY1)
		fprintf(stderr, "KEY1: VALUE: %hhx\n", value);
		/* special writes here */
		switch (address) {
			case HWREG_STAT:
			/* the bottom 3 bits of STAT are read only.	*/
				himem[address - MEM_IO] = (himem[address - MEM_IO] & 0x07) 
                	| (value & 0xF8);
				return;
				break;
			case HWREG_LCDC:
				set_lcdc(value);
				break;
			case HWREG_KEY1:
				/* the top bit of KEY1 is read only */
				himem[address - MEM_IO] = (himem[address - MEM_IO] & 0x80) | (value & 0x7f);
				return;
                break;
			case HWREG_NR52:
				himem[address - MEM_IO] = (himem[address - MEM_IO] & 0x0f) | (value & 0x80);
				break;
			default:
				himem[address - MEM_IO] = value;
				break;
		}
		/* sound registers are dealt with in the sound code */
		if ((address >= 0xff10) && (address < 0xff30)) {
			write_sound(address, value);
			return;
		}
		/* sound wave data is dealt with in the sound code */
		if ((address >= 0xff30) && (address  <= 0xff3f)) {
			write_wave(address, value);
			return;
		}

		switch(address) {
			case HWREG_DIV:
				// If DIV is written to, it is set to 0.
				himem[address - MEM_IO] = 0;
				break;
			case HWREG_BGP:
				if (console_mode != MODE_GBC_ENABLED) 
					update_bg_palette(0, value);
				break;
			case HWREG_OBP0:
				if (console_mode != MODE_GBC_ENABLED)
					update_sprite_palette(0, value);
				break;
			case HWREG_OBP1:
				if (console_mode != MODE_GBC_ENABLED)
					update_sprite_palette(1, value);
				break;
			case HWREG_DMA:
				launch_dma(value);
				break;
			case HWREG_P1:
				update_p1();
				break;
			case HWREG_LY:
			case HWREG_LYC:
				write_io(HWREG_STAT, check_coincidence(read_io(HWREG_LY), read_io(HWREG_STAT)));
				break;
			case HWREG_SC:
				/* no other gameboy is connected: 'receive' 0xff */
				if ((value & 0x80) && (value & 0x01)) {
					write_io(HWREG_SB, 0xff);
					write_io(HWREG_SC, value & (~0x80));
					raise_int(INT_SERIAL);
				}
				break;
			case HWREG_SVBK:
				/* adjust internal ram bank in gameboy color mode */
				if (console_mode == MODE_GBC_ENABLED) {
					iram_bank = value & 0x07;
					if (iram_bank == 0)
						iram_bank = 1;
					set_vector_block(MEM_INTERNAL_SW, internal0 + (iram_bank * 0x1000), SIZE_INTERNAL_SW);
					set_vector_block(MEM_INTERNAL_ECHO + SIZE_INTERNAL_0, internal0 + (iram_bank * 0x1000), SIZE_INTERNAL_ECHO - SIZE_INTERNAL_0);
				}
				break;
			case HWREG_VBK:
				/* adjust vram bank */
				if (console_mode == MODE_GBC_ENABLED)
					set_vram_bank(value & 0x01);
				break;
			case HWREG_HDMA5:
				/* initiate gbc hdma */
				if (console_mode == MODE_GBC_ENABLED)
					start_hdma(value);
				break;
			case HWREG_BGPD:
				update_gbc_bg_palette(value);
				break;
			case HWREG_OBPD:
				update_gbc_spr_palette(value);
				break;

		}
		return;
	}
	// internal ram area 1
	else {
		himem[address - MEM_IO] = value;
		return;
	}
}

void memory_save(void) {
	if ((console = CONSOLE_GBC) || (console = CONSOLE_GBA))
		save_memory("iram", internal0, IMEM_SIZE_GBC);
	else
		save_memory("iram", internal0, IMEM_SIZE_DMG);

	save_memory("himem", himem, SIZE_HIMEM);
	save_uint("iram_bank", iram_bank);
}

void memory_load(void) {
	memory_reset();
	
	if ((console = CONSOLE_GBC) || (console = CONSOLE_GBA))
		load_memory("iram", internal0, IMEM_SIZE_GBC);
	else
		load_memory("iram", internal0, IMEM_SIZE_DMG);

	load_memory("himem", himem, SIZE_HIMEM);
	iram_bank = load_uint("iram_bank");


	set_vector_block(MEM_INTERNAL_0, internal0, SIZE_INTERNAL_0);
	set_vector_block(MEM_INTERNAL_SW, internal0 + (iram_bank * 0x1000), SIZE_INTERNAL_SW);
	set_vector_block(MEM_INTERNAL_ECHO, internal0, SIZE_INTERNAL_0);
	set_vector_block(MEM_INTERNAL_ECHO + SIZE_INTERNAL_0, internal0 + (iram_bank * 0x1000), SIZE_INTERNAL_ECHO - SIZE_INTERNAL_0);
	set_vector_block(MEM_IO, himem, SIZE_HIMEM);


}


