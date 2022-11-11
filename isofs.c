/*
 * isofs.c
 * $Id: isofs.c,v 1.11 2006/09/01 17:23:22 bobi Exp $
 *
 * Copyright 2004 Bobi B., w1zard0f07@yahoo.com
 *
 * This file is part of hdl_dump.
 *
 * hdl_dump is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * hdl_dump is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with hdl_dump; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <ctype.h>
#include <string.h>
#include "isofs.h"
#include "retcodes.h"
#include "common.h"


#define CDVD_SECT_SIZE 2048


/**************************************************************/
/*
 * pvd == "Primary Volume Descriptor"
 * ptr == "Path Table Record"
 */
static int
isofs_find_pvd_addr(iin_t *iin,
                    /*@out@*/ u_int64_t *pvd_start_addr,
                    /*@out@*/ u_int64_t *ptr_start_addr,
                    /*@out@*/ char system_id[32 + 1],
                    /*@out@*/ char volume_id[32 + 1],
                    int layer)
{
    u_int32_t start_sector = 16;
    const char *data;
    u_int32_t len;
    const unsigned char *buffer;
    int result = iin->read(iin, start_sector, 1, &data, &len);
    buffer = (const unsigned char *)data;

    *pvd_start_addr = *ptr_start_addr = 0;
    *system_id = *volume_id = '\0';

    if (result == OSAL_OK)
        result = memcmp(buffer + 1, "CD001", 5) == 0 ? OSAL_OK : RET_BAD_ISOFS;
    if (layer == 1) {
        start_sector = ((u_int32_t)buffer[83] << 24 |
                        (u_int32_t)buffer[82] << 16 |
                        (u_int32_t)buffer[81] << 8 |
                        (u_int32_t)buffer[80] << 0);
        result = iin->read(iin, start_sector, 1, &data, &len);
        if (len != 2048)
            result = RET_BAD_ISOFS;
        if (result == OSAL_OK) {
            buffer = (const unsigned char *)data;
            result = (memcmp(buffer + 1, "CD001", 5) == 0 ?
                          OSAL_OK :
                          RET_BAD_ISOFS);
        }
    }

    if (result == OSAL_OK) {
        if (buffer[0] == 0x01) /* primary volume descriptor set */
        {
            *pvd_start_addr = start_sector * IIN_SECTOR_SIZE;
            *ptr_start_addr = (u_int64_t)(buffer[143] << 24 |
                                          buffer[142] << 16 |
                                          buffer[141] << 8 |
                                          buffer[140] << 0) *
                              (u_int64_t)CDVD_SECT_SIZE;
            if (system_id != NULL) {
                memcpy(system_id, buffer + 8, 32);
                system_id[32] = '\0';
                (void)rtrim(system_id);
            }
            if (volume_id != NULL) {
                memcpy(volume_id, buffer + 40, 32);
                volume_id[32] = '\0';
                (void)rtrim(volume_id);
            }
        } else
            result = RET_BAD_ISOFS; /* primary volume descriptor set not found */
    }
    return (result);
}


/**************************************************************/
static int
isofs_get_root_addr(iin_t *iin,
                    u_int64_t ptr_start_addr,
                    /*@out@*/ u_int64_t *root_start_addr)
{
    u_int32_t len;
    const char *data;
    int result = iin->read(iin, (u_int32_t)(ptr_start_addr / IIN_SECTOR_SIZE),
                           1, &data, &len);
    const unsigned char *buffer = (const unsigned char *)data;
    *root_start_addr = 0;
    if (result == OSAL_OK) {
        int found = 0;
        u_int32_t dir_start_addr;
        const unsigned char *p = buffer;
        do {
            int id_len;
            u_int16_t parent_dir;

            id_len = *p++;
            if (id_len == 0 ||
                p > buffer + 2047)
                break; /* last entry */
            p++;
            dir_start_addr = ((u_int32_t)p[3] << 24 |
                              (u_int32_t)p[2] << 16 |
                              (u_int32_t)p[1] << 8 |
                              (u_int32_t)p[0] << 0);
            p += 4;
            parent_dir = (u_int16_t)p[1] << 8 | (u_int16_t)p[0];
            p += 2;
            if (*p == '\0') { /* root dir? */
                found = 1;
                break;
            } else
                p += id_len + (id_len % 2); /* skip to next entry */
        } while (1);

        if (found)
            *root_start_addr = (u_int64_t)dir_start_addr * CDVD_SECT_SIZE;
        else
            result = RET_BAD_ISOFS; /* root dir not found */
    }
    return (result);
}


/**************************************************************/
static int
isofs_get_file_addr(iin_t *iin,
                    u_int64_t dir_start_addr,
                    const char *file_name,
                    /*@out@*/ u_int64_t *file_start_addr,
                    /*@out@*/ u_int64_t *file_length)
{
    u_int32_t file_name_len = strlen(file_name);
    u_int32_t len;
    const char *data;
    int result = iin->read(iin, (u_int32_t)(dir_start_addr / IIN_SECTOR_SIZE),
                           1, &data, &len);
    const unsigned char *buffer = (const unsigned char *)data;
    *file_start_addr = *file_length = 0;
    if (result == OSAL_OK) {
        int found = 0;
        u_int32_t start_addr, length;
        const unsigned char *p = buffer;
        do {
            int len_dr;
            u_int32_t name_len;
            const unsigned char *start = p;
            len_dr = *p++;
            if (len_dr == 0 ||
                p > buffer + 2047)
                break; /* last entry */
            p++;

            start_addr = ((u_int32_t)p[3] << 24 |
                          (u_int32_t)p[2] << 16 |
                          (u_int32_t)p[1] << 8 |
                          (u_int32_t)p[0] << 0);
            p += 8;
            length = ((u_int32_t)p[3] << 24 |
                      (u_int32_t)p[2] << 16 |
                      (u_int32_t)p[1] << 8 |
                      (u_int32_t)p[0] << 0);
            p += 8;
            p += 7; /* recording date/time */
            ++p;
            ++p;
            ++p;
            p += 4; /* flags, unit size, intereave gap size, volume seq no */
            name_len = *p++;
            if ((name_len == file_name_len) && (memcmp(p, file_name, file_name_len) == 0)) { /* found */
                found = 1;
                break;
            } else
                p = start + len_dr; /* continue to the next file */
        } while (1);

        if (found) {
            *file_start_addr = (u_int64_t)start_addr * CDVD_SECT_SIZE;
            *file_length = length;
        } else
            result = RET_NOT_FOUND;
    }
    return (result);
}


/**************************************************************/
static int
parse_config_cnf(const char *contents,
                 u_int32_t length,
                 /*@out@*/ char signature[12 + 1])
{
    /* when using memory-mapped I/O buffer is read-only */
    char buf[131072];
    int found = 0;
    char *line = buf;
    const char *end = buf + length;
    *signature = '\0';
    if (length <= sizeof(buf))
        memcpy(buf, contents, length);
    else
        return (RET_NOT_PS_CDVD);
    do {
        char *p = line, *start = line;
        while (*p != '\r' && *p != '\n' && p < end && *p != '\0')
            ++p;
        *p = '\0';
        line = p + 1;

        /* expected format: "BOOT2 = cdrom0:\SCES_xxx.xx;1\r\n" */
        p = start;
        if (memcmp(p, "BOOT2", 5) == 0) {
            p += 5;
            while (*p == ' ' || *p == '\t' || *p == '=')
                ++p;
            if ((memcmp(p, "cdrom0:\\", 8) == 0) || (memcmp(p, "cdrom1:\\", 8) == 0)) { /*cdrom1 is used by some game mods*/
                p += 8;
                while (*p != ';')
                    *signature++ = *p++;
                *signature = '\0';
                found = 1;
                break;
            }
        }
    } while (line < end);
    return (found ? OSAL_OK : RET_NOT_PS_CDVD /* perhaps PSOne CD-ROM */);
}


/**************************************************************/
static int
isofs_parse_config_cnf(iin_t *iin,
                       u_int64_t file_start_addr,
                       u_int64_t file_length,
                       /*@out@*/ char signature[12 + 1])
{
    u_int32_t len;
    const char *data;
    int result = iin->read(iin, (u_int32_t)(file_start_addr / IIN_SECTOR_SIZE),
                           1, &data, &len);
    *signature = '\0';
    if (result == OSAL_OK)
        result = parse_config_cnf(data, (u_int32_t)file_length, signature);
    return (result);
}


/**************************************************************/
static int
isofs_detect_media_type(iin_t *iin,
                        /*@out@*/ ps2_cdvd_info_t *info)
{
    const char *buf = NULL;
    u_int32_t len = 0;
    int result = iin->read(iin, 16, 1, &buf, &len);
    if (result == OSAL_OK && len == CDVD_SECT_SIZE) {
        if (memcmp(buf + 1024, "CD-XA001", 8) == 0)
            info->media_type = mt_cd;
        else if (memcmp(buf + 1024, "\0\0\0\0\0\0\0\0", 8) == 0)
            info->media_type = mt_dvd;
        else
            info->media_type = mt_unknown;
    }
    return (result);
}


/**************************************************************/
int isofs_get_ps2_cdvd_info(iin_t *iin,
                            ps2_cdvd_info_t *info)
{
    u_int64_t pvd_start_addr, ptr_start_addr, root_start_addr;
    u_int64_t system_cnf_start_addr, system_cnf_length;
    char system_id[32 + 1];
    int result;

    result = isofs_detect_media_type(iin, info);
    if (result == OSAL_OK)
        result = isofs_find_pvd_addr(iin, &pvd_start_addr, &ptr_start_addr,
                                     system_id, info->volume_id, 0);

    if (result == OSAL_OK)
        if (strcmp(system_id, "PLAYSTATION") != 0)
            result = RET_NOT_PS_CDVD;

    if (result == OSAL_OK)
        result = isofs_get_root_addr(iin, ptr_start_addr, &root_start_addr);

    if (result == OSAL_OK)
        result = isofs_get_file_addr(iin, root_start_addr, "SYSTEM.CNF;1",
                                     &system_cnf_start_addr, &system_cnf_length);

    if (result == OSAL_OK)
        result = isofs_parse_config_cnf(iin, system_cnf_start_addr,
                                        system_cnf_length, info->startup_elf);

    if (result == OSAL_OK) {
        char tmp_volume_id[32 + 1];
        if (isofs_find_pvd_addr(iin, &pvd_start_addr, &ptr_start_addr,
                                system_id, tmp_volume_id, 1) == OSAL_OK)
            info->layer_pvd = pvd_start_addr / CDVD_SECT_SIZE;
        else
            info->layer_pvd = 0;
    }

    return (result);
}
