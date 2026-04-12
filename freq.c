/* gcc -O2 -noixemul -Wall -o freq freq.c */
/*
freq 1:320/219 ALLFILES.ZIP
https://www.fidonet.fi/pub/nodelist/BINKDLST.INC
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <proto/dos.h>
#include <dos/dos.h>

int main(int argc, char *argv[])
{
    char addr_copy[128];
    char *addr;
    char *file;
    char *dot;

    unsigned int zone = 0, net = 0, node = 0, point = 0;

    char fname[64];
    char base_dir[256];
    char out_dir[256];
    char req_path[256];
    char clo_path[256];

    FILE *f_req;
    FILE *f_clo;

    BPTR lock;

    if (argc < 3) {
        printf("Use: freq Z:N/NODE[.POINT] FILE\n");
        return 1;
    }

    addr = argv[1];
    file = argv[2];

    strncpy(addr_copy, addr, sizeof(addr_copy) - 1);
    addr_copy[sizeof(addr_copy) - 1] = '\0';

    if (sscanf(addr_copy, "%u:%u/%u.%u", &zone, &net, &node, &point) < 3) {
        printf("Invalid address\n");
        return 1;
    }

    if (point)
        sprintf(fname, "%u.%u.%u.%u", zone, net, node, point);
    else
        sprintf(fname, "%u.%u.%u.0", zone, net, node);

    lock = GetProgramDir();
    NameFromLock(lock, base_dir, sizeof(base_dir));

    sprintf(out_dir, "%s/outbound", base_dir);
    sprintf(req_path, "%s/%s.req", out_dir, fname);

    /* REQ */
    f_req = fopen(req_path, "w");
    if (!f_req) {
        printf("Error creating REQ\n");
        return 1;
    }

    fprintf(f_req, "%s\r\n", file);
    fclose(f_req);

    /* CLO */
    strcpy(clo_path, req_path);

    dot = strrchr(clo_path, '.');
    if (dot)
        strcpy(dot, ".clo");

    f_clo = fopen(clo_path, "w");
    if (!f_clo) {
        printf("Error creating CLO\n");
        return 1;
    }

    fprintf(f_clo, "%s\r\n", req_path);
    fclose(f_clo);

    printf("REQ OK: %s\n", req_path);
    printf("CLO OK: %s\n", clo_path);

    return 0;
}