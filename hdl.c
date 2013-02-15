/*
 * hdl.c
 * $Id: hdl.c,v 1.15 2006/05/21 21:36:41 bobi Exp $
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
#include "hio_probe.h"
#if defined (BUILTIN_ICON)
#  include "icon.h"
#endif


/*
 * notice: that code would only run on a big-endian machine
 */


static const char *HDL_HDR0 =
"PS2ICON3D";

static const char *HDL_HDR1 =
"BOOT2 = cdrom0:\\SLUS_202.02;1\n"
"VER = 1.00\n"
"VMODE = PAL\n"
"HDDUNITPOWER = NICHDD\n";

static const char *HDL_HDR2 =
"PS2X\n"
"title0 = HD Loader\n"
"title1 = %s\n"
"bgcola = 64\n"
"bgcol0 = 22, 47, 92\n"
"bgcol1 = 3, 10, 28\n"
"bgcol2 = 3, 10, 28\n"
"bgcol3 = 22, 47, 92\n"
"lightdir0 = 0.5, 0.5, 0.5\n"
"lightdir1 = 0.0, -0.4, -1.0\n"
"lightdir2 = -0.5, -0.5, 0.5\n"
"lightcolamb = 31, 31, 31\n"
"lightcol0 = 62, 62, 55\n"
"lightcol1 = 33, 42, 64\n"
"lightcol2 = 18, 18, 49\n"
"uninstallmes0 =\n"
"uninstallmes1 =\n"
"uninstallmes2 =\n";


/**************************************************************/
static int
prepare_main (const hdl_game_t *details,
	      const apa_partition_table_t *table,
	      u_int32_t starting_partition_sector,
	      u_int32_t size_in_kb,
	      unsigned char *buffer_4m)
{
  int result;
  u_int32_t i, index = (u_int32_t) -1;
#if defined (BUILTIN_ICON)
  const char *icon = (const char*) hdloader_icon;
  u_int32_t icon_length = HDLOADER_ICON_LEN;
#else
  char *icon = NULL;
  u_int32_t icon_length;
#endif
  char icon_props [1024];
  const ps2_partition_header_t *part;

  part = NULL;
#if !defined (BUILTIN_ICON)
  /* read icon */
  result = read_file ("./icon.bin", &icon, &icon_length);
#else
  /* icon is embedded in the executable */
  result = OSAL_OK;
#endif
  if (result == OSAL_OK)
    for (i=0; i<table->part_count; ++i)
      if (get_u32 (&table->parts [i].header.start) == starting_partition_sector)
	{ /* locate starting partition index */
	  index = i;
	  part = &table->parts [index].header;
	  /* verify apa */
	  result = (get_u32 (&part->checksum) == apa_partition_checksum (part) ?
		    RET_OK : RET_BAD_APA);
	  break;
	}

  if (result == OSAL_OK) {
    if (index != (u_int32_t) -1)
      {
	u_int32_t *tmp;
	u_int32_t offset;
	u_int32_t size_remaining_in_kb;
	u_int32_t partition_usable_size_in_kb;
	u_int32_t partition_data_len_in_kb;
	u_int32_t partitions_used = 0;

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
	 *  1200: 42 4f 4f 54 32 20 3d 20  63 64 72 6f 6d 30 3a 5c  BOOT2 = cdrom0:\
	 *  1210: 53 4c 55 53 5f 32 30 32  2e 30 32 3b 31 0a 56 45  SLUS_202.02;1.VE
	 *  1220: 52 20 3d 20 31 2e 30 30  0a 56 4d 4f 44 45 20 3d  R = 1.00.VMODE =
	 *  1230: 20 50 41 4c 0a 48 44 44  55 4e 49 54 50 4f 57 45   PAL.HDDUNITPOWE
	 *  1240: 52 20 3d 20 4e 49 43 48  44 44 0a 00 00 00 00 00  R = NICHDD......
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
	buffer_4m [0x101000] = 0xed;
	buffer_4m [0x101001] = 0xfe;
	buffer_4m [0x101002] = 0xad;
	buffer_4m [0x101003] = 0xde;
	buffer_4m [0x101006] = 0x01;
	memcpy (buffer_4m + 0x101008, details->name, strlen (details->name));

	/*
         *                                  +- compatibility modes
         *                                  v
	 *1010a0: 00 00 00 00 00 00 00 00  00 00 00 00 53 43 45 53  ............SCES
	 *1010b0: 5f 35 30 33 2e 36 30 00  00 00 00 00 00 00 00 00  _503.60.........
	 */
	set_u16 (buffer_4m + 0x1010a8, details->compat_flags);
	memcpy (buffer_4m + 0x1010ac, details->startup, strlen (details->startup));

	/*
	 *	            +- media type? 0x14 for DVD, 0x12 for CD, 0x10 for PSX CD (w/o audio)?
	 *	            |           +- number of partitions
	 *	            v           v
	 *	           14 00 00 00 05 00 00 00 00 
	 *1010f5: 00 00 00 00 20 50 00 00 00 c0 1f 00	;; 0x00500000, 512MB
	 *	  f8 03 00 00 08 60 00 00 00 f0 1f 00	;; 0x00600000, 512MB
	 *	  f6 07 00 00 08 70 00 00 00 f0 1f 00	;; 0x00700000, 512MB
	 *	  f4 0b 00 00 08 44 00 00 00 f0 07 00	;; 0x00440000, 128MB
	 *	  f2 0c 00 00 08 48 00 00 00 25 01 00	;; 0x00480000, 128MB
	 *	   ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^
	 *	   +--+--+--+  +--+--+--+  +--+--+--+- BE, x / 4 = part size in kilobytes [!]
	 *	            |           +- x << 8 = data start in HDD-sectors
	 *	            +- BE, x * 512 = part offset in megabytes, incremental counter [!]
	 */

	/* that is aerial acrobatics :-) */
	set_u32 (buffer_4m + 0x1010e8, details->layer_break);
	buffer_4m [0x1010ec] = details->is_dvd ? 0x14 : 0x12;
	buffer_4m [0x1010f0] = (u_int8_t) (get_u32 (&part->nsub) + 1);
	tmp = (u_int32_t*) (buffer_4m + 0x1010f5);

	partition_usable_size_in_kb = (get_u32 (&part->length) - 0x2000) / 2; /* 2 sectors == 1K */
	partition_data_len_in_kb =
	  size_in_kb < partition_usable_size_in_kb ?
	  size_in_kb : partition_usable_size_in_kb;

	/* offset, start, data length */
	set_u32 (tmp++, 0); /* offset */
	/* 0x2000 sec = 4M (offs for 1st part.) */
	set_u32 (tmp++, (get_u32 (&part->start) + 0x2000) >> 8);
	set_u32 (tmp++, partition_data_len_in_kb * 4);
	++partitions_used;

	offset = (get_u32 (&part->length) - 0x2000) / 1024;
	size_remaining_in_kb = size_in_kb - partition_data_len_in_kb;
	for (i=0; size_remaining_in_kb>0 && i<get_u32 (&part->nsub); ++i)
	  {
	    partition_usable_size_in_kb = (get_u32 (&part->subs [i].length) - 0x0800) / 2;
	    partition_data_len_in_kb =
	      size_remaining_in_kb < partition_usable_size_in_kb ?
	      size_remaining_in_kb : partition_usable_size_in_kb;

	    /* offset, start, data length */
	    set_u32 (tmp++, offset);
	    /* 0x0800 sec = 1M (sub part. offs) */
	    set_u32 (tmp++, (get_u32 (&part->subs [i].start) + 0x0800) >> 8);
	    set_u32 (tmp++, partition_data_len_in_kb * 4);
	    ++partitions_used;

	    offset += (get_u32 (&part->subs [i].length) - 0x0800) / 1024;
	    size_remaining_in_kb -= partition_data_len_in_kb;
	  }

	buffer_4m [0x1010f0] = (u_int8_t) partitions_used;
      }
    else
      result = RET_NOT_FOUND;
  }

#if !defined (BUILTIN_ICON)
  if (icon != NULL)
    osal_free (icon);
#endif

  return (result);
}


/**************************************************************/
void
hdl_pname (const char *name,
	   char partition_name [PS2_PART_IDMAX + 1])
{
  u_int32_t game_name_len = PS2_PART_IDMAX - 1 - 7; /* limit partition name length */
  char *p;

  game_name_len = strlen (name) < game_name_len ? strlen (name) : game_name_len;
  strcpy (partition_name, "PP.HDL.");
  memcpy (partition_name + 7, name, game_name_len);
  partition_name [7 + game_name_len] = '\0';
  p = partition_name + 7; /* len ("PP.HDL.") */
  while (*p)
    {
      if (!isalnum (*p) && *p != ' ' && *p != '.')
	*p = '_'; /* escape non-alphanumeric characters with `_' */
      ++p;
    }
}


/**************************************************************/
int
hdl_extract (const dict_t *config,
	     const char *device_name,
	     const char *game_name,
	     const char *output_file,
	     progress_t *pgs)
{
  apa_partition_table_t *table;

  int result = apa_ptable_read (config, device_name, &table);
  if (result == RET_OK)
    {
      u_int32_t partition_index;
      result = apa_find_partition (table, game_name, &partition_index);
      if (result == RET_NOT_FOUND)
	{ /* assume it is `game_name' and not a partition name */
	  char partition_id [PS2_PART_IDMAX + 8];
	  result = hdl_lookup_partition (config, device_name, game_name, partition_id);
	  if (result == RET_OK)
	    result = apa_find_partition (table, partition_id, &partition_index);
	}

      if (result == RET_OK)
	{ /* partition found */
	  const u_int32_t PART_SYSDATA_SIZE = 4 _MB;
	  u_int8_t *buffer = osal_alloc (PART_SYSDATA_SIZE);
	  if (buffer != NULL)
	    {
	      hio_t *hio;
	      result = hio_probe (config, device_name, &hio);
	      if (result == OSAL_OK)
		{
		  u_int32_t len;

		  result = hio->read (hio,
				      get_u32 (&table->parts [partition_index].header.start),
				      PART_SYSDATA_SIZE / 512, buffer, &len);
		  if (result == OSAL_OK)
		    {
		      u_int32_t num_parts = buffer [0x001010f0];
		      const u_int32_t *data = (u_int32_t*) (buffer + 0x001010f5);
		      u_int32_t i;

		      if (buffer [0x00101000] == 0xed &&
			  buffer [0x00101001] == 0xfe &&
			  buffer [0x00101002] == 0xad &&
			  buffer [0x00101003] == 0xde)
			{ /* 0xdeadfeed magic found */
			  osal_handle_t file;
			  u_int64_t total_size = 0;

			  /* calculate output file size */
			  for (i=0; i<num_parts; ++i)
			    total_size += ((u_int64_t) get_u32 (data + i * 3 + 2)) << 8;

			  pgs_prepare (pgs, total_size);

			  /* create file and copy data */
			  result = osal_create_file (output_file, &file, total_size);
			  if (result == OSAL_OK)
			    {
			      u_int8_t *aligned = (void*) (((long) buffer + 511) & ~511);
			      for (i=0; result == OSAL_OK && i<num_parts; ++i)
				{ /* seek and copy */
				  u_int32_t start_s = get_u32 (data + i * 3 + 1) << 8;
				  u_int32_t length_s = get_u32 (data + i * 3 + 2) / 2;
				  u_int64_t curr = 0;
				  while (length_s > 0 && result == OSAL_OK)
				    {
				      u_int32_t count_s =
					length_s < IIN_NUM_SECTORS ? length_s : IIN_NUM_SECTORS;
				      result = hio->read (hio, start_s, count_s,
							  aligned, &len);
				      if (result == OSAL_OK)
					{
					  u_int32_t written;
					  result = osal_write (file, aligned,
							       count_s * 512, &written);
					  if (result == OSAL_OK)
					    {
					      start_s += count_s;
					      length_s -= count_s;
					      curr += count_s * 512;
					      pgs_update (pgs, curr);
					    }
					}
				    }
				  pgs_chunk_complete (pgs);
				} /* for */
			      result = osal_close (file) == OSAL_OK ? result : OSAL_ERR;
			    }
			}
		      else
			result = RET_NOT_HDL_PART;
		    }
		  hio->close (hio);
		}
	      osal_free (buffer);
	    }
	  else
	    result = RET_NO_MEM;
	}
      apa_ptable_free (table);
    }
  return (result);
}


/**************************************************************/
static int
inject_data (const dict_t *config,
	     hio_t *hio,
	     const apa_partition_table_t *table,
	     u_int32_t starting_partition_sector,
	     iin_t *iin,
	     u_int32_t size_in_kb,
	     const hdl_game_t *details,
	     progress_t *pgs)
{
  int result;
  u_int32_t i;
  const ps2_partition_header_t *part;
  char *buffer = osal_alloc (4 _MB);

  part = NULL;
  if (buffer != NULL)
    {
      result = RET_NOT_FOUND;
      for (i=0; i<table->part_count; ++i)
	if (get_u32 (&table->parts [i].header.start) == starting_partition_sector)
	  { /* locate starting partition index */
	    part = &table->parts [i].header;
	    /* verify apa */
	    result = (get_u32 (&part->checksum) == apa_partition_checksum (part) ?
		      RET_OK : RET_BAD_APA);
	    break;
	  }
    }
  else
    result = RET_NO_MEM;

  if (result == RET_OK)
    {
      result = prepare_main (details, table, starting_partition_sector,
			     size_in_kb, (u_int8_t*) buffer);
      if (result == RET_OK)
	{
	  u_int32_t kb_remaining = size_in_kb;
	  u_int32_t start_sector = 0;
	  u_int32_t bytes;
	  u_int32_t main_hdr_size_s = (4 _MB) / 512;

	  pgs_prepare (pgs, (u_int64_t) (size_in_kb + 4 * 1024) * 1024);

	  /* first: write main partition header (4MB total) */
	  result = hio->write (hio, get_u32 (&part->start), main_hdr_size_s, buffer, &bytes);
	  if (result == OSAL_OK)
	    result = bytes == 4 _MB ? OSAL_OK : OSAL_ERR;
	  osal_free (buffer), buffer = NULL;

	  /* track header, otherwise it would influence the progress calculation */
	  pgs_update (pgs, 4 _MB);
	  pgs_chunk_complete (pgs);

	  /* next: fill-in 1st partition */
	  if (result == RET_OK)
	    {
	      u_int64_t part_size = ((u_int64_t) get_u32 (&part->length)) * 512 - (4 _MB);
	      u_int64_t chunk_length_in_bytes =
		((u_int64_t) kb_remaining) * 1024 < part_size ?
		((u_int64_t) kb_remaining) * 1024 : part_size;

	      result = iin_copy_ex (iin, hio, start_sector,
				    get_u32 (&part->start) + main_hdr_size_s,
				    (u_int32_t) (chunk_length_in_bytes / IIN_SECTOR_SIZE),
				    pgs);
	      pgs_chunk_complete (pgs);
	      start_sector += (u_int32_t) (chunk_length_in_bytes / IIN_SECTOR_SIZE);
	      kb_remaining -= (u_int32_t) (chunk_length_in_bytes / 1024);
	    }

	  for (i=0; result == OSAL_OK && kb_remaining>0 && i!=get_u32 (&part->nsub); ++i)
	    { /* next: fill-in remaining partitions */
	      u_int32_t sub_part_start_s = get_u32 (&part->subs [i].start);
	      u_int64_t sub_part_size = (((u_int64_t) get_u32 (&part->subs [i].length)) *
					 512 - (1 _MB));
	      u_int32_t sub_part_hdr_size_s = (1 _MB) / 512;
	      u_int64_t chunk_length =
		((u_int64_t) kb_remaining) * 1024 < sub_part_size ?
		((u_int64_t) kb_remaining) * 1024 : sub_part_size;

	      result = iin_copy_ex (iin, hio, start_sector,
				    sub_part_start_s + sub_part_hdr_size_s,
				    (u_int32_t) (chunk_length / IIN_SECTOR_SIZE), pgs);
	      pgs_chunk_complete (pgs);
	      start_sector += (u_int32_t) (chunk_length / IIN_SECTOR_SIZE);
	      kb_remaining -= (u_int32_t) (chunk_length / 1024);
	    }

	  if (result == RET_OK &&
	      kb_remaining != 0)
	    result = RET_NO_SPACE; /* the game does not fit in the allocated space... why? */

	  /* finally: commit partition table */
	  if (result == OSAL_OK)
	    result = apa_commit_ex (config, hio, table);
	}
    }

  if (buffer != NULL)
    osal_free (buffer);

  return (result);
}


/**************************************************************/
int
hdl_inject (const dict_t *config,
	    hio_t *hio,
	    iin_t *iin,
	    hdl_game_t *details,
	    progress_t *pgs)
{
  apa_partition_table_t *table = NULL;
  int result = apa_ptable_read_ex (config, hio, &table);
  if (result == OSAL_OK)
    {
      u_int32_t sector_size, num_sectors;
      result = iin->stat (iin, &sector_size, &num_sectors);
      if (result == OSAL_OK)
	{
	  u_int64_t input_size = (u_int64_t) num_sectors * (u_int64_t) sector_size;
	  u_int32_t size_in_kb = (u_int32_t) (input_size / 1024);
	  u_int32_t size_in_mb = (u_int32_t) ((input_size + (1 _MB - 1)) / (1 _MB));
	  u_int32_t new_partition_start;

	  if (details->partition_name [0] == '\0')
	    {
	      if (strcmp (dict_lookup (config, CONFIG_PARTITION_NAMING),
			  CONFIG_PARTITION_NAMING_TOXICOS) == 0)
		/* Toxic OS partition naming: "PP.HDL.STARTUP" */
		hdl_pname (details->startup, details->partition_name);
	      else if (strcmp (dict_lookup (config, CONFIG_PARTITION_NAMING),
			       CONFIG_PARTITION_NAMING_STANDARD) == 0)
		/* HD Loader partition naming: "PP.HDL.Game name" */
		hdl_pname (details->name, details->partition_name);
	      else
		/* revert to standard when unknown */
		hdl_pname (details->name, details->partition_name);
	    }

	  result = apa_allocate_space (table, details->partition_name, size_in_mb,
				       &new_partition_start, 0); /* order by size desc */
	  if (result == RET_OK)
	    {
	      result = inject_data (config, hio, table, new_partition_start,
				    iin, size_in_kb, details, pgs);
	    }
	}
    }

  if (table != NULL)
    apa_ptable_free (table);

  return (result);
}


/**************************************************************/
static int
hdl_ginfo_read (hio_t *hio,
		const ps2_partition_header_t *part,
		hdl_game_info_t *ginfo)
{
  u_int32_t i, size;
  /* data we're interested in starts @ 0x101000 and is header
   * plus information for up to 65 partitions
   * (1 main + 64 sub) by 12 bytes each */
  const u_int32_t offset = 0x101000;
  char buffer [1024];
  int result;
  u_int32_t bytes;

  result = hio->read (hio, get_u32 (&part->start) + offset / 512, 2, buffer, &bytes);
  if (result == RET_OK)
    {
      if (bytes == 1024)
	{
	  /* calculate total size */
	  size = get_u32 (&part->length);
	  for (i=0; i<get_u32 (&part->nsub); ++i)
	    size += get_u32 (&part->subs [i].length);

	  memcpy (ginfo->partition_name, part->id, PS2_PART_IDMAX);
	  ginfo->partition_name [PS2_PART_IDMAX] = '\0';
	  strcpy (ginfo->name, buffer + 0x101008 - offset);
	  strcpy (ginfo->startup, buffer + 0x1010ac - offset);
	  ginfo->compat_flags = buffer [0x1010a8 - offset];
	  ginfo->is_dvd = buffer [0x1010ec - offset] == 0x14;
	  ginfo->start_sector = get_u32 (&part->start);
	  ginfo->total_size_in_kb = size / 2;
	}
      else
	result = RET_ERR;
    }

  return (result);
}


/**************************************************************/
int
hdl_glist_read (const dict_t *config,
		hio_t *hio,
		hdl_games_list_t **glist)
{
  apa_partition_table_t *ptable;
  int result;

  result = apa_ptable_read_ex (config, hio, &ptable);
  if (result == RET_OK)
    {
      u_int32_t i, count = 0;
      void *tmp;
      for (i=0; i<ptable->part_count; ++i)
	count += (get_u16 (&ptable->parts [i].header.flags) == 0x00 &&
		  get_u16 (&ptable->parts [i].header.type) == 0x1337);

      tmp = osal_alloc (sizeof (hdl_game_info_t) * count);
      if (tmp != NULL)
	{
	  memset (tmp, 0, sizeof (hdl_game_info_t) * count);
	  *glist = osal_alloc (sizeof (hdl_games_list_t));
	  if (*glist != NULL)
	    {
	      u_int32_t index = 0;
	      memset (*glist, 0, sizeof (hdl_games_list_t));
	      (*glist)->count = count;
	      (*glist)->games = tmp;
	      (*glist)->total_chunks = ptable->total_chunks;
	      (*glist)->free_chunks = ptable->free_chunks;
	      for (i=0; result==RET_OK&&i<ptable->part_count; ++i)
		{
		  const ps2_partition_header_t *part = &ptable->parts [i].header;
		  if (get_u16 (&part->flags) == 0x00 && get_u16 (&part->type) == 0x1337)
		    result = hdl_ginfo_read (hio, part, (*glist)->games + index++);
		}

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

      apa_ptable_free (ptable);
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
hdl_lookup_partition_ex (const dict_t *config,
			 hio_t *hio,
			 const char *game_name,
			 char partition_id [PS2_PART_IDMAX + 1])
{
  hdl_games_list_t *games = NULL;
  int result = hdl_glist_read (config, hio, &games);
  if (result == RET_OK)
    {
      int i, found = 0;
      for (i=0; i<games->count; ++i)
	if (caseless_compare (game_name, games->games [i].name))
	  { /* found */
	    strcpy (partition_id, games->games [i].partition_name);
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
		      char partition_id [PS2_PART_IDMAX + 1])
{
  hio_t *hio = NULL;
  int result = hio_probe (config, device_name, &hio);
  if (result == RET_OK)
    {
      result = hdl_lookup_partition_ex (config, hio, game_name, partition_id);
      hio->close (hio);
    }
  return (result);
}
