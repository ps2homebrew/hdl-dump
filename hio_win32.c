/*
 * hio_win32.c - Win32 interface to locally connected PS2 HDD
 * $Id: hio_win32.c,v 1.5 2004/09/26 19:39:39 b081 Exp $
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

#include "hio_win32.h"
#include "osal.h"
#include "retcodes.h"
#include <ctype.h>
#include <string.h>


typedef struct hio_win32_type
{
  hio_t hio;
  osal_handle_t device;
  unsigned long error_code; /* against osal_... */
} hio_win32_t;


/**************************************************************/
static int
win32_stat (hio_t *hio,
	    size_t *size_in_kb)
{
  hio_win32_t *hw32 = (hio_win32_t*) hio;
  bigint_t size_in_bytes;
  int result = osal_get_estimated_device_size (hw32->device, &size_in_bytes);
  if (result == OSAL_OK)
    {
      if (size_in_bytes / 1024 < (size_t) 0xffffffff)
	*size_in_kb = (size_t) (size_in_bytes / 1024);
      else
	*size_in_kb = (size_t) 0xffffffff;
    }
  else
    hw32->error_code = osal_get_last_error_code ();
  return (result);
}


/**************************************************************/
static int
win32_read (hio_t *hio,
	    size_t start_sector,
	    size_t num_sectors,
	    void *output,
	    size_t *bytes)
{
  hio_win32_t *hw32 = (hio_win32_t*) hio;
  int result = osal_seek (hw32->device, (bigint_t) start_sector * 512);
  if (result == OSAL_OK)
    result = osal_read (hw32->device, output, num_sectors * 512, bytes);
  if (result == OSAL_OK)
    ;
  else
    hw32->error_code = osal_get_last_error_code ();
  return (result);
}


/**************************************************************/
static int
win32_write (hio_t *hio,
	     size_t start_sector,
	     size_t num_sectors,
	     const void *input,
	     size_t *bytes)
{
  hio_win32_t *hw32 = (hio_win32_t*) hio;
  int result = osal_seek (hw32->device, (bigint_t) start_sector * 512);
  if (result == OSAL_OK)
    result = osal_write (hw32->device, input, num_sectors * 512, bytes);
  if (result == OSAL_OK)
    ;
  else
    hw32->error_code = osal_get_last_error_code ();
  return (result);
}


/**************************************************************/
static int
win32_flush (hio_t *hio)
{ /* win32_flush is intentionately blank */
  return (RET_OK);
}


/**************************************************************/
static int
win32_poweroff (hio_t *hio)
{ /* win32_poweroff is intentionately blank */
  return (RET_OK);
}


/**************************************************************/
static int
win32_close (hio_t *hio)
{
  hio_win32_t *hw32 = (hio_win32_t*) hio;
  int result = osal_close (hw32->device);
  osal_free (hio);
  return (result);
}


/**************************************************************/
static char*
win32_last_error (hio_t *hio)
{
  hio_win32_t *hw32 = (hio_win32_t*) hio;
  return (osal_get_error_msg (hw32->error_code));
}


/**************************************************************/
static void
win32_dispose_error (hio_t *hio,
		     char* error)
{
  osal_dispose_error_msg (error);
}


/**************************************************************/
static hio_t*
win32_alloc (osal_handle_t device)
{
  hio_win32_t *hw32 = (hio_win32_t*) osal_alloc (sizeof (hio_win32_t));
  if (hw32 != NULL)
    {
      memset (hw32, 0, sizeof (hio_win32_t));
      hw32->hio.stat = &win32_stat;
      hw32->hio.read = &win32_read;
      hw32->hio.write = &win32_write;
      hw32->hio.flush = &win32_flush;
      hw32->hio.poweroff = &win32_poweroff;
      hw32->hio.close = &win32_close;
      hw32->hio.last_error = &win32_last_error;
      hw32->hio.dispose_error = &win32_dispose_error;
      hw32->device = device;
    }
  return ((hio_t*) hw32);
}


/**************************************************************/
int
hio_win32_probe (const char *path,
		 hio_t **hio)
{
  int result;
#if defined (_BUILD_WIN32)
  if (tolower (path [0]) == 'h' &&
      tolower (path [1]) == 'd' &&
      tolower (path [2]) == 'd' &&
      isdigit (path [3]) &&
      ((path [4] == ':' &&
	path [5] == '\0') ||
       (isdigit (path [4]) &&
	path [5] == ':' &&
	path [6] == '\0')))
    result = RET_OK;
  else
    result = RET_NOT_COMPAT;
#endif
#if defined (_BUILD_UNIX)
  /* osal_map_device_name would check whether input is a device or not */
  result = RET_OK;
#endif

  if (result == RET_OK)
    {
      char device_name [MAX_PATH];
      result = osal_map_device_name (path, device_name);
#if defined (_BUILD_UNIX)
      if (result == RET_ERR ||
	  result == RET_BAD_DEVICE)
	result = RET_NOT_COMPAT;
#endif
      if (result == OSAL_OK)
	{
	  osal_handle_t device;
	  result = osal_open_device_for_writing (device_name, &device);
	  if (result == OSAL_OK)
	    {
	      *hio = win32_alloc (device);
	      if (*hio != NULL)
		; /* success */
	      else
		result = RET_NO_MEM;

	      if (result != OSAL_OK)
		osal_close (device);
	    }
	}
    }
  return (result);
}
