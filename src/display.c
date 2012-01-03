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


#define	ALL		-1

static void draw_scan_line(Byte ly);
static void clear_scan_line();
static void draw_background(const Byte lcdc, const Byte ly);
static void draw_gbc_background(const Byte lcdc, const Byte ly);
static void draw_window(const Byte lcdc, const Byte ly);
static void draw_gbc_window(const Byte lcdc, const Byte ly);
static void launch_hdma(int length);
static void draw_sprites(const Byte lcdc, const Byte ly);
static void draw_gbc_sprites(const Byte lcdc, const Byte ly);
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

static void tile_init(Tile *t, Byte* vram_px, Tile *next);
static void tile_fini(Tile *t);
static void tile_regenerate(Tile *t, const int flip);
static void tile_blit(Tile *t, const int x, const int line, const int flip, const int pal, const int priority);
static void sprite_blit(Tile *t, const int x, const int line, const int flip, const int pal, const int priority, const int h);

static Colour map_rgb(uint8_t r, uint8_t g, uint8_t b);
static Colour translate_gbc_rgb(uint8_t r, uint8_t g, uint8_t b);

enum { TILE_PALETTE 	= 0x07 };
enum { TILE_VRAM_BANK 	= 0x08 };
enum { TILE_X_FLIP 		= 0x20 };
enum { TILE_Y_FLIP 		= 0x40 };
enum { TILE_PRIORITY 	= 0x80 };

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

	display.mono_colours[0] = map_rgb(0xff, 0xff, 0xff);
	display.mono_colours[1] = map_rgb(0xaa, 0xaa, 0xaa);
	display.mono_colours[2] = map_rgb(0x55, 0x55, 0x55);
	display.mono_colours[3] = map_rgb(0x00, 0x00, 0x00);
	
	display.gbc_bg_pal_mem = malloc(64 * sizeof(Byte));
	display.gbc_spr_pal_mem = malloc(64 * sizeof(Byte));
	
	display.scan_line = malloc(DISPLAY_W * sizeof(Byte));
	
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
	SDL_FreeSurface(display.display);
	if (display.vram != NULL)
		free(display.vram);
	if (display.oam != NULL)
		free(display.oam);
	free(display.tiles_tdt_0);
	free(display.tiles_tdt_1);
	
	free(display.gbc_bg_pal_mem);
	free(display.gbc_spr_pal_mem);
	free(display.scan_line);
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
	
	for (i = 0; i < display.cache_size; i++) {
		tile_init(&display.tiles_tdt_0[i], display.vram + ((i % 256) * 16) + ((i / 256) * 0x2000), &display.tiles_tdt_0[i + 1]);
		tile_init(&display.tiles_tdt_1[i], display.vram + ((i % 256) * 16) + 0x0800 + ((i / 256) * 0x2000), &display.tiles_tdt_1[i + 1]);
	}

	/* FIXME for non gbc mode only */
	update_bg_palette(0, read_io(HWREG_BGP));
	update_sprite_palette(0, read_io(HWREG_OBP0));
	update_sprite_palette(1, read_io(HWREG_OBP1));
	memset(display.gbc_bg_pal_mem, 0xff, 64);
	memset(display.gbc_spr_pal_mem, 0xff, 64);
	
	
	for (i = 0; i < display.cache_size; i++) {
		tile_dirty(&display.tiles_tdt_0[i]);
		tile_dirty(&display.tiles_tdt_1[i]);
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
	/* if lcd is being turned on/off set ly to 0 and blank the screen */
	if ((value & 0x80) != (read_io(HWREG_LCDC) & 0x80)) {
		write_io(HWREG_LY, 0);
		SDL_FillRect(display.display, NULL, SDL_MapRGB(display.display->format, 0xff, 0xff, 0xff));
	}

	write_io(HWREG_LCDC, value);
}

/* FIXME: if lots of cycles have passed, modes could be skipped! (is this still true?) */
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
						draw_gbc_background(lcdc, ly);
					else
						clear_scan_line();
					if (lcdc & 0x20)
						draw_gbc_window(lcdc, ly);
					if (lcdc & 0x02)
						draw_gbc_sprites(lcdc, ly);
				} else {
					if (lcdc & 0x01)
						draw_background(lcdc, ly);
					else
						clear_scan_line();
					if (lcdc & 0x20)
						draw_window(lcdc, ly);
					if (lcdc & 0x02)
						draw_sprites(lcdc, ly);
				}
				draw_scan_line(ly);
			}
		/* has the lcd finished hblank? */
		} else {
			/* if running, launch hdma */
			if (display.is_hdma_active) {
				launch_hdma(1);
				hdma_length = read_io(HWREG_HDMA5) & 0x7f;
				//fprintf(stderr, "%hhu", hdma_length);
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
				if (lcdc & 0x04)
					display.sprite_height = 16;
				else
					display.sprite_height = 8;
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

static void draw_scan_line(Byte ly) {
	int i;
	Byte code;
	for (i = 0; i < DISPLAY_W; i++) {
		code = display.scan_line[i];
		if (code & 0x20)
			put_pixel(display.display, i, ly, display.spr_pal[(code >> 2) & 0x07].colour[code & 0x03]);
		else
			put_pixel(display.display, i, ly, display.bg_pal[(code >> 2) & 0x07].colour[code & 0x03]);
	}
}

static void clear_scan_line() {
	memset(display.scan_line, 0x00, DISPLAY_W);
}

void draw_frame() {
	scale_nn4x(display.display, display.screen);
	SDL_Flip(display.screen);
}

static void draw_background(const Byte lcdc, const Byte ly) {
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
		/* dont draw over the window! */
		if ((lcdc & 0x20) && (x_pos + 7 >= wx) && (ly >= wy))
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
			tile_blit(&display.tiles_tdt_1[tile_code], x_pos, offset_y, NO_FLIP, 0, PRIORITY_LOW);
		} else {
			// tile data is at 0x8000-0x8FFF (indeces unsigned)
			tile_blit(&display.tiles_tdt_0[tile_code], x_pos, offset_y, NO_FLIP, 0, PRIORITY_LOW);
		}
	}
}

static void draw_gbc_background(const Byte lcdc, const Byte ly) {
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
			tile_blit(&display.tiles_tdt_1[tile_code], x_pos, offset_y, (attrib >> 5) & 0x03, attrib & 0x07, PRIORITY_LOW);

		} else {
			if (attrib & TILE_VRAM_BANK)
				tile_code += 256;
			tile_blit(&display.tiles_tdt_0[tile_code], x_pos, offset_y, (attrib >> 5) & 0x03, attrib & 0x07, PRIORITY_LOW);
		}
	}
}

static void draw_window(const Byte lcdc, const Byte ly) {
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
			tile_blit(&display.tiles_tdt_1[tile_code], x, offset_y, NO_FLIP, 0, PRIORITY_LOW);
	    } else {
		    // tile data is at 0x8000-0x8FFF (indeces unsigned)
			tile_blit(&display.tiles_tdt_0[tile_code], x, offset_y, NO_FLIP, 0, PRIORITY_LOW);
		}
	}
}

static void draw_gbc_window(const Byte lcdc, const Byte ly) {
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
			attrib = display.vram[TILE_MAP_1 - MEM_VIDEO + VRAM_BANK_SIZE + (tile_y * 32) + tile_x];
        }
        if ((lcdc & 0x10) == 0) {
		    // tile data is at 0x8800-0x97FF (indeces signed)
			// complement upper bit
			tile_code ^= 0x80;
       		if (attrib & TILE_VRAM_BANK)
				tile_code += 256;
			tile_blit(&display.tiles_tdt_1[tile_code], x, offset_y, NO_FLIP, attrib & 0x07, PRIORITY_LOW);
		} else {
			// tile data is at 0x8000-0x8FFF (indeces unsigned)
			if (attrib & TILE_VRAM_BANK)
				tile_code += 256;
			tile_blit(&display.tiles_tdt_0[tile_code], x, offset_y, NO_FLIP, attrib & 0x07, PRIORITY_LOW);
		}
	}
}

/* TODO optimise? */
static void draw_sprites(const Byte lcdc, const Byte ly) {
	int sprite_x;
	int sprite_y;
	int offset_y;
	int i;
	Byte priority;
	for (i = OAM_BLOCKS - 1; i >= 0; i--) {
		sprite_y = get_sprite_y(i) - (signed)16;
		sprite_x = get_sprite_x(i) - (signed)8;
		priority = get_sprite_flags(i) >> 7;
		if ((ly >= sprite_y) && (ly < (sprite_y + display.sprite_height))) {
			offset_y = (signed)ly - sprite_y;
			sprite_blit(&display.tiles_tdt_0[get_sprite_pattern(i)], sprite_x, offset_y, (get_sprite_flags(i) & 0x60) >> 5, (get_sprite_flags(i) >> 4) & 0x01, priority, display.sprite_height);
		}
	}
}

static void draw_gbc_sprites(const Byte lcdc, const Byte ly) {
	int sprite_x;
	int sprite_y;
	int offset_y;
	int i;
	Byte priority;
	int tile_code;
	for (i = OAM_BLOCKS - 1; i >= 0; i--) {
		sprite_y = get_sprite_y(i) - (signed)16;
		sprite_x = get_sprite_x(i) - (signed)8;
		priority = get_sprite_flags(i) >> 7;
		if ((ly >= sprite_y) && (ly < (sprite_y + display.sprite_height))) {
			offset_y = (signed)ly - sprite_y;
			tile_code = get_sprite_pattern(i);
			if (get_sprite_flags(i) & 0x08)
				tile_code += 256;
			sprite_blit(&display.tiles_tdt_0[tile_code], sprite_x, offset_y, (get_sprite_flags(i) & 0x60) >> 5, get_sprite_flags(i) & 0x07, priority, display.sprite_height);
		}
	}
}


void update_bg_palette(unsigned n, Byte p) {
	display.bg_pal[n].colour[0] = display.mono_colours[p & 0x03];
	display.bg_pal[n].colour[1] = display.mono_colours[(p >> 2) & 0x03];
	display.bg_pal[n].colour[2] = display.mono_colours[(p >> 4) & 0x03];
	display.bg_pal[n].colour[3] = display.mono_colours[(p >> 6) & 0x03];
}

void update_sprite_palette(unsigned n, Byte p) {
	// colour 0 is transparent anyway.
	display.spr_pal[n].colour[0] = display.mono_colours[p & 0x03];
	display.spr_pal[n].colour[1] = display.mono_colours[(p >> 2) & 0x03];
	display.spr_pal[n].colour[2] = display.mono_colours[(p >> 4) & 0x03];
	display.spr_pal[n].colour[3] = display.mono_colours[(p >> 6) & 0x03];
}

void update_gbc_bg_palette(Byte value) {
	Byte bgpi, index;
	int pal, col, byte1, byte2;
	Byte r, g, b;
	
	bgpi = read_io(HWREG_BGPI);
	index = bgpi & 0x3f;
	pal = index / 8;
	col = (index % 8) / 2;

	display.gbc_bg_pal_mem[index] = value;

	byte1 = display.gbc_bg_pal_mem[(pal * 8) + col * 2];
	byte2 = display.gbc_bg_pal_mem[(pal * 8) + (col * 2) + 1];
	r = byte1 & 0x1f;
	g = ((byte1 >> 5) & 0x07) | ((byte2 & 0x03) << 3);
	b = (byte2 >> 2) & 0x1f;
	display.bg_pal[pal].colour[col] = translate_gbc_rgb(r, g, b);
	
	/* autoincrement? */
	if (bgpi & 0x80)
		write_io(HWREG_BGPI, ((index + 1) & 0x3f) | 0x80);
}

void update_gbc_spr_palette(Byte value) {
	Byte obpi, index;
	int pal, col, byte1, byte2;
	Byte r, g, b;
	
	obpi = read_io(HWREG_OBPI);
	index = obpi & 0x3f;
	pal = index / 8;
	col = (index % 8) / 2;

	display.gbc_spr_pal_mem[index] = value;

	byte1 = display.gbc_spr_pal_mem[(pal * 8) + col * 2];
	byte2 = display.gbc_spr_pal_mem[(pal * 8) + (col * 2) + 1];
	r = byte1 & 0x1f;
	g = ((byte1 >> 5) & 0x07) | ((byte2 & 0x03) << 3);
	b = (byte2 >> 2) & 0x1f;
	display.spr_pal[pal].colour[col] = translate_gbc_rgb(r, g, b);
	
	/* autoincrement? */
	if (obpi & 0x80)
		write_io(HWREG_OBPI, ((index + 1) & 0x3f) | 0x80);
}

static Colour translate_gbc_rgb(uint8_t r, uint8_t g, uint8_t b) {
	/* FIXME this simple translation produces far too vibrant colours */
	return map_rgb(r * 8, g * 8, b * 8);
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
	
	//fprintf(stderr, "src: %hx. dest: %hx. length: %i:\n", src, dest, length);
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
		//fprintf(stderr, "hblank dma: %hhx ", read_io(HWREG_HDMA5));
		write_io(HWREG_HDMA5, read_io(HWREG_HDMA5) & (~0x80));
		display.is_hdma_active = 1;
	} else {
		if (display.is_hdma_active) {
			display.is_hdma_active = 0;
			write_io(HWREG_HDMA5, 0xFF);
			return;
		}
		/* general dma */
		//fprintf(stderr, "general dma: hdma5: %hhx\n", hdma5);
		//fprintf(stderr, "general dma ");
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
	Uint8 *p = (Uint8*)surface->pixels + y * surface->pitch + x * 4;
	*(Uint32 *)p = pixel;

}
#if 0
static inline void put_pixel(const SDL_Surface *surface, const int x, const int y, 
                        const Uint32 pixel) {
	//assert (surface != NULL);
	assert (x < surface->w); assert (x >= 0);
	assert (y < surface->h); assert (y >= 0);
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
#endif

static inline Colour map_rgb(uint8_t r, uint8_t g, uint8_t b) {
#if WORDS_BIGENDIAN
	return b | (g << 8) | (r << 16);
#else
	return b | (g << 8) | (r << 16);
#endif
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

static void tile_init(Tile *t, Byte* vram_px, Tile *next) {
	int i;
	t->vram_px = vram_px;
	t->next = next;
	for (i = 0; i < 4; i++) {
		//t->is_dirty[i] = 1;
		t->cache_px[i] = NULL;
	}
}

static void tile_fini(Tile *t) {
	int i;
	for (i = 0; i < 4; i++) {
		//t->is_dirty[i] = 1;
		if (t->cache_px[i] != NULL) {
			free(t->cache_px[i]);
			t->cache_px[i] = NULL;
		}
	}
}

static void tile_regenerate(Tile *t, const int flip) {
	int x, y;
	Byte colour;
	Byte cache_x;
	Byte cache_y;
	//fill_rectangle(sprite->surface[flip], 0, 0, 8, sprite->height, 0);
	assert(t->cache_px[flip] == NULL);
	t->cache_px[flip] = malloc(8 * 8 * sizeof(Byte));
	for (y = 0; y < 8; y++) {
		for (x = 0; x < 8; x++) {
			colour  = (t->vram_px[y * 2] & (0x80  >> x)) >> (7 - x);
			colour |= (t->vram_px[(y * 2) + 1] & (0x80  >> x)) >> (7 - x) << 1;
			cache_x = x;
			cache_y = y;
			if (flip & X_FLIP)
				cache_x = 7 - cache_x;
			if (flip & Y_FLIP)
				cache_y = (8 - 1) - cache_y;
		  //put_pixel(sprite->surface[flip], cache_x, cache_y, colour_code);
			t->cache_px[flip][cache_y * 8 + cache_x] = colour;
		}
	}
	//sprite->is_invalidated[flip] = 0;
}

static void tile_blit(Tile *t, const int x, const int line, const int flip, const int pal, const int priority) {
	int i = 0;
	Byte colour_code;
	Byte data;
	int w = 8;
	if (x < 0) {
		if (x < -7)
			return;
		i = -x;
	} else if (x + w >= DISPLAY_W) {
		if (x + w >= DISPLAY_W + 8)
			return;
		w = 8 - (x + w - DISPLAY_W);
	}

	if (t->cache_px[flip] == NULL)
		tile_regenerate(t, flip);

	data = 0 | (pal << 2) | (priority << 6);

	for (; i < w; i++) {
		colour_code = t->cache_px[flip][line * 8 + i];
		//if ((line * 8 + i) > 63) {
		//	fprintf(stderr, "%i, line = %i, i = %i, x = %i\n", (line * 8 + i), line, i, x);
		//}
		display.scan_line[x + i] = colour_code | data;
		//put_pixel(s, x + i, y, display.bg_pal[pal].colour[colour_code]);
	}
}

static void sprite_blit(Tile *t, const int x, int line, const int flip, const int pal, const int priority, const int h) {
	int i = 0;
	Byte colour_code;
	Byte data;
	Byte current_px;
	int w = 8;

	/* when drawing 8x16 sprites, we need to flip the two 8x8 tiles round */
	if (h == 16) {
		if (flip & Y_FLIP) {
			if (line < 8) {
				line += 8;
			} else {
				line -= 8;
			}
		}
	}
	if (line > 7) {
		sprite_blit(t->next, x, line - 8, flip, pal, priority, 8);
		return;
	}
	if (x < 0) {
		if (x < -7)
			return;
		i = -x;
	} else if (x + w >= DISPLAY_W) {
		if (x + w >= DISPLAY_W + 8)
			return;
		w = 8 - (x + w - DISPLAY_W);
	}

	if (t->cache_px[flip] == NULL)
		tile_regenerate(t, flip);

	data = 0 | (pal << 2) | 0x20;

	for (; i < w; i++) {
		colour_code = t->cache_px[flip][line * 8 + i];
		current_px = display.scan_line[x + i];
		if (current_px & 0x40)
			continue;
		if (colour_code != 0) {
			if (priority) {
				if (!(current_px & 0x20) && ((current_px & 0x03) == 0))
					display.scan_line[x + i] = colour_code | data;
			} else {
				display.scan_line[x + i] = colour_code | data;
			}
		}
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
	
	update_bg_palette(0, read_io(HWREG_BGP));
	update_sprite_palette(0, read_io(HWREG_OBP0));
	update_sprite_palette(1, read_io(HWREG_OBP1));

	for (i = 0; i < display.cache_size; i++) {
		tile_dirty(&display.tiles_tdt_0[i]);
	}
	for (i = 0; i < display.cache_size; i++) {
		//display.tiles_tdt_1[i].vram_px = display.vram + (i * 16) + 0x0800;
		tile_dirty(&display.tiles_tdt_1[i]);
	}
}

