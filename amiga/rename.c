#include <dos/dos.h>
#include <proto/dos.h>
#include <errno.h>
#include <string.h>

int o_rename(char *from, char *to)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    char dir[512];
    char base[256];
    char newname[512];
    char *slash;
    unsigned int max = 0;

    if (Rename((STRPTR)from, (STRPTR)to))
        return 0;

    slash = strrchr(to, '/');

    if (slash)
    {
        size_t len = slash - to;
        strncpy(dir, to, len);
        dir[len] = '\0';
        strcpy(base, slash + 1);
    }
    else
    {
        strcpy(dir, "");
        strcpy(base, to);
    }

    lock = Lock((STRPTR)(dir[0] ? dir : "."), ACCESS_READ);

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

    if (Examine(lock, fib))
    {
        while (ExNext(lock, fib))
        {
            if (strncmp(fib->fib_FileName, base, strlen(base)) == 0)
            {
                const char *p = fib->fib_FileName + strlen(base);

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

    snprintf(newname, sizeof(newname), "%s.%03u", to, max + 1);

    if (Rename((STRPTR)from, (STRPTR)newname))
        return 0;

    errno = EACCES;
    return -1;
}