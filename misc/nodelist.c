/*
 * nodelist.c -- FidoNet nodelist compiler for binkd
 *
 * nodelist.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#include "portable.h" /* Canonical portable layer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_FIELDS 20
#define MAX_VAL 256

static int split_fields(char *buf, char **fields, int maxfields)
{
    int n = 0;
    char *p = buf;

    while (n < maxfields)
    {
        fields[n++] = p;
        p = strchr(p, ',');

        if (!p)
            break;

        *p++ = '\0';
    }

    return n;
}

/* Case-insensitive flag search.  Returns 1 if found; fills val if present */
static int find_flag(char **fields, int nfields, int start, const char *flag, char *val, int vsize)
{
    int flen = (int)strlen(flag);
    int i;

    for (i = start; i < nfields; i++)
    {
        char *f = fields[i];
        int j;

        for (j = 0; j < flen; j++)
        {
            if (toupper((unsigned char)f[j]) != toupper((unsigned char)flag[j]))
                break;
        }

        if (j == flen)
        {
            if (val && vsize > 0)
            {
                if (f[flen] == ':')
                {
                    strncpy(val, f + flen + 1, (size_t)(vsize - 1));
                    val[vsize - 1] = '\0';
                }
                else
                    val[0] = '\0';
            }
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    const char *nl_file;
    const char *domain;
    FILE *in;
    FILE *out;
    char buf[MAX_LINE];
    char *fields[MAX_FIELDS];
    int nf;
    int cur_zone;
    int cur_net;
    long count;

    cur_zone = 0;
    cur_net = 0;
    count = 0;

    if (argc < 3)
    {
        fprintf(stderr,
                "Usage: nodelist <nodelist_file> <domain> [<output_file>]\n");
        return 1;
    }

    nl_file = argv[1];
    domain = argv[2];
    out = (argc >= 4) ? fopen(argv[3], "w") : stdout;

    if (!out)
    {
        perror(argv[3]);
        return 1;
    }

    in = fopen(nl_file, "r");

    if (!in)
    {
        perror(nl_file);

        if (out != stdout)
            fclose(out);

        return 1;
    }

    while (fgets(buf, sizeof(buf), in))
    {
        char type[32];
        char ibn_port[32];
        char ina_host[MAX_VAL];
        int node_num;
        int port;
        int flags_start;

        str_trim(buf);

        if (!buf[0] || buf[0] == ';')
            continue;

        nf = split_fields(buf, fields, MAX_FIELDS);

        if (nf < 2)
            continue;

        if (fields[0][0] == '\0')
        {
            /* Line started with comma -- plain Node entry */
            strcpy(type, "Node");
            node_num = atoi(fields[1]);
            flags_start = 7;
        }
        else
        {
            strncpy(type, fields[0], sizeof(type) - 1);
            type[sizeof(type) - 1] = '\0';
            node_num = atoi(fields[1]);
            flags_start = 7;
        }

        /* Update zone / net context */
        if (!strcmp(type, "Zone") || !strcmp(type, "ZONE"))
        {
            cur_zone = node_num;
            cur_net = node_num;
            continue;
        }

        if (!strcmp(type, "Region") || !strcmp(type, "REGION"))
        {
            cur_net = node_num;
            continue;
        }

        if (!strcmp(type, "Host") || !strcmp(type, "HOST"))
            cur_net = node_num;

        /* Skip unusable types */
        if (!strcmp(type, "Pvt") || !strcmp(type, "PVT") || !strcmp(type, "Hold") || !strcmp(type, "HOLD") || !strcmp(type, "Down") || !strcmp(type, "DOWN") || !strcmp(type, "Boss") || !strcmp(type, "BOSS"))
            continue;

        /* Must have IBN flag */
        if (!find_flag(fields, nf, flags_start, "IBN", ibn_port, (int)sizeof(ibn_port)))
            continue;

        /* Need INA:hostname */
        ina_host[0] = '\0';
        find_flag(fields, nf, flags_start, "INA", ina_host, (int)sizeof(ina_host));

        if (!ina_host[0])
            continue;

        port = (ibn_port[0] && atoi(ibn_port) > 0) ? atoi(ibn_port) : 24554;

        fprintf(out, "node %d:%d/%d@%s %s:%d -\n", cur_zone, cur_net, node_num, domain, ina_host, port);

        count++;
    }

    fclose(in);

    if (out != stdout)
        fclose(out);

    fprintf(stderr, "nodelist: %ld BinkP node(s) found\n", count);

    return 0;
}
