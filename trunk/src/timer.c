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


#include "timer.h"
#include "memory.h"
#include "core.h"

static unsigned int tima_time;
static unsigned int div_time;

// periods for each tima setting, in machine cycles
static const unsigned int tima_periods[] = {1024, 16, 64, 256};

static inline unsigned int get_tima_period();
static inline unsigned int get_div_period();

void timer_reset() {
	tima_time = 0;
	div_time = 0;
}

void timer_check(unsigned int period) {
	div_time += period;
	// check if tima timer is enabled
	if (read_io(HWREG_TAC) & 0x04) {
		tima_time += period;
		while (tima_time >= get_tima_period()) {
			// increment tima
			write_io(HWREG_TIMA, read_io(HWREG_TIMA) + 1);
			// has tima overflowed?
			if (read_io(HWREG_TIMA) == 0) {
				// reset tima 
				write_io(HWREG_TIMA, read_io(HWREG_TMA));
				// generate timer interrupt
				write_io(HWREG_IF, read_io(HWREG_IF) | INT_TIMER);
			}
			tima_time -= get_tima_period();
		}
	} else {
		tima_time = 0;
	}
	
	while (div_time >= get_div_period())	{
		write_io(HWREG_DIV, read_io(HWREG_DIV) + 1);
		div_time -= get_div_period();
	}
}

// This function returns the time between TIMA incrementation.
static inline unsigned int get_tima_period() {
	// return the appropriate time
	return tima_periods[readb(HWREG_TAC) & 0x03];
}

static inline unsigned int get_div_period() {
	return 64;
}

