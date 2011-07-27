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

#define MAX_SAMPLE			32767
#define MIN_SAMPLE			-32767
#define HIGH				(MAX_SAMPLE / 4)
#define LOW					(MIN_SAMPLE / 4)
#define	GRND				0
#define LFSR_7_SIZE			128
#define LFSR_15_SIZE		32768
#define LFSR_15				0
#define LFSR_7				1

enum Side { LEFT, RIGHT };
enum Counter { PERIOD, LENGTH, ENVELOPE, SWEEP };

static inline void mark_channel_on(unsigned int channel);
static inline void mark_channel_off(unsigned int channel);
static void callback(void* data, Uint8 *stream, int len);
static inline void update_channel1(int clocks);
static inline void update_channel2(int clocks);
static inline void update_channel3(int clocks);
static inline void update_channel4(int clocks);
static void clock_square(SquareChannel *sq, int t);
static void clock_sample(SampleChannel *sc, int t);
static void clock_sweep(SquareChannel *sq);
static void clock_lfsr(NoiseChannel *ns, int t);
static void clock_length(Length *l, unsigned ch);
static void clock_envelope(Envelope *e);
static void clock_sweep(SquareChannel *sq);
static int get_soonest_clock(unsigned a, unsigned b, unsigned c, unsigned d, unsigned *clocks);
static void sweep_freq();
static void add_delta(int side, unsigned t, short amp, short *last_delta);

static const unsigned char dmg_wave[] = {
	0xac, 0xdd, 0xda, 0x48, 0x36, 0x02, 0xcf, 0x16, 
	0x2c, 0x04, 0xe5, 0x2c, 0xac, 0xdd, 0xda, 0x48
};

static const unsigned char gbc_wave[] = {
	0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 
	0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff
};

int sound_enabled;
int sound_cycles;

static short *lfsr[2];
static unsigned lfsr_size[2];
static short* wave_samples;
static int sample_rate = 44100;
static blip_t* blip_left;
static blip_t* blip_right;
static SoundData sound;
static SDL_mutex *sound_mutex;

extern int console;
extern int console_mode;

void sound_init(void) {
	unsigned char r7;
	unsigned short r15;
	unsigned int i;
	SDL_AudioSpec desired, actual;
	
	lfsr_size[LFSR_7] = LFSR_7_SIZE;
	lfsr_size[LFSR_15] = LFSR_15_SIZE;

	lfsr[LFSR_7] = malloc(lfsr_size[LFSR_7] * sizeof(short));
	lfsr[LFSR_15] = malloc(lfsr_size[LFSR_15] * sizeof(short));

	/* initialise 7 bit LFSR values */
	r7 = 0xff;
	for (i = 0; i < lfsr_size[LFSR_7]; i++) {
		r7 >>= 1;
		r7 |= (((r7 & 0x02) >> 1) ^ (r7 & 0x01)) << 7;
		if (r7 & 0x01)
			lfsr[LFSR_7][i] = HIGH / 15;
		else
			lfsr[LFSR_7][i] = LOW / 15;
	}

	/* initialise 15 bit LFSR values */
	r15 = 0xffff;
	for (i = 0; i < lfsr_size[LFSR_15]; i++) {
		r15 >>= 1;
		r15 |= (((r15 & 0x0002) >> 1) ^ (r15 & 0x0001)) << 15;
		if (r15 & 0x0001)
			lfsr[LFSR_15][i] = HIGH / 15;
		else
			lfsr[LFSR_15][i] = LOW / 15;
	}
	
	wave_samples = malloc(32 * sizeof(short));

	SDL_InitSubSystem(SDL_INIT_AUDIO);

	desired.freq = sample_rate;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = 2048;
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

    sound_mutex = SDL_CreateMutex();

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
	free(lfsr[LFSR_7]);
	free(lfsr[LFSR_15]);
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
	memset(&sound.channel3, 0, sizeof(sound.channel3));
	memset(&sound.channel4, 0, sizeof(sound.channel4));

	sound.channel1.period_counter = 1;
	sound.channel1.length.i = 16384;
	sound.channel1.envelope.i = 65536;
	sound.channel1.sweep.i = 32768;

	sound.channel2.period_counter = 1;
	sound.channel2.length.i = 16384;
	sound.channel2.envelope.i = 65536;

	sound.channel3.period_counter = 1;
	sound.channel3.length.i = 16384;

	sound.channel4.period_counter = 1;
	sound.channel4.length.i = 16384;
	sound.channel4.envelope.i = 65536;


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
	//sound_update();
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
			sound.channel1.length.is_continuous = value & 0x40 ? 0 : 1;
			/* trigger? */
			if (value & 0x80) {
/*
				if ((sound.channel1.length_counter != 0) && (sound.channel1.is_on))
					sound.channel1.envelope.is_zombie = 1;
				else
					sound.channel1.envelope.is_zombie = 0;
*/				
				sound.channel1.envelope.volume = read_io(HWREG_NR12) >> 4;
				sound.channel1.length.is_on = 1;
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
			sound.channel2.length.is_continuous = value & 0x40 ? 0 : 1;
			/* trigger? */
			if (value & 0x80) {
				sound.channel2.envelope.volume = read_io(HWREG_NR22) >> 4;
				sound.channel2.length.is_on = 1;
				sound.channel2.envelope.length_counter = sound.channel2.envelope.length;
				/* if length is not reloaded, maximum length is played */
				if (sound.channel2.length.length == 0)
					sound.channel2.length.length = 2047;
				mark_channel_on(2);
			}
			break;
		case HWREG_NR30: 	/* channel 3 length / cycle duty */
			sound.channel3.length.is_on = (value & 0x80) ? 1 : 0;
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
			sound.channel3.length.is_continuous = value & 0x40 ? 0 : 1;
			/* trigger? */
			if (value & 0x80) {
				sound.channel3.length.is_on = 1;
				/* if length is not reloaded, maximum length is played */
				if (sound.channel3.length.length == 0)
					sound.channel3.length.length = 2047;
				mark_channel_on(3);
			}
			break;
		case HWREG_NR41: 	/* channel 4 length */
			sound.channel4.length.length = 64 - (value & 0x3f);
			break;
		case HWREG_NR42:	/* channel 4 envelope */
			sound.channel4.envelope.length = value & 0x07;
			sound.channel4.envelope.is_increasing = value & 0x08;
			sound.channel4.envelope.volume = value >> 4;
 			break;
		case HWREG_NR43:	/* channel 4 poly counter */
			sound.channel4.period = 4 * ((value & 0x07) + 1) << (((value >> 4) & 0x0f) + 1);
			sound.channel4.lfsr.size = (value >> 3) & 0x01;
			break;
		case HWREG_NR44:	/* channel 4 init, counter selection */
			sound.channel4.length.is_continuous = value & 0x40 ? 0 : 1;
			/* trigger? */
			if (value & 0x80) {
				sound.channel4.envelope.volume = read_io(HWREG_NR42) >> 4;
				sound.channel4.length.is_on = 1;
				sound.channel4.envelope.length_counter = sound.channel4.envelope.length;
				/* if length is not reloaded, maximum length is played */
				if (sound.channel4.length.length == 0)
					sound.channel4.length.length = 2047;
				mark_channel_on(4);
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
			sound.channel4.is_on_right = (value & 0x80) ? 1 : 0;
			sound.channel3.is_on_right = (value & 0x40) ? 1 : 0;
			sound.channel2.is_on_right = (value & 0x20) ? 1 : 0;
			sound.channel1.is_on_right = (value & 0x10) ? 1 : 0;
			sound.channel4.is_on_left = (value & 0x08) ? 1 : 0;
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
	wave_samples[(address - 0xff30) * 2] = ((value >> 4) - 7) * scale;
	wave_samples[(address - 0xff30) * 2 + 1] = ((value & 0x0f) - 7) * scale;
}


/* updates NR52 with the channel status (ON) */
static inline void mark_channel_on(unsigned int channel) {
	write_io(HWREG_NR52, read_io(HWREG_NR52) | (0x01 << (channel - 1)));
}

/* updates NR52 with the channel status (OFF) */
static inline void mark_channel_off(unsigned int channel) {
	write_io(HWREG_NR52, read_io(HWREG_NR52) & ~(0x01 << (channel - 1)));
}

void sound_update() {
	if (sound_cycles == 0)
		return;

	//SDL_LockAudio();
	SDL_LockMutex(sound_mutex);
	update_channel1(sound_cycles);
	update_channel2(sound_cycles);
	update_channel3(sound_cycles);
	update_channel4(sound_cycles);

	blip_end_frame(blip_left, sound_cycles);
	blip_end_frame(blip_right, sound_cycles);
	SDL_UnlockMutex(sound_mutex);

	sound_cycles = 0;
	//SDL_UnlockAudio();
}

static void update_channel1(int clocks) {
	int c;
	int soonest;
	unsigned t = 0;
	if (!sound.channel1.length.is_on)
		return;

	while (1) {
		soonest = get_soonest_clock(sound.channel1.period_counter, sound.channel1.length.i, sound.channel1.envelope.i, sound.channel1.sweep.i, &c);
		if (c > clocks) {
			sound.channel1.period_counter -= clocks;
			sound.channel1.length.i -= clocks;
			sound.channel1.envelope.i -= clocks;
			sound.channel1.sweep.i -= clocks;
			return;
		}
		clocks -= c;
		t += c;
		sound.channel1.period_counter -= c;
		sound.channel1.length.i -= c;
		sound.channel1.envelope.i -= c;
		sound.channel1.sweep.i -= c;
		switch (soonest) {
			case PERIOD:
				sound.channel1.period_counter = sound.channel1.period + 1;
				clock_square(&sound.channel1, t);
				break;
			case LENGTH:
				sound.channel1.length.i = 16384;
				clock_length(&sound.channel1.length, 1);
				break;
			case ENVELOPE:
				sound.channel1.envelope.i = 65536;
				clock_envelope(&sound.channel1.envelope);
				break;
			case SWEEP:
				sound.channel1.sweep.i = 32768;
				clock_sweep(&sound.channel1);
				break;
		}
	}
}


static inline void update_channel2(int clocks) {
	int c;
	int soonest;
	unsigned t = 0;
	if (!sound.channel2.length.is_on)
		return;

	while (1) {
		soonest = get_soonest_clock(sound.channel2.period_counter, sound.channel2.length.i, sound.channel2.envelope.i, -1, &c);
		if (c > clocks) {
			sound.channel2.period_counter -= clocks;
			sound.channel2.length.i -= clocks;
			sound.channel2.envelope.i -= clocks;
			return;
		}
		clocks -= c;
		t += c;
		sound.channel2.period_counter -= c;
		sound.channel2.length.i -= c;
		sound.channel2.envelope.i -= c;
		switch (soonest) {
			case PERIOD:
				sound.channel2.period_counter = sound.channel2.period + 1;
				clock_square(&sound.channel2, t);
				break;
			case LENGTH:
				sound.channel2.length.i = 16384;
				clock_length(&sound.channel2.length, 2);
				break;
			case ENVELOPE:
				sound.channel2.envelope.i = 65536;
				clock_envelope(&sound.channel2.envelope);
				break;
		}
	}
}


static inline void update_channel3(int clocks) {
	int c;
	int soonest;
	unsigned t = 0;
	if (!sound.channel3.length.is_on)
		return;

	while (1) {
		soonest = get_soonest_clock(sound.channel3.period_counter, sound.channel3.length.i, -1, -1, &c);
		if (c > clocks) {
			sound.channel3.period_counter -= clocks;
			sound.channel3.length.i -= clocks;
			return;
		}
		clocks -= c;
		t += c;
		sound.channel3.period_counter -= c;
		sound.channel3.length.i -= c;
		switch (soonest) {
			case PERIOD:
				sound.channel3.period_counter = sound.channel3.period + 1;
				clock_sample(&sound.channel3, t);
				break;
			case LENGTH:
				sound.channel3.length.i = 16384;
				clock_length(&sound.channel3.length, 3);
				break;
		}
	}
}

static inline void update_channel4(int clocks) {
	int c;
	int soonest;
	unsigned t = 0;
	if ((!sound.channel4.length.is_on) || (sound.channel4.period == 0))
		return;
	while (1) {
		soonest = get_soonest_clock(sound.channel4.period_counter, sound.channel4.length.i, sound.channel4.envelope.i, -1, &c);
		if (c > clocks) {
			sound.channel4.period_counter -= clocks;
			sound.channel4.length.i -= clocks;
			sound.channel4.envelope.i -= clocks;
			return;
		}
		clocks -= c;
		t += c;
		sound.channel4.period_counter -= c;
		sound.channel4.length.i -= c;
		sound.channel4.envelope.i -= c;
		switch (soonest) {
			case PERIOD:
				sound.channel4.period_counter = sound.channel4.period + 1;
				clock_lfsr(&sound.channel4, t);
				break;
			case LENGTH:
				sound.channel4.length.i = 16384;
				clock_length(&sound.channel4.length, 4);
				break;
			case ENVELOPE:
				sound.channel4.envelope.i = 65536;
				clock_envelope(&sound.channel4.envelope);
				break;
		}
	}
}

static void clock_square(SquareChannel *sq, int t) {
	const int duty_wave_high[4] = {16, 16, 8, 24};
	const int duty_wave_low[4] = {20, 24, 24, 16};

	if ((sq->period != 0) && (sq->period != 2048)) {
		sq->duty.i = (sq->duty.i + 1) & 0x1f;
		if (sq->duty.i == duty_wave_high[sq->duty.duty]) {
			if (sq->is_on_left)
				add_delta(LEFT, t, (HIGH / 15) * sq->envelope.volume, &sq->last_delta_left);
			if (sq->is_on_right)
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

static void clock_sample(SampleChannel *sc, int t) {
	if ((sc->period != 0) && (sc->period != 2048)) {
		sc->wave.i = (sc->wave.i + 1) & 0x1f;
		if (sc->is_on_left)
			add_delta(LEFT, t,	wave_samples[sc->wave.i] >> sc->volume, &sc->last_delta_left);
		if (sound.channel3.is_on_right)
			add_delta(RIGHT, t, wave_samples[sc->wave.i] >> sc->volume, &sc->last_delta_right);
	}
}

static void clock_lfsr(NoiseChannel *ns, int t) {
	if (ns->period != 0) {
		ns->lfsr.i = (ns->lfsr.i + 1) & (lfsr_size[ns->lfsr.size] - 1);
		if (ns->is_on_left)
			add_delta(LEFT, t, lfsr[ns->lfsr.size][ns->lfsr.i] * ns->envelope.volume, &ns->last_delta_left);
		if (ns->is_on_right)
			add_delta(RIGHT, t, lfsr[ns->lfsr.size][ns->lfsr.i] * ns->envelope.volume, &ns->last_delta_right);
	}

}

static void clock_length(Length *l, unsigned ch) {
	if (!l->is_continuous) {
		--l->length;
		if (l->length == 0) {
			mark_channel_off(ch);
			l->is_on = 0;
		}
	}
}

static void clock_envelope(Envelope *e) {
	if (e->length != 0) {
		--e->length_counter;
		if (e->length_counter == 0) {
			e->length_counter = e->length;
			if (e->is_increasing) {
				if (e->volume != 15)
					++e->volume;
			} else {
				if (e->volume != 0)
					--e->volume;
				else
					e->is_zombie = 1;
			}
		}
	}
}

static void clock_sweep(SquareChannel *sq) {
	if (sq->sweep.time != 0) {
		if (sq->sweep.time_counter == 0) {
			sq->sweep.time_counter = sq->sweep.time;
			if (sq->sweep.time == 0) {
				sq->length.is_on = 0;
				//mark_channel_off(1); /* FIXME: put these in initial check */
			} else {
				sweep_freq();
			}
		} else
			--sq->sweep.time_counter;
	}	
}

static int get_soonest_clock(unsigned a, unsigned b, unsigned c, unsigned d, unsigned *clocks) {
	int soonest;
	if (a < b) {
		if (a < c) {
			if (a < d) {
				soonest = PERIOD;
				*clocks = a;
			} else {
				soonest = SWEEP;
				*clocks = d;
			}
		} else {
			if (c < d) {
				soonest = ENVELOPE;
				*clocks = c;
			} else {
				soonest = SWEEP;
				*clocks = d;
			}
		}
	} else {
		if (b < c) {
			if (b < d) {
				soonest = LENGTH;
				*clocks = b;
			} else {
				soonest = SWEEP;
				*clocks = d;
			}
		} else {
			if (c < d) {
				soonest = ENVELOPE;
				*clocks = c;
			} else {
				soonest = SWEEP;
				*clocks = d;
			}
		}
	}
	return soonest;
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
			sound.channel1.length.is_on = 0;
			sound.channel1.sweep.hidden_freq = 2048;
		}
	} else {
		sound.channel1.sweep.hidden_freq -= (sound.channel1.sweep.hidden_freq >> sound.channel1.sweep.shift_number);
		if (sound.channel1.sweep.hidden_freq > 2047) {
			sound.channel1.sweep.hidden_freq = 0; /* ? */
			sound.channel1.length.is_on = 0;
		}
	}
}

static void add_delta(int side, unsigned t, short amp, short *last_delta) {
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

void sound_save(void) {
	
	
}

void sound_load(void) {
	
}

static void callback(void* data, Uint8 *stream, int len) {
	Sint16 *buffer = (Sint16 *)stream;
	sound_update();
	
	SDL_LockMutex(sound_mutex);
	blip_read_samples(blip_left, buffer, len / 4, 1);
	blip_read_samples(blip_right, buffer + 1, len / 4, 1);
	SDL_UnlockMutex(sound_mutex);
}
