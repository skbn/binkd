/*
 * process_tic -- Process FTN .tic files from inbound to filebox
 *
 * process_tic.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#include "portable.h" /* Canonical portable layer */
#include <ctype.h>

/* Config structure */
static struct
{
    char inbound[MAXPATHLEN];
    char filebox[MAXPATHLEN];
    char pubdir[MAXPATHLEN];
    char logfile[MAXPATHLEN];
    char filelist[MAXPATHLEN];
    char newfiles[MAXPATHLEN];
    char ticlog[MAXPATHLEN];
    int copypublic;
} cfg;

static int parse_keyword_field(char *line, const char *keyword, char *out, int outsize)
{
    char *p;
    char *end;
    int len;
    int klen;
    int i;

    klen = (int)strlen(keyword);
    p = skip_ws(line);

    for (i = 0; i < klen; i++)
    {
        if (toupper((unsigned char)p[i]) != toupper((unsigned char)keyword[i]))
            return 0;
    }

    p += klen;

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

static void write_ticentry(const char *logfile, const char *file_name, const char *area_name, const char *origin_name, const char *from_name, const char *src_path, const char *dst_path)
{
    FILE *f;
    time_t t;
    struct tm tm;
    char timestamp[64];

    if (!logfile || !logfile[0])
        return;

    f = fopen(logfile, "a");

    if (!f)
        return;

    t = time(NULL);
    safe_localtime(&t, &tm);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);

    fprintf(f, "[%s] File: %s\n", timestamp, file_name);
    fprintf(f, "  Area: %s\n", area_name);

    if (origin_name[0])
        fprintf(f, "  Origin: %s\n", origin_name);

    if (from_name[0])
        fprintf(f, "  From: %s\n", from_name);

    fprintf(f, "  Src: %s\n", src_path);
    fprintf(f, "  To: %s\n", dst_path);
    fprintf(f, "\n");

    fclose(f);
}

static void append_filelist(const char *listpath, const char *file_name, long filesize, const char *dst_path)
{
    FILE *f;
    time_t t;
    struct tm tm;
    char timestamp[32];

    if (!listpath || !listpath[0])
        return;

    f = fopen(listpath, "a");

    if (!f)
        return;

    t = time(NULL);
    safe_localtime(&t, &tm);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);

    fprintf(f, "%s\t%s\t%ld\t%s\n", timestamp, file_name, filesize, dst_path);
    fclose(f);
}

static void append_newfiles(const char *newprefix, const char *file_name, long filesize, const char *dst_path)
{
    FILE *f;
    char newpath[MAXPATHLEN];
    time_t t;
    struct tm tm;
    char datebuf[16];

    if (!newprefix || !newprefix[0])
        return;

    t = time(NULL);
    safe_localtime(&t, &tm);
    strftime(datebuf, sizeof(datebuf), "%Y%m%d", &tm);

    ensure_dir(newprefix);
    path_join(newpath, (int)sizeof(newpath), newprefix, "newfiles-");
    strncat(newpath, datebuf, sizeof(newpath) - strlen(newpath) - 1);
    strncat(newpath, ".txt", sizeof(newpath) - strlen(newpath) - 1);

    f = fopen(newpath, "a");

    if (!f)
        return;

    fprintf(f, "%s\t%ld\t%s\n", file_name, filesize, dst_path);

    fclose(f);
}

static void process_one_tic(const char *ticpath, const char *inbound, const char *filebox, int copypublic, const char *pubdir, const char *logfile, const char *filelist, const char *newfiles, const char *ticlog)
{
    FILE *f;
    char line[MAX_LINE];
    char file_name[MAXPATHLEN];
    char area_name[MAXPATHLEN];
    char origin_name[MAXPATHLEN];
    char from_name[MAXPATHLEN];
    char src_path[MAXPATHLEN];
    char area_dir[MAXPATHLEN];
    char dst_path[MAXPATHLEN];
    long fsize;

    file_name[0] = '\0';
    area_name[0] = '\0';
    origin_name[0] = '\0';
    from_name[0] = '\0';
    fsize = 0;

    f = fopen(ticpath, "r");

    if (!f)
        return;

    while (fgets(line, sizeof(line), f))
    {
        if (!file_name[0])
            parse_keyword_field(line, "File", file_name, sizeof(file_name));

        if (!area_name[0])
            parse_keyword_field(line, "Area", area_name, sizeof(area_name));

        if (!origin_name[0])
            parse_keyword_field(line, "Origin", origin_name, sizeof(origin_name));

        if (!from_name[0])
            parse_keyword_field(line, "From", from_name, sizeof(from_name));
    }

    fclose(f);

    if (!file_name[0] || !area_name[0])
        return;

    path_join(src_path, sizeof(src_path), inbound, file_name);
    path_join(area_dir, sizeof(area_dir), filebox, area_name);
    path_join(dst_path, sizeof(dst_path), area_dir, file_name);

    if (!path_exists(src_path))
    {
        fprintf(stderr, "process_tic: referenced file not found: %s\n", src_path);
        return;
    }

    if (!ensure_dir(filebox) || !ensure_dir(area_dir))
        return;

    if (copypublic && pubdir && pubdir[0])
    {
        char pub_dst[MAXPATHLEN];

        path_join(pub_dst, sizeof(pub_dst), pubdir, file_name);

        if (ensure_dir(pubdir))
        {
            if (!copy_file(src_path, pub_dst))
            {
                fprintf(stderr, "process_tic: failed to copy to pubdir: %s -> %s\n", src_path, pub_dst);
                return;  /* Do not move file if copy to pubdir failed */
            }
        }
    }

    fsize = get_file_size(src_path);

    if (!move_file(src_path, dst_path))
    {
        fprintf(stderr, "process_tic: failed to move file: %s -> %s\n", src_path, dst_path);
        return;
    }

    write_ticentry(logfile, file_name, area_name, origin_name, from_name, src_path, dst_path);
    write_ticentry(ticlog, file_name, area_name, origin_name, from_name, src_path, dst_path);

    append_filelist(filelist, file_name, fsize, dst_path);
    append_newfiles(newfiles, file_name, fsize, dst_path);

    remove(ticpath);
}

static int is_tic_file(const char *name)
{
    int len;

    len = (int)strlen(name);

    if (len < 5)
        return 0;

    return (name[len - 4] == '.' && toupper((unsigned char)name[len - 3]) == 'T' && toupper((unsigned char)name[len - 2]) == 'I' && toupper((unsigned char)name[len - 1]) == 'C');
}

/* Parse configuration file */
static int parse_config(const char *conffile)
{
    ConfigCache *cache;
    char val[MAXPATHLEN];

    memset(&cfg, 0, sizeof(cfg));

    cache = config_load(conffile);

    if (!cache)
    {
        fprintf(stderr, "process_tic: cannot open config file: %s\n", conffile);
        return 0;
    }

    /* Read each field using config_lookup() */
    if (config_lookup(cache, "inbound", val, sizeof(val)))
    {
        if (!is_directory(val))
        {
            fprintf(stderr, "process_tic: inbound is not a directory: %s\n", val);
            config_cache_free(cache);
            return 0;
        }

        safe_strncpy(cfg.inbound, val, (int)sizeof(cfg.inbound));
    }

    if (config_lookup(cache, "filebox", val, sizeof(val)))
    {
        if (!is_directory(val))
        {
            fprintf(stderr, "process_tic: filebox is not a directory: %s\n", val);
            config_cache_free(cache);
            return 0;
        }

        safe_strncpy(cfg.filebox, val, (int)sizeof(cfg.filebox));
    }

    if (config_lookup(cache, "pubdir", val, sizeof(val)))
    {
        if (!is_directory(val))
        {
            fprintf(stderr, "process_tic: pubdir is not a directory: %s\n", val);
            config_cache_free(cache);
            return 0;
        }

        safe_strncpy(cfg.pubdir, val, (int)sizeof(cfg.pubdir));
        cfg.copypublic = 1;
    }

    if (config_lookup(cache, "logfile", val, sizeof(val)))
    {
        if (val[0] && val[0] != '-' && strcmp(val, "-") != 0 && path_exists(val) && !is_regular_file(val))
        {
            fprintf(stderr, "process_tic: logfile exists but is not a regular file: %s\n", val);
            config_cache_free(cache);
            return 0;
        }

        safe_strncpy(cfg.logfile, val, (int)sizeof(cfg.logfile));
    }

    if (config_lookup(cache, "filelist", val, sizeof(val)))
    {
        if (val[0] && val[0] != '-' && path_exists(val) && !is_regular_file(val))
        {
            fprintf(stderr, "process_tic: filelist exists but is not a regular file: %s\n", val);
            config_cache_free(cache);
            return 0;
        }

        safe_strncpy(cfg.filelist, val, (int)sizeof(cfg.filelist));
    }

    if (config_lookup(cache, "newfiles", val, sizeof(val)))
    {
        if (val[0] && !is_directory(val))
        {
            fprintf(stderr, "process_tic: newfiles is not a directory: %s\n", val);
            config_cache_free(cache);
            return 0;
        }

        safe_strncpy(cfg.newfiles, val, (int)sizeof(cfg.newfiles));
    }

    if (config_lookup(cache, "ticlog", val, sizeof(val)))
    {
        if (val[0] && val[0] != '-' && strcmp(val, "-") != 0 && path_exists(val) && !is_regular_file(val))
        {
            fprintf(stderr, "process_tic: ticlog exists but is not a regular file: %s\n", val);
            config_cache_free(cache);
            return 0;
        }

        safe_strncpy(cfg.ticlog, val, (int)sizeof(cfg.ticlog));
    }

    config_cache_free(cache);

    /* Validate required fields */
    if (!cfg.inbound[0] || !cfg.filebox[0])
    {
        fprintf(stderr, "process_tic: config file missing required 'inbound' or 'filebox'\n");
        return 0;
    }

    return 1;
}

int main(int argc, char *argv[])
{
    char inbound[MAXPATHLEN];
    char filebox[MAXPATHLEN];
    char ticpath[MAXPATHLEN];
    char pubdir[MAXPATHLEN];
    char logfile[MAXPATHLEN];
    char filelist[MAXPATHLEN];
    char newfiles[MAXPATHLEN];
    char ticlog[MAXPATHLEN];
    int copypublic = 0;
    DIR *dp;
    struct dirent *de;
    int found;
    int i;
    int use_config = 0;

    inbound[0] = '\0';
    filebox[0] = '\0';
    pubdir[0] = '\0';
    logfile[0] = '\0';
    filelist[0] = '\0';
    newfiles[0] = '\0';
    ticlog[0] = '\0';

    /* Check for --conf option */
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--conf") == 0 && i + 1 < argc)
        {
            if (!parse_config(argv[i + 1]))
                return 1;

            use_config = 1;
            i++; /* Skip config file path */

            break;
        }
    }

    if (use_config)
    {
        /* Use config file values */
        safe_strncpy(inbound, cfg.inbound, (int)sizeof(inbound));
        safe_strncpy(filebox, cfg.filebox, (int)sizeof(filebox));
        safe_strncpy(pubdir, cfg.pubdir, (int)sizeof(pubdir));
        safe_strncpy(logfile, cfg.logfile, (int)sizeof(logfile));
        safe_strncpy(filelist, cfg.filelist, (int)sizeof(filelist));
        safe_strncpy(newfiles, cfg.newfiles, (int)sizeof(newfiles));
        safe_strncpy(ticlog, cfg.ticlog, (int)sizeof(ticlog));
        copypublic = cfg.copypublic;
    }

    if (!inbound[0] || !filebox[0])
    {
        fprintf(stderr, "Usage: process_tic --conf <config-file> [*.tic]\n");
        return 1;
    }

    if (!is_directory(inbound))
    {
        fprintf(stderr, "process_tic: inbound is not a directory: %s\n", inbound);
        return 1;
    }

    dp = opendir(inbound);

    if (!dp)
    {
        fprintf(stderr, "process_tic: cannot open inbound directory: %s\n", inbound);
        return 1;
    }

    found = 0;

    while ((de = readdir(dp)) != NULL)
    {
        /* Skip . and .. */
        if (de->d_name[0] == '.' && (de->d_name[1] == '\0' || (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;

        if (!is_tic_file(de->d_name))
            continue;

        path_join(ticpath, sizeof(ticpath), inbound, de->d_name);

        if (!is_regular_file(ticpath))
        {
            fprintf(stderr, "process_tic: not a regular file: %s\n", ticpath);
            continue;
        }

        process_one_tic(ticpath, inbound, filebox, copypublic, pubdir, logfile, filelist, newfiles, ticlog);
        found++;
    }

    closedir(dp);
    return 0;
}
