/*
 * dirent.c -- POSIX directory scanning for AmigaOS 3
 *
 * dirent.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#ifdef AMIGA

#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "amiga/dirent.h"

/* opendir -- locks directory and allocates state for readdir() */
DIR *opendir(const char *path)
{
    DIR *dir;

    if (!path)
    {
        errno = EINVAL;
        return NULL;
    }

    dir = (DIR *)AllocMem((LONG)sizeof(DIR), MEMF_CLEAR);

    if (!dir)
    {
        errno = ENOMEM;
        return NULL;
    }

    dir->fib = (struct FileInfoBlock *)AllocMem((LONG)sizeof(struct FileInfoBlock), MEMF_CLEAR);

    if (!dir->fib)
    {
        FreeMem(dir, (LONG)sizeof(DIR));
        errno = ENOMEM;
        return NULL;
    }

    dir->lock = Lock((STRPTR)path, ACCESS_READ);

    if (!dir->lock)
    {
        FreeMem(dir->fib, (LONG)sizeof(struct FileInfoBlock));
        FreeMem(dir, (LONG)sizeof(DIR));
        errno = ENOENT;
        return NULL;
    }

    /* Examine the directory itself; this positions FIB for ExNext() */
    if (!Examine(dir->lock, dir->fib))
    {
        UnLock(dir->lock);
        FreeMem(dir->fib, (LONG)sizeof(struct FileInfoBlock));
        FreeMem(dir, (LONG)sizeof(DIR));
        errno = EACCES;
        return NULL;
    }

    /* fib_DirEntryType > 0 means this IS a directory */
    if (dir->fib->fib_DirEntryType <= 0)
    {
        UnLock(dir->lock);
        FreeMem(dir->fib, (LONG)sizeof(struct FileInfoBlock));
        FreeMem(dir, (LONG)sizeof(DIR));
        errno = ENOTDIR;
        return NULL;
    }

    dir->first = 1; /* ExNext() has not been called yet */

    return dir;
}

/* readdir -- advances to next directory entry */
struct dirent *readdir(DIR *dir)
{
    LONG dos_rc;
    LONG dos_err;

    if (!dir)
    {
        errno = EINVAL;
        return NULL;
    }

    /* ExNext() advances past the last entry returned by Examine/ExNext */
    dos_rc = ExNext(dir->lock, dir->fib);

    if (!dos_rc)
    {
        dos_err = IoErr();

        if (dos_err == ERROR_NO_MORE_ENTRIES)
            return NULL;

        errno = EIO;
        return NULL;
    }

    /* Copy name into the entry buffer */
    strncpy(dir->entry.d_name, dir->fib->fib_FileName, AMIGA_NAME_MAX - 1);
    dir->entry.d_name[AMIGA_NAME_MAX - 1] = '\0';
    dir->entry.d_ino = 0;

    return &dir->entry;
}

/* closedir -- releases all resources associated with dir */
int closedir(DIR *dir)
{
    if (!dir)
    {
        errno = EINVAL;
        return -1;
    }

    if (dir->lock)
        UnLock(dir->lock);

    if (dir->fib)
        FreeMem(dir->fib, (LONG)sizeof(struct FileInfoBlock));

    FreeMem(dir, (LONG)sizeof(DIR));
    return 0;
}

#endif /* AMIGA */
