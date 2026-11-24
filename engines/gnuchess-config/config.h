/* config.h for building GNU Chess with qmake (mbChess), instead of
   the one that "configure" would generate. Linux and macOS only:
   GNU Chess needs POSIX pipes and select(). */
#ifndef MBCHESS_GNUCHESS_CONFIG_H
#define MBCHESS_GNUCHESS_CONFIG_H

#define PACKAGE "gnuchess"
#define PACKAGE_NAME "GNU Chess"
#define PACKAGE_TARNAME "gnuchess"
#define PACKAGE_VERSION "6.2.5e"
#define PACKAGE_STRING "GNU Chess 6.2.5e"
#define PACKAGE_BUGREPORT "bug-gnu-chess@gnu.org"
#define PACKAGE_URL "https://www.gnu.org/software/chess/"
#define VERSION "6.2.5e"

/* The book and gnuchess.ini are looked up in the working directory, which
   mbChess sets to the engine's own folder. */
#define PKGDATADIR "."
#define LOCALEDIR "."

#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_INTTYPES_H 1
#define HAVE_MALLOC 1
#define HAVE_MEMORY_H 1
#define HAVE_MEMSET 1
#define HAVE_PTHREAD 1
#define HAVE_REALLOC 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRCHR 1
#define HAVE_STRCSPN 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRSTR 1
#define HAVE_STRTOL 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_TIME_H 1
#define HAVE_UNISTD_H 1
#define HAVE__BOOL 1
#define RETSIGTYPE void
#define SIZEOF_CHAR 1
#define SIZEOF_SHORT 2
#define SIZEOF_INT 4
#define SIZEOF_LONG 8
#define SIZEOF_VOIDP 8
#define STDC_HEADERS 1
#define TIME_WITH_SYS_TIME 1

#endif
