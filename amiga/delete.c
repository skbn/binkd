/*
 * delete.c -- Amiga-specific delete implementation using native DOS library
 *
 * This file provides o_delete() function that uses AmigaDOS DeleteFile()
 * instead of the standard C unlink() which doesn't work reliably on AmigaOS
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#include <dos/dos.h>
#include <proto/dos.h>
#include <errno.h>
#include <string.h>

extern void Log (int lev, char *s,...);

/*
 * Delete a file using native AmigaDOS DeleteFile()
 * Returns 0 on success, -1 on error
 */
int o_delete(char *path)
{
    BPTR lock;
    
    /* Try to delete the file directly using AmigaDOS */
    if (DeleteFile((STRPTR)path))
    {
        Log(5, "deleted `%s'", path);
        return 0;
    }
    
    /* If deletion failed, try to determine why */
    lock = Lock((STRPTR)path, ACCESS_READ);

    if (lock)
    {
        /* File exists but cannot be deleted (probably locked or protected) */
        UnLock(lock);
        Log(1, "error deleting `%s': file exists but cannot be deleted (may be locked or protected)", path);
        errno = EACCES;
    }
    else
    {
        /* File does not exist */
        Log(1, "error deleting `%s': file not found", path);
        errno = ENOENT;
    }
    
    return -1;
}
