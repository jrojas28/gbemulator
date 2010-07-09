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
 
// TODOs
// subsequent sweeps should not use the new frequency!
// make channel 1 not init if it will immediately overflow the frequency on sweep
// upon channel init, wave position is not reset: delayed 1/12 of cycle.
// if sound power is off to sound, register writes are ignored (except to wave pattern + NR52)
// initialise wave pattern mem with: 0xac, 0xdd, 0xda, 0x48, 0x36, 0x02, 0xcf, 0x16, 0x2c, 0x04, 0xe5, 0x2c, 0xac, 0xdd, 0xda, 0x48 for R-TYPE
// for gbc, initialise wave pattern mem with: 00 FF 00 FF 00 FF 00 FF 00 FF 00 FF 00 FF 00 FF

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <portaudio.h>
#include <string.h>
#include "sound.h"
#include "memory.h"
#include "save.h"

static char *lfsr_7;
static char *lfsr_15;
static double sample_rate = 22050;
static PaStream *stream;
static SoundData sound;

static inline unsigned int frequency_to_period(float frequency);
static inline unsigned int convert_time(float time);
static int call_back(const void *input_buffer, void *output_buffer,
                    unsigned long frames_per_buffer,
                    const PaStreamCallbackTimeInfo* time_info,
                    PaStreamCallbackFlags status_flags,
                    void *user_data);
static inline float gb_freq_to_freq(unsigned int gb_frequency);
static inline void mark_channel_on(unsigned int channel);
static inline void mark_channel_off(unsigned int channel);

void sound_init(void) {
	unsigned char r7;
	unsigned short r15;
	int i;
	PaError err;
	PaStreamParameters output_parameters;
	PaStreamInfo* stream_info;
	/* allocate memory for LFSR tables */
	lfsr_7 = malloc(LFSR_7_SIZE);
	lfsr_15 = malloc(LFSR_15_SIZE);

	/* initialise 7 bit LFSR values */
	r7 = 0xff;
	for (i = 0; i < LFSR_7_SIZE; i++) {
		r7 >>= 1;
		r7 |= (((r7 & 0x02) >> 1) ^ (r7 & 0x01)) << 7;
		if (r7 & 0x01)
			lfsr_7[i] = 1;
		else
			lfsr_7[i] = -1;
	}


	/* initialise 15 bit LFSR values */
	r15 = 0xffff;
	for (i = 0; i < LFSR_15_SIZE; i++) {
		r15 >>= 1;
		r15 |= (((r15 & 0x0002) >> 1) ^ (r15 & 0x0001)) << 15;
		if (r15 & 0x0001)
			lfsr_15[i] = 1;
		else
			lfsr_15[i] = -1;
	}
	

	/*
	for (i = 0; i < 32; i++) {
		//sound.channel_3.samples[i] = sin((i * 2 * M_PI) / 32) * 15;
		//printf("%hhd\n", sound_data.channel_3.samples[i]);
	}
	*/
	
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
	
	sound.volume_left = 7;
	sound.volume_right = 7;
	sound.channel_1.is_on = 0;
	sound.channel_2.is_on = 0;
	sound.channel_3.is_on = 0;
	sound.channel_4.is_on = 0;
	sound.is_on = 0;
	
}

void sound_fini(void) {
	PaError err;
	if (sound.is_on) {
		stop_sound();
	}
	err = Pa_CloseStream(stream);

	Pa_Terminate();

	free(lfsr_7);
	free(lfsr_15);
}

void stop_sound(void) {
	PaError err;
	assert(sound.is_on);
	err = Pa_StopStream(stream);
	sound.is_on = 0;
}

void start_sound(void) {
	PaError err;
	assert(!sound.is_on);
	err = Pa_StartStream(stream);
    if (err != paNoError) {
		Pa_Terminate();
		fprintf(stderr, "could not start output stream: %s\n", Pa_GetErrorText(err));
		exit(1);
	}
	sound.is_on = 1;
}

// fill with sensible defaults...
void sound_reset(void) {
	sound.volume_left = 7; // FIXME
	sound.volume_right = 7;
	
	write_sound(HWREG_NR10,	0x80);
	write_sound(HWREG_NR11,	0xBF);
	write_sound(HWREG_NR12,	0xF3);
	write_sound(HWREG_NR14,	0xBF);
	write_sound(HWREG_NR21,	0x3F);
	write_sound(HWREG_NR22,	0x00);
	write_sound(HWREG_NR24,	0xBF);
	write_sound(HWREG_NR30,	0x7F);
	write_sound(HWREG_NR31,	0xFF);
	write_sound(HWREG_NR32,	0x9F);
	write_sound(HWREG_NR33,	0xBF);
	write_sound(HWREG_NR41,	0xFF);
	write_sound(HWREG_NR42,	0x00);
	write_sound(HWREG_NR43,	0x00);
	write_sound(HWREG_NR44,	0xBF);
	write_sound(HWREG_NR50,	0x77);
	write_sound(HWREG_NR51,	0xF3);
	write_sound(HWREG_NR52,	0xF1); // TODO DIFFERENT FOR SGB
	sound.channel_1.is_on = 0;
	sound.channel_2.is_on = 0;
	sound.channel_3.is_on = 0;
	sound.channel_4.is_on = 0;

	if (!sound.is_on) {
		start_sound();
	}
}

void write_sound(Word address, Byte value) {
	//unsigned int frequency = 0;
	Byte b;
	unsigned int frequency;
	unsigned int r, s;
	//if (address < 0xFF15)
	//	printf("write: %hx: %hhx\n", address, value);
	switch (address) {
		case HWREG_NR10:	// channel 1 sweep
			// FIXME should this be commented??
			//sound.channel_1.sweep.time = convert_time((float)((value & 70) >> 4) / 128.0f);
			//sound.channel_1.sweep.sign = (value & 0x08) ? -1.0f : 1.0f;
			//sound.channel_1.sweep.number = value & 0x07;
			//printf("%hhu\n", value);
			break;
		case HWREG_NR11: 	// channel 1 length / cycle duty
			switch((value & 0xc0) >> 6) {
				case 0x00:
					sound.channel_1.duty = 0.125f;
					break;
				case 0x01:
					sound.channel_1.duty = 0.25f;
					break;
				case 0x02:
					sound.channel_1.duty = 0.5f;
					break;
				case 0x03:
					sound.channel_1.duty = 0.75f;
					break;
			}
			sound.channel_1.length = convert_time((float)(64 - (value & 0x3f)) / 256);
			break;
		case HWREG_NR12:	// channel 1 envelope
			// envelope changes occur on next channel INIT
			//if (!(value & 0xf0))
			//	sound.channel_1.envelope.volume = 0;
			//	sound.channel_1.envelope.sign = (value & 0x08) ? +1.0f : -1.0f;
			//	sound.channel_1.envelope.number = value & 0x07;
			//	sound.channel_1.envelope.time = convert_time((float)sound.channel_1.envelope.number / 64.0f);
			break;
		case HWREG_NR13:	// frequency lo
			sound.channel_1.gb_frequency = (unsigned int)value | ((unsigned int)(read_io(HWREG_NR14) & 0x07) << 8);
			sound.channel_1.period = frequency_to_period(gb_freq_to_freq(sound.channel_1.gb_frequency));
			//sound.channel_1.i = 0;	DEFINITELY NOT SUPPOSED TO BE HERE.
			break;
		case HWREG_NR14:	// frequency hi, init, counter selection
			sound.channel_1.is_continuous = (value & 0x40) ? 0 : 1;
			sound.channel_1.gb_frequency = (unsigned int)read_io(HWREG_NR13) | ((unsigned int)(value & 0x07) << 8);
			sound.channel_1.period = frequency_to_period(gb_freq_to_freq(sound.channel_1.gb_frequency));
			if (value & 0x80) {
				mark_channel_on(1);
				sound.channel_1.is_on = 1;
				//if (sound.channel_1.length == 0) {
				//	sound.channel_1.length = convert_time(64);
				//}
				sound.channel_1.i = 0;				
				b = read_io(HWREG_NR12);	// read envelope data
				sound.channel_1.envelope.volume = (b & 0xf0) >> 4;
				sound.channel_1.envelope.sign = (b & 0x08) ? +1.0f : -1.0f;
				sound.channel_1.envelope.number = b & 0x07;
				sound.channel_1.envelope.time = convert_time((float)sound.channel_1.envelope.number / 64.0f);
				sound.channel_1.envelope.i = 0;
				sound.channel_1.envelope.j = 0;
				b = read_io(HWREG_NR10);
				sound.channel_1.sweep.time = convert_time((float)((b & 70) >> 4) / 128.0f);
				sound.channel_1.sweep.sign = (b & 0x08) ? -1.0f : 1.0f;
				sound.channel_1.sweep.number = b & 0x07;
				sound.channel_1.sweep.i = 0;
				sound.channel_1.sweep.j = 0;
				sound.channel_1.sweep.shadow = sound.channel_1.gb_frequency;
				//printf("init\n");
			}
			break;
		case 0xFF15: 		// unused
			break;
		case HWREG_NR21: 	// channel 2 length / cycle duty
			switch((value & 0xc0) >> 6) {
				case 0x00:
					sound.channel_2.duty = 0.125f;
					break;
				case 0x01:
					sound.channel_2.duty = 0.25f;
					break;
				case 0x02:
					sound.channel_2.duty = 0.5f;
					break;
				case 0x03:
					sound.channel_2.duty = 0.75f;
					break;
			}
			sound.channel_2.length = convert_time((float)(64 - (value & 0x3f)) / 256);
			break;
		case HWREG_NR22:	// channel 2 envelope
			// envelope changes occur on next channel INIT
			//if (!(value & 0xf0))
			//	sound.channel_2.envelope.volume = 0;
			//sound.channel_2.envelope.sign = (value & 0x08) ? +1.0f : -1.0f;
			//sound.channel_2.envelope.number = value & 0x07;
			//sound.channel_2.envelope.time = convert_time((float)sound.channel_2.envelope.number / 64.0f);
			break;
		case HWREG_NR23:	// frequency lo
			sound.channel_2.gb_frequency = (unsigned int)value | ((unsigned int)(read_io(HWREG_NR24) & 0x07) << 8);
			sound.channel_2.period = frequency_to_period(gb_freq_to_freq(sound.channel_2.gb_frequency));
			break;		
		case HWREG_NR24:	// frequency hi, init, counter selection
			sound.channel_2.is_continuous = (value & 0x40) ? 0 : 1;
			sound.channel_2.gb_frequency = (unsigned int)read_io(HWREG_NR23) | ((unsigned int)(value & 0x07) << 8);
			sound.channel_2.period = frequency_to_period(gb_freq_to_freq(sound.channel_2.gb_frequency));
			if (value & 0x80) {
				mark_channel_on(2);
				sound.channel_2.is_on = 1;
				sound.channel_2.i = 0;				
				b = read_io(HWREG_NR22);	// read envelope data
				sound.channel_2.envelope.volume = (b & 0xf0) >> 4;
				sound.channel_2.envelope.sign = (b & 0x08) ? +1.0f : -1.0f;
				sound.channel_2.envelope.number = b & 0x07;
				sound.channel_2.envelope.time = convert_time((float)sound.channel_2.envelope.number / 64.0f);
				sound.channel_2.envelope.i = 0;
				sound.channel_2.envelope.j = 0;
			}
			break;
		case HWREG_NR30:	// channel 3 sound on/off
			sound.channel_3.is_on = (value & 0x80) ? 1 : 0;
			break;
		case HWREG_NR31:	// channel 3 length
			sound.channel_3.length = convert_time((float)(256 - value) / 256);
			break;
		case HWREG_NR32:	// channel 3 volume
			/* volume changes occur on next channel INIT */
			break;
		case HWREG_NR33:	/* frequency lo */
			frequency = (unsigned int)value | ((unsigned int)(read_io(HWREG_NR34) & 0x07) << 8);
			sound.channel_3.period = frequency_to_period(65536 / (float)(2048 - frequency));
			break;
		case HWREG_NR34:	/* frequency hi, init, counter selection */
			sound.channel_3.is_continuous = (value & 0x40) ? 0 : 1;
			frequency = (unsigned int)read_io(HWREG_NR33) | ((unsigned int)(value & 0x07) << 8);
			sound.channel_3.period = frequency_to_period(65536 / (float)(2048 - frequency));
			if (value & 0x80) {
				//fprintf(stderr, "TRIGGER3\n");
				mark_channel_on(3);
				sound.channel_3.is_on = 1;
				sound.channel_3.i = 0;
				//sound.channel_3.j = 0;
				b = read_io(HWREG_NR32);	/* read volume data */
				switch ((b & 0x60) >> 4) {
					/* volume is expressed in number of right bitshifts! */
					case 0x00: sound.channel_3.volume = -1; break; /* 0% 	*/
					case 0x01: sound.channel_3.volume = 0; break; /* 100% 	*/
					case 0x02: sound.channel_3.volume = 1; break; /* 50%	*/
					case 0x03: sound.channel_3.volume = 2; break; /* 25%	*/
				}
			}
			break;
		case HWREG_NR41:	/* channel 4 sound length */
			sound.channel_4.length = convert_time((float)(64 - (value & 0x3f)) / 256);
			break;
		case HWREG_NR42:	/* channel 4 envelope */
			/* envelope changes occur on next channel INIT */
				//if (!(value & 0xf0))
				//	sound.channel_4.envelope.volume = 0;
				//sound.channel_4.envelope.sign = (value & 0x08) ? +1.0f : -1.0f;
				//sound.channel_4.envelope.number = value & 0x07;
				//sound.channel_4.envelope.time = convert_time((float)sound.channel_4.envelope.number / 64.0f);
			break;
		case HWREG_NR43:	/* LFSR settings */
			s = (value & 0xf0) >> 4;
			r = value & 0x07;
			if (r != 0)
				frequency = (524288 / r) >> (s + 1);
			else
				frequency = (524288 * 2) >> (s + 1);
			sound.channel_4.period = frequency_to_period(frequency);
			sound.channel_4.counter = (value & 0x08) ? LFSR_7 : LFSR_15;
			sound.channel_4.j %= (value & 0x08) ? LFSR_7_SIZE : LFSR_15_SIZE;
			break;
		case HWREG_NR44:	/* channel 4 init + counter selection */
			sound.channel_4.is_continuous = (value & 0x40) ? 0 : 1;
			if (value & 0x80) {
				mark_channel_on(4);
				sound.channel_4.is_on = 1;
				//sound.channel_4.i = 0;
				b = read_io(HWREG_NR42);	// read envelope data
				sound.channel_4.envelope.volume = (b & 0xf0) >> 4;
				sound.channel_4.envelope.sign = (b & 0x08) ? +1.0f : -1.0f;
				sound.channel_4.envelope.number = b & 0x07;
				sound.channel_4.envelope.time = convert_time((float)sound.channel_4.envelope.number / 64.0f);
				sound.channel_4.envelope.i = 0;
				sound.channel_4.envelope.j = 0;
			}
			break;
		case HWREG_NR50:	/* channel control */
			sound.volume_left = value & 0x07;
			sound.volume_right = (value & 0x70) >> 4;
			break;
		case HWREG_NR52:	// turn off sound
			//sound.is_on =  (value & 0x80) ? 1 : 0; /*FIXME*/
			break;
		case HWREG_NR51:	/* output terminal selection */
			sound.channel_4.is_on_right = (value & 0x80) ? 1 : 0;
			sound.channel_3.is_on_right = (value & 0x40) ? 1 : 0;
			sound.channel_2.is_on_right = (value & 0x20) ? 1 : 0;
			sound.channel_1.is_on_right = (value & 0x10) ? 1 : 0;
			sound.channel_4.is_on_left = (value & 0x08) ? 1 : 0;
			sound.channel_3.is_on_left = (value & 0x04) ? 1 : 0;
			sound.channel_2.is_on_left = (value & 0x02) ? 1 : 0;
			sound.channel_1.is_on_left = (value & 0x01) ? 1 : 0;
			break;
		default:
			break;
	}
}

/*
 * updates the wave pattern cache when the gb's wave pattern memory is modified
 */
void write_wave(Word address, Byte value) {
	/* wave memory is set up so that high the nibble is played first */
	sound.channel_3.samples[(address - 0xff30) * 2] = ((signed int)((value & 0xf0) >> 4) * 2) - 15;
	sound.channel_3.samples[((address - 0xff30) * 2) + 1] = ((signed int)(value & 0x0f) * 2) - 15;
}


/* 
 * converts a frequency into a period (in samples NOT seconds!!!) 
 */
static inline unsigned int frequency_to_period(float frequency) {
	return (float)sample_rate / frequency;
} 

/*
 * converts time in seconds to time in samples (which is what the callback uses)
 */
static inline unsigned int convert_time(float time) {
	return sample_rate * time;
}

/* converts a gameboy frequency into a real frequency (in 1/seconds) */
static inline float gb_freq_to_freq(unsigned int gb_frequency) {
	assert(gb_frequency < 2048);
	return 131072 / (float)(2048 - gb_frequency);
}

/* updates NR52 with the channel status (ON) */
static inline void mark_channel_on(unsigned int channel) {
	write_io(HWREG_NR52, read_io(HWREG_NR52) | (0x01 << ((unsigned char)channel - 1)));
}

/* updates NR52 with the channel status (OFF) */
static inline void mark_channel_off(unsigned int channel) {
	write_io(HWREG_NR52, read_io(HWREG_NR52) & ~(0x01 << ((unsigned char)channel - 1)));
}

/*
 * portaudio sound callback. portaudio calls this whenever its sound buffer
 * needs refilling. to maintain portability, avoid weird function calls.
 * this function should be kept efficient, as the for loop must iterate
 * a number of times equal to the sample rate every second.
 */
static int call_back(const void *input_buffer, void *output_buffer,
                    unsigned long frames_per_buffer,
                    const PaStreamCallbackTimeInfo* time_info,
                    PaStreamCallbackFlags status_flags,
                    void *user_data) {
	SoundData *data = (SoundData*)user_data;
	unsigned char *out = (unsigned char *)output_buffer;
	char left[CHANNELS];
	char right[CHANNELS];
	unsigned long i, j;
	signed int temp;
	signed short so1, so2;
	/* Prevent unused variable warnings. */
	(void) time_info;
	(void) status_flags;
	(void) input_buffer;

	for(i = 0; i < frames_per_buffer; i++) {
		/* initialise samples to 0 */
		 for (j = 0; j < CHANNELS; j++) {
		 	left[j] = GROUND;
		 	right[j] = GROUND;
		 }
		
		// Channel 1 ===================================================
		if (data->channel_1.is_on) {
			// generate square wave
			if (data->channel_1.i < (data->channel_1.period * data->channel_1.duty)) {
				left[0] = 1 * data->channel_1.envelope.volume * data->channel_1.is_on_left;
				right[0] = 1 * data->channel_1.envelope.volume * data->channel_1.is_on_right;
			} else {
				left[0] = -1 * data->channel_1.envelope.volume * data->channel_1.is_on_left;
				right[0] = -1 * data->channel_1.envelope.volume * data->channel_1.is_on_right;
			}
			++data->channel_1.i;
			// if the waveform has reached its period, restart it
			if (data->channel_1.i >= data->channel_1.period)
				data->channel_1.i = 0;
			// if the sound is not continuous it, reduce its length
			if (!data->channel_1.is_continuous) {
				--data->channel_1.length;
				// if the sound has completed its length, turn off the channel
				if(data->channel_1.length == 0) {
					data->channel_1.is_on = 0;
					mark_channel_off(1);
				}
			}
			// is sweep enabled?
			if ((data->channel_1.sweep.number != 0) && (data->channel_1.sweep.time != 0)) {
				++data->channel_1.sweep.i;
				// is it time to shift the frequency?
				if ((data->channel_1.sweep.i == data->channel_1.sweep.time) /*&& 
				   (data->channel_1.sweep.j != data->channel_1.sweep.number)*/) {
					//data->channel_1.gb_frequency = data->channel_1.sweep.initial_frequency + ((data->channel_1.sweep.initial_frequency * data->channel_1.sweep.sign) / (1 << data->channel_1.sweep.number));
					temp = data->channel_1.sweep.shadow + ((data->channel_1.sweep.shadow >> data->channel_1.sweep.number) * data->channel_1.sweep.sign);
					// if the frequency has overflowed, disable channel
					if ((temp >= 2048) || (temp < 0)) {
						data->channel_1.is_on = 0;
						mark_channel_off(1);
					} else {
						// convert from gameboy frequency to a real frequency and update the period
						data->channel_1.sweep.shadow = data->channel_1.gb_frequency = temp;
						data->channel_1.period = frequency_to_period(gb_freq_to_freq(data->channel_1.gb_frequency));
						write_io(HWREG_NR13, (Byte)(data->channel_1.gb_frequency & 0xff));
						write_io(HWREG_NR14, (read_io(HWREG_NR14) & 0xf8) | ((data->channel_1.gb_frequency & 700) >> 8));
						// TODO writeback this value to registers
						data->channel_1.sweep.i = 0;
						++data->channel_1.sweep.j;
					}
				}
			}
			if (data->channel_1.envelope.number != 0) {
				++data->channel_1.envelope.i;
				// is it time to shift the volume?
				if ((data->channel_1.envelope.i == data->channel_1.envelope.time)) /* && 
				   (data->channel_1.envelope.j != data->channel_1.envelope.number))*/ {
					data->channel_1.envelope.volume += data->channel_1.envelope.sign;
					//printf("volume: %hhd. number: %u<-%u\n", data->channel_1.envelope.volume, data->channel_1.envelope.number, data->channel_1.envelope.j);
					if (data->channel_1.envelope.volume < 0) 
						data->channel_1.envelope.volume = 0;
					if (data->channel_1.envelope.volume > 15)
						data->channel_1.envelope.volume = 15;
					if (data->channel_1.envelope.volume == 0) {
						data->channel_1.is_on = 0;
						mark_channel_off(1); // FIXME should this be here??
					}
					data->channel_1.envelope.i = 0;
					++data->channel_1.envelope.j;
				}				
			}
		}

		// Channel 2 ===================================================
		if (data->channel_2.is_on) {
			// generate square wave
			if (data->channel_2.i < (data->channel_2.period * data->channel_2.duty)) {
				left[1] = 1 * data->channel_2.envelope.volume * data->channel_2.is_on_left;
				right[1] = 1 * data->channel_2.envelope.volume * data->channel_2.is_on_right;
			} else {
				left[1] = -1 * data->channel_2.envelope.volume * data->channel_2.is_on_left;
				right[1] = -1 * data->channel_2.envelope.volume * data->channel_2.is_on_right;
			}
			++data->channel_2.i;
			// if the waveform has reached its period, restart it
			if (data->channel_2.i >= data->channel_2.period)
				data->channel_2.i = 0;
			// if the sound is not continuous it, reduce its length
			if (!data->channel_2.is_continuous) {
				--data->channel_2.length;
				// if the sound has completed its length, turn off the channel
				if(data->channel_2.length == 0) {
					data->channel_2.is_on = 0;
					mark_channel_off(2);
				}
			}
			if (data->channel_2.envelope.number != 0) {
				++data->channel_2.envelope.i;
				// is it time to shift the volume?
				if (data->channel_2.envelope.i == data->channel_2.envelope.time) {
					data->channel_2.envelope.volume += data->channel_2.envelope.sign;
					if (data->channel_2.envelope.volume < 0) 
						data->channel_2.envelope.volume = 0;
					if (data->channel_2.envelope.volume > 15)
						data->channel_2.envelope.volume = 15;
					if (data->channel_2.envelope.volume == 0) {
						data->channel_2.is_on = 0;
						mark_channel_off(2); // FIXME should this be here??
					}
					data->channel_2.envelope.i = 0;
					++data->channel_2.envelope.j;
				}				
			}
		}
		
		// Channel 3 ===================================================
		if (data->channel_3.is_on) {
			// output arbitrary wave data
			if (data->channel_3.volume != 255) {
				if (data->channel_3.is_on_left)
					left[2] = (data->channel_3.samples[(data->channel_3.i * 32) / data->channel_3.period]) >> data->channel_3.volume;
				if (data->channel_3.is_on_right)		
					right[2] = (data->channel_3.samples[(data->channel_3.i * 32) / data->channel_3.period]) >> data->channel_3.volume;
			}
			++data->channel_3.i;
			// if the waveform has reached its period, restart it
			if (data->channel_3.i >= data->channel_3.period) {
				data->channel_3.i = 0;
			}
			// if the sound is not continuous, reduce its length
			if (!data->channel_3.is_continuous) {
				--data->channel_3.length;
				// if the sound has completed its length, turn off the channel
				if(data->channel_3.length == 0) {
					data->channel_3.is_on = 0;
					mark_channel_off(3);
				}
			}
		}

		// Channel 4 ===================================================
		if (data->channel_4.is_on) {
			// output noise
			if (data->channel_4.counter == LFSR_7) {
				left[3] = lfsr_7[data->channel_4.j] * data->channel_4.envelope.volume * data->channel_4.is_on_left;
				right[3] = lfsr_7[data->channel_4.j] * data->channel_4.envelope.volume * data->channel_4.is_on_right;
			} else {
				left[3] = lfsr_15[data->channel_4.j] * data->channel_4.envelope.volume * data->channel_4.is_on_left;
				right[3] = lfsr_15[data->channel_4.j] * data->channel_4.envelope.volume * data->channel_4.is_on_right;
			}
			++data->channel_4.i;
			// if the waveform has reached its period, restart it
			if (data->channel_4.i >= data->channel_4.period) {
				data->channel_4.i = 0;
				++data->channel_4.j;
				if (((data->channel_4.counter == LFSR_7) 
					&& (data->channel_4.j >= LFSR_7_SIZE)) 
						|| ((data->channel_4.counter == LFSR_15) 
							&& (data->channel_4.j >= LFSR_15_SIZE)))
					data->channel_4.j = 0;
			}
			// if the sound is not continuous, reduce its length
			//printf("%u. %hhu\n", data->channel_4.length, data->channel_4.is_continuous);
			if (!data->channel_4.is_continuous) {
				--data->channel_4.length;
				// if the sound has completed its length, turn off the channel
				if(data->channel_4.length == 0) { // TODO unset bit in FF26
					data->channel_4.is_on = 0;
					mark_channel_off(4);
				}
			}
			if (data->channel_4.envelope.number != 0) {
				++data->channel_4.envelope.i;
				// is it time to shift the volume?
				if (data->channel_4.envelope.i == data->channel_4.envelope.time) {
					data->channel_4.envelope.volume += data->channel_4.envelope.sign;
					if (data->channel_4.envelope.volume < 0)
						data->channel_4.envelope.volume = 0;
					if (data->channel_4.envelope.volume > 15)
						data->channel_4.envelope.volume = 15;
					if (data->channel_2.envelope.volume == 0) {
						data->channel_4.is_on = 0;
						mark_channel_off(4); // FIXME should this be here??
					}
					data->channel_4.envelope.i = 0;
					++data->channel_4.envelope.j;
				}
			}
		}

		/*
		 * mix the channels and expand to larger type temporarily, so that 
		 * volume can be reduced with as little rounding down data loss 
		 * as possible.
		 */
		so1 = ((left[0] + left[1] + left[2] + left[3]) * data->volume_left) / 7;
		so2 = ((right[0] + right[1] + right[2] + right[3]) * data->volume_right) / 7;
		// output the mixed samples
		//printf("%+hhi\n", left[1]);
		*out++ = (signed char)so1;
		*out++ = (signed char)so2;
    }
    
    return paContinue;
}

void sound_save(void) {
	save_uint("channel_1.length", sound.channel_1.length);
	save_uint("channel_1.period", sound.channel_1.period);
	save_int("channel_1.gb_frequency", sound.channel_1.gb_frequency);
	save_uint("channel_1.i", sound.channel_1.i);
	
	save_uint("channel_1.length", sound.channel_1.length);
	save_uint("channel_1.sweep.number", sound.channel_1.sweep.number);
	save_uint("channel_1.sweep.time", sound.channel_1.sweep.time);
	save_int("channel_1.sweep.shadow", sound.channel_1.sweep.shadow);
	save_uint("channel_1.sweep.i", sound.channel_1.sweep.i);
	save_uint("channel_1.sweep.j", sound.channel_1.sweep.j);
	
	
}

void sound_load(void) {
	save_uint("channel_1.sweep.number", sound.channel_1.sweep.number);
	save_uint("channel_1.sweep.time", sound.channel_1.sweep.time);
	save_int("channel_1.sweep.shadow", sound.channel_1.sweep.shadow);
	save_uint("channel_1.sweep.i", sound.channel_1.sweep.i);
	save_uint("channel_1.sweep.j", sound.channel_1.sweep.j);

	
}


