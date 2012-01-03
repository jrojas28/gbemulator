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

Byte regs[5];
Byte regs_latched[5];

int is_halted;

//struct tm start_time;

time_t start_time = 0;

void rtc_set_register(Byte r, Byte value) {
	r -= RTC_SUBTRACT_REG;
	regs[r] = value;
	set_registers();
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
	fprintf(stderr, "rtc_set_register: %hhx: %hhx\n", r, value);
}

Byte rtc_get_register(Byte r) {
	r -= RTC_SUBTRACT_REG;
	fprintf(stderr, "rtc_get_register: %hhx: %hhx\n", r, regs_latched[r]);
	return regs_latched[r];
}

void rtc_latch() {
	fprintf(stderr, "rtc latch\n");
	set_registers();
	for (int i = 0; i < 0x05; i++) {
		regs_latched[i] = regs[i];
	}
}

static void set_registers() {
	time_t elapsed = time(NULL) - start_time;
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

// FIXME: ENDIAN
void rtc_save_sram(FILE *fp) {
	uint64_t t = start_time;
	size_t c;
	c = fwrite(&t, sizeof(Byte), sizeof(t), fp);
	if (c < sizeof(t)) {
		fprintf(stderr, "error: wrote only %zu of %zu bytes of rtc to sram file\n", c, sizeof(t));
		perror("fwrite");
		return;
	}
}

void rtc_load_sram(FILE *fp) {
	size_t c;
	uint64_t t;
	c = fread(&t, sizeof(Byte), sizeof(t), fp);
	if (c != sizeof(t)) {
		fprintf(stderr, "error: read only %zu of %zu bytes of rtc from sram file\n", c, sizeof(t));
		perror("fread");
		return;
	}
	start_time = t;
}

void reset_rtc() {
	for (int i = 0; i < 0x05; i++) {
		regs[i] = 0;
		regs_latched[i] = 0;
	}
	is_halted = 0;
	start_time = time(NULL);
}
