/*
 * iin_nero.c
 * $Id: iin_nero.c,v 1.7 2004/12/04 10:20:52 b081 Exp $
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

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "iin_nero.h"
#include "iin_img_base.h"
#include "osal.h"
#include "retcodes.h"
#include "aligned.h"
#include "common.h"


/* whether to make additional check for "NER5" in the header */
#define CHECK_NER5


typedef enum data_mode_type
  {
    dm_mode1_plain = 0,
    dm_mode1_raw = 1,
    dm_mode2_plain = 2,
    dm_mode2_raw = 3
  } data_mode_t;

/* values for Nero found out by building test images */
static const u_int32_t RAW_SECTOR_SIZE [4] = { 2048, 2352, 2048, 2352 };
static const u_int32_t RAW_SKIP_OFFSET [4] = {    0,   16,    0,   24 }; 


/**************************************************************/
static int
probe_nero_image (osal_handle_t in,
		  u_int64_t file_size,
		  data_mode_t *mode,
		  u_int32_t *header_length,
		  u_int32_t *footer_length)
{
  char header [156];
  int result;

  if (file_size < 156)
    return (RET_NOT_COMPAT);

  result = osal_seek (in, file_size - 156);
  if (result == OSAL_OK)
    { /* read header (located 156 bytes before end-of-file during my tests :-) ) */
      u_int32_t len;
      result = osal_read (in, header, 156, &len);
      if (result == OSAL_OK)
	result = len == 156 ? OSAL_OK : RET_NOT_COMPAT;
    }

  if (result == OSAL_OK)
    result = (header [0x00] == 'C' &&
	      header [0x01] == 'U' &&
	      header [0x02] == 'E' &&
	      header [0x03] == 'X') ? OSAL_OK : RET_NOT_COMPAT;

#if defined (CHECK_NER5)
  if (result == OSAL_OK)
    result = (header [0x90] == 'N' &&
	      header [0x91] == 'E' &&
	      header [0x92] == 'R' &&
	      header [0x93] == '5') ? OSAL_OK : RET_NOT_COMPAT;
#endif /* CHECK_NER5 defined? */

  if (result == OSAL_OK)
    {
      int type = (unsigned char) header [0x54];
      switch (type)
	{ /* mode 1 */
	case 0x00: *mode = dm_mode1_plain; break;
	case 0x05: *mode = dm_mode1_raw; break;
	  /* mode 2 */
	case 0x02: *mode = dm_mode2_plain; break;
	case 0x06: *mode = dm_mode2_raw; break;
	default: result = RET_BAD_COMPAT;
	}
    }
  if (result == OSAL_OK)
    {
      *header_length = 150 * RAW_SECTOR_SIZE [*mode];
      *footer_length = 156;
    }
  return (result);
}


/**************************************************************/
static int
probe_nero_track (osal_handle_t in,
		  u_int64_t file_size,
		  data_mode_t *mode,
		  u_int32_t *header_length,
		  u_int32_t *footer_length)
{
  /* NOTE: by some reason, Nero track file is one sector shorter than the Nero image file */
  char header [72];
  int result;

  if (file_size < 72)
    return (RET_NOT_COMPAT);

  result = osal_seek (in, file_size - 72);
  if (result == OSAL_OK)
    { /* read header (located 72 bytes before end-of-file during my tests :-) ) */
      u_int32_t len;
      result = osal_read (in, header, 72, &len);
      if (result == OSAL_OK)
	result = len == 72 ? OSAL_OK : RET_NOT_COMPAT;
    }

  if (result == OSAL_OK)
    result = (header [0x00] == 'E' &&
	      header [0x01] == 'T' &&
	      header [0x02] == 'N' &&
	      header [0x03] == '2') ? OSAL_OK : RET_NOT_COMPAT;

#if defined (CHECK_NER5)
  if (result == OSAL_OK)
    result = (header [0x3C] == 'N' &&
	      header [0x3D] == 'E' &&
	      header [0x3E] == 'R' &&
	      header [0x3F] == '5') ? OSAL_OK : RET_NOT_COMPAT;
#endif /* CHECK_NER5 defined? */

  if (result == OSAL_OK)
    {
      *mode = dm_mode1_plain;
      *header_length = 0;
      *footer_length = 72;
    }
  return (result);
}


/**************************************************************/
int
iin_nero_probe_path (const char *path,
		     iin_t **iin)
{
  u_int32_t device_sector_size;
  osal_handle_t in;
  u_int64_t file_size;
  u_int32_t header_size, footer_size;
  data_mode_t mode;
  int result = osal_get_volume_sect_size (path, &device_sector_size);
  if (result == OSAL_OK)
    result = osal_open (path, &in, 0); /* do not disable cache yet */
  if (result == OSAL_OK)
    {
      result = osal_get_file_size (in, &file_size);
      if (result == OSAL_OK)
	{
	  result = probe_nero_image (in, file_size, &mode, &header_size, &footer_size);
	  if (result != OSAL_OK)
	    result = probe_nero_track (in, file_size, &mode, &header_size, &footer_size);
	}
      osal_close (in);
    }

  if (result == RET_OK)
    {
      iin_img_base_t *img_base =
	img_base_alloc (RAW_SECTOR_SIZE [mode], RAW_SKIP_OFFSET [mode]);
      if (img_base != NULL)
	result = img_base_add_part (img_base, path,
				    (u_int32_t) ((file_size - header_size - footer_size) /
				                 RAW_SECTOR_SIZE [mode]),
				    (u_int64_t) header_size, device_sector_size);
      else
	/* img_base_alloc failed */
	result = RET_NO_MEM;
      if (result == OSAL_OK)
	{ /* success */
	  *iin = (iin_t*) img_base;
	  switch (mode)
	    {
	    case dm_mode1_plain:
	      strcpy ((*iin)->source_type, "Nero Image, Mode 1, plain"); break;
	    case dm_mode1_raw:
	      strcpy ((*iin)->source_type, "Nero Image, Mode 1, RAW"); break;
	    case dm_mode2_plain:
	      strcpy ((*iin)->source_type, "Nero Image, Mode 2, plain"); break;
	    case dm_mode2_raw:
	      strcpy ((*iin)->source_type, "Nero Image, Mode 2, RAW"); break;
	    }
	}
      else if (img_base != NULL)
	((iin_t*) img_base)->close ((iin_t*) img_base);
    }
  return (result);
}
