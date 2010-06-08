## Process this file with automake to produce Makefile.in
## Created by Anjuta

SUBDIRS = src 

gbemdocdir = ${prefix}/doc/gbem
gbemdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS


EXTRA_DIST = $(gbemdoc_DATA)


# Remove doc directory on uninstall
uninstall-local:
	-rm -r $(gbemdocdir)
