#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <fileio.h>

#include "ipconfig.h"

static char *GetNextToken(char *line, char delimiter)
{
    char *field_end = '\0', *result;
    static char *current_line = NULL;

    if (line != NULL) {
        current_line = line;
    }

    while (*current_line == delimiter)
        current_line++;
    if (current_line[0] != '\0') {
        if ((field_end = strchr(current_line, delimiter)) == NULL) {
            field_end = &current_line[strlen(current_line)];
        }

        *field_end = '\0';
    }

    if (current_line[0] != '\0' && current_line[1] != '\0') {
        result = current_line;
        current_line = field_end + 1;
    } else {
        result = NULL;
        current_line = NULL;
    }

    return result;
}

int ParseNetAddr(const char *address, unsigned char *octlets)
{
    int result;
    char address_copy[16], *octlet;
    unsigned char i;

    if (strlen(address) < 16) {
        strcpy(address_copy, address);

        if ((octlet = strtok(address_copy, ".")) != NULL) {
            result = 0;

            octlets[0] = strtoul(octlet, NULL, 10);
            for (i = 1; i < 4; i++) {
                if ((octlet = strtok(NULL, ".")) == NULL) {
                    result = EINVAL;
                    break;
                } else
                    octlets[i] = strtoul(octlet, NULL, 10);
            }
        } else
            result = EINVAL;
    } else
        result = EINVAL;


    return result;
}

int ParseConfig(const char *path, char *ip_address, char *subnet_mask, char *gateway)
{
    int fd, result, size;
    char *FileBuffer, *line, *field;
    unsigned int i;

    if ((fd = fioOpen(path, O_RDONLY)) >= 0) {
        size = fioLseek(fd, 0, SEEK_END);
        fioLseek(fd, 0, SEEK_SET);
        if ((FileBuffer = malloc(size)) != NULL) {
            if (fioRead(fd, FileBuffer, size) == size) {
                if ((line = strtok(FileBuffer, "\r\n")) != NULL) {
                    result = EINVAL;
                    do {
                        i = 0;
                        while (line[i] == ' ')
                            i++;
                        if (line[i] != '#' && line[i] != '\0') {
                            if ((field = GetNextToken(line, ' ')) != NULL) {
                                strncpy(ip_address, field, 15);
                                ip_address[15] = '\0';
                                if ((field = GetNextToken(NULL, ' ')) != NULL) {
                                    strncpy(subnet_mask, field, 15);
                                    subnet_mask[15] = '\0';
                                    if ((field = GetNextToken(NULL, ' ')) != NULL) {
                                        strncpy(gateway, field, 15);
                                        gateway[15] = '\0';
                                        result = 0;
                                        break;
                                    }
                                }
                            }
                        }
                    } while ((line = strtok(NULL, "\r\n")) != NULL);
                } else
                    result = EINVAL;
            } else
                result = EIO;

            free(FileBuffer);
        } else
            result = ENOMEM;

        fioClose(fd);
    } else
        result = fd;

    return result;
}
