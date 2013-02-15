/*
 * iin_iso.c
 * $Id: iin_iso.c,v 1.5 2004/08/15 16:44:19 b081 Exp $
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
#include "iin_iso.h"
#include "osal.h"
#include "retcodes.h"
#include "aligned.h"


typedef struct iin_iso_type
{
  iin_t iin;
  osal_handle_t file;
  aligned_t *al;
} iin_iso_t;


static int iso_stat (iin_t *iin,
		     size_t *sector_size,
		     size_t *num_sectors);

static int iso_read (iin_t *iin,
		     size_t start_sector,
		     size_t num_sectors,
		     const char **data,
		     size_t *length);

static int iso_close (iin_t *iin);


/**************************************************************/
static iin_iso_t*
iso_alloc (osal_handle_t file,
	   size_t device_sector_size)
{
  iin_iso_t *iso = (iin_iso_t*) osal_alloc (sizeof (iin_iso_t));
  if (iso != NULL)
    {
      iin_t *iin = &iso->iin;
      aligned_t *al = al_alloc (file, device_sector_size,
				IIN_SECTOR_SIZE * IIN_NUM_SECTORS / device_sector_size);
      if (al != NULL)
	{ /* success */
	  memset (iso, 0, sizeof (iin_iso_t));
	  iin->stat = &iso_stat;
	  iin->read = &iso_read;
	  iin->close = &iso_close;
	  strcpy (iin->source_type, "Plain ISO file");
	  iso->file = file;
	  iso->al = al;
	}
      else
	{ /* failed */
	  osal_free (iso);
	  iso = NULL;
	}
    }
  return (iso);
}


/**************************************************************/
static int
iso_stat (iin_t *iin,
	  size_t *sector_size,
	  size_t *num_sectors)
{
  iin_iso_t *iso = (iin_iso_t*) iin;
  bigint_t size_in_bytes;
  int result = osal_get_file_size (iso->file, &size_in_bytes);
  if (result == OSAL_OK)
    {
      *sector_size = IIN_SECTOR_SIZE;
      *num_sectors = (size_t) (size_in_bytes / *sector_size);
    }
  return (result);
}


/**************************************************************/
static int
iso_read (iin_t *iin,
	  size_t start_sector,
	  size_t num_sectors,
	  const char **data,
	  size_t *length)
{
  iin_iso_t *iso = (iin_iso_t*) iin;
  return (al_read (iso->al, (bigint_t) start_sector * IIN_SECTOR_SIZE, data,
		   num_sectors * IIN_SECTOR_SIZE, length));
}


/**************************************************************/
static int
iso_close (iin_t *iin)
{
  iin_iso_t *iso = (iin_iso_t*) iin;
  int result;
  al_free (iso->al);
  result = osal_close (iso->file);
  osal_free (iin);
  return (result);
}


/**************************************************************/
int
iin_iso_probe_path (const char *path,
		    iin_t **iin)
{
  osal_handle_t file;
  int result = osal_open (path, &file, 0);
  if (result == OSAL_OK)
    { /* at offset 0x00008000 there should be "\x01CD001" */
      result = osal_seek (file, (bigint_t) 0x00008000);
      if (result == OSAL_OK)
	{
	  unsigned char buffer [6];
	  size_t bytes;
	  result = osal_read (file, buffer, sizeof (buffer), &bytes);
	  if (result == OSAL_OK)
	    {
	      if (bytes == 6 &&
		  memcmp (buffer, "\001CD001", 6) == 0)
		;
	      else
		result = RET_NOT_COMPAT;
	    }
	}
      osal_close (file);
    }

  if (result == OSAL_OK)
    { /* open ISO image file again, with no cache this time */
      result = osal_open (path, &file, 1);
      if (result == OSAL_OK)
	{
	  size_t sector_size;
	  result = osal_get_volume_sect_size (path, &sector_size);
	  if (result == OSAL_OK)
	    {
	      *iin = (iin_t*) iso_alloc (file, sector_size);
	      if (*iin != NULL)
		; /* success */
	      else
		{ /* iso_alloc failed */
		  osal_close (file);
		  result = RET_NO_MEM;
		}
	    }
	}
    }
  return (result);
}
