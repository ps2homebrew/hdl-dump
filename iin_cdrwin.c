/*
 * iin_cdrwin.c
 * $Id: iin_cdrwin.c,v 1.12 2006/09/01 17:26:06 bobi Exp $
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
#include <stdlib.h>
#include <string.h>
#if defined(_DEBUG)
#include <stdio.h>
#endif
#include "iin_cdrwin.h"
#include "iin_img_base.h"
#include "osal.h"
#include "retcodes.h"
#include "common.h"


typedef enum cdrwin_data_mode_type {
    cdm_mode1_plain = 0,
    cdm_mode1_raw = 1,
    cdm_mode2_plain = 2,
    cdm_mode2_raw = 3
} cdrwin_data_mode_t;

/* values for CDRWIN found out by building test images and by looking at
   http://www.disctronics.co.uk/technology/cd-rom/cdrom_spec.htm */
static const u_int32_t RAW_SECTOR_SIZE[4] = {2048, 2352, 2336, 2352};
static const u_int32_t RAW_SKIP_OFFSET[4] = {0, 16, 8, 24};


/**************************************************************/
#define is_space_or_tab(ch) ((ch) == ' ' || (ch) == '\t')

static int /* would also check whether file exists or not */
cue_parse_file_line(const char *cuesheet_path,
                    char *line,
                    char source[MAX_PATH])
{
    /* FILE xxx BINARY or FILE "xx xx" BINARY accepted */
    int result;
    char *p = line;
    while (is_space_or_tab(*p))
        ++p;
    line = p;
    while (!is_space_or_tab(*p) && *p != '\0')
        ++p;
    *p++ = '\0';
    result = caseless_compare(line, "file") ? RET_OK : RET_NOT_COMPAT;

    if (result == RET_OK) {
        char *dest = source, *dest_end = source + MAX_PATH - 1;
        while (is_space_or_tab(*p))
            ++p;
        if (*p == '\"') { /* "FILE NAME" */
            ++p;          /* skip initial " */
            while (*p != '\"' && *p != '\0' && dest < dest_end)
                *dest++ = *p++;
            if (*p++ != '\"')
                result = RET_NOT_COMPAT;
        } else { /* FILE_NAME */
            while (!is_space_or_tab(*p) && *p != '\0' && dest < dest_end)
                *dest++ = *p++;
        }
        *dest = '\0'; /* zero-terminate destination */
        if (result == OSAL_OK) {
            while (is_space_or_tab(*p))
                ++p;
            result = caseless_compare(p, "binary") ? RET_OK : RET_NOT_COMPAT;
        }
    }

    if (result == RET_OK) { /* check whether file exists */
        if (file_exists(source))
            ;  /* ok */
        else { /* check for the source in the same folder as the cue file */
            result = lookup_file(source, cuesheet_path);
            if (result == RET_OK)
                ; /* linked file found */
            else if (result == RET_FILE_NOT_FOUND)
                result = RET_BROKEN_LINK;
        }
    }

    return (result);
}


/**************************************************************/
static int
cue_parse_track_line(char *line,
                     /*@out@*/ cdrwin_data_mode_t *mode)
{
    int result;
    char *p = line;
    while (is_space_or_tab(*p))
        ++p;
    line = p;
    while (!is_space_or_tab(*p) && *p != '\0')
        ++p;
    *p++ = '\0';
    result = caseless_compare(line, "track") ? RET_OK : RET_NOT_COMPAT;

    if (result == RET_OK) { /* TRACK 01 MODE?/???? accepted */
        char *track_no, *text_mode;

        while (is_space_or_tab(*p))
            ++p;
        track_no = p;
        while (!is_space_or_tab(*p) && *p != '\0')
            ++p;
        *p++ = '\0';

        while (is_space_or_tab(*p))
            ++p;
        text_mode = p;

        result = atoi(track_no) == 1 ? RET_OK : RET_BAD_COMPAT;
        if (result == RET_OK)
            result = (tolower(text_mode[0]) == 'm' &&
                      tolower(text_mode[1]) == 'o' &&
                      tolower(text_mode[2]) == 'd' &&
                      tolower(text_mode[3]) == 'e') ?
                         RET_OK :
                         RET_BAD_COMPAT;
        if (result == RET_OK) {
            int sector_size = atoi(text_mode + 6);
            if (text_mode[4] == '1')
                /* mode 1 */
                switch (sector_size) {
                    case 2048:
                        *mode = cdm_mode1_plain;
                        break;
                    case 2352:
                        *mode = cdm_mode1_raw;
                        break;
                    default:
                        result = RET_BAD_COMPAT;
                }
            else
                /* mode 2 */
                switch (sector_size) {
                    case 2336:
                        *mode = cdm_mode2_plain;
                        break;
                    case 2352:
                        *mode = cdm_mode2_raw;
                        break;
                    default:
                        result = RET_BAD_COMPAT;
                }
        }
    }

    return (result);
}


/**************************************************************/
static int
cue_parse_index_line(char *line)
{
    int result;
    char *p = line;
    while (is_space_or_tab(*p))
        ++p;
    line = p;
    while (!is_space_or_tab(*p) && *p != '\0')
        ++p;
    *p++ = '\0';
    result = caseless_compare(line, "index") ? RET_OK : RET_NOT_COMPAT;

    return (result);
}


/**************************************************************/
static int
cue_parse(const char *path,
          char source[MAX_PATH],
          cdrwin_data_mode_t *mode)
{
    osal_handle_t in;
    u_int64_t file_size = 0;
    int result = osal_open(path, &in, 0);
    if (result == OSAL_OK) { /* get file size */
        result = osal_get_file_size(in, &file_size);
        osal_close(&in);
    }
    if (result == OSAL_OK)
        result = file_size < 1024 ? OSAL_OK : RET_NOT_COMPAT; /* a cuesheet up to 1KB is accepted */

    if (result == OSAL_OK) {
        char *contents;
        u_int32_t length;
        result = read_file(path, &contents, &length);
        if (result == OSAL_OK) {
            char *line = strtok(contents, "\r\n");
            if (line != NULL && *line != '\0')
                /* extract data file path and check whether it exists or not */
                result = cue_parse_file_line(path, line, source);
            else
                result = RET_NOT_COMPAT;

            if (result == OSAL_OK) { /* get mode and bytes per sector */
                line = strtok(NULL, "\r\n");
                if (line != NULL && *line != '\0')
                    result = cue_parse_track_line(line, mode);
                else
                    result = RET_BAD_COMPAT;
            }

            if (result == OSAL_OK) { /* validate that 3rd line is an index line */
                line = strtok(NULL, "\r\n");
                if (line != NULL && *line != '\0')
                    result = cue_parse_index_line(line);
                else
                    result = RET_BAD_COMPAT;
            }

            if (result == OSAL_OK) { /* validate that 5th line is an index line or empty */
                line = strtok(NULL, "\r\n");
                if (line != NULL && *line != '\0') {
                    line = strtok(NULL, "\r\n");
                    if (line != NULL && *line != '\0') {
                        result = cue_parse_index_line(line);
                        if (result != RET_OK)
                            result = RET_MULTITRACK;
                    } else
                        result = RET_OK;
                } else
                    result = RET_OK;
            }

            osal_free(contents);
        }
    }

    return (result);
}


/**************************************************************/
int iin_cdrwin_probe_path(const char *path,
                          iin_t **iin)
{
    char source[MAX_PATH];
    cdrwin_data_mode_t mode = cdm_mode1_plain;

    int result = cue_parse(path, source, &mode);
    if (result == RET_OK) {
        u_int64_t file_size;
        u_int32_t device_sector_size;
        result = osal_get_file_size_ex(source, &file_size);
        if (result == OSAL_OK)
            result = osal_get_volume_sect_size(source, &device_sector_size);
        if (result == OSAL_OK) {
            iin_img_base_t *img_base =
                img_base_alloc(RAW_SECTOR_SIZE[mode], RAW_SKIP_OFFSET[mode]);
            if (img_base != NULL)
                result = img_base_add_part(img_base, source,
                                           (u_int32_t)(file_size / RAW_SECTOR_SIZE[mode]),
                                           (u_int64_t)0, device_sector_size);
            else
                /* img_base_alloc failed */
                result = RET_NO_MEM;
            if (result == OSAL_OK) { /* success */
                *iin = (iin_t *)img_base;
                switch (mode) {
                    case cdm_mode1_plain:
                        strcpy((*iin)->source_type, "ISO Image, Mode 1, plain");
                        break;
                    case cdm_mode1_raw:
                        strcpy((*iin)->source_type, "BIN Image, Mode 1, RAW");
                        break;
                    case cdm_mode2_plain:
                        strcpy((*iin)->source_type, "BIN Image, Mode 2, plain");
                        break;
                    case cdm_mode2_raw:
                        strcpy((*iin)->source_type, "BIN Image, Mode 2, RAW");
                        break;
                }
            } else if (img_base != NULL)
                ((iin_t *)img_base)->close((iin_t *)img_base);
        }
    }
    return (result);
}
