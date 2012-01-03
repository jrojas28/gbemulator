#ifndef _RTC_H
#define _RTC_H

#include <stdio.h>
#include "gbem.h"

void rtc_set_register(Byte r, Byte value);
Byte rtc_get_register(Byte r);

void rtc_save_sram(FILE *fp);
void rtc_load_sram(FILE *fp);
void reset_rtc();
void rtc_latch();

#endif
