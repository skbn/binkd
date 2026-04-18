#include <dos/dos.h>
#include <proto/dos.h>
#include <errno.h>

#include <string.h>
#include <stdlib.h> /* atoi */

#define PATHBUF 512

int o_rename(char *from, char *to)
{
    BPTR lock;
    struct FileInfoBlock *fib;

    char dir[PATHBUF];
    char base[PATHBUF];
    char newname[PATHBUF];
    const char *s;
    char *slash;
    ULONG max = 0;
    BPTR dirlock;
    char *d = NULL;
    const char *src = NULL;
    ULONG n = 0;

    /* Try direct rename first */
    if (Rename((STRPTR)from, (STRPTR)to))
        return 0;

    /* Split path */
    slash = strrchr(to, '/');
    if (!slash)
        slash = strrchr(to, ':');

    if (slash)
    {
        ULONG len = slash - to;
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

    /* Lock directory */
    dirlock = Lock((STRPTR)dir, ACCESS_READ);
    if (!dirlock)
    {
        errno = ENOENT;
        return -1;
    }

    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);

    if (!fib)
    {
        UnLock(dirlock);
        errno = ENOMEM;
        return -1;
    }

    /* Scan directory safely under lock */
    if (Examine(dirlock, fib))
    {
        while (ExNext(dirlock, fib))
        {
            if (strncmp(fib->fib_FileName, base, strlen(base)) == 0)
            {
  				const char *p = NULL;

           		if (strncmp(fib->fib_FileName, base, strlen(base)) != 0)
                    continue;
	
         	    p = fib->fib_FileName + strlen(base);

                if (*p != '.') continue;
                
                /* .001 style */
                if (isdigit((UBYTE)p[1]) && isdigit((UBYTE)p[2]) && isdigit((UBYTE)p[3]))
                {
                    n = (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
                    if (n > max) max = n;
                }

                /* FIDO volume style (.mo0 .th1 etc) */
                if (isdigit((UBYTE)p[1]) && !isdigit((UBYTE)p[2]))
                {
                    n = p[1] - '0';
                    if (n > max) max = n;
                }
            }
        }
    }

    FreeDosObject(DOS_FIB, fib);
    UnLock(dirlock);

    /* Build new name */
    d = newname;
    src = to;
    n = max + 1;

    if (n > 999) n = 0;

    /* Copy base */
    while (*src && (d - newname) < (PATHBUF - 5))
        *d++ = *src++;

    *d++ = '.';

    /* Manual 3-digit write */
    *d++ = '0' + (n / 100);
    n %= 100;
    *d++ = '0' + (n / 10);
    *d++ = '0' + (n % 10);
    *d = '\0';

    /* Rename */
    if (Rename((STRPTR)from, (STRPTR)newname))
        return 0;

    errno = EACCES;
    return -1;
}
