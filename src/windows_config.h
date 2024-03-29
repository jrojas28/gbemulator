/* windows_config.h. */

/* Define if you have the `getline' function.
 * If the compiler does not have getline, undefine this: a replacement
 * is included */
/* #  define HAVE_GETLINE */

/* Name of package */
#define PACKAGE "gbem"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "gbem"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "gbem 0.1"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "gbem"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.1"

/* Version number of package */
#define VERSION "0.1"


/* if you are compiling for a big endian target, define this.
 * No idea whether the emulator will work or not though, but it is
 * designed with endianness (somewhat) in mind */
/* #  define WORDS_BIGENDIAN */

/* commented out these unused defines. Without these headers, some
 * porting will be necessary */
#if 0

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

#endif
