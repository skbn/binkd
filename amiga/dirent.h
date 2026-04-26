/*
 * dirent.h -- POSIX directory scanning for AmigaOS 3
 *
 * dirent.h is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#ifndef _AMIGA_DIRENT_H
#define _AMIGA_DIRENT_H

#ifdef AMIGA

#include <dos/dos.h>
#include <dos/exall.h>

/* Maximum name length: AmigaDOS allows 107 characters */
#define AMIGA_NAME_MAX 108

struct dirent
{
    unsigned long d_ino;         /* inode -- always 0 on AmigaDOS */
    char d_name[AMIGA_NAME_MAX]; /* null-terminated file name */
};

/* struct utimbuf for AmigaOS 3 without ixemul */
#ifndef _AMIGA_UTIMBUF_DEFINED
#define _AMIGA_UTIMBUF_DEFINED

struct utimbuf
{
    long actime;  /* access time (unused by SetFileDate) */
    long modtime; /* modification time (POSIX time_t) */
};

int utime(const char *path, const struct utimbuf *times);
#endif

typedef struct _amiga_dir
{
    BPTR lock;                 /* directory lock */
    struct FileInfoBlock *fib; /* reusable FileInfoBlock */
    int first;                 /* flag: first call not yet */
    struct dirent entry;       /* storage returned to caller */
} DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

#endif /* AMIGA */
#endif /* _AMIGA_DIRENT_H */
