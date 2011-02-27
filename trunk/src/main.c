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
#include <locale.h>

#if !defined _WIN64 && !defined _WIN32
#include "config.h"
#else
#include "windows_config.h"
#endif

#include "memory.h"
#include "core.h"
#include "cart.h"
#include "timer.h"
#include "display.h"
#include "joypad.h"
#include "sound.h"
#include "debug.h"
#include "save.h"

#define TIMING_GRANULARITY	10000
#define TIMING_INTERVAL		(1000000000 / TIMING_GRANULARITY)
#define MAX_CPU_CYCLES		200

int console;
int console_mode;

extern CoreState core;

void reset(void);
void quit(void);
extern int debugging;
extern int sound_cycles;



int main(int argc, char *argv[]) {
	int i;
	unsigned int is_paused, is_sound_on;
	unsigned int cycles;
	unsigned int core_period;
	SDL_Event event;
	unsigned int core_time;
	unsigned int delay;
	int is_delayed;
	unsigned int real_time;
	unsigned int real_time_passed;
	unsigned int delays;
	unsigned int frame_time;
	bool turbo = false;

	printf("%s v%s\n", PACKAGE_NAME, PACKAGE_VERSION);
	if (argc != 2) {
		printf("Invalid arguments\n");
		return 1;
	}
#if 0
	if (argc == 3) {
		memory_init();
		load_rom(argv[2]);
		debug_init();
		disasm();
		unload_rom();
		memory_fini();
		exit(0);
	}
#endif

	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr,"sdl initialisation failed: %s\b", SDL_GetError());
		exit(1);
	}

	memory_init();
	console = CONSOLE_AUTO;
	//console = CONSOLE_DMG;
	//console_mode = MODE_DMG;
	load_rom(argv[1]);
	display_init();
	joypad_init();
	sound_init();
	debug_init();
	reset();
	is_paused = 0;
	is_sound_on = 1;
	cycles = 0;
	core_time = 0;
	delay = 1;
	is_delayed = 0;
	real_time = SDL_GetTicks() * 1000000;
	delays = 0;
	frame_time = SDL_GetTicks();
	// main loop
	// TODO intelligent algorithm for working out number of cycles to execute
	// based on interrupt predictions...
	
	while(1) {
		if ((!is_paused) && (!is_delayed)) {
			for (i = 0; i < 10; i++) {
				cycles = execute_cycles(40);

				core_period = (cycles >> 2) * (1000000000/(1048576));
				core_time += core_period;
				timer_check(cycles * core.frequency);
				display_update(cycles);
				sound_update();
			}
		}
		
		if (is_paused) 
			SDL_Delay(10);

		delays = core_time / TIMING_INTERVAL;
		if (delays > delay) {
			real_time_passed = ((SDL_GetTicks() * 1000000) - real_time);
			if ((core_time > real_time_passed) && (!turbo)) {
				if (core_time > real_time_passed + (2 * 1000000))
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

		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_QUIT:
					quit();
					exit(0);
					break;
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_d) {
						printf("d\n");
						debugging = !debugging;
					}
					if (event.key.keysym.sym == SDLK_p) {
						is_paused = !is_paused;
						if (is_paused)
							stop_sound();
						else
							start_sound();
					}
					if (event.key.keysym.sym == SDLK_s) {
						is_sound_on = !is_sound_on;
						if (is_sound_on)
							start_sound();
						else
							stop_sound();
					}
					if (event.key.keysym.sym == SDLK_r) {
						printf("reset\n");
						reset();
					}
					if (event.key.keysym.sym == SDLK_F1) {
						save_state();
					}
					if (event.key.keysym.sym == SDLK_F2) {
						load_state();
					}
					if(event.key.keysym.sym == SDLK_ESCAPE) {
						quit();
						exit(0);
					}
					if (event.key.keysym.sym == SDLK_LCTRL) {
						turbo = 1;
						is_delayed = 0;
						break;
					}
				case SDL_KEYUP:
					if (event.key.keysym.sym == SDLK_LCTRL) {
						turbo = 0;
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
void reset(void) {
	// the order in which these are called is important
	memory_reset();
	cart_reset();
	core_reset();
	display_reset();
	timer_reset();
	sound_reset();
}

void quit(void) {
	sound_fini();
	unload_rom();
	display_fini();
	memory_fini();
	SDL_Quit();
}

void new_frame(void) {
#if 0
	const unsigned fps = 60;		/* 59.72 but meh */
	const unsigned frame_us = (1000000 / fps);
	static Uint32 last_time;
	static int frame = 0;
	Uint32 time = SDL_GetTicks();
	if (frame == 0)
		last_time = SDL_GetTicks();
	if (((time - last_time) * 1000) < frame_us * (frame + 1)) {
		fprintf(stderr, "delaying: %ums\n", (frame_us * (frame + 1) - ((time - last_time) * 1000)) / 1000);
		SDL_Delay((frame_us * (frame + 1) - ((time - last_time) * 1000)) / 1000);
		
	}
	++frame;
	if (frame == fps)
		frame = 0;
#endif
}
