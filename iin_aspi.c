/*
 * iin_aspi.c
 * $Id: iin_aspi.c,v 1.9 2006/06/18 13:11:19 bobi Exp $
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

#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include "wnaspi32.h"
#include "iin_aspi.h"
#include "osal.h"
#include "retcodes.h"
#include "iin.h"
#include "aspi_hlio.h"


/* that is for a preliminary check only; device is probed anyway */
#define MAX_HOSTS 15
#define MAX_SCSI_ID 15
#define MAX_LUN 7


typedef struct iin_aspi_type
{
    iin_t iin;
    int host, scsi_id, lun;
    u_int32_t size_in_sectors, sector_size; /* cached; can be obtained always, by calling aspi_stat */
    char *unaligned, *buffer;
    unsigned long error_code; /* against aspi_... */
} iin_aspi_t;


/**************************************************************/
static int
aspicd_stat(iin_t *iin,
            u_int32_t *sector_size,
            u_int32_t *size_in_sectors)
{
    const iin_aspi_t *aspi = (const iin_aspi_t *)iin;
    *sector_size = aspi->sector_size;
    *size_in_sectors = aspi->size_in_sectors;
    return (RET_OK);
}


/**************************************************************/
static int
aspicd_read(iin_t *iin,
            u_int32_t start_sector,
            u_int32_t num_sectors,
            const char **data,
            u_int32_t *length)
{
    iin_aspi_t *aspi = (iin_aspi_t *)iin;
    int result;

    /* TODO: divide requests on chunks, of up to 16KB data each */
    result = aspi_read_10(aspi->host, aspi->scsi_id, aspi->lun,
                          start_sector, (num_sectors * IIN_SECTOR_SIZE + aspi->sector_size - 1) / aspi->sector_size,
                          aspi->buffer);
    if (result == RET_OK) {
        *data = aspi->buffer;
        *length = num_sectors * IIN_SECTOR_SIZE;
    } else {
        if (result == RET_ERR)
            result = RET_ASPI_ERROR;
        aspi->error_code = aspi_get_last_error_code();
    }
    return (result);
}


/**************************************************************/
static int
aspicd_close(iin_t *iin)
{
    iin_aspi_t *aspi = (iin_aspi_t *)iin;
    osal_free(aspi->unaligned);
    osal_free(aspi);
    aspi_unload();
    return (RET_OK);
}


/**************************************************************/
static char *
aspicd_last_error(iin_t *iin)
{
    iin_aspi_t *aspi = (iin_aspi_t *)iin;
    return ((char *)aspi_get_error_msg(aspi->error_code));
}


/**************************************************************/
static void
aspicd_dispose_error(iin_t *iin,
                     char *error)
{
    aspi_dispose_error_msg(error);
}


/**************************************************************/
static iin_t *
aspicd_alloc(int host, int scsi_id, int lun,
             u_int32_t size_in_sectors,
             u_int32_t sector_size,
             u_int32_t reqd_alignment)
{
    iin_aspi_t *aspi;

    /* make data buffer compatible with unbuffered file I/O (should be aligned @ sector size) */
    if (reqd_alignment < 512)
        reqd_alignment = 512;

    aspi = (iin_aspi_t *)osal_alloc(sizeof(iin_aspi_t));
    if (aspi != NULL) {
        char *tmp = osal_alloc(IIN_NUM_SECTORS * IIN_SECTOR_SIZE + reqd_alignment);
        if (tmp != NULL) {
            memset(aspi, 0, sizeof(iin_aspi_t));
            aspi->iin.stat = &aspicd_stat;
            aspi->iin.read = &aspicd_read;
            aspi->iin.close = &aspicd_close;
            aspi->iin.last_error = &aspicd_last_error;
            aspi->iin.dispose_error = &aspicd_dispose_error;
            strcpy(aspi->iin.source_type, "Optical drive via ASPI");
            aspi->host = host;
            aspi->scsi_id = scsi_id;
            aspi->lun = lun;
            aspi->size_in_sectors = size_in_sectors;
            aspi->sector_size = sector_size;
            aspi->unaligned = tmp;
            aspi->buffer = (void *)(((unsigned long)tmp + reqd_alignment - 1) &
                                    ~((unsigned long)reqd_alignment - 1));
        } else { /* unable to allocate read buffer */
            osal_free(aspi);
            aspi = NULL;
        }
    }
    return ((iin_t *)aspi);
}


/**************************************************************/
int iin_aspi_probe_path(const char *path,
                        iin_t **iin)
{
    int result = RET_NOT_COMPAT;
    int host, scsi_id, lun;
    int aspi_init = 0;

#if 0
  if (!dict_get_flag (config, CONFIG_ENABLE_ASPI_FLAG, 0))
    return (result);
#endif

    /* expected pattern is "cd0:2:0" */
    if (tolower(path[0]) == 'c' &&
        tolower(path[1]) == 'd') {
        char *endp;
        host = strtol(path + 2, &endp, 10);
        if (host >= 0 && host <= MAX_HOSTS && *endp == ':') {
            scsi_id = strtol(endp + 1, &endp, 10);
            if (scsi_id >= 0 && scsi_id <= MAX_SCSI_ID && *endp == ':') {
                lun = strtol(endp + 1, &endp, 10);
                if (lun >= 0 && lun <= MAX_LUN && *endp == '\0')
                    result = RET_OK;
            }
        }
    }

    if (result == RET_OK) {
        result = aspi_load();
        aspi_init = result == RET_OK;
    }

    if (result == RET_OK) { /* pattern matched */
        u_int32_t size_in_sectors, sector_size;
        result = aspi_stat(host, scsi_id, lun, &sector_size, &size_in_sectors);
        if (result == RET_OK) { /* TODO: inquire SCSI host/device to ask the required alignment */
            *iin = aspicd_alloc(host, scsi_id, lun,
                                size_in_sectors, sector_size,
                                512);
            if (iin != NULL)
                ;
            else
                result = RET_NO_MEM;
        } else if (result == RET_ERR)
            result = RET_ASPI_ERROR;
    }

    if (result != RET_OK && aspi_init)
        aspi_unload();

    return (result);
}
