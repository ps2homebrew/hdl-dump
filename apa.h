/*
 * apa.h
 * $Id: apa.h,v 1.11 2006/09/01 17:33:15 bobi Exp $
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

#if !defined(_APA_H)
#define _APA_H

#include "config.h"
#include "ps2_hdd.h"
#include "osal.h"
#include "hio.h"
#include "dict.h"

C_START

#define MAX_MBR_KELF_SIZE 883200

/* chunks_map */
static const char MAP_AVAIL = '.';
static const char MAP_MAIN = 'M';
static const char MAP_SUB = 's';
static const char MAP_COLL = 'x';
static const char MAP_ALLOC = '*';


typedef struct apa_partition_type
{
    int existing;
    int modified;
    int linked;
    ps2_partition_header_t header;
} apa_partition_t;


typedef struct apa_slice_type
{
    int slice_index;
    u_int32_t size_in_mb;

    u_int32_t total_chunks;
    u_int32_t allocated_chunks;
    u_int32_t free_chunks;
    /*@only@*/ char *chunks_map;

    /* existing partitions */
    u_int32_t part_alloc_;
    u_int32_t part_count;
    /*@only@*/ apa_partition_t *parts;
} apa_slice_t;

typedef struct apa_toc_type
{
    u_int32_t size_in_kb;

    int is_toxic, is_2_slice, got_2nd_slice;

    apa_slice_t slice[2];
} apa_toc_t;
typedef /*@out@*/ /*@only@*/ /*@null@*/ apa_toc_t *apa_toc_p_t;


u_int32_t apa_partition_checksum(const ps2_partition_header_t *part);

int is_apa_partitioned(osal_handle_t handle);

void apa_toc_free(/*@special@*/ /*@only@*/ apa_toc_t *toc) /*@releases toc@*/;

int apa_toc_read(const dict_t *config,
                 const char *device,
                 /*@special@*/ apa_toc_p_t *toc) /*@allocates *toc@*/ /*@defines *toc@*/;

int apa_toc_read_ex(hio_t *hio,
                    /*@special@*/ apa_toc_p_t *toc) /*@allocates *toc@*/ /*@defines *toc@*/;

int apa_find_partition(const apa_toc_t *toc,
                       const char *partition_name,
                       /*@out@*/ int *slice_index,
                       /*@out@*/ u_int32_t *partition_index);

/* slice_index is a hint for where to try first: slice 0 or 1 */
int apa_allocate_space(apa_toc_t *toc,
                       const char *partition_name,
                       u_int32_t size_in_mb,
                       /*@in@*/ /*@out@*/ int *slice_index,
                       /*@out@*/ u_int32_t *new_partition_start,
                       int decreasing_size);

int apa_delete_partition(apa_toc_t *toc,
                         const char *partition_name);

int apa_commit(const dict_t *config,
               const char *device_name,
               const apa_toc_t *toc);

int apa_commit_ex(hio_t *hio,
                  const apa_toc_t *toc);

int apa_diag(const dict_t *config,
             const char *device,
             /*@out@*/ char *buffer,
             size_t buffer_size);

int apa_diag_ex(hio_t *hio,
                /*@out@*/ char *buffer,
                size_t buffer_size);

int apa_initialize(const dict_t *config,
                   const char *device,
				   const char *file_name);

int apa_initialize_ex(hio_t *hio, const char *file_name);

int apa_dump_mbr(const dict_t *config,
                 const char *device, const char *file_name);

C_END

#endif /* _APA_H defined? */
