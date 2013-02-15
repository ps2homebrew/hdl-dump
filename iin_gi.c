/*
 * iin_gi.c
 * $Id: iin_gi.c,v 1.8 2004/12/04 10:20:52 b081 Exp $
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

#include <string.h>

#include "iin_gi.h"
#include "iin_img_base.h"
#include "osal.h"
#include "retcodes.h"
#include "common.h"


/* whether to make additional check for "0x11111111" in the header */
#define CHECK_11111111


typedef enum data_mode_type
  {
    dm_mode1_plain = 0,
    dm_mode2_plain = 1
  } data_mode_t;

/* values for Global Image found out by building test images */
static const u_int32_t RAW_SECTOR_SIZE [2] = { 2048, 2336 };
static const u_int32_t RAW_SKIP_OFFSET [2] = {    0,    8 }; 
static const u_int32_t FILE_SIZES_OFFSET [4] = { 0x9c, 0xa0, 0xa4, 0xa8 }; /* BE, in sectors */
static const u_int32_t FILE_NAMES_OFFSET [4] = { 0x00b0, 0x01b4, 0x02b8, 0x03bd };


/**************************************************************/
int
iin_gi_probe_path (const char *path,
		   iin_t **iin)
{
  u_int32_t device_sector_size;
  osal_handle_t in;
  int result = osal_get_volume_sect_size (path, &device_sector_size);
  if (result == OSAL_OK)
    result = osal_open (path, &in, 0); /* do not disable cache yet */
  if (result == OSAL_OK)
    {
      int single_file = 0;
      data_mode_t mode;
      u_int32_t len;
      unsigned char header [1476];
      iin_img_base_t *img_base = NULL;
      u_int32_t num_sectors;

      result = osal_read (in, header, 1476, &len);
      if (result == OSAL_OK)
	result = (header [0x00] == 0xda &&
		  header [0x01] == 0xda &&
		  header [0x02] == 0xfe &&
		  header [0x03] == 0xfe) ? OSAL_OK : RET_NOT_COMPAT;

#if defined (CHECK_11111111)
      if (result == OSAL_OK)
	result = (header [0x14] == 0x11 &&
		  header [0x15] == 0x11 &&
		  header [0x16] == 0x11 &&
		  header [0x17] == 0x11) ? OSAL_OK : RET_NOT_COMPAT;
#endif /* CHECK_11111111 defined? */

      if (result == OSAL_OK)
	{ /* check whether image is one or multiple parts */
	  if (header [0x62] == 0x22 &&
	      header [0x63] == 0x22 &&
	      header [0x64] == 0x22 &&
	      header [0x65] == 0x22)
	    single_file = 1;
	  else if (header [0x62] == 0x88 &&
		   header [0x63] == 0x88 &&
		   header [0x64] == 0x88 &&
		   header [0x65] == 0x88)
	    single_file = 0;
	  else
	    result = RET_BAD_COMPAT;
	}

      mode = dm_mode1_plain;
      if (result == OSAL_OK)
	{ /* mode1/mode2? */
	  switch (header [0x7e])
	    {
	    case 0x01: mode = dm_mode1_plain; break;
	    case 0x02: mode = dm_mode2_plain; break;
	    default:   result = RET_BAD_COMPAT;
	    }
	  if (result == OSAL_OK)
	    { /* total number of sectors */
	      num_sectors = (header [0x37] << 24 |
			     header [0x36] << 16 |
			     header [0x35] <<  8 |
			     header [0x34]);
	      result = (num_sectors == (header [0x3b] << 24 |
					header [0x3a] << 16 |
					header [0x39] <<  8 |
					header [0x38]) &&
			num_sectors == (header [0x7d] << 24 |
					header [0x7c] << 16 |
					header [0x7b] <<  8 |
					header [0x7a])) ? OSAL_OK : RET_BAD_COMPAT;
	    }
	}

      if (result == OSAL_OK)
	{
	  img_base = img_base_alloc (RAW_SECTOR_SIZE [mode], RAW_SKIP_OFFSET [mode]);
	  result = img_base != NULL ? OSAL_OK : RET_NO_MEM;
	}

      if (result == OSAL_OK)
	{
	  if (single_file)
	    /* single file only -> CD image or DVD image with size less than 2GB */
	    result = img_base_add_part (img_base, path, num_sectors,
					(u_int64_t) 152, device_sector_size);
	  else
	    { /* multiple files */
	      u_int32_t num_files = header [0x98], i;
	      for (i=0; i<num_files; ++i)
		{
		  char source [MAX_PATH];
		  u_int32_t length_s = (header [FILE_SIZES_OFFSET [i] + 3] << 24 |
				     header [FILE_SIZES_OFFSET [i] + 2] << 16 |
				     header [FILE_SIZES_OFFSET [i] + 1] <<  8 |
				     header [FILE_SIZES_OFFSET [i]]);

		  strncpy (source, (const char*) header + FILE_NAMES_OFFSET [i], 0x0104);
		  source [0x0103] = '\0';
		  result = lookup_file (source, path);
		  if (result == RET_OK)
		    ; /* linked file found */
		  else if (result == RET_FILE_NOT_FOUND)
		    result = RET_BROKEN_LINK;

		  if (result == RET_OK)
		    result = osal_get_volume_sect_size (source, &device_sector_size);
		  if (result == RET_OK)
		    result = img_base_add_part (img_base, source, length_s,
						(u_int64_t) 0, device_sector_size);
		}
	    }
	}

      if (result == OSAL_OK)
	{
	  *iin = (iin_t*) img_base;
	  switch (mode)
	    {
	    case dm_mode1_plain: strcpy ((*iin)->source_type, "Global Image, Mode1"); break;
	    case dm_mode2_plain: strcpy ((*iin)->source_type, "Global Image, Mode2"); break;
	    }
	}
      else if (img_base != NULL)
	((iin_t*) img_base)->close ((iin_t*) img_base);

      osal_close (in);
    }
  return (result);
}
