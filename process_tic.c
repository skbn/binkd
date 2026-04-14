/* gcc -O2 -noixemul -o process_tic process_tic.c */

/*
 * process_tic [inbound [filebox]] [--copypublic]
 *
 * exec "process_tic " *.tic *.TIC
 * exec "process_tic --copypublic" *.tic *.TIC
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>

#define MAX_PATH	512
#define MAX_LINE	512

static int my_toupper(int c)
{
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';

    return c;
}

static int my_strnicmp(const char *a, const char *b, int n)
{
    int i, ca, cb;

    for (i = 0; i < n; i++)
    {
        ca = my_toupper((unsigned char)a[i]);
        cb = my_toupper((unsigned char)b[i]);

        if (ca != cb) return ca - cb;
        if (ca == 0)  return 0;
    }
    return 0;
}

static void trim_nl(char *s)
{
    char *p;

    p = strchr(s, '\n'); if (p) *p = '\0';
    p = strchr(s, '\r'); if (p) *p = '\0';
}

static char *skip_ws(char *s)
{
    while (*s == ' ' || *s == '\t')
		s++;

    return s;
}

static void build_path(char *out, int outsize, const char *base, const char *sub)
{
    int blen, avail;
    char last;

    strncpy(out, base, outsize - 1);
    out[outsize - 1] = '\0';

    blen = (int)strlen(out);
    last = (blen > 0) ? out[blen - 1] : '\0';

    if (last != '/' && last != ':')
    {
        avail = outsize - 1 - blen;

        if (avail > 0)
			strncat(out, "/", avail);
    }

    avail = outsize - 1 - (int)strlen(out);

    if (avail > 0)
		strncat(out, sub, avail);
}

static int ensure_dir(const char *path)
{
    BPTR lock;

    lock = Lock((STRPTR)path, ACCESS_READ);

    if (lock)
	{
		UnLock(lock);
		return 1;
	}

    printf("  MKDIR: %s\n", path);

    lock = CreateDir((STRPTR)path);

    if (!lock)
    {
        printf("  ERROR mkdir: %s (IoErr=%ld)\n", path, (long)IoErr());
        return 0;
    }

    UnLock(lock);

    return 1;
}

static int move_file(const char *src, const char *dst)
{
    DeleteFile((STRPTR)dst);

    if (Rename((STRPTR)src, (STRPTR)dst))
		return 1;

    printf("  ERROR move: %s -> %s (IoErr=%ld)\n", src, dst, (long)IoErr());

    return 0;
}

static int parse_file_field(char *line, char *out, int outsize)
{
    char *p;
    char *end;
    int len;

    p = skip_ws(line);

    if (my_strnicmp(p, "File", 4) != 0)
		return 0;

    p += 4;

    if (*p != ' ' && *p != '\t')
		return 0;

    p = skip_ws(p);
    trim_nl(p);
    end = p;

    while (*end && *end != ' ' && *end != '\t')
		end++;

    *end = '\0';

    len = (int)strlen(p);

    if (len <= 0 || len >= outsize)
		return 0;

    strncpy(out, p, outsize - 1);
    out[outsize - 1] = '\0';

    return 1;
}

static int parse_area_field(char *line, char *out, int outsize)
{
    char *p;
    char *end;
    int len;

    p = skip_ws(line);

    if (my_strnicmp(p, "Area", 4) != 0)
		return 0;

    p += 4;

    if (*p != ' ' && *p != '\t')
		return 0;

    p = skip_ws(p);
    trim_nl(p);
    end = p;

    while (*end && *end != ' ' && *end != '\t')
		end++;

    *end = '\0';
    len = (int)strlen(p);

    if (len <= 0 || len >= outsize)
		return 0;

    strncpy(out, p, outsize - 1);
    out[outsize - 1] = '\0';

    return 1;
}

static void process_one_tic(const char *ticpath, const char *inbound, const char *filebox,
                            int copypublic, const char *pubdir)
{
    FILE *f;
    char line[MAX_LINE];
    char file_name[MAX_PATH];
    char area_name[MAX_PATH];
    char src_path[MAX_PATH];
    char area_dir[MAX_PATH];
    char dst_path[MAX_PATH];
    BPTR chk;

    file_name[0] = '\0';
    area_name[0] = '\0';

    printf("\nTIC: %s\n", ticpath);

    f = fopen(ticpath, "r");

    if (!f)
	{
		printf("  ERROR: The TIC cannot be opened\n");
		return;
	}

    while (fgets(line, sizeof(line), f))
    {
        if (!file_name[0]) parse_file_field(line, file_name, sizeof(file_name));
        if (!area_name[0]) parse_area_field(line, area_name, sizeof(area_name));
    }

    fclose(f);

    if (!file_name[0])
	{
		printf("  ERROR: without File field in TIC\n");
		 return;
	}

    if (!area_name[0])
	{
		printf("  ERROR: without Area field in TIC\n");
		return;
	}

    printf("  File : %s\n", file_name);
    printf("  Area : %s\n", area_name);

    build_path(src_path, sizeof(src_path), inbound,  file_name);
    build_path(area_dir, sizeof(area_dir), filebox,  area_name);
    build_path(dst_path, sizeof(dst_path), area_dir, file_name);

    printf("  Src  : %s\n", src_path);
    printf("  Dst  : %s\n", dst_path);

    /* Verify that the source file exists before taking action. */
    chk = Lock((STRPTR)src_path, ACCESS_READ);

    if (!chk)
    {
        printf("  ERROR: archivo no encontrado en inbound: %s\n", src_path);
        return;
    }

    UnLock(chk);

    if (!ensure_dir(filebox))  return;
    if (!ensure_dir(area_dir)) return;

    /* --copypublic: copy to PROGDIR:pub/ before moving/deleting */
    if (copypublic && pubdir && pubdir[0])
    {
        char pub_dst[MAX_PATH];
        BPTR src_lock, dst_lock;

        build_path(pub_dst, sizeof(pub_dst), pubdir, file_name);

        if (ensure_dir(pubdir))
        {
            /* Use AmigaDOS Copy via a lock - simplest: open/write loop */
            src_lock = Open((STRPTR)src_path, MODE_OLDFILE);

            if (src_lock)
            {
                dst_lock = Open((STRPTR)pub_dst, MODE_NEWFILE);

                if (dst_lock)
                {
                    char cbuf[4096];
                    LONG rd;

                    while ((rd = Read(src_lock, cbuf, sizeof(cbuf))) > 0)
                        Write(dst_lock, cbuf, rd);

                    Close(dst_lock);

                    printf("  PUB  : copied to %s\n", pub_dst);
                }
                else printf("  WARN : cannot create pub copy: %s\n", pub_dst);

                Close(src_lock);
            }
            else printf("  WARN : cannot open source for pub copy: %s\n", src_path);
        }
    }

    if (!move_file(src_path, dst_path))
		return;

    printf("  OK   : moved to %s\n", dst_path);

    if (!DeleteFile((STRPTR)ticpath))
        printf("  WARN : The TIC could not be deleted (IoErr=%ld)\n", (long)IoErr());
    else
        printf("  OK   : TIC erased\n");
}

static int is_tic_file(const char *name)
{
    int len = (int)strlen(name);

    if (len < 5) return 0;

    return (my_strnicmp(name + len - 4, ".tic", 4) == 0);
}

int main(int argc, char *argv[])
{
    char inbound[MAX_PATH];
    char filebox[MAX_PATH];
    char ticpath[MAX_PATH];
    char progdir[MAX_PATH];
    char pubdir[MAX_PATH];
    int  copypublic = 0;
    BPTR proglock;
    BPTR dirlock;
    struct FileInfoBlock *fib;
    int found;
    int i;
    char pd[MAX_PATH];
	BPTR pl;

    /* Parse arguments:
     *   process_tic [inbound [filebox]] [--copypublic]
     *
     *   --copypublic  Before deleting the file from inbound, copy it to
     *                 PROGDIR:pub/ (creating pub/ if needed). This makes
     *                 received files available to srifreq for file requests.
     */
    inbound[0] = '\0'; filebox[0] = '\0'; pubdir[0] = '\0';

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--copypublic") == 0) {
            copypublic = 1;
        } else if (!inbound[0]) {
            strncpy(inbound, argv[i], sizeof(inbound)-1);
            inbound[sizeof(inbound)-1] = '\0';
        } else if (!filebox[0]) {
            strncpy(filebox, argv[i], sizeof(filebox)-1);
            filebox[sizeof(filebox)-1] = '\0';
        }
    }

    if (!inbound[0] || !filebox[0])
    {
        proglock = GetProgramDir();

        if (!proglock) {
			printf("ERROR: GetProgramDir failed\n");
			return 1;
		}

        if (!NameFromLock(proglock, (STRPTR)progdir, sizeof(progdir))) {
            printf("ERROR: NameFromLock failed (IoErr=%ld)\n", (long)IoErr());
            return 1;
        }

        if (!inbound[0]) build_path(inbound, sizeof(inbound), progdir, "inbound");
        if (!filebox[0]) build_path(filebox, sizeof(filebox), progdir, "filebox");
    }

    /* pub dir is always relative to PROGDIR: for --copypublic */
  
    pl = GetProgramDir();

    if (pl && NameFromLock(pl, (STRPTR)pd, sizeof(pd)))
	    build_path(pubdir, sizeof(pubdir), pd, "pub");

    printf("process_tic - TIC processor AmigaOS\n");
    printf("Inbound : %s\n", inbound);
    printf("Filebox : %s\n", filebox);

    if (copypublic) printf("PubDir  : %s (--copypublic)\n", pubdir);

    fib = (struct FileInfoBlock *) AllocMem(sizeof(struct FileInfoBlock), MEMF_PUBLIC | MEMF_CLEAR);

    if (!fib)
    {
        printf("ERROR: without memory for FileInfoBlock\n");
        return 1;
    }

    dirlock = Lock((STRPTR)inbound, ACCESS_READ);
    if (!dirlock)
    {
        printf("ERROR: inbound cannot be opened: %s (IoErr=%ld)\n", inbound, (long)IoErr());
        FreeMem(fib, sizeof(struct FileInfoBlock));
        return 1;
    }

    if (!Examine(dirlock, fib))
    {
        printf("ERROR: Examine inbound failure (IoErr=%ld)\n", (long)IoErr());
        UnLock(dirlock);
        FreeMem(fib, sizeof(struct FileInfoBlock));
        return 1;
    }

    found = 0;
    while (ExNext(dirlock, fib))
    {
        if (fib->fib_DirEntryType < 0 && is_tic_file(fib->fib_FileName))
        {
            build_path(ticpath, sizeof(ticpath), inbound, fib->fib_FileName);
            process_one_tic(ticpath, inbound, filebox, copypublic, pubdir);
            found++;
        }
    }

    UnLock(dirlock);
    FreeMem(fib, sizeof(struct FileInfoBlock));

    printf("\nProcessed: %d TIC(s).\n", found);
    return 0;
}
