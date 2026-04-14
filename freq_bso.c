/* gcc -O2 -noixemul -Wall -o freq_bso freq_bso.c */

/*
 * Base directory from $BINKD_OUTBOUND or PROGDIR:.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proto/dos.h>
#include <dos/dos.h>

int main(int argc, char *argv[])
{
    char addr_copy[128];
    unsigned int zone=0, net=0, node=0, point=0;
    char fname[16], base_dir[256], out_dir[300];
    char req_path[512], clo_path[512];
    char *env;
    FILE *f;
    BPTR lock;

    if (argc < 3) {
        printf("Usage: freq_bso Z:N/NODE[.POINT] FILE\n");
        printf("  Creates outbound.0ZZ/NNNNNNNN.req (always with zone suffix)\n");
        printf("  Base dir from $BINKD_OUTBOUND or PROGDIR:\n");
        return 1;
    }

    strncpy(addr_copy, argv[1], sizeof(addr_copy)-1);
    addr_copy[sizeof(addr_copy)-1] = '\0';

    /* parse address - with or without point */
    if (sscanf(addr_copy, "%u:%u/%u.%u", &zone,&net,&node,&point) < 3 &&
        sscanf(addr_copy, "%u:%u/%u",    &zone,&net,&node) < 3) {
        printf("Invalid address: %s\n", argv[1]);
        return 1;
    }

    /* BSO filename: net and node as 4-digit uppercase hex */
    sprintf(fname, "%04X%04X", net, node);

    /* base directory */
    env = getenv("BINKD_OUTBOUND");

    if (env && *env) {
        strncpy(base_dir, env, sizeof(base_dir)-1);
        base_dir[sizeof(base_dir)-1] = '\0';
    } else {
        lock = GetProgramDir();

        if (!NameFromLock(lock, base_dir, sizeof(base_dir))) {
            printf("Error: cannot get program directory\n");
            return 1;
        }
    }

    /* ALWAYS use outbound.0ZZ/ with hex zone - never plain outbound/ */
    sprintf(out_dir, "%s/outbound.%03X", base_dir, zone);

    /* create outbound.0ZZ if it does not exist */
    lock = Lock(out_dir, ACCESS_READ);

    if (lock) {
        UnLock(lock);
    } else {
        lock = CreateDir(out_dir);

        if (!lock) {
			printf("Error creating: %s\n", out_dir);
			return 1;
		}

        UnLock(lock);
    }

    sprintf(req_path, "%s/%s.req", out_dir, fname);
    sprintf(clo_path, "%s/%s.clo", out_dir, fname);

    /* REQ: one file name per line, append so multiple requests accumulate */
    f = fopen(req_path, "a");

    if (!f) {
		printf("Error opening REQ: %s\n", req_path);
		return 1;
	}

    fprintf(f, "%s\r\n", argv[2]);
    fclose(f);

    /* CLO: absolute path to the REQ file */
    f = fopen(clo_path, "a");

    if (!f) {
		printf("Error opening CLO: %s\n", clo_path);
		return 1;
	}

    fprintf(f, "%s\r\n", req_path);
    fclose(f);

    printf("BSO FREQ:\n");
    printf("  Node : %u:%u/%u", zone, net, node);

    if (point) printf(".%u", point);

    printf("\n  File : %s\n  Dir  : %s\n  REQ  : %s\n  CLO  : %s\n",
           argv[2], out_dir, req_path, clo_path);

    return 0;
}
