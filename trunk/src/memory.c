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
#include "cart.h"
#include "display.h"
#include "joypad.h"
#include "sound.h"
#include "save.h"

#define ADDRESS_SPACE		0x10000
#define VT_ENTRIES 			(ADDRESS_SPACE / VT_GRANULARITY)
#define VT_SIZE 			(VT_ENTRIES * sizeof(Byte*))
#define SIZE_HIMEM 			(SIZE_IO + SIZE_INTERNAL_1 + SIZE_EMPTY_UNUSABLE_1)

Byte *internal0 = NULL;
Byte** vector_table = NULL;
Byte* himem = NULL;

unsigned mem_map[256];

void memory_init() {
	internal0 = malloc(sizeof(Byte) * SIZE_INTERNAL_0);
	himem = malloc (sizeof(Byte) * SIZE_HIMEM);
	vector_table = malloc(sizeof(Byte*) * VT_ENTRIES);
}

void memory_reset() {
	memset(vector_table, 0, VT_SIZE);
	memset(internal0, 0, SIZE_INTERNAL_0);
	memset(himem, 0, SIZE_HIMEM);

	set_vector_block(MEM_INTERNAL_0, internal0, SIZE_INTERNAL_0);
	set_vector_block(MEM_INTERNAL_ECHO, internal0, SIZE_INTERNAL_ECHO);
	set_vector_block(MEM_IO, himem, SIZE_HIMEM);
}

void memory_fini() {
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
		write_video_ram(address, value);
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
	// echo of internal ram area 0
	else if (address < MEM_INTERNAL_ECHO + SIZE_INTERNAL_ECHO) {
		internal0[address - MEM_INTERNAL_ECHO] = value;
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
		switch (address) {
        /* the bottom 3 bits of STAT are read only.	*/
			case HWREG_STAT:
				himem[address - MEM_IO] = (himem[address - MEM_IO] & 0x07) 
                	| (value & 0xF8);
				return;
				break;
		/* the top bit of KEY1 is read only */
			case HWREG_KEY1:
				himem[address - MEM_IO] = (himem[address - MEM_IO] & 0x80) | (value & 0x7f);
				return;
                break;
		}

		himem[address - MEM_IO] = value;
		if ((address >= 0xff10) && (address < 0xff30)) {
			write_sound(address, value);
			return;
		}
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
				update_bg_palette();
				break;
			case HWREG_OBP0:
				update_sprite_palette_0();
				break;
			case HWREG_OBP1:
				update_sprite_palette_1();
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
		}
		return;

	}

	// unusable memory
	else if (address < MEM_INTERNAL_1) {
		//printf("Bad memory write to unusable location (%hx)!\n", address);
		return;
	}
	// internal ram area 1
	else {
		himem[address - MEM_IO] = value;
		return;
	}
}

void memory_save() {
	save_memory("iram", internal0, SIZE_INTERNAL_0);
	save_memory("himem", himem, SIZE_HIMEM);
}

void memory_load() {
	load_memory("iram", internal0, SIZE_INTERNAL_0);
	load_memory("himem", himem, SIZE_HIMEM);

	set_vector_block(MEM_INTERNAL_0, internal0, SIZE_INTERNAL_0);
	set_vector_block(MEM_INTERNAL_ECHO, internal0, SIZE_INTERNAL_ECHO);
	set_vector_block(MEM_IO, himem, SIZE_HIMEM);
}


