/*
 * common.c
 * $Id: common.c,v 1.18 2007-05-12 20:14:05 bobi Exp $
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
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "retcodes.h"
#include "osal.h"
#include "hdl.h"


#define MAX_READ_FILE_SIZE (4 * 1024 * 1024) /* 4MB */


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
  while (*p != '\0')
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
	   u_int64_t bytes,
	   u_int32_t buff_size,
	   progress_t *pgs)
{
  void *buffer = osal_alloc (buff_size);
  if (buffer != NULL)
    {
      u_int64_t copied = 0;
      int result, copy_til_eof = (bytes == 0);
      u_int32_t len;
      do
	{
	  u_int32_t chunk_size = (copy_til_eof ? buff_size :
			       ((u_int64_t) buff_size < bytes) ? buff_size : (u_int32_t) bytes);
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
	   u_int32_t *len)
{
  /*@only@*/ osal_handle_t file = OSAL_HANDLE_INIT;
  int result = osal_open (file_name, &file, 0);
  *data = NULL; *len = 0;
  if (result == OSAL_OK)
    {
      u_int64_t size;
      result = osal_get_file_size (file, &size);
      if (result == OSAL_OK)
	{
	  if (size <= MAX_READ_FILE_SIZE)
	    {
	      *data = osal_alloc ((u_int32_t) (size + 1));
	      if (*data != NULL)
		{
		  result = osal_read (file, *data, (u_int32_t) size, len);
		  (*data) [*len] = '\0'; /* zero-terminate */
		}
	      else
		result = RET_NO_MEM;
	    }
	  else
	    result = RET_NO_MEM; /* file size is limited to MAX_READ_FILE_SIZE bytes */
	}
      (void) osal_close (&file);
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
	    u_int32_t len)
{
  /*@only@*/ osal_handle_t handle = OSAL_HANDLE_INIT;
  int result = osal_create_file (file_name, &handle, (u_int64_t) len);
  if (result == OSAL_OK)
    {
      u_int32_t bytes;
      result = osal_write (handle, data, len, &bytes);
      result = osal_close (&handle) == OSAL_OK ? result : OSAL_ERR;
    }
  return (result);
}


/**************************************************************/
int
dump_device (const dict_t *config,
	     const char *device_name,
	     const char *output_file,
	     u_int64_t max_size,
	     progress_t *pgs)
{
  /*@only@*/ iin_t *iin = NULL;
  int result = iin_probe (config, device_name, &iin);
  if (result == OSAL_OK)
    {
      u_int32_t sector_size, num_sectors;
      result = iin->stat (iin, &sector_size, &num_sectors);
      if (result == OSAL_OK)
	{
	  u_int64_t size_in_bytes = (u_int64_t) num_sectors * sector_size;
	  /*@only@*/ osal_handle_t file = OSAL_HANDLE_INIT;

	  pgs_prepare (pgs, size_in_bytes);

	  /* limit file size if requested */
	  if (max_size > 0 &&
	      size_in_bytes > max_size)
	    size_in_bytes = max_size;

	  result = osal_create_file (output_file, &file, size_in_bytes);
	  if (result == OSAL_OK)
	    {
	      u_int64_t offset = 0;
	      while (result == RET_OK && offset < size_in_bytes)
		{
		  u_int32_t bytes = 0;
		  const char *buf = NULL;
		  const u_int64_t BYTES_AT_ONCE = 1024 * 1024;
		  const u_int64_t remaining = size_in_bytes - offset;
		  const u_int32_t count =
		    (u_int32_t) (remaining > BYTES_AT_ONCE ?
				 BYTES_AT_ONCE : remaining);
		  result = iin->read (iin, offset / 2048, count / 2048,
				      &buf, &bytes);
		  if (result == RET_OK)
		    {
		      u_int32_t stored = 0;
		      result = osal_write (file, buf, bytes, &stored);
		      if (result == RET_OK)
			{
			  offset += count;
			  result = pgs_update (pgs, offset);
			}
		    }
		}
	      /* TODO: store GetLastError () on error */
	      result = osal_close (&file) == OSAL_OK ? result : OSAL_ERR;
	    }
	}
      (void) iin->close (iin);
      /* TODO: restore last error on error */
    }

  return (result);
}


/**************************************************************/
int
file_exists (const char *path)
{
  /*@only@*/ osal_handle_t in = OSAL_HANDLE_INIT;
  int result = osal_open (path, &in, 1);
  if (result == OSAL_OK)
    {
      (void) osal_close (&in);
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
      u_int32_t len;

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
	  u_int32_t start_sector,
	  u_int32_t num_sectors,
	  progress_t *pgs)
{
  int result = OSAL_OK;
  u_int64_t copied = 0;
  u_int32_t data_len = 1;
  while (result == OSAL_OK && num_sectors > 0 && data_len > 0)
    {
      const char *data;
      u_int32_t bytes_written;
      u_int32_t sectors =
	num_sectors > IIN_NUM_SECTORS ? IIN_NUM_SECTORS : num_sectors;
      result = iin->read (iin, start_sector, sectors, &data, &data_len);
      if (result == OSAL_OK)
	result = osal_write (out, data, data_len, &bytes_written);
      if (result == OSAL_OK)
	{
	  u_int32_t sectors_read = data_len / IIN_SECTOR_SIZE;
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
	     u_int32_t input_start_sector,
	     u_int32_t output_start_sector,
	     u_int32_t num_sectors,
	     progress_t *pgs)
{
  int result = OSAL_OK;
  u_int64_t copied = 0;
  u_int32_t data_len = 1;
  while (result == OSAL_OK && num_sectors > 0 && data_len > 0)
    {
      const char *data;
      u_int32_t bytes_written;
      u_int32_t sectors_read;
      u_int32_t sectors = num_sectors > IIN_NUM_SECTORS ? IIN_NUM_SECTORS : num_sectors;
      result = iin->read (iin, input_start_sector, sectors, &data, &data_len);
      if (result == OSAL_OK)
	{
	  sectors_read = data_len / IIN_SECTOR_SIZE;
	  result = hio->write (hio, output_start_sector,
			       data_len / 512, data, &bytes_written);
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


/**************************************************************/
#if defined (_BUILD_WIN32)
const char*
get_app_data (void)
{
  static char path[1024];
  LONG path_size = sizeof (path);
  HKEY shell_fold = NULL;
  LONG retv = RegOpenKeyEx (HKEY_CURRENT_USER,
			    "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
			    0, KEY_READ,
			    &shell_fold);
  if (retv == ERROR_SUCCESS)
    {
      retv = RegQueryValue (shell_fold, "AppData", path, &path_size);
      (void) RegCloseKey (shell_fold);
    }
  return (retv == ERROR_SUCCESS ? path : NULL);
}
#endif /* _BUILD_WIN32 defined? */


/**************************************************************/
const char*
get_config_file (void)
{
  static char config_file[256] = { "" };

  if (*config_file == '\0')
    { /* default location */
      const char *p;
      strcpy (config_file, "./hdl_dump.conf");

      /* decide where config file is depending on the OS/build type */
#if defined (_BUILD_WIN32)
      p = get_app_data ();
      if (p != NULL)
	{
	  strcpy (config_file, p);
	  strcat (config_file, "\\hdl_dump.conf");
	}
      else
	{
	  p = getenv ("USERPROFILE");
	  if (p != NULL)
	    {
	      strcpy (config_file, p);
	      strcat (config_file, "\\Application Data\\hdl_dump.conf");
	    }
	}
#elif defined (_BUILD_UNIX)
      p = getenv ("HOME");
      if (p != NULL)
	{
	  strcpy (config_file, p);
	  strcat (config_file, "/.hdl_dump.conf");
	}
#endif
    }

  return (config_file);
}


/**************************************************************/
void
set_config_defaults (dict_t *config)
{
  char disc_database[256];
  const char *p;

#if defined (_BUILD_WIN32)
  /* disable ASPI by default */
  dict_put_flag (config, CONFIG_ENABLE_ASPI_FLAG, 0);
#endif

  /* decide where disc compatibility database would be kept */
  strcpy (disc_database, "./hdl_dump.list"); /* default */
#if defined (_BUILD_WIN32) && !defined (_BUILD_WINE)
  p = get_app_data ();
  if (p != NULL)
    {
      strcpy (disc_database, p);
      strcat (disc_database, "\\hdl_dump.list");
    }
  else
    {
      p = getenv ("USERPROFILE");
      if (p != NULL)
	{
	  strcpy (disc_database, p);
	  strcat (disc_database, "\\Application Data\\hdl_dump.list");
	}
    }
#else
  /* Unix/Linux/WineLib */
  p = getenv ("HOME");
  if (p != NULL)
    {
      strcpy (disc_database, p);
      strcat (disc_database, "/.hdl_dump.list");
    }
#endif
  (void) dict_put (config, CONFIG_DISC_DATABASE_FILE, disc_database);
}


/**************************************************************/
compat_flags_t
parse_compat_flags (const char *flags)
{
  compat_flags_t result = 0;
  if (flags != NULL)
    {
      size_t len = strlen (flags), i;
      if (flags[0] == '0' && flags[1] == 'x')
	{ /* hex: 0x... */
	  unsigned long retval = strtoul (flags, NULL, 0);
	  if (retval < (1 << MAX_FLAGS))
	    result = (compat_flags_t) retval;
	  else
	    /* out-of-range */
	    result = COMPAT_FLAGS_INVALID;
	}
      else if (flags[0] == '+' && (len % 2) == 0)
	{ /* +1, +1+2, +2+3, ... */
	  for (i=0; i<len/2; ++i)
	    {
	      if (flags[i * 2 + 0] == '+')
		{
		  int flag = flags[i * 2 + 1] - '0';
		  if (flag >= 1 && flag <= MAX_FLAGS)
		    { /* support up to MAX_FLAGS flags */
		      int bit = 1 << (flag - 1);
		      if ((result & bit) == 0)
			result |= bit;
		      else
			{ /* flag used twice */
			  result = COMPAT_FLAGS_INVALID;
			  break;
			}
		    }
		  else
		    { /* not in [1..MAX_FLAGS] */
		      result = COMPAT_FLAGS_INVALID;
		      break;
		    }
		}
	      else
		{ /* pair doesn't start with a plus */
		  result = COMPAT_FLAGS_INVALID;
		  break;
		}
	    }
	}
      else
	{ /* don't know how to handle those flags */
	  result = COMPAT_FLAGS_INVALID;
	}
    }
  return (result);
}


/**************************************************************/
int
ddb_lookup (const dict_t *config,
	    const char *startup,
	    char name[HDL_GAME_NAME_MAX + 1],
	    compat_flags_t *flags)
{
  int result;
  const char *disc_db = dict_lookup (config, CONFIG_DISC_DATABASE_FILE);
  dict_t *list = disc_db != NULL ? dict_restore (NULL, disc_db) : NULL;
  *name = '\0'; *flags = 0;
  if (list != NULL)
    {
      const char *entry = dict_lookup (list, startup);
      if (entry != NULL)
	{ /* game info has been found */
	  size_t entry_len = strlen (entry);
	  char *pos = strrchr (entry, ';');

	  result = RET_OK;
	  if (pos != NULL && pos[1] != 'x')
	    { /* scan for compatibility flags */
	      compat_flags_t flags2 = 0;
	      if (strcmp (pos + 1, "0") != 0)
		flags2 = parse_compat_flags (pos + 1);
	      if (flags2 != COMPAT_FLAGS_INVALID)
		{ /* we have compatibility flags */
		  *flags = flags2;
		  entry_len = pos - entry;
		}
	    }
	  else if (pos != NULL && pos[1] == 'x')
	    { /* marked as incompatible; warn */
	      result = RET_DDB_INCOMPATIBLE;
	      entry_len = pos - entry;
	    }
	  /* entry has been found => fill game name */
	  memcpy (name, entry, (entry_len < HDL_GAME_NAME_MAX ?
				entry_len : HDL_GAME_NAME_MAX));
	  name[entry_len < HDL_GAME_NAME_MAX ? entry_len : HDL_GAME_NAME_MAX] = '\0';
	}
      else
	result = RET_NO_DDBENTRY;
      dict_free (list);
    }
  else
    result = RET_NO_DISC_DB;
  return (result);
}


/**************************************************************/
int
ddb_update (const dict_t *config,
	    const char *startup,
	    const char *name,
	    compat_flags_t flags)
{
  int result = RET_OK;
  const char *disc_db = dict_lookup (config, CONFIG_DISC_DATABASE_FILE);
  dict_t *list = dict_restore (NULL, disc_db);
  if (list != NULL)
    {
      char tmp[500];
      compat_flags_t dummy;
      result = ddb_lookup (config, startup, tmp, &dummy);
      if (result != RET_DDB_INCOMPATIBLE)
	{ /* do not overwrite entries, marked as incompatible */
	  sprintf (tmp, "%s;0x%02x", name, flags);
	  (void) dict_put (list, startup, tmp);

	  result = dict_store (list, disc_db);
	}
      dict_free (list), list = NULL;
    }
  else
    result = RET_NO_DISC_DB;
  return (result);
}
