/*
 * iin_optical.c
 * $Id: iin_optical.c,v 1.11 2006/06/18 13:11:46 bobi Exp $
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
#if defined (IIN_OPTICAL_MMAP)
#  define MMAP_AREA_SIZE IIN_SECTOR_SIZE * 65536 /* 128MB */
  osal_mmap_t *mm;
  u_int64_t total_size;
  u_int64_t ptr_start, ptr_end;
  void *ptr;
#else
  aligned_t *al;
#endif
  unsigned long error_code; /* against osal_... */
} iin_optical_t;


/**************************************************************/
static int
opt_stat (iin_t *iin,
	  u_int32_t *sector_size,
	  u_int32_t *num_sectors)
{
  iin_optical_t *opt = (iin_optical_t*) iin;
  u_int64_t size_in_bytes;
  int result = osal_get_device_size (opt->device, &size_in_bytes);
  if (result == OSAL_OK)
    {
      *sector_size = IIN_SECTOR_SIZE;
      *num_sectors = (u_int32_t) (size_in_bytes / IIN_SECTOR_SIZE);
    }
  else
    opt->error_code = osal_get_last_error_code ();
  return (result);
}


/**************************************************************/
static int
opt_read (iin_t *iin,
	  u_int32_t start_sector,
	  u_int32_t num_sectors,
	  const char **data,
	  u_int32_t *length)
{
  iin_optical_t *opt = (iin_optical_t*) iin;

#if defined (IIN_OPTICAL_MMAP)
  u_int64_t range_start = (u_int64_t) start_sector * IIN_SECTOR_SIZE;
  u_int64_t range_end = range_start + num_sectors * IIN_SECTOR_SIZE;

  if (range_end > opt->total_size)
    range_end = opt->total_size;
  if (!(opt->ptr_start <= range_start &&
	range_end <= opt->ptr_end))
    { /* need to (re)map requested region */
      u_int64_t tmp_start = range_start;
      u_int64_t tmp_end = range_start + MMAP_AREA_SIZE;
      int result;

      if (tmp_end > opt->total_size)
	tmp_end = opt->total_size;

      if (opt->ptr != NULL)
	{ /* reset current mapping first */
	  result = osal_munmap (opt->mm);
	  if (result != OSAL_OK)
	    return (result);
	  opt->mm = NULL;
	  opt->ptr = NULL;
	  opt->ptr_start = opt->ptr_end = 0;
	}

      result = osal_mmap (&opt->mm, &opt->ptr, opt->device,
			  tmp_start, tmp_end - tmp_start);
      if (result == OSAL_OK)
	{ /* output args are set-up below */
	  opt->ptr_start = tmp_start;
	  opt->ptr_end = tmp_end;
	}
      else
	return (result);
    }
  /* information we need is memory-mapped already */
  *data = (const char*) opt->ptr + (range_start - opt->ptr_start);
  *length = (u_int32_t) (range_end - range_start);
  return (RET_OK);

#else
  int result = al_read (opt->al, (u_int64_t) start_sector * IIN_SECTOR_SIZE,
			data, num_sectors * IIN_SECTOR_SIZE, length);
  if (result == RET_OK)
    ;
  else
    opt->error_code = osal_get_last_error_code ();
  return (result);
#endif /* IIN_OPTICAL_MMAP defined? */
}


/**************************************************************/
static int
opt_close (iin_t *iin)
{
  iin_optical_t *opt = (iin_optical_t*) iin;
  int result;

#if defined (IIN_OPTICAL_MMAP)
  if (opt->mm != NULL)
    osal_munmap (opt->mm);

#else
  al_free (opt->al);
#endif

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
	   u_int32_t device_sector_size)
{
  iin_optical_t *opt = (iin_optical_t*) osal_alloc (sizeof (iin_optical_t));
  if (opt != NULL)
    {
      iin_t *iin = &opt->iin;
#if !defined (IIN_OPTICAL_MMAP)
      aligned_t *al = al_alloc (device, device_sector_size, IIN_SECTOR_SIZE *
				IIN_NUM_SECTORS / device_sector_size);
      if (al != NULL)
	{ /* success */
#endif
	  memset (opt, 0, sizeof (iin_optical_t));
	  iin->stat = &opt_stat;
	  iin->read = &opt_read;
	  iin->close = &opt_close;
	  iin->last_error = &opt_last_error;
	  iin->dispose_error = &opt_dispose_error;
	  strcpy (iin->source_type, "Optical drive");
	  opt->device = device;
#if defined (IIN_OPTICAL_MMAP)
	  opt->mm = NULL;
	  opt->ptr = NULL;
	  opt->ptr_start = opt->ptr_end = 0;
	  {
	    u_int32_t sectors, size;
	    if (opt_stat (iin, &size, &sectors) == RET_OK)
	      opt->total_size = (u_int64_t) sectors * size;
	    else
	      {
		osal_free (opt);
		opt = NULL;
	      }
	  }
#else
	  opt->al = al;
	}
      else
	{ /* failed */
	  osal_free (opt);
	  opt = NULL;
	}
#endif
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
	      u_int32_t sector_size;
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
    {
#if defined (_BUILD_WIN32)
      return (RET_NOT_COMPAT);
#else
      /* FreeBSD patch to support device nodes */
      char device_name [MAX_PATH];
      int result = osal_map_device_name (path, device_name);
      if (result == OSAL_OK)
	{
	  osal_handle_t device;
	  result = osal_open (device_name, &device, 1);
	  if (result == OSAL_OK)
	    {
	      u_int32_t sector_size;
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
      else
	result = RET_NOT_COMPAT;
      return (result);
#endif
    }
}
