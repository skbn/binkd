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
    const char *ext;
    char lext[5];

    ext = strrchr(filename, '.');
    if (!ext || strlen(ext) < 3) return 0;

    strncpy(lext, ext, 4);
    lext[4] = '\0';
    strlower(lext);

    if ((strncmp(lext, ".su", 3) == 0 && strlen(lext) == 4) ||
        (strncmp(lext, ".mo", 3) == 0 && strlen(lext) == 4) ||
        (strncmp(lext, ".tu", 3) == 0 && strlen(lext) == 4) ||
        (strncmp(lext, ".we", 3) == 0 && strlen(lext) == 4) ||
        (strncmp(lext, ".th", 3) == 0 && strlen(lext) == 4) ||
        (strncmp(lext, ".fr", 3) == 0 && strlen(lext) == 4) ||
        (strncmp(lext, ".sa", 3) == 0 && strlen(lext) == 4))
    {
        return 1;
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
                /* Intentar ZIP */
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