/*
 * bsycleanup.c -- Cleanup functions for .bsy/.csy/.try files
 *
 * bsycleanup.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "sys.h"
#include "readcfg.h"
#include "ftnq.h"
#include "ftnnode.h"
#include "tools.h"
#include "readdir.h"

/*
 * Clean up old .bsy and .csy files at startup
 * Scans all domain outbounds
 */

/* Helper: scan a single directory for .bsy/.csy files and delete them */
static void scan_and_delete_bsy_in_dir(const char *dir, BINKD_CONFIG *config)
{
    DIR *dp;
    struct dirent *de;
    char buf[MAXPATHLEN + 1];

    if ((dp = opendir(dir)) == 0)
    {
        return;
    }

    while ((de = readdir(dp)) != 0)
    {
        char *s = de->d_name;
        int len = strlen(s);

        if (len > 4 && (!STRICMP(s + len - 4, ".bsy") || !STRICMP(s + len - 4, ".csy") || !STRICMP(s + len - 4, ".try")))
        {
            strnzcpy(buf, dir, sizeof(buf));
            strnzcat(buf, PATH_SEPARATOR, sizeof(buf));
            strnzcat(buf, s, sizeof(buf));

            Log(2, "deleting %s", buf);

            delete(buf);
        }
    }

    closedir(dp);
}

void cleanup_old_bsy(BINKD_CONFIG *config)
{
    DIR *dp;
    char outb_path[MAXPATHLEN + 1], base_path[MAXPATHLEN + 1];
    struct dirent *de;
    FTN_DOMAIN *curr_domain;
    int len;

    Log(2, "cleaning up .bsy/.csy/.try files at startup");

    /* Scan all domain outbounds */
    for (curr_domain = config->pDomains.first; curr_domain; curr_domain = curr_domain->next)
    {
        if (curr_domain->alias4 != 0)
            continue;

        /* Build base path: path + separator */
        strnzcpy(base_path, curr_domain->path, sizeof(base_path));
#ifndef AMIGA
        if (base_path[strlen(base_path) - 1] == ':')
            strcat(base_path, PATH_SEPARATOR);
#endif

#ifdef AMIGADOS_4D_OUTBOUND
        if (config->aso)
        {
            /* ASO mode: direct outbound path */
            strnzcpy(outb_path, base_path, sizeof(outb_path));
            strnzcat(outb_path, PATH_SEPARATOR, sizeof(outb_path));
            strnzcat(outb_path, curr_domain->dir, sizeof(outb_path));
            Log(7, "cleanup_old_bsy (ASO): scanning domain '%s', path '%s'", curr_domain->name, outb_path);
            scan_and_delete_bsy_in_dir(outb_path, config);
        }
        else
#endif
        {
            /* BSO mode: scan for outbound.xxx directories */
            Log(7, "cleanup_old_bsy (BSO): scanning domain '%s', base '%s'", curr_domain->name, base_path);

            if ((dp = opendir(base_path)) == 0)
                continue;

            len = strlen(curr_domain->dir);

            while ((de = readdir(dp)) != 0)
            {
                /* Match outbound or outbound.xxx */
                if (!STRNICMP(de->d_name, curr_domain->dir, len) && (de->d_name[len] == 0 || (de->d_name[len] == '.' && isxdigit(de->d_name[len + 1]))))
                {
                    strnzcpy(outb_path, base_path, sizeof(outb_path));
                    strnzcat(outb_path, PATH_SEPARATOR, sizeof(outb_path));
                    strnzcat(outb_path, de->d_name, sizeof(outb_path));

                    Log(7, "cleanup_old_bsy (BSO): scanning outbound dir '%s'", outb_path);

                    scan_and_delete_bsy_in_dir(outb_path, config);
                }
            }

            closedir(dp);
        }
    }
}
