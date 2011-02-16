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
#include <assert.h>
#include <math.h>
#include "gbem.h"
#include "display.h"
#include "memory.h"
#include "core.h"
#include "save.h"
#include "scale.h"

static inline Byte set_mode(Byte stat, Byte mode);

static void draw_background(const Byte lcdc, const Byte ly, const int colour);
static void draw_gbc_background(const Byte lcdc, const Byte ly, const int colour);
static void draw_window(const Byte lcdc, const Byte ly, const int colour);
static void draw_gbc_window(const Byte lcdc, const Byte ly, const int colour);
static void launch_hdma(int length);
static void draw_sprites(const Byte lcdc, const Byte ly, const int priority);
static inline Byte get_sprite_x(const unsigned int sprite);
static inline Byte get_sprite_y(const unsigned int sprite);
static inline Byte get_sprite_pattern(const unsigned int sprite);
static inline Byte get_sprite_flags(const unsigned int sprite);
static inline Uint32 get_pixel(const SDL_Surface *surface, const int x, 
						const int y);
static inline void put_pixel(const SDL_Surface *surface, const int x, 
    					const int y, const Uint32 pixel);
static void fill_rectangle(SDL_Surface *surface, const int x, const int y, 
						const int w, const int h, 
						const unsigned int colour);
static void tile_init(Tile *tile);
static void tile_fini(Tile *tile);
static void tile_regenerate(Tile *tile);
static void tile_set_palette(Tile *tile, SDL_Palette *palette);
static void tile_blit_0(Tile *tile, SDL_Surface *surface, const int x, 
						const int y, const int line, const int w);
static void tile_blit_123(Tile *tile, SDL_Surface *surface, const int x, 
						const int y, const int line, const int w);
static void sprite_init(Sprite *sprite);
static void sprite_fini(Sprite *sprite);
static void sprite_regenerate(Sprite *sprite, const int flip);
static void sprite_set_palette(Sprite *sprite, SDL_Palette *palette);
static void sprite_blit(Sprite *sprite, SDL_Surface* surface, const int x, const int y, 
					const int line, const int flip);
static void sprite_set_height(Sprite *sprite, int height);


Display display;
extern int console;
extern int console_mode;

void display_init(void) {
	display.x_res = DISPLAY_W * 4;
	display.y_res = DISPLAY_H * 4;
	display.bpp = 32;

	display.screen = SDL_SetVideoMode(display.x_res, display.y_res, display.bpp, SDL_SWSURFACE);
	if (display.screen == NULL) {
		fprintf(stderr, "video mode initialisation failed\n");
		exit(1);
	}
	#ifdef WINDOWS
		// redirecting the standard input/output to the console 
		// is required with windows.
		activate_console(); 
	#endif // WINDOWS

	printf("sdl video initialised.\n");
	SDL_WM_SetCaption("gbem", "gbem");

	display.display = SDL_CreateRGBSurface(SDL_SWSURFACE, 
                                 DISPLAY_W, DISPLAY_H, display.bpp, 0, 0, 0, 0);
	if (display.display == NULL) {
		fprintf(stderr, "could not create surface\n");
		exit(1);
	}

	//display.display = SDL_DisplayFormat(display.display);

	display.colours[0].r = 0xFF; 
	display.colours[0].g = 0xFF; 
	display.colours[0].b = 0xFF;

	display.colours[1].r = 0xAA; 
	display.colours[1].g = 0xAA; 
	display.colours[1].b = 0xAA;

	display.colours[2].r = 0x55; 
	display.colours[2].g = 0x55; 
	display.colours[2].b = 0x55;

	display.colours[3].r = 0x00; 
	display.colours[3].g = 0x00; 
	display.colours[3].b = 0x00;

	display.background_palette.ncolors = 4;
	display.background_palette.colors = malloc(sizeof(SDL_Color) * 5);

	display.sprite_palette[0].ncolors = 4;
	display.sprite_palette[0].colors = malloc(sizeof(SDL_Color) * 4);
	display.sprite_palette[1].ncolors = 4;
	display.sprite_palette[1].colors = malloc(sizeof(SDL_Color) * 4);

	display.background_palette.colors[4].r = 0xFF;
	display.background_palette.colors[4].g = 0xFF;
	display.background_palette.colors[4].b = 0xFF;
	
	display.vram = NULL;
	display.oam = NULL;
	
	return;
}

void display_fini(void) {
	int i;
	for (i = 0; i < display.cache_size; i++) {
		tile_fini(&display.tiles_tdt_0[i]);
	}
	for (i = 0; i < display.cache_size; i++) {
		tile_fini(&display.tiles_tdt_1[i]);
	}
	for (i = 0; i < display.cache_size; i++) {
		sprite_fini(&display.sprites[i]);
	}

	SDL_FreeSurface(display.display);
	if (display.vram != NULL)
		free(display.vram);
	if (display.oam != NULL)
		free(display.oam);
	free(display.background_palette.colors);
	free(display.sprite_palette[0].colors);
	free(display.sprite_palette[1].colors);
	free(display.tiles_tdt_0);
	free(display.tiles_tdt_1);
	free(display.sprites);

}


void display_reset(void) {
	int i;
	if ((console == CONSOLE_GBC) || (console == CONSOLE_GBA)) {
		display.vram = malloc(sizeof(Byte) * VRAM_SIZE_GBC);
		memset(display.vram, 0, VRAM_SIZE_GBC);
	} else {
		display.vram = malloc(sizeof(Byte) * VRAM_SIZE_DMG);
		memset(display.vram, 0, VRAM_SIZE_DMG);
	}

	display.vram_bank = 0;
	display.oam = malloc(sizeof(Byte) * SIZE_OAM);
	memset(display.oam, 0, SIZE_OAM);
	set_vector_block(MEM_VIDEO, display.vram + (display.vram_bank * 0x2000), SIZE_VIDEO);
	set_vector_block(MEM_OAM, display.oam, 0x100);
	
	if (console_mode == MODE_GBC_ENABLED) {
		display.cache_size = 512;
	} else {
		display.cache_size = 256;
	}

	display.tiles_tdt_0 = malloc(sizeof(Tile) * display.cache_size);
	display.tiles_tdt_1 = malloc(sizeof(Tile) * display.cache_size);
	display.sprites = malloc(sizeof(Sprite) * display.cache_size);

	for (i = 0; i < display.cache_size; i++) {
		tile_init(&display.tiles_tdt_0[i]);
	}
	for (i = 0; i < display.cache_size; i++) {
		tile_init(&display.tiles_tdt_1[i]);
	}
	for (i = 0; i < display.cache_size; i++) {
		sprite_init(&display.sprites[i]);
	}

	update_bg_palette();
	update_sprite_palette_0();
	update_sprite_palette_1();
	
	for (i = 0; i < display.cache_size; i++) {
		display.tiles_tdt_0[i].pixel_data = display.vram + ((i % 256) * 16) + ((i / 256) * 0x2000);
		tile_set_palette(&display.tiles_tdt_0[i], &display.background_palette);
		tile_invalidate(&display.tiles_tdt_0[i]);
	}
	for (i = 0; i < display.cache_size; i++) {
		display.tiles_tdt_1[i].pixel_data = display.vram + ((i % 256) * 16) + 0x0800 + ((i / 256) * 0x2000);
		tile_set_palette(&display.tiles_tdt_1[i], &display.background_palette);
		tile_invalidate(&display.tiles_tdt_1[i]);
	}
	for (i = 0; i < display.cache_size; i++) {
		display.sprites[i].pixel_data = display.vram + ((i % 256) * 16) + ((i / 256) * 0x2000);
		sprite_set_palette(&display.sprites[i], &display.background_palette);
		sprite_invalidate(&display.sprites[i]);
	}

	display.sprite_height = 8;
	display.cycles = 0;	
	display.is_hdma_active = 0;
	SDL_FillRect(display.display, NULL, SDL_MapRGB(display.display->format, 0xff, 0xff, 0xff));
	SDL_FillRect(display.screen, NULL, SDL_MapRGB(display.screen->format, 0xff, 0xff, 0xff));
}

void set_vram_bank(unsigned int bank) {
	display.vram_bank = bank;
	set_vector_block(MEM_VIDEO, display.vram + (display.vram_bank * 0x2000), SIZE_VIDEO);
}

void set_lcdc(Byte value) {
	if ((value & 0x80) != (read_io(HWREG_LCDC) & 0x80)) {
		write_io(HWREG_LY, 0);
		SDL_FillRect(display.display, NULL, SDL_MapRGB(display.display->format, 0xff, 0xff, 0xff));
	}
	write_io(HWREG_LCDC, value);
}


/* FIXME: if lots of cycles have passed, modes could be skipped! */
void display_update(unsigned int cycles) {
	Byte ly, stat, lcdc, hdma_length;
	int i;
	display.cycles += cycles;
	ly = read_io(HWREG_LY);
	stat = read_io(HWREG_STAT);
	lcdc = read_io(HWREG_LCDC);
	if (!(lcdc & 0x80)) {
		lcd_off_start:
		if (display.cycles < OAM_CYCLES) {
			/* check that we are not already in oam */
			if ((stat & STAT_MODES) != STAT_MODE_OAM) {
				/* set the mode flag in STAT */
				stat = (stat & (~STAT_MODES)) | STAT_MODE_OAM;
				/* if oam stat interrupt is enabled, raise the interrupt */
				if (stat & STAT_INT_OAM) {
					raise_int(INT_STAT);
				}
			}
		} else
		/* is the lcd reading from oam and vram? */
		if (display.cycles < OAM_VRAM_CYCLES) {
			/* check that we are not already in oam / vram */
			if ((stat & STAT_MODES) != STAT_MODE_OAM_VRAM) {
				/* set the mode flag in STAT */
				stat = (stat & (~STAT_MODES)) | STAT_MODE_OAM_VRAM;
			}
		} else 
		/* is the lcd in hblank? */
		if (display.cycles < HBLANK_CYCLES) {
			/* check that we are not already in hblank */
			if ((stat & STAT_MODES) != STAT_MODE_HBLANK) {
				/* set the mode flag in STAT */
				stat = (stat & (~STAT_MODES)) | STAT_MODE_HBLANK;
				/* if hblank stat interrupt is enabled, raise the interrupt */
				if (stat & STAT_INT_HBLANK) {
					raise_int(INT_STAT);
				}
			}
		/* has the lcd finished hblank? */
		} else {
			display.cycles -= HBLANK_CYCLES;
			goto lcd_off_start;
		}
		write_io(HWREG_LY, ly);
		write_io(HWREG_STAT, stat);
		return;
	}
	
	start:
	/* are we drawing the screen or are we in vblank? */
	if (ly < DISPLAY_H) {
		/* is the lcd reading from oam? */
		if (display.cycles < OAM_CYCLES) {
			/* check that we are not already in oam */
			if ((stat & STAT_MODES) != STAT_MODE_OAM) {
				/* set the mode flag in STAT */
				stat = (stat & (~STAT_MODES)) | STAT_MODE_OAM;
				/* if oam stat interrupt is enabled, raise the interrupt */
				if (stat & STAT_INT_OAM) {
					raise_int(INT_STAT);
				}
			}
		} else
		/* is the lcd reading from oam and vram? */
		if (display.cycles < OAM_VRAM_CYCLES) {
			/* check that we are not already in oam / vram */
			if ((stat & STAT_MODES) != STAT_MODE_OAM_VRAM) {
				/* set the mode flag in STAT */
				stat = (stat & (~STAT_MODES)) | STAT_MODE_OAM_VRAM;
			}
		} else 
		/* is the lcd in hblank? */
		if (display.cycles < HBLANK_CYCLES) {
			/* check that we are not already in hblank */
			if ((stat & STAT_MODES) != STAT_MODE_HBLANK) {
				/* set the mode flag in STAT */
				stat = (stat & (~STAT_MODES)) | STAT_MODE_HBLANK;
				/* if hblank stat interrupt is enabled, raise the interrupt */
				if (stat & STAT_INT_HBLANK) {
					raise_int(INT_STAT);
				}
				/* draw the line */
				if (console_mode == MODE_GBC_ENABLED) {
					if (lcdc & 0x01)
						draw_gbc_background(lcdc, ly, COLOUR_0);
					if (lcdc & 0x20)
						draw_gbc_window(lcdc, ly, COLOUR_0);
					if (lcdc & 0x02)
						draw_sprites(lcdc, ly, PRIORITY_LOW);
					if (lcdc & 0x01)
						draw_gbc_background(lcdc, ly, COLOUR_123);
					if (lcdc & 0x20)
						draw_gbc_window(lcdc, ly, COLOUR_123);
					if (lcdc & 0x02)
						draw_sprites(lcdc, ly, PRIORITY_HIGH);
				} else {
					if (lcdc & 0x01)
						draw_background(lcdc, ly, COLOUR_0);
					if (lcdc & 0x20)
						draw_window(lcdc, ly, COLOUR_0);
					if (lcdc & 0x02)
						draw_sprites(lcdc, ly, PRIORITY_LOW);
					if (lcdc & 0x01)
						draw_background(lcdc, ly, COLOUR_123);
					if (lcdc & 0x20)
						draw_window(lcdc, ly, COLOUR_123);
					if (lcdc & 0x02)
						draw_sprites(lcdc, ly, PRIORITY_HIGH);
				}
			}
		/* has the lcd finished hblank? */
		} else {
			/* if running, launch hdma */
			if (display.is_hdma_active) {
				launch_hdma(1);
				hdma_length = read_io(HWREG_HDMA5) & 0x7f;
				fprintf(stderr, "%hhu", hdma_length);
				if (hdma_length == 0) {
					display.is_hdma_active = 0;
					write_io(HWREG_HDMA5, 0xff);
				} else {
					--hdma_length;
					write_io(HWREG_HDMA5, hdma_length);
				}
			}
			++ly;
			stat = check_coincidence(ly, stat);
			/* start from the beginning on the next line */
			display.cycles -= HBLANK_CYCLES;
			goto start;
		}
	} else {
		/* in vblank */
		/* check that we are not already in vblank */
		if ((stat & STAT_MODES) != STAT_MODE_VBLANK) {
			/* set the mode flag in STAT */
			stat = (stat & (~STAT_MODES)) | STAT_MODE_VBLANK;
			/* if vblank stat interrupt is enabled, raise the interrupt */
			if (stat & STAT_INT_VBLANK) {
				raise_int(INT_STAT);
			}
			raise_int(INT_VBLANK);
		}
		if (display.cycles >= HBLANK_CYCLES) {
			++ly;
			stat = check_coincidence(ly, stat);
			display.cycles -= HBLANK_CYCLES;
			/* has vblank just ended? */
			if (ly == 154) {
				ly = 0;
				stat = check_coincidence(ly, stat);
				draw_frame();
				SDL_FillRect(display.display, NULL, SDL_MapRGB(display.display->format, 0xff, 0xff, 0xff));
				//new_frame();
				// before we begin redrawing the screen, sort out some things.
				// update sprite palettes - fairly ugly hack.
				for (i = 0; i < OAM_BLOCKS; i++) {
					sprite_set_palette(&display.sprites[get_sprite_pattern(i)], 
						&display.sprite_palette[(get_sprite_flags(i) 
							& FLAG_PALETTE) >> 4]);
				}
				// update sprite height - another fairly ugly hack.
				// if spriteHeight has been changed from 8 to 16:
				if ((lcdc & 0x04) && (display.sprite_height == 8)) {
					display.sprite_height = 16;
					for (i = 0; i < display.cache_size; i++) {
						//sprites_[i]->setHeight(16);
						sprite_set_height(&display.sprites[i], 16);
					}
				}
				// if spriteHeight has been changed from 16 to 8:
				if ((!(lcdc & 0x04)) && (display.sprite_height == 16)) {
					display.sprite_height = 8;
					for (i = 0; i < display.cache_size; i++) {
						sprite_set_height(&display.sprites[i], 8);
					}
				}
			}
			goto start;
		}
	}
	write_io(HWREG_LY, ly);
	write_io(HWREG_STAT, stat);
}

Byte check_coincidence(Byte ly, Byte stat) {
	if (ly == read_io(HWREG_LYC)) {
		/* check that this a new coincidence */
		if (!(stat & STAT_FLAG_COINCIDENCE)) {
			stat |= STAT_FLAG_COINCIDENCE;	/* set coincidence flag */
			if (stat & STAT_INT_COINCIDENCE)
				raise_int(INT_STAT);
		}
	} else {
		stat &= ~(STAT_FLAG_COINCIDENCE);	/* unset coincidence flag */
	}
	return stat;
}

void draw_frame() {
	scale_nn4x(display.display, display.screen);
	SDL_Flip(display.screen);
}

static void draw_background(const Byte lcdc, const Byte ly, const int colour) {
    unsigned int x;
	Byte scx = read_io(HWREG_SCX);
    Byte scy = read_io(HWREG_SCY);
    Byte wx = read_io(HWREG_WX);
    Byte wy = read_io(HWREG_WY);
	unsigned int tile_code;
	Byte offset_x = scx & 0x07;
	Byte offset_y = (ly + scy) & 0x07;
	Byte bg_y = (ly + scy) & 0xFF;
	Byte tile_x;
	Byte tile_y;
	int screen_x;
	unsigned int x_pos;
	for (x = scx; x <= (scx + (unsigned)DISPLAY_W); x += 8) {
		Byte bg_x = x & 0xFF;
		tile_x = (bg_x / 8);
		tile_y = (bg_y / 8);
		screen_x = scx / 8;
		x_pos = x - scx - offset_x;
		/* dont draw over the window! FIXME. */
		if ((lcdc & 0x20) && (x_pos + 15 >= wx) && (ly >= wy))
			continue;
		if ((lcdc & 0x08) == 0) {
			// tile map is at 0x9800-0x9BFF bank 0
			tile_code = display.vram[TILE_MAP_0 - MEM_VIDEO + (tile_y * 32) + tile_x];
		} else {
			// tile map is at 0x9C00-0x9FFF bank 0
			tile_code = display.vram[TILE_MAP_1 - MEM_VIDEO + (tile_y * 32) + tile_x];
		}
		if ((lcdc & 0x10) == 0) {
			// tile data is at 0x8800-0x97FF (indeces signed)
			// complement upper bit
			tile_code ^= 0x80;
			if (colour == COLOUR_0)
				tile_blit_0(&display.tiles_tdt_1[tile_code], display.display, x_pos, ly, offset_y, 8);
			if (colour == COLOUR_123)
				tile_blit_123(&display.tiles_tdt_1[tile_code], display.display, x_pos, ly, offset_y, 8);
		} else {
			// tile data is at 0x8000-0x8FFF (indeces unsigned)
			if (colour == COLOUR_0)
				tile_blit_0(&display.tiles_tdt_0[tile_code], display.display, x_pos, ly, offset_y, 8);
			if (colour == COLOUR_123)
				tile_blit_123(&display.tiles_tdt_0[tile_code], display.display, x_pos, ly, offset_y, 8);
		}
	}
}

static void draw_gbc_background(const Byte lcdc, const Byte ly, const int colour) {
	#define TILE_PALETTE		0x07
	#define TILE_VRAM_BANK		0x08
	#define TILE_X_FLIP			0x20
	#define TILE_Y_FLIP			0x40
	#define TILE_PRIORITY		0x80
	unsigned int x;
	Byte scx = read_io(HWREG_SCX);
	Byte scy = read_io(HWREG_SCY);
	Byte wx = read_io(HWREG_WX);
	Byte wy = read_io(HWREG_WY);
	unsigned int tile_code;
	Byte offset_x = scx & 0x07;
	Byte offset_y = (ly + scy) & 0x07;
	Byte bg_y = (ly + scy) & 0xFF;
	Byte tile_x;
	Byte tile_y;
	Byte attrib;
	int screen_x;
	unsigned int x_pos;
	for (x = scx; x <= (scx + (unsigned)DISPLAY_W); x += 8) {
		Byte bg_x = x & 0xFF;
		tile_x = (bg_x / 8);
		tile_y = (bg_y / 8);
		screen_x = scx / 8;
		x_pos = x - scx - offset_x;
		/* dont draw over the window! */
		if ((lcdc & 0x20) && (x_pos + 7 >= wx) && (ly >= wy))
			continue;
		if ((lcdc & 0x08) == 0) {
			// tile map is at 0x9800-0x9BFF bank 0
			tile_code = display.vram[TILE_MAP_0 - MEM_VIDEO + (tile_y * 32) + tile_x];
			attrib = display.vram[TILE_MAP_0 - MEM_VIDEO + VRAM_BANK_SIZE + (tile_y * 32) + tile_x];
		} else {
			// tile map is at 0x9C00-0x9FFF bank 0
			tile_code = display.vram[TILE_MAP_1 - MEM_VIDEO + (tile_y * 32) + tile_x];
			attrib = display.vram[TILE_MAP_1 - MEM_VIDEO + VRAM_BANK_SIZE + (tile_y * 32) + tile_x];
		}
		if ((lcdc & 0x10) == 0) {
			// tile data is at 0x8800-0x97FF (indeces signed)
			// complement upper bit
			tile_code ^= 0x80;
			if (attrib & TILE_VRAM_BANK)
				tile_code += 256;
			if (colour == COLOUR_0)
				tile_blit_0(&display.tiles_tdt_1[tile_code], display.display, x_pos, ly, offset_y, 8);
			if (colour == COLOUR_123)
				tile_blit_123(&display.tiles_tdt_1[tile_code], display.display, x_pos, ly, offset_y, 8);
		} else {
			if (attrib & TILE_VRAM_BANK)
				tile_code += 256;
			// tile data is at 0x8000-0x8FFF (indeces unsigned)
			if (colour == COLOUR_0)
				tile_blit_0(&display.tiles_tdt_0[tile_code], display.display, x_pos, ly, offset_y, 8);
			if (colour == COLOUR_123)
				tile_blit_123(&display.tiles_tdt_0[tile_code], display.display, x_pos, ly, offset_y, 8);
		}
	}
}

static void draw_window(const Byte lcdc, const Byte ly, const int colour) {
    unsigned int win_x;
	Byte wx = read_io(HWREG_WX);
    Byte wy = read_io(HWREG_WY);
	unsigned int tile_code;
	Byte offset_y = (ly - wy) & 0x07;
	Byte win_y = (ly - wy);
	Byte tile_x;
	Byte tile_y;
	int x;
	if ((win_y >= DISPLAY_H) || (ly < wy))
		return;
	for (win_x = 0; win_x <= 255; win_x += 8) {
		tile_x = (win_x / 8);
		tile_y = (win_y / 8);
		x = win_x + wx - (signed)7;
		if (x >= DISPLAY_W)
			return;
        if ((lcdc & 0x40) == 0) {
            // tile map is at 0x9800-0x9BFF
			tile_code = display.vram[TILE_MAP_0 - MEM_VIDEO + (tile_y * 32) + tile_x];
        } else {
            // tile map is at 0x9C00-0x9FFF
			tile_code = display.vram[TILE_MAP_1 - MEM_VIDEO + (tile_y * 32) + tile_x];
        }
        if ((lcdc & 0x10) == 0) {
		    // tile data is at 0x8800-0x97FF (indeces signed)
			// complement upper bit
			tile_code ^= 0x80;
			if (colour == COLOUR_0)
				tile_blit_0(&display.tiles_tdt_1[tile_code], display.display, x, ly, offset_y, 8);
			if (colour == COLOUR_123)
				tile_blit_123(&display.tiles_tdt_1[tile_code], display.display, x, ly, offset_y, 8);
	    } else {
		    // tile data is at 0x8000-0x8FFF (indeces unsigned)
			if (colour == COLOUR_0)
				tile_blit_0(&display.tiles_tdt_1[tile_code], display.display, x, ly, offset_y, 8);
			if (colour == COLOUR_123)
				tile_blit_123(&display.tiles_tdt_1[tile_code], display.display, x, ly, offset_y, 8);
		}
	}
}

static void draw_gbc_window(const Byte lcdc, const Byte ly, const int colour) {
    unsigned int win_x;
	Byte wx = read_io(HWREG_WX);
    Byte wy = read_io(HWREG_WY);
	unsigned int tile_code;
	Byte offset_y = (ly - wy) & 0x07;
	Byte win_y = (ly - wy);
	Byte tile_x;
	Byte tile_y;
	Byte attrib;
	int x;
	if ((win_y >= DISPLAY_H) || (ly < wy))
		return;
	for (win_x = 0; win_x <= 255; win_x += 8) {
		tile_x = (win_x / 8);
		tile_y = (win_y / 8);
		x = win_x + wx - (signed)7;
		if (x >= DISPLAY_W)
			return;
        if ((lcdc & 0x40) == 0) {
            // tile map is at 0x9800-0x9BFF
			tile_code = display.vram[TILE_MAP_0 - MEM_VIDEO + (tile_y * 32) + tile_x];
			attrib = display.vram[TILE_MAP_0 - MEM_VIDEO + VRAM_BANK_SIZE + (tile_y * 32) + tile_x];
        } else {
            // tile map is at 0x9C00-0x9FFF
			tile_code = display.vram[TILE_MAP_1 - MEM_VIDEO + (tile_y * 32) + tile_x];
			attrib = display.vram[TILE_MAP_0 - MEM_VIDEO + VRAM_BANK_SIZE + (tile_y * 32) + tile_x];
        }
        if ((lcdc & 0x10) == 0) {
		    // tile data is at 0x8800-0x97FF (indeces signed)
			// complement upper bit
			tile_code ^= 0x80;
       		if (attrib & TILE_VRAM_BANK)
				tile_code += 256;
			if (colour == COLOUR_0)
				tile_blit_0(&display.tiles_tdt_1[tile_code], display.display, x, ly, offset_y, 8);
			if (colour == COLOUR_123)
				tile_blit_123(&display.tiles_tdt_1[tile_code], display.display, x, ly, offset_y, 8);
		} else {
			if (attrib & TILE_VRAM_BANK)
				tile_code += 256;
			// tile data is at 0x8000-0x8FFF (indeces unsigned)
			if (colour == COLOUR_0)
				tile_blit_0(&display.tiles_tdt_1[tile_code], display.display, x, ly, offset_y, 8);
			if (colour == COLOUR_123)
				tile_blit_123(&display.tiles_tdt_1[tile_code], display.display, x, ly, offset_y, 8);
		}
	}
}


/* TODO optimise? */
void draw_sprites(const Byte lcdc, const Byte ly, const int priority) {
	int sprite_x;
	int sprite_y;
	int offset_y;
	int i;
	Byte sprite_priority;
	for (i = 0; i < OAM_BLOCKS; i++) {
		sprite_y = get_sprite_y(i) - (signed)16;
		sprite_x = get_sprite_x(i) - (signed)8;
		sprite_priority = get_sprite_flags(i) >> 7;
		if ((ly >= sprite_y) && (ly < (sprite_y + display.sprite_height)) && (sprite_priority == priority)) {
			offset_y = (signed)ly - sprite_y;
			sprite_blit(&display.sprites[get_sprite_pattern(i)], display.display, sprite_x, ly, offset_y, (get_sprite_flags(i) & 0x60) >> 5);
		}
	}
}


void update_bg_palette(void) {
	int i;
	Byte bgp = read_io(HWREG_BGP);
	display.background_palette.colors[0].r = display.colours[bgp & 0x03].r;
	display.background_palette.colors[0].g = display.colours[bgp & 0x03].g;
	display.background_palette.colors[0].b = display.colours[bgp & 0x03].b;

	display.background_palette.colors[1].r = display.colours[(bgp >> 2) & 0x03].r;
	display.background_palette.colors[1].g = display.colours[(bgp >> 2) & 0x03].g;
	display.background_palette.colors[1].b = display.colours[(bgp >> 2) & 0x03].b;

	display.background_palette.colors[2].r = display.colours[(bgp >> 4) & 0x03].r;
	display.background_palette.colors[2].g = display.colours[(bgp >> 4) & 0x03].g;
	display.background_palette.colors[2].b = display.colours[(bgp >> 4) & 0x03].b;

	display.background_palette.colors[3].r = display.colours[(bgp >> 6) & 0x03].r;
	display.background_palette.colors[3].g = display.colours[(bgp >> 6) & 0x03].g;
	display.background_palette.colors[3].b = display.colours[(bgp >> 6) & 0x03].b;

	for (i = 0; i < display.cache_size; i++) {
		tile_set_palette(&display.tiles_tdt_0[i], &display.background_palette);
	}
	for (i = 0; i < display.cache_size; i++) {
		tile_set_palette(&display.tiles_tdt_1[i], &display.background_palette);
	}
}

void update_sprite_palette_0(void) {
	Byte obp0 = read_io(HWREG_OBP0);
	// colour 0 is transparent anyway.
	display.sprite_palette[0].colors[0].r = display.colours[obp0 & 0x03].r;
	display.sprite_palette[0].colors[0].g = display.colours[obp0 & 0x03].g;
	display.sprite_palette[0].colors[0].b = display.colours[obp0 & 0x03].b;

	display.sprite_palette[0].colors[1].r = display.colours[(obp0 >> 2) & 0x03].r;
	display.sprite_palette[0].colors[1].g = display.colours[(obp0 >> 2) & 0x03].g;
	display.sprite_palette[0].colors[1].b = display.colours[(obp0 >> 2) & 0x03].b;

	display.sprite_palette[0].colors[2].r = display.colours[(obp0 >> 4) & 0x03].r;
	display.sprite_palette[0].colors[2].g = display.colours[(obp0 >> 4) & 0x03].g;
	display.sprite_palette[0].colors[2].b = display.colours[(obp0 >> 4) & 0x03].b;

	display.sprite_palette[0].colors[3].r = display.colours[(obp0 >> 6) & 0x03].r;
	display.sprite_palette[0].colors[3].g = display.colours[(obp0 >> 6) & 0x03].g;
	display.sprite_palette[0].colors[3].b = display.colours[(obp0 >> 6) & 0x03].b;

	/* the sprite cache is updated elsewhere... */
}

void update_sprite_palette_1(void) {
	Byte obp1 = read_io(HWREG_OBP1);
	// colour 0 is transparent anyway.
	display.sprite_palette[1].colors[0].r = display.colours[obp1 & 0x03].r;
	display.sprite_palette[1].colors[0].g = display.colours[obp1 & 0x03].g;
	display.sprite_palette[1].colors[0].b = display.colours[obp1 & 0x03].b;

	display.sprite_palette[1].colors[1].r = display.colours[(obp1 >> 2) & 0x03].r;
	display.sprite_palette[1].colors[1].g = display.colours[(obp1 >> 2) & 0x03].g;
	display.sprite_palette[1].colors[1].b = display.colours[(obp1 >> 2) & 0x03].b;

	display.sprite_palette[1].colors[2].r = display.colours[(obp1 >> 4) & 0x03].r;
	display.sprite_palette[1].colors[2].g = display.colours[(obp1 >> 4) & 0x03].g;
	display.sprite_palette[1].colors[2].b = display.colours[(obp1 >> 4) & 0x03].b;

	display.sprite_palette[1].colors[3].r = display.colours[(obp1 >> 6) & 0x03].r;
	display.sprite_palette[1].colors[3].g = display.colours[(obp1 >> 6) & 0x03].g;
	display.sprite_palette[1].colors[3].b = display.colours[(obp1 >> 6) & 0x03].b;

	/* the sprite cache is updated elsewhere... */
}

static inline Byte get_sprite_x(const unsigned int sprite) {
	return display.oam[(OAM_BLOCK_SIZE * sprite) + OAM_XPOS];
}

static inline Byte get_sprite_y(const unsigned int sprite) {
	return display.oam[(OAM_BLOCK_SIZE * sprite) + OAM_YPOS];
}

static inline Byte get_sprite_pattern(const unsigned int sprite) {
	if (display.sprite_height == 8)
		return display.oam[(OAM_BLOCK_SIZE * sprite) + OAM_PATTERN];
	else
		return (display.oam[(OAM_BLOCK_SIZE * sprite) + OAM_PATTERN]) & 0xFE;
}

static inline Byte get_sprite_flags(const unsigned int sprite) {
	return display.oam[(OAM_BLOCK_SIZE * sprite) + OAM_FLAGS];
}

void launch_dma(Byte address) {
	unsigned int i;
	Word real_address = address * 0x100;
	for (i = 0; i < SIZE_OAM; i++) {
		display.oam[i] = readb(real_address + i);
	}
}

static void launch_hdma(int length) {
	int i;
	Word src = (read_io(HWREG_HDMA2) & 0xf0) + ((Word)read_io(HWREG_HDMA1) << 8);
	Word dest = (read_io(HWREG_HDMA4) & 0xf0) + ((Word)(read_io(HWREG_HDMA3) & 0x1f) << 8) + MEM_VIDEO;
	
	fprintf(stderr, "src: %hx. dest: %hx. length: %i:\n", src, dest, length);
	//fprintf(stderr, "hdma1: %hhx. hdma2: %hhx. hdma3: %hhx. hdma4: %hhx. hdma5: %hhx\n", read_io(HWREG_HDMA1), read_io(HWREG_HDMA3), read_io(HWREG_HDMA3), read_io(HWREG_HDMA4), read_io(HWREG_HDMA5));
	length *= 16;
	assert(length <= 0x800);
	if ((src <= 0x7ff0) || ((src >= 0xa000) && (src <= 0xdff0))) {
		for (i = 0; i < length; i++)
			write_vram(dest++, readb(src++));
		write_io(HWREG_HDMA2, src & 0xf0);
		write_io(HWREG_HDMA1, (src >> 8));
		write_io(HWREG_HDMA4, dest & 0xf0);
		write_io(HWREG_HDMA3, (dest >> 8) & 0x1f);
	} else {
		fprintf(stderr, "hdma src/dest invalid. src: %hx dest: %hx\n", src, dest);
	}
}

void start_hdma(Byte hdma5) {
	if (hdma5 & 0x80) {
		/* hblank dma */
		fprintf(stderr, "hblank dma: %hhx ", read_io(HWREG_HDMA5));
		write_io(HWREG_HDMA5, read_io(HWREG_HDMA5) & (~0x80));
		display.is_hdma_active = 1;
	} else {
		if (display.is_hdma_active) {
			display.is_hdma_active = 0;
			write_io(HWREG_HDMA5, 0xFF);
			return;
		}
		/* general dma */
		fprintf(stderr, "general dma: hdma5: %hhx\n", hdma5);
		fprintf(stderr, "general dma ");
		launch_hdma((hdma5 & 0x7f) + 1);
		write_io(HWREG_HDMA5, 0xFF);
	}
}

// gets colour of a pixel
static inline Uint32 get_pixel(const SDL_Surface *surface, const int x, 
			const int y) {
	//assert (surface != NULL);
	//assert (x < surface->w); assert (x >= 0);
	//assert (y < surface->h); assert (y >= 0);
	unsigned int bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
	switch (bpp) {
		case 1:
			return *p;
		case 2:
			return *(Uint16*)p;
		case 3:
			if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				return p[0] << 16 | p[1] << 8 | p[2];
			} else {
				return p[0] | p[1] << 8 | p[2] << 16;
			}
		case 4:
			return *(Uint32*)p;
		default:
			return 0; // shouldn't happen, but avoids warnings
	}
}

// sets colour of a pixel
static inline void put_pixel(const SDL_Surface *surface, const int x, const int y, 
                        const Uint32 pixel) {
	//assert (surface != NULL);
	//assert (x < surface->w); assert (x >= 0);
	//assert (y < surface->h); assert (y >= 0);
	unsigned int bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;
	switch (bpp) {
		case 1:
			*p = pixel;
			break;
 		case 2:
			*(Uint16*)p = pixel;
			break;
		case 3:
			if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				p[0] = (pixel >> 16) & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = pixel & 0xff;
			} else {
				p[0] = pixel & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = (pixel >> 16) & 0xff;
			}
			break;
		case 4:
			*(Uint32 *)p = pixel;
			break;
	}
}


static void fill_rectangle(SDL_Surface *surface, const int x, const int y, 
                            const int w, const int h, 
                            const unsigned int colour) {
	SDL_Rect r;
	assert(surface != NULL);
	assert(w >= 0); assert(h >= 0);
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	if (SDL_FillRect(surface, &r, colour) < 0) {
		fprintf(stderr, "warning: SDL_FillRect() failed: %s\n", SDL_GetError());
		exit(1);
	}
}

static void tile_init(Tile *tile) {
	tile->pixel_data = NULL;
	tile->surface_0 = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, 8, 0, 0, 0, 0);
	tile->surface_123 = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, 8, 0, 0, 0, 0);
	if ((tile->surface_0 == NULL) || (tile->surface_123 == NULL)) {
		fprintf(stderr, "surface creation failed: %s\n", SDL_GetError());
		exit(1);
	}
	SDL_SetColorKey(tile->surface_123, SDL_SRCCOLORKEY | SDL_RLEACCEL, 4);
}

static void tile_fini(Tile *tile) {
	SDL_FreeSurface(tile->surface_0);
	SDL_FreeSurface(tile->surface_123);
}

static void tile_regenerate(Tile *tile) {
	int x, y;
	Byte colour_code;
	fill_rectangle(tile->surface_0, 0, 0, 8, 8, 4);
	fill_rectangle(tile->surface_123, 0, 0, 8, 8, 4);
	for (y = 0; y < 8; y++) {
		for (x = 0; x < 8; x++) {
			colour_code  = (tile->pixel_data[y * 2] & (0x80  >> x)) >> (7 - x);
			colour_code |= (tile->pixel_data[(y * 2) + 1] & (0x80  >> x)) >> (7 - x) << 1;
			if (colour_code == 0)
			    put_pixel(tile->surface_0, x, y, colour_code);
			else
			    put_pixel(tile->surface_123, x, y, colour_code);
		}
	}
	tile->is_invalidated = 0;
}

static void tile_set_palette(Tile *tile, SDL_Palette *palette) {
	SDL_SetPalette(tile->surface_0, SDL_LOGPAL, palette->colors, 0, palette->ncolors);
	SDL_SetPalette(tile->surface_123, SDL_LOGPAL, palette->colors, 0, palette->ncolors);
}

static void tile_blit_0(Tile *tile, SDL_Surface *surface, const int x, const int y, const int line, const int w) {
	SDL_Rect src;
	SDL_Rect dest;
	if (tile->is_invalidated == 1)
		tile_regenerate(tile);
	src.x = 0;
	src.y = line;
	src.w = w;
	src.h = 1;
	dest.x = x;
	dest.y = y;

	SDL_BlitSurface(tile->surface_0, &src, surface, &dest);
}

static void tile_blit_123(Tile *tile, SDL_Surface *surface, const int x, const int y, const int line, const int w) {
	SDL_Rect dest;
	SDL_Rect src;
	if (tile->is_invalidated == 1)
		tile_regenerate(tile);
	src.x = 0;
	src.y = line;
	src.w = w;
	src.h = 1;


	dest.x = x;
	dest.y = y;

	SDL_BlitSurface(tile->surface_123, &src, surface, &dest);
}

static void sprite_init(Sprite *sprite) {
	int i;
	sprite->pixel_data = NULL;
	for (i = 0; i < 4; i++) {
		sprite->surface[i] = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, 8, 0, 0, 0, 0);
		if (sprite->surface[i] == NULL) {
			fprintf(stderr, "surface creation failed: %s\n", SDL_GetError());
			exit(1);
		}
		SDL_SetColorKey(sprite->surface[i], SDL_SRCCOLORKEY | SDL_RLEACCEL, 0);
		sprite->is_invalidated[i] = 1;
	}
	sprite->height = 8;
}

static void sprite_fini(Sprite *sprite) {
	int i;
	for (i = 0; i < 4; i++) {
		SDL_FreeSurface(sprite->surface[i]);
	}
}

void sprite_regenerate(Sprite *sprite, const int flip) {
	int x, y;
	Byte colour_code;
	Byte cache_x;
	Byte cache_y;
	fill_rectangle(sprite->surface[flip], 0, 0, 8, sprite->height, 0);
	for (y = 0; y < sprite->height; y++) {
		for (x = 0; x < 8; x++) {
			colour_code  = (sprite->pixel_data[y * 2] & (0x80  >> x)) >> (7 - x);
			colour_code |= (sprite->pixel_data[(y * 2) + 1] & (0x80  >> x)) >> (7 - x) << 1;
			cache_x = x;
			cache_y = y;
			if (flip & X_FLIP)
				cache_x = 7 - cache_x;
			if (flip & Y_FLIP)
				cache_y = (sprite->height - 1) - cache_y;
		    put_pixel(sprite->surface[flip], cache_x, cache_y, colour_code);
		}
	}
	sprite->is_invalidated[flip] = 0;
}

void sprite_set_palette(Sprite *sprite, SDL_Palette *palette) {
	int i;
	for (i = 0; i < 4; i++) {
		SDL_SetPalette(sprite->surface[i], SDL_LOGPAL, palette->colors, 0, palette->ncolors);
	}
}

void sprite_blit(Sprite *sprite, SDL_Surface* surface, const int x, const int y, const int line, const int flip) {
	SDL_Rect src;
	SDL_Rect dest;
	if (sprite->is_invalidated[flip] == 1)
		sprite_regenerate(sprite, flip);
	src.x = 0;
	src.y = line;
	src.w = 8;
	src.h = 1;
	dest.x = x;
	dest.y = y;

	SDL_BlitSurface(sprite->surface[flip], &src, surface, &dest);
}

void sprite_set_height(Sprite *sprite, int height) {
	int i;
	sprite->height = height;
	for (i = 0; i < 4; i++) {
		SDL_FreeSurface(sprite->surface[i]);
		sprite->surface[i] = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, height, 8, 0, 0, 0, 0);
		SDL_SetColorKey(sprite->surface[i], SDL_SRCCOLORKEY | SDL_RLEACCEL, 0);
		sprite->is_invalidated[i] = 1;
	}
}

void display_save(void) {
	save_uint("display.cycles", display.cycles);
	save_int("sheight", display.sprite_height);

	if ((console == CONSOLE_GBC) || (console == CONSOLE_GBA))
		save_memory("vram", display.vram, VRAM_SIZE_GBC);
	else
		save_memory("vram", display.vram, VRAM_SIZE_DMG);

	save_memory("oam", display.oam, SIZE_OAM);
	save_uint("vram_bank", display.vram_bank);
	
	save_uint("dma", display.is_hdma_active);
}

void display_load(void) {
	int i;
	
	display_reset();
	
	display.cycles = load_uint("display.cycles");
	display.sprite_height = load_int("sheight");
	if ((console == CONSOLE_GBC) || (console == CONSOLE_GBA))
		load_memory("vram", display.vram, VRAM_SIZE_GBC);
	else
		load_memory("vram", display.vram, VRAM_SIZE_DMG);
	display.vram_bank = load_uint("vram_bank");
	set_vector_block(MEM_VIDEO, display.vram + (display.vram_bank * 0x2000), SIZE_VIDEO);
	load_memory("oam", display.oam, SIZE_OAM);
	
	display.is_hdma_active = load_uint("dma");
	
	update_bg_palette();
	update_sprite_palette_0();
	update_sprite_palette_1();

	for (i = 0; i < display.cache_size; i++) {
		tile_invalidate(&display.tiles_tdt_0[i]);
	}
	for (i = 0; i < display.cache_size; i++) {
		display.tiles_tdt_1[i].pixel_data = display.vram + (i * 16) + 0x0800;
		tile_invalidate(&display.tiles_tdt_1[i]);
	}
	for (i = 0; i < display.cache_size; i++) {
		display.sprites[i].pixel_data = display.vram + (i * 16);
		sprite_invalidate(&display.sprites[i]);
		sprite_set_height(&display.sprites[i], display.sprite_height);
	}
}

