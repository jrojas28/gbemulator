/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * debug.c
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
#include <string.h>
#include "debug.h"
#include "memory.h"

char *replace_substring(char *string, const char *substring_1, 
                        const char *substring_2);

static char **mnemonics;
static char **operands;
static unsigned int *opcodes;
static unsigned int *length;
static unsigned int *cycles;
static unsigned int entries;

void debug_init() {
	char buffer[256];
	char name[6];
	char args[15];
	char fn[] = "gb.tab";
	int c;
	unsigned int opcode;
	unsigned int byte;
	unsigned int cycle;
	FILE *fp;
	entries = 0;
	
	fp = fopen(fn, "r");
	if (fp == NULL) {
		fprintf(stderr, "could not open disassembly table file: %s\n", fn);
		return;
	}
	printf("loading disassembly table file: %s\n", fn);
	// FIXME: lines longer than 256 will mess this up (getline()? - posix only)
	while(fgets(buffer, 256, fp)) {
		if (buffer[0] == ';')
			continue;
		c = sscanf(buffer, "%5s%14s%x%u%u", name, args, &opcode, &byte, &cycle);
		if (c == 5) {
			++entries;
			mnemonics = (char **) realloc(mnemonics, sizeof(char *) * (entries));
			operands = (char **) realloc(operands, sizeof(char *) * (entries));
			opcodes = (unsigned int *) realloc(opcodes, sizeof(unsigned int) * (entries));
			length = (unsigned int *) realloc(length, sizeof(unsigned int) * (entries));
			cycles = (unsigned int *) realloc(cycles, sizeof(unsigned int) * (entries));
			mnemonics[entries - 1] = malloc((strlen(name) + 1) * sizeof(char));
			strcpy(mnemonics[entries - 1], name);
			operands[entries - 1] = malloc((strlen(args) + 1) * sizeof(char));
			strcpy(operands[entries - 1], args);
			opcodes[entries - 1] = opcode;
			length[entries - 1] = byte;
			cycles[entries - 1] = cycle;
		}
	}
	fclose(fp);
}

void disasm_instr(Word address) {
	int i;
	unsigned int opcode = readb(address);
	unsigned int opcode_size;
	char buffer[16];
    char *temp;
	fprintf(stdout, "%04hx:\t", address);
	for (i = 0; i < entries; i++) {
		if (opcode == opcodes[i])
			break;
	}
	if (i == entries) {
		fprintf(stdout, "invalid instruction\n");
		return;
	}
	fprintf(stdout, "%6s\t", mnemonics[i]);
	/* dont bother printing "", just return */
	if (strcmp(operands[i], "\"\"") == 0) {
		fprintf(stdout, "            \t\t");
		return;
	}
	/* '*' means immediate, so we must replace it as such */
    if (strstr(operands[i], "*") != NULL) {
        opcode_size = (opcodes[i] / 0x100) + 1;
        if ((length[i] - opcode_size) == 1) {
            snprintf(buffer, 16, "%02hhx", readb(address + opcode_size));
        } else {
            snprintf(buffer, 16, "%04hx", readw(address + opcode_size));
        }
        temp = replace_substring(operands[i], "*", buffer);
		fprintf(stdout, "%12s\t\t", temp);
		free(temp);
        return;
    }
        if (strstr(operands[i], "@") != NULL) {
        int rel_addr;
        opcode_size = (opcodes[i] / 0x100) + 1;
        rel_addr = readb(address + opcode_size);
        snprintf(buffer, 16, "%02hhX", readb(address + opcode_size));
        temp = replace_substring(operands[i], "@", buffer);
		fprintf(stdout, "%12s\t\t", temp);
		free(temp);
        return;
    }   
    
	
	
	fprintf(stdout, "%12s\t\t", operands[i]);
	fprintf(stdout, "");
	

}


/* replaces first occurance of substring_1 in string with substring_2
 * returns pointer to dynamically allocated string with the result.
 * remember to free this! note: null string returned if substring1 isnt in
 * string.
 */
char *replace_substring(char *string, const char *substring_1, 
                        const char *substring_2) {
    char *temp;
    char *new_string;
    temp = strstr(string, substring_1);
    if (temp == NULL) {
        return NULL;
    }
    new_string = malloc(strlen(string) + 1 - strlen(substring_1) 
                            + strlen (substring_2));
    strcpy(new_string, string);
    strcpy(new_string + (temp - string), substring_2);
    strcpy (new_string + (temp - string) + strlen(substring_2), 
                temp + strlen(substring_1));
    return new_string;
}


