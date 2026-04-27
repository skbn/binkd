/*
 * freq.c -- Append a file-request entry to an outbound .req / .clo pair
 *
 * freq.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#include "portable.h" /* Canonical portable layer */

#define FREQ_MAX_PATH (MAXPATHLEN + 1)

/* build_aso_paths -- ASO flat layout */
static int build_aso_paths(const char *outbound, unsigned int zone, unsigned int net, unsigned int node, unsigned int point, char *req_path, char *clo_path, int pathsize)
{
    char *dot;

    if (mkdir_recursive(outbound) < 0 && !path_exists(outbound))
        return -1;

    snprintf(req_path, (size_t)pathsize, "%s/%u.%u.%u.%u.req", outbound, zone, net, node, point);
    safe_strncpy(clo_path, req_path, pathsize);
    dot = strrchr(clo_path, '.');

    if (dot)
        strcpy(dot, ".clo");

    return 0;
}

/* build_bso_paths -- BSO BinkleyStyle layout (lowercase hex)
 * use_zone_ext: 0 = outbound/ (binkd default), 1 = outbound.ZZZ/ (with zone ext) */
static int build_bso_paths(const char *outbound, unsigned int zone, unsigned int net, unsigned int node, unsigned int point, int use_zone_ext, char *req_path, char *clo_path, int pathsize)
{
    char zone_dir[FREQ_MAX_PATH];
    char node_dir[FREQ_MAX_PATH];
    const char *base_dir;

    if (use_zone_ext)
    {
        /* Zone dir: <outbound>.ZZZ or <outbound>.ZZZZ (lowercase hex)
         * FTS-5005: If zone > 4095, use 4 hex digits instead of 3 */
        if (zone > 0xFFF)
            snprintf(zone_dir, sizeof(zone_dir), "%s.%04x", outbound, zone);
        else
            snprintf(zone_dir, sizeof(zone_dir), "%s.%03x", outbound, zone);

        str_tolower(zone_dir);
        base_dir = zone_dir;
    }
    else
    {
        /* No zone extension: use outbound/ directly (binkd default) */
        base_dir = outbound;
    }

    if (mkdir_recursive(base_dir) < 0 && !path_exists(base_dir))
        return -1;

    if (point == 0)
    {
        snprintf(req_path, (size_t)pathsize, "%s/%04x%04x.req", base_dir, net, node);
        snprintf(clo_path, (size_t)pathsize, "%s/%04x%04x.clo", base_dir, net, node);
    }
    else
    {
        snprintf(node_dir, sizeof(node_dir), "%s/%04x%04x.pnt", base_dir, net, node);
        str_tolower(node_dir);

        if (mkdir_recursive(node_dir) < 0 && !path_exists(node_dir))
            return -1;

        snprintf(req_path, (size_t)pathsize, "%s/%08x.req", node_dir, point);
        snprintf(clo_path, (size_t)pathsize, "%s/%08x.clo", node_dir, point);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    unsigned int zone, net, node, point;
    char addr_copy[128];
    char req_path[FREQ_MAX_PATH];
    char clo_path[FREQ_MAX_PATH];
    char abs_outbound[FREQ_MAX_PATH];
    FILE *f;
    const char *outbound;
    const char *arg_outbound;
    const char *arg_addr;
    const char *password = NULL; /* --password <pass> !pass suffix */
    long newer_than = 0;         /* --newer-than <unixts> +ts suffix */
    int update = 0;              /* --update  U suffix */
    int use_bso = 0;
    int use_zone_ext = 0; /* --zone-ext use outbound.ZZZ/ */
    int argi = 1;
    int nfiles = 0;

    zone = net = node = point = 0;

    /* Parse flags -- All optional, order-independent, before positional args */
    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] == '-')
    {
        if (strcmp(argv[argi], "--bso") == 0)
        {
            use_bso = 1;
            argi++;
        }
        else if (strcmp(argv[argi], "--aso") == 0)
        {
            use_bso = 0;
            argi++;
        }
        else if (strcmp(argv[argi], "--zone-ext") == 0)
        {
            use_zone_ext = 1;
            argi++;
        }
        else if (strcmp(argv[argi], "--update") == 0)
        {
            update = 1;
            argi++;
        }
        else if (strcmp(argv[argi], "--password") == 0 && argi + 1 < argc)
        {
            password = argv[++argi];
            argi++;
        }
        else if (strcmp(argv[argi], "--newer-than") == 0 && argi + 1 < argc)
        {
            /* Binkds copy */
            /* Support both Unix timestamp (seconds since epoch) and relative time
             * - With suffix (7d, 1h, 30m): treated as relative time from now
             * - Plain number: if > 1 year in seconds (31536000), treated as Unix timestamp
             * otherwise treated as relative seconds */
            char *time_arg = argv[++argi];
            long parsed = parse_time(time_arg);

            /* Heuristic: if value has no letter suffix and is > 1 year in seconds,
             * assume it's an absolute Unix timestamp (year 2001+)
             * Otherwise it's relative time */
            if (parsed > 31536000L)  /* > 1 year ≈ Unix timestamps from 1971+ */
                newer_than = parsed; /* Absolute Unix timestamp */
            else
                newer_than = time(NULL) - parsed; /* Relative: seconds/minutes/hours/days ago */
            argi++;
        }
        else
            break; /* Unknown flag — stop, treat rest as positional */
    }

    if (argc - argi < 3)
    {
        fprintf(stderr, "Usage: freq [--bso|--aso] [--zone-ext] [--update] [--password <pass>] [--newer-than <time>] <outbound> <address> <files>...\n");
        fprintf(stderr, "  --bso            Use BSO outbound structure (default: outbound/)\n");
        fprintf(stderr, "  --zone-ext       Use zone extension (outbound.002/) with --bso\n");
        fprintf(stderr, "  --aso            Use ASO flat outbound structure\n");
        fprintf(stderr, "  --password <pw>  append !pw to each request line\n");
        fprintf(stderr, "  --newer-than <t> append +<timestamp> (request if file is newer)\n");
        fprintf(stderr, "                   <t> can be: 7d=7 days, 1h=1 hour, 30m=30 min\n");
        fprintf(stderr, "                   Or Unix timestamp: values > 1 year = absolute\n");
        fprintf(stderr, "  --update         append U flag (update request)\n");
        fprintf(stderr, "Multiple filenames can be listed after the address.\n");
        return 1;
    }

    arg_outbound = argv[argi++];
    arg_addr = argv[argi++];

    /* Remaining args are filenames */
    if (!make_abs_path(arg_outbound, abs_outbound, (int)sizeof(abs_outbound)))
    {
        fprintf(stderr, "freq: cannot resolve outbound path: %s\n", arg_outbound);
        return 1;
    }

    if (!is_directory(abs_outbound))
    {
        fprintf(stderr, "freq: outbound is not a directory: %s\n", abs_outbound);
        return 1;
    }

    outbound = abs_outbound;

    safe_strncpy(addr_copy, arg_addr, (int)sizeof(addr_copy));

    if (sscanf(addr_copy, "%u:%u/%u.%u", &zone, &net, &node, &point) < 3 && sscanf(addr_copy, "%u:%u/%u", &zone, &net, &node) < 3)
    {
        fprintf(stderr, "freq: invalid address: %s\n", arg_addr);
        return 1;
    }

    if (use_bso)
    {
        if (build_bso_paths(abs_outbound, zone, net, node, point, use_zone_ext, req_path, clo_path, FREQ_MAX_PATH) != 0)
        {
            fprintf(stderr, "freq: cannot create BSO outbound paths\n");
            return 1;
        }
    }
    else
    {
        if (build_aso_paths(abs_outbound, zone, net, node, point, req_path, clo_path, FREQ_MAX_PATH) < 0)
        {
            fprintf(stderr, "freq: cannot create ASO outbound directory: %s\n", outbound);
            return 1;
        }
    }

    /* Append all filenames to .req */
    f = fopen(req_path, "a");

    if (!f)
    {
        fprintf(stderr, "freq: cannot open REQ: %s\n", req_path);
        return 1;
    }

    for (; argi < argc; argi++)
    {
        const char *fname = argv[argi];

        /* Validate filename for safety */
        if (!is_safe_filename(fname))
        {
            fprintf(stderr, "freq: invalid filename: %s\n", fname);
            fclose(f);
            return 1;
        }

        /* Build request line: filename [!password] [+timestamp] [U] */
        fprintf(f, "%s", fname);

        if (password && password[0])
            fprintf(f, " !%s", password);

        if (newer_than > 0)
            fprintf(f, " +%ld", newer_than);

        if (update)
            fprintf(f, " U");

        fprintf(f, "\r\n");
        nfiles++;
    }

    fclose(f);

    if (nfiles == 0)
    {
        fprintf(stderr, "freq: no filenames specified\n");
        return 1;
    }

    /* Append .req full path to .clo (once per invocation) */
    f = fopen(clo_path, "a");

    if (!f)
    {
        fprintf(stderr, "freq: cannot open CLO: %s\n", clo_path);
        return 1;
    }

    fprintf(f, "%s\r\n", req_path);
    fclose(f);

    printf("freq (%s): node %u:%u/%u", use_bso ? "bso" : "aso", zone, net, node);

    if (point)
        printf(".%u", point);

    printf("  %d file(s)\n  REQ : %s\n  CLO : %s\n", nfiles, req_path, clo_path);

    return 0;
}
