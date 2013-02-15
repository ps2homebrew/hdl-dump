/*
 * osal_win32.c
 * $Id: osal_win32.c,v 1.10 2004/09/12 17:25:27 b081 Exp $
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

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include "retcodes.h"
#include "osal.h"
#include "apa.h"


static int osal_dlist_alloc (osal_dlist_t **dlist);

static int osal_dlist_add (osal_dlist_t *dlist,
			   const char *name,
			   bigint_t capacity,
			   int is_ps2,
			   unsigned long status);


/**************************************************************/
unsigned long
osal_get_last_error_code (void)
{
  return (GetLastError ());
}


/**************************************************************/
char*
osal_get_error_msg (unsigned long err)
{
  char *error = NULL;
  FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		 NULL, err, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
		 (LPTSTR) &error, 0, NULL);
  return (error);
}


/**************************************************************/
char*
osal_get_last_error_msg (void)
{
  return (osal_get_error_msg (GetLastError ()));
}


/**************************************************************/
void
osal_dispose_error_msg (char *msg)
{
  LocalFree (msg);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_open (const char *name,
	   osal_handle_t *handle,
	   int no_cache)
{
  *handle = CreateFile (name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | (no_cache ? FILE_FLAG_NO_BUFFERING : 0), NULL);
  return (*handle != INVALID_HANDLE_VALUE ? OSAL_OK : OSAL_ERR);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_open_device_for_writing (const char *device_name,
			      osal_handle_t *handle)
{
  *handle = CreateFile (device_name, GENERIC_WRITE | GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
  return (*handle != INVALID_HANDLE_VALUE ? OSAL_OK : OSAL_ERR);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_create_file (const char *path,
		  osal_handle_t *handle,
		  bigint_t estimated_size)
{
  *handle = CreateFile (path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
			CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
  if (*handle != INVALID_HANDLE_VALUE)
    {
      if (estimated_size > 0)
	{
	  /* set file size to reduce fragmentation */
	  LARGE_INTEGER zero, output_size;

	  output_size.QuadPart = estimated_size;
	  zero.QuadPart = 0;

	  if (SetFilePointerEx (*handle, output_size, NULL, FILE_BEGIN) &&
	      SetEndOfFile (*handle) &&
	      SetFilePointerEx (*handle, zero, NULL, FILE_BEGIN))
	    return (OSAL_OK);
	  else
	    {
	      CloseHandle (*handle);
	      DeleteFile (path);
	      return (OSAL_ERR); /* error setting file length */
	    }
	}
      return (OSAL_OK);
    }
  return (OSAL_ERR); /* error creating file */
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_get_estimated_device_size (osal_handle_t handle,
				bigint_t *size_in_bytes)
{
  DISK_GEOMETRY geo;
  DWORD len = 0;

  if (DeviceIoControl (handle, IOCTL_DISK_GET_DRIVE_GEOMETRY,
		       NULL, 0, &geo, sizeof (DISK_GEOMETRY), &len, NULL))
    {
      *size_in_bytes = (geo.Cylinders.QuadPart * geo.TracksPerCylinder *
			geo.SectorsPerTrack * geo.BytesPerSector);
      return (OSAL_OK);
    }
  else
    return (OSAL_ERR);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_get_device_size (osal_handle_t handle,
		      bigint_t *size_in_bytes)
{
  int result = osal_get_estimated_device_size (handle, size_in_bytes);
  if (result == OSAL_OK)
    {
      LARGE_INTEGER offs;
      offs.QuadPart = *size_in_bytes;
      if (SetFilePointerEx (handle, offs, NULL, FILE_BEGIN)) /* seek to "end" */
	{
	  const size_t BUFF_SIZE = 1024 * 1024;
	  void *buffer = LocalAlloc (LMEM_FIXED, BUFF_SIZE);
	  if (buffer != NULL)
	    {
	      BOOL success;
	      DWORD len;
	      do
		{ /* count the number of bytes readen after the end */
		  success = ReadFile (handle, buffer, BUFF_SIZE, &len, NULL);
		  if (success)
		    *size_in_bytes += len;
		}
	      while (success && len > 0);
	      LocalFree (buffer);

	      offs.QuadPart = 0;
	      return (SetFilePointerEx (handle, offs, NULL, FILE_BEGIN) ? OSAL_OK : OSAL_ERR);
	    }
	  else
	    return (OSAL_NO_MEM); /* out-of-memory */
	}
      else
	return (OSAL_ERR); /* seek fails */
    }
  else
    return (result); /* error */
}


/**************************************************************/
int
osal_get_device_sect_size (osal_handle_t handle,
			   size_t *size_in_bytes)
{
  DISK_GEOMETRY geo;
  DWORD len = 0;

  if (DeviceIoControl (handle, IOCTL_DISK_GET_DRIVE_GEOMETRY,
		       NULL, 0, &geo, sizeof (DISK_GEOMETRY), &len, NULL))
    {
      *size_in_bytes = geo.BytesPerSector;
      return (OSAL_OK);
    }
  else
    return (OSAL_ERR);
}


/**************************************************************/
int
osal_get_volume_sect_size (const char *volume_root,
			   size_t *size_in_bytes)
{
  char volume [10]; /* copy drive letter and a slash - "C:\" */
  char *p = volume, *end = volume + sizeof (volume) - 2;
  while (*volume_root != '\\' && p < end)
    *p++ = *volume_root++;
  if (p < end)
    {
      DWORD sectors_per_clust, bytes_per_sect, free_clusters, total_clusters;
      *p++ = '\\';
      *p = '\0';
      if (GetDiskFreeSpace (volume, &sectors_per_clust, &bytes_per_sect,
			    &free_clusters, &total_clusters))
	{
	  *size_in_bytes = bytes_per_sect;
	  return (OSAL_OK);
	}
      else
	return (OSAL_ERR);
    }
  else
    { /* if called with no volume name ask for full path */
      char full_path [MAX_PATH], *dummy;
      DWORD result = GetFullPathName (volume_root, MAX_PATH, full_path, &dummy);
      if (result <= MAX_PATH)
	return (osal_get_volume_sect_size (full_path, size_in_bytes));
      else
	return (RET_BAD_FORMAT);
    }
}


/**************************************************************/
int
osal_get_file_size (osal_handle_t handle,
		    bigint_t *size_in_bytes)
{
  LARGE_INTEGER size;
  if (GetFileSizeEx (handle, &size))
    {
      *size_in_bytes = size.QuadPart;
      return (OSAL_OK);
    }
  else
    return (OSAL_ERR);
}


/**************************************************************/
int
osal_get_file_size_ex (const char *path,
		       bigint_t *size_in_bytes)
{
  osal_handle_t in;
  int result = osal_open (path, &in, 1);
  if (result == OSAL_OK)
    {
      result = osal_get_file_size (in, size_in_bytes);
      osal_close (in);
    }
  return (result);
}


/**************************************************************/
int
osal_seek (osal_handle_t handle,
	   bigint_t abs_pos)
{
  LARGE_INTEGER offs;
  offs.QuadPart = abs_pos;
  return (SetFilePointerEx (handle, offs, NULL, FILE_BEGIN) ? OSAL_OK : OSAL_ERR);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_read (osal_handle_t handle,
	   void *out,
	   size_t bytes,
	   size_t *stored)
{
  DWORD len;
  if (ReadFile (handle, out, bytes, &len, NULL))
    {
      *stored = len;
      return (OSAL_OK);
    }
  else
    return (OSAL_ERR);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_write (osal_handle_t handle,
	    const void *in,
	    size_t bytes,
	    size_t *stored)
{
  DWORD len;
  if (WriteFile (handle, in, bytes, &len, NULL))
    {
      *stored = len;
      return (OSAL_OK);
    }
  else
    return (OSAL_ERR);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_close (osal_handle_t handle)
{
  return (CloseHandle (handle) ? OSAL_OK : OSAL_ERR);
}


/**************************************************************/
void*
osal_alloc (size_t bytes)
{
  return (LocalAlloc (LMEM_FIXED, bytes));
}


/**************************************************************/
void
osal_free (void *ptr)
{
  if (ptr != NULL)
    LocalFree (ptr);
}


/**************************************************************/
int
osal_query_hard_drives (osal_dlist_t **hard_drives)
{
  size_t i;
  int result;

  *hard_drives = NULL;
  result = osal_dlist_alloc (hard_drives);
  for (i=0; result == RET_OK && i<16; ++i)
    {
      char device_name [20];
      HANDLE device;
      sprintf (device_name, "\\\\.\\PhysicalDrive%d", i);
      if (osal_open (device_name, &device, TRUE) == OSAL_OK)
	{ /* device exists */
	  LONGLONG size_in_bytes;
	  sprintf (device_name, "hdd%d:", i);
	  if (osal_get_estimated_device_size (device, &size_in_bytes) == OSAL_OK)
	    result = osal_dlist_add (*hard_drives, device_name, size_in_bytes,
				     is_apa_partition (device), ERROR_SUCCESS);
	  else
	    result = osal_dlist_add (*hard_drives, device_name, (bigint_t) 0, 0, GetLastError ());
	  
	  CloseHandle (device);
	}
      else
	break; /* first open error is the end of list */
    }

  if (result != RET_OK &&
      *hard_drives != NULL)
    osal_dlist_free (*hard_drives);

  return (result);
}


/**************************************************************/
int
osal_query_optical_drives (osal_dlist_t **optical_drives)
{
  size_t i;
  int result;

  *optical_drives = NULL;
  result = osal_dlist_alloc (optical_drives);
  for (i=0; result == RET_OK && i<16; ++i)
    {
      char device_name [20];
      HANDLE device;
      sprintf (device_name, "\\\\.\\CdRom%d", i);
      if (osal_open (device_name, &device, TRUE) == OSAL_OK)
	{ /* device exists */
	  LONGLONG size_in_bytes;
	  sprintf (device_name, "cd%d:", i);
	  if (osal_get_estimated_device_size (device, &size_in_bytes) == OSAL_OK)
	    result = osal_dlist_add (*optical_drives, device_name,
				     size_in_bytes, 0, ERROR_SUCCESS);
	  else
	    result = osal_dlist_add (*optical_drives, device_name,
				     (bigint_t) 0, 0, GetLastError ());
	  
	  CloseHandle (device);
	}
      else
	break; /* first open error is the end of list */
    }

  if (result != RET_OK &&
      *optical_drives != NULL)
    osal_dlist_free (*optical_drives);

  return (result);
}


/**************************************************************/
int
osal_query_devices (osal_dlist_t **hard_drives,
		    osal_dlist_t **optical_drives)
{
  int result = osal_query_hard_drives (hard_drives);
  if (result == RET_OK)
    result = osal_query_optical_drives (optical_drives);
  return (result);
}


/**************************************************************/
static int
osal_dlist_alloc (osal_dlist_t **dlist)
{
  *dlist = osal_alloc (sizeof (osal_dlist_t));
  if (*dlist != NULL)
    {
      (*dlist)->allocated = (*dlist)->used = 0;
      (*dlist)->device = NULL;
      return (RET_OK);
    }
  else
    return (RET_NO_MEM);
}


/**************************************************************/
static int
osal_dlist_add (osal_dlist_t *dlist,
		const char *name,
		bigint_t capacity,
		int is_ps2,
		unsigned long status)
{
  osal_dev_t *dev;
  if (dlist->allocated == dlist->used)
    { /* allocate memory if necessary */
      osal_dev_t *tmp = osal_alloc ((dlist->allocated + 16) * sizeof (osal_dev_t));
      if (tmp != NULL)
	{
	  if (dlist->device != NULL)
	    {
	      memcpy (tmp, dlist->device, dlist->used * sizeof (osal_dev_t));
	      osal_free (dlist->device);
	    }
	  dlist->device = tmp;
	  dlist->allocated += 16;
	}
      else
	return (RET_NO_MEM);
    }

  /* add the new entry */
  dev = dlist->device + dlist->used;
  strncpy (dev->name, name, DEV_MAX_NAME_LEN);
  dev->name [DEV_MAX_NAME_LEN - 1] = '\0';
  dev->capacity = capacity;
  dev->is_ps2 = is_ps2;
  dev->status = status;
  ++dlist->used;
  return (RET_OK);
}


/**************************************************************/
void
osal_dlist_free (osal_dlist_t *dlist)
{
  if (dlist != NULL)
    {
      if (dlist->device != NULL)
	osal_free (dlist->device);
      osal_free (dlist);
    }
}


/**************************************************************/
int /* RET_OK, RET_BAD_FORMAT, RET_BAD_DEVICE */
osal_map_device_name (const char *input,
		      char output [MAX_PATH])
{
  if (memcmp (input, "hdd", 3) == 0)
    {
      char *endp;
      long index = strtol (input + 3, &endp, 10);
      if (endp == input + 3)
	return (RET_BAD_FORMAT); /* bad format: no number after hdd */
      if (endp [0] == ':' &&
	  endp [1] == '\0')
	{
	  sprintf (output, "\\\\.\\PhysicalDrive%ld", index);
	  return (RET_OK);
	}
      else
	return (RET_BAD_FORMAT);
    }
  else if (memcmp (input, "cd", 2) == 0)
    {
      char *endp;
      long index = strtol (input + 2, &endp, 10);
      if (endp == input + 2)
	return (RET_BAD_FORMAT); /* bad format: no number after hdd */
      if (endp [0] == ':' &&
	  endp [1] == '\0')
	{
	  sprintf (output, "\\\\.\\CdRom%ld", index);
	  return (RET_OK);
	}
      else
	return (RET_BAD_FORMAT);
    }
  else
    return (RET_BAD_DEVICE);
}
