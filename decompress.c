/* gcc -O2 -noixemul -o decompress decompress.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

/* Convert to lowercase */
void strlower(char *s)
{
    while (*s) {
        *s = tolower((unsigned char)*s);
        s++;
    }
}

/* Check if it's a valid extension */
int is_compressible(const char *filename)
{
    const char *p;

    for (p = filename; *p; p++) {

		/* Need three chars */
        if (p[0] == '.' && p[1] && p[2] && p[3])
        {
            char a = tolower((unsigned char)p[1]);
            char b = tolower((unsigned char)p[2]);
            char c = tolower((unsigned char)p[3]);

            /* validar patrón .xx? */
            if (isalpha(a) && isalpha(b) && (isalnum(c) || c == '?'))
            {
                if ((a == 's' && b == 'u') ||
                    (a == 'm' && b == 'o') ||
                    (a == 't' && b == 'u') ||
                    (a == 'w' && b == 'e') ||
                    (a == 't' && b == 'h') ||
                    (a == 'f' && b == 'r') ||
                    (a == 's' && b == 'a'))
                {
                    return 1;
                }
            }
        }
    }

    return 0;
}

/* Check if it's a pkt */
int is_pkt(const char *filename)
{
    const char *ext;
    char lext[5];

    ext = strrchr(filename, '.');
    if (!ext) return 0;

    strncpy(lext, ext, 4);
    lext[4] = '\0';
    strlower(lext);

    return (strcmp(lext, ".pkt") == 0);
}

/* Delete compressed file */
int delete_file(const char *src)
{
    char cmd[256];
    sprintf(cmd, "delete \"%s\"", src);
    return system(cmd);
}

int main(void)
{
    DIR *dp;
    struct dirent *entry;
    char path[512];
    char cmd[512];

    dp = opendir("inbound/");
    if (dp == NULL) {
        fprintf(stderr, "It cannot be opened inbound/\n");
        return 1;
    }

    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        sprintf(path, "inbound/%s", entry->d_name);
        printf("Processing: %s\n", path);

        if (is_compressible(entry->d_name)) {
            /* Try with LZH */
            sprintf(cmd, "lha x \"%s\" \"inbound/\"", path);
            if (system(cmd) == 0) {
                printf("Decompressed with LZH in inbound: %s\n", entry->d_name);
                delete_file(path);
            } else {
                /* Try with ZIP */
                sprintf(cmd, "unzip -o \"%s\" -d \"inbound/\"", path);
                if (system(cmd) == 0) {
                    printf("Decompressed with ZIP in inbound: %s\n", entry->d_name);
                    delete_file(path);
                } else {
                    printf("It could not be decompressed: %s\n", entry->d_name);
                }
            }
        } 
    }

    closedir(dp);
    return 0;
}