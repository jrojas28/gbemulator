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

static const unsigned int tima_periods[] = {1000000000/4096, 1000000000/262144, 
	                                        1000000000/65536, 1000000000/16384};
static inline unsigned int get_tima_period();
static inline unsigned int get_div_period();


void timer_reset() {
	tima_time = 0;
	div_time = 0;
}

void timer_check(unsigned int period) {
	tima_time += period;
	div_time += period;
	// check if tima timer is enabled
	if ((tima_time >= get_tima_period()) && ((read_io(HWREG_TAC) & 0x04) != 0)){
		// increment tima
		write_io(HWREG_TIMA, read_io(HWREG_TIMA) + 1);
		// has tima overflowed?
		if (read_io(HWREG_TIMA) == 0) {
			// reset tima 
			write_io(HWREG_TIMA, read_io(HWREG_TMA));
			// generate timer interrupt
			write_io(HWREG_IF, read_io(HWREG_IF) | INT_TIMER);
		}
		tima_time = 0;
	}
	if (div_time >= get_div_period())	{
		write_io(HWREG_DIV, read_io(HWREG_DIV) + 1);
		div_time = 0;
	}
}

// This function returns the time between TIMA incrementation.
static inline unsigned int get_tima_period() {
	// is the timer stopped? If so, return 0.
	if ((readb(HWREG_TAC) & 0x04) == 0)
		return 0;
	// return the appropriate time
	return tima_periods[readb(HWREG_TAC) & 0x03];
}

static inline unsigned int get_div_period() {
	return 1000000000/16384;
}

