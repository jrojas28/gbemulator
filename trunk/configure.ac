dnl Process this file with autoconf to produce a configure script.
dnl Created by Anjuta application wizard.

AC_INIT([gbem],[0.1])

SDL_REQUIRED=1.2.0

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

AC_CHECK_FUNCS([getline])

AC_SEARCH_LIBS([strerror],[cposix])
AC_PROG_CC
AC_PROG_CC
AC_DIAGNOSE([obsolete],[AM_PROG_CC_STDC:
	your code should no longer depend upon `am_cv_prog_cc_stdc', but upon
	`ac_cv_prog_cc_stdc'.  Remove this warning and the assignment when
	you adjust the code.  You can also remove the above call to
	AC_PROG_CC if you already called it elsewhere.])
am_cv_prog_cc_stdc=$ac_cv_prog_cc_stdc

AC_HEADER_STDC
AC_PATH_XTRA

AC_C_BIGENDIAN

#LT_INIT

dnl ***************************************************************************
dnl Check for SDL
dnl ***************************************************************************
AM_PATH_SDL($SDL_REQUIRED,
            :,
	    AC_MSG_ERROR([SDL version $SDL_REQUIRED not found]))

#PKG_CHECK_MODULES(PORTAUDIO, portaudio-2.0 >= 19, true,
#	AC_MSG_ERROR([you need portaudio-2.0 >= 19 - Please see http://www.portaudio.com ]))
#AC_SUBST(PORTAUDIO_CFLAGS)
#AC_SUBST(PORTAUDIO_LIBS)


#CXXFLAGS="$CXXFLAGS $SDL_CFLAGS $PORTAUDIO_CFLAGS"
#CFLAGS="$CFLAGS $SDL_CFLAGS $PORTAUDIO_CFLAGS"
#CFLAGS="$SDL_CFLAGS $PORTAUDIO_CFLAGS"
CFLAGS="$SDL_CFLAGS"
#CPPFLAGS="$CPPFLAGS $SDL_CFLAGS $PORTAUDIO_CFLAGS"
#LIBS="$LIBS $SDL_LIBS $PORTAUDIO_LIBS"
LIBS="$LIBS $SDL_LIBS"






AC_CONFIG_FILES([
Makefile
src/Makefile

])
AC_OUTPUT
