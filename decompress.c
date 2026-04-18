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
    char lext[64];
    char *dot;
    char day[4];
    int i;

    ext = strrchr(filename, '.');

    if (!ext) return 0;

    strncpy(lext, ext, sizeof(lext) - 1);
    lext[sizeof(lext) - 1] = '\0';
    strlower(lext);

    /* Strip .000 .001 .002 */
    dot = strrchr(lext, '.');
    if (dot && strlen(dot + 1) == 3 && isdigit((unsigned char)dot[1]) && isdigit((unsigned char)dot[2]) && isdigit((unsigned char)dot[3]))
    {
        *dot = '\0';
    }

    /* Now check FIDO day base + optional volume digit/char */
    /* Extract first 3 chars after '.' */
    if (lext[0] != '.')
        return 0;

    day[0] = lext[1];
    day[1] = lext[2];
    day[2] = '\0';

    /* Must be letters (mo tu we etc) */
    for (i = 0; i < 2; i++)
    {
        if (!isalpha((unsigned char)day[i]))
            return 0;
    }

    /* Accept .mo .mo0 .moA etc */
    if (strcmp(lext, ".mo") == 0 ||
        strcmp(lext, ".tu") == 0 ||
        strcmp(lext, ".we") == 0 ||
        strcmp(lext, ".th") == 0 ||
        strcmp(lext, ".fr") == 0 ||
        strcmp(lext, ".sa") == 0 ||
        strcmp(lext, ".su") == 0)
    {
        return 1;
    }

    /* FIDO extended: .mo0 .mo1 .thA etc */
    if ((strlen(lext) == 4) &&
        lext[0] == '.' &&
        isalpha((unsigned char)lext[1]) &&
        isalpha((unsigned char)lext[2]) &&
        isalnum((unsigned char)lext[3]))
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