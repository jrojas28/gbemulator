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

#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdio.h>
#include "gbem.h"

#define VT_GRANULARITY 0x100

void memory_reset(void);
void memory_init(void);
void memory_fini(void);
void memory_save(void);
void memory_load(void);

static inline Byte readb(Word address);
void writeb(Word address, Byte value);
static inline Word readw(Word address);
static inline void writew(Word address, Word w);

static inline void write_io(Word address, Byte value);
static inline Byte read_io(Word address);

static inline void set_vector(Word address, Byte* real_address);
static inline Byte* get_vector(Word address);
static inline void set_vector_block(Word address, Byte* real_address, unsigned c);


static inline Word readw(Word address) {
	Word w;
	// convert little endian word to host endian
#ifdef WORDS_BIGENDIAN
	w = readb(address + 1) | (readb(address) << 8);
#else
	w = readb(address) | (readb(address + 1) << 8);
#endif
	return w;
}

static inline void writew(Word address, Word w) {
	// write as little endian
#ifdef WORDS_BIGENDIAN
	writeb(address + 1, w & 0xFF);
	writeb(address, (w >> 8) & 0xFF);
#else
	writeb(address, w & 0xFF);
	writeb(address + 1, (w >> 8) & 0xFF);
#endif
}

static inline void write_io(Word address, Byte value) {
	extern Byte* himem;
	himem[address - MEM_IO] = value;
}

static inline Byte read_io(Word address) {
	extern Byte* himem;
	return himem[address - MEM_IO];
}

static inline void set_vector(Word address, Byte* real_address) {
	extern Byte** vector_table;
	vector_table[address] = real_address;
}

static inline Byte* get_vector(Word address) {
	extern Byte** vector_table;
	return vector_table[address];
}

static inline Byte readb(Word address) {
	extern Byte** vector_table;
	return *(vector_table[address >> 8] + (address & 0xFF));
}

static inline void set_vector_block(Word address, Byte* real_address, unsigned c) {
	unsigned int i;
	for (i = 0; i < (c / VT_GRANULARITY); i++) {
		set_vector((address >> 8) + (Word)i, real_address + (i * VT_GRANULARITY));
	}
}

#endif /* _MEMORY_H */
