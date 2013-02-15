/*
 * iin_net.c, based on iin_hdloader.c, v 1.1
 * $Id: iin_net.c,v 1.2 2004/08/15 16:44:19 b081 Exp $
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
#include "iin_net.h"
#include "osal.h"
#include "retcodes.h"
#include "aligned.h"
#include "apa.h"
#include "hio_net.h"
#include "net_io.h"


#define _MB * (1024 * 1024) /* really ugly :-) */


typedef struct hdl_part_type
{
  size_t start_s, length_s; /* in 512-byte long sectors, relative to the RAW HDD */
  bigint_t offset; /* in bytes, relative to CD start */
} hdl_part_t;

typedef struct iin_net_type
{
  iin_t iin;
  hio_t *hio; /* hio_net_t, actually */
  char *unaligned, *buffer;

  size_t num_parts;
  hdl_part_t *parts;
  const hdl_part_t *last_part; /* the last partition where requested sectors were */
} iin_net_t;


static int net_stat (iin_t *iin,
		     size_t *sector_size,
		     size_t *num_sectors);

static int net_read (iin_t *iin,
		     size_t start_sector,
		     size_t num_sectors,
		     const char **data,
		     size_t *length);

static int net_close (iin_t *iin);


/**************************************************************/
static iin_net_t*
net_alloc (hio_t *hio,
	   size_t num_parts,
	   const hdl_part_t *parts)
{
  iin_net_t *net = osal_alloc (sizeof (iin_net_t));
  if (net != NULL)
    {
      iin_t *iin = &net->iin;
      hdl_part_t *parts2 = osal_alloc (sizeof (hdl_part_t) * num_parts);
      char *buffer = osal_alloc (IIN_SECTOR_SIZE * IIN_NUM_SECTORS + HDD_SECTOR_SIZE);
      if (parts2 != NULL && buffer != NULL)
	{ /* success */
	  size_t i;
	  memset (net, 0, sizeof (iin_net_t));
	  iin->stat = &net_stat;
	  iin->read = &net_read;
	  iin->close = &net_close;
	  strcpy (iin->source_type, "HD Loader partition via TCP/IP");
	  net->hio = hio;
	  net->unaligned = buffer;
	  net->buffer = (void*) (((long) buffer + HDD_SECTOR_SIZE - 1) & ~(HDD_SECTOR_SIZE - 1));
	  net->num_parts = num_parts;
	  net->parts = parts2;
	  for (i=0; i<num_parts; ++i)
	    net->parts [i] = parts [i];
	  net->last_part = net->parts;
	}
      else
	{ /* failed */
	  if (parts2 != NULL)
	    osal_free (parts2);
	  if (buffer != NULL)
	    osal_free (buffer);
	  osal_free (net);
	  net = NULL;
	}
    }
  return (net);
}


/**************************************************************/
static int
net_stat (iin_t *iin,
	  size_t *sector_size,
	  size_t *num_sectors)
{
  iin_net_t *net = (iin_net_t*) iin;
  bigint_t total_size = 0;
  size_t i;
  for (i=0; i<net->num_parts; ++i)
    total_size += (bigint_t) net->parts [i].length_s * HDD_SECTOR_SIZE;
  *sector_size = IIN_SECTOR_SIZE;
  *num_sectors = total_size / IIN_SECTOR_SIZE;
  return (OSAL_OK);
}


/**************************************************************/
static int
net_read (iin_t *iin,
	  size_t start_sector,
	  size_t num_sectors,
	  const char **data,
	  size_t *length)
{
  int result;
  iin_net_t *net = (iin_net_t*) iin;
  bigint_t start_offset = (bigint_t) start_sector * IIN_SECTOR_SIZE;
  bigint_t abs_offset, bytes_til_part_end;
  size_t data_len;
  /* find the partition where requested sectors are */
  if (net->last_part->offset <= start_offset &&
      start_offset < net->last_part->offset + (bigint_t)net->last_part->length_s * HDD_SECTOR_SIZE)
    ; /* same partition as the last time */
  else
    { /* make a linear scan to locate the partition */
      int found = 0;
      size_t i;
      for (i=0; i<net->num_parts; ++i)
	{
	  const hdl_part_t *part = net->parts + i;
	  if (part->offset <= start_offset &&
	      start_offset < part->offset + (bigint_t) part->length_s * HDD_SECTOR_SIZE)
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

  abs_offset = ((bigint_t) net->last_part->start_s * HDD_SECTOR_SIZE +
		start_offset - net->last_part->offset);
  bytes_til_part_end = ((bigint_t) net->last_part->length_s * HDD_SECTOR_SIZE -
			(start_offset - net->last_part->offset));
  data_len = num_sectors * IIN_SECTOR_SIZE;
  if (data_len > bytes_til_part_end)
    data_len = bytes_til_part_end;
  result = net->hio->read (net->hio, abs_offset / HDD_SECTOR_SIZE, data_len / HDD_SECTOR_SIZE,
			   net->buffer, length);
  if (result == RET_OK)
    *data = net->buffer;
  return (result);
}


/**************************************************************/
static int
net_close (iin_t *iin)
{
  iin_net_t *net = (iin_net_t*) iin;
  osal_free (net->parts);
  osal_free (net->unaligned);
  net->hio->close (net->hio);
  osal_free (net);
  return (RET_OK);
}


/**************************************************************/
int
iin_net_probe_path (const char *path,
		    iin_t **iin)
{
  int result = RET_NOT_COMPAT;
  char *endp;
  int a, b, c, d;
  char ip_addr_only [16];

  a = strtol (path, &endp, 10);
  if (a > 0 && a <= 255 && *endp == '.')
    { /* there is a chance */
      b = strtol (endp + 1, &endp, 10);
      if (b >= 0 && b <= 255 && *endp == '.')
	{
	  c = strtol (endp + 1, &endp, 10);
	  if (c >= 0 && c <= 255 && *endp == '.')
	    {
	      d = strtol (endp + 1, &endp, 10);
	      if (d >= 0 && d <= 255 && *endp == ':' && endp - path <= 15)
		/* that could be an IP address */
		result = RET_OK;
	    }
	}
    }

  if (result == RET_OK)
    {
      hio_t *hio;
      memcpy (ip_addr_only, path, endp - path);
      ip_addr_only [endp - path] = '\0';

      result = hio_net_probe (ip_addr_only, &hio);
      if (result == OSAL_OK)
	{
	  const char *partition_name = strchr (path, ':') + 1;
	  apa_partition_table_t *table = NULL;
	  size_t partition_index;

	  result = apa_ptable_read_ex (hio, &table);
	  if (result == OSAL_OK)
	    {
	      result = apa_find_partition (table, partition_name, &partition_index);
	      if (result == RET_NOT_FOUND)
		{ /* attempt to locate partition name by prepending "PP.HDL." */
		  char alt_part_name [100];
		  sprintf (alt_part_name, "PP.HDL.%s", partition_name);
		  result = apa_find_partition (table, alt_part_name, &partition_index);
		}
	    }
	  if (result == OSAL_OK)
	    { /* partition is found - read structure */
	      unsigned char *buffer = osal_alloc (4 _MB);
	      if (buffer != NULL)
		{ /* get HD Loader header */
		  size_t len;
		  const ps2_partition_header_t *part = &table->parts [partition_index].header;
		  result = hio->read (hio, part->start, (4 _MB) / HDD_SECTOR_SIZE, buffer, &len);
		  if (result == OSAL_OK)
		    result = (len == 4 _MB ? OSAL_OK : RET_BAD_APA); /* read failed? */
		  if (result == OSAL_OK)
		    result = (buffer [0x00101000] == 0xed &&
			      buffer [0x00101001] == 0xfe &&
			      buffer [0x00101002] == 0xad &&
			      buffer [0x00101003] == 0xde) ? OSAL_OK : RET_NOT_HDL_PART;
		  if (result == OSAL_OK)
		    { /* that is a HD Loader partition */
		      size_t num_parts = buffer [0x001010f0];
		      hdl_part_t *parts = osal_alloc (sizeof (hdl_part_t) * num_parts);
		      if (parts != NULL)
			{
			  bigint_t offset = 0;
			  const size_t *data = (size_t*) (buffer + 0x001010f5);
			  size_t i;
			  for (i=0; i<num_parts; ++i)
			    {
			      parts [i].offset = offset;
			      parts [i].start_s = data [i * 3 + 1] << 8;
			      parts [i].length_s = data [i * 3 + 2] / 2;
			      offset += (bigint_t) parts [i].length_s * HDD_SECTOR_SIZE;
			    }
			  *iin = (iin_t*) net_alloc (hio, num_parts, parts);
			  osal_free (parts);

			  if (*iin != NULL)
			    { /* success */
			      result = RET_OK;
			    }
			  else
			    { /* error */
			      hio->close (hio);
			      result = RET_NO_MEM;
			    }
			}
		      else
			result = RET_NO_MEM;
		    }
		  osal_free (buffer);
		}
	      else
		result = RET_NO_MEM;
	    }
	} /* hio ok? */
    }
  return (result);
}
