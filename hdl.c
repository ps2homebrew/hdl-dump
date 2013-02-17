/*
 * hdl.c
 * $Id: hdl.c,v 1.17 2006/09/01 17:31:19 bobi Exp $
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
#include <stdio.h>
#include <string.h>

#include "byteseq.h"
#include "iin.h"
#include "hdl.h"
#include "apa.h"
#include "retcodes.h"
#include "osal.h"
#include "common.h"
#include "hio.h"
#if defined (BUILTIN_ICON)
#  include "icon.h"
#endif
#  include "kelf.h"


/*
 * notice: that code would only run on a big-endian machine
 */


static const char *HDL_HDR0 =
"PS2ICON3D";

static const char *HDL_HDR1 =
"BOOT2 = PATINFO\n"
"VER = 1.00\n"
"VMODE = PAL\n"
"HDDUNITPOWER = NICHDD\n";

static const char *HDL_HDR2 =
"PS2X\n"
"title0 = HD Loader\n"
"title1 = %s\n"
"bgcola = 999\n"
"bgcol0 = 20, 20, 60\n"
"bgcol1 = 20, 20, 60\n"
"bgcol2 = 20, 20, 60\n"
"bgcol3 = 20, 20, 60\n"
"lightdir0 = 0.5, 0.5, 0.5\n"
"lightdir1 = 0.0, -0.4, -1.0\n"
"lightdir2 = -0.5, -0.5, 0.5\n"
"lightcolamb = 255, 255, 255\n"
"lightcol0 = 127, 127, 127\n"
"lightcol1 = 178, 178, 178\n"
"lightcol2 = 127, 127, 127\n"
"uninstallmes0 =\n"
"uninstallmes1 =\n"
"uninstallmes2 =\n";


/**************************************************************/
static int
prepare_main (const hdl_game_t *details,
	      const apa_toc_t *toc,
	      int slice_index,
	      u_int32_t starting_partition_sector,
	      u_int32_t size_in_kb,
	      /*@out@*/ u_int8_t *buffer_4m)
{
  int result;
  u_int32_t i;

  const char *patinfo_header = (const char*) hdloader_kelf_header;
  u_int32_t patinfo_header_length = HDLOADER_KELF_HEADER_LEN;
  char *patinfo = NULL;
  u_int32_t patinfo_length;
  const char *patinfo_footer = (const char*) hdloader_kelf_footer;
  u_int32_t patinfo_footer_length = HDLOADER_KELF_FOOTER_LEN;
  u_int32_t patinfo_kelf_length = KELF_LENGTH;
  
#if defined (BUILTIN_ICON)
  const char *icon = (const char*) hdloader_icon;
  u_int32_t icon_length = HDLOADER_ICON_LEN;
#else
  char *icon = NULL;
  u_int32_t icon_length;
#endif
  char icon_props[1024];
  const ps2_partition_header_t *part;
  const apa_slice_t *slice = toc->slice + slice_index;

  part = NULL;

#if !defined (BUILTIN_ICON)
  /* read icon */
  result = read_file ("./icon.bin", &icon, &icon_length);
#else
  /* icon is embedded in the executable */
  result = OSAL_OK;
#endif
  if (result == OSAL_OK)
 {
    result = read_file ("./PATINFO.ELF", &patinfo, &patinfo_length);
	if (result == OSAL_OK)
    for (i = 0; i < slice->part_count; ++i)
      if (get_u32 (&slice->parts[i].header.start) == starting_partition_sector)
	{ /* locate starting partition index */
	  part = &slice->parts[i].header;
	  break;
	}
  }

  if (part == NULL)
    result = RET_NOT_FOUND;

  if (result == OSAL_OK && part != NULL)
    {
      const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
      u_int32_t *tmp;
      u_int32_t offset;
      u_int32_t size_remaining_in_kb;
      u_int32_t partition_usable_size_in_kb;
      u_int32_t partition_data_len_in_kb;
      u_int32_t partitions_used = 0;
      u_int32_t sector;

      /* PS2 partition header */
      memset (buffer_4m, 0, 4 * 1024 * 1024);
      memcpy (buffer_4m, part, 1024);

      sprintf (icon_props, HDL_HDR2, details->name);

      /*
       *  1000: 50 53 32 49 43 4f 4e 33  44 00 00 00 00 00 00 00  PS2ICON3D.......
       */
      memcpy (buffer_4m + 0x001000, HDL_HDR0, strlen (HDL_HDR0));

      /*
       *                     +--+--+--+- BE, BOOT2 = ... section length in bytes
       *                     |  |  |  |               +--+--+--+- BE, PSX2... len in bytes
       *                     v  v  v  v               v  v  v  v
       *  1010: 00 02 00 00 4b 00 00 00  00 04 00 00 70 01 00 00
       *  1020: 00 08 00 00 58 81 00 00  00 08 00 00 58 81 00 00
       *         ^  ^  ^  ^  ^  ^  ^  ^               ^  ^  ^  ^
       *         |  |  |  |  +--+--+--+- same as =>   +--+--+--+- BE, icon length in bytes
       *         +--+--+--+- part offset relative to 0x1000
       */
      tmp = (u_int32_t*) (buffer_4m + 0x001010);
      set_u32 (tmp++, 0x0200);
      set_u32 (tmp++, strlen (HDL_HDR1));
      set_u32 (tmp++, 0x0400);
      set_u32 (tmp++, strlen (icon_props));
      set_u32 (tmp++, 0x0800);
      set_u32 (tmp++, icon_length);
      set_u32 (tmp++, 0x0800);
      set_u32 (tmp++, icon_length);

      /*
       *  1030: 00 00 11 00 58 81 00 00  00 00 00 00 00 00 00 00
       *         ^  ^  ^  ^  ^  ^  ^  ^
       *         |  |  |  |  +--+--+--+- BE, PATINFO.KELF length in bytes
       *         +--+--+--+- PATINFO.KELF offset relative to 0x1000
       */
      set_u32 (tmp++, 0x110000);
      set_u32 (tmp++, patinfo_kelf_length);

      /*
       *  1200: 42 4f 4f 54 32 20 3d 20 50 41 54 49 4e 46 4f 0a  BOOT2 = PATINFO.
       *  1210: 56 45 52 20 3D 20 31 2E 30 30 0A 56 4D 4F 44 45  VER = 1.00.VMODE
       *  1220: 20 3d 20 50 41 4c 0a 48 44 44 55 4e 49 54 50 4f   = PAL.HDDUNITPO
       *  1230: 57 45 52 20 3d 20 4e 49 43 48 44 44 0a 00 00 00  WER = NICHDD....
       */
      memcpy (buffer_4m + 0x001200, HDL_HDR1, strlen (HDL_HDR1));

      /*
       *  1400: 50 53 32 58 0a 74 69 74  6c 65 30 20 3d 20 48 44  PS2X.title0 = HD
       *  1410: 20 4c 6f 61 64 65 72 0a  74 69 74 6c 65 31 20 3d   Loader.title1 =
       *  1420: 20 54 77 69 73 74 65 64  20 4d 65 74 61 6c 3a 20   Twisted Metal: 
       *        ...
       */
      memcpy (buffer_4m + 0x001400, icon_props, strlen (icon_props));

      /*
       *  1800: 00 00 01 00 01 00 00 00  07 00 00 00 00 00 80 3f  ...............?
       *        ... icon ...
       *  9950: 00 04 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
       */
      memcpy (buffer_4m + 0x001800, icon, icon_length);

      /*
       *101000: ed fe ad de 00 00 01 00  54 77 69 73 74 65 64 20  ........Twisted 
       *101010: 4d 65 74 61 6c 3a 20 42  6c 61 63 6b 00 00 00 00  Metal: Black....
       */
      set_u32 (buffer_4m + 0x101000, 0xdeadfeed);
      buffer_4m[0x101006] = 0x01;
      memcpy (buffer_4m + 0x101008, details->name, strlen (details->name));

      /*
       *                                  +- compatibility modes
       *                                  v
       *1010a0: 00 00 00 00 00 00 00 00  00 00 00 00 53 43 45 53  ............SCES
       *1010b0: 5f 35 30 33 2e 36 30 00  00 00 00 00 00 00 00 00  _503.60.........
       */
      set_u16 (buffer_4m + 0x1010a8, (u_int16_t) details->compat_flags);
      memcpy (buffer_4m + 0x1010ac, details->startup, strlen (details->startup));

      /*
       *	            +- media type? 0x14 for DVD, 0x12 for CD, 0x10 for PSX CD (w/o audio)?
       *	            |           +- number of partitions
       *	            v           v
       *	           14 00 00 00 05 00 00 00 00 
       *1010f5: 00 00 00 00 20 50 00 00 00 c0 1f 00	;; 0x00500000, 512MB
       *	f8 03 00 00 08 60 00 00 00 f0 1f 00	;; 0x00600000, 512MB
       *	f6 07 00 00 08 70 00 00 00 f0 1f 00	;; 0x00700000, 512MB
       *	f4 0b 00 00 08 44 00 00 00 f0 07 00	;; 0x00440000, 128MB
       *	f2 0c 00 00 08 48 00 00 00 25 01 00	;; 0x00480000, 128MB
       *	 ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^
       *	 +--+--+--+  +--+--+--+  +--+--+--+- BE, x / 4 = part size in kilobytes[!]
       *	          |           +- x << 8 = data start in HDD-sectors
       *	          +- BE, x * 512 = part offset in megabytes, incremental counter[!]
       */

      /* that is aerial acrobatics :-) */
      set_u32 (buffer_4m + 0x1010e8, details->layer_break);
      buffer_4m[0x1010ec] = (u_int8_t) (details->is_dvd ? 0x14 : 0x12);
      buffer_4m[0x1010f0] = (u_int8_t) (get_u32 (&part->nsub) + 1);
      tmp = (u_int32_t*) (buffer_4m + 0x1010f5);

  /*
       *111000: 01 00 00 01 00 03 00 4a  00 01 02 19 00 00 00 56 
       *        ... PATINFO.KELF ...
       *179640: 7e bd 13 b2 4e 1f 26 08  29 53 97 37 13 c3 71 1c 
       */
      memcpy (buffer_4m + 0x111000, patinfo_header, patinfo_header_length);
      memcpy (buffer_4m + 0x111080, patinfo, patinfo_length);
      memcpy (buffer_4m + 0x179640, patinfo_footer, patinfo_footer_length);

      partition_usable_size_in_kb = (get_u32 (&part->length) - 0x2000) / 2; /* 2 sectors == 1K */
      partition_data_len_in_kb =
	size_in_kb < partition_usable_size_in_kb ?
	size_in_kb : partition_usable_size_in_kb;

      /* offset, start, data length */
      set_u32 (tmp++, 0); /* offset */
      /* 0x2000 sec = 4M (offs for 1st part.) */
      sector = get_u32 (&part->start) + 0x2000 + SLICE_2_OFFS * slice_index;
      set_u32 (tmp++, sector >> 8);
      set_u32 (tmp++, partition_data_len_in_kb * 4);
      ++partitions_used;

      offset = (get_u32 (&part->length) - 0x2000) / 1024;
      size_remaining_in_kb = size_in_kb - partition_data_len_in_kb;
      for (i=0; size_remaining_in_kb>0 && i<get_u32 (&part->nsub); ++i)
	{
	  partition_usable_size_in_kb =
	    (get_u32 (&part->subs[i].length) - 0x0800) / 2;
	  partition_data_len_in_kb =
	    size_remaining_in_kb < partition_usable_size_in_kb ?
	    size_remaining_in_kb : partition_usable_size_in_kb;

	  /* offset, start, data length */
	  set_u32 (tmp++, offset);
	  /* 0x0800 sec = 1M (sub part. offs) */
	  sector = (get_u32 (&part->subs[i].start) + 0x0800 +
		    SLICE_2_OFFS * slice_index);
	  set_u32 (tmp++, sector >> 8);
	  set_u32 (tmp++, partition_data_len_in_kb * 4);
	  ++partitions_used;

	  offset += (get_u32 (&part->subs[i].length) - 0x0800) / 1024;
	  size_remaining_in_kb -= partition_data_len_in_kb;
	}

      buffer_4m[0x1010f0] = (u_int8_t) partitions_used;
    }

#if !defined (BUILTIN_ICON)
  if (icon != NULL)
    osal_free (icon);
#endif

  if (patinfo != NULL)
    osal_free (patinfo);

  return (result);
}


/**************************************************************/
void
hdl_pname (const char *name,
	   char partition_name[PS2_PART_IDMAX + 1])
{
  u_int32_t game_name_len = PS2_PART_IDMAX - 1 - 7; /* limit partition name length */
  char *p;

  game_name_len = strlen (name) < game_name_len ? strlen (name) : game_name_len;
  strcpy (partition_name, "PP.HDL.");
  memmove (partition_name + 7, name, game_name_len);
  partition_name[7 + game_name_len] = '\0';
  p = partition_name + 7; /* len ("PP.HDL.") */
  while (*p != '\0')
    {
      if (!isalnum (*p) && *p != ' ' && *p != '.')
	*p = '_'; /* escape non-alphanumeric characters with `_' */
      ++p;
    }
}


/**************************************************************/
int
hdl_extract_ex (hio_t *hio,
		const char *game_name,
		const char *output_file,
		progress_t *pgs)
{
  /*@only@*/ apa_toc_t *toc = NULL;
  int result = apa_toc_read_ex (hio, &toc);
  if (result == RET_OK && toc != NULL)
    {
      int slice_index = -1;
      u_int32_t partition_index;
      result = apa_find_partition (toc, game_name, &slice_index,
				   &partition_index);
      if (result == RET_NOT_FOUND)
	{ /* assume it is `game_name' and not a partition name */
	  char partition_id[PS2_PART_IDMAX + 8];
	  result = hdl_lookup_partition_ex (hio, game_name, partition_id);
	  if (result == RET_OK)
	    result = apa_find_partition (toc, partition_id,
					 &slice_index, &partition_index);
	}

      if (result == RET_OK)
	{ /* partition found */
	  hdl_game_alloc_table_t gat;
	  unsigned char *buffer =
	    osal_alloc ((IIN_NUM_SECTORS + 1) * IIN_SECTOR_SIZE);
	  result = (buffer != NULL ? RET_OK : RET_NO_MEM);
	  if (result == RET_OK)
	    {
	      result = hdl_read_game_alloc_table (hio, toc, slice_index,
						  partition_index, &gat);
	      if (result == RET_OK)
		{
		  /*@only@*/ osal_handle_t file = OSAL_HANDLE_INIT;
		  u_int64_t total_size = (u_int64_t) gat.size_in_kb * 1024;

		  pgs_prepare (pgs, total_size);

		  /* create file and copy data */
		  result = osal_create_file (output_file, &file, total_size);
		  if (result == OSAL_OK)
		    {
		      u_int32_t i;
		      u_int8_t *aligned =
			(void*) (((long) buffer + 511) & ~511);
		      for (i = 0; result == OSAL_OK && i < gat.count; ++i)
			{ /* seek and copy */
			  u_int32_t start_s = gat.part[i].start;
			  u_int32_t length_s = gat.part[i].len;
			  u_int64_t curr = 0;
			  while (length_s > 0 && result == OSAL_OK)
			    {
			      u_int32_t len;
			      u_int32_t count_s = (length_s < IIN_NUM_SECTORS ?
						   length_s : IIN_NUM_SECTORS);
			      result = hio->read (hio, start_s, count_s,
						  aligned, &len);
			      if (result == OSAL_OK)
				{
				  u_int32_t written;
				  result = osal_write (file, aligned,
						       count_s * 512,
						       &written);
				  if (result == OSAL_OK)
				    {
				      start_s += count_s;
				      length_s -= count_s;
				      curr += count_s * 512;
				      result = pgs_update (pgs, curr);
				    }
				}
			    }
			  pgs_chunk_complete (pgs);
			} /* for */
		      result = (osal_close (&file) == OSAL_OK ?
				result : OSAL_ERR);
		    }
		}
	    }
	  if (buffer != NULL)
	    osal_free (buffer);
	}
      apa_toc_free (toc);
    }
  return (result);
}


/**************************************************************/
int
hdl_extract (const dict_t *config,
	     const char *device_name,
	     const char *game_name,
	     const char *output_file,
	     progress_t *pgs)
{
  /*@only@*/ hio_t *hio = NULL;
  int result = hio_probe (config, device_name, &hio);
  if (result == OSAL_OK && hio != NULL)
    {
      result = hdl_extract_ex (hio, game_name, output_file, pgs);
      (void) hio->close (hio), hio = NULL;
    }
  return (result);
}


/**************************************************************/
static int
inject_data (hio_t *hio,
	     const apa_toc_t *toc,
	     int slice_index,
	     u_int32_t starting_partition_sector,
	     iin_t *iin,
	     u_int32_t size_in_kb,
	     const hdl_game_t *details,
	     progress_t *pgs)
{
  int result;
  u_int32_t i;
  const ps2_partition_header_t *part;
  const apa_slice_t *slice = toc->slice + slice_index;
  char *buffer;

  part = NULL;
  result = RET_NOT_FOUND;
  for (i = 0; i < slice->part_count; ++i)
    if (get_u32 (&slice->parts[i].header.start) == starting_partition_sector)
      { /* locate starting partition index */
	part = &slice->parts[i].header;
	result = RET_OK;
	break;
      }
  if (part == NULL)
    return (RET_INVARIANT);

  buffer = osal_alloc (4 _MB);
  if (buffer == NULL)
    result = RET_NO_MEM;
  if (result == RET_OK && part != NULL && buffer != NULL)
    {
      const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
      result = prepare_main (details, toc, slice_index,
			     starting_partition_sector,
			     size_in_kb, (u_int8_t*) buffer);
      if (result == RET_OK)
	{
	  u_int32_t kb_remaining = size_in_kb;
	  u_int32_t offset = 0;
	  u_int32_t bytes;
	  u_int32_t sector;
	  const u_int32_t MAIN_HDR_SIZE_S = (4 _MB) / 512;

	  pgs_prepare (pgs, (u_int64_t) (size_in_kb + 4 * 1024) * 1024);

	  /* first: write main partition header (4MB total) */
	  sector = get_u32 (&part->start) + SLICE_2_OFFS * slice_index;
	  result = hio->write (hio, sector, MAIN_HDR_SIZE_S,
			       buffer, &bytes);
	  if (result == OSAL_OK)
	    result = bytes == 4 _MB ? OSAL_OK : OSAL_ERR;
	  osal_free (buffer), buffer = NULL;

	  /* track header, otherwise it would influence progress calculation */
	  (void) pgs_update (pgs, 4 _MB);
	  pgs_chunk_complete (pgs);

	  /* next: fill-in 1st partition */
	  if (result == RET_OK)
	    {
	      u_int64_t part_size =
		((u_int64_t) get_u32 (&part->length)) * 512 - (4 _MB);
	      u_int64_t chunk_length = /* in bytes */
		((u_int64_t) kb_remaining) * 1024 < part_size ?
		((u_int64_t) kb_remaining) * 1024 : part_size;

	      u_int32_t start = (get_u32 (&part->start) + MAIN_HDR_SIZE_S +
				 SLICE_2_OFFS * slice_index);
	      u_int32_t count = (u_int32_t) (chunk_length / IIN_SECTOR_SIZE);

	      result = iin_copy_ex (iin, hio, offset, start, count, pgs);
	      pgs_chunk_complete (pgs);
	      offset += count;
	      kb_remaining -= (u_int32_t) (chunk_length / 1024);
	    }

	  for (i = 0; result == OSAL_OK && kb_remaining > 0 && i != get_u32 (&part->nsub); ++i)
	    { /* next: fill-in remaining partitions */
	      u_int32_t SUB_PART_HDR_SIZE_S = (1 _MB) / 512;
	      u_int64_t part_size =
		((u_int64_t) get_u32 (&part->subs[i].length)) * 512 - (1 _MB);
	      u_int64_t chunk_length = /* in bytes */
		((u_int64_t) kb_remaining) * 1024 < part_size ?
		((u_int64_t) kb_remaining) * 1024 : part_size;

	      u_int32_t start = (get_u32 (&part->subs[i].start) +
				 SUB_PART_HDR_SIZE_S +
				 SLICE_2_OFFS * slice_index);
	      u_int32_t count = (u_int32_t) (chunk_length / IIN_SECTOR_SIZE);

	      result = iin_copy_ex (iin, hio, offset,
				    start, count, pgs);
	      pgs_chunk_complete (pgs);
	      offset += count;
	      kb_remaining -= (u_int32_t) (chunk_length / 1024);
	    }

	  if (result == RET_OK && kb_remaining != 0)
	    result = RET_NO_SPACE; /* the game does not fit in the allocated space... why? */

	  /* finally: commit partition table */
	  if (result == OSAL_OK)
	    result = apa_commit_ex (hio, toc);
	}
    }

  if (buffer != NULL)
    osal_free (buffer);

  return (result);
}


/**************************************************************/
int
hdl_inject (hio_t *hio,
	    iin_t *iin,
	    hdl_game_t *details,
	    int slice_index,
	    progress_t *pgs)
{
  /*@only@*/ apa_toc_t *toc = NULL;
  int result = apa_toc_read_ex (hio, &toc);
  if (result == OSAL_OK && toc != NULL)
    {
      u_int32_t sector_size, num_sectors;
      result = iin->stat (iin, &sector_size, &num_sectors);
      if (result == OSAL_OK)
	{
	  u_int64_t input_size = (u_int64_t) num_sectors * sector_size;
	  u_int32_t size_in_kb = (u_int32_t) (input_size / 1024);
	  u_int32_t size_in_mb =
	    (u_int32_t) ((input_size + (1 _MB - 1)) / (1 _MB));
	  u_int32_t new_partition_start;

	  if (details->partition_name[0] == '\0')
	    { /* partition naming is now auto-detected */
	      if (toc->is_toxic)
		/* Toxic OS partition naming: "PP.HDL.STARTUP" */
		hdl_pname (details->startup, details->partition_name);
	      else
		/* HD Loader partition naming: "PP.HDL.Game name" */
		hdl_pname (details->name, details->partition_name);
	    }

	  result = apa_allocate_space (toc, details->partition_name,
				       size_in_mb, &slice_index,
				       &new_partition_start,
				       0); /* order by size desc */
	  if (result == RET_OK)
	    {
	      result = inject_data (hio, toc, slice_index, new_partition_start,
				    iin, size_in_kb, details, pgs);
	    }
	}
    }

  if (toc != NULL)
    apa_toc_free (toc);

  return (result);
}


/**************************************************************/
static int
hdl_ginfo_read (hio_t *hio,
		int slice_index,
		const ps2_partition_header_t *part,
		hdl_game_info_t *ginfo)
{
  u_int32_t i, size;
  /* data we're interested in starts @ 0x101000 and is header
   * plus information for up to 65 partitions
   * (1 main + 64 sub) by 12 bytes each */
  const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
  const u_int32_t HDL_HEADER_OFFS = 0x101000;
  char buffer[1024];
  int result;
  u_int32_t bytes;
  u_int32_t sector = (get_u32 (&part->start) + HDL_HEADER_OFFS / 512 +
		      slice_index * SLICE_2_OFFS);

  result = hio->read (hio, sector, 2, buffer, &bytes);
  if (result == RET_OK)
    {
      if (bytes == 1024)
	{
	  u_int32_t num_parts = (u_int32_t) buffer[0x00f0];
	  const u_int32_t *data = (u_int32_t*) (buffer + 0x00f5);

	  /* calculate allocated size */
	  size = get_u32 (&part->length);
	  for (i = 0; i < get_u32 (&part->nsub); ++i)
	    size += get_u32 (&part->subs[i].length);
	  ginfo->alloc_size_in_kb = size / 2;

	  size = 0;
	  for (i = 0; i < num_parts; ++i)
	    size += get_u32 (data + (i * 3 + 2));
	  ginfo->raw_size_in_kb = size / 4;

	  memcpy (ginfo->partition_name, part->id, PS2_PART_IDMAX);
	  ginfo->partition_name[PS2_PART_IDMAX] = '\0';
	  strcpy (ginfo->name, buffer + 0x0008);
	  strcpy (ginfo->startup, buffer + 0x00ac);
	  ginfo->compat_flags = (compat_flags_t) get_u16 (buffer + 0x00a8);
	  ginfo->is_dvd = (buffer[0x00ec] == 0x14);
	  ginfo->slice_index = slice_index;
	  ginfo->start_sector = get_u32 (&part->start);
	}
      else
	result = RET_ERR;
    }

  return (result);
}


/**************************************************************/
static size_t
hdl_games_count (const apa_slice_t *slice)
{
  size_t i, count = 0;
  for (i = 0; i < slice->part_count; ++i)
    count += (get_u16 (&slice->parts[i].header.flags) == 0x00 &&
	      get_u16 (&slice->parts[i].header.type) == 0x1337);
  return (count);
}


/**************************************************************/
static int
hdl_glist_read_slice (hio_t *hio,
		      hdl_games_list_t *glist,
		      const apa_toc_t *toc,
		      int slice_index)
{
  size_t i;
  int result = RET_OK;
  for (i = 0; result == RET_OK && i < toc->slice[slice_index].part_count; ++i)
    {
      const ps2_partition_header_t *part =
	&toc->slice[slice_index].parts[i].header;
      if (get_u16 (&part->flags) == 0x00 &&
	  get_u16 (&part->type) == 0x1337)
	result = hdl_ginfo_read (hio, slice_index, part,
				 glist->games + glist->count++);
    }
  return (result);
}


/**************************************************************/
int
hdl_glist_read (hio_t *hio,
		hdl_games_list_t **glist)
{
  /*@only@*/ apa_toc_t *toc = NULL;
  int result = apa_toc_read_ex (hio, &toc);
  if (result == RET_OK && toc != NULL)
    {
      size_t count;
      void *tmp;

      count = (hdl_games_count (toc->slice + 0) +
	       hdl_games_count (toc->slice + 1));

      tmp = osal_alloc (sizeof (hdl_game_info_t) * count);
      if (tmp != NULL)
	{
	  memset (tmp, 0, sizeof (hdl_game_info_t) * count);
	  *glist = osal_alloc (sizeof (hdl_games_list_t));
	  if (*glist != NULL)
	    {
	      memset (*glist, 0, sizeof (hdl_games_list_t));
	      (*glist)->count = 0;
	      (*glist)->games = tmp;
	      (*glist)->total_chunks = (toc->slice[0].total_chunks +
					toc->slice[1].total_chunks);
	      (*glist)->free_chunks = (toc->slice[0].free_chunks +
				       toc->slice[1].free_chunks);

	      result = hdl_glist_read_slice (hio, *glist, toc, 0);
	      if (result == RET_OK)
		result = hdl_glist_read_slice (hio, *glist, toc, 1);

	      if (result != RET_OK)
		osal_free (*glist);
	    }
	  else
	    result = RET_NO_MEM;
	  if (result != RET_OK)
	    osal_free (tmp);
	}
      else
	result = RET_NO_MEM;

      apa_toc_free (toc);
    }
  return (result);
}


/**************************************************************/
void
hdl_glist_free (hdl_games_list_t *glist)
{
  if (glist != NULL)
    {
      osal_free (glist->games);
      osal_free (glist);
    }
}


/**************************************************************/
int
hdl_lookup_partition_ex (hio_t *hio,
			 const char *game_name,
			 char partition_id[PS2_PART_IDMAX + 1])
{
  /*@only@*/ hdl_games_list_t *games = NULL;
  int result;
  *partition_id = '\0';
  result = hdl_glist_read (hio, &games);
  if (result == RET_OK && games != NULL)
    {
      size_t i;
      int found = 0;
      for (i = 0; i < games->count; ++i)
	if (caseless_compare (game_name, games->games[i].name))
	  { /* found */
	    strcpy (partition_id, games->games[i].partition_name);
	    found = 1;
	    break;
	  }
      if (!found)
	result = RET_NOT_FOUND;

      hdl_glist_free (games);
    }
  return (result);
}


/**************************************************************/
int
hdl_lookup_partition (const dict_t *config,
		      const char *device_name,
		      const char *game_name,
		      char partition_id[PS2_PART_IDMAX + 1])
{
  /*@only@*/ hio_t *hio = NULL;
  int result;
  *partition_id = '\0';
  result = hio_probe (config, device_name, &hio);
  if (result == RET_OK && hio != NULL)
    {
      result = hdl_lookup_partition_ex (hio, game_name, partition_id);
      (void) hio->close (hio);
    }
  return (result);
}


/**************************************************************/
int
hdl_read_game_alloc_table (hio_t *hio,
			   const apa_toc_t *toc,
			   int slice_index,
			   u_int32_t partition_index,
			   hdl_game_alloc_table_t *gat)
{
  const apa_slice_t *slice = toc->slice + slice_index;
  const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
  unsigned char buffer[1024];
  u_int32_t len;
  u_int32_t sect = (get_u32 (&slice->parts[partition_index].header.start) +
		    0x00101000 / 512 + slice_index * SLICE_2_OFFS);
  int result = hio->read (hio, sect, 2, buffer, &len);
  if (result == OSAL_OK)
    {
      if (get_u32 (buffer) == 0xdeadfeed)
	{ /* 0xdeadfeed magic found */
	  const u_int32_t *data = (u_int32_t*) (buffer + 0x00f5);
	  u_int32_t i;

	  gat->count = buffer[0x00f0];
	  gat->size_in_kb = 0;
	  for (i = 0; i < gat->count; ++i)
	    {
	      gat->part[i].start = get_u32 (data + (i * 3 + 1)) << 8;
	      gat->part[i].len = get_u32 (data + (i * 3 + 2)) / 2;
	      gat->size_in_kb += gat->part[i].len / 2;
	    }
	}
      else
	result = RET_NOT_HDL_PART;
    }
  return (result);
}


/**************************************************************/
int
hdl_modify_game (hio_t *hio,
		 apa_toc_t *toc,
		 int slice_index,
		 u_int32_t starting_partition_sector,
		 const char *new_name, /* or NULL */
		 compat_flags_t new_compat_flags) /* or COMPAT_FLAGS_INVALID */
{
  apa_slice_t *slice = toc->slice + slice_index;
  const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
  apa_partition_t *part = NULL;
  u_int32_t i;
  u_int8_t hdl_hdr[1024];
  u_int32_t sector, bytes = 0;
  int result;

  for (i = 0; part == NULL && i < slice->part_count; ++i)
    if (get_u32 (&slice->parts[i].header.start) == starting_partition_sector)
      /* starting partition index located */
      part = slice->parts + i;
  if (part == NULL)
    return (RET_NOT_FOUND);

  /* BUG: hdl_modify_game wouldn't change part name in Sony HDD browser */
  sector = (get_u32 (&part->header.start) +
	    0x00101000 / 512 + slice_index * SLICE_2_OFFS);
  result = hio->read (hio, sector, 2, hdl_hdr, &bytes);
  if (result == RET_OK)
    {
      if (new_name != NULL && !toc->is_toxic)
	{ /* HD Loader partition naming: "PP.HDL.Game name" */
	  char part_id[PS2_PART_IDMAX];
	  int tmp_slice_index = 0;
	  u_int32_t tmp_partition_index = 0;
	  hdl_pname (new_name, part_id);
	  result = apa_find_partition (toc, part_id, &tmp_slice_index,
				       &tmp_partition_index);
	  if (result == RET_NOT_FOUND)
	    {
	      strcpy (part->header.id, part_id);
	      set_u32 (&part->header.checksum,
		       apa_partition_checksum (&part->header));
	      part->modified = 1;
	      result = RET_OK;
	    }
	  else if (result == RET_OK)
	    /* partition with such name already exists */
	    result = RET_PART_EXISTS;
	}

      if (result == RET_OK)
	{
	  if (new_name != NULL)
	    {
	      memset (hdl_hdr + 0x08, 0, 0xa0);
	      memcpy (hdl_hdr + 0x08, new_name, strlen (new_name));
	    }
	  if (new_compat_flags != COMPAT_FLAGS_INVALID)
	    set_u16 (hdl_hdr + 0xa8, new_compat_flags);
	  result = hio->write (hio, sector, 2, hdl_hdr, &bytes);
	}
      if (result == RET_OK)
	result = apa_commit_ex (hio, toc);
    }
  return (result);
}
