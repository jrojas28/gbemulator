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

void sound_update();

typedef struct {
	unsigned duty;
	unsigned i;
} Duty;

typedef struct {
	unsigned i;
	short* samples;
} Wave;

typedef struct {
	unsigned i;
	unsigned length;
	bool is_continuous;
} Length;

typedef struct {
	unsigned volume;
	bool is_increasing;
	unsigned length_counter;
	unsigned length;
	bool is_zombie;
	unsigned i;
} Envelope;

typedef struct {
	unsigned hidden_freq;
	unsigned time_counter;
	unsigned time;
	bool is_decreasing;
	unsigned shift_number;
	unsigned i;
} Sweep;


typedef struct {
	Duty duty;
	Envelope envelope;
	Sweep sweep;
	Length length;

	unsigned freq;
	unsigned period;
	unsigned period_counter;
	short last_delta_right;
	short last_delta_left;
	bool is_on;
	
	bool is_on_left;
	bool is_on_right;
} SquareChannel;

/*
typedef struct {
	Duty duty;
	Envelope envelope;
	Length length;
	unsigned freq;
	unsigned period;
	unsigned period_counter;
	short last_delta_right;
	short last_delta_left;
	bool is_continuous;
	bool is_on;
	
	bool is_on_left;
	bool is_on_right;
} Channel2;
*/

typedef struct {
	Wave wave;
	Length length;
	unsigned volume;
	unsigned freq;
	unsigned period;
	unsigned period_counter;
	short last_delta_right;
	short last_delta_left;
	bool is_on;
	
	bool is_on_left;
	bool is_on_right;
} SampleChannel;

typedef struct {
	bool is_on;
	unsigned right_level;
	unsigned left_level;
	SquareChannel channel1;
	SquareChannel channel2;
	SampleChannel channel3;
} SoundData;

void sound_init(void);
void sound_fini(void);
void write_sound(Word address, Byte value);
void write_wave(Word address, Byte value);
void stop_sound(void);
void start_sound(void);
void sound_save(void);
void sound_load(void);
void sound_reset(void);

#endif /* _SOUND_H */

