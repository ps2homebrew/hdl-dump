/*
 * hio_dbg.c - debug dumps access
 * $Id: hio_dbg.c,v 1.2 2006/09/01 17:27:32 bobi Exp $
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

#include "hio_dbg.h"
#include "net_io.h"
#include "osal.h"
#include "retcodes.h"
#include <stdio.h>
#include <string.h>


typedef struct hio_dbg_type
{
    hio_t hio;
    /*  FILE *in;  read-only */
    FILE *out;      /* a clone, write-only */
    u_int32_t size; /* in KB */
    unsigned long error_code;
} hio_dbg_t;


/**************************************************************/
/*static int
clone_file (const char *fn,
            char *copy_name)
{
    FILE *in;
    int result = RET_ERR;

    strcpy (copy_name, fn);
    strcat (copy_name, ".bak");
    in = fopen (fn, "rb");
    if (in != NULL)
    {
        FILE *out = fopen (copy_name, "wb");
        if (out != NULL)
        {
            char buf[1024];
            size_t bytes;
            while ((bytes = fread (buf, 1, sizeof (buf), in)) > 0)
            (void) fwrite (buf, 1, bytes, out);
            result = RET_OK;
            fclose (out);
        }
        fclose (in);
    }
    return (result);
}
*/

/**************************************************************/
static int
dbg_stat(hio_t *hio,
         /*@out@*/ u_int32_t *size_in_kb)
{
    hio_dbg_t *dbg = (hio_dbg_t *)hio;
    *size_in_kb = dbg->size;
    return (RET_OK);
}


/**************************************************************/
static int /* returns non-zero on success */
remap_sector(const hio_dbg_t *dbg,
             u_int32_t start_sector,
             u_int32_t num_sectors,
             /*@out@*/ u_int32_t *offset,
             /*@out@*/ u_int32_t *len)
{
    static const u_int32_t HDL_HEADER_OFFS_IN_SECT =
        0x00101000 / HDD_SECTOR_SIZE;
    static const u_int32_t IN_SECT_128MB = (128 * 2048);

    if ((start_sector + num_sectors) / 2 > dbg->size)
        return (0);                                 /* behind eof */
    else if ((start_sector % IN_SECT_128MB) == 0) { /* start sector multiplies on APA header */
        *offset = (start_sector / IN_SECT_128MB) * 2048;
        *len = (num_sectors <= 2 ? num_sectors : 2) * HDD_SECTOR_SIZE;
        return (1);
    } else if (start_sector > HDL_HEADER_OFFS_IN_SECT &&
               !((start_sector - HDL_HEADER_OFFS_IN_SECT) % IN_SECT_128MB)) { /* start sector multiplies on HD Loader game header */
        *offset = ((start_sector - HDL_HEADER_OFFS_IN_SECT) /
                   IN_SECT_128MB) *
                      2048 +
                  1024;
        *len = (num_sectors <= 2 ? num_sectors : 2) * HDD_SECTOR_SIZE;
        return (1);
    }
    return (0);
}


/**************************************************************/
static int
dbg_read(hio_t *hio,
         u_int32_t start_sector,
         u_int32_t num_sectors,
         /*@out@*/ void *output,
         /*@out@*/ u_int32_t *bytes)
{
    hio_dbg_t *dbg = (hio_dbg_t *)hio;
    u_int32_t offset = 0, total_bytes = 0;
    int result = RET_OK;

    /* "read" zeroes by default */
    memset(output, 0, HDD_SECTOR_SIZE * num_sectors);

    if (remap_sector(dbg, start_sector, num_sectors, &offset, &total_bytes) &&
        total_bytes > 0 &&
        fseek(dbg->out, offset, SEEK_SET) == 0) {
        *bytes = fread(output, 1, total_bytes, dbg->out);
        result = (*bytes == total_bytes ? RET_OK : RET_ERR);
    }

    return (result);
}


/**************************************************************/
static int
dbg_write(hio_t *hio,
          u_int32_t start_sector,
          u_int32_t num_sectors,
          const void *input,
          /*@out@*/ u_int32_t *bytes)
{
    hio_dbg_t *dbg = (hio_dbg_t *)hio;
    u_int32_t offset = 0, total_bytes = 0;
    int result = RET_OK;

    if (remap_sector(dbg, start_sector, num_sectors, &offset, &total_bytes) &&
        total_bytes > 0 &&
        fseek(dbg->out, offset, SEEK_SET) == 0) {
        *bytes = fwrite(input, 1, total_bytes, dbg->out);
        if (*bytes == total_bytes)
            *bytes = num_sectors * 512;
        else
            result = RET_ERR;
    } else
        *bytes = num_sectors * 512;
    return (result);
}


/**************************************************************/
static int
dbg_poweroff(hio_t *hio)
{
    return (RET_OK);
}


/**************************************************************/
static int
dbg_flush(hio_t *hio)
{
    return (RET_OK);
}


/**************************************************************/
static int
dbg_close(/*@special@*/ /*@only@*/ hio_t *hio) /*@releases hio@*/
{
    hio_dbg_t *dbg = (hio_dbg_t *)hio;
    fclose(dbg->out);
    osal_free(dbg);
    return (RET_OK);
}


/**************************************************************/
static char *
dbg_last_error(hio_t *hio)
{
    hio_dbg_t *dbg = (hio_dbg_t *)hio;
    return (osal_get_error_msg(dbg->error_code));
}


/**************************************************************/
static void
dbg_dispose_error(hio_t *hio,
                  /*@only@*/ char *error)
{
    osal_dispose_error_msg(error);
}


/**************************************************************/
static hio_t *
dbg_alloc(const char *dump_path)
{
    /*  char copy[MAX_PATH];*/
    hio_dbg_t *dbg;

    /*  if (clone_file (dump_path, copy) != RET_OK)
    return (NULL);*/

    dbg = (hio_dbg_t *)osal_alloc(sizeof(hio_dbg_t));
    if (dbg != NULL) {
        int bail_out = 1;
        memset(dbg, 0, sizeof(hio_dbg_t));
        dbg->hio.stat = &dbg_stat;
        dbg->hio.read = &dbg_read;
        dbg->hio.write = &dbg_write;
        dbg->hio.flush = &dbg_flush;
        dbg->hio.close = &dbg_close;
        dbg->hio.poweroff = &dbg_poweroff;
        dbg->hio.last_error = &dbg_last_error;
        dbg->hio.dispose_error = &dbg_dispose_error;
        dbg->error_code = 0;

        dbg->out = fopen(dump_path, "r+b");
        if (dbg->out != NULL) {
            /*  dbg->out = fopen (dump_path, "r+b");*/
            if (fseek(dbg->out, 0, SEEK_END) == 0) {
                long size = ftell(dbg->out);
                if (size != -1) { /* debug dump includes 2KB for each 128MB */
                                  /* dbg->size = (size / 2048) * 128 * 1024;*/
                    dbg->size = 1 + size / 1024;
                    bail_out = 0; /* success */
                }
            }
        }

        if (bail_out)
            dbg = NULL;
    }
    return ((hio_t *)dbg);
}


/**************************************************************/
int hio_dbg_probe(const dict_t *config,
                  const char *path,
                  hio_t **hio)
{
    static const char *MONIKER = "dbg:";
    int result = RET_NOT_COMPAT;
    if (memcmp(path, MONIKER, strlen(MONIKER)) == 0) {
        *hio = dbg_alloc(path + strlen(MONIKER));
        if (*hio != NULL)
            result = RET_OK;
    }
    return (result);
}
