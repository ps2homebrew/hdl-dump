/*
 * iin_hdloader.c
 * $Id: iin_hdloader.c,v 1.10 2004/12/04 10:20:52 b081 Exp $
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
#include "iin_hdloader.h"
#include "iin_img_base.h"
#include "hio_win32.h"
#include "osal.h"
#include "retcodes.h"
#include "aligned.h"
#include "apa.h"
#include "net_io.h"


#define _MB * (1024 * 1024) /* really ugly :-) */


/**************************************************************/
int
iin_hdloader_probe_path (const char *path,
			 iin_t **iin)
{
  if (tolower (path [0]) == 'h' &&
      tolower (path [1]) == 'd' &&
      tolower (path [2]) == 'd' &&
      isdigit (path [3]) &&
      (path [4] == ':' ||
       (isdigit (path [4]) &&
	path [5] == ':')))
    {
      char device_name [6 + 1]; /* hdd??:\0 */
      char real_device_name [MAX_PATH];
      const char *partition_name = strchr (path, ':') + 1;
      osal_handle_t device = OSAL_HANDLE_INIT;
      apa_partition_table_t *table = NULL;
      u_int32_t partition_index;
      u_int32_t device_sector_size;
      int result;
      hio_t *hio = NULL;

      /* open the device for RAW reading */
      memcpy (device_name, path, 6);
      if (device_name [4] == ':')
	device_name [5] = '\0'; /* hdd?: */
      else
	device_name [6] = '\0'; /* hdd??: */
      result = osal_map_device_name (device_name, real_device_name);
      if (result == OSAL_OK)
	result = osal_open (real_device_name, &device, 0); /* w/ caching */
      if (result == OSAL_OK)
	result = osal_get_device_sect_size (device, &device_sector_size);
      if (OSAL_IS_OPENED (device))
	osal_close (device);

      if (result == OSAL_OK)
	result = hio_win32_probe (device_name, &hio);

      /* get APA */
      if (result == OSAL_OK)
	result = apa_ptable_read_ex (hio, &table);
      if (result == OSAL_OK)
	{
	  result = apa_find_partition (table, partition_name,
				       &partition_index);
	  if (result == RET_NOT_FOUND)
	    { /* attempt to locate partition name by prepending "PP.HDL." */
	      char alt_part_name [100];
	      sprintf (alt_part_name, "PP.HDL.%s", partition_name);
	      result = apa_find_partition (table, alt_part_name,
					   &partition_index);
	    }
	}
      if (result == OSAL_OK)
	{ /* partition is found - read structure */
	  unsigned char *buffer = osal_alloc (4 _MB);
	  if (buffer != NULL)
	    { /* get HD Loader header */
	      iin_img_base_t *img_base = NULL;
	      u_int32_t len;
	      const ps2_partition_header_t *part =
		&table->parts [partition_index].header;
	      result = hio->read (hio, part->start,
				  (4 _MB) / HDD_SECTOR_SIZE, buffer, &len);
	      if (result == OSAL_OK)
		result = (len == 4 _MB ? OSAL_OK : RET_BAD_APA);
	      if (result == OSAL_OK)
		result = ((buffer [0x00101000] == 0xed &&
			   buffer [0x00101001] == 0xfe &&
			   buffer [0x00101002] == 0xad &&
			   buffer [0x00101003] == 0xde) ?
			  OSAL_OK : RET_NOT_HDL_PART);
	      if (result == OSAL_OK)
		{
		  img_base = img_base_alloc (2048, 0);
		  result = img_base != NULL ? OSAL_OK : RET_NO_MEM;
		}
	      if (result == OSAL_OK)
		{ /* that is a HD Loader partition */
		  u_int32_t num_parts = buffer [0x001010f0];
		  const u_int32_t *data = (u_int32_t*) (buffer + 0x001010f5);
		  u_int32_t i;
		  for (i=0; result == OSAL_OK && i<num_parts; ++i)
		    {
		      u_int64_t start = ((u_int64_t) data [i * 3 + 1] << 8) *512;
		      u_int64_t length = (u_int64_t) data [i * 3 + 2] * 256;
		      result = img_base_add_part (img_base, real_device_name,
						  (u_int32_t) (length / 2048),
						  start, device_sector_size);
		    }
		}

	      if (result == OSAL_OK)
		{ /* success */
		  *iin = (iin_t*) img_base;
		  strcpy ((*iin)->source_type, "HD Loader partition");
		}
	      else if (img_base != NULL)
		((iin_t*) img_base)->close ((iin_t*) img_base);

	      osal_free (buffer);
	    }
	  else
	    result = RET_NO_MEM;
	}

      /* cleanup */
      if (table != NULL)
	apa_ptable_free (table);
      if (hio != NULL)
	hio->close (hio);

      return (result);
    }
  else
    return (RET_NOT_COMPAT);
}
