/*
 * portable.c -- Shared implementations for misc tools portable layer
 *
 * portable.c is a part of binkd project
 *
 * This file provides implementations for common utility functions
 * used across binkd misc tools. Include portable.h for declarations
 * C89 strict. Covers AmigaOS 3, POSIX, Win32, OS/2, DOS
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 *
 */

#include "portable.h"
#include <ctype.h>

/* trim_nl -- Strip trailing newline (\n and \r) from string */
void trim_nl(char *s)
{
    char *p = strchr(s, '\n');

    if (p)
        *p = '\0';

    p = strchr(s, '\r');

    if (p)
        *p = '\0';
}

/* str_trim -- Strip trailing whitespace (space, \r, \n) from string */
void str_trim(char *s)
{
    int n = (int)strlen(s);

    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' '))
        s[--n] = '\0';
}

/* str_upper -- Convert string to uppercase in-place */
void str_upper(char *s)
{
    while (*s)
    {
        *s = (char)toupper((unsigned char)*s);
        s++;
    }
}

/* str_tolower -- Convert string to lowercase in-place */
void str_tolower(char *s)
{
    for (; *s; s++)
    {
        if (*s >= 'A' && *s <= 'Z')
            *s = (char)(*s + ('a' - 'A'));
    }
}

/* skip_ws -- Skip leading whitespace */
char *skip_ws(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;

    return s;
}

/* wildmatch -- Portable wildcard match: case-insensitive, supports * and ? */
int wildmatch(const char *pat, const char *str)
{
    while (*pat)
    {
        if (*pat == '*')
        {
            while (*pat == '*')
                pat++;

            if (!*pat)
                return 1;

            while (*str)
            {
                if (wildmatch(pat, str++))
                    return 1;
            }

            return 0;
        }

        if (*pat == '?')
        {
            if (!*str)
                return 0;

            pat++;
            str++;
        }
        else
        {
            if (toupper((unsigned char)*pat) != toupper((unsigned char)*str))
                return 0;

            pat++;
            str++;
        }
    }

    return (*str == '\0') ? 1 : 0;
}

/* is_wildcard -- True if name contains * or ? */
int is_wildcard(const char *s)
{
    while (*s)
    {
        if (*s == '*' || *s == '?')
            return 1;

        s++;
    }

    return 0;
}

/* ensure_dir -- Ensure directory exists, creating if necessary */
int ensure_dir(const char *path)
{
    if (path_exists(path))
        return 1;

    return (mkdir_recursive(path) == 0) ? 1 : 0;
}

/* copy_file -- Portable binary file copy */
int copy_file(const char *src, const char *dst)
{
    FILE *in, *out;
    char buf[4096];
    int n;

    in = fopen(src, "rb");

    if (!in)
        return 0;

    out = fopen(dst, "wb");

    if (!out)
    {
        fclose(in);
        return 0;
    }

    while ((n = (int)fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, (size_t)n, out);

    fclose(out);
    fclose(in);

    return 1;
}

/* move_file -- Try rename first, fall back to copy+delete */
int move_file(const char *src, const char *dst)
{
    remove(dst);

    if (rename(src, dst) == 0)
        return 1;

    if (copy_file(src, dst))
    {
        remove(src);
        return 1;
    }

    return 0;
}

/* get_file_size -- Return file size in bytes, or -1 on error */
long get_file_size(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0)
        return (long)st.st_size;

    return -1;
}

/* get_file_mtime -- Return Unix mtime of a file, or 0 on error */
long get_file_mtime(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0)
        return 0;

    return (long)st.st_mtime;
}

/* port_path_exists -- Check if path exists (native per OS) */

int port_path_exists(const char *p)
{
#ifdef AMIGA
    BPTR l = Lock((STRPTR)p, ACCESS_READ);

    if (l)
    {
        UnLock(l);
        return 1;
    }

    return 0;
#else
    struct stat st;
    return (stat(p, &st) == 0) ? 1 : 0;
#endif
}

/* port_mkdir_one -- Create single directory (native per OS) */
int port_mkdir_one(const char *p)
{
#ifdef AMIGA
    BPTR l = CreateDir((STRPTR)p);

    if (l)
    {
        UnLock(l);
        return 0;
    }

    return -1;
#else
    return mkdir(p, 0755);
#endif
}

/* safe_localtime -- Thread-safe localtime, portable across all OS */
void safe_localtime(const time_t *t, struct tm *tm)
{
#if defined(AMIGA) || defined(DOS)
    *tm = *localtime(t);
#elif defined(WIN32) || defined(__MINGW32__) || defined(VISUALCPP)
    localtime_s(tm, t);
#else
    localtime_r(t, tm);
#endif
}

/* mkdir_recursive -- Create full path, making all missing components */
int mkdir_recursive(const char *path)
{
    char tmp[MP_MAXPATH];
    char *p;
    int len;

    if (!path || !path[0])
        return -1;

    strncpy(tmp, path, MP_MAXPATH - 1);
    tmp[MP_MAXPATH - 1] = '\0';

    len = (int)strlen(tmp);

    /* Strip trailing slash */
    while (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\'))
        tmp[--len] = '\0';

    /* Walk every '/' component and create missing dirs */
    for (p = tmp + 1; *p; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            *p = '\0';

            if (!path_exists(tmp))
                mkdir_one(tmp); /* ignore per-component errors */

            *p = '/';
        }
    }

    /* Create the leaf */
    if (!path_exists(tmp))
        return mkdir_one(tmp);

    return 0;
}

/* safe_strncpy -- Ctrncpy that always NUL-terminates */
void safe_strncpy(char *dst, const char *src, int dstsize)
{
    int len;

    if (dstsize <= 0)
        return;

    len = (int)strlen(src);

    if (len > dstsize - 1)
        len = dstsize - 1;

    memcpy(dst, src, (size_t)len);
    dst[len] = '\0';
}

/* path_join -- Concatenate base path with sub path */
void path_join(char *out, int outsize, const char *base, const char *sub)
{
    int blen;
    char last;

    safe_strncpy(out, base, outsize);
    blen = (int)strlen(out);
    last = (blen > 0) ? out[blen - 1] : '\0';

    if (last != '/' && last != ':' && last != '\\')
    {
        if (outsize - 1 - blen > 0)
        {
            out[blen] = '/';
            out[blen + 1] = '\0';
            blen++;
        }
    }

    safe_strncpy(out + blen, sub, outsize - blen);
}

/* make_abs_path -- Resolve a possibly-relative path to absolute
 * Covers AmigaOS, Win32, OS/2, DOS and Unix
 * Returns 1 on success, 0 on failure (src copied verbatim as fallback)
 */
int make_abs_path(const char *src, char *dst, int dstlen)
{
#ifdef AMIGA
    BPTR lock = Lock((STRPTR)src, SHARED_LOCK);

    if (!lock)
    {
        safe_strncpy(dst, src, dstlen);
        return 0;
    }

    if (!NameFromLock(lock, (STRPTR)dst, dstlen))
    {
        UnLock(lock);
        safe_strncpy(dst, src, dstlen);
        return 0;
    }

    UnLock(lock);

    return 1;
#elif defined(WIN32) || defined(__MINGW32__) || defined(__WATCOMC__) || defined(VISUALCPP) || defined(OS2)
    if (_fullpath(dst, src, (size_t)dstlen) != NULL)
        return 1;

    safe_strncpy(dst, src, dstlen);
    return 0;
#elif defined(DOS)
    if (src[0] != '\\' && src[1] != ':')
    {
        char cwd[MAXPATHLEN + 1];

        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            snprintf(dst, dstlen, "%s\\%s", cwd, src);
            return 1;
        }
    }

    safe_strncpy(dst, src, dstlen);
    return 0;
#else
    char buf[MAXPATHLEN + 1];

    if (realpath(src, buf) != NULL)
    {
        safe_strncpy(dst, buf, dstlen);
        return 1;
    }

    if (src[0] != '/')
    {
        char cwd[MAXPATHLEN + 1];

        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            snprintf(dst, dstlen, "%s/%s", cwd, src);
            return 1;
        }
    }

    safe_strncpy(dst, src, dstlen);
    return 0;
#endif
}
