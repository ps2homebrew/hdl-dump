/*
 * common.c
 * $Id: common.c,v 1.12 2004/09/26 19:39:39 b081 Exp $
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
#include "common.h"
#include "retcodes.h"
#include "osal.h"


#define MAX_READ_FILE_SIZE (1 * 1024 * 1024) /* 1MB */


/**************************************************************/
char*
ltrim (char *text)
{
  char *dest = text, *src = text;
  while (*src == ' ' || *src == '\t')
    ++src; /* find first non-white */
  if (src != dest)
    do
      *dest++ = *src++;
    while (*src != '\0'); /* do-while ensures that \0 is copied */
  return (text);
}


/**************************************************************/
char*
rtrim (char *text)
{
  char *last_non_space = text - 1;
  char *p = text;
  while (*p)
    {
      if (*p != ' ' && *p != '\t')
	last_non_space = p;
      ++p;
    }
  *(last_non_space + 1) = '\0';
  return (text);
}


/**************************************************************/
int /* nonzero if same, zero if different */
caseless_compare (const char *s1,
		  const char *s2)
{
  if (s1 != NULL && s2 != NULL)
    {
      while (*s1 != '\0' && *s2 != '\0')
	{
	  if (tolower (*s1) == tolower (*s2))
	    {
	      ++s1;
	      ++s2;
	    }
	  else
	    return (0);
	}
      return (*s1 == *s2);
    }
  else if (s1 == NULL && s2 == NULL)
    return (1);
  else
    return (0);
}


/**************************************************************/
int /* would copy until EOF if bytes == 0 */
copy_data (osal_handle_t in,
	   osal_handle_t out,
	   bigint_t bytes,
	   size_t buff_size,
	   progress_t *pgs)
{
  void *buffer = osal_alloc (buff_size);
  if (buffer != NULL)
    {
      bigint_t copied = 0;
      int result, copy_til_eof = (bytes == 0);
      size_t len;
      do
	{
	  size_t chunk_size = (copy_til_eof ? buff_size :
			       ((bigint_t) buff_size < bytes) ? buff_size : (size_t) bytes);
	  result = osal_read (in, buffer, chunk_size, &len);
	  if (result == OSAL_OK && len > 0)
	    {
	      result = osal_write (out, buffer, len, &len);
	      if (result == OSAL_OK)
		{
		  copied += chunk_size;
		  result = pgs_update (pgs, copied);
		}
	    }
	  if (!copy_til_eof)
	    bytes -= chunk_size;
	}
      while (result == OSAL_OK && (copy_til_eof || bytes > 0) && len > 0); /* len == 0 @ EOF */
      osal_free (buffer);
      return (result);
    }
  else
    return (RET_NO_MEM);
}


/**************************************************************/
int
read_file (const char *file_name,
	   char **data,
	   size_t *len)
{
  osal_handle_t file;
  int result = osal_open (file_name, &file, 0);
  *data = NULL;
  if (result == OSAL_OK)
    {
      bigint_t size;
      result = osal_get_file_size (file, &size);
      if (result == OSAL_OK)
	{
	  if (size <= MAX_READ_FILE_SIZE)
	    {
	      *data = osal_alloc ((size_t) (size + 1));
	      if (*data != NULL)
		{
		  result = osal_read (file, *data, (size_t) size, len);
		  (*data) [*len] = '\0'; /* zero-terminate */
		}
	      else
		result = RET_NO_MEM;
	    }
	  else
	    result = RET_NO_MEM; /* file size is limited to MAX_READ_FILE_SIZE bytes */
	}
      osal_close (file);
    }
  if (result != OSAL_OK)
    if (*data != NULL)
      osal_free (*data);
  return (result);
}


/**************************************************************/
int
write_file (const char *file_name,
	    const void *data,
	    size_t len)
{
  osal_handle_t handle;
  int result = osal_create_file (file_name, &handle, (bigint_t) len);
  if (result == OSAL_OK)
    {
      size_t bytes;
      result = osal_write (handle, data, len, &bytes);
      result = osal_close (handle) == OSAL_OK ? result : OSAL_ERR;
    }
  return (result);
}


/**************************************************************/
int
dump_device (const char *device_name,
	     const char *output_file,
	     bigint_t max_size,
	     progress_t *pgs)
{
  osal_handle_t device;
  int result = osal_open (device_name, &device, 1);
  if (result == OSAL_OK)
    {
      bigint_t size_in_bytes = 0;
      result = osal_get_estimated_device_size (device, &size_in_bytes);
      if (result == OSAL_OK)
	{
	  osal_handle_t file;

	  pgs_prepare (pgs, size_in_bytes);

	  /* limit file size if requested */
	  if (max_size > 0 &&
	      size_in_bytes > max_size)
	    size_in_bytes = max_size;

	  result = osal_create_file (output_file, &file, size_in_bytes);
	  if (result == OSAL_OK)
	    {
	      result = copy_data (device, file, (bigint_t) 0, 1 _MB, pgs);
	      /* TODO: store GetLastError () on error */
	      result = osal_close (file) == OSAL_OK ? result : OSAL_ERR;
	    }
	}
      osal_close (device);
      /* TODO: restore last error on error */
    }

  return (result);
}


/**************************************************************/
int
file_exists (const char *path)
{
  osal_handle_t in;
  int result = osal_open (path, &in, 1);
  if (result == OSAL_OK)
    {
      osal_close (in);
      return (1);
    }
  else
    return (0);
}


/**************************************************************/
int
lookup_file (char original_file [MAX_PATH],
	     const char *secondary_file)
{
  if (file_exists (original_file))
    return (RET_OK); /* ok */
  else
    { /* check for the source in the same folder as the cue file */
      char tmp [MAX_PATH];
      const char *src_fname_start, *cue_path_end;
      char *p;
      size_t len;

      strcpy (tmp, original_file);
      src_fname_start = strrchr (tmp, '\\'); /* windowz-style */
      if (src_fname_start == NULL) src_fname_start = strrchr (tmp, '/'); /* unix-style */
      if (src_fname_start == NULL) src_fname_start = tmp - 1;
      ++src_fname_start; /* skip \ or / character */

      cue_path_end = strrchr (secondary_file, '\\');
      if (cue_path_end == NULL) cue_path_end = strrchr (secondary_file, '/');
      if (cue_path_end == NULL) cue_path_end = secondary_file - 1;
      ++cue_path_end;

      /* no check for buffer overflow! */
      p = original_file; len = cue_path_end - secondary_file;
      memcpy (p, secondary_file, len); p += len; /* includes `\' or `/` */
      strcpy (p, src_fname_start);

      return (file_exists (original_file) ? RET_OK : RET_FILE_NOT_FOUND);
    }
}


/**************************************************************/
int
iin_copy (iin_t *iin,
	  osal_handle_t out,
	  size_t start_sector,
	  size_t num_sectors,
	  progress_t *pgs)
{
  int result = OSAL_OK;
  bigint_t copied = 0;
  size_t data_len = 1;
  while (result == OSAL_OK && num_sectors > 0 && data_len > 0)
    {
      const char *data;
      size_t bytes_written;
      size_t sectors = num_sectors > IIN_NUM_SECTORS ? IIN_NUM_SECTORS : num_sectors;
      result = iin->read (iin, start_sector, sectors, &data, &data_len);
      if (result == OSAL_OK)
	result = osal_write (out, data, data_len, &bytes_written);
      if (result == OSAL_OK)
	{
	  size_t sectors_read = data_len / IIN_SECTOR_SIZE;
	  num_sectors -= sectors_read;
	  start_sector += sectors_read;
	  copied += data_len;
	  result = pgs_update (pgs, copied);
	}
    }

  return (result);
}


/**************************************************************/
int
iin_copy_ex (iin_t *iin,
	     hio_t *hio,
	     size_t input_start_sector,
	     size_t output_start_sector,
	     size_t num_sectors,
	     progress_t *pgs)
{
  int result = OSAL_OK;
  bigint_t copied = 0;
  size_t data_len = 1;
  while (result == OSAL_OK && num_sectors > 0 && data_len > 0)
    {
      const char *data;
      size_t bytes_written;
      size_t sectors_read;
      size_t sectors = num_sectors > IIN_NUM_SECTORS ? IIN_NUM_SECTORS : num_sectors;
      result = iin->read (iin, input_start_sector, sectors, &data, &data_len);
      if (result == OSAL_OK)
	{
	  sectors_read = data_len / IIN_SECTOR_SIZE;
	  result = hio->write (hio, output_start_sector, data_len / 512, data, &bytes_written);
	  if (result == OSAL_OK)
	    result = bytes_written == data_len ? OSAL_OK : OSAL_ERR;
	}
      if (result == OSAL_OK)
	{
	  num_sectors -= sectors_read;
	  input_start_sector += sectors_read;
	  output_start_sector += data_len / 512;
	  copied += data_len;
	  result = pgs_update (pgs, copied);
	}
    }

  return (result);
}
