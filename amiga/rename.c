#include <dos/dos.h>
#include <proto/dos.h>
#include <errno.h>

#include <string.h>
#include <stdlib.h> /* atoi */
#include <stdio.h> /* snprintf */

#define PATHBUF 512

int o_rename(char *from, char *to)
{
    BPTR lock;
    struct FileInfoBlock *fib;

    char dir[PATHBUF];
    char base[PATHBUF];
    char newname[PATHBUF];
    char *slash;

    unsigned int max = 0;

    /* Direct attempt */
    if (Rename((STRPTR)from, (STRPTR)to))
        return 0;

    /* Detect separator */
    slash = strrchr(to, '/');

    if (!slash)
        slash = strrchr(to, ':');

    if (slash)
    {
        size_t len = slash - to;
        if (len >= PATHBUF) len = PATHBUF - 1;

        strncpy(dir, to, len);
        dir[len] = '\0';

        strncpy(base, slash + 1, PATHBUF - 1);
        base[PATHBUF - 1] = '\0';
    }
    else
    {
        strcpy(dir, ".");
        strncpy(base, to, PATHBUF - 1);
        base[PATHBUF - 1] = '\0';
    }

    /* Open directory */
    lock = Lock((STRPTR)dir, ACCESS_READ);

    if (!lock)
    {
        errno = ENOENT;
        return -1;
    }

    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);

    if (!fib)
    {
        UnLock(lock);
        errno = ENOMEM;
        return -1;
    }

    /* Scan directory */
    if (Examine(lock, fib))
    {
        while (ExNext(lock, fib))
        {
            if (strncmp(fib->fib_FileName, base, strlen(base)) == 0)
            {
                const char *p = fib->fib_FileName + strlen(base);

				/* Find last numeric extension */
                if (*p == '.')
                {
                    unsigned int n = atoi(p + 1);
                    if (n > max)
                        max = n;
                }
            }
        }
    }

    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);

    /* Create new name */
    snprintf(newname, PATHBUF, "%s.%03u", to, max + 1);

    /* Rename */
    if (Rename((STRPTR)from, (STRPTR)newname))
        return 0;

    errno = EACCES;

    return -1;
}