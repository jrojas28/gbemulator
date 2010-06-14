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

#ifndef _SOUND_H
#define _SOUND_H

#include <stdbool.h>
#include "gbem.h"

#define HIGH 				127
#define LOW 				(-128)
#define GROUND				0
#define CHANNELS 			4
#define LFSR_7_SIZE			127
#define LFSR_15_SIZE		32767
#define LFSR_7				0
#define LFSR_15				1


typedef struct {
	unsigned int number;
	unsigned int time;
	signed int shadow;
	float sign;
	unsigned int i, j;
} Sweep;

typedef struct {
	unsigned int number;
	unsigned int time;
	char volume;
	char sign;
	unsigned int i, j;
} Envelope;

typedef struct {
	unsigned int length;
	unsigned int period;
	signed int gb_frequency;
	unsigned int i;
	bool is_on;
	bool is_continuous;
	bool is_on_left;
	bool is_on_right;
	float duty;
	Sweep sweep;
	Envelope envelope;
} Channel_1;

typedef struct {
	unsigned int length;
	unsigned int period;
	signed int gb_frequency;
	float frequency;
	unsigned int i;
	bool is_on;
	bool is_continuous;
	bool is_on_left;
	bool is_on_right;
	float duty;
	Envelope envelope;
} Channel_2;

typedef struct {
	unsigned int length;
	unsigned int period;
	float frequency;
	unsigned int i, j;
	signed char samples[32];
	unsigned char volume;
	bool is_on_left;
	bool is_on_right;
	bool is_on;
	bool is_continuous;
} Channel_3;

typedef struct {
	unsigned int length;
	unsigned int period;
	unsigned int i, j;
	unsigned int counter;
	bool is_on_left;
	bool is_on_right;
	bool is_on;
	bool is_continuous;
	Envelope envelope;
} Channel_4;

typedef struct {
	Channel_1 channel_1;
	Channel_2 channel_2;
	Channel_3 channel_3;
	Channel_4 channel_4;	
	float volume_left;
	float volume_right;
	bool is_on;
} SoundData;


void sound_init();
void sound_fini();
void write_sound(Word address, Byte value);
void write_wave(Word address, Byte value);


#endif /* _SOUND_H */

