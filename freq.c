/* gcc -O2 -noixemul -Wall -o freq freq.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proto/dos.h>
#include <dos/dos.h>

int main(int argc, char *argv[])
{
    char addr_copy[128];
    unsigned int zone=0, net=0, node=0, point=0;
    char fname[64], base_dir[256], out_dir[300];
    char req_path[512], clo_path[512];
    char *dot, *env_outbound;
    FILE *f_req, *f_clo;
    BPTR lock;

    if (argc < 3) {
        printf("Use: freq Z:N/NODE[.POINT] FILE\n");
        return 1;
    }

    strncpy(addr_copy, argv[1], sizeof(addr_copy)-1);
    addr_copy[sizeof(addr_copy)-1] = '\0';

    if (sscanf(addr_copy, "%u:%u/%u.%u", &zone,&net,&node,&point) < 3 &&
        sscanf(addr_copy, "%u:%u/%u",    &zone,&net,&node) < 3) {
        printf("Invalid address: %s\n", argv[1]);
        return 1;
    }

    sprintf(fname, "%u.%u.%u.%u", zone, net, node, point);

    env_outbound = getenv("BINKD_OUTBOUND");
    if (env_outbound && *env_outbound) {
        strncpy(base_dir, env_outbound, sizeof(base_dir)-1);
        base_dir[sizeof(base_dir)-1] = '\0';
    } else {
        lock = GetProgramDir();

        if (!NameFromLock(lock, base_dir, sizeof(base_dir))) {
            printf("Error retrieving program directory\n");
            return 1;
        }
    }

    sprintf(out_dir, "%s/outbound", base_dir);

    lock = Lock(out_dir, ACCESS_READ);
    if (lock) {
		UnLock(lock);
	}
    else {
        lock = CreateDir(out_dir);
        if (!lock)
		{
			printf("Error creating: %s\n", out_dir); return 1;
		}
        UnLock(lock);
    }

    sprintf(req_path, "%s/%s.req", out_dir, fname);
    strncpy(clo_path, req_path, sizeof(clo_path)-1);
    clo_path[sizeof(clo_path)-1] = '\0';
    dot = strrchr(clo_path, '.');

    if (dot) strcpy(dot, ".clo");

    f_req = fopen(req_path, "a");

    if (!f_req)
	{
		printf("Error opening REQ: %s\n", req_path);
		return 1;
	}

    fprintf(f_req, "%s\r\n", argv[2]);
    fclose(f_req);

    f_clo = fopen(clo_path, "a");

    if (!f_clo)
	{
		printf("Error opening CLO: %s\n", clo_path);
		return 1;
	}

    fprintf(f_clo, "%s\r\n", req_path);
    fclose(f_clo);

    printf("FREQ ASO:\n  Node : %u:%u/%u", zone, net, node);

    if (point) printf(".%u", point);

    printf("\n  File: %s\n  REQ: %s\n  CLO: %s\n", argv[2], req_path, clo_path);

    return 0;
}
