#include <dos/dos.h>
#include <proto/dos.h>
#include <errno.h>

int o_rename(char *from, char *to)
{
  /* Delete destination before renaming */

  if (Rename((STRPTR)from, (STRPTR)to))
    return 0;

  /* AmigaOS Rename() fails if destination already exists, unlike POSIX
   * rename() which atomically replaces it.  Delete the destination and
   * retry so that duplicate arcmail/pkt files overwrite rather than
   * accumulating as 0000p000.SA0, 0000p001.SA0 ... in inbound. */
  if (DeleteFile((STRPTR)to))
  {
    if (Rename((STRPTR)from, (STRPTR)to))
      return 0;
  }

  errno = EACCES;
  return -1;
}
