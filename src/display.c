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

static void draw_background(const Byte lcdc, const Byte ly);
static void draw_window(const Byte lcdc, const Byte ly);
static void draw_sprites(const Byte lcdc, const Byte ly);
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
static void scale_nearest_neighbor(SDL_Surface *src, SDL_Surface *dest);
static void tile_init(Tile *tile);
static void tile_fini(Tile *tile);
static void tile_regenerate(Tile *tile);
static void tile_set_palette(Tile *tile, SDL_Palette *palette);
static void tile_blit_0(Tile *tile, SDL_Surface *surface, const int x, 
    					const int y, const int line);
static void tile_blit_123(Tile *tile, SDL_Surface *surface, const int x, 
    					const int y, const int line);

static void sprite_init(Sprite *sprite);
static void sprite_fini(Sprite *sprite);
void sprite_regenerate(Sprite *sprite, const int flip);
void sprite_set_palette(Sprite *sprite, SDL_Palette *palette);
void sprite_blit(Sprite *sprite, SDL_Surface* surface, const int x, const int y, 
					const int line, const int flip);
void sprite_set_height(Sprite *sprite, int height);


Display display;

void display_init() {
	display.x_res = 320;
	display.y_res = 288;
	display.bpp = 32;

	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("sdl video initialisation failed: %s\b", SDL_GetError());
		exit(1);
	}
	//atexit(SDL_Quit);
	display.screen = SDL_SetVideoMode(display.x_res, display.y_res, display.bpp, SDL_SWSURFACE);
	if (display.screen == NULL) {
		//cerr << xRes_ << "x" << yRes_ << " video mode initialisation failed\n";
		printf("video mode initialisation failed\n");
		exit(1);
	}
	#ifdef WINDOWS
		// redirecting the standard input/output to the console 
		// is required with windows.
		activate_console(); 
	#endif // WINDOWS

	//cout << "sdl video initialised: " << xRes_ << "x" << yRes_
	//          << " (bpp: " << bpp_ << ")" << "\n";
	printf("sdl video initialised.\n");
	SDL_WM_SetCaption("gbem", "gbem");

	display.display = SDL_CreateRGBSurface(SDL_SWSURFACE, 
                                 DISPLAY_W, DISPLAY_H, display.bpp, 0, 0, 0, 0);	
	display.display = SDL_DisplayFormat(display.display);

	display.video_ram = malloc(sizeof(Byte) * SIZE_VIDEO);
	display.oam = malloc(sizeof(Byte) * SIZE_OAM);

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
	
	display.tiles_tdt_0 = malloc(sizeof(Tile) * TILES);
	display.tiles_tdt_1 = malloc(sizeof(Tile) * TILES);
	display.sprites = malloc(sizeof(Sprite) * SPRITES);

	for (int i = 0; i < TILES; i++) {
		tile_init(&display.tiles_tdt_0[i]);
	}
	for (int i = 0; i < TILES; i++) {
		tile_init(&display.tiles_tdt_1[i]);
	}
	for (int i = 0; i < SPRITES; i++) {
		sprite_init(&display.sprites[i]);
	}
	
	return;
}

void display_fini() {
	for (int i = 0; i < TILES; i++) {
		tile_fini(&display.tiles_tdt_0[i]);
	}
	for (int i = 0; i < TILES; i++) {
		tile_fini(&display.tiles_tdt_1[i]);
	}
	for (int i = 0; i < SPRITES; i++) {
		sprite_fini(&display.sprites[i]);
	}

	SDL_FreeSurface(display.display);
	free(display.video_ram);
	free(display.oam);
	free(display.background_palette.colors);
	free(display.sprite_palette[0].colors);
	free(display.sprite_palette[1].colors);
	free(display.tiles_tdt_0);
	free(display.tiles_tdt_1);
	free(display.sprites);
	SDL_Quit();
}


void display_reset() {
	bzero(display.video_ram, SIZE_VIDEO);
	bzero(display.oam, SIZE_OAM);
	set_vector_block(MEM_VIDEO, display.video_ram, SIZE_VIDEO);
	set_vector_block(MEM_OAM, display.oam, 0x100);
	display.last_lcdc = read_io(HWREG_LCDC);
	for (int i = 0; i < TILES; i++) {
		display.tiles_tdt_0[i].pixel_data = display.video_ram + (i * 16);
		tile_set_palette(&display.tiles_tdt_0[i], &display.background_palette);
		tile_invalidate(&display.tiles_tdt_0[i]);
	}
	for (int i = 0; i < TILES; i++) {
		display.tiles_tdt_1[i].pixel_data = display.video_ram + (i * 16) + 0x0800;
		tile_set_palette(&display.tiles_tdt_1[i], &display.background_palette);
		tile_invalidate(&display.tiles_tdt_1[i]);
	}
	for (int i = 0; i < SPRITES; i++) {
		display.sprites[i].pixel_data = display.video_ram + (i * 16);
		sprite_set_palette(&display.sprites[i], &display.background_palette);
		sprite_invalidate(&display.sprites[i]);
	}

	display.sprite_height = 8;
	display.cycles = 0;	
}


void display_update(unsigned int cycles) {
	Byte ly, stat, lcdc;
	cycles = cycles >> 2;
	display.cycles += cycles;
	ly = read_io(HWREG_LY);
	stat = read_io(HWREG_STAT);
	lcdc = read_io(HWREG_LCDC);
	if (ly == readb(HWREG_LYC)) {
		if (!(stat & STAT_FLAG_COINCIDENCE) 
                && (stat & STAT_INT_COINCIDENCE))
			// generate interrupt if enabled
			writeb(HWREG_IF, readb(HWREG_IF) | INT_STAT);
			stat |= STAT_FLAG_COINCIDENCE;	// set coincidence flag
	} else
	    stat &= ~(STAT_FLAG_COINCIDENCE);	// unset coincidence flag
	start:
	if (ly < DISPLAY_H) {
		// not in vblank
		if (display.cycles < 80) {
			// lcd controller is reading from oam memory
			if (((stat & STAT_MODES) != STAT_MODE_OAM) && (stat & STAT_INT_OAM))
				// generate interrupt if enabled
				writeb(HWREG_IF, readb(HWREG_IF) | INT_STAT);
			stat = (stat & (~STAT_MODES)) | STAT_MODE_OAM;
		} else if (display.cycles < 80 + 172) {
			// lcd controller is reading from oam memory and vram
			// FIXME does this go here?
			if (((stat & STAT_MODES) != STAT_MODE_OAM) && (stat & STAT_INT_OAM))
				// generate interrupt if enabled
				writeb(HWREG_IF, readb(HWREG_IF) | INT_STAT);
			stat = (stat & (~STAT_MODES)) | STAT_MODE_OAM_VRAM;
		} else if (display.cycles < 80 + 172 + 204) {
			// lcd controller in hblank
			if (((stat & STAT_MODES) != STAT_MODE_HBLANK)
                    && (stat & STAT_INT_HBLANK))
				// generate interrupt if enabled
			    writeb(HWREG_IF, readb(HWREG_IF) | INT_STAT);
			stat = (stat & (~STAT_MODES)) | STAT_MODE_HBLANK;
		} else {
			display.cycles -= (80 + 172 + 204);
		    if (lcdc & 0x01)
			    draw_background(lcdc, ly);
		    if (lcdc & 0x20)
			    draw_window(lcdc, ly);
			if (lcdc & 0x02)
			    draw_sprites(lcdc, ly);
			++ly;
			if (ly == readb(HWREG_LYC)) {
				if (!(stat & STAT_FLAG_COINCIDENCE) 
                        && (stat & STAT_INT_COINCIDENCE))
					// generate interrupt if enabled
    				writeb(HWREG_IF, readb(HWREG_IF) | INT_STAT);
					stat |= STAT_FLAG_COINCIDENCE;	// set coincidence flag
			} else
			    stat &= ~(STAT_FLAG_COINCIDENCE);	// unset coincidence flag
			goto start;
		}
	} else {
		// check if lcd controller is in vblank or if it has finished.
		if (ly == 154) {
			// vblank has just finished
			ly = 0;
			draw_frame();
			if (ly == readb(HWREG_LYC)) {
				if (!(stat & STAT_FLAG_COINCIDENCE) 
                        && (stat & STAT_INT_COINCIDENCE))
					// generate interrupt if enabled
					writeb(HWREG_IF, readb(HWREG_IF) | INT_STAT);
					stat |= STAT_FLAG_COINCIDENCE;	// set coincidence flag
			} else
			    stat &= ~(STAT_FLAG_COINCIDENCE);	// unset coincidence flag
			// before we begin redrawing the screen, sort out some things.
			// update sprite palettes - fairly ugly hack.
			for (int sprite = 0; sprite < OAM_BLOCKS; sprite++) {
				sprite_set_palette(&display.sprites[get_sprite_pattern(sprite)], 
				    &display.sprite_palette[(get_sprite_flags(sprite) 
					    & FLAG_PALETTE) >> 4]);
			}
			// update sprite height - another fairly ugly hack.
			// if spriteHeight has been changed from 8 to 16:
			if ((lcdc & 0x04) && (display.sprite_height == 8)) {
				display.sprite_height = 16;
				for (int i = 0; i < SPRITES; i++) {
					//sprites_[i]->setHeight(16);
					sprite_set_height(&display.sprites[i], 16);
				}
			}
			// if spriteHeight has been changed from 16 to 8:
			if ((!(lcdc & 0x04)) && (display.sprite_height == 16)) {
				display.sprite_height = 8;
				for (int i = 0; i < SPRITES; i++) {
					sprite_set_height(&display.sprites[i], 8);
				}
			}
			goto start;
		} else {
			// in vlbank
			if (((stat & STAT_MODES) != STAT_MODE_VBLANK) 
                    && (stat & STAT_INT_VBLANK)) {
				writeb(HWREG_IF, readb(HWREG_IF) | INT_STAT);
			}
			if (((stat & STAT_MODES) != STAT_MODE_VBLANK) 
					&& (readb(HWREG_IE) & INT_VBLANK)) {
				writeb(HWREG_IF, readb(HWREG_IF) | INT_VBLANK);
			}

			stat = (stat & (~STAT_MODES)) | STAT_MODE_VBLANK;
			if (display.cycles >= 456) {
				// although we're in vblank LY continues to increase.
				++ly;
				if (ly == readb(HWREG_LYC)) {
					if (!(stat & STAT_FLAG_COINCIDENCE) 
                            && (stat & STAT_INT_COINCIDENCE))
						// generate interrupt if enabled
						writeb(HWREG_IF, readb(HWREG_IF) | INT_STAT);
					stat |= STAT_FLAG_COINCIDENCE;		// set coincidence flag
					} else
			    		stat &= ~(STAT_FLAG_COINCIDENCE);	// unset coincidence flag
				display.cycles -= 456;
				goto start;
			}
		}
	}
	write_io(HWREG_LY, ly);
	write_io(HWREG_STAT, stat);
}


void draw_frame() {
	scale_nearest_neighbor(display.display, display.screen);
	//SDL_BlitSurface(display.display, NULL, display.screen, NULL);
	SDL_Flip(display.screen);
//	SDL_UpdateRect(display.screen, 0, 0, 0, 0);
}



static void draw_background(const Byte lcdc, const Byte ly) {
    Byte scx = read_io(HWREG_SCX);
    Byte scy = read_io(HWREG_SCY);
	Byte tile_code;
	Byte offset_x = scx & 0x07;
	Byte offset_y = (ly + scy) & 0x07;
	Byte bg_y = (ly + scy) & 0xFF;
	Byte tile_x;
	Byte tile_y;
	int screen_x;
	for (unsigned int x = scx; x <= (scx + (unsigned)DISPLAY_W); x += 8) {
		Byte bg_x = x & 0xFF;
		tile_x = (bg_x / 8);
		tile_y = (bg_y / 8);
		screen_x = scx / 8;
		if ((lcdc & 0x08) == 0) {
			// tile map is at 0x9800-0x9BFF
			tile_code = read_video_ram(TILE_MAP_0 + (tile_y * 32) + tile_x);
		} else {
			// tile map is at 0x9C00-0x9FFF
			tile_code = read_video_ram(TILE_MAP_1 + (tile_y * 32) + tile_x);
		}
		if ((lcdc & 0x10) == 0) {
			// tile data is at 0x8800-0x97FF (indeces signed)
			// complement upper bit
			tile_code ^= 0x80;
			tile_blit_0(&display.tiles_tdt_1[tile_code], display.display, x - scx - offset_x, ly, offset_y);
			tile_blit_123(&display.tiles_tdt_1[tile_code], display.display, x - scx - offset_x, ly, offset_y);
		} else {
			// tile data is at 0x8000-0x8FFF (indeces unsigned)
			tile_blit_0(&display.tiles_tdt_0[tile_code], display.display, x - scx - offset_x, ly, offset_y);
			tile_blit_123(&display.tiles_tdt_0[tile_code], display.display, x - scx - offset_x, ly, offset_y);
		}
	}
}

static void draw_window(const Byte lcdc, const Byte ly) {
    Byte wx = read_io(HWREG_WX);
    Byte wy = read_io(HWREG_WY);
	Byte tile_code;
	Byte offset_y = (ly - wy) & 0x07;
	Byte win_y = (ly - wy);
	Byte tile_x;
	Byte tile_y;
	int x;
	//int screen_x;
	if ((win_y >= DISPLAY_H) || (ly < wy))
		return;
	for (unsigned int win_x = 0; win_x <= 255; win_x += 8) {
		tile_x = (win_x / 8);
		tile_y = (win_y / 8);
		x = win_x + wx - (signed)7;
		if (x >= DISPLAY_W)
			return;
        if ((lcdc & 0x40) == 0) {
            // tile map is at 0x9800-0x9BFF
			tile_code = read_video_ram(TILE_MAP_0 + (tile_y * 32) + tile_x);
        } else {
            // tile map is at 0x9C00-0x9FFF
			tile_code = read_video_ram(TILE_MAP_1 + (tile_y * 32) + tile_x);
        }
        if ((lcdc & 0x10) == 0) {
		    // tile data is at 0x8800-0x97FF (indeces signed)
			// complement upper bit
			tile_code ^= 0x80;
			tile_blit_0(&display.tiles_tdt_1[tile_code], display.display, x, ly, offset_y);
			tile_blit_123(&display.tiles_tdt_1[tile_code], display.display, x, ly, offset_y);
	    } else {
		    // tile data is at 0x8000-0x8FFF (indeces unsigned)
			//tilesTdt0_[tileCode]->blit0(display_, x, ly, offsetY);
			//tilesTdt0_[tileCode]->blit123(display_, x, ly, offsetY);
			tile_blit_0(&display.tiles_tdt_1[tile_code], display.display, x, ly, offset_y);
			tile_blit_123(&display.tiles_tdt_1[tile_code], display.display, x, ly, offset_y);
		}
	}
}


void draw_sprites(const Byte lcdc, const Byte ly) {
	int sprite_x;
	int sprite_y;
	int offset_y;
	for (int sprite = 0; sprite < OAM_BLOCKS; sprite++) {
		sprite_y = get_sprite_y(sprite) - (signed)16;
		sprite_x = get_sprite_x(sprite) - (signed)8;
		if ((ly >= sprite_y) && (ly < (sprite_y + display.sprite_height))) {
			offset_y = (signed)ly - sprite_y;
			sprite_blit(&display.sprites[get_sprite_pattern(sprite)], display.display, sprite_x, ly, offset_y, (get_sprite_flags(sprite) & 0x60) >> 5);
			//sprites_[getSpritePattern(sprite)]->blit(display_, spriteX, ly, offsetY, (getSpriteFlags(sprite) & 0x60) >> 5);
		}
	}
}


void update_bg_palette() {
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

	for (int i = 0; i < TILES; i++) {
		tile_set_palette(&display.tiles_tdt_0[i], &display.background_palette);
	}
	for (int i = 0; i < TILES; i++) {
		tile_set_palette(&display.tiles_tdt_1[i], &display.background_palette);
	}
}

void update_sprite_palette_0() {
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
}

void update_sprite_palette_1() {
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

static void scale_nearest_neighbor(SDL_Surface *src, SDL_Surface *dest) {
	double w_ratio = src->w / (double)dest->w;
	double h_ratio = src->h / (double)dest->h;
	double px, py;
	int pixel;

	for (int y = 0; y < dest->h; y++) {
		for (int x = 0; x < dest->w; x++) {
			px = floor(x * w_ratio);
			py = floor(y * h_ratio);
			pixel = get_pixel(src, (int)px, (int)py);
			put_pixel(dest, (int)x, (int)y, pixel);
		}
	}
	return;
}


void launch_dma(Byte address) {
	Word real_address = address * 0x100;
	for (unsigned int i = 0; i < SIZE_OAM; i++) {
		display.oam[i] = readb(real_address + i);
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
	//unsigned int bpp = 4;
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
	assert(surface != NULL);
	assert(w >= 0); assert(h >= 0);
	SDL_Rect r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	if (SDL_FillRect(surface, &r, colour) < 0) {
		printf("warning: SDL_FillRect() failed: %s\n", SDL_GetError());
		exit(1);
	}
}

static void tile_init(Tile *tile) {
	tile->pixel_data = NULL;
	tile->surface_0 = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, 8, 0, 0, 0, 0);
	tile->surface_123 = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, 8, 0, 0, 0, 0);
	if ((tile->surface_0 == NULL) || (tile->surface_123 == NULL)) {
		printf("surface creation failed: %s\n", SDL_GetError());
		exit(1);
	}
	SDL_SetColorKey(tile->surface_123, SDL_SRCCOLORKEY | SDL_RLEACCEL, 4);
}

static void tile_fini(Tile *tile) {
	SDL_FreeSurface(tile->surface_0);
	SDL_FreeSurface(tile->surface_123);
}

static void tile_regenerate(Tile *tile) {
	Byte colour_code;
	fill_rectangle(tile->surface_0, 0, 0, 8, 8, 4);
	fill_rectangle(tile->surface_123, 0, 0, 8, 8, 4);
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
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

static void tile_blit_0(Tile *tile, SDL_Surface *surface, const int x, const int y, 
					const int line) {
	if (tile->is_invalidated == 1)
		tile_regenerate(tile);
	SDL_Rect src;
	src.x = 0;
	src.y = line;
	src.w = 8;
	src.h = 1;

	SDL_Rect dest;
	dest.x = x;
	dest.y = y;

	SDL_BlitSurface(tile->surface_0, &src, surface, &dest);
}

static void tile_blit_123(Tile *tile, SDL_Surface *surface, const int x, const int y, 
					const int line) {
	if (tile->is_invalidated == 1)
		tile_regenerate(tile);
	SDL_Rect src;
	src.x = 0;
	src.y = line;
	src.w = 8;
	src.h = 1;

	SDL_Rect dest;
	dest.x = x;
	dest.y = y;

	SDL_BlitSurface(tile->surface_123, &src, surface, &dest);
}

static void sprite_init(Sprite *sprite) {
	sprite->pixel_data = NULL;
	for (int i = 0; i < 4; i++) {
		sprite->surface[i] = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, 8, 0, 0, 0, 0);
		if (sprite->surface[i] == NULL) {
			printf("surface creation failed: %s\n", SDL_GetError());
			exit(1);
		}
		SDL_SetColorKey(sprite->surface[i], SDL_SRCCOLORKEY | SDL_RLEACCEL, 0);
		sprite->is_invalidated[i] = 1;
	}
	sprite->height = 8;
}

static void sprite_fini(Sprite *sprite) {
	for (int i = 0; i < 4; i++) {
		SDL_FreeSurface(sprite->surface[i]);
	}
}

void sprite_regenerate(Sprite *sprite, const int flip) {
	Byte colour_code;
	Byte cache_x;
	Byte cache_y;
	fill_rectangle(sprite->surface[flip], 0, 0, 8, sprite->height, 0);
	for (int y = 0; y < sprite->height; y++) {
		for (int x = 0; x < 8; x++) {
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
	for (int i = 0; i < 4; i++) {
		SDL_SetPalette(sprite->surface[i], SDL_LOGPAL, palette->colors, 0, palette->ncolors);
	}
}

void sprite_blit(Sprite *sprite, SDL_Surface* surface, const int x, const int y, 
					const int line, const int flip) {
	if (sprite->is_invalidated[flip] == 1)
		sprite_regenerate(sprite, flip);
	SDL_Rect src;
	src.x = 0;
	src.y = line;
	src.w = 8;
	src.h = 1;

	SDL_Rect dest;
	dest.x = x;
	dest.y = y;

	SDL_BlitSurface(sprite->surface[flip], &src, surface, &dest);
}


void sprite_set_height(Sprite *sprite, int height) {
	sprite->height = height;
	for (int i = 0; i < 4; i++) {
		SDL_FreeSurface(sprite->surface[i]);
		sprite->surface[i] = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, height, 8, 0, 0, 0, 0);
		SDL_SetColorKey(sprite->surface[i], SDL_SRCCOLORKEY | SDL_RLEACCEL, 0);
		sprite->is_invalidated[i] = 1;
	}
}
