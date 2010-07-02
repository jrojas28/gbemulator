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

#ifndef _DISPLAY_H
#define _DISPLAY_H

#include <SDL/SDL.h>


#define DISPLAY_W 				160
#define	DISPLAY_H				144

#define OAM_CYCLES				(80)
#define OAM_VRAM_CYCLES			(172 + OAM_CYCLES)
#define HBLANK_CYCLES			(204 + OAM_VRAM_CYCLES)

#define BG_W					256
#define BG_H					256

#define TDT_0					0x8000
#define	TDT_0_LEN				0x1000
#define TDT_1					0x8800
#define	TDT_1_LEN				0x1000

#define TILE_MAP_0              0x9800
#define TILE_MAP_1              0x9C00

#define MAX_SPRITES_PER_LINE	10
#define OAM_BLOCKS				40
#define OAM_BLOCK_SIZE			4

#define OAM_YPOS				0
#define OAM_XPOS				1
#define OAM_PATTERN				2
#define OAM_FLAGS				3
#define FLAG_PRIORITY			0x80
#define FLAG_YFLIP				0x40
#define FLAG_XFLIP				0x20
#define FLAG_PALETTE			0x10

#define STAT_MODE_HBLANK		0x00
#define STAT_MODE_VBLANK		0x01
#define STAT_MODE_OAM			0x02
#define STAT_MODE_OAM_VRAM		0x03
#define STAT_FLAG_COINCIDENCE	0x04
#define STAT_INT_HBLANK			0x08
#define STAT_INT_VBLANK			0x10
#define STAT_INT_OAM			0x20
#define STAT_INT_COINCIDENCE	0x40
#define STAT_MODES				0x03

#define OAM_FLAG_PRIORITY		0x80
#define OAM_FLAG_YFLIP			0x40
#define OAM_FLAG_XFLIP			0x20
#define OAM_FLAG_PALETTE		0x10

#define TILES					256
#define SPRITES					256

#define COLOUR_0				0
#define COLOUR_123				1
#define PRIORITY_HIGH			0
#define PRIORITY_LOW			1

#define X_FLIP 0x01
#define Y_FLIP 0x02

struct tile;
struct tprite;

typedef struct {
	SDL_Surface *screen;
	SDL_Surface *display;
	SDL_Palette background_palette;
	SDL_Palette sprite_palette[2];
	SDL_Color colours[4];

	unsigned int x_res, y_res, bpp;
	unsigned int cycles;
	Byte *vram;
	Byte *oam;
	Uint32 palette_bg[4];
	Uint32 palette_sprite_0[4];
	Uint32 palette_sprite_1[4];
	int sprite_height;
	struct tile* tiles_tdt_0;
	struct tile* tiles_tdt_1;
	struct sprite* sprites;
	unsigned int vram_bank;
} Display;

typedef struct tile {
	int is_invalidated;
	int height;
	SDL_Surface* surface_0;
	SDL_Surface* surface_123;
	Byte *pixel_data;
} Tile;

typedef struct sprite {
	int is_invalidated[4];
	int height;
	SDL_Surface* surface[4];
	Byte *pixel_data;
} Sprite;


void display_update(unsigned int cycles);
void display_reset();
void display_init();
void display_fini();
void draw_frame();
void update_bg_palette();
void update_sprite_palette_0();
void update_sprite_palette_1();
Byte check_coincidence(Byte ly, Byte stat);
void launch_dma(Byte address);
void display_save();
void display_load();
void set_vram_bank(unsigned int bank);

static inline void write_vram(Word address, Byte value);
static inline Byte read_vram(Word address);
static inline void write_oam(Word address, Byte value);
static inline Byte read_oam(Word address);
static inline void tile_invalidate(Tile *tile);
static inline void sprite_invalidate(Sprite *sprite);


static inline void write_vram(const Word address, const Byte value) {
	extern Display display;
	// NO else here, tile data tables overlap!
	if ((address >= TDT_0) && (address < (TDT_0 + TDT_0_LEN))) {
		tile_invalidate(&display.tiles_tdt_0[(address - TDT_0) >> 4]);
		sprite_invalidate(&display.sprites[(address - TDT_0) >> 4]);
	}

	if ((address >= TDT_1) && (address < (TDT_1 + TDT_1_LEN)))
		tile_invalidate(&display.tiles_tdt_1[(address - TDT_1) >> 4]);
    display.vram[address - MEM_VIDEO + (display.vram_bank * 0x2000)] = value;
}

static inline Byte read_vram(const Word address) {
	extern Display display;
	return display.vram[address - MEM_VIDEO + (display.vram_bank * 0x2000)];
}

static inline void write_oam(const Word address, const Byte value) {
	extern Display display;	
    display.vram[address - MEM_OAM] = value;
}

static inline Byte read_oam(const Word address) {
	extern Display display;
	return display.oam[address - MEM_OAM];
}

static inline void sprite_invalidate(Sprite *sprite) {
	for (int i = 0; i < 4; i++) {
		sprite->is_invalidated[i] = 1;
	}
}

static inline void tile_invalidate(Tile *tile) {
	tile->is_invalidated = 1;
}

#endif	//_DISPLAY_H
