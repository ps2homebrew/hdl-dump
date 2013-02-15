/*
 * iin_iml.c
 * $Id: iin_iml.c,v 1.9 2006/09/01 17:25:20 bobi Exp $
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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "iin_iml.h"
#include "iin_img_base.h"
#include "osal.h"
#include "retcodes.h"
#include "common.h"


#define GROW_ALLOC 256


typedef struct iml_file_type
{
  u_int64_t offset; /* relative to the begining of the file */
  u_int32_t start_s, end_s;
  char *path;
} iml_file_t;

typedef struct iml_files_type
{
  u_int32_t used, alloc;
  iml_file_t *files;
} iml_files_t;


typedef struct iin_iml_type
{
  iin_t iin;
  FILE *zero;
} iin_iml_t;


/**************************************************************/
static iml_files_t*
list_alloc (void)
{
  iml_files_t *list = (iml_files_t*) osal_alloc (sizeof (iml_files_t));
  if (list != NULL)
    {
      memset (list, 0, sizeof (iml_files_t));
    }
  return (list);
}


/**************************************************************/
static void
list_free (iml_files_t *list)
{
  u_int32_t i;
  for (i=0; i<list->used; ++i)
    osal_free (list->files [i].path);
  osal_free (list->files);
  osal_free (list);
}


/**************************************************************/
static int
list_grow (iml_files_t *list)
{
  iml_file_t *tmp =
    (iml_file_t*) osal_alloc ((list->alloc + GROW_ALLOC) * sizeof (iml_file_t));
  if (tmp != NULL)
    {
      if (list->files != NULL)
	{ /* transfer old data and free old buffer */
	  memcpy (tmp, list->files, list->used * sizeof (iml_file_t));
	  osal_free (list->files);
	}
      list->files = tmp;
      list->alloc += GROW_ALLOC;
      return (OSAL_OK);
    }
  else
    return (RET_NO_MEM);
}


/**************************************************************/
static int
list_add_file (iml_files_t *list,
	       const char *path,
	       u_int32_t start_s,
	       u_int32_t end_s,
	       u_int64_t offset)
{
  int result = OSAL_OK;
  if (list->used == list->alloc)
    result = list_grow (list);
  if (result == OSAL_OK)
    {
      u_int32_t len = strlen (path);
      iml_file_t *dest;
      dest = list->files + list->used;
      dest->offset = offset;
      dest->start_s = start_s;
      dest->end_s = end_s;
      dest->path = osal_alloc (len + 1);
      if (dest->path != NULL)
	{
	  strcpy (dest->path, path);
	  dest->offset = offset;
	  ++list->used;
	}
      else
	result = RET_NO_MEM;
    }
  return (result);
}


/**************************************************************/
#define is_space_or_tab(ch) ((ch) == ' ' || (ch) == '\t')

static int
process_loc_line (iml_files_t *list,
		  char *line)
{ /* 322 322 0.0 0 "SYSTEM.CNF" */
  int result = OSAL_OK;
  u_int64_t offset;
  u_int32_t start_s, end_s;
  char *path, *endp;

  start_s = strtoul (line, &endp, 10); /* start */
  result = is_space_or_tab (*endp) ? OSAL_OK : RET_BAD_COMPAT;

  if (result == OSAL_OK)
    {
      while (is_space_or_tab (*endp)) ++endp;
      end_s = strtoul (endp, &endp, 10); /* end */
      result = is_space_or_tab (*endp) ? OSAL_OK : RET_BAD_COMPAT;
    }

  if (result == OSAL_OK)
    { /* dummies */
      while (is_space_or_tab (*endp)) ++endp;
      (void) strtod (endp, &endp);
      result = is_space_or_tab (*endp) ? OSAL_OK : RET_BAD_COMPAT;
      if (result == OSAL_OK)
	{
	  while (is_space_or_tab (*endp)) ++endp;
	  (void) strtol (endp, &endp, 10);
	  result = is_space_or_tab (*endp) ? OSAL_OK : RET_BAD_COMPAT;
	}
    }

  if (result == OSAL_OK)
    { /* path */
      while (is_space_or_tab (*endp)) ++endp;
      path = endp;
      if (*endp == '\"')
	{ /* "FILE NAME" */
	  ++path; ++endp; /* skip initial " */
	  while (*endp != '\"' && *endp != '\0') ++endp;
	  if (*endp == '\"')
	    *endp++ = '\0'; /* remove trailing " */
	  else
	    result = RET_NOT_COMPAT;
	}
      else
	/* FILE_NAME */
	while (!is_space_or_tab (*endp) && *endp != '\0') ++endp;

      if (result == OSAL_OK)
	{
	  if (*endp != '\0')
	    { /* offset? */
	      *endp++ = '\0';
	      while (is_space_or_tab (*endp)) ++endp;
	      offset = strtoul (endp, NULL, 10); /* could cause overflows, but it is not likely */
	    }
	  else
	    offset = 0;
	}
    }

  if (result == OSAL_OK)
    result = list_add_file (list, path, start_s, end_s, offset);
  return (result);
}


/**************************************************************/

enum section_type_t
  {
    st_unk,
    st_sys,
    st_cue,
    st_loc
  };

static int
build_file_list (const char *iml_path,
		 iml_files_t **list2)
{
  u_int64_t file_size;
  int result;
  iml_files_t *list = list_alloc ();

  result = list != NULL ? OSAL_OK : RET_NO_MEM;
  if (result == OSAL_OK)
    result = osal_get_file_size_ex (iml_path, &file_size);
  if (result == OSAL_OK)
    result = file_size <= 1 _MB ? OSAL_OK : RET_NOT_COMPAT;
  if (result == OSAL_OK)
    {
      char *data;
      u_int32_t length;
      result = read_file (iml_path, &data, &length);
      if (result == OSAL_OK)
	{
	  enum section_type_t sec = st_unk; /* current section */
	  char *line = strtok (data, "\r\n");
	  if (line != NULL && *line != '\0')
	    do
	      {
		if (caseless_compare (line, "[sys]"))
		  sec = st_sys;
		else if (caseless_compare (line, "[/sys]"))
		  sec = st_unk;
		else if (caseless_compare (line, "[cue]"))
		  sec = st_cue;
		else if (caseless_compare (line, "[/cue]"))
		  sec = st_unk;
		else if (caseless_compare (line, "[loc]"))
		  sec = st_loc;
		else if (caseless_compare (line, "[/loc]"))
		  sec = st_unk;
		else
		  { /* a line in one of the sections */
		    if (sec == st_loc &&
			line [0] != '#' &&
			isdigit (line [0]))
		      { /* a file: "322 322 0.0 0 \"SYSTEM.CNF\"" */
			result = process_loc_line (list, line);
		      }
		  }
		line = strtok (NULL, "\r\n");
	      }
	    while (line != NULL && *line != '\0');
	  osal_free (data);
	}
    }

  if (result == OSAL_OK)
    result = list->used > 0 ? OSAL_OK : RET_NOT_COMPAT;

  if (result == OSAL_OK)
    *list2 = list; /* success */
  else if (list != NULL)
    list_free (list);

  return (result);
}


/**************************************************************/
int
iin_iml_probe_path (const char *path,
		    iin_t **iin)
{
  iml_files_t *list;
  int result = build_file_list (path, &list);
  if (result == OSAL_OK)
    { /* gaps are automagically handled by iin_img_base_t */
      u_int32_t i;
      iml_file_t *prev = NULL;
      iin_img_base_t *img_base = img_base_alloc (2048 /* RAW sect size */, 0 /* skip per sect */);
      char source [MAX_PATH];
      u_int32_t device_sector_size;

      if (img_base != NULL)
	{
	  for (i=0; result == OSAL_OK && i<list->used; ++i)
	    {
	      iml_file_t *curr = list->files + i;
	      u_int32_t gap_s = prev != NULL ? curr->start_s - (prev->end_s + 1) : 0;
	      if (gap_s == 0)
		;
	      else
		/* add a gap between previous and current file */
		img_base_add_gap (img_base, gap_s);

	      strcpy (source, curr->path);
	      result = lookup_file (source, path);
	      if (result == OSAL_OK)
		result = osal_get_volume_sect_size (source, &device_sector_size);
	      else if (result == RET_FILE_NOT_FOUND)
		result = RET_BROKEN_LINK;
	      if (result == OSAL_OK)
		result = img_base_add_part (img_base, source, curr->end_s - curr->start_s + 1,
					    curr->offset, device_sector_size);
	      prev = curr;
	    }

	  if (result == OSAL_OK)
	    {
	      *iin = (iin_t*) img_base;
	      strcpy ((*iin)->source_type, "IML file");
	    }
	  else
	    ((iin_t*) img_base)->close ((iin_t*) img_base);
	}
      else
	result = RET_NO_MEM;

      list_free (list);
    }
  return (result);
}
