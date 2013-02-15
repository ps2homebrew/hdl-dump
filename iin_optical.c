/*
 * iin_optical.c
 * $Id: iin_optical.c,v 1.7 2004/09/12 17:25:27 b081 Exp $
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
#include "iin_optical.h"
#include "osal.h"
#include "retcodes.h"
#include "aligned.h"


typedef struct iin_optical_type
{
  iin_t iin;
  osal_handle_t device;
  aligned_t *al;
  unsigned long error_code; /* against osal_... */
} iin_optical_t;


/**************************************************************/
static int
opt_stat (iin_t *iin,
	  size_t *sector_size,
	  size_t *num_sectors)
{
  iin_optical_t *opt = (iin_optical_t*) iin;
  bigint_t size_in_bytes;
  int result = osal_get_device_size (opt->device, &size_in_bytes);
  if (result == OSAL_OK)
    {
      *sector_size = IIN_SECTOR_SIZE;
      *num_sectors = (size_t) (size_in_bytes / IIN_SECTOR_SIZE);
    }
  else
    opt->error_code = osal_get_last_error_code ();
  return (result);
}


/**************************************************************/
static int
opt_read (iin_t *iin,
	  size_t start_sector,
	  size_t num_sectors,
	  const char **data,
	  size_t *length)
{
  iin_optical_t *opt = (iin_optical_t*) iin;
  int result = al_read (opt->al, (bigint_t) start_sector * IIN_SECTOR_SIZE, data,
			num_sectors * IIN_SECTOR_SIZE, length);
  if (result != RET_OK)
    ;
  else
    opt->error_code = osal_get_last_error_code ();
  return (result);
}


/**************************************************************/
static int
opt_close (iin_t *iin)
{
  iin_optical_t *opt = (iin_optical_t*) iin;
  int result;
  al_free (opt->al);
  result = osal_close (opt->device);
  if (result == RET_OK)
    ;
  else
    opt->error_code = osal_get_last_error_code ();
  osal_free (iin);
  return (result);
}


/**************************************************************/
static char*
opt_last_error (iin_t *iin)
{
  iin_optical_t *opt = (iin_optical_t*) iin;
  return (osal_get_error_msg (opt->error_code));
}


/**************************************************************/
static void
opt_dispose_error (iin_t *iin,
		   char* error)
{
  osal_dispose_error_msg (error);
}


/**************************************************************/
static iin_optical_t*
opt_alloc (osal_handle_t device,
	   size_t device_sector_size)
{
  iin_optical_t *opt = (iin_optical_t*) osal_alloc (sizeof (iin_optical_t));
  if (opt != NULL)
    {
      iin_t *iin = &opt->iin;
      aligned_t *al = al_alloc (device, device_sector_size,
				IIN_SECTOR_SIZE * IIN_NUM_SECTORS / device_sector_size);
      if (al != NULL)
	{ /* success */
	  memset (opt, 0, sizeof (iin_optical_t));
	  iin->stat = &opt_stat;
	  iin->read = &opt_read;
	  iin->close = &opt_close;
	  iin->last_error = &opt_last_error;
	  iin->dispose_error = &opt_dispose_error;
	  strcpy (iin->source_type, "Optical drive");
	  opt->device = device;
	  opt->al = al;
	}
      else
	{ /* failed */
	  osal_free (opt);
	  opt = NULL;
	}
    }
  return (opt);
}


/**************************************************************/
int
iin_optical_probe_path (const char *path,
			iin_t **iin)
{
  if (tolower (path [0]) == 'c' &&
      tolower (path [1]) == 'd' &&
      isdigit (path [2]) &&
      ((path [3] == ':' &&
	path [4] == '\0') ||
       (isdigit (path [3]) &&
	path [4] == ':' &&
	path [5] == '\0')))
    { /* "cd?:" or "cd??:" matched */
      char device_name [MAX_PATH];
      int result = osal_map_device_name (path, device_name);
      if (result == OSAL_OK)
	{
	  osal_handle_t device;
	  result = osal_open (device_name, &device, 1);
	  if (result == OSAL_OK)
	    {
	      size_t sector_size;
	      result = osal_get_device_sect_size (device, &sector_size);
	      if (result == OSAL_OK)
		{
		  *iin = (iin_t*) opt_alloc (device, sector_size);
		  if (*iin != NULL)
		    ; /* success */
		  else
		    { /* opt_alloc failed */
		      osal_close (device);
		      result = RET_NO_MEM;
		    }
		}
	    }
	}
      return (result);
    }
  else
    return (RET_NOT_COMPAT);
}
