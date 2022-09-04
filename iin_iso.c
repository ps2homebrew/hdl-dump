/*
 * iin_iso.c
 * $Id: iin_iso.c,v 1.11 2006/09/01 17:24:59 bobi Exp $
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
#include "iin_iso.h"
#include "iin_img_base.h"
#include "osal.h"
#include "retcodes.h"


/**************************************************************/
int iin_iso_probe_path(const char *path,
                       iin_t **iin)
{
    osal_handle_t file;
    u_int32_t size_in_sectors, volume_sector_size;
    int result = osal_open(path, &file, 0); /* open with caching enabled */
    if (result == OSAL_OK) {
        u_int32_t bytes;
        unsigned char buffer[6];
        result = osal_read(file, buffer, sizeof(buffer), &bytes); /* probe zso */
        if (result == OSAL_OK)
            if (memcmp(buffer, "ZISO", 4) == 0)
                ; /* success */
            else {
                /* at offset 0x00008000 there should be "\x01CD001" */
                result = osal_seek(file, (u_int64_t)0x00008000);
                if (result == OSAL_OK) {
                    result = osal_read(file, buffer, sizeof(buffer), &bytes);
                    if (result == OSAL_OK) {
                        if (bytes == 6 &&
                            memcmp(buffer, "\001CD001", 6) == 0)
                            ; /* success */
                        else
                            result = OSAL_OK;
                    }
                }
            }

        size_in_sectors = 0;
        if (result == OSAL_OK) {
            u_int64_t size_in_bytes;
            result = osal_get_file_size(file, &size_in_bytes);
            if (result == OSAL_OK)
                size_in_sectors = (u_int32_t)(size_in_bytes / 2048);
        }

        if (result == OSAL_OK)
            result = osal_get_volume_sect_size(path, &volume_sector_size);

        osal_close(&file);
    }

    if (result == OSAL_OK) {
        iin_img_base_t *img_base = img_base_alloc(2048, 0);
        if (img_base != NULL) {
            result = img_base_add_part(img_base, path, size_in_sectors, 0, volume_sector_size);
            if (result == OSAL_OK) {
                *iin = (iin_t *)img_base;
                strcpy((*iin)->source_type, "Plain ISO/ZSO file");
            } else
                ((iin_t *)img_base)->close((iin_t *)img_base);
        } else
            result = RET_NO_MEM;
    }
    return (result);
}
