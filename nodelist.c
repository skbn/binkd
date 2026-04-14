/* gcc -O2 -noixemul -Wall -o nodelist nodelist.c */

/*
 * FidoNet nodelist compiler for binkd - AmigaOS version (no ixemul)
 *
 * Usage: nodelist <nodelist_file> <domain> [<output_file>]
 *
 * Reads a FidoNet nodelist, extracts nodes with IBN flag (BinkP)
 * and generates "node" lines for binkd.conf.
 *
 * Nodelist line format (comma-separated):
 *   [type,]node_num,name,city,sysop,phone,baud,flag1,flag2,...
 *   type: Zone/Region/Host/Hub/Pvt/Hold/Down/Boss/Node (empty = Node)
 *   Relevant flags: IBN[:port]  INA:hostname
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE	1024
#define MAX_FIELDS	64
#define MAX_VAL		256

/* Split a nodelist line into fields, handling the optional type prefix.
 * Returns number of fields. Fields array points into buf (modified). */
static int split_fields(char *buf, char **fields, int maxfields)
{
    int n = 0;
    char *p = buf;

    while (n < maxfields) {
        fields[n++] = p;
        p = strchr(p, ',');

        if (!p) break;

        *p++ = '\0';
    }
    return n;
}

static void str_trim(char *s)
{
    int n = (int)strlen(s);

    while (n > 0 && ((unsigned char)s[n-1] < 32 || s[n-1] == ' '))
        s[--n] = '\0';
}

/* Case-insensitive search for "FLAG" or "FLAG:value" in a comma-separated
 * flags string (fields[7] onwards joined, or individual field array). */
static int find_flag(char **fields, int nfields, int start,
                     const char *flag, char *val, int vsize)
{
    int flen = (int)strlen(flag);
    int i;

    for (i = start; i < nfields; i++) {
        char *f = fields[i];

        /* case-insensitive compare */
        int j;
        for (j = 0; j < flen; j++) {
            if (toupper((unsigned char)f[j]) != toupper((unsigned char)flag[j]))
                break;
        }

        if (j == flen) {
            if (f[flen] == ':' && val && vsize > 0) {
                strncpy(val, f + flen + 1, vsize - 1);
                val[vsize - 1] = '\0';
            } else if (val && vsize > 0)
                val[0] = '\0';

            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    const char *nl_file, *domain;
    FILE *in, *out;
    char buf[MAX_LINE];
    char *fields[MAX_FIELDS];
    int nf;
    int cur_zone = 0, cur_net = 0;
    long count = 0;

    /* Fields offsets: for lines WITH type: f[0]=type f[1]=num f[2..6]=name..baud f[7+]=flags
     * For lines WITHOUT type (start with ','): f[0]="" f[1]=num ... f[7+]=flags
     * Actually when line starts with ',' sscanf sees empty first field */

    if (argc < 3) {
        fprintf(stderr, "Usage: nodelist <nodelist_file> <domain> [<output>]\nExample: nodelist z2daily.104 fidonet nodes.cfg\n");
        return 1;
    }

    nl_file = argv[1];
    domain = argv[2];
    out = (argc >= 4) ? fopen(argv[3], "w") : stdout;

    if (!out) {
		perror(argv[3]); return 1;
	}

    in = fopen(nl_file, "r");

    if (!in) {
		perror(nl_file);

		if (out != stdout)
			fclose(out);

		return 1;
	}

    if (out != stdout)
        fprintf(out, "; Generated from %s, domain %s\n", nl_file, domain);

    while (fgets(buf, sizeof(buf), in)) {
        char type[32];
        char ibn_port[32], ina_host[MAX_VAL];
        int node_num;
        int port;
        int flags_start;  /* index in fields[] where flags begin */

        str_trim(buf);

        /* Skip comments and blanks */
        if (!buf[0] || buf[0] == ';')
			continue;

        nf = split_fields(buf, fields, MAX_FIELDS);

        if (nf < 2) continue;

        /* Determine if first field is a keyword or empty (Node type) */
        if (fields[0][0] == '\0') {
            /* Line started with comma - plain Node entry */
            strcpy(type, "Node");

            /* fields: [0]="" [1]=num [2]=name [3]=city [4]=sysop [5]=phone [6]=baud [7+]=flags */
            if (nf < 2) continue;

            node_num    = atoi(fields[1]);
            flags_start = 7;
        } else {
            /* First field is type keyword */
            strncpy(type, fields[0], sizeof(type)-1);
            type[sizeof(type)-1] = '\0';

            /* fields: [0]=type [1]=num [2]=name ... [6]=baud [7+]=flags */
            if (nf < 2) continue;

            node_num = atoi(fields[1]);
            flags_start = 7;
        }

        /* Update zone/net context */
        if (!strcmp(type, "Zone") || !strcmp(type, "ZONE")) {
            cur_zone = node_num;
            cur_net = node_num;
            continue;
        }

        if (!strcmp(type, "Region") || !strcmp(type, "REGION")) {
            cur_net = node_num;
            continue;
        }

        /* Host itself might have IBN, fall through */
        if (!strcmp(type, "Host") || !strcmp(type, "HOST")) {
            cur_net = node_num;
        }

        /* Skip unusable node types */
        if (!strcmp(type, "Pvt") || !strcmp(type, "PVT") ||
            !strcmp(type, "Hold") || !strcmp(type, "HOLD") ||
            !strcmp(type, "Down") || !strcmp(type, "DOWN") ||
            !strcmp(type, "Boss") || !strcmp(type, "BOSS"))
            continue;

        /* Must have IBN flag */
        if (!find_flag(fields, nf, flags_start, "IBN", ibn_port, sizeof(ibn_port)))
            continue;

        /* Need a hostname: INA:host */
        ina_host[0] = '\0';
        find_flag(fields, nf, flags_start, "INA", ina_host, sizeof(ina_host));

		/* No usable address */
        if (!ina_host[0]) continue;

        port = (ibn_port[0] && atoi(ibn_port) > 0) ? atoi(ibn_port) : 24554;

        fprintf(out, "node %d:%d/%d@%s %s:%d -\n",
                cur_zone, cur_net, node_num, domain, ina_host, port);

        count++;
    }

    fclose(in);

    if (out != stdout) {
        fprintf(out, "; Total: %ld BinkP nodes\n", count);
        fclose(out);
    }

    fprintf(stderr, "nodelist: %ld BinkP nodes found\n", count);
    return 0;
}
