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
#include "joypad.h"
#include "memory.h"

static SDLKey key_binds[8];
static int is_pressed[8];


void joypad_init(void) {
	int i;
	// defaults
	key_binds[BUTTON_RIGHT] 	= SDLK_RIGHT;
	key_binds[BUTTON_LEFT] 		= SDLK_LEFT;
	key_binds[BUTTON_UP] 		= SDLK_UP;
	key_binds[BUTTON_DOWN] 		= SDLK_DOWN;
	key_binds[BUTTON_A] 		= SDLK_x;
	key_binds[BUTTON_B] 		= SDLK_z;
	key_binds[BUTTON_SELECT]	= SDLK_BACKSLASH;
	key_binds[BUTTON_START]		= SDLK_SPACE;

	for (i = 0; i < NUM_BUTTONS; i++) {
		is_pressed[i] = 0;
	}
	
}

void key_event(SDL_KeyboardEvent* event) {
	int i;
	for (i = 0; i < NUM_BUTTONS; i++) {
		if (event->keysym.sym == key_binds[i]) {
			if (event->type == SDL_KEYDOWN)
				is_pressed[i] = 1;
			else
				is_pressed[i] = 0;
		}
	}
}

void update_p1(void) {
	Byte p1 = read_io(HWREG_P1) | 0x0F;		// set all buttons as unpressed
	// a 0 bit represents a button being pressed.
	// the following code unsets appropriate bits.
	if ((p1 & 0x10) == 0) {			// p14 (direction buttons)
		if (is_pressed[BUTTON_RIGHT])
			p1 &= ~0x01;
		if (is_pressed[BUTTON_LEFT])
			p1 &= ~0x02;
		if (is_pressed[BUTTON_UP])
			p1 &= ~0x04;
		if (is_pressed[BUTTON_DOWN])
			p1 &= ~0x08;
	}
	if ((p1 & 0x20) == 0) {			// p15 (other buttons)
		if (is_pressed[BUTTON_A])
			p1 &= ~0x01;
		if (is_pressed[BUTTON_B])
			p1 &= ~0x02;
		if (is_pressed[BUTTON_SELECT])
			p1 &= ~0x04;
		if (is_pressed[BUTTON_START])
			p1 &= ~0x08;
	}
	write_io(HWREG_P1, p1);
}