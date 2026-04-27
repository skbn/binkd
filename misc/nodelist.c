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
    int cur_node;
    int cur_point;
    int is_pointlist;
    int boss_zone, boss_net, boss_node;
    long node_count;
    long point_count;
    int i;

    cur_zone = 0;
    cur_net = 0;
    cur_node = 0;
    cur_point = 0;
    is_pointlist = 0;
    boss_zone = 0;
    boss_net = 0;
    boss_node = 0;
    node_count = 0;
    point_count = 0;

    if (argc < 3)
    {
        fprintf(stderr, "Usage: nodelist [--pointlist] <nodelist_file> <domain> [<output_file>]\n"
                "       --pointlist  Process pointlist format (Boss/Point styles per FTS-5002)\n");
        return 1;
    }

    i = 1;

    if (strcmp(argv[i], "--pointlist") == 0)
    {
        is_pointlist = 1;
        i++;
    }

    if (argc - i < 2)
    {
        fprintf(stderr, "Usage: nodelist [--pointlist] <nodelist_file> <domain> [<output_file>]\n");
        return 1;
    }

    nl_file = argv[i++];
    domain = argv[i++];
    out = (argc > i) ? fopen(argv[i], "w") : stdout;

    if (!out)
    {
        perror(argv[i]);
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
            /* Line started with comma -- plain Node entry (or Point entry in Boss format) */
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

        /* Pointlist processing (FTS-5002 formats) */
        if (is_pointlist)
        {
            /* Boss format: Boss,Z:N/N followed by point entries */
            if (!strcmp(type, "Boss") || !strcmp(type, "BOSS"))
            {
                /* Parse boss address (field 1 or 2 depending on format) */
                char *addr_field = NULL;
                
                if (nf > 2 && fields[2] && (strchr(fields[2], ':') || strchr(fields[2], '/')))
                    addr_field = fields[2];
                else if (nf > 1 && fields[1] && (strchr(fields[1], ':') || strchr(fields[1], '/')))
                    addr_field = fields[1];
                
                if (addr_field)
                {
                    int parsed_zone = 0, parsed_net = 0, parsed_node = 0;
                    
                    if (strchr(addr_field, ':'))
                    {
                        /* Format: Z:N/N */
                        if (sscanf(addr_field, "%d:%d/%d", &parsed_zone, &parsed_net, &parsed_node) == 3)
                        {
                            boss_zone = parsed_zone;
                            boss_net = parsed_net;
                            boss_node = parsed_node;
                        }
                    }
                    else
                    {
                        /* Format: N/N (no zone) */
                        if (sscanf(addr_field, "%d/%d", &parsed_net, &parsed_node) == 2)
                        {
                            boss_net = parsed_net;
                            boss_node = parsed_node;
                            boss_zone = cur_zone > 0 ? cur_zone : 1;
                        }
                    }
                }
                continue;
            }

            /* Point format (FTS-5002 2.2): Point,N,... */
            if (!strcmp(type, "Point") || !strcmp(type, "POINT"))
            {
                int point_num = node_num; /* field 1 is point number in this format */

                /* Use current zone/net context, or boss context if set */
                int pzone = boss_zone > 0 ? boss_zone : (cur_zone > 0 ? cur_zone : 1);
                int pnet = boss_net > 0 ? boss_net : (cur_net > 0 ? cur_net : 0);
                int pboss = boss_node > 0 ? boss_node : (cur_node > 0 ? cur_node : 0);

                /* Check for IBN/INA flags (optional, defaults if not present) */
                if (!find_flag(fields, nf, flags_start, "IBN", ibn_port, (int)sizeof(ibn_port)))
                    ibn_port[0] = '\0';

                if (!find_flag(fields, nf, flags_start, "INA", ina_host, (int)sizeof(ina_host)))
                    ina_host[0] = '\0';

                port = (ibn_port[0] && atoi(ibn_port) > 0) ? atoi(ibn_port) : 24554;
               
                fprintf(out, "node %d:%d/%d.%d@%s %s:%d -\n", pzone, pnet, pboss, point_num, domain, ina_host[0] ? ina_host : "-", port);
                point_count++;
                continue;
            }

            /* In pointlist mode, regular nodes with leading comma are points (Boss format 2.1) */
            if (fields[0][0] == '\0' && boss_node > 0)
            {
                int point_num = node_num; /* field 1 contains point number */

                /* Check for IBN/INA flags (optional, defaults if not present) */
                if (!find_flag(fields, nf, flags_start, "IBN", ibn_port, (int)sizeof(ibn_port)))
                    ibn_port[0] = '\0';

                if (!find_flag(fields, nf, flags_start, "INA", ina_host, (int)sizeof(ina_host)))
                    ina_host[0] = '\0';

                port = (ibn_port[0] && atoi(ibn_port) > 0) ? atoi(ibn_port) : 24554;

                fprintf(out, "node %d:%d/%d.%d@%s %s:%d -\n", boss_zone, boss_net, boss_node, point_num, domain, ina_host[0] ? ina_host : "-", port);
               
                point_count++;

                continue;
            }
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

        if (!strcmp(type, "Node") || !strcmp(type, "NODE"))
            cur_node = node_num;

        /* Skip unusable types (including Boss/Point in regular nodelist mode) */
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

        node_count++;
    }

    fclose(in);

    if (out != stdout)
        fclose(out);

    if (is_pointlist)
        fprintf(stderr, "nodelist: %ld BinkP node(s), %ld point(s) found\n", node_count, point_count);
    else
        fprintf(stderr, "nodelist: %ld BinkP node(s) found\n", node_count);

    return 0;
}
