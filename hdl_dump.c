/*
 * hdl_dump.c
 * $Id: hdl_dump.c,v 1.21 2007-05-12 20:15:58 bobi Exp $
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

#if defined(_BUILD_WIN32)
#include <windows.h>
#endif
#include <assert.h>
#include <ctype.h>
#include <signal.h>
#if defined(_BUILD_WIN32)
/* b0rken in cygwin's headers */
#undef SIGINT
#define SIGINT 2
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "byteseq.h"
#include "retcodes.h"
#include "osal.h"
#include "apa.h"
#include "common.h"
#include "progress.h"
#include "hdl.h"
#include "isofs.h"
#include "iin.h"
#if defined(_BUILD_WIN32)
#include "iin_spti.h"
#include "aspi_hlio.h"
#endif
#include "aligned.h"
#include "hio.h"
#include "dict.h"
#include "net_io.h"

#define UNBOLD       "\033[0m"
#define BOLD         "\033[1m"
#define WARNING_SIGN "/\033[4m!\033[0m\\"

/* command names */
#define CMD_QUERY "query"
#if defined(INCLUDE_DUMP_CMD)
#define CMD_DUMP "dump"
#endif
#if defined(INCLUDE_COMPARE_IIN_CMD)
#define CMD_COMPARE_IIN "compare_iin"
#endif
#define CMD_TOC     "toc"
#define CMD_HDL_TOC "hdl_toc"
#if defined(INCLUDE_MAP_CMD)
#define CMD_MAP "map"
#endif
#if defined(INCLUDE_HIDE_CMD)
#define CMD_HIDE "delete"
#endif
#if defined(INCLUDE_ZERO_CMD)
#define CMD_ZERO "zero"
#endif
#if defined(INCLUDE_CUTOUT_CMD)
#define CMD_CUTOUT "cutout"
#endif
#if defined(INCLUDE_INFO_CMD)
#define CMD_HDL_INFO "info"
#endif
#define CMD_HDL_EXTRACT    "extract"
#define CMD_HDL_INJECT_CD  "inject_cd"
#define CMD_HDL_INJECT_DVD "inject_dvd"
#define CMD_HDL_INSTALL    "install"
#define CMD_CDVD_INFO      "cdvd_info"
#define CMD_CDVD_INFO2     "cdvd_info2"
#define CMD_POWER_OFF      "poweroff"
#if defined(INCLUDE_INITIALIZE_CMD)
#define CMD_INITIALIZE "initialize"
#endif
#if defined(INCLUDE_DUMP_MBR_CMD)
#define CMD_DUMP_MBR "dump_mbr"
#endif
#if defined(INCLUDE_BACKUP_TOC_CMD)
#define CMD_BACKUP_TOC "backup_toc"
#endif
#if defined(INCLUDE_RESTORE_TOC_CMD)
#define CMD_RESTORE_TOC "restore_toc"
#endif
#if defined(INCLUDE_DIAG_CMD)
#define CMD_DIAG "diag"
#endif
#if defined(INCLUDE_MODIFY_CMD)
#define CMD_MODIFY "modify"
#endif
#if defined(INCLUDE_COPY_HDD_CMD)
#define CMD_COPY_HDD "copy_hdd"
#endif
#define CMD_MODIFY_HEADER "modify_header"

/**************************************************************/
static void
show_apa_slice(const apa_slice_t *slice)
{
    u_int32_t i;

    if (slice->parts == NULL)
        return;
    for (i = 0; i < slice->part_count; ++i) {
        const ps2_partition_header_t *part = &slice->parts[i].header;

        fprintf(stdout, "%06lx00%c%c %5luMB ",
                (unsigned long)(get_u32(&part->start) >> 8),
                slice->parts[i].existing != 0 ? '.' : '*',
                slice->parts[i].modified != 0 ? '*' : ':',
                (unsigned long)(get_u32(&part->length) / 2048));
        if (get_u32(&part->main) == 0)
            fprintf(stdout, "%4x [%-*s]\n",
                    (unsigned int)get_u16(&part->type),
                    PS2_PART_IDMAX, part->id);
        else
            fprintf(stdout, "      part # %2lu in %06lx00\n",
                    (unsigned long)(get_u32(&part->number)),
                    (unsigned long)(get_u32(&part->main) >> 8));
    }

    fprintf(stdout, "Total slice size: %uMB, used: %uMB, available: %uMB\n",
            (unsigned int)slice->size_in_mb,
            (unsigned int)(slice->allocated_chunks * 128),
            (unsigned int)(slice->free_chunks * 128));
}


/**************************************************************/
static void
show_apa_slice2(const apa_slice_t *slice)
{
    u_int32_t i;

    if (slice->parts == NULL)
        return;
    fprintf(stdout, "type   start     #parts size name\n");
    for (i = 0; i < slice->part_count; ++i) {
        const ps2_partition_header_t *part = &slice->parts[i].header;
        if (get_u32(&part->main) == 0) {
            u_int32_t j, count = get_u32(&part->nsub);
            u_int32_t tot_len = get_u32(&part->length);
            for (j = 0; j < count; ++j)
                tot_len += get_u32(&part->subs[j].length);
            fprintf(stdout, "0x%04x %06lx00%c%c %2lu %5luMB %-*s\n",
                    (unsigned int)get_u16(&part->type),
                    (unsigned long)get_u32(&part->start) >> 8,
                    slice->parts[i].existing != 0 ? '.' : '*',
                    slice->parts[i].modified != 0 ? '*' : ':',
                    (unsigned long)count + 1, /* main partition counts, too */
                    (unsigned long)tot_len / 2048,
                    PS2_PART_IDMAX, part->id);
        }
    }

    fprintf(stdout, "Total slice size: %uMB, used: %uMB, available: %uMB\n",
            (unsigned int)slice->size_in_mb,
            (unsigned int)(slice->allocated_chunks * 128),
            (unsigned int)(slice->free_chunks * 128));
}


/**************************************************************/
static void
show_apa_toc(const apa_toc_t *toc,
             int view)
{
    if (toc->got_2nd_slice != 0)
        fprintf(stdout, "Slice 1\n");
    if (view == 0)
        show_apa_slice(toc->slice + 0);
    else
        show_apa_slice2(toc->slice + 0);
    if (toc->got_2nd_slice != 0) {
        fprintf(stdout, "Slice 2 (starting sectors relative to 0x10000000)\n");
        if (view == 0)
            show_apa_slice(toc->slice + 1);
        else
            show_apa_slice2(toc->slice + 1);
    }
}


/**************************************************************/
#if defined(INCLUDE_MAP_CMD) || defined(INCLUDE_CUTOUT_CMD)
static void
show_slice_map(const apa_slice_t *slice)
{
    /* show device map */
    const char *map = slice->chunks_map;
    const u_int32_t GIGS_PER_ROW = 8;
    u_int32_t i, count = 0;

    for (i = 0; i < slice->total_chunks; ++i) {
        if (count == 0)
            fprintf(stdout, "%3uGB: ",
                    (unsigned int)((i / ((GIGS_PER_ROW * 1024) / 128)) *
                                   GIGS_PER_ROW));

        (void)fputc(map[i], stdout);
        if ((count & 0x07) == 0x07)
            (void)fputc(' ', stdout);

        if (++count == ((GIGS_PER_ROW * 1024) / 128)) /* 8G on each row */
        {
            (void)fputc('\n', stdout);
            count = 0;
        }
    }

    fprintf(stdout, "\nTotal slice size: %uMB, used: %uMB, available: %uMB\n",
            (unsigned int)slice->size_in_mb,
            (unsigned int)(slice->allocated_chunks * 128),
            (unsigned int)(slice->free_chunks * 128));
}

static void
show_apa_map(const apa_toc_t *toc)
{
    if (toc->got_2nd_slice)
        fprintf(stdout, "Slice 1\n");
    show_slice_map(toc->slice + 0);
    if (toc->got_2nd_slice) {
        fprintf(stdout, "Slice 2 (starting sectors relative to 0x10000000)\n");
        show_slice_map(toc->slice + 1);
    }
}
#endif /* INCLUDE_MAP_CMD or INCLUDE_CUTOUT_CMD defined? */


/**************************************************************/
static int
show_toc(const dict_t *config,
         const char *device_name)
{
    /*@only@*/ apa_toc_t *toc = NULL;
    int result = apa_toc_read(config, device_name, &toc);
    if (result == RET_OK && toc != NULL) {
        show_apa_toc(toc, 1);
        apa_toc_free(toc);
    }
    return (result);
}


/**************************************************************/
static int
show_hdl_toc(const dict_t *config,
             const char *device_name)
{
    /*@only@*/ hio_t *hio = NULL;
    int result = hio_probe(config, device_name, &hio);
    if (result == RET_OK && hio != NULL) {
        /*@only@*/ hdl_games_list_t *glist = NULL;
        result = hdl_glist_read(hio, &glist);
        if (result == RET_OK && glist != NULL) {
            u_int32_t i, j;
            printf("%-4s%9s %-*s %-3s %-12s %s\n",
                   "type", "size", MAX_FLAGS * 2 - 1, "flags", "dma",
                   "startup", "name");
            for (i = 0; i < glist->count; ++i) {
                const hdl_game_info_t *game = glist->games + i;
                char compat_flags[MAX_FLAGS * 2 + 1];
                char dma[4];
                /*unsigned short dma_dummy = 0;*/
                dma[0] = dma[1] = '\0';
                compat_flags[0] = '\0';
                for (j = 0; j < MAX_FLAGS; ++j)
                    if (((int)game->compat_flags & (1 << j)) != 0) {
                        char buffer[5];
                        sprintf(buffer, "+%u", (unsigned int)(j + 1));
                        strcat(compat_flags, buffer);
                    }
                if (compat_flags[0] == '+')
                    compat_flags[0] = ' '; /* trim leading + */
                else {
                    compat_flags[0] = '0';
                    compat_flags[1] = '\0';
                }
                /*dma_dummy = game->dma;*/
                if ((unsigned short)game->dma % 256 == 32) {
                    int dma_dummy = 0;
                    dma_dummy = ((unsigned short)game->dma - 32) / 256;
                    if (dma_dummy < 3) {
                        char buffer[5];
                        sprintf(buffer, "%s", "*m");
                        strcat(dma, buffer);
                        sprintf(buffer, "%u", (unsigned int)dma_dummy);
                        strcat(dma, buffer);
                    }
                } else if ((unsigned short)game->dma % 256 == 64) {
                    int dma_dummy = 0;
                    dma_dummy = ((unsigned short)game->dma - 64) / 256;
                    if (dma_dummy < 5) {
                        char buffer[5];
                        sprintf(buffer, "%s", "*u");
                        strcat(dma, buffer);
                        sprintf(buffer, "%u", (unsigned int)dma_dummy);
                        strcat(dma, buffer);
                    }
                }

                printf("%3s %7luKB %*s %-3s %-12s %s\n",
                       game->is_dvd != 0 ? "DVD" : "CD ",
                       (unsigned long)game->raw_size_in_kb,
                       MAX_FLAGS * 2 - 1, compat_flags,
                       dma,
                       game->startup,
                       game->name);
            }
            printf("total %uMB, used %uMB, available %uMB\n",
                   (unsigned int)(glist->total_chunks * 128),
                   (unsigned int)((glist->total_chunks -
                                   glist->free_chunks) *
                                  128),
                   (unsigned int)(glist->free_chunks * 128));

            hdl_glist_free(glist);
        }
        (void)hio->close(hio), hio = NULL;
    }
    return (result);
}

/**************************************************************/
#if defined(INCLUDE_MAP_CMD)
static int
show_map(const dict_t *config,
         const char *device_name)
{
    /*@only@*/ apa_toc_t *toc = NULL;
    int result = apa_toc_read(config, device_name, &toc);
    if (result == RET_OK && toc != NULL) {
        show_apa_map(toc);
        apa_toc_free(toc);
    }
    return (result);
}
#endif /* INCLUDE_MAP_CMD defined? */


/**************************************************************/
#if defined(INCLUDE_INFO_CMD)
static int
show_hdl_game_info(const dict_t *config,
                   const char *device_name,
                   const char *game_name)
{
    /*@only@*/ hio_t *hio = NULL;
    int result = hio_probe(config, device_name, &hio);
    if (result == RET_OK && hio != NULL) {
        /*@only@*/ apa_toc_t *toc = NULL;
        result = apa_toc_read_ex(hio, &toc);
        if (result == RET_OK && toc != NULL) {
            int slice_index;
            u_int32_t partition_index;
            result = apa_find_partition(toc, game_name,
                                        &slice_index, &partition_index);
            if (result == RET_NOT_FOUND) { /* assume `name' is game name, instead of partition name */
                char partition_id[PS2_PART_IDMAX + 1];
                result = hdl_lookup_partition_ex(hio, game_name, partition_id);
                if (result == RET_OK)
                    result = apa_find_partition(toc, partition_id,
                                                &slice_index, &partition_index);
            }

            if (result == RET_OK) {                        /* partition found */
                const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
                u_int8_t buffer[1024];
                u_int32_t len;
                u_int32_t sect =
                    get_u32(&toc->slice[slice_index].parts[partition_index].header.start) + 0x00101000 / 512 + slice_index * SLICE_2_OFFS;
                result = hio->read(hio, sect, 1024 / HDD_SECTOR_SIZE,
                                   buffer, &len);
                if (result == OSAL_OK) {
                    const char *signature = (char *)buffer + 0x00ac;
                    const char *hdl_name = (char *)buffer + 0x0008;
                    u_int32_t type = (u_int32_t)buffer[0x00ec];
                    u_int32_t num_parts = (u_int32_t)buffer[0x00f0];
                    const u_int32_t *data = (u_int32_t *)(buffer + 0x00f5);
                    u_int32_t i;

                    if (get_u32(buffer) == 0xdeadfeed) { /* 0xdeadfeed magic found */
                        u_int64_t total_size = 0;

#if 0
                    /* save main partition header for debug purposes */
                    write_file (signature, buffer, 0x0200);
#endif

                        fprintf(stdout, "%s: [%s], %s\n",
                                signature, hdl_name,
                                (type == 0x14 ? "DVD" : "CD"));
                        if (buffer[0x00a9] != 0) {
                            u_int8_t flags = get_u8(buffer + 0x00a9);
                            char compat_flags[MAX_FLAGS * 2 + 1];
                            compat_flags[0] = compat_flags[1] = '\0';
                            for (i = 0; i < MAX_FLAGS; ++i)
                                if ((flags & (1 << i)) != 0) {
                                    char tmp[5];
                                    sprintf(tmp, "+%u", (unsigned int)(i + 1));
                                    strcat(compat_flags, tmp);
                                }
                            fprintf(stdout, "opl compatibility flags: %s\n",
                                    compat_flags + 1);
                        }
                        if (buffer[0x00aa] != 0) {
                            u_int8_t dma_type = get_u8(buffer + 0x00aa);
                            if ((unsigned int)dma_type == 32) {
                                u_int8_t dma_mode = get_u8(buffer + 0x00ab);
                                if ((unsigned int)dma_mode < 3) {
                                    char dma[4];
                                    char tmp[5];
                                    dma[0] = dma[1] = '\0';
                                    sprintf(tmp, "%s", "*m");
                                    strcat(dma, tmp);
                                    sprintf(tmp, "%u", (unsigned int)dma_mode);
                                    strcat(dma, tmp);
                                    fprintf(stdout, "dma mode: %s\n", dma);
                                }
                            } else if ((unsigned int)dma_type == 64) {
                                u_int8_t dma_mode = get_u8(buffer + 0x00ab);
                                if ((unsigned int)dma_mode < 5) {
                                    char dma[4];
                                    char tmp[5];
                                    dma[0] = dma[1] = '\0';
                                    sprintf(tmp, "%s", "*u");
                                    strcat(dma, tmp);
                                    sprintf(tmp, "%u", (unsigned int)dma_mode);
                                    strcat(dma, tmp);
                                    fprintf(stdout, "dma mode: %s\n", dma);
                                }
                            }
                        }
                        for (i = 0; i < num_parts; ++i) {
                            u_int32_t start = get_u32(data + (i * 3 + 1));
                            u_int32_t length = get_u32(data + (i * 3 + 2));
                            total_size += ((u_int64_t)length) << 8;
                            fprintf(stdout,
                                    "\tpart %2u is from sector 0x%06lx00, "
                                    "%7luKB long\n",
                                    (unsigned int)(i + 1),
                                    (unsigned long)start,
                                    (unsigned long)length / 4);
                        }
                        fprintf(stdout,
                                "Total size: %luKB (%luMB approx.)\n",
                                (unsigned long)(total_size / 1024),
                                (unsigned long)(total_size / (1024 * 1024)));
                    } else
                        result = RET_NOT_HDL_PART;
                } else
                    result = RET_NO_MEM;
            }
            apa_toc_free(toc);
        }
        (void)hio->close(hio), hio = NULL;
    }
    return (result);
}
#endif /* INCLUDE_INFO_CMD defined? */


/**************************************************************/
#if defined(INCLUDE_CUTOUT_CMD)
static int
show_apa_cut_out_for_inject(const dict_t *config,
                            const char *device_name,
                            int slice_index,
                            u_int32_t size_in_mb)
{
    /*@only@*/ apa_toc_t *toc = NULL;
    int result = apa_toc_read(config, device_name, &toc);
    if (result == RET_OK && toc != NULL) {
        u_int32_t new_partition_index;

        result = apa_allocate_space(toc,
                                    "<new partition>",
                                    size_in_mb,
                                    &slice_index,
                                    &new_partition_index,
                                    0);
        if (result == RET_OK) {
            show_apa_toc(toc, 0);
            show_apa_map(toc);
        }

        apa_toc_free(toc);
    }
    return (result);
}
#endif /* INCLUDE_CUTOUT_CMD defined? */


/**************************************************************/
#if defined(INCLUDE_HIDE_CMD)
static int
delete_partition(const dict_t *config,
                 const char *device_name,
                 const char *name)
{
    /*@only@*/ apa_toc_t *toc = NULL;
    int result = apa_toc_read(config, device_name, &toc);
    if (result == RET_OK && toc != NULL) {
        result = apa_HIDE_partition(toc, name);
        if (result == RET_NOT_FOUND) { /* assume `name' is game name, instead of partition name */
            char partition_id[PS2_PART_IDMAX + 1];
            result = hdl_lookup_partition(config, device_name,
                                          name, partition_id);
            if (result == RET_OK)
                result = apa_HIDE_partition(toc, partition_id);
        }

        if (result == RET_OK)
            result = apa_commit(config, device_name, toc);

        apa_toc_free(toc);
    }
    return (result);
}
#endif /* INCLUDE_HIDE_CMD defined? */


/**************************************************************/
#if defined(INCLUDE_ZERO_CMD)
static int
zero_device(const char *device_name)
{
    /*@only@*/ osal_handle_t device = OSAL_HANDLE_INIT;
    int result = osal_open_device_for_writing(device_name, &device);
    if (result == OSAL_OK) {
        void *buffer = osal_alloc(1 _MB);
        if (buffer != NULL) {
            static const int ZERO_BYTE = 0x00;
            u_int32_t bytes, counter = 0;
            memset(buffer, ZERO_BYTE, 1 _MB);
            do {
                result = osal_write(device, buffer, 1 _MB, &bytes);
                if ((counter & 0x0f) == 0x0f) {
                    fprintf(stdout, "%.2fGB   \r", (double)counter / 1024.0);
                    (void)fflush(stdout);
                }
                ++counter;
            } while (result == OSAL_OK && bytes > 0);
            osal_free(buffer);
        } else
            result = RET_NO_MEM;
        (void)osal_close(&device);
    }
    return (result);
}
#endif /* INCLUDE_ZERO_CMD defined? */


/**************************************************************/
static int
cdvd_info(const dict_t *config,
          const char *path,
          int new_style,
          FILE *out)
{
    /*@only@*/ iin_t *iin = NULL;
    int result = iin_probe(config, path, &iin);
    if (result == OSAL_OK && iin != NULL) {
        ps2_cdvd_info_t info;
        u_int32_t num_sectors, sector_size;
        result = iin->stat(iin, &sector_size, &num_sectors);
        if (result == OSAL_OK) {
            result = isofs_get_ps2_cdvd_info(iin, &info);
            if (result == OSAL_OK) {
                u_int64_t tot_size = (u_int64_t)num_sectors * sector_size;
                if (new_style == 0)
                    fprintf(out, "\"%s\" \"%s\" %s %luKB\n",
                            info.startup_elf, info.volume_id,
                            (info.layer_pvd != 0 ? "dual layer" : ""),
                            (unsigned long)(tot_size / 1024));
                else
                    fprintf(out, "%s%s %luKB \"%s\" \"%s\" \n",
                            (info.layer_pvd != 0 ? "dual-layer " : ""),
                            (info.media_type == mt_cd  ? "CD" :
                             info.media_type == mt_dvd ? "DVD" :
                                                         "?"),
                            (unsigned long)(tot_size / 1024),
                            info.volume_id, info.startup_elf);
            }
        }
        (void)iin->close(iin), iin = NULL;
    }
    return (result);
}


/**************************************************************/
#if defined(_BUILD_WIN32)
static int
query_devices(const dict_t *config)
{
    /*@only@*/ osal_dlist_t *hard_drives = NULL;
    /*@only@*/ osal_dlist_t *optical_drives = NULL;

    int result = osal_query_devices(&hard_drives, &optical_drives);
    if (result == RET_OK) {
        u_int32_t i;

        if (hard_drives != NULL) {
            fprintf(stdout, "Hard drives:\n");
            for (i = 0; i < hard_drives->used; ++i) {
                const osal_dev_t *dev = hard_drives->device + i;
                fprintf(stdout, "\t%s ", dev->name);
                if (dev->status == 0) {
                    fprintf(stdout, "%lu MB",
                            (unsigned long)(dev->capacity / 1024) / 1024);
                    if (dev->is_ps2 == RET_OK)
                        fprintf(stdout, ", formatted Playstation 2 HDD\n");
                    else
                        fprintf(stdout, "\n");
                } else {
                    char *error = osal_get_error_msg(dev->status);
                    if (error != NULL) {
                        (void)fputs(error, stdout);
                        osal_dispose_error_msg(error);
                    } else
                        fprintf(stdout, "error querying device.\n");
                }
            }
            osal_dlist_free(hard_drives);
        }

        if (optical_drives != NULL) {
            fprintf(stdout, "\nOptical drives:\n");
            for (i = 0; optical_drives != NULL && i < optical_drives->used; ++i) {
                const osal_dev_t *dev = optical_drives->device + i;
                fprintf(stdout, "\t%s ", dev->name);
                if (dev->status == 0)
                    fprintf(stdout, "%lu MB\n",
                            (unsigned long)(dev->capacity / 1024) / 1024);
                else {
                    char *error = osal_get_error_msg(dev->status);
                    if (error != NULL) {
                        (void)fputs(error, stdout);
                        osal_dispose_error_msg(error);
                    } else
                        fprintf(stdout, "error querying device.\n");
                }
            }
            osal_dlist_free(optical_drives);
        }
    }

    if (dict_get_flag(config, CONFIG_ENABLE_ASPI_FLAG, 0)) { /* ASPI support is enabled/disabled via config file, now */
        if (aspi_load() == RET_OK) {
            scsi_devices_list_t *dlist;
            result = aspi_scan_scsi_bus(&dlist);
            if (result == RET_OK) {
                u_int32_t i;

                fprintf(stdout, "\nOptical drives via ASPI:\n");
                for (i = 0; i < dlist->used; ++i) {
                    if (dlist->device[i].type == 0x05) { /* MMC device */
                        printf("\tcd%d:%d:%d  ",
                               dlist->device[i].host,
                               dlist->device[i].scsi_id,
                               dlist->device[i].lun);

                        if (dlist->device[i].name[0] != '\0')
                            printf("(%s):  ", dlist->device[i].name);

                        if (dlist->device[i].size_in_sectors != -1 &&
                            dlist->device[i].sector_size != -1)
                            printf("%lu MB\n",
                                   (unsigned long)(((u_int64_t)dlist->device[i].size_in_sectors *
                                                    dlist->device[i].sector_size) /
                                                   (1024 * 1024)));
                        else {
#if 1 /* used to be not really meaningful */
                            const char *error = aspi_get_error_msg(dlist->device[i].status);
                            printf("%s\n", error);
                            aspi_dispose_error_msg((char *)error);
#else
                            printf("Stat failed.\n");
#endif
                        }
                    }
                }
                aspi_dlist_free(dlist);
            } else
                fprintf(stderr, "\nError scanning SCSI bus.\n");
            aspi_unload();
        } else
            fprintf(stderr, "\nUnable to initialize ASPI.\n");
    }

    if (1) { /* list drive letters accessible via SPTI */
        char path[4] = {'?', ':', '\\'};
        char iin_path[3] = {'?', ':'};
        fprintf(stdout, "\nOptical drives via SPTI:\n");
        for (path[0] = 'c'; path[0] <= 'z'; ++path[0]) {
            if (GetDriveType(path) == DRIVE_CDROM) {
                /*@only@*/ iin_t *iin = NULL;
                iin_path[0] = path[0];
                fprintf(stdout, "\t%s ", iin_path);
                result = iin_probe(config, iin_path, &iin);
                if (result == RET_OK && iin != NULL) {
                    u_int32_t num_sectors, sector_size;
                    result = iin->stat(iin, &sector_size, &num_sectors);
                    if (result == RET_OK)
                        fprintf(stdout, "%u MB",
                                (unsigned int)(num_sectors / 512));
                    (void)iin->close(iin);
                }
                fprintf(stdout, "\n");
            }
        }
        fprintf(stdout, "\n");
        result = RET_OK; /* ignore errors */
    }

    return (result);
}
#endif /* _BUILD_WIN32 defined? */


/**************************************************************/
#if defined(INCLUDE_COMPARE_IIN_CMD)
static int
compare_iin(const dict_t *config,
            const char *path1,
            const char *path2,
            progress_t *pgs)
{
    /*@only@*/ iin_t *iin1 = NULL;
    int different = 0, longer_file = 0;
    int result = iin_probe(config, path1, &iin1);
    if (result == OSAL_OK && iin1 != NULL) {
        /*@only@*/ iin_t *iin2 = NULL;
        result = iin_probe(config, path2, &iin2);
        if (result == OSAL_OK && iin2 != NULL) {
            u_int32_t len1, len2;
            const char *data1, *data2;
            u_int32_t sector = 0;
            u_int32_t sector_size1, num_sectors1;
            u_int32_t sector_size2, num_sectors2;
            u_int64_t size1, size2;

            result = iin1->stat(iin1, &sector_size1, &num_sectors1);
            if (result == OSAL_OK) {
                size1 = (u_int64_t)num_sectors1 * sector_size1;
                result = iin2->stat(iin2, &sector_size2, &num_sectors2);
            }
            if (result == OSAL_OK) {
                size2 = (u_int64_t)num_sectors2 * sector_size2;
                if (sector_size1 == IIN_SECTOR_SIZE &&
                    sector_size2 == IIN_SECTOR_SIZE)
                    /* progress indicator is set up against the shorter file */
                    pgs_prepare(pgs, size1 < size2 ? size1 : size2);
                else
                    /* unable to compare with different sector sizes */
                    result = RET_DIFFERENT;
            }

            len1 = len2 = IIN_SECTOR_SIZE;
            while (result == OSAL_OK &&
                   len1 / IIN_SECTOR_SIZE > 0 &&
                   len2 / IIN_SECTOR_SIZE > 0) {
                u_int32_t sectors1 = (num_sectors1 > IIN_NUM_SECTORS ?
                                          IIN_NUM_SECTORS :
                                          num_sectors1);
                result = iin1->read(iin1, sector, sectors1, &data1, &len1);
                if (result == OSAL_OK) {
                    u_int32_t sectors2 = (num_sectors2 > IIN_NUM_SECTORS ?
                                              IIN_NUM_SECTORS :
                                              num_sectors2);
                    result = iin2->read(iin2, sector, sectors2, &data2, &len2);
                    if (result == OSAL_OK) {
                        u_int32_t len = (len1 <= len2 ? len1 : len2); /* lesser from the two */
                        u_int32_t len_s = len / IIN_SECTOR_SIZE;

                        if (memcmp(data1, data2, len) != 0) {
                            different = 1;
                            break;
                        }

                        if (len1 != len2 &&
                            (len1 == 0 || len2 == 0)) { /* track which file is longer */
                            if (len2 == 0)
                                longer_file = 1;
                            else if (len1 == 0)
                                longer_file = 2;
                        }

                        num_sectors1 -= len_s;
                        num_sectors2 -= len_s;
                        sector += len_s;
                        result = pgs_update(pgs, (u_int64_t)sector * IIN_SECTOR_SIZE);
                    }
                }
            } /* loop */
            (void)iin2->close(iin2), iin2 = NULL;
        }
        (void)iin1->close(iin1), iin1 = NULL;
    }

    /* handle result code */
    if (result == OSAL_OK) {  /* no I/O errors */
        if (different == 0) { /* contents are the same */
            switch (longer_file) {
                case 0:
                    return (RET_OK); /* neither */
                case 1:
                    return (RET_1ST_LONGER);
                case 2:
                    return (RET_2ND_LONGER);
                default:
                    return (RET_ERR); /* should not be here */
            }
        } else
            /* contents are different */
            return (RET_DIFFERENT);
    } else
        return (result);
}
#endif /* INCLUDE_COMPARE_IIN_CMD defined? */


/**************************************************************/
#if defined(INCLUDE_BACKUP_TOC_CMD)
static int
backup_toc(const dict_t *config,
           const char *source_device,
           const char *file)
{
    /*@only@*/ hio_t *in = NULL;
    int result = hio_probe(config, source_device, &in);
    if (result == RET_OK && in != NULL) {
        u_int32_t size_in_kb;
        result = in->stat(in, &size_in_kb);
        if (result == RET_OK) {
            u_int32_t sectors = size_in_kb * 2;
            FILE *out = fopen(file, "wb");
            if (out != NULL) {
                unsigned char buf[1024];
                u_int32_t sector = 0, bytes;
                while (sector < sectors) {
                    result = in->read(in, sector, 2, buf, &bytes);
                    if (result == RET_OK && bytes == 1024)
                        result = (fwrite(buf, 1, 1024, out) == 1024 ?
                                      RET_OK :
                                      RET_ERR);
                    if (result == RET_OK) {
                        result = in->read(in, sector + 2056, 2,
                                          buf, &bytes);
                        if (result == RET_OK && bytes == 1024)
                            result = (fwrite(buf, 1, 1024, out) == 1024 ?
                                          RET_OK :
                                          RET_ERR);
                    }
                    sector += 128 * 1024 * 2;
                }
                (void)fclose(out);
            } else
                result = RET_ERR;
        }
        (void)in->close(in), in = NULL;
    }
    return (result);
}
#endif /* INCLUDE_BACKUP_TOC_CMD defined? */


/**************************************************************/
#if defined(INCLUDE_RESTORE_TOC_CMD)
static int
restore_toc(const dict_t *config,
            const char *dest_device,
            const char *file)
{
    int result = RET_OK;
    FILE *in = fopen(file, "rb");
    if (in != NULL) {
        /*@only@*/ hio_t *out = NULL;
        result = hio_probe(config, dest_device, &out);
        if (result == RET_OK && out != NULL) {
            unsigned char buf[1024];
            u_int32_t sector = 0;
            size_t bytes;
            do {
                u_int32_t len;
                bytes = fread(buf, 1, 1024, in);
                if (bytes == 1024) {
                    result = out->write(out, sector, 2, buf, &len);
                    if (result == RET_OK)
                        result = (len == 1024 ? RET_OK : RET_ERR);
                } else
                    result = (bytes == 0 ? RET_OK : RET_ERR);
                if (result == RET_OK) {
                    bytes = fread(buf, 1, 1024, in);
                    if (bytes == 1024) {
                        result = out->write(out, sector + 2056, 2, buf, &len);
                        if (result == RET_OK)
                            result = (len == 1024 ? RET_OK : RET_ERR);
                    } else
                        result = (bytes == 0 ? RET_OK : RET_ERR);
                }
                sector += 128 * 1024 * 2;
            } while (result == RET_OK && bytes == 1024);
            (void)out->close(out), out = NULL;
        }
        (void)fclose(in);
    } else
        result = RET_ERR;
    return (result);
}
#endif /* INCLUDE_RESTORE_TOC_CMD defined? */


/**************************************************************/
static int
inject(const dict_t *config,
       const char *output,
       const char *name,
       const char *input,
       const char *startup, /* or NULL */
       compat_flags_t compat_flags,
       unsigned short dma,
       int is_dvd,
       int is_hidden,
       int slice_index,
       progress_t *pgs)
{
    hdl_game_t game;
    int result = RET_OK;
    /*@only@*/ iin_t *iin = NULL;
    /*@only@*/ hio_t *hio = NULL;

    result = iin_probe(config, input, &iin);
    if (result == RET_OK && iin != NULL) {
        result = hio_probe(config, output, &hio);
        if (result == RET_OK && hio != NULL) {
            ps2_cdvd_info_t info;
            memset(&game, 0, sizeof(hdl_game_t));
            memmove(game.name, name, sizeof(game.name) - 1);
            game.name[sizeof(game.name) - 1] = '\0';
            if (game.name[0] != '_' && game.name[1] != '_') {
                result = isofs_get_ps2_cdvd_info(iin, &info);
                if (result == RET_OK) {
                    if (info.layer_pvd != 0)
                        game.layer_break = (u_int32_t)info.layer_pvd - 16;
                    else
                        game.layer_break = 0;
                }
            }
            if (startup != NULL) { /* use given startup file */
                memmove(game.startup, startup, sizeof(game.startup) - 1);
                game.startup[sizeof(game.startup) - 1] = '\0';
                if (result == RET_NOT_PS_CDVD)
                    /* ... and ignore possible `not a PS2 CD/DVD' error */
                    result = RET_OK;
            } else { /* we got startup file from above; fail if not PS2 CD/DVD */
                if (result == RET_OK)
                    strcpy(game.startup, info.startup_elf);
            }
            game.compat_flags = compat_flags;
            game.dma = dma;
            game.is_dvd = is_dvd;

            if (result == RET_OK)
                /* update compatibility database */
                (void)ddb_update(config, game.startup,
                                 game.name, game.compat_flags);

            if (result == RET_OK)
                result = hdl_inject(hio, iin, &game, slice_index, is_hidden, pgs);

            (void)hio->close(hio), hio = NULL;
        }
        (void)iin->close(iin), iin = NULL;
    }

    return (result);
}


/**************************************************************/
static int
install(const dict_t *config,
        const char *output,
        const char *input,
        int slice_index,
        int is_hidden,
        progress_t *pgs)
{
    hdl_game_t game;
    /*@only@*/ iin_t *iin = NULL;
    int result;
    u_int32_t sector_size, num_sectors;

    result = iin_probe(config, input, &iin);
    if (result == RET_OK && iin != NULL) {
        result = iin->stat(iin, &sector_size, &num_sectors);
        if (result == RET_OK) {
            /*@only@*/ hio_t *hio = NULL;
            result = hio_probe(config, output, &hio);
            if (result == RET_OK && hio != NULL) {
                ps2_cdvd_info_t info;
                char name[HDL_GAME_NAME_MAX + 1];
                compat_flags_t flags;
                int incompatible;

                result = isofs_get_ps2_cdvd_info(iin, &info);
                if (result == RET_OK) {
                    result = ddb_lookup(config, info.startup_elf, name, &flags);
                    incompatible = (result == RET_DDB_INCOMPATIBLE) ? 1 : 0;

                    if (result == RET_OK || incompatible == 1) {
                        memset(&game, 0, sizeof(hdl_game_t));
                        strcpy(game.name, name);
                        if (info.layer_pvd != 0)
                            game.layer_break = (u_int32_t)info.layer_pvd - 16;
                        else
                            game.layer_break = 0;
                        strcpy(game.startup, info.startup_elf);
                        game.compat_flags = flags;

                        /* TODO: the following assumption might be incorrect */
                        switch (info.media_type) {
                            case mt_cd:
                                game.is_dvd = 0;
                                break;
                            case mt_dvd:
                                game.is_dvd = 1;
                                break;
                            case mt_unknown:
                                game.is_dvd = ((u_int64_t)sector_size * num_sectors) > (750 _MB) ? 1 : 0;
                                break;
                        }

                        result = hdl_inject(hio, iin, &game, slice_index, is_hidden, pgs);
                        result = (result == RET_OK && incompatible == 1 ?
                                      RET_DDB_INCOMPATIBLE :
                                      result);
                    }
                }
                (void)hio->close(hio), hio = NULL;
            }
        }
        (void)iin->close(iin), iin = NULL;
    }

    return (result);
}


/**************************************************************/
static int
diag(const dict_t *config,
     const char *device)
{
    char buf[1024 * 10];
    int result;
    buf[0] = '\0';
    result = apa_diag(config, device, buf, sizeof(buf));
    if (result == RET_OK)
        (void)puts(buf);
    return (result);
}


/**************************************************************/
static int
modify(const dict_t *config,
       const char *device,
       const char *game,
       const char *new_name,
       compat_flags_t new_flags,
       unsigned short new_dma,
       int is_hidden)
{
    /*@only@*/ hio_t *hio = NULL;
    int result = hio_probe(config, device, &hio);
    if (result == RET_OK && hio != NULL) {
        /*@only@*/ apa_toc_t *toc = NULL;
        result = apa_toc_read_ex(hio, &toc);
        if (result == RET_OK && toc != NULL) {
            int slice_index = 0;
            u_int32_t partition_index = 0;
            result = apa_find_partition(toc, game, &slice_index,
                                        &partition_index);
            if (result == RET_NOT_FOUND) { /* assume it is `game_name' and not a partition name */
                char partition_id[PS2_PART_IDMAX + 1];
                result = hdl_lookup_partition_ex(hio, game, partition_id);
                if (result == RET_OK)
                    result = apa_find_partition(toc, partition_id,
                                                &slice_index, &partition_index);
            }

            if (result == RET_OK) {
                u_int32_t start_sector = get_u32(&toc->slice[slice_index].parts[partition_index].header.start);
                result = hdl_modify_game(hio, toc, slice_index, start_sector,
                                         new_name, new_flags, new_dma, is_hidden);
            }

            apa_toc_free(toc), toc = NULL;
        }
        (void)hio->close(hio), hio = NULL;
    }
    return (result);
}


/**************************************************************/
static int
modify_header(const dict_t *config,
              const char *device,
              const char *partname)
{
    /*@only@*/ hio_t *hio = NULL;
    int result = hio_probe(config, device, &hio);
    if (result == RET_OK && hio != NULL) {
        /*@only@*/ apa_toc_t *toc = NULL;
        result = apa_toc_read_ex(hio, &toc);
        if (result == RET_OK && toc != NULL) {
            int slice_index = 0;
            u_int32_t partition_index = 0;
            result = apa_find_partition(toc, partname, &slice_index,
                                        &partition_index);

            if (result == RET_OK) {
                u_int32_t start_sector = get_u32(&toc->slice[slice_index].parts[partition_index].header.start);
                result = hdd_inject_header(hio, toc, slice_index, start_sector);
            }

            apa_toc_free(toc), toc = NULL;
        }
        (void)hio->close(hio), hio = NULL;
    }
    return (result);
}


/**************************************************************/
static int
copy_hdd(const dict_t *config,
         const char *src_device_name,
         const char *dest_device_name,
         const char *flags,
         progress_t *pgs)
{
    hio_t *hio = NULL;
    int result, i;
    hdl_games_list_t *in_list = NULL, *out_list = NULL;
    size_t count = 0, flags_count = 0, chunks_needed = 0;

    /* read games lists for both -- source and destination */
    result = hio_probe(config, src_device_name, &hio);
    if (result == RET_OK && hio != NULL) {
        result = hdl_glist_read(hio, &in_list);
        (void)hio->close(hio), hio = NULL;
    }
    if (result == RET_OK) {
        result = hio_probe(config, dest_device_name, &hio);
        if (result == RET_OK && hio != NULL) {
            result = hdl_glist_read(hio, &out_list);
            (void)hio->close(hio), hio = NULL;
        }
    }

    if (result == RET_OK) { /* calculate space required */
        flags_count = (flags ? strlen(flags) : 0);
        for (i = 0; i < in_list->count; ++i)
            if (i >= flags_count || tolower(flags[i]) == 'y') {
                const hdl_game_info_t *game = in_list->games + i;
                ++count;
                chunks_needed += (game->alloc_size_in_kb / 1024 + 127) / 128;
            }
        result = (out_list->free_chunks >= chunks_needed ?
                      RET_OK :
                      RET_NO_SPACE);
    }

    if (result == RET_OK && count > 0) {
        printf("%ludMB in %lu game(s) remaining...\n",
               (long unsigned int)chunks_needed * 128, (long unsigned int)count);
        for (i = 0; result == RET_OK && i < in_list->count; ++i)
            if (i >= flags_count || tolower(flags[i]) == 'y') { /* copy that game */
                char in[1024];
                const hdl_game_info_t *game = in_list->games + i;
                int is_hidden = 0;

                /* Determine if the partition is hidden */
                if (strncmp(game->partition_name, HIDDEN_PART, 3) == 0)
                    is_hidden = 1;

                sprintf(in, "%s@%s", game->name, src_device_name);
                result = inject(config, dest_device_name, game->name,
                                in, game->startup, game->compat_flags, game->dma,
                                game->is_dvd, is_hidden, -1, pgs);
                if (result == RET_OK)
                    fprintf(stdout, "  %s copied.                               \n",
                            game->name);
                if (result == RET_PART_EXISTS) { /* treat "exists" errors as warnings */
                    fprintf(stderr, " ...skipping %s\n", game->name);
                    result = RET_OK;
                }
            }
    } else if (count == 0) {
        printf("Nothing to do.\n");
        result = RET_NOT_ALLOWED;
    }

    if (in_list != NULL)
        hdl_glist_free(in_list);
    if (out_list != NULL)
        hdl_glist_free(out_list);

    return (result);
}


/**************************************************************/
static int
remote_poweroff(const dict_t *config,
                const char *ip)
{
    /*@only@*/ hio_t *hio = NULL;
    int result = hio_probe(config, ip, &hio);
    if (result == RET_OK && hio != NULL) {
        result = hio->poweroff(hio);
        (void)hio->close(hio), hio = NULL;
    }
    return (result);
}


/**************************************************************/
static volatile int sigint_catched = 0;

static void
handle_sigint(/*@unused@*/ int signo)
{
    fprintf(stderr, "Ctrl+C\n");
    sigint_catched = 1;
#if defined(_BUILD_WIN32)
    while (1)
        Sleep(1); /* endless loop; will end when main thread ends */
#endif
}

static int
progress_cb(progress_t *pgs, /*@unused@*/ void *data)
{
    static time_t last_flush = 0;
    time_t now = time(NULL);

    if (pgs->remaining != -1)
        fprintf(stdout,
                "%3d%%, %s remaining, %.2f MB/sec         \r",
                pgs->pc_completed, pgs->remaining_text,
                (double)pgs->curr_bps / (1024.0 * 1024.0));
    else
        fprintf(stdout, "%3d%%\r", pgs->pc_completed);

    if (now > last_flush) { /* flush about once per second */
        (void)fflush(stdout);
        last_flush = now;
    }

    return (sigint_catched == 0 ? RET_OK : RET_INTERRUPTED);
}

/* progress is allocated, but never freed, which is not a big deal for a CLI app */
static progress_t *
get_progress(void)
{
#if 0
  return (NULL);
#else
    progress_t *pgs = pgs_alloc(&progress_cb, NULL);
    return (pgs);
#endif
}


/**************************************************************/
/*@noreturn@*/ static void
show_usage_and_exit(const char *app_path,
                    const char *command)
{
    int command_found;
    static const struct help_entry_t
    {
        const char *command_name;
        const char *command_arguments;
        const char *description, *details;
        const char *example1, *example2;
        int dangerous;
    } help[] =
    {
#if defined(_BUILD_WIN32)
        {CMD_QUERY, NULL,
         "display a list of all recognized hard- and optical drives",
         NULL, NULL, NULL, 0},
#endif
#if defined(INCLUDE_DUMP_CMD)
        {CMD_DUMP, "device file",
         "make device image (AKA ISO-image)", NULL,
         "cd0: c:\\tekken.iso", "\"Tekken@192.168.0.10\" ./tekken.iso", 0},
#endif
#if defined(INCLUDE_COMPARE_IIN_CMD)
        {CMD_COMPARE_IIN, "iin1 iin2",
         "compare two ISO inputs", NULL,
         "c:\\tekken.cue cd0:", "c:\\gt3.gi GT3@hdd1:", 0},
#endif
        {CMD_TOC, "device",
         "display PlayStation 2 HDD TOC", NULL,
         "hdd1:", "192.168.0.10", 0},
        {CMD_HDL_TOC, "device",
         "display a list of all HDL games on the PlayStation 2 HDD", NULL,
         "hdd1:", "192.168.0.10", 0},
#if defined(INCLUDE_MAP_CMD)
        {CMD_MAP, "device",
         "display PlayStation 2 HDD usage map", NULL,
         "hdd1:", NULL, 0},
#endif
#if defined(INCLUDE_HIDE_CMD)
        {CMD_HIDE, "device partition/game",
         "hide PlayStation 2 HDD partition", "First attempts to locate partition\n"
                                             "by name, then by game name. It is better to use another tool for removing.",
         "hdd1: \"PP.HDL.Tekken Tag Tournament\"", "192.168.0.10 \"Tekken\"", 1},
#endif
#if defined(INCLUDE_ZERO_CMD)
        {CMD_ZERO, "device",
         "fill HDD with zeroes. All information on the HDD will be lost", NULL,
         "hdd1:", NULL, 1},
#endif
#if defined(INCLUDE_CUTOUT_CMD)
        {CMD_CUTOUT, "device size_in_MB [@slice_index]",
         "display partition table as if a new partition has been created",
         "slice_index is the index of the slice to attempt to allocate in first\n"
         "-- 1 or 2.",
         "hdd1: 2560", "192.168.0.10 640", 0},
#endif
#if defined(INCLUDE_INFO_CMD)
        {CMD_HDL_INFO, "device partition",
         "display information about HDL partition", NULL,
         "hdd1: \"tekken tag tournament\"", "192.168.0.10 Tekken", 0},
#endif
        {CMD_HDL_EXTRACT, "device name output_file",
         "extract application image from HDL partition", NULL,
         "hdd1: \"tekken tag tournament\" c:\\tekken.iso", NULL, 0},
        {CMD_HDL_INJECT_CD, "target name source [startup] [+flags] [*dma] [@slice_index] [-hide]",
         "create a new HDL partition from a CD",
         "You can use boot.elf, list.cio, icon.sys. Check Readme\n"
         "Supported inputs: plain ISO files, CDRWIN cuesheets, Nero images and tracks,\n"
         "RecordNow! Global images, HDL partitions (PP.HDL.Xenosaga@hdd1:) and\n"
         "Sony CD/DVD generator IML files (with full paths).\n"
         /* "Startup file, dma and compatibility flags are optional.\n" */
         "Items in brackets [] are optional.\n"
         "Flags syntax is `+#[+#[+#]]' or `0xNN', for example `+1', `+2+3', `0x00', `0x03' etc.\n"
         "DMA syntax is `*u4` for UDMA4 (default).\n"
         "-hide will cause it to be hidden in HDDOSD/Browser 2.0",
         "192.168.0.10 \"Tekken Tag Tournament\" cd0: SCES_xxx.xx *u4",
         "hdd1: \"Tekken\" c:\\tekken.iso SCES_xxx.xx +1+2 *u4", 1},
        {CMD_HDL_INJECT_DVD, "target name source [startup] [+flags] [*dma] [@slice_index] [-hide]",
         "create a new HDL partition from a DVD",
         "You can use boot.elf, list.cio, icon.sys. Check Readme\n"
         "DVD-9 supports only ISO or IML.\n"
         "Supported inputs: plain ISO files, CDRWIN cuesheets, Nero images and tracks,\n"
         "RecordNow! Global images, HDL partitions (PP.HDL.Xenosaga@192....) and\n"
         "Sony CD/DVD generator IML files (with full paths).\n"
         /* "Startup file, dma and compatibility flags are optional.\n" */
         "Items in brackets [] are optional.\n"
         "Flags syntax is `+#[+#[+#]]' or `0xNN', for example `+1', `+2+3', `0x00', `0x03', etc.\n"
         "DMA syntax is `*u4` for UDMA4 (default).\n"
         "-hide will cause it to be hidden in HDDOSD/Browser 2.0",
         "192.168.0.10 \"Gran Turismo 3\" cd0: *u4",
         "hdd1: \"Gran Turismo 3\" c:\\gt3.iso SCES_xxx.xx +2+3 *u4", 1},
        {CMD_HDL_INSTALL, "target source [@slice_index] [-hide]",
         "create a new HDL partition from a source, that has an entry in compatibility list",
         "You need boot.elf for installing the game. More info in Readme\n"
         "-hide will cause it to be hidden in HDDOSD/Browser 2.0",
         "192.168.0.10 cd0:", "hdd1: c:\\gt3.iso", 1},
        {CMD_CDVD_INFO, "iin_input",
         "display signature (startup file), volume label and data size for a CD-/DVD-drive or image file", NULL,
         "c:\\gt3.gi", "\"hdd2:Gran Turismo 3\"", 0},
        {CMD_CDVD_INFO2, "iin_input",
         "display media type, startup ELF, volume label and data size for a CD-/DVD-drive or image file", NULL,
         "c:\\gt3.gi", "\"hdd2:Gran Turismo 3\"", 0},
        {CMD_POWER_OFF, "ip",
         "power off Playstation 2", NULL,
         "192.168.0.10", NULL, 0},
#if defined(INCLUDE_INITIALIZE_CMD)
        {CMD_INITIALIZE, "device input_file",
         "inject input_file into MBR",
         "All your partitions remain intact!!!" CMD_INITIALIZE " is rewrited by AKuHAK.",
         "hdd1: MBR.KELF", NULL, 1},
#endif /* INCLUDE_INITIALIZE_CMD defined? */
#if defined(INCLUDE_DUMP_MBR_CMD)
        {CMD_DUMP_MBR, "device output_file",
         "dump mbr to disc", NULL,
         "hdd1: MBR.KELF", NULL, 0},
#endif /* INCLUDE_DUMP_MBR_CMD defined? */
#if defined(INCLUDE_BACKUP_TOC_CMD)
        {CMD_BACKUP_TOC, "device file",
         "dump TOC into a binary file", NULL,
         "hdd1: toc.bak", NULL, 0},
#endif /* INCLUDE_BACKUP_TOC_CMD defined? */
#if defined(INCLUDE_RESTORE_TOC_CMD)
        {CMD_RESTORE_TOC, "device file",
         "restore TOC from a binary file", NULL,
         "hdd1: toc.bak", NULL, 1},
#endif /* INCLUDE_RESTORE_TOC_CMD defined? */
#if defined(INCLUDE_DIAG_CMD)
        {CMD_DIAG, "device",
         "scan PS2 HDD for partition errors", NULL,
         "hdd1:", "192.168.0.10", 0},
#endif /* INCLUDE_DIAG_CMD defined? */
#if defined(INCLUDE_MODIFY_CMD)
        {CMD_MODIFY, "device game [new_name] [new_flags] [dma] [-hide/-unhide]",
         "rename a game and/or change compatibility flags",
         "Items in brackets [] are optional but at least one option must be used.\n"
         "Flags syntax is `+#[+#[+#]]' or `0xNN', for example `+1', `+2+3', `0x00', `0x03', etc.\n"
         "DMA syntax is `*u4` for UDMA4\n"
         "-hide or -unhide will change the visibility of a game in HDDOSD/Browser 2.0",
         "hdd1: DDS \"Digital Devil Saga\"",
         "192.168.0.100 \"FF X-2\" +3", 1},
#endif /* INCLUDE_MODIFY_CMD defined? */
#if defined(INCLUDE_COPY_HDD_CMD)
        {CMD_COPY_HDD, "source_device destination_device [flags]",
         "copy file between two device",
         "You need boot.elf and list.ico for installing the game. More info in Readme\n"
         "Be careful all games will use one list.ico and boot.elf.\n"
         "Copy games from one device to another. Flags is a sequence of `y' or `n'\n"
         "characters, one for each game on the source device, given in the same order as\n"
         "in hdl_toc command list. If no character given for a particular game (or flags\n"
         "are missing) yes is assumed. " CMD_COPY_HDD " is contributed by JimmyZ.",
         "hdd1: 192.168.0.100 # to copy all games",
         "hdd1: hdd2: ynyn # to copy all games but 2nd and 4th", 1},
#endif /* INCLUDE_COPY_HDD_CMD defined? */
        {CMD_MODIFY_HEADER, "device partition_name",
         "inject attributes into partition header for using with HDD OSD or BB Navigator",
         "system.cnf,\n"
         "icon.sys,\n"
         "list.ico,\n"
         "del.ico,    if it is not present the list.ico will be used\n"
         "boot.kelf,\n"
         "boot.elf,   if boot.kelf not present, boot.elf will be parsed\n"
         "boot.kirx,\n"
         "logo.raw (logo.bak will be created).\n"
         "Every file can be skipped. More info about using and restrictions in README.\n" CMD_MODIFY_HEADER " is contributed by AKuHAK.",
         "hdd2: PP.POPS-00001",
         "192.168.0.10 PP.HDL.Battlefield", 1},
        {NULL, NULL,
         NULL,
         NULL, NULL, 0}
    };
    const char *app;
    if (strrchr(app_path, '/') != NULL)
        app = strrchr(app_path, '/') + 1;
    else if (strrchr(app_path, '\\') != NULL)
        app = strrchr(app_path, '\\') + 1;
    else
        app = app_path;

    fprintf(stderr,
            "hdl_dump-" VERSION " by The W1zard 0f 0z (AKA b...) w1zard0f07@yahoo.com\n"
            "revisited by AKuHAK\nhttps://github.com/ps2homebrew/hdl-dump\n"
            "\n");

    command_found = 0;
    if (command != NULL) { /* display particular command help */
        const struct help_entry_t *h = help;
        while (h->command_name != NULL) {
            if (strcmp(command, h->command_name) == 0) {
                fprintf(stderr,
                        BOLD
                        "Usage:" UNBOLD
                        "\t%s %s\n"
                        "\n"
                        "%s\n",
                        h->command_name, h->command_arguments,
                        h->description);
                if (h->details != NULL)
                    fprintf(stderr, "\n%s", h->details);
                if (h->example1 != NULL)
                    fprintf(stderr, "\n\n" BOLD "Example:\n\n" UNBOLD "%s %s %s\n", app, h->command_name, h->example1);
                if (h->example2 != NULL)
                    fprintf(stderr, "\tor\n"
                                    "%s %s %s\n",
                            app, h->command_name, h->example2);
                if (h->dangerous != 0)
                    fprintf(stderr,
                            "\n" BOLD
                            "Warning:\n\n" UNBOLD
                            "This command does write on the HDD\n"
                            "and could cause corruption. Use with care.\n");
                command_found = 1;
                break;
            }
            ++h;
        }
    }

    if (command == NULL || command_found == 0) { /* display all commands only */
        const struct help_entry_t *h = help;

        fprintf(stderr, BOLD "Usage: " UNBOLD "%s command arguments\n"
                             "\n" BOLD "Commands list:" UNBOLD,
                app);
        while (h->command_name != NULL) {
            fprintf(stderr, "\n  %-15s%s ", h->command_name, h->description);
            if (h->dangerous != 0)
                fprintf(stderr, WARNING_SIGN);
            ++h;
        }
        fprintf(stderr, "\n");

        fprintf(stderr,
                "\n"
                "Use: %s command\n"
                "to show \"command\" help.\n"
                "\n"
                "Warning: Commands, marked with " WARNING_SIGN " does write on the HDD\n"
                "         and could cause corruption. Use with care.\n"
                "\n"
                "License: You are only allowed to use this program with a software\n"
                "         you legally own. Use at your own risk.\n",
                app);

        if (command != NULL && command_found == 0) {
            fprintf(stderr,
                    "\n"
                    "%s: unrecognized command.\n",
                    command);
        }
    }

    exit(100);
}

static void
map_device_name_or_exit(const char *input,
                        /*@out@*/ char output[MAX_PATH])
{
    int result = osal_map_device_name(input, output);
    switch (result) {
        case RET_OK:
            return;
        case RET_BAD_FORMAT:
            fprintf(stderr, "%s: bad format.\n", input);
            exit(100 + RET_BAD_FORMAT);
        case RET_BAD_DEVICE:
            fprintf(stderr, "%s: bad device.\n", input);
            exit(100 + RET_BAD_DEVICE);
    }
}

/*@noreturn@*/ static void
handle_result_and_exit(int result,
                       /*@null@*/ const char *device2,
                       /*@null@*/ const char *partition2)
{
    const char *device = (device2 != NULL ? device2 : "unknown device");
    const char *partition = (partition2 != NULL ?
                                 partition2 :
                                 "unknown partition");

    switch (result) {
        case RET_OK:
            exit(0);

        case RET_ERR: {
            unsigned long err_code = osal_get_last_error_code();
            char *error = osal_get_last_error_msg();
            if (error != NULL) {
                fprintf(stderr, "%08lx (%lu): %s\n", err_code, err_code, error);
                osal_dispose_error_msg(error);
            } else
                fprintf(stderr, "%08lx (%lu): Unknown error.\n", err_code, err_code);
        }
            exit(1);

        case RET_NO_MEM:
            fprintf(stderr, "Out of memory.\n");
            exit(2);

        case RET_NOT_APA:
            fprintf(stderr, "%s: not a PlayStation 2 HDD.\n", device);
            exit(100 + RET_NOT_APA);

        case RET_NOT_HDL_PART:
            fprintf(stderr, "%s: not a HDL partition", device);
            if (partition != NULL)
                fprintf(stderr, ": \"%s\".\n", partition);
            else
                fprintf(stderr, ".\n");
            exit(100 + RET_NOT_HDL_PART);

        case RET_NOT_FOUND:
            fprintf(stderr, "%s: partition not found", device);
            if (partition != NULL)
                fprintf(stderr, ": \"%s\".\n", partition);
            else
                fprintf(stderr, ".\n");
            exit(100 + RET_NOT_FOUND);

        case RET_NO_SPACE:
            fprintf(stderr, "%s: not enough free space.\n", device);
            exit(100 + RET_NO_SPACE);

        case RET_BAD_APA:
            fprintf(stderr, "%s: APA partition is broken; aborting.\n", device);
            exit(100 + RET_BAD_APA);

        case RET_DIFFERENT:
            fprintf(stderr, "Contents are different.\n");
            exit(100 + RET_DIFFERENT);

        case RET_INTERRUPTED:
            fprintf(stderr, "\nInterrupted.\n");
            exit(100 + RET_INTERRUPTED);

        case RET_PART_EXISTS:
            fprintf(stderr, "%s: partition with such name already exists: \"%s\".\n",
                    device, partition);
            exit(100 + RET_PART_EXISTS);

        case RET_BAD_ISOFS:
            fprintf(stderr, "%s: bad ISOFS.\n", device);
            exit(100 + RET_BAD_ISOFS);

        case RET_NOT_PS_CDVD:
            fprintf(stderr, "%s: not a Playstation CD-ROM/DVD-ROM.\n", device);
            exit(100 + RET_NOT_PS_CDVD);

        case RET_BAD_SYSCNF:
            fprintf(stderr, "%s: SYSTEM.CNF is not in the expected format.\n", device);
            exit(100 + RET_BAD_SYSCNF);

        case RET_NOT_COMPAT:
            fprintf(stderr, "Input or output is unsupported.\n");
            exit(100 + RET_NOT_COMPAT);

        case RET_NOT_ALLOWED:
            fprintf(stderr, "Operation is not allowed.\n");
            exit(100 + RET_NOT_ALLOWED);

        case RET_BAD_COMPAT:
            fprintf(stderr, "Input or output is supported, but invalid.\n");
            exit(100 + RET_BAD_COMPAT);

        case RET_SVR_ERR:
            fprintf(stderr, "Server reported error.\n");
            exit(100 + RET_SVR_ERR);

        case RET_1ST_LONGER:
            fprintf(stderr, "First input is longer, but until then the contents are the same.\n");
            exit(100 + RET_1ST_LONGER);

        case RET_2ND_LONGER:
            fprintf(stderr, "Second input is longer, but until then the contents are the same.\n");
            exit(100 + RET_2ND_LONGER);

        case RET_FILE_NOT_FOUND:
            fprintf(stderr, "File not found.\n");
            exit(100 + RET_FILE_NOT_FOUND);

        case RET_BROKEN_LINK:
            fprintf(stderr, "Broken link (linked file not found).\n");
            exit(100 + RET_BROKEN_LINK);

        case RET_CROSS_128GB:
            fprintf(stderr, "Unable to limit HDD size to 128GB - data behind 128GB mark.\n");
            exit(100 + RET_CROSS_128GB);

#if defined(_BUILD_WIN32)
        case RET_ASPI_ERROR:
            fprintf(stderr, "ASPI error: 0x%08lx (SRB/Sense/ASC/ASCQ) %s\n",
                    aspi_get_last_error_code(),
                    aspi_get_last_error_msg());
            exit(100 + RET_ASPI_ERROR);
#endif

        case RET_NO_DISC_DB:
            fprintf(stderr, "Disc database file could not be found.\n");
            exit(100 + RET_NO_DISC_DB);

        case RET_NO_DDBENTRY:
            fprintf(stderr, "There is no entry for that game in the disc database.\n");
            exit(100 + RET_NO_DDBENTRY);

        case RET_DDB_INCOMPATIBLE:
            fprintf(stderr, "Game is incompatible, according to disc database.\n");
            exit(100 + RET_DDB_INCOMPATIBLE);

        case RET_TIMEOUT:
            fprintf(stderr, "Network communication timeout.\n");
            exit(100 + RET_TIMEOUT);

        case RET_PROTO_ERR:
            fprintf(stderr, "Network communication protocol error.\n");
            exit(100 + RET_PROTO_ERR);

        case RET_INVARIANT:
            fprintf(stderr, "Errm... that is not allowed. Nope. No way.\n");
            exit(100 + RET_INVARIANT);

#if defined(_BUILD_WIN32)
        case RET_SPTI_ERROR:
            fprintf(stderr, "SPTI error: 0x%08lx (SRB/Sense/ASC/ASCQ) %s\n",
                    spti_get_last_error_code(),
                    spti_get_last_error_msg());
            exit(100 + RET_SPTI_ERROR);
#endif

        case RET_MBR_KELF_SIZE:
            fprintf(stderr, "The file size exceeds the %d bytes limit.\n", MAX_MBR_KELF_SIZE);
            exit(100 + RET_MBR_KELF_SIZE);

        case RET_INVALID_KELF:
            fprintf(stderr, "Invalid kelf header.\n");
            exit(100 + RET_INVALID_KELF);

        default:
            fprintf(stderr, "%s: don't know what the error is: %d.\n", device, result);
            exit(200);
    }
}


int main(int argc, char *argv[])
{
    dict_t *config = NULL;

    /* load configuration */
    config = dict_alloc();
    if (config != NULL) {
        set_config_defaults(config);
        (void)dict_restore(config, get_config_file());
        (void)dict_store(config, get_config_file());
    } else
        handle_result_and_exit(RET_NO_MEM, NULL, NULL);

    /* handle Ctrl+C gracefully */
    (void)signal(SIGINT, &handle_sigint);

    if (argc > 1) {
        const char *command_name = argv[1];

        if (0)
            ;

#if defined(_BUILD_WIN32)
        else if (caseless_compare(command_name, CMD_QUERY)) { /* show all devices */
            handle_result_and_exit(query_devices(config), NULL, NULL);
        }
#endif

#if defined(INCLUDE_DUMP_CMD)
        else if (caseless_compare(command_name, CMD_DUMP)) { /* dump CD/DVD-ROM to the HDD */
            if (argc != 4)
                show_usage_and_exit(argv[0], CMD_DUMP);

            handle_result_and_exit(dump_device(config, argv[2], argv[3],
                                               0, get_progress()),
                                   argv[2], NULL);
        }
#endif /* INCLUDE_DUMP_CMD defined? */

#if defined(INCLUDE_COMPARE_IIN_CMD)
        else if (caseless_compare(command_name, CMD_COMPARE_IIN)) { /* compare two iso inputs */
            if (argc != 4)
                show_usage_and_exit(argv[0], CMD_COMPARE_IIN);
            handle_result_and_exit(compare_iin(config, argv[2], argv[3],
                                               get_progress()),
                                   NULL, NULL);
        }
#endif /* INCLUDE_COMPARE_IIN_CMD defined? */

        else if (caseless_compare(command_name, CMD_TOC)) { /* show TOC of a PlayStation 2 HDD */
            if (argc != 3)
                show_usage_and_exit(argv[0], CMD_TOC);
            handle_result_and_exit(show_toc(config, argv[2]), argv[2], NULL);
        }

        else if (caseless_compare(command_name, CMD_HDL_TOC)) { /* show a TOC of installed games only */
            if (argc != 3)
                show_usage_and_exit(argv[0], CMD_HDL_TOC);
            handle_result_and_exit(show_hdl_toc(config, argv[2]),
                                   argv[2], NULL);
        }

#if defined(INCLUDE_MAP_CMD)
        else if (caseless_compare(command_name, CMD_MAP)) { /* show map of a PlayStation 2 HDD */
            if (argc != 3)
                show_usage_and_exit(argv[0], CMD_MAP);

            handle_result_and_exit(show_map(config, argv[2]), argv[2], NULL);
        }
#endif /* INCLUDE_MAP_CMD defined? */

#if defined(INCLUDE_HIDE_CMD)
        else if (caseless_compare(command_name, CMD_HIDE)) { /* delete partition */
            if (argc != 4)
                show_usage_and_exit(argv[0], CMD_HIDE);

            handle_result_and_exit(delete_partition(config, argv[2], argv[3]),
                                   argv[2], argv[3]);
        }
#endif /* INCLUDE_HIDE_CMD defined? */

#if defined(INCLUDE_ZERO_CMD)
        else if (caseless_compare(command_name, CMD_ZERO)) { /* zero HDD */
            char device_name[MAX_PATH];

            if (argc != 3)
                show_usage_and_exit(argv[0], CMD_ZERO);

            map_device_name_or_exit(argv[2], device_name);

            handle_result_and_exit(zero_device(device_name),
                                   argv[2], NULL);
        }
#endif /* INCLUDE_ZERO_CMD defined? */

#if defined(INCLUDE_INFO_CMD)
        else if (caseless_compare(command_name, CMD_HDL_INFO)) { /* show installed game info */
            if (argc != 4)
                show_usage_and_exit(argv[0], CMD_HDL_INFO);

            handle_result_and_exit(show_hdl_game_info(config, argv[2],
                                                      argv[3]),
                                   argv[2], argv[3]);
        }
#endif /* INCLUDE_INFO_CMD defined? */

        else if (caseless_compare(command_name, CMD_HDL_EXTRACT)) { /* extract game image from a HDL partition */
            if (argc != 5)
                show_usage_and_exit(argv[0], CMD_HDL_EXTRACT);

            handle_result_and_exit(hdl_extract(config, argv[2], argv[3],
                                               argv[4], get_progress()),
                                   argv[2], argv[3]);
        }

        else if (caseless_compare(command_name, CMD_HDL_INJECT_CD) ||
                 caseless_compare(command_name, CMD_HDL_INJECT_DVD)) { /* inject game image into a new HDL partition */
            int slice_index = -1;
            compat_flags_t compat_flags = COMPAT_FLAGS_INVALID;
            unsigned short dma = 0;
            const char *startup = NULL;
            int is_dvd =
                caseless_compare(command_name, CMD_HDL_INJECT_CD) ? 0 : 1;
            int is_hidden = 0; /* Games are visible by default */
            int i;

            if (!(argc >= 5 && argc <= 10))
                show_usage_and_exit(argv[0], command_name);

            compat_flags = parse_compat_flags("0x00");
            for (i = 5; i < argc; ++i) {
                if (argv[i][0] == '@')
                    /* slice index */
                    slice_index = (int)strtoul(argv[i] + 1, NULL, 10) - 1;
                else if (argv[i][0] == '+' ||
                         (argv[i][0] == '0' && argv[i][1] == 'x'))
                    /* compatibility flags */
                    compat_flags = parse_compat_flags(argv[i]);
                else if (argv[i][0] == '-' && argv[i][1] == 'h' && argv[i][2] == 'i')
                    /* assume it's the -hide switch */
                    is_hidden = 1;
                else if (argv[i][0] == '*')
                    /* dma modes */
                    dma = parse_dma(argv[i]);
                else
                    /* startup file */
                    startup = argv[i];
            }

            if (dma == 0) {
                const char *df_dma = dict_lookup(config, CONFIG_DEFAULT_DMA);
                if (df_dma == NULL)
                    df_dma = "*u4";
                dma = parse_dma(df_dma);
            }

            if (compat_flags == COMPAT_FLAGS_INVALID ||
                !(slice_index >= -1 && slice_index <= 1) || (dma == 0)) {
                show_usage_and_exit(argv[0], command_name);
            }

            handle_result_and_exit(inject(config, argv[2], argv[3], argv[4],
                                          startup, compat_flags, dma, is_dvd,
                                          is_hidden, slice_index, get_progress()),
                                   argv[2], argv[3]);
        }

        else if (caseless_compare(command_name, CMD_HDL_INSTALL)) {
            int slice_index = -1;
            int is_hidden = 0; /* Games are visible by default */
            int i;

            if (!(argc >= 4 && argc <= 6))
                show_usage_and_exit(argv[0], CMD_HDL_INSTALL);

            for (i = 4; i < argc; ++i) {
                if (argv[i][0] == '@')
                    /* slice index */
                    slice_index = (int)strtoul(argv[i] + 1, NULL, 10) - 1;
                else if (argv[i][0] == '-' && argv[i][1] == 'h' && argv[i][2] == 'i')
                    /* assume it's the -hide switch */
                    is_hidden = 1;
                else
                    show_usage_and_exit(argv[0], CMD_HDL_INSTALL);
            }

            handle_result_and_exit(install(config, argv[2], argv[3], slice_index,
                                           is_hidden, get_progress()),
                                   argv[2], argv[3]);
        }

#if defined(INCLUDE_CUTOUT_CMD)
        else if (caseless_compare(command_name, CMD_CUTOUT)) { /* calculate and display how to arrange a new HDL partition */
            int slice_index = -1;
            if ((argc != 4 && argc != 5) || (argc == 5 && argv[4][0] != '@'))
                show_usage_and_exit(argv[0], CMD_CUTOUT);

            if (argc == 5)
                slice_index = (int)strtol(argv[4] + 1, NULL, 10) - 1;
            if (!(slice_index >= -1 && slice_index <= 1))
                show_usage_and_exit(argv[0], CMD_CUTOUT);

            handle_result_and_exit(show_apa_cut_out_for_inject(config, argv[2],
                                                               slice_index,
                                                               atoi(argv[3])),
                                   argv[2], NULL);
        }
#endif /* INCLUDE_CUTOUT_CMD defined? */

        else if (caseless_compare(command_name, CMD_CDVD_INFO)) { /* try to display startup file and volume label for an iin */
            if (argc != 3)
                show_usage_and_exit(argv[0], CMD_CDVD_INFO);

            handle_result_and_exit(cdvd_info(config, argv[2], 0, stdout),
                                   argv[2], NULL);
        }

        else if (caseless_compare(command_name, CMD_CDVD_INFO2)) { /* try to display startup file and volume label for an iin */
            if (argc != 3)
                show_usage_and_exit(argv[0], CMD_CDVD_INFO);

            handle_result_and_exit(cdvd_info(config, argv[2], 1, stdout),
                                   argv[2], NULL);
        }

        else if (caseless_compare(command_name, CMD_POWER_OFF)) { /* PS2 power-off */
            if (argc != 3)
                show_usage_and_exit(argv[0], CMD_POWER_OFF);

            handle_result_and_exit(remote_poweroff(config, argv[2]),
                                   argv[2], NULL);
        }

#if defined(INCLUDE_INITIALIZE_CMD)
        else if (caseless_compare(command_name, CMD_INITIALIZE)) { /* prepare a HDD for HDL usage */
            if (argc != 4)
                show_usage_and_exit(argv[0], CMD_INITIALIZE);

            handle_result_and_exit(apa_initialize(config, argv[2], argv[3]),
                                   argv[2], NULL);
        }
#endif /* INCLUDE_INITIALIZE_CMD defined? */

#if defined(INCLUDE_DUMP_MBR_CMD)
        else if (caseless_compare(command_name, CMD_DUMP_MBR)) {
            if (argc != 4)
                show_usage_and_exit(argv[0], CMD_DUMP_MBR);

            handle_result_and_exit(apa_dump_mbr(config, argv[2], argv[3]),
                                   argv[2], NULL);
        }
#endif /* INCLUDE_DUMP_MBR_CMD defined? */

#if defined(INCLUDE_BACKUP_TOC_CMD)
        else if (caseless_compare(command_name, CMD_BACKUP_TOC)) {
            if (argc != 4)
                show_usage_and_exit(argv[0], CMD_BACKUP_TOC);
            handle_result_and_exit(backup_toc(config, argv[2], argv[3]),
                                   argv[2], NULL);
        }
#endif /* INCLUDE_BACKUP_TOC_CMD defined? */

#if defined(INCLUDE_RESTORE_TOC_CMD)
        else if (caseless_compare(command_name, CMD_RESTORE_TOC)) {
            if (argc != 4)
                show_usage_and_exit(argv[0], CMD_RESTORE_TOC);
            handle_result_and_exit(restore_toc(config, argv[2], argv[3]),
                                   argv[2], NULL);
        }
#endif /* INCLUDE_RESTORE_TOC_CMD defined? */

#if defined(INCLUDE_DIAG_CMD)
        else if (caseless_compare(command_name, CMD_DIAG)) {
            if (argc != 3)
                show_usage_and_exit(argv[0], CMD_DIAG);
            handle_result_and_exit(diag(config, argv[2]), argv[2], NULL);
        }
#endif /* INCLUDE_DIAG_CMD defined? */

#if defined(INCLUDE_MODIFY_CMD)
        else if (caseless_compare(command_name, CMD_MODIFY)) {
            const char *new_name = NULL;
            compat_flags_t new_flags = COMPAT_FLAGS_INVALID;
            unsigned short new_dma = 0;
            int is_hidden = -1;
            int i;

            if (!(argc >= 5 && argc <= 8))
                show_usage_and_exit(argv[0], CMD_MODIFY);

            for (i = 4; i < argc; ++i) {
                if (argv[i][0] == '+' ||
                    (argv[i][0] == '0' && argv[i][1] == 'x'))
                    /* compatibility flags */
                    new_flags = parse_compat_flags(argv[i]);
                else if (argv[i][0] == '-') {
                    /* switches */
                    if (argv[i][1] == 'h' && argv[i][2] == 'i')
                        /* assume it's the -hide switch */
                        is_hidden = 1;
                    else if (argv[i][1] == 'u' && argv[i][2] == 'n')
                        /* assume it's the -unhide switch */
                        is_hidden = 0;
                } else if (argv[i][0] == '*')
                    /* dma modes */
                    new_dma = parse_dma(argv[i]);
                else
                    /* new name */
                    new_name = argv[i];
            }

            if (new_name != NULL && caseless_compare(argv[3], new_name))
                new_name = NULL; /* new name is same as the present one */

            if (new_name == NULL && new_dma == 0 &&
                new_flags == COMPAT_FLAGS_INVALID && is_hidden == -1)
                show_usage_and_exit(argv[0], CMD_MODIFY); /* Nothing was modified */

            handle_result_and_exit(modify(config, argv[2], argv[3], new_name,
                                          new_flags, new_dma, is_hidden),
                                   argv[2], argv[3]);
        }
#endif /* INCLUDE_MODIFY_CMD defined? */
        else if (caseless_compare(command_name, CMD_MODIFY_HEADER)) {
            if (argc != 4)
                show_usage_and_exit(argv[0], CMD_MODIFY_HEADER);
            handle_result_and_exit(modify_header(config, argv[2], argv[3]), argv[2], argv[3]);
        }

#if defined(INCLUDE_COPY_HDD_CMD)
        else if (caseless_compare(command_name, CMD_COPY_HDD)) {
            if (argc != 4 && argc != 5)
                show_usage_and_exit(argv[0], CMD_COPY_HDD);
            handle_result_and_exit(copy_hdd(config, argv[2], argv[3],
                                            argc == 5 ? argv[4] : NULL,
                                            get_progress()),
                                   argv[2], NULL);
        }
#endif /* INCLUDE_COPY_HDD_CMD defined? */

        else { /* something else... -h perhaps? */
            show_usage_and_exit(argv[0], command_name);
        }
    } else {
        show_usage_and_exit(argv[0], NULL);
    }
    return (0); /* please compiler */
}
