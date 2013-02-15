/*
 * hdl.c
 * $Id: hdl.c,v 1.8 2004/08/20 12:35:17 b081 Exp $
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

#include "iin.h"
#include "hdl.h"
#include "apa.h"
#include "retcodes.h"
#include "osal.h"
#include "common.h"
#include "hio_probe.h"


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
prepare_main (const char *game_name,
	      const char *signature,
	      const apa_partition_table_t *table,
	      size_t starting_partition_sector,
	      size_t size_in_kb,
	      int is_dvd,
	      unsigned char *buffer_4m)
{
  int result;
  size_t i, index = (size_t) -1;
  char *icon = NULL;
  size_t icon_length;
  char icon_props [1024];
  const ps2_partition_header_t *part;

  /* read icon */
  result = read_file ("./icon.bin", &icon, &icon_length);
  if (result == OSAL_OK)
    for (i=0; i<table->part_count; ++i)
      if (table->parts [i].header.start == starting_partition_sector)
	{ /* locate starting partition index */
	  index = i;
	  part = &table->parts [index].header;
	  /* verify apa */
	  result = part->checksum == apa_partition_checksum (part) ? RET_OK : RET_BAD_APA;
	  break;
	}

  if (result == OSAL_OK) {
    if (index != (size_t) -1)
      {
	unsigned long *tmp;
	size_t offset;
	size_t size_remaining_in_kb;
	size_t partition_usable_size_in_kb;
	size_t partition_data_len_in_kb;
	size_t partitions_used = 0;

	/* PS2 partition header */
	memset (buffer_4m, 0, 4 * 1024 * 1024);
	memcpy (buffer_4m, part, 1024);

	sprintf (icon_props, HDL_HDR2, game_name);

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
	tmp = (unsigned long*) (buffer_4m + 0x001010);
	tmp [0] = 0x0200;
	tmp [1] = strlen (HDL_HDR1);
	tmp [2] = 0x0400;
	tmp [3] = strlen (icon_props);
	tmp [4] = 0x0800;
	tmp [5] = icon_length;
	tmp [6] = 0x0800;
	tmp [7] = icon_length;

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
	memcpy (buffer_4m + 0x101008, game_name, strlen (game_name));

	/*
         *                                  +- compatibility modes
         *                                  v
	 *1010a0: 00 00 00 00 00 00 00 00  00 00 00 00 53 43 45 53  ............SCES
	 *1010b0: 5f 35 30 33 2e 36 30 00  00 00 00 00 00 00 00 00  _503.60.........
	 */
	memcpy (buffer_4m + 0x1010ac, signature, strlen (signature));

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
	 *	            |           +- x << 8 = data start in sectors
	 *	            +- BE, x * 512 = part offset in megabytes, incremental counter [!]
	 */

	/* that is aerial acrobatics :-) */
	buffer_4m [0x1010ec] = is_dvd ? 0x14 : 0x12;
	buffer_4m [0x1010f0] = (unsigned char) (part->nsub + 1);
	tmp = (unsigned long*) (buffer_4m + 0x1010f5);

	partition_usable_size_in_kb = (part->length - 0x2000) / 2; /* 2 sectors == 1K */
	partition_data_len_in_kb =
	  size_in_kb < partition_usable_size_in_kb ?
	  size_in_kb : partition_usable_size_in_kb;

	/* offset, start, data length */
	*tmp++ = 0; /* offset */
	*tmp++ = (part->start + 0x2000) >> 8; /* 0x2000 sectors = 4M (data offset for 1st part.) */
	*tmp++ = partition_data_len_in_kb * 4;
	++partitions_used;

	offset = (part->length - 0x2000) / 1024;
	size_remaining_in_kb = size_in_kb - partition_data_len_in_kb;
	for (i=0; size_remaining_in_kb>0 && i<part->nsub; ++i)
	  {
	    partition_usable_size_in_kb = (part->subs [i].length - 0x0800) / 2;
	    partition_data_len_in_kb =
	      size_remaining_in_kb < partition_usable_size_in_kb ?
	      size_remaining_in_kb : partition_usable_size_in_kb;

	    /* offset, start, data length */
	    *tmp++ = offset;
	    *tmp++ = (part->subs [i].start + 0x0800) >> 8; /* 0x0800 sec = 1M (sub part. offs) */
	    *tmp++ = partition_data_len_in_kb * 4;
	    ++partitions_used;

	    offset += (part->subs [i].length - 0x0800) / 1024;
	    size_remaining_in_kb -= partition_data_len_in_kb;
	  }

	buffer_4m [0x1010f0] = (unsigned char) partitions_used;
      }
    else
      result = RET_NOT_FOUND;
  }

  if (icon != NULL)
    osal_free (icon);

  return (result);
}


/**************************************************************/
int
hdl_extract (const char *device_name,
	     const char *game_name,
	     const char *output_file,
	     progress_t *pgs)
{
  apa_partition_table_t *table;

  int result = apa_ptable_read (device_name, &table);
  if (result == RET_OK)
    {
      size_t partition_index;
      result = apa_find_partition (table, game_name, &partition_index);
      if (result == RET_NOT_FOUND)
	{ /* use heuristics - look among the HD Loader partitions */
	  char tmp [PS2_PART_IDMAX + 8];
	  strcpy (tmp, "PP.HDL.");
	  strcat (tmp, game_name);
	  result = apa_find_partition (table, tmp, &partition_index);
	}

      if (result == RET_OK)
	{ /* partition found */
	  const size_t PART_SYSDATA_SIZE = 4 _MB;
	  unsigned char *buffer = osal_alloc (PART_SYSDATA_SIZE);
	  if (buffer != NULL)
	    {
	      hio_t *hio;
	      result = hio_probe (device_name, &hio);
	      if (result == OSAL_OK)
		{
		  size_t len;

		  result = hio->read (hio, table->parts [partition_index].header.start,
				      PART_SYSDATA_SIZE / 512, buffer, &len);
		  if (result == OSAL_OK)
		    {
		      size_t num_parts = buffer [0x001010f0];
		      const size_t *data = (size_t*) (buffer + 0x001010f5);
		      size_t i;

		      if (buffer [0x00101000] == 0xed &&
			  buffer [0x00101001] == 0xfe &&
			  buffer [0x00101002] == 0xad &&
			  buffer [0x00101003] == 0xde)
			{ /* 0xdeadfeed magic found */
			  osal_handle_t file;
			  bigint_t total_size = 0;

			  /* calculate output file size */
			  for (i=0; i<num_parts; ++i)
			    total_size += ((bigint_t) data [i * 3 + 2]) << 8;

			  pgs_prepare (pgs, total_size);

			  /* create file and copy data */
			  result = osal_create_file (output_file, &file, total_size);
			  if (result == OSAL_OK)
			    {
			      unsigned char *aligned = (void*) (((long) buffer + 511) & ~511);
			      for (i=0; result == OSAL_OK && i<num_parts; ++i)
				{ /* seek and copy */
				  size_t start_s = data [i * 3 + 1] << 8;
				  size_t length_s = data [i * 3 + 2] / 2;
				  bigint_t curr = 0;
				  while (length_s > 0 && result == OSAL_OK)
				    {
				      size_t count_s =
					length_s < IIN_NUM_SECTORS ? length_s : IIN_NUM_SECTORS;
				      result = hio->read (hio, start_s, count_s,
							  aligned, &len);
				      if (result == OSAL_OK)
					{
					  size_t written;
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
inject_data (const char *path,
	     const apa_partition_table_t *table,
	     size_t starting_partition_sector,
	     iin_t *iin,
	     size_t size_in_kb,
	     const char *game_name,
	     const char *game_signature,
	     int is_dvd,
	     progress_t *pgs)
{
  int result;
  size_t i;
  const ps2_partition_header_t *part;
  char *buffer = osal_alloc (4 _MB);

  if (buffer != NULL)
    {
      part = NULL;
      for (i=0; i<table->part_count; ++i)
	if (table->parts [i].header.start == starting_partition_sector)
	  { /* locate starting partition index */
	    part = &table->parts [i].header;
	    /* verify apa */
	    result = part->checksum == apa_partition_checksum (part) ? RET_OK : RET_BAD_APA;
	    break;
	  }
      if (part == NULL) /* not found */
	result = RET_NOT_FOUND;
    }
  else
    result = RET_NO_MEM;

  if (result == RET_OK)
    {
      result = prepare_main (game_name, game_signature,
			     table, starting_partition_sector,
			     size_in_kb, is_dvd,
			     (unsigned char*) buffer);
      if (result == RET_OK)
	{
	  hio_t *hio;
	  result = hio_probe (path, &hio);
	  if (result == OSAL_OK)
	    {
	      size_t kb_remaining = size_in_kb;
	      size_t start_sector = 0;
	      size_t bytes;
	      size_t main_hdr_size_s = (4 _MB) / 512;

	      pgs_prepare (pgs, (bigint_t) (size_in_kb + 4 * 1024) * 1024);

	      /* first: write main partition header (4MB total) */
	      result = hio->write (hio, part->start, main_hdr_size_s, buffer, &bytes);
	      if (result == OSAL_OK)
		result = bytes == 4 _MB ? OSAL_OK : OSAL_ERR;
	      osal_free (buffer), buffer = NULL;

	      /* track header, otherwise it would influence the progress calculation */
	      pgs_update (pgs, 4 _MB);
	      pgs_chunk_complete (pgs);

	      /* next: fill-in 1st partition */
	      if (result == RET_OK)
		{
		  bigint_t part_size = ((bigint_t) part->length) * 512 - (4 _MB);
		  bigint_t chunk_length_in_bytes =
		    ((bigint_t) kb_remaining) * 1024 < part_size ?
		    ((bigint_t) kb_remaining) * 1024 : part_size;

		  result = iin_copy_ex (iin, hio, start_sector, part->start + main_hdr_size_s,
					chunk_length_in_bytes / IIN_SECTOR_SIZE, pgs);
		  pgs_chunk_complete (pgs);
		  start_sector += chunk_length_in_bytes / IIN_SECTOR_SIZE;
		  kb_remaining -= (size_t) (chunk_length_in_bytes / 1024);
		}

	      for (i=0; result == OSAL_OK && kb_remaining>0 && i!=part->nsub; ++i)
		{ /* next: fill-in remaining partitions */
		  size_t sub_part_start_s = part->subs [i].start;
		  bigint_t sub_part_size = ((bigint_t) part->subs [i].length) * 512 - (1 _MB);
		  size_t sub_part_hdr_size_s = (1 _MB) / 512;
		  bigint_t chunk_length =
		    ((bigint_t) kb_remaining) * 1024 < sub_part_size ?
		    ((bigint_t) kb_remaining) * 1024 : sub_part_size;

		  result = iin_copy_ex (iin, hio, start_sector,
					sub_part_start_s + sub_part_hdr_size_s,
					chunk_length / IIN_SECTOR_SIZE, pgs);
		  pgs_chunk_complete (pgs);
		  start_sector += chunk_length / IIN_SECTOR_SIZE;
		  kb_remaining -= (size_t) (chunk_length / 1024);
		}

	      if (result == RET_OK &&
		  kb_remaining != 0)
		result = RET_NO_SPACE; /* the game does not fit in the allocated space... why? */

	      /* finally: commit partition table */
	      if (result == OSAL_OK)
		result = apa_commit_ex (hio, table);

	      result = hio->close (hio) == OSAL_OK ? result : OSAL_ERR;
	    }
	}
    }

  if (buffer != NULL)
    osal_free (buffer);

  return (result);
}


/**************************************************************/
int
hdl_inject (const char *device_name,
	    const char *game_name,
	    const char *game_signature,
	    const char *input_path,
	    int input_is_dvd,
	    progress_t *pgs)
{
  apa_partition_table_t *table = NULL;
  int result = apa_ptable_read (device_name, &table);
  if (result == OSAL_OK)
    {
      iin_t *iin;
      result = iin_probe (input_path, &iin);
      if (result == OSAL_OK)
	{
	  size_t sector_size, num_sectors;
	  result = iin->stat (iin, &sector_size, &num_sectors);
	  if (result == OSAL_OK)
	    {
	      bigint_t input_size = (bigint_t) num_sectors * (bigint_t) sector_size;
	      size_t size_in_kb = (size_t) (input_size / 1024);
	      size_t size_in_mb = (size_t) ((input_size + (1 _MB - 1)) / (1 _MB));
	      char partition_name [PS2_PART_IDMAX];
	      size_t new_partition_start;
	      char *p;

	      size_t game_name_len = PS2_PART_IDMAX - 1 - 7;
	      game_name_len =
		strlen (game_name) < game_name_len ? strlen (game_name) : game_name_len;
	      strcpy (partition_name, "PP.HDL.");
	      memcpy (partition_name + 7, game_name, game_name_len);
	      partition_name [7 + game_name_len] = '\0'; /* limit game name length */

	      p = partition_name + 7; /* len ("PP.HDL.") */
	      while (*p)
		{
		  if (!isalnum (*p) && *p != ' ')
		    *p = '_'; /* escape some characters with `_' */
		  ++p;
		}
	      result = apa_allocate_space (table, partition_name, size_in_mb,
					   &new_partition_start, 0); /* order by size desc */
	      if (result == RET_OK)
		{
		  result = inject_data (device_name,
					table,
					new_partition_start,
					iin,
					size_in_kb,
					game_name,
					game_signature,
					input_is_dvd,
					pgs);
		}
	    }
	  iin->close (iin);
	}
    }

  if (table != NULL)
    apa_ptable_free (table);

  return (result);
}
