/*
 * apa.c
 * $Id: apa.c,v 1.18 2007-05-12 20:13:29 bobi Exp $
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

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "byteseq.h"
#include "retcodes.h"
#include "common.h"
#include "ps2_hdd.h"
#include "osal.h"
#include "apa.h"
#include "hio.h"


#define AUTO_DELETE_EMPTY


#define _MB *(1024 * 1024) /* really ugly :-) */

const char *PPAA_MAGIC =
    "PS2ICON3D";

typedef struct ps2_partition_run_type
{
    u_int32_t sector;
    u_int32_t size_in_mb;
} ps2_partition_run_t;


static int apa_check(const apa_toc_t *toc);

static int apa_check_slice(const apa_slice_t *slice);

static int apa_delete_partition_from_slice(apa_slice_t *slice,
                                           const char *partition_name);

/**************************************************************/
u_int32_t
apa_partition_checksum(const ps2_partition_header_t *part)
{
    const u_int32_t *p = (const u_int32_t *)part;
    register u_int32_t i;
    u_int32_t sum = 0;
    for (i = 1; i < 256; ++i)
        sum += get_u32(p + i);
    return (sum);
}


/**************************************************************/
int /* RET_OK, RET_NOT_APA, RET_ERR */
is_apa_partitioned(osal_handle_t handle)
{
    ps2_partition_header_t part;
    u_int32_t bytes;

    int result = osal_read(handle, &part, sizeof(part), &bytes);
    if (result == OSAL_OK) {
        if (bytes == sizeof(part) &&
            memcmp(part.magic, PS2_PARTITION_MAGIC, 4) == 0 &&
            get_u32(&part.checksum) == apa_partition_checksum(&part))
            return (RET_OK); /* APA */
        else
            return (RET_NOT_APA); /* not APA */
    } else
        return (result); /* error */
}


/**************************************************************/
static void
apa_slice_free(apa_slice_t *slice)
{
    if (slice->chunks_map != NULL)
        osal_free(slice->chunks_map);
    slice->chunks_map = NULL;
    if (slice->parts != NULL)
        osal_free(slice->parts);
    slice->parts = NULL;
}


/**************************************************************/
void apa_toc_free(/*@only@*/ apa_toc_t *toc) /*@releases toc@*/
{
    if (toc != NULL) {
        apa_slice_free(toc->slice + 0);
        apa_slice_free(toc->slice + 1);
        osal_free(toc);
    }
}


/**************************************************************/
static int
apa_part_add(/*@special@*/ apa_slice_t *slice,
             const ps2_partition_header_t *part,
             int existing,
             int linked)
/*@uses slice->part_count, slice->part_alloc_, slice->parts@*/
/*@modifies slice->part_count, slice->part_alloc_, slice->parts@*/
/*@sets *slice->parts@*/
{
    if (slice->part_count == slice->part_alloc_) { /* grow buffer */
        const u_int32_t GROW = 200;
        u_int32_t bytes = (slice->part_alloc_ + GROW) * sizeof(apa_partition_t);
        apa_partition_t *tmp = osal_alloc(bytes);
        if (tmp != NULL) {
            memset(tmp, 0, bytes);
            if (slice->parts != NULL) { /* copy existing */
                memcpy(tmp, slice->parts,
                       slice->part_count * sizeof(apa_partition_t));
                osal_free(slice->parts);
            }
            slice->parts = tmp;
            slice->part_alloc_ += GROW;
        } else
            return (RET_NO_MEM);
    }

    assert(slice->parts != NULL);
    if (slice->parts == NULL)
        return (RET_INVARIANT);
    memcpy(&slice->parts[slice->part_count].header, part,
           sizeof(ps2_partition_header_t));
    slice->parts[slice->part_count].existing = existing;
    slice->parts[slice->part_count].modified = !existing;
    slice->parts[slice->part_count].linked = linked;
    ++slice->part_count;
    return (RET_OK);
}


/**************************************************************/
static int
apa_setup_statistics(/*@special@*/ apa_slice_t *slice)
/*@uses slice->size_in_mb, slice->part_count, slice->parts@*/
/*@sets slice->total_chunks, slice->allocated_chunks,
       slice->free_chunks, slice->chunks_map, *slice->chunks_map@*/
{

    char *map;

    slice->total_chunks = slice->size_in_mb / 128;
    map = osal_alloc(slice->total_chunks * sizeof(char));
    if (map != NULL) {
        u_int32_t i;
        *map = MAP_AVAIL;
        for (i = 0; i < slice->total_chunks; ++i)
            map[i] = MAP_AVAIL;

        /* build occupided/available space map */
        slice->allocated_chunks = 0;
        slice->free_chunks = slice->total_chunks;
        for (i = 0; i < slice->part_count; ++i) {
            const ps2_partition_header_t *part = &slice->parts[i].header;
            u_int32_t part_no = get_u32(&part->start) / ((128 _MB) / 512);
            u_int32_t num_parts = get_u32(&part->length) / ((128 _MB) / 512);

            /* "alloc" num_parts starting at part_no */
            while (num_parts) {
                if (map[part_no] == MAP_AVAIL)
                    map[part_no] = (get_u32(&part->main) == 0 ?
                                        MAP_MAIN :
                                        MAP_SUB);
                else
                    map[part_no] = MAP_COLL; /* collision */
                ++part_no;
                --num_parts;
                ++slice->allocated_chunks;
                --slice->free_chunks;
            }
        }

        if (slice->chunks_map != NULL)
            osal_free(slice->chunks_map);
        slice->chunks_map = map;

        return (RET_OK);
    } else
        return (RET_NO_MEM);
}


/**************************************************************/
int apa_toc_read(const dict_t *config,
                 const char *path,
                 apa_toc_p_t *toc)
{
    /*@only@*/ hio_t *hio = NULL;
    int result;
    *toc = NULL;
    result = hio_probe(config, path, &hio);
    if (result == OSAL_OK && hio != NULL) {
        result = apa_toc_read_ex(hio, toc);
        (void)hio->close(hio), hio = NULL;
    }
    return (result);
}


/**************************************************************/
static int
apa_slice_read(hio_t *hio,
               /*@special@*/ apa_toc_t *toc,
               int slice_index,
               int raw)
/*@uses toc->got_2nd_slice,toc->size_in_kb@*/
/*@sets toc->slice[slice_index]@*/
{
    const u_int32_t EXACTLY_128MB = 128 * 1024 * 1024; /* KB */
    const u_int32_t ALMOST_128MB = EXACTLY_128MB - 1;  /* KB */
    const u_int32_t SLICE_2_OFFS = 0x10000000;         /* sectors */
    apa_slice_t *slice = toc->slice + slice_index;
    int result;
    u_int32_t total_sectors;
    u_int32_t bytes;
    ps2_partition_header_t part;
    u_int32_t sector = 0;
    u_int32_t count = 0;

    /* calculate total number of sectors for the requested slice */
    if (toc->got_2nd_slice)
        if (slice_index == 0)
            total_sectors = (toc->size_in_kb < ALMOST_128MB ?
                                 toc->size_in_kb :
                                 ALMOST_128MB) *
                            2; /* 1st */
        else
            total_sectors = (toc->size_in_kb - EXACTLY_128MB) * 2; /* 2nd */
    else
        total_sectors = toc->size_in_kb * 2; /* the one and only */

    slice->slice_index = slice_index;
    slice->size_in_mb = total_sectors / 2048;

    slice->total_chunks = slice->allocated_chunks = slice->free_chunks = 0;
    osal_free(slice->chunks_map), slice->chunks_map = NULL;

    slice->part_alloc_ = 0;
    slice->part_count = 0;
    osal_free(slice->parts), slice->parts = NULL;

    do { /* read partitions */
        result = hio->read(hio, sector + SLICE_2_OFFS * slice_index, 2,
                           &part, &bytes);
        if (result == RET_OK &&
            bytes == sizeof(part)) {
            if (memcmp(part.magic, PS2_PARTITION_MAGIC, 4) != 0)
                result = RET_NOT_APA;
            if (result == RET_OK &&
                get_u32(&part.checksum) == apa_partition_checksum(&part)) { /* NOTE: start should be the same as sector? */
                u_int32_t start = get_u32(&part.start);
                if (start < total_sectors &&
                    start + get_u32(&part.length) < total_sectors) {
                    ++count;
                    result = apa_part_add(slice, &part, 1, 1);
                    if (result == RET_OK)
                        sector = get_u32(&part.next);
                } else { /* partition behind end-of-HDD */
                    if (toc->got_2nd_slice)
                        result = RET_CROSS_128GB; /* data behind 128GB mark */
                    else
                        result = RET_BAD_APA; /* data behind end-of-HDD */
                    break;
                }

                /* TODO: check whether next partition is not loaded already --
                 * do not deadlock; that is a quick-and-dirty hack */
                if (slice->part_count > 10000)
                    result = RET_BAD_APA;
            } else
                result = (result == RET_OK ? RET_BAD_APA : result);
        } else
            result = RET_NOT_APA;
    } while (result == RET_OK && sector != 0);

    if (!raw) {
        if (count == 0)
            /* no partitions? __boot should always exists */
            result = RET_BAD_APA;

        if (result == RET_OK) {
            result = apa_setup_statistics(slice);
            if (result == RET_OK)
                result = apa_check_slice(slice);

#if defined(AUTO_DELETE_EMPTY)
            /* automatically remove "__empty" partitions */
            while (result == RET_OK)
                result = apa_delete_partition_from_slice(slice, "__empty");
            if (result == RET_NOT_FOUND)
                result = apa_check_slice(slice);
#endif
        }
    }

    return (result);
}


/**************************************************************/
static int
apa_toc_read_internal(hio_t *hio,
                      /*@special@*/ apa_toc_p_t *toc2,
                      int raw) /*@allocates *toc2@*/ /*@defines *toc2@*/
{
    u_int32_t size_in_kb;
    int result = hio->stat(hio, &size_in_kb);
    if (result == OSAL_OK) {
        /*@only@*/ apa_toc_t *toc = (apa_toc_t *)osal_alloc(sizeof(apa_toc_t));
        if (toc != NULL) {
            u_int32_t bytes;
            ps2_partition_header_t part;

            /* read MBR to auto-detect partition dialect */
            memset(toc, 0, sizeof(apa_toc_t));
            result = hio->read(hio, 0, 2, &part, &bytes);
            if (result == RET_OK && bytes == 1024) {
                toc->size_in_kb = size_in_kb;
                toc->is_toxic =
                    (memcmp(part.mbr.toxic_magic, "APAEXT\0\0", 8) == 0);
                toc->is_2_slice = 0;
                toc->got_2nd_slice = 0;
                if (toc->is_toxic) {
                    toc->is_2_slice = (part.mbr.toxic_flags & 0x01) != 0;
                    if (toc->is_2_slice)
                        toc->got_2nd_slice = (size_in_kb > 128 * 1024 * 1024);
                }

                if (result == RET_OK)
                    result = apa_slice_read(hio, toc, 0, raw); /* only or 1st */
                if (result == RET_OK &&
                    toc->got_2nd_slice)
                    result = apa_slice_read(hio, toc, 1, raw); /* 2nd */
            }

            if (result == RET_OK)
                *toc2 = toc;
            else
                apa_toc_free(toc), toc = NULL;
        } else
            result = RET_NO_MEM;
    }
    return (result);
}


/**************************************************************/
int apa_toc_read_ex(hio_t *hio,
                    apa_toc_p_t *toc)
{
    /*@only@*/ apa_toc_t *tmp = NULL;
    int result = apa_toc_read_internal(hio, &tmp, 0);
    if (result == RET_OK)
        *toc = tmp;
    return (result);
}


/**************************************************************/
static int
apa_find_partition_in_slice(const apa_slice_t *slice,
                            const char *partition_name,
                            /*@out@*/ u_int32_t *partition_index)
{
    u_int32_t i;
    int result = RET_NOT_FOUND;
    *partition_index = (u_int32_t)-1;
    for (i = 0; i < slice->part_count; ++i) {
        const ps2_partition_header_t *part = &slice->parts[i].header;
        if (get_u32(&part->main) == 0) { /* trim partition name */
            char id_copy[PS2_PART_IDMAX + 1], *part_id_end;
            memcpy(id_copy, part->id, PS2_PART_IDMAX);
            id_copy[PS2_PART_IDMAX] = '\0';
            part_id_end = id_copy + PS2_PART_IDMAX - 1;
            while (part_id_end > id_copy && *part_id_end == ' ')
                *part_id_end-- = '\0';
            if (caseless_compare(id_copy, partition_name)) { /* found */
                *partition_index = i;
                result = RET_OK;
                break;
            }
        }
    }
    return (result);
}


/**************************************************************/
int apa_find_partition(const apa_toc_t *toc,
                       const char *partition_name,
                       int *slice_index,
                       u_int32_t *partition_index)
{
    int result = apa_find_partition_in_slice(toc->slice + 0, partition_name,
                                             partition_index);
    *slice_index = 0;
    if (result == RET_NOT_FOUND && toc->got_2nd_slice) {
        result = apa_find_partition_in_slice(toc->slice + 1, partition_name,
                                             partition_index);
        if (result == RET_OK)
            *slice_index = 1;
    }
    return (result);
}


/**************************************************************/
static int
compare_partitions(const void *e1,
                   const void *e2)
{
    const ps2_partition_run_t *p1 = e1;
    const ps2_partition_run_t *p2 = e2;
    int diff = (int)p2->size_in_mb - (int)p1->size_in_mb;
    if (diff != 0)
        return (diff);
    else
        return ((int)p1->sector - (int)p2->sector);
}


/* descending by size, ascending by sector */
static void
sort_partitions(ps2_partition_run_t *partitions,
                u_int32_t partitions_used)
{
    /* TODO: consider better sorting to take care about partitioning on as few runs as possible */
    qsort(partitions, partitions_used, sizeof(ps2_partition_run_t), &compare_partitions);
}


/* join neighbour partitions of the same size, but sum up to max_part_size_in_mb */
static void
optimize_partitions(ps2_partition_run_t *partitions,
                    u_int32_t *partitions_used,
                    u_int32_t max_part_size_in_mb)
{
    int have_join;
    do {
        u_int32_t i;
        have_join = 0;
        for (i = 0; i < *partitions_used - 1; ++i) {
            u_int32_t curr_part_end_sector =
                partitions[i].sector +
                partitions[i].size_in_mb * ((1 _MB) / 512);
            u_int32_t u_int32_to_be = partitions[i].size_in_mb * 2;
            int next_is_same_size =
                partitions[i].size_in_mb == partitions[i + 1].size_in_mb;
            int would_be_aligned =
                partitions[i].sector % (u_int32_to_be * ((1 _MB) / 512)) == 0;

            if (u_int32_to_be <= max_part_size_in_mb &&
                curr_part_end_sector == partitions[i + 1].sector &&
                next_is_same_size &&
                would_be_aligned) {
                partitions[i].size_in_mb *= 2;
                memmove(partitions + i + 1, partitions + i + 2,
                        (*partitions_used - i - 2) * sizeof(ps2_partition_run_t));
                --(*partitions_used);
                have_join = 1;
            }
        }
    } while (have_join);
}


/**************************************************************/
static void
set_ps2fs_datetime(/*@out@*/ ps2fs_datetime_t *dt,
                   time_t to)
{
    const struct tm *tm = localtime(&to);
    if (tm != NULL) {
        dt->unused = 0;
        dt->sec = (u_int8_t)tm->tm_sec;
        dt->min = (u_int8_t)tm->tm_min;
        dt->hour = (u_int8_t)tm->tm_hour;
        dt->day = (u_int8_t)tm->tm_mday;
        dt->month = (u_int8_t)(tm->tm_mon + 1);
        set_u16(&dt->year, (u_int16_t)(tm->tm_year + 1900));
    } else { /* highly unlikely */
        dt->unused = 0;
        dt->sec = dt->min = dt->hour = 0;
        dt->day = 1;
        dt->month = 1;
        set_u16(&dt->year, 2005);
    }
}


static void
setup_main_part(/*@out@*/ ps2_partition_header_t *part,
                const char *name,
                const ps2_partition_run_t *partitions,
                u_int32_t partitions_used,
                u_int32_t last_partition_sector)
{
    u_int32_t i;
    memset(part, 0, sizeof(ps2_partition_header_t));
    memcpy(part->magic, PS2_PARTITION_MAGIC, 4);
    set_u32(&part->next, partitions_used > 0 ? partitions[1].sector : 0);
    set_u32(&part->prev, last_partition_sector);
    memmove(part->id, name,
            strlen(name) > PS2_PART_IDMAX ? PS2_PART_IDMAX : strlen(name));
    set_u32(&part->start, partitions[0].sector);
    set_u32(&part->length, partitions[0].size_in_mb * ((1 _MB) / 512));
    set_u16(&part->type, PS2_HDL_PARTITION);
    set_u16(&part->flags, 0);
    set_u32(&part->nsub, partitions_used - 1);
    set_ps2fs_datetime(&part->created, time(NULL));
    set_u32(&part->main, 0);
    set_u32(&part->number, 0);
    set_u32(&part->modver, 0x201);
    for (i = 1; i < partitions_used; ++i) {
        set_u32(&part->subs[i - 1].start, partitions[i].sector);
        set_u32(&part->subs[i - 1].length, partitions[i].size_in_mb * ((1 _MB) / 512));
    }
    set_u32(&part->checksum, apa_partition_checksum(part));
}


static void
setup_sub_part(ps2_partition_header_t *part,
               u_int32_t index,
               const ps2_partition_run_t *partitions,
               u_int32_t partitions_used)
{
    memset(part, 0, sizeof(ps2_partition_header_t));
    memcpy(part->magic, PS2_PARTITION_MAGIC, 4);
    set_u32(&part->next, index + 1 < partitions_used ? partitions[index + 1].sector : 0);
    set_u32(&part->prev, partitions[index - 1].sector);
    set_u32(&part->start, partitions[index].sector);
    set_u32(&part->length, partitions[index].size_in_mb * ((1 _MB) / 512));
    set_u16(&part->type, PS2_HDL_PARTITION);
    set_u16(&part->flags, PS2_PART_FLAG_SUB);
    set_u32(&part->nsub, 0);
    set_ps2fs_datetime(&part->created, time(NULL));
    set_u32(&part->main, partitions[0].sector);
    set_u32(&part->number, index);
    set_u32(&part->modver, 0x201);
    set_u32(&part->checksum, apa_partition_checksum(part));
}


/**************************************************************/
static int
sort_by_starting_sector(const void *e1,
                        const void *e2)
{
    const apa_partition_t *p1 = (const apa_partition_t *)e1;
    const apa_partition_t *p2 = (const apa_partition_t *)e2;
    return (get_u32(&p1->header.start) > get_u32(&p2->header.start) ? 1 : -1);
}

static void
normalize_linked_list(apa_slice_t *slice)
{
    qsort(slice->parts, slice->part_count, sizeof(apa_partition_t), sort_by_starting_sector);
    if (slice->part_count >= 1) {
        u_int32_t i;
        for (i = 0; i < slice->part_count; ++i) {
            apa_partition_t *prev = slice->parts + (i > 0 ? i - 1 : slice->part_count - 1);
            apa_partition_t *curr = slice->parts + i;
            apa_partition_t *next = slice->parts + (i + 1 < slice->part_count ? i + 1 : 0);
            /* no need to be endian-aware, since both have same endianess */
            if (curr->header.prev != prev->header.start) {
                curr->modified = 1;
                curr->header.prev = prev->header.start;
            }
            if (curr->header.next != next->header.start) {
                curr->modified = 1;
                curr->header.next = next->header.start;
            }
            if (curr->modified)
                set_u32(&curr->header.checksum, apa_partition_checksum(&curr->header));
        }
    }
}


/**************************************************************/
static int
apa_allocate_space_in_slice(apa_slice_t *slice,
                            const char *partition_name,
                            u_int32_t size_in_mb,
                            /*@out@*/ u_int32_t *new_partition_start,
                            int decreasing_size)
{
    int result = RET_OK;
    char *map = slice->chunks_map;


    *new_partition_start = (u_int32_t)-1;
    if (size_in_mb == 0)
        return (RET_INVARIANT);

    if (slice->free_chunks * 128 >= size_in_mb) {
        u_int32_t max_part_size_in_entries =
            slice->total_chunks < 32 ? 1 : slice->total_chunks / 32;
        u_int32_t max_part_size_in_mb = max_part_size_in_entries * 128;
        u_int32_t estimated_entries = (size_in_mb + 127) / 128 + 1;
        u_int32_t partitions_used = 0;
        ps2_partition_run_t *partitions =
            osal_alloc(estimated_entries * sizeof(ps2_partition_run_t));
        if (partitions != NULL) {
            /* use the most straight forward approach possible:
         fill from the first gap onwards */
            u_int32_t mb_remaining = size_in_mb;
            u_int32_t allocated_mb, overhead_mb, i;
            partitions->sector = partitions->size_in_mb = 0;
            for (i = 0; i < estimated_entries; ++i) { /* initialize */
                partitions[i].sector = 0;
                partitions[i].size_in_mb = 0;
            }
            for (i = 0; mb_remaining > 0 && i < slice->total_chunks; ++i)
                if (map[i] == MAP_AVAIL) {
                    partitions[partitions_used].sector = i * ((128 _MB) / 512);
                    partitions[partitions_used].size_in_mb = 128;
                    map[i] = MAP_ALLOC; /* "allocate" chunk */
                    ++partitions_used;
                    mb_remaining = (mb_remaining > 128 ? mb_remaining - 128 : 0);
                }

            optimize_partitions(partitions, &partitions_used,
                                max_part_size_in_mb);

            /* calculate overhead (4M for main + 1M for each sub)
         and allocate additional 128 M partition if necessary */
            allocated_mb = 0;
            overhead_mb = 3;
            for (i = 0; i < partitions_used; ++i) {
                allocated_mb += partitions[i].size_in_mb;
                ++overhead_mb;
            }
            if (allocated_mb < size_in_mb + overhead_mb) { /* add one more partition or return RET_NO_SPACE */
                int free_entry_found = 0;
                for (i = 0; i < slice->total_chunks; ++i)
                    if (map[i] == MAP_AVAIL) {
                        partitions[partitions_used].sector =
                            i * ((128 _MB) / 512);
                        partitions[partitions_used].size_in_mb = 128;
                        ++partitions_used;
                        optimize_partitions(partitions, &partitions_used,
                                            max_part_size_in_mb);

                        free_entry_found = 1;
                        break;
                    }
                result = free_entry_found ? RET_OK : RET_NO_SPACE;
            } else
                result = RET_OK;

            if (result == RET_OK) { /* create new partitions in the partition slice */
                ps2_partition_header_t part;
                u_int32_t last_partition_start =
                    slice->part_count > 0 ?
                        get_u32(&slice->parts[slice->part_count - 1].header.start) :
                        0;

                if (decreasing_size)
                    sort_partitions(partitions, partitions_used);

                /* current last partition would be remapped by normalize */
                setup_main_part(&part, partition_name, partitions,
                                partitions_used, last_partition_start);
                result = apa_part_add(slice, &part, 0, 1);
                for (i = 1; result == RET_OK && i < partitions_used; ++i) {
                    setup_sub_part(&part, i, partitions, partitions_used);
                    result = apa_part_add(slice, &part, 0, 1);
                }
                if (result == RET_OK) {
                    normalize_linked_list(slice);
                    *new_partition_start = partitions[0].sector;
                }
            }
            osal_free(partitions);
        } else
            result = RET_NO_MEM; /* out-of-memory */
    } else
        result = RET_NO_SPACE; /* not enough free space */

    return (result);
}


/**************************************************************/
int apa_allocate_space(apa_toc_t *toc,
                       const char *partition_name,
                       u_int32_t size_in_mb,
                       int *slice_index,
                       u_int32_t *new_partition_start,
                       int decreasing_size)
{
    int result;
    int tmp_slice_index = 0;
    u_int32_t tmp_partition_index = 0;

    /* check if such name exists */
    result = apa_find_partition(toc, partition_name, &tmp_slice_index,
                                &tmp_partition_index);
    if (result == RET_OK)
        return (RET_PART_EXISTS);

    if (!(*slice_index == 0 || (*slice_index == 1 && toc->got_2nd_slice)))
        *slice_index = (toc->got_2nd_slice ? 1 : 0); /* try in 2nd first */
    result = apa_allocate_space_in_slice(toc->slice + *slice_index,
                                         partition_name, size_in_mb,
                                         new_partition_start, decreasing_size);
    if (result == RET_OK)
        ;
    else if (toc->got_2nd_slice) {
        *slice_index = (*slice_index == 0 ? 1 : 0);
        result = apa_allocate_space_in_slice(toc->slice + *slice_index,
                                             partition_name, size_in_mb,
                                             new_partition_start,
                                             decreasing_size);
    }
    return (result);
}


/**************************************************************/
static int
apa_delete_partition_from_slice(apa_slice_t *slice,
                                const char *partition_name)
{
    u_int32_t partition_index;
    int result = apa_find_partition_in_slice(slice, partition_name,
                                             &partition_index);
    if (result == RET_OK) {
        u_int32_t i, count = 1;
        u_int32_t pending_deletes[PS2_PART_MAXSUB]; /* starting sectors of parts pending delete */
        const ps2_partition_header_t *part =
            &slice->parts[partition_index].header;

        if (get_u16(&part->type) == 1)
            return (RET_NOT_ALLOWED); /* deletion of system partitions is not allowed */

        /* preserve a list of partitions to be deleted */
        pending_deletes[0] = get_u32(&part->start);
        for (i = 0; i < get_u32(&part->nsub); ++i)
            pending_deletes[count++] = get_u32(&part->subs[i].start);

        /* remove partitions from the double-linked list */
        i = 0;
        while (i < slice->part_count) {
            u_int32_t j;
            int found = 0;
            for (j = 0; j < count; ++j)
                if (get_u32(&slice->parts[i].header.start) == pending_deletes[j]) {
                    found = 1;
                    break;
                }
            if (found) {                                                             /* remove this partition */
                u_int32_t part_no = get_u32(&slice->parts[i].header.start) / 262144; /* 262144 sectors == 128M */
                u_int32_t num_parts = get_u32(&slice->parts[i].header.length) / 262144;

                memmove(slice->parts + i, slice->parts + i + 1,
                        sizeof(apa_partition_t) * (slice->part_count - i - 1));
                --slice->part_count;

                /* "free" num_parts starting at part_no */
                while (num_parts) {
                    slice->chunks_map[part_no] = MAP_AVAIL;
                    ++part_no;
                    --num_parts;
                    --slice->allocated_chunks;
                    ++slice->free_chunks;
                }
            } else
                ++i;
        }

        normalize_linked_list(slice);
    }
    return (result);
}


/**************************************************************/
int apa_delete_partition(apa_toc_t *toc,
                         const char *partition_name)
{
    int result = apa_delete_partition_from_slice(toc->slice + 0,
                                                 partition_name);
    if (result == RET_NOT_FOUND)
        result = apa_delete_partition_from_slice(toc->slice + 1,
                                                 partition_name);
    return (result);
}


/**************************************************************/
int apa_commit(const dict_t *config,
               const char *path,
               const apa_toc_t *toc)
{
    int result = apa_check(toc);
    if (result == RET_OK) {
        /*@only@*/ hio_t *hio = NULL;
        result = hio_probe(config, path, &hio);
        if (result == OSAL_OK && hio != NULL) {
            result = apa_commit_ex(hio, toc);
            if (result == RET_OK) {
                (void)hio->flush(hio);
                result = hio->close(hio);
            } else
                (void)hio->close(hio); /* ignore close error in this case */
            hio = NULL;
        }
    }
    return (result);
}


/**************************************************************/
static int
apa_commit_slice(hio_t *hio,
                 const apa_toc_t *toc,
                 int slice_index)
{
    const apa_slice_t *slice = toc->slice + slice_index;
    int result = apa_check_slice(slice);
    if (result == RET_OK) {
        u_int32_t i;
        for (i = 0; result == RET_OK && i < slice->part_count; ++i) {
            if (slice->parts[i].modified) {
                const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
                u_int32_t bytes;
                const ps2_partition_header_t *part = &slice->parts[i].header;
                u_int32_t sector = (get_u32(&part->start) +
                                    SLICE_2_OFFS * slice_index);
                result = hio->write(hio, sector, 2, part, &bytes);
                if (result == OSAL_OK)
                    result = (bytes == sizeof(ps2_partition_header_t) ?
                                  OSAL_OK :
                                  RET_ERR);
            }
        }
    }
    return (result);
}


/**************************************************************/
int apa_commit_ex(hio_t *hio,
                  const apa_toc_t *toc)
{
    int result = apa_check(toc);
    if (result == RET_OK) {
        result = apa_commit_slice(hio, toc, 0);
        if (result == RET_OK &&
            toc->got_2nd_slice)
            result = apa_commit_slice(hio, toc, 1);
    }
    return (result);
}


/**************************************************************/
#define ADD_PROBLEM(buff, size, problem, len) \
    if ((len) < (size)) {                     \
        memcpy(buff, problem, len);           \
        (size) -= (len);                      \
        (buff) += (len);                      \
        *(buff) = '\0';                       \
    }

static int
apa_list_problems(const apa_slice_t *slice,
                  /*@out@*/ char *buffer,
                  size_t buffer_size)
{ /* NOTE: keep in sync with apa_check */
    u_int32_t i, j, k;
    char tmp[1024];
    size_t len;

    const u_int32_t total_sectors = slice->size_in_mb * 1024 * 2;

    *buffer = '\0';
    for (i = 0; i < slice->part_count; ++i) {
        const ps2_partition_header_t *part = &slice->parts[i].header;
        u_int32_t checksum = apa_partition_checksum(part);
        if (get_u32(&part->checksum) != checksum) {
            len = sprintf(tmp, "%06lx00: bad checksum: 0x%08lx instead of 0x%08lx;\n",
                          (unsigned long)(get_u32(&part->start) >> 8),
                          (unsigned long)get_u32(&part->checksum),
                          (unsigned long)checksum);
            ADD_PROBLEM(buffer, buffer_size, tmp, len);
        }

        if (get_u32(&part->start) < total_sectors &&
            get_u32(&part->start) + get_u32(&part->length) <= total_sectors)
            ;
        else {
            len = sprintf(tmp, "%06lx00 +%06lx00: outside data area;\n",
                          (unsigned long)(get_u32(&part->start) >> 8),
                          (unsigned long)(get_u32(&part->length) >> 8));
            ADD_PROBLEM(buffer, buffer_size, tmp, len);
        }

        if ((get_u32(&part->length) % ((128 _MB) / 512)) != 0) {
            len = sprintf(tmp, "%06lx00: size %06lx00 not multiple to 128MB;\n",
                          (unsigned long)(get_u32(&part->start) >> 8),
                          (unsigned long)(get_u32(&part->length) >> 8));
            ADD_PROBLEM(buffer, buffer_size, tmp, len);
        }

        if ((get_u32(&part->start) % get_u32(&part->length)) != 0) {
            len = sprintf(tmp, "%06lx00: start not multiple to size %06lx00;\n",
                          (unsigned long)(get_u32(&part->start) >> 8),
                          (unsigned long)(get_u32(&part->length) >> 8));
            ADD_PROBLEM(buffer, buffer_size, tmp, len);
        }

        if (get_u32(&part->main) == 0 &&
            get_u16(&part->flags) == 0 &&
            get_u32(&part->start) != 0) { /* check sub-partitions */
            u_int32_t count = 0;
            for (j = 0; j < slice->part_count; ++j) {
                const ps2_partition_header_t *part2 = &slice->parts[j].header;
                if (get_u32(&part2->main) == get_u32(&part->start)) { /* sub-partition of current main partition */
                    int found;
                    if (get_u16(&part2->flags) != PS2_PART_FLAG_SUB) {
                        len = sprintf(tmp, "%06lx00: mismatching sub-partition flag;\n",
                                      (unsigned long)(get_u32(&part2->start) >> 8));
                        ADD_PROBLEM(buffer, buffer_size, tmp, len);
                    }

                    found = 0;
                    for (k = 0; k < get_u32(&part->nsub); ++k)
                        if (get_u32(&part->subs[k].start) == get_u32(&part2->start)) { /* in list */
                            if (get_u32(&part->subs[k].length) != get_u32(&part2->length)) {
                                len = sprintf(tmp, "%06lx00: mismatching sub-partition size: %06lx00 != %06lx00;\n",
                                              (unsigned long)(get_u32(&part2->start) >> 8),
                                              (unsigned long)(get_u32(&part2->length) >> 8),
                                              (unsigned long)(get_u32(&part->subs[k].length) >> 8));
                                ADD_PROBLEM(buffer, buffer_size, tmp, len);
                            }
                            found = 1;
                            break;
                        }
                    if (!found) {
                        len = sprintf(tmp, "%06lx00: not a sub-partition of %06lx00;\n",
                                      (unsigned long)(get_u32(&part2->start) >> 8),
                                      (unsigned long)(get_u32(&part->start) >> 8));
                        ADD_PROBLEM(buffer, buffer_size, tmp, len);
                    }

                    ++count;
                }
            }
            if (count != get_u32(&part->nsub)) {
                len = sprintf(tmp, "%06lx00: only %u sub-partitions found of %u;\n",
                              (unsigned long)(get_u32(&part->start) >> 8),
                              (unsigned int)count, (unsigned int)get_u32(&part->nsub));
                ADD_PROBLEM(buffer, buffer_size, tmp, len);
            }
        }
    }

    /* verify double-linked list */
    for (i = 0; i < slice->part_count; ++i) {
        apa_partition_t *prev = slice->parts + (i > 0 ? i - 1 : slice->part_count - 1);
        apa_partition_t *curr = slice->parts + i;
        apa_partition_t *next = slice->parts + (i + 1 < slice->part_count ? i + 1 : 0);
        if (get_u32(&curr->header.prev) != get_u32(&prev->header.start)) {
            len = sprintf(tmp, "%06lx00: previous is %06lx00, not %06lx00;\n",
                          (unsigned long)(get_u32(&curr->header.start) >> 8),
                          (unsigned long)(get_u32(&prev->header.start) >> 8),
                          (unsigned long)(get_u32(&curr->header.prev) >> 8));
            ADD_PROBLEM(buffer, buffer_size, tmp, len);
        }
        if (get_u32(&curr->header.next) != get_u32(&next->header.start)) {
            len = sprintf(tmp, "%06lx00: next is %06lx00, not %06lx00;\n",
                          (unsigned long)(get_u32(&curr->header.start) >> 8),
                          (unsigned long)(get_u32(&next->header.start) >> 8),
                          (unsigned long)(get_u32(&curr->header.next) >> 8));
            ADD_PROBLEM(buffer, buffer_size, tmp, len);
        }
    }

    return (RET_OK);
}


/**************************************************************/
static int
apa_check_slice(const apa_slice_t *slice)
{ /* NOTE: keep in sync with apa_list_problems */
    u_int32_t i, j, k;

    const u_int32_t total_sectors = slice->size_in_mb * 1024 * 2;

    for (i = 0; i < slice->part_count; ++i) {
        const ps2_partition_header_t *part = &slice->parts[i].header;
        if (get_u32(&part->checksum) != apa_partition_checksum(part))
            return (RET_BAD_APA); /* bad checksum */

        if (get_u32(&part->start) < total_sectors &&
            get_u32(&part->start) + get_u32(&part->length) <= total_sectors)
            ;
        else
            return (RET_BAD_APA); /* data behind end-of-slice */

        if ((get_u32(&part->length) % ((128 _MB) / 512)) != 0)
            return (RET_BAD_APA); /* partition size not multiple to 128MB */

        if ((get_u32(&part->start) % get_u32(&part->length)) != 0)
            return (RET_BAD_APA); /* partition start not multiple on partition size */

        if (get_u32(&part->main) == 0 &&
            get_u16(&part->flags) == 0 &&
            get_u32(&part->start) != 0) { /* check sub-partitions */
            u_int32_t count = 0;
            for (j = 0; j < slice->part_count; ++j) {
                const ps2_partition_header_t *part2 = &slice->parts[j].header;
                if (get_u32(&part2->main) == get_u32(&part->start)) { /* sub-partition of current main partition */
                    int found;
                    if (get_u16(&part2->flags) != PS2_PART_FLAG_SUB)
                        return (RET_BAD_APA);

                    found = 0;
                    for (k = 0; k < get_u32(&part->nsub); ++k)
                        if (get_u32(&part->subs[k].start) == get_u32(&part2->start)) { /* in list */
                            if (get_u32(&part->subs[k].length) != get_u32(&part2->length))
                                return (RET_BAD_APA);
                            found = 1;
                            break;
                        }
                    if (!found)
                        return (RET_BAD_APA); /* not found in the list */

                    ++count;
                }
            }
            if (count != get_u32(&part->nsub))
                return (RET_BAD_APA); /* wrong number of sub-partitions */
        }
    }

    /* verify double-linked list */
    for (i = 0; i < slice->part_count; ++i) {
        apa_partition_t *prev = slice->parts + (i > 0 ? i - 1 : slice->part_count - 1);
        apa_partition_t *curr = slice->parts + i;
        apa_partition_t *next = slice->parts + (i + 1 < slice->part_count ? i + 1 : 0);
        if (get_u32(&curr->header.prev) != get_u32(&prev->header.start) ||
            get_u32(&curr->header.next) != get_u32(&next->header.start))
            return (RET_BAD_APA); /* bad links */
    }

    return (RET_OK);
}


/**************************************************************/
static int
apa_check(const apa_toc_t *toc)
{
    int result = apa_check_slice(toc->slice + 0);
    if (result == RET_OK &&
        toc->got_2nd_slice)
        result = apa_check_slice(toc->slice + 1);
    return (result);
}


/**************************************************************/
int apa_diag_ex(hio_t *hio,
                /*@out@*/ char *buffer,
                size_t buffer_size)
{
    /*@only@*/ apa_toc_t *toc = NULL;
    int result = apa_toc_read_internal(hio, &toc, 1);
    if (result == RET_OK && toc != NULL) {
        if (toc->got_2nd_slice)
            ADD_PROBLEM(buffer, buffer_size, "Slice 1\n", strlen("Slice 1\n"));
        result = apa_list_problems(toc->slice + 0, buffer, buffer_size);
        if (result == RET_OK && toc->got_2nd_slice) {
            ADD_PROBLEM(buffer, buffer_size, "Slice 2\n", strlen("Slice 2\n"));
            result = apa_list_problems(toc->slice + 1, buffer, buffer_size);
        }
        apa_toc_free(toc), toc = NULL;
    }
    return (result);
}


/**************************************************************/
int apa_diag(const dict_t *config,
             const char *device,
             char *buffer,
             size_t buffer_size)
{
    /*@only@*/ hio_t *hio = NULL;
    int result;
    *buffer = '\0';
    result = hio_probe(config, device, &hio);
    if (result == RET_OK && hio != NULL) {
        result = apa_diag_ex(hio, buffer, buffer_size);
        (void)hio->close(hio), hio = NULL;
    }
    return (result);
}


/**************************************************************/
int apa_initialize(const dict_t *config,
                   const char *device,
                   const char *file_name)
{
    /*@out@*/ hio_t *hio = NULL;
    int result = hio_probe(config, device, &hio);
    if (result == RET_OK && hio != NULL) {
        result = apa_initialize_ex(hio, file_name);
        if (result == RET_OK) {
            fprintf(stdout, "MBR data successfully injected\n");
            result = hio->close(hio);
        } else
            (void)hio->close(hio); /* ignore close error in this case */
        hio = NULL;
    }
    return (result);
}


/**************************************************************/
int apa_initialize_ex(hio_t *hio, const char *file_name)
{
    ps2_partition_header_t header;
    u_int32_t dummy;

    u_int32_t prev;
    u_int32_t next;
    int result;

    int osd_start = 0x002020;

    char *mbrelf = NULL;
    u_int32_t mbrelf_length;

    char buffer[1024];
    char *buffer_1725sect;
    result = hio->read(hio, 0, 2, buffer, &dummy);
    if (result != RET_OK)
        return (result);

    next = (u_int32_t)get_u32(buffer + 8);
    prev = (u_int32_t)get_u32(buffer + 12);

    result = read_file(file_name, &mbrelf, &mbrelf_length);
    if (result != OSAL_OK)
        return (result);

    buffer_1725sect = osal_alloc(MAX_MBR_KELF_SIZE);
    /* check MBR file */
    if (mbrelf_length > MAX_MBR_KELF_SIZE)
        result = RET_MBR_KELF_SIZE;
    else if (mbrelf[0] != 0x01 || mbrelf[3] != 0x04)
        result = RET_INVALID_KELF;
    else {
        memcpy(buffer_1725sect, mbrelf, mbrelf_length);
        result = hio->write(hio, osd_start, MAX_MBR_KELF_SIZE / 512, buffer_1725sect, &dummy);
    }

    osal_free(mbrelf);
    osal_free(buffer_1725sect);
    if (result != OSAL_OK)
        return result;

    /* prepare MBR */

    memset(&header, 0, sizeof(ps2_partition_header_t));
    strcpy((void *)header.magic, PS2_PARTITION_MAGIC);
    strcpy(header.id, "__mbr");
    set_u32(&header.length, 128 * 1024 * 2);
    set_u16(&header.type, 0x0001);
    set_ps2fs_datetime(&header.created, time(NULL));
    memcpy(header.mbr.magic, PS2_MBR_MAGIC, 32);
    header.mbr.version = PS2_MBR_VERSION;
    header.mbr.nsector = 0;
    set_ps2fs_datetime(&header.mbr.created, time(NULL));

    /*fix broken - just injection*/
    set_u32(&header.modver, 0x201);
    set_u32(&header.prev, prev);
    set_u32(&header.next, next);
    set_u32(&header.mbr.data_start, osd_start);
    set_u32(&header.mbr.data_len, (mbrelf_length + 511) / 512);
    set_u32(&header.checksum, apa_partition_checksum(&header));

    /* save __mbr partition */
    return (hio->write(hio, 0, 2, &header, &dummy));
}

/**************************************************************/
int apa_dump_mbr(const dict_t *config, const char *device, const char *file_name)
{
    /*@out@*/ hio_t *hio = NULL;
    int result = hio_probe(config, device, &hio);
    ps2_partition_header_t header;
    u_int32_t szread;


    if (result == RET_OK && hio != NULL) {
        u_int32_t bytes;

        /* read MBR to auto-detect partition dialect */
        result = hio->read(hio, 0, 2, &header, &bytes);

        if (result == RET_OK) {
            char *buffer;
            buffer = malloc(header.mbr.data_len * 512);
            result = hio->read(hio, header.mbr.data_start, header.mbr.data_len, buffer, &szread);
            /* won't overwrite */
            if (result != RET_OK)
                return result;

            result = write_file(file_name, buffer, szread);
            free(buffer);
        }
    }

    if (result == OSAL_OK) {
        fprintf(stdout, "MBR data successfully dumped\n");
        result = hio->close(hio);
    } else
        (void)hio->close(hio); /* ignore close error in this case */
    hio = NULL;

    return (result);
}

/**************************************************************/
char *ppa_files_name[] = {
    "system.cnf", /* Contains settings used when the application starts */
    "icon.sys",   /* Contains settings for displaying the application’s icon */
    "list.ico",   /* Contains data for the icon displayed in the list of applications */
    "del.ico",    /* Contains data for the icon displayed when the application is deleted (can be the same as the list-view icon) */
    "boot.kelf",  /* https://github.com/ps2homebrew/OPL-Launcher */
    "boot.kirx",  /* found on some retail games */
    NULL,
};

int apa_dump_header(hio_t *hio, u_int32_t starting_partition_sector)
{
    ppaa_partition_t *head;
    int index = 0, result;
    char *buffer;
    u_int32_t bytes_read;

    buffer = osal_alloc(4 _MB);
    result = hio->read(hio, starting_partition_sector + PPAA_START / 512, 4 _MB / 512, buffer, &bytes_read);
    if (result != RET_OK)
        return (result);

    head = (ppaa_partition_t *)buffer;

    if (strncmp(head->magic, PPAA_MAGIC, sizeof(head->magic)))
        return RET_BAD_APA;

    while (index < 62) {
        ssize_t bytes_to_read = head->file[index].size;
        char *filename, genname[10];

        if (bytes_to_read == 0)
            break;

        if (ppa_files_name[index])
            filename = ppa_files_name[index];
        else {
            filename = genname;
            sprintf(genname, "HEADER_%d", index);
        }

        fprintf(stdout, "%-10s offset=0x%-10x size=%lu\n", filename, head->file[index].offset, bytes_to_read);
        result = write_file(filename, buffer + head->file[index].offset, bytes_to_read);
        if (result != RET_OK)
            return (result);
        index++;
    }

    osal_free(buffer);
    return 0;
}
