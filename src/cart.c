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
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include "cart.h"
#include "memory.h"
#include "save.h"

static void set_switchable_rom(void);
static void set_switchable_ram(void);
static void save_sram(void);
static int find_sram_file(void);
static void load_sram(void);

static const char ram_ext[] = ".sav";

Cart cart;

static const Byte sg_data[] ="\xce\xed\x66\x66\xcc\x0d\x00\x0b\x03\x73\x00\x83"
                             "\x00\x0c\x00\x0d\x00\x08\x11\x1f\x88\x89\x00\x0e"
                             "\xdc\xcc\x6e\xe6\xdd\xdd\xd9\x99\xbb\xbb\x67\x63"
                             "\x6e\x0e\xec\xcc\xdd\xdc\x99\x9f\xbb\xb9\x33\x3e";

extern int console;
extern int console_mode;

int load_rom(const char* fn) {
	int i;
	size_t c;
	int title_len;
	struct stat fstats;
	FILE *rom_file;
	// check that rom is not already loaded
	assert(cart.is_loaded == 0);
	
	// get rom file size
	if (stat(fn, &fstats) == 0)
		cart.rom_size = fstats.st_size;
	else {
		fprintf(stderr, "rom loading failed.\n");
		perror("stat");
		return -1;
	}

	// no roms will be smaller than 32kB. 
	if (cart.rom_size < 32 * 1024) {
		fprintf(stderr, "rom error: roms cannot be smaller than 32kB\n");
		return -1;
	}
	
	// no roms will be larger than 1536kB (12Mbit)
	if (cart.rom_size > 1536 * 1024) {
		//printf("rom error: roms cannot be larger than 12Mbit\n");
		//return -1;
	}

	// load rom into memory
	cart.rom = malloc(cart.rom_size);
	rom_file = fopen(fn, "rb");
	if (rom_file == NULL) {
		fprintf(stderr, "rom loading failed.\n");
		perror("fopen");
		return -1;
	}
	
	c = fread(cart.rom, 1, cart.rom_size, rom_file);
	if (c != cart.rom_size) {
		fprintf(stderr, "file read error: %zu  of %u bytes read\n", c, cart.rom_size);
		perror("fread");
		return -1;
	}
	
	fclose(rom_file);
	// test if rom is valid, by checking the scrolling graphic data
	for (i = 0; i < CART_ROM_TITLE - CART_SG_DATA; ++i) {
		if (cart.rom[CART_SG_DATA + i] != sg_data[i]) {
			fprintf(stderr, "invalid rom: scrolling graphic mismatch\n");
			free(cart.rom);
			return -1;
		}
	}
	// check and get rom title
	printf("rom %s loaded (%u bytes)\n", fn, cart.rom_size);
	title_len = strlen((char *)cart.rom + CART_ROM_TITLE);
	if (title_len > 17) {
		fprintf(stderr, "invalid rom: title too long (>16 characters)\n");
		//free(cart.rom);
		//return -1;
	}
	cart.rom_title = malloc (title_len + 1);
	strncpy(cart.rom_title, (char *)cart.rom + CART_ROM_TITLE, title_len);
	cart.rom_title[title_len] = '\0';
	printf("\ttitle: %s", cart.rom_title);

	/* detect colour gameboy cartridge */
	switch (cart.rom[CART_COLOR]) {
		case 0x80:
			if (console == CONSOLE_AUTO)
				console = CONSOLE_GBC;
			if ((console == CONSOLE_GBC) || (console == CONSOLE_GBA))
				console_mode = MODE_GBC_ENABLED;
			break;
		case 0xC0:
			if (console == CONSOLE_AUTO)
				console = CONSOLE_GBC;
			if ((console == CONSOLE_GBC) || (console == CONSOLE_GBA))
				console_mode = MODE_GBC_ENABLED;
			break;
		default:
			if (console == CONSOLE_AUTO)
				console = CONSOLE_DMG;
			if ((console == CONSOLE_GBC) || (console == CONSOLE_GBA))
				console_mode = MODE_DMG_EMU;
			break;
	}
	
	// TODO: check licensee codes?
	if (cart.rom[CART_SGB_FUNC] == 0x03)
		cart.is_for_sgb = 1;
	else
		cart.is_for_sgb = 0;
	
	// read cartridge info
	cart.has_ram = 0; cart.has_batt = 0; cart.has_sram = 0; cart.has_rumble = 0; 
	cart.has_timer = 0; cart.has_mmm01 = 0;
	switch (cart.rom[CART_INFO]) {
		case 0x00:
			cart.mbc = 0;
			break;
		case 0x01:
			cart.mbc = 1;
			break;
		case 0x02:
			cart.mbc = 1; cart.has_ram = 1;
			break;
		case 0x03:
			cart.mbc = 1; cart.has_ram = 1; cart.has_batt = 1;
			break;
		case 0x05:
			cart.mbc = 2;
			break;
		case 0x06:
			cart.mbc = 2; cart.has_batt = 1;
			break;
		case 0x08:
			cart.mbc = 0; cart.has_ram = 1;
			break;
		case 0x09:
			cart.mbc = 0; cart.has_ram = 1; cart.has_batt = 1;
			break;
		case 0x0B:
			cart.has_mmm01 = 1;
			break;
		case 0x0C:
			cart.has_mmm01 = 1; cart.has_sram = 1;
			break;
		case 0x0D:
			cart.has_mmm01 = 1; cart.has_sram = 1; cart.has_batt = 1;
			break;
		case 0x0F:
			cart.mbc = 3; cart.has_batt = 1; cart.has_timer = 1;
			break;
		case 0x10:
			cart.mbc = 3; cart.has_ram = 1; cart.has_batt = 1; 
			cart.has_timer = 1;
			break;
		case 0x11:
			cart.mbc = 3;
			break;
		case 0x12:
			cart.mbc = 3; cart.has_ram = 1;
			break;
		case 0x13:
			cart.mbc = 3; cart.has_ram = 1; cart.has_batt = 1;
			break;
		case 0x19:
			cart.mbc = 5;
			break;
		case 0x1A:
			cart.mbc = 5; cart.has_ram = 1;
			break;
		case 0x1B:
			cart.mbc = 5; cart.has_ram = 1; cart.has_batt = 1;
			break;
		case 0x1C:
			cart.mbc = 5; cart.has_rumble = 1;
			break;
		case 0x1D:
			cart.mbc = 5; cart.has_sram = 1; cart.has_rumble = 1;
			break;
		case 0x1E:
			cart.mbc = 5; cart.has_sram = 1; cart.has_batt = 1; 
			cart.has_rumble = 1;
			break;
		case 0x1F:
			// pocket camera.
			fprintf(stderr, "pocket camera rom detected - not implemented\n");
			break;
		case 0xFD:
			// bandai TAMA5
			fprintf(stderr, "Bandai TAMA5 detected - not implemented\n");
			break;
		case 0xFE:
			// hudson HuC-3
			fprintf(stderr, "Hudson HuC-3 detected - not implemented\n");
			break;
		case 0xFF:
			// hudson HuC-1
			fprintf(stderr, "Hudson HuC-1 detected - not implemented\n");
			break;
		default:
			cart.mbc = 0;
			fprintf(stderr, "unrecognised cartridge type. assuming rom only and continuing anyway...\n");
			break;
	}

	// read cartridge ram size
	switch (cart.rom[CART_RAM_SIZE]) {
		case 0x00:
			cart.ram_size = 0;
			cart.ram_banks = 0;
			break;
		case 0x01:
			cart.ram_size = 2 * 1024;
			cart.ram_banks = 1;
			break;
		case 0x02:
			cart.ram_size = 8 * 1024;
			cart.ram_banks = 1;
			break;
		case 0x03:
			cart.ram_size = 32 * 1024;
			cart.ram_banks = 4;
			break;
		case 0x04:
			cart.ram_size = 128 * 1024;
			cart.ram_banks = 16;
			break;
		default:
			cart.ram_size = 0;
			cart.ram_banks = 0;
			printf("unrecognised ram size... assuming none and continuing anyway...\n");
			break;
	}

	// read cartridge rom size
	switch (cart.rom[CART_ROM_SIZE]) {
		case 0x00:
			cart.rom_size = 32 * 1024;
			cart.rom_banks = 2;
			break;
		case 0x01:
			cart.rom_size = 64 * 1024;
			cart.rom_banks = 4;
			break;
		case 0x02:
			cart.rom_size = 128 * 1024;
			cart.rom_banks = 8;
			break;
		case 0x03:
			cart.rom_size = 256 * 1024;
			cart.rom_banks = 16;
			break;
		case 0x04:
			cart.rom_size = 512 * 1024;
			cart.rom_banks = 32;
			break;
		case 0x05:
			cart.rom_size = 1024 * 1024;
			cart.rom_banks = 64;
			break;
		case 0x06:
			cart.rom_size = 2 * 1024 * 1024;
			cart.rom_banks = 128;
			break;
		case 0x52:
			cart.rom_size = 9 * 1024 * 1024 * 8;
			cart.rom_banks = 72;
			break;
		case 0x53:
			cart.rom_size = 10 * 1024 * 1024 * 8;
			cart.rom_banks = 80;
			break;
		case 0x54:
			cart.rom_size = 12 * 1024 * 1024 * 8;
			cart.rom_banks = 96;
			break;
		default:
			cart.rom_banks = cart.rom_size / 0x4000;
			printf("unrecognised rom size... assuming %uB and continuing anyway...\n", cart.rom_size);
			break;
	}
	// TODO: checksum / complement check test
	// TODO: display more information?

	if (cart.mbc == 2)
		cart.ram_size = 512;

	// allocate memory for on cartridge ram.
	if (cart.ram_size < 8 * 1024)
		cart.ram = malloc(sizeof(Byte) * 8 * 1025);	// simply to stop crashes.
	else
		cart.ram = malloc(sizeof(Byte) * cart.ram_size);

		
	cart.is_loaded = 1;


	printf("\tmbc: %u\trom size: %u\tram size:%u\n", cart.mbc, 
	    		cart.rom_size, cart.ram_size);
	
	cart.rom_fn = malloc(sizeof(char) * (strlen(fn) + 1));
	strcpy(cart.rom_fn, fn);

	/* if the cart has ram, see if a ram file was saved previously */
	if (cart.ram_size > 0) {
		if (find_sram_file()) {
			load_sram();
		} else {
			printf("could not find sram file\n");
		}
	}
	return 0;
	
}

void cart_reset(void) {
	// cant reset a nonloaded cartridge
	assert(cart.is_loaded == 1);
	// set up some defaults
	cart.rom_bank = 1;
	cart.rom_block = 0;
	cart.ram_bank = 0;
	//bzero(cart.ram, cart.ram_size); // supposed to be commented!
	if (cart.mbc == 1)
		cart.mbc_mode = MBC1_MODE_16MROM_8KRAM;
	set_vector_block(MEM_ROM_BANK_0, cart.rom, SIZE_ROM_BANK_0);
	//mem.setRom(rom);
	set_switchable_rom();
	set_switchable_ram();
}

void unload_rom(void) {
	// check that rom is already loaded
	assert(cart.is_loaded == 1);
	if (cart.ram_size > 0)
		save_sram();
	free(cart.ram);
	free(cart.rom);
	free(cart.rom_title);
	free(cart.rom_fn);
	cart.ram = 0;
	cart.rom = 0;
	cart.rom_title = 0;
	cart.rom_fn = 0;
	cart.is_loaded = 0;
}


static void set_switchable_rom(void) {
	//mem.setRomSw(rom_ + (romBank_ * 0x4000) + (cart.rom_block * 0x80000));
	set_vector_block(MEM_ROM_BANK_SW, cart.rom + (cart.rom_bank * 0x4000) + 
						(cart.rom_block * 0x80000), SIZE_ROM_BANK_SW);
}

static void set_switchable_ram(void) {
	//mem.setExtRamSw(ram_ + (cart.ram_bank * 0x4000));
	set_vector_block(MEM_RAM_BANK_SW, cart.ram + (cart.ram_bank * 0x2000), 
						SIZE_RAM_BANK_SW);
}

static void mbc3_latch_clock(void) {
	
}

/* FIXME: 	check for bad bank selection: prevent overflow exploits
 * 			implement ram bank disabling/enabling
 */
void write_rom(Word address, Byte value) {
	switch (cart.mbc) {
		case 1:
		// mbc1 ram bank enable/disable
		if (address < 0x2000) {
			return;
		}
		// mbc1 rom bank selection
		if ((address >= 0x2000) && (address < 0x4000)) {
			cart.rom_bank = value & 0x1F;
			if (cart.rom_bank == 0) {
				cart.rom_bank = 1;
			} else if (cart.rom_bank == 0x20) {
				cart.rom_bank = 0x21;
			} else if (cart.rom_bank == 0x40) {
				cart.rom_bank = 0x41;
			} else if (cart.rom_bank == 0x60) {
				cart.rom_bank = 0x61;
			}
			//fprintf(stderr, "value: %hhx\tbank:%x\n", value, cart.rom_bank);
			set_switchable_rom();
			return;
		}
		// mbc1 ram bank selection
		if ((cart.mbc_mode = MBC1_MODE_4MROM_32KRAM) && (address >= 0x4000) 
				&& (address < 0x6000)) {
			cart.ram_bank = value & 0x03;
			set_switchable_ram();
			return;
		}
		// mbc1 rom block selection
		if ((cart.mbc_mode = MBC1_MODE_16MROM_8KRAM) && (address >= 0x4000) 
				&& (address < 0x6000)) {
			cart.rom_block = value & 0x03;
			set_switchable_rom();
			return;
		}
		// mbc1 mode selection
		if ((address >= 0x6000) && (address < 0x8000)) {

			if ((value & 0x01) == 0x01) {
				cart.mbc_mode = MBC1_MODE_4MROM_32KRAM;
				cart.rom_bank = cart.rom_bank & 0x1F;
				set_switchable_rom();
			} else {
				cart.mbc_mode = MBC1_MODE_16MROM_8KRAM;
				cart.ram_bank = 0;
				set_switchable_ram();
			}
			return;
		}
			break;
		case 2:
			// mbc2 ram bank enable/disable
			if (address < 0x2000) {
				return;
			}
			// mbc2 rom bank selection
			if ((address >= 0x2000) && (address < 0x4000)) {
				cart.rom_bank = value & 0x0F;
				if (cart.rom_bank == 0)
					cart.rom_bank = 1;
				set_switchable_rom();
				return;
			}
			break;
		case 3:
			// mbc3 ram bank and TIMER enable/disable
			if (address < 0x2000) {
				// STUBBED
				return;
			}
			// mbc3 rom bank selection
			if ((address >= 0x2000) && (address < 0x4000)) {
				cart.rom_bank = value & 0x7F;
				if (cart.rom_bank == 0)
					cart.rom_bank = 1;
				set_switchable_rom();
				return;
			}
			// mbc3 ram bank / rtc map selection
			if ((address >= 0x4000) && (address < 0x6000)) {
				if ((value >= 0x08) && (value <= 0x0C)) {
					cart.mbc3_rtc_map = value;
				} else {
					cart.ram_bank = value & 0x03;
					set_switchable_ram();
				}
				return;
			}
			// mbc3 latch clock data
			if ((address >= 0x6000) && (address < 0x8000)) {
				if ((value & 0xFE) == 0) {
					// STUBBED: Latch clock data here
				}
				return;
			}
			break;
		case 4:
			/* FIXME: unimplemented */
			break;
		case 5:
		// mbc5 ram bank enable/disable
		if (address < 0x2000) {
			return;
		}
		// mbc5 rom bank selection
		if ((address >= 0x2000) && (address < 0x3000)) {
			cart.rom_bank = (cart.rom_bank & (~0xff)) | value;
			if (cart.rom_bank == 0)
				cart.rom_bank = 1;
			set_switchable_rom();
			return;
		}
		if ((address >= 0x3000) && (address < 0x4000)) {
			cart.rom_bank = (cart.rom_bank & 0xff) | ((value & 0x01) << 8);
			if (cart.rom_bank == 0)
				cart.rom_bank = 1;
			set_switchable_rom();
			return;
		}

		// mbc5 ram bank selection
			if ((address >= 0x4000) && (address < 0x6000)) {
			cart.ram_bank = value & 0x0f;
			set_switchable_ram();
			return;
		}
			break;
		default:
			printf("invalid write to rom\n");
			break;
	}	

}

Byte read_rom(Word address) {
	// bank 0
	if (address < MEM_ROM_BANK_0 + SIZE_ROM_BANK_0) {
		return cart.rom[address];
	}
	// switchable bank
	else if (address < MEM_ROM_BANK_SW + SIZE_ROM_BANK_SW) {
		return cart.rom[address - MEM_ROM_BANK_SW + (cart.rom_bank * 0x4000) + 
					  (cart.rom_block * 0x80000)];
	}
	else {
		// NOTREACHED
		assert(0);
		return 0;
	}
}

/*
 * saves the state of the ram to a file. 
 * TODO: place the sram file somewhere intelligent
 */
static void save_sram(void) {
	char *fn;
	FILE *fp;
	size_t c;
	fn = malloc(strlen(cart.rom_fn) + strlen(ram_ext) + 1);
	strcpy(fn, cart.rom_fn);
	strcat(fn, ram_ext);
	fp = fopen(fn, "w");
	if (fp == NULL) {
		fprintf(stderr, "could not open sram file for writing: %s\n", fn);
		perror("fopen");
		free(fn);
		return;
	}
	printf("saving sram to %s...\n", fn);
	c = fwrite(cart.ram, sizeof(Byte), cart.ram_size, fp);
	if (c < cart.ram_size) {
		fprintf(stderr, "error: wrote only %zu of %u bytes to sram file\n", c, cart.ram_size);
		perror("fwrite");
		fclose(fp);
		free(fn);
		return;
	}
	fclose(fp);
	free(fn);
}

/*
 * searches for an sram files. returns 1 if found, otherwise 0.
 */
int find_sram_file(void) {
	char *fn;
	struct stat fstats;
	fn = malloc(strlen(cart.rom_fn) + strlen(ram_ext) + 1);
	strcpy(fn, cart.rom_fn);
	strcat(fn, ram_ext);
	/* check if file is accessible */
	if (stat(fn, &fstats) != 0)	
		return 0;
	/* check if file is the correct size */
	if (fstats.st_size == cart.ram_size)
		return 1;
	else
		return 0;
}

static void load_sram(void) {
	char *fn;
	FILE *fp;
	size_t c;
	fn = malloc(strlen(cart.rom_fn) + strlen(ram_ext) + 1);
	strcpy(fn, cart.rom_fn);
	strcat(fn, ram_ext);
	fp = fopen(fn, "r");
	if (fp == NULL) {
		fprintf(stderr, "could not open sram file for reading: %s\n", fn);
		perror("fopen");
		free(fn);
		return;
	}
	printf("loading sram from %s...\n", fn);
	c = fread(cart.ram, sizeof(Byte), cart.ram_size, fp);
	if (c != cart.ram_size) {
		fprintf(stderr, "error: read only %zu of %u bytes from sram file\n", c, cart.ram_size);
		perror("fread");
		fclose(fp);
		free(fn);
		return;
	}
	fclose(fp);
	free(fn);	
}

void cart_save(void) {
	if (cart.ram_size > 0)
		save_memory("cram", cart.ram, cart.ram_size);
	save_uint("rom_bank", cart.rom_bank);
	save_uint("rom_block", cart.rom_block);
	save_uint("ram_bank", cart.ram_bank);
	save_uint("mbc_mode", cart.mbc_mode);
}

void cart_load(void) {
	cart_reset();
		
	if (cart.ram_size > 0)
		load_memory("cram", cart.ram, cart.ram_size);
	cart.rom_bank = load_uint("rom_bank");
	cart.rom_block = load_uint("rom_block");
	cart.ram_bank = load_uint("ram_bank");
	cart.mbc_mode = load_uint("mbc_mode");
	set_switchable_rom();
	set_switchable_ram();
}
