#include <dos/dos.h>
#include <proto/dos.h>
#include <errno.h>

int o_rename(char *from, char *to)
{
  if (Rename((STRPTR)from, (STRPTR)to))	/* cross-volume move won't work */
  {
    return 0;
  }
  else
  {
    errno = EACCES;
    return -1;
  }
}
