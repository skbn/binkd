/*
 * portable.h -- Portability layer for standalone binkd misc tools
 *
 * portable.h is a part of binkd project
 *
 * This is the single canonical portable.h; all misc utilities include this
 * C89 strict. Covers AmigaOS 3, POSIX, Win32, OS/2, DOS
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 *
 */

#ifndef BINKD_PORTABLE_H
#define BINKD_PORTABLE_H

/* _POSIX_C_SOURCE for opendir/readdir/localtime_r under -std=c89
 * _XOPEN_SOURCE 500 additionally exposes realpath() on glibc */
#ifndef AMIGA
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#ifdef AMIGA

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <sys/stat.h> /* stat() / struct stat via libnix/ADE */
#include <sys/types.h>
#include "amiga/dirent.h" /* opendir / readdir / closedir */

/* snprintf/vsnprintf: ADE/libnix declares them in stdio.h (already included
 * above via <stdio.h>). The implementation is provided by snprintf.c which
 * must be linked when building the misc tools. No redeclaration needed */

#elif defined(VISUALCPP)
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "nt/dirwin32.h" /* opendir/readdir/closedir for MSVC */
#elif defined(__MINGW32__) || defined(WIN32)
#include <stdlib.h>
#include <dirent.h> /* MinGW provides dirent.h natively */
#include <sys/stat.h>
#include <sys/types.h>
#elif defined(OS2) && (defined(IBMC) || defined(__WATCOMC__))
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "os2/dirent.h" /* opendir/readdir/closedir for OS/2 ICC/WC */
#elif defined(OS2)
#include <stdlib.h>
#include <dirent.h> /* EMX provides dirent.h natively */
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(DOS)
#include <sys/stat.h>
#include <sys/types.h>
#include "dos/dirent.h" /* opendir/readdir/closedir for DOS/DJGPP */
#else                   /* POSIX / *nix */
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/param.h>
#endif

#ifndef MAXPATHLEN
#if defined(_MAX_PATH)
#define MAXPATHLEN _MAX_PATH
#elif defined(PATH_MAX)
#define MAXPATHLEN PATH_MAX
#else
#define MAXPATHLEN 1024
#endif
#endif

/* Generic line buffer size for config files and text processing */
#ifndef MAX_LINE
#define MAX_LINE 1024
#endif

/* path_exists / mkdir_one -- native implementations per OS */
int port_path_exists(const char *p);
int port_mkdir_one(const char *p);
#define path_exists(p) port_path_exists(p)
#define mkdir_one(p) port_mkdir_one(p)

/* safe_localtime -- thread-safe localtime, portable across all OS */
void safe_localtime(const time_t *t, struct tm *tm);

/* mkdir_recursive -- create full path, making all missing components */
#define MP_MAXPATH 512
int mkdir_recursive(const char *path);

/* safe_strncpy -- strncpy that always NUL-terminates */
void safe_strncpy(char *dst, const char *src, int dstsize);

/* String utilities */
void trim_nl(char *s);
void str_trim(char *s);
void str_upper(char *s);
void str_tolower(char *s);
char *skip_ws(char *s);

/* Wildcard matching */
int wildmatch(const char *pat, const char *str);
int is_wildcard(const char *s);

/* File operations */
int ensure_dir(const char *path);
int copy_file(const char *src, const char *dst);
int move_file(const char *src, const char *dst);
long get_file_size(const char *path);
long get_file_mtime(const char *path);

/* Path utilities */
void path_join(char *out, int outsize, const char *base, const char *sub);
int make_abs_path(const char *src, char *dst, int dstlen);

#endif /* BINKD_PORTABLE_H */
