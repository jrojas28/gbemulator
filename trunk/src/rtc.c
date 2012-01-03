#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#include "gbem.h"

#define RTC_SUBTRACT_REG	0x08
#define RTC_REG_S			0x00
#define RTC_REG_M			0x01
#define RTC_REG_H			0x02
#define RTC_REG_DL			0x03
#define RTC_REG_DH			0x04

static void set_registers();
static void byte_swap(Byte *ptr, size_t size);

Byte regs[5];
Byte regs_latched[5];

int is_halted;

time_t start_time = 0;
time_t halt_time = 0;

void rtc_set_register(Byte r, Byte value) {
	r -= RTC_SUBTRACT_REG;
	set_registers();
	regs[r] = value;

	// has the timer just been halted?
	if ((!is_halted) && (regs[RTC_REG_DH] & 0x40)) {
		halt_time = time(NULL);
	} else
	// has the timer just been unhalted?
	if ((is_halted) && !(regs[RTC_REG_DH] & 0x40)) {
		time_t time_halted = time(NULL) - halt_time;
		start_time += time_halted;
	}
	
	is_halted = (regs[RTC_REG_DH] & 0x40);

	// if the timer is active, save the time
	if (!is_halted) {
		start_time = time(NULL);
		start_time -= regs[RTC_REG_S];
		start_time -= regs[RTC_REG_M] * 60;
		start_time -= regs[RTC_REG_H] * 60 * 60;
		start_time -= regs[RTC_REG_DL] * 60 * 60 * 24;
		start_time -= (regs[RTC_REG_DH] & 0x01) * 60 * 60 * 24 * 256;
		start_time -= ((regs[RTC_REG_DH] & 0x80) >> 7) * 60 * 60 * 24 * 256 * 2;
	}
	//fprintf(stderr, "rtc_set_register: %hhx: %hhx\n", r, value);
}

Byte rtc_get_register(Byte r) {
	r -= RTC_SUBTRACT_REG;
	//fprintf(stderr, "rtc_get_register: %hhx: %hhx\n", r, regs_latched[r]);
	return regs_latched[r];
}

void rtc_latch() {
	//fprintf(stderr, "rtc latch\n");
	set_registers();
	for (int i = 0; i < 0x05; i++) {
		regs_latched[i] = regs[i];
	}
}

static void set_registers() {
	time_t elapsed = time(NULL) - start_time;
	if (is_halted) {
		time_t time_halted = time(NULL) - halt_time;
		elapsed -= time_halted;
	}
	if (elapsed < 0) {
		fprintf(stderr, "real time clock error: clock was started in the future. Check system clock / time zone.\n");
		elapsed = 0;
	}
	regs[RTC_REG_S] = elapsed % 60;
	regs[RTC_REG_M] = (elapsed % (60 * 60)) / 60;
	regs[RTC_REG_H] = (elapsed % (60 * 60 * 24)) / (60 * 60);
	regs[RTC_REG_DL] = (elapsed % (60 * 60 * 24 * 256)) / (60 * 60 * 24);
	regs[RTC_REG_DH] = ((elapsed % (60 * 60 * 24 * 256 * 2)) / (60 * 60 * 24 * 256)) % 2;
	// has the timer overflowed?
	if ((elapsed / (60 * 60 * 24 * 256 * 2)))
		regs[RTC_REG_DH] |= 0x80;
	regs[RTC_REG_DH] |= is_halted;
}

typedef struct {
	uint64_t start_time;
	uint32_t is_halted;
	uint64_t halt_time;
} __attribute__ ((packed)) Rtc_save_block;

void rtc_save_sram(FILE *fp) {
	Rtc_save_block save_block;
	save_block.start_time = start_time;
	save_block.is_halted = is_halted;
	save_block.halt_time = halt_time;
	size_t c;

	#ifdef WORDS_BIGENDIAN
	byte_swap((Byte *)&save_block.start_time, sizeof(save_block.start_time));
	byte_swap((Byte *)&save_block.is_halted, sizeof(save_block.is_halted));
	byte_swap((Byte *)&save_block.halt_time, sizeof(save_block.halt_time));
	#endif
	
	c = fwrite(&save_block, sizeof(Byte), sizeof(save_block), fp);
	if (c < sizeof(save_block)) {
		fprintf(stderr, "error: wrote only %zu of %zu bytes of rtc to sram file\n", c, sizeof(save_block));
		perror("fwrite");
		return;
	}
}

void rtc_load_sram(FILE *fp) {
	size_t c;
	Rtc_save_block save_block;
	
	c = fread(&save_block, sizeof(Byte), sizeof(save_block), fp);
	if (c != sizeof(save_block)) {
		fprintf(stderr, "error: read only %zu of %zu bytes of rtc from sram file\n", c, sizeof(save_block));
		perror("fread");
		return;
	}

	#ifdef WORDS_BIGENDIAN
	byte_swap((Byte *)&save_block.start_time, sizeof(save_block.start_time));
	byte_swap((Byte *)&save_block.is_halted, sizeof(save_block.is_halted));
	byte_swap((Byte *)&save_block.halt_time, sizeof(save_block.halt_time));
	#endif

	start_time = save_block.start_time;
	is_halted = save_block.is_halted;
	halt_time = save_block.halt_time;
}

void reset_rtc() {
	for (int i = 0; i < 0x05; i++) {
		regs[i] = 0;
		regs_latched[i] = 0;
	}
	is_halted = 0;
	start_time = time(NULL);
	halt_time = 0;
}

static void byte_swap(Byte *ptr, size_t size) {
	Byte temp;
	for (int i = 0; i < (size / 2); i++) {
		temp = ptr[i];
		ptr[i] = ptr[size - i - 1];
		ptr[size - i - 1] = temp;
	}
}

