/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) abhoriel 2010 <abhoriel@gmail.com>
 * 
 * gbem is free software copyrighted by abhoriel.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name ``abhoriel'' nor the name of any other
 *    contributor may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * gbem IS PROVIDED BY abhoriel ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL abhoriel OR ANY OTHER CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>

#include <SDL/SDL.h>

#include "config.h"
#include "memory.h"
#include "core.h"
#include "cart.h"
#include "timer.h"
#include "display.h"
#include "joypad.h"

#define TIMING_GRANULARITY	50
#define TIMING_INTERVAL		(1000000000 / TIMING_GRANULARITY)
#define MAX_CPU_CYCLES		200

void reset();
void quit();

int main (int argc, char *argv[]) {
	printf("%s v%s\n", PACKAGE_NAME, PACKAGE_VERSION);
	if (argc != 2) {
		printf("Invalid arguments\n");
		return 1;
	}
	//Debugger debugger(&core);

	memory_init();
	load_rom(argv[1]);
	display_init();
	joypad_init();
	reset();
	
	unsigned int cycles = 0;
	unsigned int core_period;				// in nanoseconds
	SDL_Event event;
	unsigned int core_time = 0;				
	unsigned int delay = 1;
	int is_delayed = 0;
	unsigned int real_time = SDL_GetTicks() * 1000000;
	unsigned int real_time_passed;
	unsigned int delays = 0;
	unsigned int frame_time = SDL_GetTicks();
	// main loop
	while(1) {
		if (!is_delayed) {
			cycles = execute_cycles(MAX_CPU_CYCLES);
			core_period = (cycles >> 2) * (1000000000/4194304); // FIXME
			core_time += core_period;
			timer_check(core_period);
			display_update(cycles);
		}
		
		// 50 FPS
		if ((SDL_GetTicks() - frame_time) >= 20) {
				//draw_frame();
			frame_time = SDL_GetTicks();
		}

/*
		delays = core_time / TIMING_INTERVAL;
		if (delays > delay) {
			real_time_passed = ((SDL_GetTicks() * 1000000) - real_time);
			if (core_time > real_time_passed) {
				is_delayed = 1;
			} else {
				delay = delays;
				is_delayed = 0;	
			}
			if (delay >= TIMING_GRANULARITY) {
				delay = 1;
				core_time = 0;
				real_time = SDL_GetTicks() * 1000000;
			}
		}
*/


		delays = core_time / TIMING_INTERVAL;
		if (delays > delay) {
			real_time_passed = ((SDL_GetTicks() * 1000000) - real_time);
			if (core_time > real_time_passed) {
				if (core_time > real_time_passed + 1 * 1000000)
					SDL_Delay(1);
				is_delayed = 1;
			} else {
				delay = delays;
				is_delayed = 0;
			}
			if (delay >= TIMING_GRANULARITY) {
				delay = 1;
				core_time = 0;
				real_time = SDL_GetTicks() * 1000000;
			}
		}


/*
		// this part of the loop is slow, so we dont do it every time
		if ((core_time / 20000000) == delay) {
			real_time_passed = ((SDL_GetTicks() * 1000000) - real_time);
			printf("%u\n", ((core_time - real_time_passed) / 1000000) * 5);
			if (core_time > real_time_passed) {
				SDL_Delay((core_time - real_time_passed) / 1000000);
			}
			++delay;
			if (delay == 51) {
				delay = 1;
				core_time = 0;
				real_time = SDL_GetTicks() * 1000000;
			}
		}
*/
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_QUIT:
					quit();
					exit(0);
					break;
				case SDL_KEYDOWN:
				case SDL_KEYUP:
					if(event.key.keysym.sym == SDLK_ESCAPE) {
						quit();
						exit(0);
					}
					key_event(&event.key);
					break;
				default:
					break;
			}
		}
	}
	return 0;
}


// reset the emulator. must be called before ROM execution.
void reset() {
	// the order in which these are called is important
	memory_reset();
	cart_reset();
	core_reset();
	display_reset();
	timer_reset();
}

void quit() {
	unload_rom();
	display_fini();
	memory_fini();
}
