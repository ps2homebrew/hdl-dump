/*
 * hdl.h
 * $Id: hdl.h,v 1.4 2004/09/26 19:39:39 b081 Exp $
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

#include "progress.h"
#include "hio.h"
#include "iin.h"
#include "ps2_hdd.h"


#define HDL_GAME_NAME_MAX  64

typedef struct hdl_game_type
{
  char name [HDL_GAME_NAME_MAX + 1];
  char partition_name [PS2_PART_IDMAX + 1];
  char startup [8 + 1 + 3 + 1];
  unsigned char compat_flags;
  int is_dvd;
} hdl_game_t;

typedef struct hdl_game_info_type
{
  char partition_name [PS2_PART_IDMAX + 1];
  char name [HDL_GAME_NAME_MAX + 1];
  char startup [8 + 1 + 3 + 1];
  unsigned char compat_flags;
  int is_dvd;
  unsigned long start_sector;
  unsigned long total_size_in_kb;
} hdl_game_info_t;

typedef struct hdl_games_list_type
{
  size_t count;
  hdl_game_info_t *games;
  size_t total_chunks;
  size_t free_chunks;
} hdl_games_list_t;


void hdl_pname (const char *name,
		char partition_name [PS2_PART_IDMAX + 1]);

int hdl_extract (const char *device,
		 const char *name,   /* of the game */
		 const char *output, /* file */
		 progress_t *pgs);

int hdl_inject (hio_t *hio,
		iin_t *iin,
		hdl_game_t *details,
		progress_t *pgs);


int hdl_glist_read (hio_t *hio,
		    hdl_games_list_t **glist);

void hdl_glist_free (hdl_games_list_t *glist);


#endif /* _HDL_H defined? */
