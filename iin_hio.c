/*
 * iin_hio.c, based on iin_net.c, v 1.7
 * $Id: iin_hio.c,v 1.1 2006/09/01 17:37:58 bobi Exp $
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
#include <stdlib.h>
#include "iin_hio.h"
#include "osal.h"
#include "retcodes.h"
#include "aligned.h"
#include "apa.h"
#include "hdl.h"
#include "net_io.h"
#include "hio.h"


#define _MB * (1024 * 1024) /* really ugly :-) */


typedef struct iin_hio_type
{
  iin_t iin;
  hio_t *hio;
  char *unaligned, *buffer;

  u_int32_t num_parts;
  struct hdl_part_t
  {
    /* in 512-byte long sectors, relative to the RAW HDD */
    u_int32_t start_s, length_s;
    u_int64_t offset; /* in bytes, relative to CD start */
  } parts[64];
  const struct hdl_part_t *last_part; /* last hit */
} iin_hio_t;


/**************************************************************/
static int
hio_stat (iin_t *iin,
	  u_int32_t *sector_size,
	  u_int32_t *num_sectors)
{
  iin_hio_t *net = (iin_hio_t*) iin;
  u_int64_t total_size = 0;
  u_int32_t i;
  for (i = 0; i < net->num_parts; ++i)
    total_size += (u_int64_t) net->parts[i].length_s * HDD_SECTOR_SIZE;
  *sector_size = IIN_SECTOR_SIZE;
  *num_sectors = (u_int32_t) (total_size / IIN_SECTOR_SIZE);
  return (OSAL_OK);
}


/**************************************************************/
static int
hio_read (iin_t *iin,
	  u_int32_t start_sector,
	  u_int32_t num_sectors,
	  const char **data,
	  u_int32_t *length)
{
  int result;
  iin_hio_t *net = (iin_hio_t*) iin;
  u_int64_t start_offset = (u_int64_t) start_sector * IIN_SECTOR_SIZE;
  u_int64_t abs_offset, bytes_til_part_end;
  u_int32_t data_len;
  /* find the partition where requested sectors are */
  if (net->last_part->offset <= start_offset &&
      start_offset < net->last_part->offset + (u_int64_t)net->last_part->length_s * HDD_SECTOR_SIZE)
    ; /* same partition as the last time */
  else
    { /* make a linear scan to locate the partition */
      int found = 0;
      u_int32_t i;
      for (i = 0; i < net->num_parts; ++i)
	{
	  const struct hdl_part_t *part = net->parts + i;
	  if (part->offset <= start_offset &&
	      start_offset < part->offset + (u_int64_t) part->length_s * HDD_SECTOR_SIZE)
	    { /* found */
	      net->last_part = part;
	      found = 1;
	      break;
	    }
	}
      if (!found)
	{
	  *length = 0; /* should be behind end-of-file? */
	  return (OSAL_OK);
	}
    }

  abs_offset = ((u_int64_t) net->last_part->start_s * HDD_SECTOR_SIZE +
		start_offset - net->last_part->offset);
  bytes_til_part_end = ((u_int64_t) net->last_part->length_s * HDD_SECTOR_SIZE -
			(start_offset - net->last_part->offset));
  data_len = num_sectors * IIN_SECTOR_SIZE;
  if (data_len > bytes_til_part_end)
    data_len = (u_int32_t) bytes_til_part_end;
  result = net->hio->read (net->hio, abs_offset / HDD_SECTOR_SIZE,
			   data_len / HDD_SECTOR_SIZE, net->buffer, length);
  if (result == RET_OK)
    *data = net->buffer;
  return (result);
}


/**************************************************************/
static int
hio_close (iin_t *iin)
{
  iin_hio_t *net = (iin_hio_t*) iin;
  osal_free (net->unaligned);
  net->hio->close (net->hio);
  osal_free (net);
  return (RET_OK);
}


/**************************************************************/
static char*
hio_last_error (iin_t *iin)
{
  iin_hio_t *net = (iin_hio_t*) iin;
  return (net->hio->last_error (net->hio));
}


/**************************************************************/
static void
hio_dispose_error (iin_t *iin,
		   char* error)
{
  iin_hio_t *net = (iin_hio_t*) iin;
  net->hio->dispose_error (net->hio, error);
}


/**************************************************************/
static iin_hio_t*
hio_alloc (hio_t *hio,
	   const hdl_game_alloc_table_t *gtoc)
{
  iin_hio_t *net = osal_alloc (sizeof (iin_hio_t));
  if (net != NULL)
    {
      iin_t *iin = &net->iin;
      char *buffer =
	osal_alloc (IIN_SECTOR_SIZE * IIN_NUM_SECTORS + HDD_SECTOR_SIZE);
      if (buffer != NULL)
	{ /* success */
	  u_int64_t offset = 0;
	  u_int32_t i;

	  memset (net, 0, sizeof (iin_hio_t));
	  iin->stat = &hio_stat;
	  iin->read = &hio_read;
	  iin->close = &hio_close;
	  iin->last_error = &hio_last_error;
	  iin->dispose_error = &hio_dispose_error;
	  strcpy (iin->source_type, "HD Loader partition via hio");
	  net->hio = hio;
	  net->unaligned = buffer;
	  net->buffer = (void*) (((long) buffer + HDD_SECTOR_SIZE - 1) &
				 ~(HDD_SECTOR_SIZE - 1));
	  net->num_parts = gtoc->count;

	  /* create chunks map */
	  for (i = 0; i < gtoc->count; ++i)
	    {
	      net->parts[i].offset = offset;
	      net->parts[i].start_s = gtoc->part[i].start;
	      net->parts[i].length_s = gtoc->part[i].len;
	      offset += (u_int64_t) gtoc->part[i].len * HDD_SECTOR_SIZE;
	    }
	  net->last_part = net->parts + 0;
	}
      else
	{ /* failed */
	  if (buffer != NULL)
	    osal_free (buffer);
	  osal_free (net);
	  net = NULL;
	}
    }
  return (net);
}


/**************************************************************/
/* 'partition@device' */
int
iin_hio_probe_path (const dict_t *dict,
		    const char *path,
		    iin_t **iin)
{
  int result = RET_NOT_COMPAT;
  const char *pos = strchr (path, '@'); /* "partition name@device" */

  if (pos != NULL && pos - path < 256)
    {
      const char *device = pos + 1;
      hio_t *hio = NULL;
      result = hio_probe (dict, device, &hio);
      if (result == RET_OK && hio != NULL)
	{
	  /*@only@*/ apa_toc_t *toc = NULL;
	  result = apa_toc_read_ex (hio, &toc);
	  if (result == RET_OK && toc != NULL)
	    {
	      int slice_index = 0;
	      u_int32_t partition_index = 0;
	      const size_t name_len = (pos - path);
	      char game_name[256];
	      memcpy (game_name, path, name_len);
	      game_name[name_len] = '\0';

	      /* locate partition */
	      result = apa_find_partition (toc, game_name, &slice_index,
					   &partition_index);
	      if (result == RET_NOT_FOUND)
		{
		  char partition_id[PS2_PART_IDMAX + 1];
		  result = hdl_lookup_partition_ex (hio, game_name,
						    partition_id);
		  if (result == RET_OK)
		    result = apa_find_partition (toc, partition_id,
						 &slice_index,
						 &partition_index);
		}

	      /* read game allocation table */
	      if (result == RET_OK)
		{
		  hdl_game_alloc_table_t gtoc;
		  result = hdl_read_game_alloc_table (hio, toc, slice_index,
						      partition_index, &gtoc);
		  if (result == RET_OK)
		    {
		      *iin = (iin_t*) hio_alloc (hio, &gtoc);
		      if (*iin)
			; /* success */
		      else
			result = RET_NO_MEM;
		    }
		}

	      apa_toc_free (toc), toc = NULL;
	    }
	  if (result != RET_OK)
	    (void) hio->close (hio), hio = NULL;
	}
    }

  return (result);
}
