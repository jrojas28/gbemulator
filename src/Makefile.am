## Process this file with automake to produce Makefile.in

## Created by Anjuta

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"

AM_CFLAGS =\
	 -Wall\
	 -pedantic\
	 -g\
	 -std=c99
#	 -pg

bin_PROGRAMS = gbem

gbem_SOURCES = \
	main.c\
	core.c \
	core.h \
	gbem.h \
	memory.c \
	memory.h \
	cart.c \
	cart.h \
	display.c \
	display.h \
	joypad.c \
	joypad.h \
	timer.c \
	timer.h \
	sound.c \
	sound.h \
	debug.c \
	debug.h

gbem_LDFLAGS = #-pg

gbem_LDADD = 
