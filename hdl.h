/*
 * hdl.h
 * $Id: hdl.h,v 1.9 2006/09/01 17:29:13 bobi Exp $
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

#if !defined (_HDL_H)
#define _HDL_H

#include "config.h"
#include "progress.h"
#include "hio.h"
#include "iin.h"
#include "apa.h"
#include "ps2_hdd.h"

C_START

#define HDL_GAME_NAME_MAX  64

typedef struct hdl_game_type
{
  char name[HDL_GAME_NAME_MAX + 1];
  char partition_name[PS2_PART_IDMAX + 1];
  char startup[8 + 1 + 3 + 1];
  compat_flags_t compat_flags;
  int is_dvd;
  u_int32_t layer_break;
} hdl_game_t;

typedef struct hdl_game_info_type
{
  char partition_name[PS2_PART_IDMAX + 1];
  char name[HDL_GAME_NAME_MAX + 1];
  char startup[8 + 1 + 3 + 1];
  compat_flags_t compat_flags;
  int is_dvd;
  int slice_index;
  u_int32_t start_sector;
  u_int32_t raw_size_in_kb;
  u_int32_t alloc_size_in_kb;
} hdl_game_info_t;

typedef struct hdl_games_list_type
{
  u_int32_t count;
  /*@only@*/ hdl_game_info_t *games;
  u_int32_t total_chunks;
  u_int32_t free_chunks;
} hdl_games_list_t;
typedef /*@only@*/ /*@null@*/ /*@out@*/ hdl_games_list_t* hdl_games_list_p_t;


void hdl_pname (const char *name,
		/*@out@*/ char partition_name[PS2_PART_IDMAX + 1]);

int hdl_extract_ex (hio_t *hio,
		    const char *game_name,
		    const char *output_file,
		    progress_t *pgs);

int hdl_extract (const dict_t *config,
		 const char *device,
		 const char *name,   /* of the game */
		 const char *output, /* file */
		 progress_t *pgs);

int hdl_inject (hio_t *hio,
		iin_t *iin,
		hdl_game_t *details,
		int slice_index,
		progress_t *pgs);


int hdl_glist_read (hio_t *hio,
		    /*@special@*/ hdl_games_list_p_t *glist) /*@allocates *glist@*/ /*@defines *glist@*/;

void hdl_glist_free (/*@special@*/ /*@only@*/ hdl_games_list_t *glist) /*@releases glist@*/;

int hdl_lookup_partition_ex (hio_t *hio,
			     const char *game_name,
			     /*@out@*/ char partition_id[PS2_PART_IDMAX + 1]);

int hdl_lookup_partition (const dict_t *config,
			  const char *device_name,
			  const char *game_name,
			  /*@out@*/ char partition_id[PS2_PART_IDMAX + 1]);

typedef struct hdl_game_alloc_table_type
{
  u_int32_t count;
  u_int32_t size_in_kb;
  struct
  { /* those are in 512-byte long sectors */
    u_int32_t start, len;
  } part[64];
} hdl_game_alloc_table_t;

int hdl_read_game_alloc_table (hio_t *hio,
			       const apa_toc_t *toc,
			       int slice_index,
			       u_int32_t partition_index,
			       /*@out@*/ hdl_game_alloc_table_t *gat);

int hdl_modify_game (hio_t *hio,
		     apa_toc_t *toc,
		     int slice_index,
		     u_int32_t starting_partition_sector,
		     const char *new_name, /* or NULL */
		     compat_flags_t new_compat_flags); /* or COMPAT_FLAGS_INVALID */

int hdd_inject_header (hio_t *hio,
		     apa_toc_t *toc,
		     int slice_index,
		     u_int32_t starting_partition_sector);

C_END

#endif /* _HDL_H defined? */
