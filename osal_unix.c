/*
 * osal_unix.c
 * $Id: osal_unix.c,v 1.7 2006/05/21 21:41:15 bobi Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined (__APPLE__) || defined (__FreeBSD__)
/* patch for MacOS X + external USB HDD box by G.S. */
#  include <sys/disk.h>
#endif
#include <fcntl.h>
#include "retcodes.h"
#include "osal.h"
#include "apa.h"


/* memory-mapped files */
struct osal_mmap_type
{ /* keep anything required for unmap operation */
  void *start;
  size_t length;
};


/**************************************************************/
unsigned long
osal_get_last_error_code (void)
{
  return (errno);
}


/**************************************************************/
char*
osal_get_error_msg (unsigned long err)
{
  return (strerror (err));
}


/**************************************************************/
char*
osal_get_last_error_msg (void)
{
  return (osal_get_error_msg (osal_get_last_error_code ()));
}


/**************************************************************/
void
osal_dispose_error_msg (char *msg)
{
  /* this point should not be freed */
  msg = NULL;
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_open (const char *name,
	   osal_handle_t *handle,
	   int no_cache)
{
  handle->desc = open64 (name, O_RDONLY | O_LARGEFILE, 0);
  return (handle->desc == -1 ? OSAL_ERR : OSAL_OK);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_open_device_for_writing (const char *device_name,
			      osal_handle_t *handle)
{
  handle->desc = open (device_name, O_RDWR | O_LARGEFILE, S_IRUSR | S_IWUSR);
  return (handle->desc == -1 ? OSAL_ERR : OSAL_OK);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_create_file (const char *path,
		  osal_handle_t *handle,
		  u_int64_t estimated_size)
{
  int result = RET_ERR;
  handle->desc = open64 (path,
			 O_CREAT | O_EXCL | O_LARGEFILE | O_RDWR,
			 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (handle->desc != -1)
    { /* success */
      if (estimated_size > 0)
	{
	  off64_t offs = lseek64 (handle->desc, estimated_size - 1, SEEK_END);
	  if (offs != -1)
	    {
	      char dummy = '\0';
	      u_int32_t bytes = write (handle->desc, &dummy, 1);
	      if (bytes == 1)
		{
		  offs = lseek64 (handle->desc, 0, SEEK_SET);
		  if (offs == 0)
		    { /* success */
		      result = RET_OK;
		    }
		}
	    }
	}
      else
	result = RET_OK;

      if (result != RET_OK)
	{ /* delete file on error */
	  close (handle->desc);
	  handle->desc = -1; /* make sure "is file opened" test would fail */
	  unlink (path);
	}
    }
  else
    result = RET_ERR;
  return (result);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_get_estimated_device_size (osal_handle_t handle,
				u_int64_t *size_in_bytes)
{
  struct stat64 st;
  int result;
  memset (&st, 0, sizeof (struct stat64)); /* play on the safe side */
  result = fstat64 (handle.desc, &st) == 0 ? RET_OK : RET_ERR;
  if (result == RET_OK)
    { /* success */
      *size_in_bytes = st.st_size; /* might be 0 for block devices? */
      if (*size_in_bytes == 0)
	{ /* try with lseek... */
	  off64_t curr = lseek64 (handle.desc, 0, SEEK_CUR);
	  if (curr >= 0)
	    {
	      off64_t size = lseek64 (handle.desc, 0, SEEK_END);
#if defined (__APPLE__)
	      /* patch for MacOS X + external USB HDD box by G.S. */
	      if (size == 0)
		{
		  u_int32_t blocksize;
		  u_int64_t blockcount;
		  if (ioctl (handle.desc, DKIOCGETBLOCKSIZE, &blocksize) < 0)
		    return (OSAL_ERR);
		  if (ioctl (handle.desc, DKIOCGETBLOCKCOUNT, &blockcount) < 0)
		    return (OSAL_ERR);
		  size = blockcount * blocksize;
		}
#elif defined (__FreeBSD__)
	      /* basically the same as MacOS X patch, but ioctl is different */
	      if (size == 0)
		{
		  u_int blocksize;
		  off_t mediasize;
		  if (ioctl (handle.desc, DIOCGSECTORSIZE, &blocksize) < 0)
		    return (OSAL_ERR);
		  if (ioctl (handle.desc, DIOCGMEDIASIZE, &mediasize) < 0)
		    return (OSAL_ERR);
		  size = mediasize;
		}
#endif
	      if (size >= 0)
		{
		  *size_in_bytes = size;
		  result = (lseek64 (handle.desc, curr, SEEK_SET) >= 0 ?
			    OSAL_OK : OSAL_ERR);
		}
	      else
		result = RET_ERR;
	    }
	  else
	    result = RET_ERR;
	}
    }
  else
    result = RET_ERR;
  return (result);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_get_device_size (osal_handle_t handle,
		      u_int64_t *size_in_bytes)
{
  return (osal_get_estimated_device_size (handle, size_in_bytes));
}


/**************************************************************/
int
osal_get_device_sect_size (osal_handle_t handle,
			   u_int32_t *size_in_bytes)
{ /* TODO: osal_get_device_sect_size */
  *size_in_bytes = 4096; /* that is a resonable sector size */
  return (OSAL_OK);
}


/**************************************************************/
int
osal_get_volume_sect_size (const char *volume_root,
			   u_int32_t *size_in_bytes)
{
  struct stat64 st;
  int result = stat64 (volume_root, &st) == 0 ? RET_OK : RET_ERR;
  if (result == RET_OK)
    *size_in_bytes = st.st_blksize; 
  return (result);
}


/**************************************************************/
int
osal_get_file_size (osal_handle_t handle,
		    u_int64_t *size_in_bytes)
{
  off64_t offs_curr = lseek64 (handle.desc, 0, SEEK_CUR);
  if (offs_curr != -1)
    {
      off64_t offs_end = lseek64 (handle.desc, 0, SEEK_END);
      if (offs_end != -1)
	{
	  if (lseek64 (handle.desc, offs_curr, SEEK_SET) == offs_curr)
	    { /* success */
	      *size_in_bytes = offs_end;
	      return (RET_OK);
	    }
	}
    }
  return (RET_ERR);
}


/**************************************************************/
int
osal_get_file_size_ex (const char *path,
		       u_int64_t *size_in_bytes)
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
	   u_int64_t abs_pos)
{
  return (lseek64 (handle.desc, abs_pos, SEEK_SET) == -1 ? OSAL_ERR : OSAL_OK);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_read (osal_handle_t handle,
	   void *out,
	   u_int32_t bytes,
	   u_int32_t *stored)
{
  int n = read (handle.desc, out, bytes);
  if (n != -1)
    { /* success */
      *stored = n;
      return (OSAL_OK);
    }
  else
    return (OSAL_ERR);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_write (osal_handle_t handle,
	    const void *in,
	    u_int32_t bytes,
	    u_int32_t *stored)
{
  int n = write(handle.desc, in, bytes);
  if (n != -1)
    { /* success */
      *stored = n;
      return (OSAL_OK);
    }
  else
    return (OSAL_ERR);
}


/**************************************************************/
int /* OSAL_OK, OSAL_ERR */
osal_close (osal_handle_t handle)
{
  return (close (handle.desc) == -1 ? OSAL_ERR : OSAL_OK);
}


/**************************************************************/
void*
osal_alloc (u_int32_t bytes)
{
  return (malloc (bytes));
}


/**************************************************************/
void
osal_free (void *ptr)
{
  if (ptr != NULL)
    free (ptr);
}


/**************************************************************/
int
osal_mmap (osal_mmap_t **mm,
	   void **p,
	   osal_handle_t handle,
	   u_int64_t offset,
	   u_int32_t length)
{
  *mm = osal_alloc (sizeof (osal_mmap_t));
  if (*mm != NULL)
    {
      void *addr = mmap64 (NULL, length, PROT_READ, MAP_SHARED,
			   handle.desc, offset);
      if (addr != MAP_FAILED)
	{ /* success */
	  (*mm)->start = addr;
	  (*mm)->length = length;
	  *p = addr;
	  return (OSAL_OK);
	}
      else
	{ /* mmap failed */
	  osal_free (*mm), *mm = NULL;
	  return (OSAL_ERR);
	}
    }
  else /* malloc failed */
    return (OSAL_NO_MEM);
}


/**************************************************************/
int
osal_munmap (osal_mmap_t *mm)
{
  int result = munmap (mm->start, mm->length);
  if (result == 0)
    osal_free (mm);
  return (result == 0 ? OSAL_OK : OSAL_ERR);
}


/**************************************************************/
int
osal_query_hard_drives (osal_dlist_t **hard_drives)
{
  /* TODO: under unix, they are like files */
  return (OSAL_ERR);
}


/**************************************************************/
int
osal_query_optical_drives (osal_dlist_t **optical_drives)
{
  /* TODO: under unix, they are like files */
  return (OSAL_ERR);
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
  struct stat st;
  int result = stat (input, &st) == 0 ? RET_OK : RET_ERR;
  if (result == RET_OK)
    { /* accept the input, only if it is a block device */
#if !defined (_DEBUG) /* in debug mode treat files as devices */
      result = st.st_mode & S_IFBLK ? RET_OK : RET_BAD_DEVICE;
#endif
      if (result == RET_OK)
	strcpy (output, input);
    }
  return (result);
}
