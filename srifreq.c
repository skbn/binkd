/*  gcc -O2 -noixemul -Wall -o srifreq srifreq.c */

/*
 * Use binkd.conf:
 *   exec "path/srifreq *S" *.req
 *
 * CONFIGURATION (environment variables or arguments)
 *   SRIFREQ_PUBDIR   Directory with public files (default: pub/)
 *   SRIFREQ_LOG      Log file (optional)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <exec/types.h>

#define MAX_LINE	512
#define MAX_PATH	256
#define MAX_REQS	64

typedef struct {
    char sysop[128];
    char aka[256];
    char request_list[MAX_PATH];
    char response_file[MAX_PATH];
    char our_aka[128];
    char remote_status[32];
    char system_status[32];
    char password[64];
    int got_request_list;
    int got_response_file;
} SRIF;

static void str_trim(char *s)
{
    int n = (int)strlen(s);

    while (n > 0 && (s[n-1] == '\r' || s[n-1] == '\n' || s[n-1] == ' '))
        s[--n] = '\0';
}

static void str_upper(char *s)
{
    while (*s) {
		*s = (char)toupper((unsigned char)*s);
		s++;
	}
}

static int parse_srif(const char *path, SRIF *srif)
{
    FILE *f;
    char line[MAX_LINE];
    char token[MAX_LINE], value[MAX_LINE];

    memset(srif, 0, sizeof(SRIF));

    f = fopen(path, "r");

    if (!f) return 0;

    while (fgets(line, sizeof(line), f)) {
        str_trim(line);

        if (!line[0]) continue;

        /* token = first word, value = the rest */
        token[0] = '\0'; value[0] = '\0';

        if (sscanf(line, "%s %[^\n]", token, value) < 1)
			continue;

        str_upper(token);

        if (!strcmp(token, "SYSOP")) {
			strncpy(srif->sysop, value, sizeof(srif->sysop)-1);
		}
        else if (!strcmp(token, "AKA")) {
			if (!srif->aka[0])
				strncpy(srif->sysop, value, sizeof(srif->aka)-1);
		}
        else if (!strcmp(token, "REQUESTLIST")) {
			strncpy(srif->request_list,  value, MAX_PATH-1);
			srif->got_request_list = 1;
		}
        else if (!strcmp(token, "RESPONSEFILE")) {
			strncpy(srif->response_file, value, MAX_PATH-1);
			srif->got_response_file = 1;
		}
        else if (!strcmp(token, "OURAKA")) {
			strncpy(srif->our_aka, value, sizeof(srif->our_aka)-1);
		}
        else if (!strcmp(token, "REMOTESTATUS")) {
			strncpy(srif->remote_status, value, sizeof(srif->remote_status)-1);
		}
        else if (!strcmp(token, "SYSTEMSTATUS")) {
			strncpy(srif->system_status, value, sizeof(srif->system_status)-1);
		}
        else if (!strcmp(token, "PASSWORD")) {
			strncpy(srif->password, value, sizeof(srif->password)-1);
		}
    }

    fclose(f);

    return srif->got_request_list;
}

static void do_log(const char *logpath, const char *msg)
{
    FILE *lf;

    if (!logpath || !*logpath) return;

    lf = fopen(logpath, "a");

    if (lf) {
		fprintf(lf, "srifreq: %s\n", msg);
		fclose(lf);
	}
}

int main(int argc, char *argv[])
{
    const char *srif_path;
    const char *pub_dir;
    const char *log_path;
    SRIF srif;
    FILE *req_f, *rsp_f;
    char line[MAX_LINE];
    char req_name[MAX_LINE];
    char found_path[MAX_PATH];
    char logbuf[MAX_PATH + 64];
    BPTR lock;
    int found_count = 0;

    if (argc < 2) {
        printf("Use: srifreq <srif_file>\n");
        printf("Variables: SRIFREQ_PUBDIR (public dir), SRIFREQ_LOG (log)\n");
        return 1;
    }

    srif_path = argv[1];
    pub_dir = getenv("SRIFREQ_PUBDIR");
    log_path = getenv("SRIFREQ_LOG");

    if (!pub_dir || !*pub_dir) {
        /* default: exe directory + /pub */
        static char default_pub[MAX_PATH];
        BPTR plock = GetProgramDir();

        if (NameFromLock(plock, default_pub, sizeof(default_pub)-4))
            strcat(default_pub, "/pub");

        pub_dir = default_pub;
    }

    sprintf(logbuf, "processing SRIF: %s", srif_path);
    do_log(log_path, logbuf);

    if (!parse_srif(srif_path, &srif)) {
        sprintf(logbuf, "ERROR: cannot parse SRIF or missing RequestList: %s", srif_path);
        do_log(log_path, logbuf);
        fprintf(stderr, "srifreq: %s\n", logbuf);
        return 1;
    }

    sprintf(logbuf, "sysop: %s, req: %s", srif.sysop, srif.request_list);
    do_log(log_path, logbuf);

    /* Read the .req file and search for each file in pub_dir */
    req_f = fopen(srif.request_list, "r");

    if (!req_f) {
        sprintf(logbuf, "ERROR: cannot open RequestList: %s", srif.request_list);
        do_log(log_path, logbuf);
        return 1;
    }

    /* open response file if specified */
    rsp_f = NULL;
    if (srif.got_response_file && srif.response_file[0]) {
        rsp_f = fopen(srif.response_file, "w");

        if (!rsp_f) {
            sprintf(logbuf, "WARNING: cannot create ResponseFile: %s", srif.response_file);
            do_log(log_path, logbuf);
        }
    }

    while (fgets(line, sizeof(line), req_f)) {
        str_trim(line);

        if (!line[0] || line[0] == ';' || line[0] == '#')
			continue;

        /* The req can have a magic URL or filename */
        /* Extract only the first token (name, no action options) */
        if (sscanf(line, "%s", req_name) < 1) continue;

        /* Ignore http/ftp URLs */
        if (strncmp(req_name, "http", 4) == 0 || strncmp(req_name, "ftp", 3) == 0)
            continue;

        /* search for the file in pub_dir */
        sprintf(found_path, "%s/%s", pub_dir, req_name);
        lock = Lock(found_path, ACCESS_READ);

        if (lock) {
            UnLock(lock);
            sprintf(logbuf, "FOUND: %s -> %s", req_name, found_path);
            do_log(log_path, logbuf);
            printf("srifreq: found %s\n", found_path);
            found_count++;

            /* Write to the response file if it exists */
            if (rsp_f)
                fprintf(rsp_f, "%s\r\n", found_path);
        } else {
            sprintf(logbuf, "NOT FOUND: %s (searched in %s)", req_name, pub_dir);
            do_log(log_path, logbuf);
            printf("srifreq: not found: %s\n", req_name);
        }
    }

    fclose(req_f);
    if (rsp_f) fclose(rsp_f);

    sprintf(logbuf, "done: %d file(s) found", found_count);
    do_log(log_path, logbuf);
    printf("srifreq: %d file(s) found\n", found_count);

    return (found_count > 0) ? 0 : 1;
}
