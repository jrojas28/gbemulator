/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * save.c
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
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "save.h"
#include "cart.h"
#include "core.h"
#include "memory.h"
#include "sound.h"
#include "display.h"

#define NO_MATCH	-1

static char* get_fn();
static int get_index(char* key);

static char* encode_base64(unsigned char* raw, unsigned int c);
static void decode_base64(char* enc, unsigned char* raw, unsigned int count);
static char base64_lookup(char c);

unsigned int save_slot = 0;
static unsigned int err = 0;
static const char ext[] = ".st8";
static FILE* fp = NULL;

static char** keys = NULL;
static char** values = NULL;
static unsigned int entries;

void save_state() {
	char* fn;
	fn = get_fn();
	fp = fopen(fn, "w");
	if (fp == NULL) {
		fprintf(stderr, "could not open state file for writing: %s\n", fn);
		perror("fopen");
		free(fn);
		return;
	}
	printf("saving state to %s...\n", fn);
	core_save();
	memory_save();
	cart_save();
	display_save();
	sound_save();
	
	fclose(fp);
	fp = NULL;
	free(fn);
}

void load_state() {
	int i;
	char* fn;
	char* separator;
	char* comment;
	char* newline;
	char* key;
	char* value;
	size_t line_len;
	fn = get_fn();
	fp = fopen(fn, "r");
	char* line = NULL;
	if (fp == NULL) {
		fprintf(stderr, "could not open state file for reading: %s\n", fn);
		perror("fopen");
		free(fn);
		return;
	}
	printf("loading state from %s...\n", fn);
	while (getline(&line, &line_len, fp) >= 0) {
		if (line[0] == '#')
			continue;
		separator = strstr(line, "=");
		comment = strstr(line, "#");
		newline = strstr(line, "\n");
		*newline = '\0'; /* remove newline character */
		if ((separator != NULL) && ((comment == NULL) || (separator < comment)))
		{
			++entries;
			key = line;
			key[separator - line] = '\0';
			if (comment != NULL)
				line[comment - line] = '\0';
			value = line + (separator - line) + 1;
			//printf("%s:%s", key, value);
			keys = realloc(keys, sizeof(char *) * entries);
			values = realloc(values, sizeof(char *) * entries);
			keys[entries - 1] = malloc(strlen(key) + 1);
			values[entries - 1] = malloc(strlen(value) + 1);
			strcpy(keys[entries - 1], key);
			strcpy(values[entries - 1], value);
		}
		free(line);
		line = NULL;
	}
	
	core_load();
	memory_load();
	cart_load();
	display_load();
	sound_load();
	
	for (i = 0; i < entries; i++) {
		free(keys[i]);
		free(values[i]);
	}
	free(keys);
	free(values);
	keys = NULL;
	values = NULL;
	entries = 0;
	fclose(fp);
	fp = NULL;
	free(fn);
}

void save_memory(char* key, Byte* mem, unsigned int count) {
	char* enc;
	assert(fp != NULL);
	if (err) return;
	enc = encode_base64(mem, count);
	save_string(key, enc);
	free(enc);
}

void save_string(char* key, char* s) {
	assert(fp != NULL);
	if (err) return;
	fprintf(fp, "%s=%s\n", key, s);
}

void save_int(char* key, int i) {
	assert(fp != NULL);
	if (err) return;
	fprintf(fp, "%s=%i\n", key, i);
}

void save_uint(char* key, unsigned int i) {
	assert(fp != NULL);
	if (err) return;
	fprintf(fp, "%s=%u\n", key, i);
}

void save_byte(char* key, Byte i) {
	assert(fp != NULL);
	if (err) return;
	fprintf(fp, "%s=%hhx\n", key, i);
}

void save_word(char* key, Word i) {
	assert(fp != NULL);
	if (err) return;
	fprintf(fp, "%s=%hx\n", key, i);
}

void save_float(char* key, float f) {
	assert(fp != NULL);
	if (err) return;
	fprintf(fp, "%s=%f\n", key, f);
}

void load_memory(char* key, Byte* mem, unsigned int count) {
	int i;
	assert(fp != NULL);
	i = get_index(key);
	decode_base64(values[i], mem, count);
}

char* load_string(char* key) {
	int i;
	assert(fp != NULL);
	i = get_index(key);
	return values[i];
}

unsigned int load_uint(char* key) {
	int i;
	unsigned int v;
	assert(fp != NULL);
	i = get_index(key);
	if (sscanf(values[i], "%u", &v) != 1) {
		fprintf(stderr, "not uint in key: \"%s\"\n value: \"%s\"\n", key, values[i]);
		exit(1);
	}
	return v;
}

int load_int(char* key) {
	int i;
	int v;
	assert(fp != NULL);
	i = get_index(key);
	if (sscanf(values[i], "%i", &v) != 1) {
		fprintf(stderr, "not int in key: \"%s\"\n value: \"%s\"\n", key, values[i]);
		exit(1);
	}
	return v;
}

Byte load_byte(char* key) {
	int i;
	Byte v;
	assert(fp != NULL);
	i = get_index(key);
	if (sscanf(values[i], "%hhx", &v) != 1) {
		fprintf(stderr, "not uint in key: \"%s\"\n value: \"%s\"\n", key, values[i]);
		exit(1);
	}
		
	return v;
}

Word load_word(char* key) {
	int i;
	Word v;
	assert(fp != NULL);
	i = get_index(key);
	if (sscanf(values[i], "%hx", &v) != 1) {
		fprintf(stderr, "not int in key: \"%s\"\n value: \"%s\"\n", key, values[i]);
		exit(1);	
	}
	return v;
}



static int get_index(char* key) {
	int i;
	for (i = 0; i < entries; i++) {
		if (!strcmp(keys[i], key))
			return i;
	}
	fprintf(stderr, "key: %s not found in file\n", key);
	exit(1);
}

/* generates a name for a saved state file, can be used for saving or loading
 * uses the rom title, and save slot number.
 * returns a pointer to a dynamically allocated string, which must be free()'d.
 */
static char* get_fn() {
	extern Cart cart;
	char *fn;
	unsigned int fn_len;
	assert(save_slot < 100);
	assert(cart.is_loaded);
	fn_len = strlen(cart.rom_title) + 2 + strlen(ext) + 1;
	fn = malloc(fn_len * sizeof(char));
	sprintf(fn, "%s%02u%s", cart.rom_title, save_slot, ext);
	return fn;
}

/*
static void check_write_error(size_t c, unsigned int count) {
	// TODO: check for EOF?!
	if (c < count) {
		fprintf(stderr, "error: wrote only %zu of %u bytes of memory to state file\n", c, count);
		perror("fprintf");
		fclose(fp);
		fp = NULL;
		err = 1;
	}
}
*/

#define INVALID		0x7f
#define PADDING		0x7e

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char pad = '=';

static char* encode_base64(unsigned char* raw, unsigned int c) {
	int i, j;
	int enc_len;
	char* enc;
	uint32_t b;
	unsigned int enc_i;
	enc_len = sizeof(char) * ((((c / 3) + !!(c % 3)) * 4) + 1);
	enc = malloc(enc_len);
	for (i = 0; i < (c - 2); i += 3) {
		enc_i = (i / 3) * 4;
		b = 0 | (raw[i] << 16) | (raw[i + 1] << 8) | raw[i + 2];
		for (j = 0; j < 4; j++) {
			enc[enc_i + j] = b64[(b & 0x00fc0000 >> (j * 6)) >> ((3 - j) * 6)];
		}
	}
	/* encode any remaining bytes */
	enc_i += 4;
	switch(c - i) {	
		case 0:
			break;
		case 1:
			b = 0 | (raw[i] << 16);
			enc[enc_i + 0] = b64[(b & 0x00fc0000) >> 18];
			enc[enc_i + 1] = b64[(b & 0x0003f000) >> 12];
			enc[enc_i + 2] = pad;
			enc[enc_i + 3] = pad;
			break;
		case 2:
			b = 0 | (raw[i] << 16) | (raw[i + 1] << 8);
			enc[enc_i + 0] = b64[(b & 0x00fc0000) >> 18];
			enc[enc_i + 1] = b64[(b & 0x0003f000) >> 12];
			enc[enc_i + 2] = b64[(b & 0x00000fc0) >> 6];
			enc[enc_i + 3] = pad;
			break;
	}
	enc[enc_len - 1] = '\0';
	return enc;
}

static void decode_base64(char* enc, unsigned char* raw, unsigned int count) {
	int i, j, padding;
	int enc_len = strlen(enc);
	uint32_t b;
	unsigned char c[4];
	unsigned int raw_i;
	if ((enc_len % 4) != 0) {
		fprintf(stderr, "invalid base64 string (length: %u)\n", enc_len);
		fprintf(stderr, "%s\n", enc);
		exit(1);
	}
	padding = 0;
	for (i = 0; i < enc_len; i += 4) {
		if (padding) {
			fprintf(stderr, "invalid base64 padding");
			exit(1);
		}
		b = 0;
		for (j = 0; j < 4; j++) {
			c[j] = base64_lookup(enc[i + j]);
			if (c[j] == INVALID) {
				fprintf(stderr, "invalid base64 character: %02hhx at %i\n", enc[i + j], i + j);
				exit(1);
			}
			if (c[j] == PADDING)
				++padding;
			b |= ((c[j] & 0x3f) << (6 * (3 - j)));
		}
		if (padding > 2) {
			fprintf(stderr, "invalid base64 padding\n");
			exit(1);
		}
		raw_i = (i / 4) * 3;
		if ((raw_i + 3 - padding) > count) {
			fprintf(stderr, "base64 data too long\n");
			exit(1);
		}
		for (j = 0; j < 3 - padding; j++) {
			raw[raw_i + j] = (b & (0x00ff0000 >> (j * 8))) >> ((2 - j) * 8);
		}
	}
	if ((raw_i + 3 - padding) < count) {
			fprintf(stderr, "base64 data too short\n");
			exit(1);
		}
}

static char base64_lookup(char c) {
	int i;
	for (i = 0; i < 64; i++)
		if (b64[i] == c)
			return i;
	if (c == pad)
		return PADDING;
	return INVALID;
}
