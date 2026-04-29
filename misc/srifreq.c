/*
 * srifreq.c -- SRIF-compatible file-request server for binkd
 *
 * srifreq.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#include "portable.h" /* Canonical portable layer */
#include <ctype.h>
#include <time.h>

/* Private directory entry (dynamically allocated list) */
typedef struct PrivDir
{
    char path[MAXPATHLEN];
    char password[64];
    struct PrivDir *next;
} PrivDir;

/* Node tracking entry for rate limiting */
typedef struct NodeTrack
{
    char aka[256];    /* Node address (4D/5D) */
    int files;        /* Files downloaded in window */
    long bytes;       /* Bytes downloaded in window */
    time_t last_time; /* Timestamp of last download */
    struct NodeTrack *next;
} NodeTrack;

/* Global configuration (filled from --conf file) */
typedef struct
{
    char pubdir[MAXPATHLEN];
    char logfile[MAXPATHLEN];
    char aliases[MAXPATHLEN];
    char trackfile[MAXPATHLEN]; /* Path to tracking file */
    int maxfiles;               /* Max files per node per window (0=unlimited) */
    long maxbytes;              /* Max bytes per node per window (0=unlimited) */
    long timewindow;            /* Time window in seconds (0=no window) */
    PrivDir *privdirs;          /* Linked list, NULL if none */
    NodeTrack *tracking;        /* Linked list of tracked nodes */
} Config;

/* Alias table -- Loaded from file at startup */
typedef struct
{
    char name[64];
    char path[MAXPATHLEN];
} Alias;

/* SRIF parsing */
typedef struct
{
    char sysop[128];
    char aka[256];
    char request_list[MAXPATHLEN];
    char response_list[MAXPATHLEN];
    char our_aka[128];
    char caller_id[64]; /* CallerID: IP or phone of remote */
    char password[64];  /* Password: session password */
    int time_limit;     /* Time: minutes left, -1 = unlimited */
    long tranx;         /* TRANX: remote local time as Unix ts (hex in SRIF) */
    int protected_sess; /* RemoteStatus: 1=PROTECTED, 0=UNPROTECTED */
    int listed;         /* SystemStatus: 1=LISTED, 0=UNLISTED */
    int got_request_list;
    int got_response_list;
} SRIF;

static Alias *g_aliases = NULL;
static int g_nalias = 0;
static int g_alias_cap = 0;
static Config g_conf;

static void config_init(void)
{
    memset(&g_conf, 0, sizeof(g_conf));
    g_conf.privdirs = NULL;
    g_conf.tracking = NULL;
    g_conf.maxfiles = 0;   /* 0 = unlimited */
    g_conf.maxbytes = 0;   /* 0 = unlimited */
    g_conf.timewindow = 0; /* 0 = no window */
}

static void config_add_private(const char *path, const char *password)
{
    PrivDir *pd = (PrivDir *)malloc(sizeof(PrivDir));
    PrivDir *tail;

    if (!pd)
        return;

    safe_strncpy(pd->path, path, (int)sizeof(pd->path));
    safe_strncpy(pd->password, password, (int)sizeof(pd->password));

    pd->next = NULL;

    /* Append to tail */
    if (!g_conf.privdirs)
        g_conf.privdirs = pd;
    else
    {
        tail = g_conf.privdirs;

        while (tail->next)
            tail = tail->next;

        tail->next = pd;
    }
}

static void srifreq_config_cleanup(void)
{
    PrivDir *pd = g_conf.privdirs;
    NodeTrack *nt = g_conf.tracking;

    while (pd)
    {
        PrivDir *next = pd->next;
        free(pd);
        pd = next;
    }

    g_conf.privdirs = NULL;

    while (nt)
    {
        NodeTrack *next = nt->next;
        free(nt);
        nt = next;
    }

    g_conf.tracking = NULL;

    if (g_aliases)
    {
        free(g_aliases);
        g_aliases = NULL;
        g_nalias = 0;
        g_alias_cap = 0;
    }
}

static int load_config(const char *path)
{
    ConfigCache *cache;
    char val[MAXPATHLEN];
    char pw[64];
    FILE *f;

    cache = config_load(path);

    if (!cache)
    {
        fprintf(stderr, "srifreq: cannot open config: %s\n", path);
        return 0;
    }

    if (config_lookup(cache, "pubdir", val, sizeof(val)))
    {
        if (!is_directory(val))
        {
            fprintf(stderr, "srifreq: pubdir is not a directory: %s\n", val);
            config_cache_free(cache);
            return 0;
        }

        safe_strncpy(g_conf.pubdir, val, (int)sizeof(g_conf.pubdir));
    }

    if (config_lookup(cache, "logfile", val, sizeof(val)))
    {
        if (val[0] && !is_regular_file(val) && strcmp(val, "-") != 0)
        {
            fprintf(stderr, "srifreq: logfile is not a regular file: %s\n", val);
            config_cache_free(cache);
            return 0;
        }

        safe_strncpy(g_conf.logfile, val, (int)sizeof(g_conf.logfile));
    }

    if (config_lookup(cache, "aliases", val, sizeof(val)))
    {
        if (val[0] && !is_regular_file(val) && strcmp(val, "-") != 0)
        {
            fprintf(stderr, "srifreq: aliases file is not a regular file: %s\n", val);
            config_cache_free(cache);
            return 0;
        }

        safe_strncpy(g_conf.aliases, val, (int)sizeof(g_conf.aliases));
    }

    if (config_lookup(cache, "trackfile", val, sizeof(val)))
    {
        if (val[0] && !is_regular_file(val))
        {
            fprintf(stderr, "srifreq: trackfile is not a regular file: %s\n", val);
            config_cache_free(cache);
            return 0;
        }

        safe_strncpy(g_conf.trackfile, val, (int)sizeof(g_conf.trackfile));
    }

    if (config_lookup(cache, "maxfiles", val, sizeof(val)))
        g_conf.maxfiles = atoi(val);

    if (config_lookup(cache, "maxsize", val, sizeof(val)))
        g_conf.maxbytes = parse_size(val);

    if (config_lookup(cache, "timewindow", val, sizeof(val)))
        g_conf.timewindow = parse_time(val);

    /* Handle private dirs - need to re-scan file for password field */
    f = fopen(path, "r");

    if (f)
    {
        char line[MAX_LINE];
        char key[64], v[MAXPATHLEN];

        while (fgets(line, sizeof(line), f))
        {
            key[0] = v[0] = pw[0] = '\0';

            if (!parse_config_line(line, key, (int)sizeof(key), v, (int)sizeof(v)))
                continue;

            if (strcmp(key, "private") == 0)
            {
                sscanf(line, "%*s %*s %63s", pw);

                if (pw[0])
                {
                    if (!is_directory(v))
                    {
                        fprintf(stderr, "srifreq: private path is not a directory: %s\n", v);
                        continue;
                    }

                    config_add_private(v, pw);
                }
            }
        }

        fclose(f);
    }

    config_cache_free(cache);

    return 1;
}

/* tracking_load -- Load node tracking data from file */
static void tracking_load(void)
{
    FILE *f;
    char line[MAX_LINE];
    char aka[256];
    int files;
    long bytes;
    long timestamp;
    NodeTrack *nt;
    time_t now;

    now = time(NULL);

    if (!g_conf.trackfile[0])
        return;

    f = fopen(g_conf.trackfile, "r");

    if (!f)
        return;

    while (fgets(line, sizeof(line), f))
    {
        str_trim(line);

        if (!line[0] || line[0] == '#')
            continue;

        if (sscanf(line, "%255s %d %ld %ld", aka, &files, &bytes, &timestamp) != 4)
            continue;

        /* Skip if outside time window (also reject future/corrupt timestamps) */
        if (g_conf.timewindow > 0 && (timestamp > now || (now - timestamp) > g_conf.timewindow))
            continue;

        /* Create new tracking entry */
        nt = (NodeTrack *)malloc(sizeof(NodeTrack));

        if (!nt)
            continue;

        safe_strncpy(nt->aka, aka, (int)sizeof(nt->aka));

        nt->files = files;
        nt->bytes = bytes;
        nt->last_time = (time_t)timestamp;
        nt->next = g_conf.tracking;
        g_conf.tracking = nt;
    }

    fclose(f);
}

/* tracking_save -- Save node tracking data to file */
static void tracking_save(void)
{
    FILE *f;
    NodeTrack *nt;

    if (!g_conf.trackfile[0])
        return;

    f = fopen(g_conf.trackfile, "w");

    if (!f)
        return;

    fprintf(f, "# srifreq tracking file - Format: AKA files bytes timestamp\n");

    for (nt = g_conf.tracking; nt; nt = nt->next)
    {
        fprintf(f, "%s %d %ld %ld\n", nt->aka, nt->files, nt->bytes, (long)nt->last_time);
    }

    fclose(f);
}

/* tracking_find -- Find tracking entry for a node */
static NodeTrack *tracking_find(const char *aka)
{
    NodeTrack *nt;

    for (nt = g_conf.tracking; nt; nt = nt->next)
    {
        if (strcmp(nt->aka, aka) == 0)
            return nt;
    }

    return NULL;
}

/* tracking_update -- Update tracking after serving a file */
static void tracking_update(const char *aka, long filesize)
{
    NodeTrack *nt;
    time_t now;

    nt = tracking_find(aka);
    now = time(NULL);

    if (nt)
    {
        /* Update existing entry */
        nt->files++;
        nt->bytes += filesize;
        nt->last_time = now;
    }
    else
    {
        /* Create new entry */
        nt = (NodeTrack *)malloc(sizeof(NodeTrack));

        if (nt)
        {
            safe_strncpy(nt->aka, aka, (int)sizeof(nt->aka));
            nt->files = 1;
            nt->bytes = filesize;
            nt->last_time = now;
            nt->next = g_conf.tracking;
            g_conf.tracking = nt;
        }
    }
}

/* tracking_check -- Check if node exceeds limits */
static int tracking_check(const char *aka, char *msg, int msglen)
{
    NodeTrack *nt = tracking_find(aka);

    if (!nt)
        return 1; /* No tracking yet, allow */

    /* Check max files */
    if (g_conf.maxfiles > 0 && nt->files >= g_conf.maxfiles)
    {
        snprintf(msg, msglen, "RATE LIMIT: max files (%d) reached for %s", g_conf.maxfiles, aka);
        return 0;
    }

    /* Check max bytes */
    if (g_conf.maxbytes > 0 && nt->bytes >= g_conf.maxbytes)
    {
        snprintf(msg, msglen, "RATE LIMIT: max bytes (%ld) reached for %s", g_conf.maxbytes, aka);
        return 0;
    }

    return 1; /* Within limits */
}

/* passwd_match -- Case-insensitive comparison of two password strings */
static int passwd_match(const char *a, const char *b)
{
    char ua[64], ub[64];
    int i;

    safe_strncpy(ua, a, (int)sizeof(ua));
    safe_strncpy(ub, b, (int)sizeof(ub));

    for (i = 0; ua[i]; i++)
        ua[i] = (char)toupper((unsigned char)ua[i]);

    for (i = 0; ub[i]; i++)
        ub[i] = (char)toupper((unsigned char)ub[i]);

    return strcmp(ua, ub) == 0;
}

/* is_abs_path -- True if path is absolute (POSIX, Win32, AmigaDOS device:) */
static int is_abs_path(const char *p)
{
    if (!p || !p[0])
        return 0;

    if (p[0] == '/' || p[0] == '\\')
        return 1;

#ifdef AMIGA
    if (strchr(p, ':') != NULL)
        return 1;
#else
    if (p[1] == ':')
        return 1; /* C:\ etc. */
#endif
    return 0;
}

/*
 * load_aliases -- Read alias definitions from file
 * Lines starting with '#' or empty are skipped
 * Format: <NAME>  <path>
 */
static void load_aliases(const char *filepath)
{
    FILE *f;
    char line[MAX_LINE];
    char name[64];
    char path[MAXPATHLEN];
    int n;

    /* Free previous aliases and start fresh */
    if (g_aliases)
    {
        free(g_aliases);
        g_aliases = NULL;
    }

    g_nalias = 0;
    g_alias_cap = 0;

    if (!filepath || !filepath[0] || strcmp(filepath, "-") == 0)
        return;

    f = fopen(filepath, "r");

    if (!f)
    {
        fprintf(stderr, "srifreq: cannot open aliases file: %s\n", filepath);
        return;
    }

    while (fgets(line, sizeof(line), f))
    {
        char *p;

        /* Strip trailing newline */
        n = (int)strlen(line);

        while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n'))
            line[--n] = '\0';

        /* Skip blanks and comments */
        p = line;

        while (*p == ' ' || *p == '\t')
            p++;

        if (!*p || *p == '#')
            continue;

        name[0] = '\0';
        path[0] = '\0';

        if (sscanf(p, "%63s %1023[^\n]", name, path) < 2)
            continue;

        if (!name[0] || !path[0])
            continue;

        /* Trim leading whitespace from path (sscanf may capture spaces if multiple spaces between fields) */
        p = path;

        while (*p == ' ' || *p == '\t')
            p++;

        if (p != path)
            memmove(path, p, strlen(p) + 1);

        /* Grow array dynamically if needed */
        if (g_nalias >= g_alias_cap)
        {
            int new_cap;
            Alias *new_arr;

            new_cap = g_alias_cap ? g_alias_cap * 2 : 16;
            new_arr = realloc(g_aliases, (size_t)new_cap * sizeof(Alias));

            if (!new_arr)
                break;

            g_aliases = new_arr;
            g_alias_cap = new_cap;
        }

        safe_strncpy(g_aliases[g_nalias].name, name, (int)sizeof(g_aliases[g_nalias].name));
        safe_strncpy(g_aliases[g_nalias].path, path, (int)sizeof(g_aliases[g_nalias].path));
        g_nalias++;
    }

    fclose(f);

    /*printf("srifreq: loaded %d alias(es) from %s\n", g_nalias, filepath);*/
}

/*
 * find_alias -- look up name in alias table (case-insensitive)
 * Returns the path string, or NULL if not found
 */
static const char *find_alias(const char *name)
{
    char upper[64];
    char aname[64];
    int i;
    int n;
    int j;

    /* Convert name to uppercase */
    n = (int)strlen(name);

    if (n >= (int)sizeof(upper))
        n = (int)sizeof(upper) - 1;

    for (i = 0; i < n; i++)
        upper[i] = (char)toupper((unsigned char)name[i]);

    upper[n] = '\0';

    for (i = 0; i < g_nalias; i++)
    {
        int an;
        an = (int)strlen(g_aliases[i].name);

        if (an >= (int)sizeof(aname))
            an = (int)sizeof(aname) - 1;

        for (j = 0; j < an; j++)
            aname[j] = (char)toupper((unsigned char)g_aliases[i].name[j]);

        aname[an] = '\0';

        if (strcmp(upper, aname) == 0)
            return g_aliases[i].path;
    }

    return NULL;
}

static int parse_srif(const char *path, SRIF *srif)
{
    FILE *f;
    char line[MAX_LINE];
    char token[MAX_LINE];
    char value[MAX_LINE];

    memset(srif, 0, sizeof(SRIF));
    srif->time_limit = -1; /* default: unlimited */
    srif->listed = 1;      /* default: assume listed */
    srif->protected_sess = 0;

    f = fopen(path, "r");
    if (!f)
        return 0;

    while (fgets(line, sizeof(line), f))
    {
        char *p = strchr(line, '\n');

        if (p)
            *p = '\0';

        p = strchr(line, '\r');

        if (p)
            *p = '\0';

        if (sscanf(line, "%1023s %1023[^\n]", token, value) < 1)
            continue;

        str_upper(token);

        if (!strcmp(token, "SYSOP"))
            safe_strncpy(srif->sysop, value, (int)sizeof(srif->sysop));
        else if (!strcmp(token, "AKA") && !srif->aka[0])
            safe_strncpy(srif->aka, value, (int)sizeof(srif->aka));
        else if (!strcmp(token, "REQUESTLIST"))
        {
            safe_strncpy(srif->request_list, value, MAXPATHLEN);
            srif->got_request_list = 1;
        }
        else if (!strcmp(token, "RESPONSELIST"))
        {
            safe_strncpy(srif->response_list, value, MAXPATHLEN);
            srif->got_response_list = 1;
        }
        else if (!strcmp(token, "OURAKA"))
            safe_strncpy(srif->our_aka, value, (int)sizeof(srif->our_aka));
        else if (!strcmp(token, "PASSWORD"))
            safe_strncpy(srif->password, value, (int)sizeof(srif->password));
        else if (!strcmp(token, "CALLERID"))
            safe_strncpy(srif->caller_id, value, (int)sizeof(srif->caller_id));
        else if (!strcmp(token, "TIME"))
            srif->time_limit = atoi(value);
        else if (!strcmp(token, "TRANX"))
        {
            /* TRANX is a hex Unix timestamp: 5a326682 or 16-digit */
            unsigned long v = 0;
            sscanf(value, "%lx", &v);
            srif->tranx = (long)v;
        }
        else if (!strcmp(token, "REMOTESTATUS"))
        {
            char tmp[32];
            safe_strncpy(tmp, value, (int)sizeof(tmp));
            str_upper(tmp);
            srif->protected_sess = (strncmp(tmp, "PROTECTED", 9) == 0) ? 1 : 0;
        }
        else if (!strcmp(token, "SYSTEMSTATUS"))
        {
            char tmp[32];
            safe_strncpy(tmp, value, (int)sizeof(tmp));
            str_upper(tmp);
            srif->listed = (strncmp(tmp, "LISTED", 6) == 0) ? 1 : 0;
        }
    }

    fclose(f);

    return srif->got_request_list;
}

static void do_log(const char *logpath, const char *msg)
{
    FILE *lf;
    time_t t;
    struct tm tm;
    char timestamp[32];

    if (!logpath || !logpath[0] || strcmp(logpath, "-") == 0)
        return;

    t = time(NULL);
    safe_localtime(&t, &tm);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);

    lf = fopen(logpath, "a");

    if (lf)
    {
        fprintf(lf, "[%s] srifreq: %s\n", timestamp, msg);
        fclose(lf);
    }
}

static int serve_one(const char *req_name, const char *found_path, const char *req_pass, long req_newer, int req_update, const SRIF *srif, FILE *rsp_f, const char *log_path, char *logbuf, int logbuf_size)
{
    long fsize;
    long mtime;

    /* Check rate limits before serving */
    if (g_conf.trackfile[0] && !tracking_check(srif->aka, logbuf, logbuf_size))
    {
        do_log(log_path, logbuf);
        return 0;
    }

    /* RemoteStatus: if session is unprotected and a password is required, deny */
    if (req_pass[0] && !srif->protected_sess)
    {
        snprintf(logbuf, logbuf_size, "PASSWORD DENY (unprotected session): %s", req_name);
        do_log(log_path, logbuf);
        return 0;
    }

    /* Password check: !pw must match SRIF PASSWORD (case-insensitive) */
    if (req_pass[0])
    {
        if (!passwd_match(req_pass, srif->password))
        {
            snprintf(logbuf, logbuf_size, "PASSWORD FAIL: %s", req_name);
            do_log(log_path, logbuf);
            return 0;
        }
    }

    /* Update request (U flag): serve only if file is newer than TRANX */
    if (req_update && srif->tranx > 0)
    {
        mtime = get_file_mtime(found_path);

        if (mtime <= srif->tranx)
        {
            snprintf(logbuf, logbuf_size, "NOT UPDATED: %s (mtime=%ld tranx=%ld)", req_name, mtime, srif->tranx);
            do_log(log_path, logbuf);
            return 0;
        }
    }

    /* Timestamp check: +ts means "only if file is newer than ts" */
    if (req_newer > 0)
    {
        mtime = get_file_mtime(found_path);

        if (mtime <= req_newer)
        {
            snprintf(logbuf, logbuf_size, "NOT NEWER: %s (mtime=%ld req=%ld)", req_name, mtime, req_newer);

            do_log(log_path, logbuf);
            return 0;
        }
    }

    snprintf(logbuf, logbuf_size, "FOUND: %s -> %s", req_name, found_path);
    do_log(log_path, logbuf);

    if (rsp_f)
        fprintf(rsp_f, "+%s\r\n", found_path);

    /* Update tracking after successful serve */
    if (g_conf.trackfile[0])
    {
        fsize = get_file_size(found_path);

        if (fsize < 0)
            fsize = 0;

        tracking_update(srif->aka, fsize);
    }

    return 1;
}

int main(int argc, char *argv[])
{
    const char *srif_path;
    SRIF srif;
    FILE *req_f;
    FILE *rsp_f;
    char line[MAX_LINE];
    char req_name[MAX_LINE];
    char req_pass[64];
    long req_newer;
    int req_update;
    char found_path[MAXPATHLEN];
    char logbuf[MAXPATHLEN * 4 + 128];
    int found_count;

    config_init();

    /* --conf <file> <srif_file> */
    if (argc >= 4 && strcmp(argv[1], "--conf") == 0)
    {
        if (!load_config(argv[2]))
            return 1;

        srif_path = argv[3];
    }
    else
    {
        fprintf(stderr, "Usage:\n"
                        "  srifreq --conf <srifreq.conf> <srif_file>\n"
                        "\n"
                        "Config file keys: pubdir, logfile, aliases, private <dir> "
                        "<password>\n");

        return 1;
    }

    if (!g_conf.pubdir[0])
    {
        do_log(g_conf.logfile, "ERROR: pubdir not set in config");
        srifreq_config_cleanup();
        return 1;
    }

    /* Load tracking data if configured */
    tracking_load();

    load_aliases(g_conf.aliases[0] ? g_conf.aliases : NULL);

    snprintf(logbuf, sizeof(logbuf), "processing SRIF: %s", srif_path);
    do_log(g_conf.logfile, logbuf);

    if (!parse_srif(srif_path, &srif))
    {
        snprintf(logbuf, sizeof(logbuf), "ERROR: cannot parse SRIF or missing RequestList: %s", srif_path);
        do_log(g_conf.logfile, logbuf);
        srifreq_config_cleanup();
        return 1;
    }

    /* SystemStatus: deny unlisted systems entirely */
    if (!srif.listed)
    {
        snprintf(logbuf, sizeof(logbuf), "DENIED: system is UNLISTED (aka: %s)", srif.aka);
        do_log(g_conf.logfile, logbuf);
        srifreq_config_cleanup();
        return 1;
    }

    snprintf(logbuf, sizeof(logbuf), "sysop: %s  aka: %s  status: %s%s  caller: %s  req: %s", srif.sysop, srif.aka, srif.protected_sess ? "PROTECTED" : "UNPROTECTED", srif.tranx ? " (TRANX)" : "", srif.caller_id[0] ? srif.caller_id : "?", srif.request_list);
    do_log(g_conf.logfile, logbuf);

    /* Log rate limiting status if active */
    if (g_conf.trackfile[0] && (g_conf.maxfiles > 0 || g_conf.maxbytes > 0))
    {
        snprintf(logbuf, sizeof(logbuf), "rate limits: maxfiles=%d maxbytes=%ld window=%lds", g_conf.maxfiles, g_conf.maxbytes, g_conf.timewindow);
        do_log(g_conf.logfile, logbuf);
    }

    req_f = fopen(srif.request_list, "r");

    if (!req_f)
    {
        snprintf(logbuf, sizeof(logbuf), "WARN: RequestList not available: %s", srif.request_list);
        do_log(g_conf.logfile, logbuf);
        srifreq_config_cleanup();
        return 0;
    }

    rsp_f = NULL;

    if (srif.got_response_list && srif.response_list[0])
    {
        rsp_f = fopen(srif.response_list, "w");

        if (!rsp_f)
        {
            snprintf(logbuf, sizeof(logbuf), "WARN: cannot create ResponseList: %s", srif.response_list);
            do_log(g_conf.logfile, logbuf);
        }
    }

    found_count = 0;

    /* Check and update track file */
    while (fgets(line, sizeof(line), req_f))
    {
        char *p;
        const char *alias_path;
        int i;

        str_trim(line);

        if (!line[0] || line[0] == ';' || line[0] == '#')
            continue;

        /* Parse: filename [!password] [+timestamp] [U] */
        req_name[0] = '\0';
        req_pass[0] = '\0';
        req_newer = 0;
        req_update = 0;

        if (sscanf(line, "%1023s", req_name) < 1)
            continue;

        /* Skip URLs */
        if (strncmp(req_name, "http", 4) == 0 || strncmp(req_name, "ftp", 3) == 0)
            continue;

        /* Parse modifiers from the rest of the line */
        p = strstr(line, req_name);

        if (p)
            p += strlen(req_name);
        else
            p = line + strlen(line);

        while (*p)
        {
            while (*p == ' ' || *p == '\t')
                p++;

            if (*p == '!')
            {
                /* !password */
                i = 0;
                p++;

                while (*p && *p != ' ' && *p != '\t' && i < (int)sizeof(req_pass) - 1)
                    req_pass[i++] = *p++;

                req_pass[i] = '\0';
            }
            else if (*p == '+')
            {
                /* +unix_timestamp */
                p++;
                req_newer = atol(p);

                while (*p && *p != ' ' && *p != '\t')
                    p++;
            }
            else if (*p == 'U' && (p[1] == '\0' || p[1] == ' ' || p[1] == '\t'))
            {
                /* U = update request */
                req_update = 1;
                p++;
            }
            else if (*p)
                p++; /* Skip unknown token */
        }

        found_path[0] = '\0';

        /* Check alias table first */
        alias_path = find_alias(req_name);

        if (alias_path)
        {
            if (is_abs_path(alias_path))
                safe_strncpy(found_path, alias_path, MAXPATHLEN);
            else
                path_join(found_path, MAXPATHLEN, g_conf.pubdir, alias_path);

            if (is_regular_file(found_path))
                found_count += serve_one(req_name, found_path, req_pass, req_newer, req_update, &srif, rsp_f, g_conf.logfile, logbuf, (int)sizeof(logbuf));
            else if (path_exists(found_path))
            {
                snprintf(logbuf, sizeof(logbuf), "SKIP (not a file): %s -> %s", req_name, found_path);
                do_log(g_conf.logfile, logbuf);
            }
            else
            {
                snprintf(logbuf, sizeof(logbuf), "NOT FOUND (alias): %s -> %s", req_name, found_path);
                do_log(g_conf.logfile, logbuf);
            }

            continue;
        }

        /* Wildcard: scan pubdir and all privdirs whose password matches */
        if (is_wildcard(req_name))
        {
            DIR *dp;
            struct dirent *de;
            PrivDir *pd;

            /* Scan pubdir (no password needed) */
            dp = opendir(g_conf.pubdir);

            if (dp)
            {
                while ((de = readdir(dp)) != NULL)
                {
                    if (de->d_name[0] == '.' && (de->d_name[1] == '\0' || (de->d_name[1] == '.' && de->d_name[2] == '\0')))
                        continue;

                    if (!wildmatch(req_name, de->d_name))
                        continue;

                    path_join(found_path, MAXPATHLEN, g_conf.pubdir, de->d_name);

                    if (is_regular_file(found_path))
                        found_count += serve_one(de->d_name, found_path, "", req_newer, req_update, &srif, rsp_f, g_conf.logfile, logbuf, (int)sizeof(logbuf));
                }

                closedir(dp);
            }

            /* Scan matching privdirs */
            for (pd = g_conf.privdirs; pd; pd = pd->next)
            {
                if (!passwd_match(req_pass, pd->password))
                    continue;

                dp = opendir(pd->path);

                if (!dp)
                {
                    snprintf(logbuf, sizeof(logbuf), "WARN: cannot open private dir: %s", pd->path);
                    do_log(g_conf.logfile, logbuf);
                    continue;
                }

                while ((de = readdir(dp)) != NULL)
                {
                    if (de->d_name[0] == '.' && (de->d_name[1] == '\0' || (de->d_name[1] == '.' && de->d_name[2] == '\0')))
                        continue;

                    if (!wildmatch(req_name, de->d_name))
                        continue;

                    path_join(found_path, MAXPATHLEN, pd->path, de->d_name);

                    if (is_regular_file(found_path))
                        found_count += serve_one(de->d_name, found_path, req_pass, req_newer, req_update, &srif, rsp_f, g_conf.logfile, logbuf, (int)sizeof(logbuf));
                }

                closedir(dp);
            }

            continue;
        }

        /* Plain filename: try pubdir first, then privdirs if password given */
        path_join(found_path, MAXPATHLEN, g_conf.pubdir, req_name);

        if (is_regular_file(found_path))
        {
            found_count += serve_one(req_name, found_path, req_pass, req_newer, req_update, &srif, rsp_f, g_conf.logfile, logbuf, (int)sizeof(logbuf));
        }
        else if (path_exists(found_path))
        {
            snprintf(logbuf, sizeof(logbuf), "SKIP (not a file): %s", found_path);
            do_log(g_conf.logfile, logbuf);
        }
        else if (req_pass[0])
        {
            /* Try each private dir whose password matches */
            PrivDir *pd;
            int served;

            served = 0;

            for (pd = g_conf.privdirs; pd && !served; pd = pd->next)
            {
                if (!passwd_match(req_pass, pd->password))
                    continue;

                path_join(found_path, MAXPATHLEN, pd->path, req_name);

                if (is_regular_file(found_path))
                {
                    found_count += serve_one(req_name, found_path, req_pass, req_newer, req_update, &srif, rsp_f, g_conf.logfile, logbuf, (int)sizeof(logbuf));
                    served = 1;
                }
                else if (path_exists(found_path))
                {
                    snprintf(logbuf, sizeof(logbuf), "SKIP (not a file): %s", found_path);
                    do_log(g_conf.logfile, logbuf);
                    served = 1; /* Stop searching, but don't count as served */
                }
            }
            if (!served)
            {
                snprintf(logbuf, sizeof(logbuf), "NOT FOUND: %s", req_name);
                do_log(g_conf.logfile, logbuf);
            }
        }
        else
        {
            snprintf(logbuf, sizeof(logbuf), "NOT FOUND: %s (pub: %s)", req_name, g_conf.pubdir);
            do_log(g_conf.logfile, logbuf);
        }
    }

    fclose(req_f);

    if (rsp_f)
        fclose(rsp_f);

    snprintf(logbuf, sizeof(logbuf), "done: %d file(s) found", found_count);
    do_log(g_conf.logfile, logbuf);

    /* Save tracking data */
    tracking_save();

    srifreq_config_cleanup();

    return (found_count > 0) ? 0 : 1;
}
