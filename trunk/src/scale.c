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

#include <SDL/SDL.h>
#include <assert.h>
#include <stdint.h>
#include "scale.h"
 
static inline Uint32 get_pixel(const SDL_Surface *surface, const int x, 
			const int y);
static inline void put_pixel(const SDL_Surface *surface, const int x, const int y, 
                        const Uint32 pixel);

/* adjust as necessary ! */
const unsigned int bpp = 4;

void scale_nn(SDL_Surface* restrict src, SDL_Surface* restrict dest) {
	int px, py, y, x;
	int pixel;
	for (y = 0; y < dest->h; y++) {
		for (x = 0; x < dest->w; x++) {
			px = x * src->w / dest->w;
			py = y * src->h / dest->h;
			pixel = get_pixel(src, px, py);
			put_pixel(dest, x, y, pixel);
		}
	}
	return;
}

/*
void scale_nn2x(SDL_Surface* restrict src, SDL_Surface* restrict dest) {
	int pixel;
	int px, py;
	for (int y = 0; y < src->h; y++) {
		py = y * 2;
		for (int x = 0; x < src->w; x++) {
			px = x * 2;
			pixel = get_pixel(src, x, y);
			put_pixel(dest, px + 0, py + 0, pixel);
			put_pixel(dest, px + 1, py + 0, pixel);
			put_pixel(dest, px + 0, py + 1, pixel);
			put_pixel(dest, px + 1, py + 1, pixel);
		}
	}
}
*/

/* fast? nearest neighbour 2x upscaling */
void scale_nn2x(SDL_Surface* restrict src, SDL_Surface* restrict dest) {
	Uint8 * restrict p_src;
	Uint8 * restrict p_dest;
	Uint8 * restrict p_dest2;
	Uint32 pixel;
	int y, x;
	for (y = 0; y < src->h; y++) {
		p_src = (Uint8 *)src->pixels + (src->pitch * y);
		p_dest = (Uint8 *)dest->pixels + (dest->pitch * y * 2);
		p_dest2 = (Uint8 *)dest->pixels + dest->pitch * ((y * 2) + 1);
		for (x = 0; x < src->w; x++) {
			pixel = *(Uint32 *)p_src;
			*(Uint32 *)(p_dest) = pixel;
			*(Uint32 *)(p_dest + bpp) = pixel;
			*(Uint32 *)(p_dest2) = pixel;
			*(Uint32 *)(p_dest2 + bpp) = pixel;
			p_src += bpp;
			p_dest += bpp * 2;
			p_dest2 += bpp * 2;
		}
	}
}


static inline Uint32 get_pixel(const SDL_Surface* restrict surface, const int x, const int y) {
	return *(Uint32 *)((Uint8 *)surface->pixels + (y * surface->pitch) + (x * 4));
}

static inline void put_pixel(const SDL_Surface* restrict surface, const int x, const int y, const Uint32 pixel) {
	*(Uint32 *)((Uint8 *)surface->pixels + (y * surface->pitch) + (x * 4)) = pixel;
}



