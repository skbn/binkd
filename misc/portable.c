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
    {
        if (fwrite(buf, 1, (size_t)n, out) != (size_t)n)
        {
            fclose(out);
            fclose(in);
            return 0; /* Write error (disk full, etc.) */
        }
    }

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

/* is_directory -- Check if path is a directory */
int is_directory(const char *p)
{
#ifdef AMIGA
    BPTR l = Lock((STRPTR)p, ACCESS_READ);
    struct FileInfoBlock *fib;
    int res = 0;

    if (l)
    {
        fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);

        if (fib)
        {
            if (Examine(l, fib))
                res = (fib->fib_DirEntryType >= 0) ? 1 : 0;

            FreeDosObject(DOS_FIB, fib);
        }

        UnLock(l);
    }
    return res;
#else
    struct stat st;

    if (stat(p, &st) != 0)
        return 0;

    return (S_ISDIR(st.st_mode)) ? 1 : 0;
#endif
}

/* is_regular_file -- Check if path is a regular file (not directory, not device) */
int is_regular_file(const char *p)
{
#ifdef AMIGA
    BPTR l = Lock((STRPTR)p, ACCESS_READ);
    struct FileInfoBlock *fib;
    int res = 0;

    if (l)
    {
        fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);

        if (fib)
        {
            if (Examine(l, fib))
                res = (fib->fib_DirEntryType < 0) ? 1 : 0; /* Files have negative type */

            FreeDosObject(DOS_FIB, fib);
        }

        UnLock(l);
    }
    return res;
#else
    struct stat st;

    if (stat(p, &st) != 0)
        return 0;

    return (S_ISREG(st.st_mode)) ? 1 : 0;
#endif
}

/* is_safe_filename -- Validate filename
 * Whitelist approach: only allows alphanumeric, dot, underscore, hyphen
 * Rejects: ., .., names starting with -, spaces
 * Returns 1 if safe, 0 if invalid */
int is_safe_filename(const char *name)
{
    size_t len;

    if (!name || !*name)
        return 0;

    len = strlen(name);
    if (len == 0 || len > 255)
        return 0;

    /* Avoid "." and ".." */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return 0;

    /* Do not allow names starting with '-' */
    if (name[0] == '-')
        return 0;
    
    /* Whitelist: only alphanumeric, dot, underscore, hyphen */
    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)name[i];

        if (!(isalnum(c) || c == '.' || c == '_' || c == '-'))
            return 0;
    }

    return 1;
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
#elif WIN32
	return mkdir(p);
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

/* parse_config_line -- Parse a config line into key and value
 * Handles: BOM, leading/trailing whitespace, tabs, comments inline (#)
 * Returns 1 if valid key=value parsed, 0 if blank/comment/invalid
 */
int parse_config_line(const char *line, char *key, int klen, char *val, int vlen)
{
    const unsigned char *p = (const unsigned char *)line;
    char *kdst, *vdst;
    int kcnt = 0, vcnt = 0;

    if (!line || !key || klen <= 0 || !val || vlen <= 0)
        return 0;

    /* Skip UTF-8 BOM */
    if (p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF)
        p += 3;

    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;

    /* Skip blank lines and comments */
    if (*p == '\0' || *p == '#' || *p == ';')
        return 0;

    /* Parse key */
    kdst = key;

    while (*p && *p != ' ' && *p != '\t' && *p != '=' && kcnt < klen - 1)
    {
        *kdst++ = (char)*p++;
        kcnt++;
    }

    *kdst = '\0';

    if (kcnt == 0)
        return 0;

    /* Skip separator (spaces, tabs, =) */
    while (*p == ' ' || *p == '\t' || *p == '=')
        p++;

    /* Parse value until comment or end of line */
    vdst = val;

    while (*p && *p != '#' && *p != '\r' && *p != '\n' && vcnt < vlen - 1)
    {
        *vdst++ = (char)*p++;
        vcnt++;
    }

    *vdst = '\0';

    /* Trim trailing whitespace from value */
    while (vcnt > 0 && (val[vcnt - 1] == ' ' || val[vcnt - 1] == '\t'))
        val[--vcnt] = '\0';

    return (vcnt > 0) ? 1 : 0; /* Return 0 if no value found */
}

/* config_get -- Open config file and return value for a specific key
 * Returns 1 if found, 0 if not found or error
 */
int config_get(const char *filename, const char *key, char *val, int vlen)
{
    FILE *f;
    char line[MAX_LINE];
    char kbuf[64];
    int found = 0;

    if (!filename || !key || !val || vlen <= 0)
        return 0;

    f = fopen(filename, "r");

    if (!f)
        return 0;

    while (fgets(line, sizeof(line), f))
    {
        kbuf[0] = '\0';
        val[0] = '\0';

        if (parse_config_line(line, kbuf, (int)sizeof(kbuf), val, vlen))
        {
            if (strcmp(kbuf, key) == 0)
            {
                found = 1;
                break;
            }
        }
    }

    fclose(f);

    return found;
}

/* parse_size -- Parse size string with suffixes (k, K, m, M, g, G)
 * Plain number = bytes. Returns size in bytes, or 0 on error
 */
long parse_size(const char *s)
{
    long num;
    char suffix;
    int n;

    if (!s || !s[0])
        return 0;

    n = sscanf(s, "%ld%c", &num, &suffix);

    if (n == 1)
        return num; /* Plain bytes */

    if (n == 2)
    {
        suffix = (char)tolower((unsigned char)suffix);

        switch (suffix)
        {
        case 'k':
            return num * 1024;
        case 'm':
            return num * 1024 * 1024;
        case 'g':
            return num * 1024 * 1024 * 1024;
        default:
            return 0; /* Invalid suffix */
        }
    }

    return 0;
}

/* parse_time -- Parse time string with suffixes (s, m, h, d)
 * Plain number = seconds. Returns time in seconds, or 0 on error
 */
long parse_time(const char *s)
{
    long num;
    char suffix;
    int n;

    if (!s || !s[0])
        return 0;

    n = sscanf(s, "%ld%c", &num, &suffix);

    if (n == 1)
        return num; /* Plain seconds */

    if (n == 2)
    {
        suffix = (char)tolower((unsigned char)suffix);

        switch (suffix)
        {
        case 's':
            return num;
        case 'm':
            return num * 60;
        case 'h':
            return num * 60 * 60;
        case 'd':
            return num * 60 * 60 * 24;
        default:
            return 0; /* Invalid suffix */
        }
    }

    return 0;
}

/* Config cache in memory implementation */
typedef struct ConfigEntry
{
    char key[64];
    char value[MAXPATHLEN];
    struct ConfigEntry *next;
} ConfigEntry;

struct ConfigCache
{
    ConfigEntry *entries;
    int count;
};

/* config_load -- Load entire config file into memory cache
 * Returns cache pointer on success, NULL on error
 */
ConfigCache *config_load(const char *filename)
{
    FILE *f;
    char line[MAX_LINE];
    char key[64], val[MAXPATHLEN];
    ConfigCache *cache;
    ConfigEntry *entry;

    if (!filename)
        return NULL;

    f = fopen(filename, "r");
    if (!f)
        return NULL;

    cache = (ConfigCache *)malloc(sizeof(ConfigCache));
    if (!cache)
    {
        fclose(f);
        return NULL;
    }

    cache->entries = NULL;
    cache->count = 0;

    while (fgets(line, sizeof(line), f))
    {
        key[0] = '\0';
        val[0] = '\0';

        if (!parse_config_line(line, key, (int)sizeof(key), val, (int)sizeof(val)))
            continue;

        entry = (ConfigEntry *)malloc(sizeof(ConfigEntry));
        if (!entry)
            continue;

        safe_strncpy(entry->key, key, (int)sizeof(entry->key));
        safe_strncpy(entry->value, val, (int)sizeof(entry->value));
        entry->next = cache->entries;
        cache->entries = entry;
        cache->count++;
    }

    fclose(f);
    return cache;
}

/* config_lookup -- Look up a key in cached config
 * Returns 1 if found, 0 if not found
 */
int config_lookup(ConfigCache *cache, const char *key, char *val, int vlen)
{
    ConfigEntry *entry;

    if (!cache || !key || !val || vlen <= 0)
        return 0;

    for (entry = cache->entries; entry; entry = entry->next)
    {
        if (strcmp(entry->key, key) == 0)
        {
            safe_strncpy(val, entry->value, vlen);
            return 1;
        }
    }

    return 0;
}

/* config_cache_free -- Free cached config memory */
void config_cache_free(ConfigCache *cache)
{
    ConfigEntry *entry, *next;

    if (!cache)
        return;

    entry = cache->entries;

    while (entry)
    {
        next = entry->next;
        free(entry);
        entry = next;
    }

    free(cache);
}
