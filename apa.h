/*
 * apa.h
 * $Id: apa.h,v 1.6 2004/08/15 16:44:19 b081 Exp $
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

#if !defined (_APA_H)
#define _APA_H

#include "ps2_hdd.h"
#include "osal.h"
#include "hio.h"


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


typedef struct apa_partition_table_type
{
  size_t device_size_in_mb;
  size_t total_chunks;
  size_t allocated_chunks;
  size_t free_chunks;

  char *chunks_map;

  /* existing partitions */
  size_t part_alloc_;
  size_t part_count;
  apa_partition_t *parts;
} apa_partition_table_t;


u_int32_t apa_partition_checksum (const ps2_partition_header_t *part);

int is_apa_partition (osal_handle_t handle);

void apa_ptable_free (apa_partition_table_t *table);

int apa_ptable_read (const char *device,
		     apa_partition_table_t **table);

int apa_ptable_read_ex (hio_t *hio,
			apa_partition_table_t **table);

int apa_find_partition (const apa_partition_table_t *table,
			const char *partition_name,
			size_t *partition_index);

int apa_allocate_space (apa_partition_table_t *table,
			const char *partition_name,
			size_t size_in_mb,
			size_t *new_partition_start,
			int decreasing_size);

int apa_delete_partition (apa_partition_table_t *table,
			  const char *partition_name);

int apa_commit (const char *device_name,
		const apa_partition_table_t *table);

int apa_commit_ex (hio_t *hio,
		   const apa_partition_table_t *table);

#endif /* _APA_H defined? */
