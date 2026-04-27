/*
 * decompress.c -- Decompress FTN bundle archives from an inbound directory
 *
 * decompress.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#include "portable.h" /* Canonical portable layer */
#include <ctype.h>

#define MAX_CMD 1100

/* Archive format codes detected by magic bytes */
#define FMT_UNKNOWN 0
#define FMT_ZIP 1
#define FMT_LZH 2
#define FMT_ARC 3

/* detect_format -- Read first bytes and identify archive type */
static int detect_format(const char *path)
{
    unsigned char buf[8];
    FILE *f = fopen(path, "rb");
    int n;

    if (!f)
        return FMT_UNKNOWN;

    n = (int)fread(buf, 1, sizeof(buf), f);

    fclose(f);

    if (n < 2)
        return FMT_UNKNOWN;

    /* ZIP: PK\x03\x04 */
    if (n >= 4 && buf[0] == 0x50 && buf[1] == 0x4B && buf[2] == 0x03 && buf[3] == 0x04)
        return FMT_ZIP;

    /* LZH: offset 2 = '-', offset 3 = 'l', offset 6 = '-' (e.g. -lh5-) */
    if (n >= 7 && buf[2] == '-' && buf[3] == 'l' && buf[6] == '-')
        return FMT_LZH;

    /* ARC: 0x1A followed by type byte 1..18 */
    if (buf[0] == 0x1A && buf[1] >= 1 && buf[1] <= 18)
        return FMT_ARC;

    return FMT_UNKNOWN;
}

/* is_ftn_bundle -- Check filename has an FTN day-of-week extension */
static int is_ftn_bundle(const char *filename)
{
    const char *p;

    for (p = filename; *p; p++)
    {
        if (p[0] == '.' && p[1] && p[2])
        {
            char a = (char)tolower((unsigned char)p[1]);
            char b = (char)tolower((unsigned char)p[2]);

            if ((a == 's' && b == 'u') || (a == 'm' && b == 'o') ||
                (a == 't' && b == 'u') || (a == 'w' && b == 'e') ||
                (a == 't' && b == 'h') || (a == 'f' && b == 'r') ||
                (a == 's' && b == 'a'))
            {
                /* .TH  .TH0  .TH.001 */
                if (p[3] == '\0' || isdigit((unsigned char)p[3]) || p[3] == '.')
                    return 1;
            }
        }
    }

    return 0;
}

/* delete_file -- Remove a file, portable */
static void delete_file(const char *path)
{
#if defined(AMIGA)
    DeleteFile((STRPTR)path);
#elif defined(WIN32) || defined(__MINGW32__) || defined(VISUALCPP)
    DeleteFileA(path);
#else
    remove(path);
#endif
}

/* run_decompressor -- Invoke external tool for the detected format
 * outdir must end without trailing slash on POSIX; lha needs trailing / */
static int run_decompressor(int fmt, const char *path, const char *outdir)
{
    char cmd[MAX_CMD];
    const char *basename;

    /* Security: validate basename for invalid characters (not full path) */
    /* Find the last path separator (/ : \) to extract basename */
    basename = strrchr(path, '/');
    
    if (!basename)
        basename = strrchr(path, ':'); /* Amiga volume separator */

    if (!basename)
        basename = strrchr(path, '\\'); /* Windows separator */

    if (!basename)
        basename = path;
    else
        basename++; /* Skip the separator */

    if (!is_safe_filename(basename))
    {
        fprintf(stderr, "decompress: invalid filename: %s\n", basename);
        return -1;
    }

    /* Validate outdir is a directory (multiplatform) */
    if (!is_directory(outdir))
    {
        fprintf(stderr, "decompress: outdir is not a directory: %s\n", outdir);
        return -1;
    }

    switch (fmt)
    {
    case FMT_ZIP:
#if defined(WIN32) || defined(__MINGW32__) || defined(VISUALCPP)
        snprintf(cmd, MAX_CMD, "unzip -o \"%s\" -d \"%s\"", path, outdir);
#else
        snprintf(cmd, MAX_CMD, "unzip -o \"%s\" -d \"%s\"", path, outdir);
#endif
        break;

    case FMT_LZH:
#ifdef AMIGA
        snprintf(cmd, MAX_CMD, "lha x \"%s\" \"%s/\"", path, outdir);
#else
        snprintf(cmd, MAX_CMD, "lha e \"%s\" \"%s/\"", path, outdir);
#endif
        break;

    case FMT_ARC:
#if defined(WIN32) || defined(__MINGW32__) || defined(VISUALCPP)
        snprintf(cmd, MAX_CMD, "arc x \"%s\" \"%s\"", path, outdir);
#else
        snprintf(cmd, MAX_CMD, "cd \"%s\" && arc x \"%s\"", outdir, path);
#endif
        break;

    default:
        return -1;
    }

    int rc = system(cmd);

    if (rc == 0 || rc == 1)
        return 0;

    return -1;
}

int main(int argc, char *argv[])
{
    DIR *dp;
    struct dirent *entry;
    char path[MAXPATHLEN];
    const char *inbound;
    const char *outdir;
    int total = 0;
    int ok = 0;

    if (argc < 3)
    {
        fprintf(stderr,
                "Usage: decompress <inbound_dir> <output_dir>\n"
                "Detects format by magic bytes (ZIP/LZH/ARC).\n"
                "Processes FTN day bundles (.SU/.MO/.TU/.WE/.TH/.FR/.SA).\n");

        return 1;
    }

    inbound = argv[1];
    outdir = argv[2];

    if (!is_directory(inbound))
    {
        fprintf(stderr, "decompress: inbound is not a directory: %s\n", inbound);
        return 1;
    }

    if (!is_directory(outdir))
    {
        fprintf(stderr, "decompress: outdir is not a directory: %s\n", outdir);
        return 1;
    }

    dp = opendir(inbound);

    if (dp == NULL)
        return 1;

    while ((entry = readdir(dp)) != NULL)
    {
        int fmt;

        /* Skip . and .. (AmigaOS readdir does not return these, POSIX does) */
        if (entry->d_name[0] == '.' && (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;

        if (!is_ftn_bundle(entry->d_name))
            continue;

        path_join(path, MAXPATHLEN, inbound, entry->d_name);

        if (!is_regular_file(path))
        {
            fprintf(stderr, "decompress: not a regular file: %s\n", path);
            continue;
        }

        fmt = detect_format(path);

        if (fmt == FMT_UNKNOWN)
            continue;

        total++;

        if (run_decompressor(fmt, path, outdir) == 0)
        {
            delete_file(path);
            ok++;
        }
    }

    closedir(dp);

    return (total == 0 || ok == total) ? 0 : 1;
}
