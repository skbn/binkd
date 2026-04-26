/*
 * utime.c -- utime() stub for AmigaOS 3 without ixemul
 *
 * utime.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#ifdef AMIGA

#include <dos/dos.h>
#include <proto/dos.h>
#include <errno.h>
#include <time.h>

#include "amiga/dirent.h" /* declares struct utimbuf and utime() prototype */

/* Days between AmigaDOS epoch (1978-01-01) and POSIX epoch (1970-01-01) */
#define AMIGA_EPOCH_DELTA_DAYS 2922UL
#define SECONDS_PER_DAY 86400UL
#define SECONDS_PER_MINUTE 60UL

int utime(const char *path, const struct utimbuf *times)
{
    struct DateStamp ds;
    LONG seconds_today;
    LONG total_seconds;

    if (!path)
    {
        errno = EINVAL;
        return -1;
    }

    if (!times)
    {
        /* Use current time */
        DateStamp(&ds);
    }
    else
    {
        LONG t = (LONG)times->modtime;

        if (t < (LONG)(AMIGA_EPOCH_DELTA_DAYS * SECONDS_PER_DAY))
        {
            /* Time predates AmigaDOS epoch -- clamp to epoch */
            t = 0;
        }
        else
        {
            t -= (LONG)(AMIGA_EPOCH_DELTA_DAYS * SECONDS_PER_DAY);
        }

        ds.ds_Days = (LONG)(t / (LONG)SECONDS_PER_DAY);
        total_seconds = t % (LONG)SECONDS_PER_DAY;
        ds.ds_Minute = (LONG)(total_seconds / (LONG)SECONDS_PER_MINUTE);
        seconds_today = total_seconds % (LONG)SECONDS_PER_MINUTE;
        ds.ds_Tick = seconds_today * (LONG)TICKS_PER_SECOND;
    }

    if (!SetFileDate((STRPTR)path, &ds))
    {
        /* Most likely cause: file does not exist or is write-protected */
        LONG err = IoErr();

        if (err == ERROR_OBJECT_NOT_FOUND || err == ERROR_DIR_NOT_FOUND)
            errno = ENOENT;
        else
            errno = EACCES;
        return -1;
    }

    return 0;
}

#endif /* AMIGA */
