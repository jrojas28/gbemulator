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
#include <math.h>
#include <assert.h>
#include <string.h>
#include <SDL/SDL.h>
#include "gbem.h"
#include "sound.h"
#include "memory.h"
#include "save.h"
#include "blip_buf.h"

static short *lfsr_7;
static short *lfsr_15;
double sample_rate = 44100;
static blip_t* blip_left;
static blip_t* blip_right;

static SoundData sound;

static inline void mark_channel_on(unsigned int channel);
static inline void mark_channel_off(unsigned int channel);
static void callback(void* data, Uint8 *stream, int len);

static inline void update_channel1(int clocks);
static inline void update_channel2(int clocks);
static inline void update_channel3(int clocks);
static void sweep_freq();

static const unsigned char dmg_wave[] = {
	0xac, 0xdd, 0xda, 0x48, 0x36, 0x02, 0xcf, 0x16, 
	0x2c, 0x04, 0xe5, 0x2c, 0xac, 0xdd, 0xda, 0x48
};

static const unsigned char gbc_wave[] = {
	0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 
	0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff
};

bool sound_enabled;

#define MAX_SAMPLE			32767
#define MIN_SAMPLE			-32767

#define HIGH				(MAX_SAMPLE / 4)
#define LOW					(MIN_SAMPLE / 4)
#define	GRND				0
#define LFSR_7_SIZE			127
#define LFSR_15_SIZE		32767
#define LFSR_7				0
#define LFSR_15				1

int sound_cycles;


extern int console;
extern int console_mode;

void sound_init(void) {
	unsigned char r7;
	unsigned short r15;
	int i;
	SDL_AudioSpec desired;
	
	lfsr_7 = malloc(LFSR_7_SIZE * sizeof(short));
	lfsr_15 = malloc(LFSR_15_SIZE * sizeof(short));

	/* initialise 7 bit LFSR values */
	r7 = 0xff;
	for (i = 0; i < LFSR_7_SIZE; i++) {
		r7 >>= 1;
		r7 |= (((r7 & 0x02) >> 1) ^ (r7 & 0x01)) << 7;
		if (r7 & 0x01)
			lfsr_7[i] = HIGH;
		else
			lfsr_7[i] = LOW;
	}

	/* initialise 15 bit LFSR values */
	r15 = 0xffff;
	for (i = 0; i < LFSR_15_SIZE; i++) {
		r15 >>= 1;
		r15 |= (((r15 & 0x0002) >> 1) ^ (r15 & 0x0001)) << 15;
		if (r15 & 0x0001)
			lfsr_15[i] = HIGH;
		else
			lfsr_15[i] = LOW;
	}
	
	sound.channel3.wave.samples = malloc(32 * sizeof(short));

	desired.freq = sample_rate;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = 1024;
	desired.callback = callback;
	desired.userdata = NULL;
	
	if (SDL_OpenAudio(&desired, NULL) < 0) {
		fprintf(stderr, "couldn't initialise SDL audio: %s\n", SDL_GetError());
		exit(1);
   	}
	fprintf(stdout, "sdl audio initialised.\n");
	
	blip_left = blip_new(sample_rate / 10);
	blip_set_rates(blip_left, 4194304, sample_rate);

	blip_right = blip_new(sample_rate / 10);
	blip_set_rates(blip_right, 4194304, sample_rate);

	start_sound();

#if 0
	err = Pa_Initialize();
	if (err != paNoError) {
		Pa_Terminate();
		fprintf(stderr, "could not initialise portaudio: %s\n", Pa_GetErrorText(err));
		exit(1);
	}

	output_parameters.device = Pa_GetDefaultOutputDevice();
	//pdi = Pa_GetDeviceInfo(output_parameters.device);
	output_parameters.channelCount = 2;
	output_parameters.hostApiSpecificStreamInfo = NULL;
	output_parameters.sampleFormat = paInt8;
	output_parameters.suggestedLatency = Pa_GetDeviceInfo(output_parameters.device)->defaultLowOutputLatency;

	err = Pa_OpenStream(&stream,
						NULL, /* no input */
						&output_parameters,
						sample_rate,
						paFramesPerBufferUnspecified,
						paClipOff,      /* we won't output out of range samples so don't bother clipping them */
						call_back,
						&sound);

	if (err != paNoError) {
		Pa_Terminate();
		fprintf(stderr, "could not open output stream: %s\n", Pa_GetErrorText(err));
		exit(1);
	} else {
		stream_info = Pa_GetStreamInfo(stream);
		printf("portaudio stream opened\n");
		printf("\toutput latency: %fms\n", stream_info->outputLatency * 1000);
		printf("\tsample rate: %f\n", stream_info->sampleRate);
		if ((stream_info->outputLatency * 1000) > 150) {
			printf("warning: high sound output latency\n");
		}
	}
#endif

}

void sound_fini(void) {
//	PaError err;
	if (sound_enabled == 1) {
		stop_sound();
	}
	
/*
	err = Pa_CloseStream(stream);
	Pa_Terminate();
*/
	SDL_CloseAudio();
	free(lfsr_7);
	free(lfsr_15);
}

void stop_sound(void) {
//	PaError err;
	assert(sound_enabled == 1);
	SDL_PauseAudio(1);
	//err = Pa_StopStream(stream);
	sound_enabled = 0;
}

void start_sound(void) {
//	PaError err;
	assert(sound_enabled == 0);
	SDL_PauseAudio(0);
/*
	err = Pa_StartStream(stream);
	if (err != paNoError) {
		Pa_Terminate();
		fprintf(stderr, "could not start output stream: %s\n", Pa_GetErrorText(err));
		exit(1);
	}
*/
	sound_enabled = 1;
}

void sound_reset(void) {
	int i;
	write_io(HWREG_NR10, 0x80);
	write_io(HWREG_NR11, 0xbf);
	write_io(HWREG_NR12, 0xf3);
	write_io(HWREG_NR13, 0xff);
	write_io(HWREG_NR14, 0xbf);
	/* write_io(HWREG_NR20, 0xff); */
	write_io(HWREG_NR21, 0x3f);
	write_io(HWREG_NR22, 0x00);
	write_io(HWREG_NR23, 0xff);
	write_io(HWREG_NR24, 0xbf);
	write_io(HWREG_NR30, 0x7f);
	write_io(HWREG_NR31, 0xff);
	write_io(HWREG_NR32, 0x9f);
	write_io(HWREG_NR33, 0xff);
	write_io(HWREG_NR34, 0xbf);
	/* write_io(HWREG_NR40, 0xff); */
	write_io(HWREG_NR41, 0xff);
	write_io(HWREG_NR42, 0x00);
	write_io(HWREG_NR43, 0x00);
	write_io(HWREG_NR44, 0xbf);
	write_io(HWREG_NR50, 0x77);
	write_io(HWREG_NR51, 0xf3);
	
	if (console == CONSOLE_SGB)
		write_io(HWREG_NR52, 0xf0);
	else
		write_io(HWREG_NR52, 0xf1);
	
	memset(&sound.channel1, 0, sizeof(sound.channel1));
	memset(&sound.channel2, 0, sizeof(sound.channel2));
	//memset(&sound.channel3, 0, sizeof(sound.channel3));

	sound.channel1.last_delta_right = 0;
	sound.channel1.last_delta_left = 0;
	
	sound.channel2.last_delta_right = 0;
	sound.channel2.last_delta_left = 0;

	//sound.channel1.envelope.
	
/*
	sound.channel_1.is_on = 0;
	sound.channel_2.is_on = 0;
	sound.channel_3.is_on = 0;
	sound.channel_4.is_on = 0;
*/

	if ((console == CONSOLE_GBC) || (console == CONSOLE_GBA)) {
		for (i = 0; i < 16; i++)
			write_wave(0xff30 + i, gbc_wave[i]);
	} else {
		for (i = 0; i < 16; i++)
			write_wave(0xff30 + i, dmg_wave[i]);
	}

	if (!sound_enabled) {
		start_sound();
	}
	
	sound_cycles = 0;
}

void write_sound(Word address, Byte value) {
	unsigned freq;
	sound_update();
	switch (address) {
		case HWREG_NR10:	/* channel 1 sweep */
			sound.channel1.sweep.time = (value >> 4) & 0x07;
			//sound.channel1.sweep.time_counter = sound.channel1.sweep.time;
			sound.channel1.sweep.is_decreasing = (value >> 3) & 0x01;
			sound.channel1.sweep.shift_number = value & 0x07;
			break;
		case HWREG_NR11: 	/* channel 1 length / cycle duty */
			sound.channel1.length.length = 64 - (value & 0x3f);
			sound.channel1.duty.duty = value >> 6;
			break;
		case HWREG_NR12:	/* channel 1 envelope */
			sound.channel1.envelope.length = value & 0x07;
			sound.channel1.envelope.is_increasing = value & 0x08;
			sound.channel1.envelope.volume = value >> 4;
 			break;
		case HWREG_NR13:	/* channel 1 frequency lo */
			freq = (sound.channel1.freq & 0x700) | value;
			sound.channel1.freq = freq;
			sound.channel1.period = 2048 - freq;
			break;
		case HWREG_NR14:	/* frequency hi, init, counter selection */
			freq = (sound.channel1.freq & 0xff) | ((value & 0x07) << 8);
			sound.channel1.freq = freq;
			sound.channel1.period = 2048 - freq;
			sound.channel1.is_continuous = value & 0x40 ? 0 : 1;
			/* trigger? */
			if (value & 0x80) {
/*
				if ((sound.channel1.length_counter != 0) && (sound.channel1.is_on))
					sound.channel1.envelope.is_zombie = 1;
				else
					sound.channel1.envelope.is_zombie = 0;
*/				
				sound.channel1.envelope.volume = read_io(HWREG_NR12) >> 4;
				sound.channel1.is_on = 1;
				sound.channel1.envelope.length_counter = sound.channel1.envelope.length;
				/* sweep init */
				sound.channel1.sweep.hidden_freq = sound.channel1.freq;
				sound.channel1.sweep.time_counter = sound.channel1.sweep.time;
				/* if length is not reloaded, maximum length is played */
				if (sound.channel1.length.length == 0)
					sound.channel1.length.length = 2047;
				mark_channel_on(1);
				if (sound.channel1.sweep.time && sound.channel1.sweep.shift_number)
					sweep_freq();
			}
			break;
		case HWREG_NR21: 	/* channel 2 length / cycle duty */
			sound.channel2.length.length = 64 - (value & 0x3f);
			sound.channel2.duty.duty = value >> 6;
			break;
		case HWREG_NR22:	/* channel 2 envelope */
			sound.channel2.envelope.length = value & 0x07;
			sound.channel2.envelope.is_increasing = value & 0x08;
			sound.channel2.envelope.volume = value >> 4;
 			break;
		case HWREG_NR23:	/* channel 2 frequency lo */
			freq = (sound.channel2.freq & 0x700) | value;
			sound.channel2.freq = freq;
			sound.channel2.period = 2048 - freq;
			break;
		case HWREG_NR24:	/* channel 2 frequency hi, init, counter selection */
			freq = (sound.channel2.freq & 0xff) | ((value & 0x07) << 8);
			sound.channel2.freq = freq;
			sound.channel2.period = 2048 - freq;
			sound.channel2.is_continuous = value & 0x40 ? 0 : 1;
			/* trigger? */
			if (value & 0x80) {
				sound.channel2.envelope.volume = read_io(HWREG_NR22) >> 4;
				sound.channel2.is_on = 1;
				sound.channel2.envelope.length_counter = sound.channel2.envelope.length;
				/* if length is not reloaded, maximum length is played */
				if (sound.channel2.length.length == 0)
					sound.channel2.length.length = 2047;
				mark_channel_on(2);
			}
			break;
		case HWREG_NR30: 	/* channel 3 length / cycle duty */
			sound.channel3.is_on = (value & 0x80) ? 1 : 0;
			break;
		case HWREG_NR31: 	/* channel 3 length */
			sound.channel3.length.length = 256 - value; /* FIXME should be more */
			break;
		case HWREG_NR32:	/* channel 3 level */
			sound.channel3.volume = ((value >> 5) & 0x03) - 1;
			if (sound.channel3.volume == -1)
				sound.channel3.volume = 16;
 			break;
		case HWREG_NR33:	/* channel 3 frequency lo */
			freq = (sound.channel3.freq & 0x700) | value;
			sound.channel3.freq = freq;
			sound.channel3.period = (2048 - freq) << 1;
			break;
		case HWREG_NR34:	/* channel 3 frequency hi, init, counter selection */
			freq = (sound.channel3.freq & 0xff) | ((value & 0x07) << 8);
			sound.channel3.freq = freq;
			sound.channel3.period = (2048 - freq) << 1;
			sound.channel3.is_continuous = value & 0x40 ? 0 : 1;
			/* trigger? */
			if (value & 0x80) {
				sound.channel3.is_on = 1;
				/* if length is not reloaded, maximum length is played */
				if (sound.channel3.length.length == 0)
					sound.channel3.length.length = 2047;
				mark_channel_on(3);
			}
			break;

		case HWREG_NR50:	/* L/R volume control */
			sound.right_level = value & 0x07;
			sound.left_level = value >> 4;
			break;
		case HWREG_NR52:	/* sound on/off */
			sound.is_on = (value & 0x80) ? 1 : 0;
			break;
		case HWREG_NR51:	/* output terminal selection */
			//sound.channel_4.is_on_right = (value & 0x80) ? 1 : 0;
			sound.channel3.is_on_right = (value & 0x40) ? 1 : 0;
			sound.channel2.is_on_right = (value & 0x20) ? 1 : 0;
			sound.channel1.is_on_right = (value & 0x10) ? 1 : 0;
			//sound.channel_4.is_on_left = (value & 0x08) ? 1 : 0;
			sound.channel3.is_on_left = (value & 0x04) ? 1 : 0;
			sound.channel2.is_on_left = (value & 0x02) ? 1 : 0;
			sound.channel1.is_on_left = (value & 0x01) ? 1 : 0;
			break;
		default:
			break;
	}
}

/*
 * updates the wave pattern cache when the gb's wave pattern memory is modified
 * highest nibble is played first
 */
void write_wave(Word address, Byte value) {
	const short scale = ((HIGH * 2) / 15);
	sound.channel3.wave.samples[(address - 0xff30) * 2] = ((value >> 4) - 7) * scale;
	sound.channel3.wave.samples[(address - 0xff30) * 2 + 1] = ((value & 0x0f) - 7) * scale;
}


/* updates NR52 with the channel status (ON) */
static inline void mark_channel_on(unsigned int channel) {
	write_io(HWREG_NR52, read_io(HWREG_NR52) | (0x01 << (channel - 1)));
}

/* updates NR52 with the channel status (OFF) */
static inline void mark_channel_off(unsigned int channel) {
	write_io(HWREG_NR52, read_io(HWREG_NR52) & ~(0x01 << (channel - 1)));
}


/* The gameboy sound clock is 4.194304Mhz / 32 = 131072Hz
 * this clock is the same even in double speed mode.
 */

unsigned total_time = 0;

void sound_update() {
	if (sound_cycles == 0)
		return;

	SDL_LockAudio();

	/* channel 1 */
	update_channel1(sound_cycles);
	update_channel2(sound_cycles);
	update_channel3(sound_cycles);

	blip_end_frame(blip_left, sound_cycles);
	blip_end_frame(blip_right, sound_cycles);

	total_time += sound_cycles;
	sound_cycles = 0;
	SDL_UnlockAudio();
}



enum Side {
	LEFT,
	RIGHT
};

static void add_delta(enum Side side, unsigned t, short amp, short *last_delta);
static inline bool inside_buffer(blip_t* b, unsigned t);

static inline bool inside_buffer(blip_t* b, unsigned t) {
	
}

static void add_delta(enum Side side, unsigned t, short amp, short *last_delta) {
	blip_t* b;
	if (side == LEFT) {
		b = blip_left;
		amp = (amp / 7) * sound.left_level;
	} else {
		b = blip_right;
		amp = (amp / 7) * sound.right_level;
	}
	blip_add_delta(b, t, amp - *last_delta);
	*last_delta = amp;
}

static void clock_square(SquareChannel *sq, int t) {
	const int duty_wave_high[4] = {16, 16, 8, 24};
	const int duty_wave_low[4] = {20, 24, 24, 16};

	if ((sq->period != 0) && (sq->period != 2048)) {
		sq->period_counter = sq->period;
		sq->duty.i = (sq->duty.i + 1) & 0x1f;
		if (sq->duty.i == duty_wave_high[sq->duty.duty]) {
			if (sq->is_on_left)
				add_delta(LEFT, t, (HIGH / 15) * sq->envelope.volume, &sq->last_delta_left);
			if (sound.channel1.is_on_right)
				add_delta(RIGHT, t, (HIGH / 15) * sq->envelope.volume, &sq->last_delta_right);
		}
		else
		if (sq->duty.i == duty_wave_low[sq->duty.duty]) {
			if (sq->is_on_left)
				add_delta(LEFT, t, (LOW / 15) * sq->envelope.volume, &sq->last_delta_left);
			if (sq->is_on_right)
				add_delta(RIGHT, t, (LOW / 15) * sq->envelope.volume, &sq->last_delta_right);
		}
	}	
}
/*
static void clock_length(Length *l, unsigned ch) {
	if (!sound.channel1.is_continuous) {
		--sound.channel1.length.length;
		if (sound.channel1.length.length == 0) {
			mark_channel_off(1);
			sound.channel1.is_on = 0;
		}
	}
}
*/

static inline void update_channel1(int clocks) {
	unsigned i;
	if (!sound.channel1.is_on)
		return;
	for (i = 0; i < clocks; i++) {
		/* wave generation */
		//fprintf(stderr, "%u ", sound.channel1.period_counter);
 		if (sound.channel1.period_counter == 0) {
			clock_square(&sound.channel1, i);
		} else
			--sound.channel1.period_counter;
		/* length counter */
		++sound.channel1.length.i;
		if (sound.channel1.length.i == 16384) {
			sound.channel1.length.i = 0;
			if (!sound.channel1.is_continuous) {
				--sound.channel1.length.length;
				if (sound.channel1.length.length == 0) {
					mark_channel_off(1);
					sound.channel1.is_on = 0;
				}
			}
		}
		/* envelope */
		++sound.channel1.envelope.i;
		if (sound.channel1.envelope.i == 65536) {
			sound.channel1.envelope.i = 0;
			if (sound.channel1.envelope.length != 0) {
				--sound.channel1.envelope.length_counter;
				if (sound.channel1.envelope.length_counter == 0) {
					sound.channel1.envelope.length_counter = sound.channel1.envelope.length;
					if (sound.channel1.envelope.is_increasing) {
						if (sound.channel1.envelope.volume != 15)
							++sound.channel1.envelope.volume;
					} else {
						if (sound.channel1.envelope.volume != 0)
							--sound.channel1.envelope.volume;
						else
							sound.channel1.envelope.is_zombie = 1;
					}
				}
			}
		}
		/* sweep */
		++sound.channel1.sweep.i;
		if (sound.channel1.sweep.i == 32768) {
			sound.channel1.sweep.i = 0;
			if (sound.channel1.sweep.time != 0) {
				if (sound.channel1.sweep.time_counter == 0) {
					sound.channel1.sweep.time_counter = sound.channel1.sweep.time;
					if (sound.channel1.sweep.time == 0) {
						sound.channel1.is_on = 0;
						//mark_channel_off(1); /* FIXME: put these in initial check */
					} else {
						sweep_freq();
					}
				} else
					--sound.channel1.sweep.time_counter;
			}
		}
	}
}


static inline void update_channel2(int clocks) {
	unsigned i;
	if (!sound.channel2.is_on)
		return;
	for (i = 0; i < clocks; i++) {
		/* wave generation */
		if (sound.channel2.period_counter == 0) {
			clock_square(&sound.channel2, i);
		} else
			--sound.channel2.period_counter;
		/* length counter */
		++sound.channel2.length.i;
		if (sound.channel2.length.i == 16384) {
			sound.channel2.length.i = 0;
			if (!sound.channel2.is_continuous) {
				--sound.channel2.length.length;
				if (sound.channel2.length.length == 0) {
					mark_channel_off(2);
					sound.channel2.is_on = 0;
				}
			}
		}
		/* envelope */
		++sound.channel2.envelope.i;
		if (sound.channel2.envelope.i == 65536) {
			sound.channel2.envelope.i = 0;
			if (sound.channel2.envelope.length != 0) {
				--sound.channel2.envelope.length_counter;
				if (sound.channel2.envelope.length_counter == 0) {
					sound.channel2.envelope.length_counter = sound.channel2.envelope.length;
					if (sound.channel2.envelope.is_increasing) {
						if (sound.channel2.envelope.volume != 15)
							++sound.channel2.envelope.volume;
					} else {
						if (sound.channel2.envelope.volume != 0)
							--sound.channel2.envelope.volume;
						else
							sound.channel2.envelope.is_zombie = 1;
					}
				}
			}
		}
	}
}


static inline void update_channel3(int clocks) {
	unsigned i;
	if (!sound.channel3.is_on)
		return;
	for (i = 0; i < clocks; i++) {
		/* wave generation */
		if (sound.channel3.period_counter == 0) {
			if ((sound.channel3.period != 0) && (sound.channel3.period != 2048)) {
				sound.channel3.period_counter = sound.channel3.period;
				sound.channel3.wave.i = (sound.channel3.wave.i + 1) & 0x1f;
				if (sound.channel3.is_on_left)
					add_delta(LEFT, i, sound.channel3.wave.samples[sound.channel3.wave.i] >> sound.channel3.volume, &sound.channel3.last_delta_left);
				if (sound.channel3.is_on_right)
					add_delta(RIGHT, i, sound.channel3.wave.samples[sound.channel3.wave.i] >> sound.channel3.volume, &sound.channel3.last_delta_right);
			}
		} else
			--sound.channel3.period_counter;
		/* length counter */
		++sound.channel3.length.i;
		if (sound.channel3.length.i == 16384) {
			sound.channel3.length.i = 0;
			if (!sound.channel3.is_continuous) {
				--sound.channel3.length.length;
				if (sound.channel3.length.length == 0) {
					mark_channel_off(3);
					sound.channel3.is_on = 0;
				}
			}
		}
	}
}


static void sweep_freq() {
	sound.channel1.freq = sound.channel1.sweep.hidden_freq;
	if (sound.channel1.freq == 0)
		sound.channel1.period = 0;
	else
		sound.channel1.period = 2048 - sound.channel1.freq;
	
	if (!sound.channel1.sweep.is_decreasing) {
		sound.channel1.sweep.hidden_freq += (sound.channel1.sweep.hidden_freq >> sound.channel1.sweep.shift_number);
		if (sound.channel1.sweep.hidden_freq > 2047) {
			sound.channel1.is_on = 0;
			sound.channel1.sweep.hidden_freq = 2048;
		}
	} else {
		sound.channel1.sweep.hidden_freq -= (sound.channel1.sweep.hidden_freq >> sound.channel1.sweep.shift_number);
		if (sound.channel1.sweep.hidden_freq > 2047) {
			sound.channel1.sweep.hidden_freq = 0; /* ? */
			sound.channel1.is_on = 0;
		}
	}
}

void sound_save(void) {
	
	
}

void sound_load(void) {
	
}


static void callback(void* data, Uint8 *stream, int len) {
	Sint16 *buffer = (Sint16 *)stream;
		
	blip_read_samples(blip_left, buffer, len / 4, 1);
	blip_read_samples(blip_right, buffer + 1, len / 4, 1);
	
	/*
	long period = sample_rate / freq;
	for (i = 0; i < (len / 2); i += 2) {
		if (wave_index < (period / 2)) {
			buffer[i] = 10000;
			buffer[i + 1] = 10000;
		}
		else {
			buffer[i] = -10000;
			buffer[i + 1] = -10000;
		}
			
		++wave_index;
		if (wave_index == period)
			wave_index = 0;
	}
	*/
}
