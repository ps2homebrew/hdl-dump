/*
 * iin_img_base.c
 * $Id: iin_img_base.c,v 1.6 2004/08/20 12:35:17 b081 Exp $
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
#include "iin_img_base.h"
#include "osal.h"
#include "retcodes.h"
#include "aligned.h"
#include "common.h"


#define PARTS_GROW 64


typedef struct part_type
{
  size_t offset_s, length_s; /* in IIN_SECTOR_SIZE-based sectors */
  bigint_t skip; /* number of bytes to skip at the begining of the input (header?) */
  char *input_path;
  size_t device_sector_size; /* sector size of the device where the input is located */
} part_t;

struct iin_img_base_type
{
  iin_t iin;

  part_t *current; /* where "file" and "al" currently are */
  osal_handle_t file;
  aligned_t *al;

  char *unaligned, *buffer;
  size_t raw_sector_size, raw_skip_offset;
  size_t offset_s;

  size_t num_parts, alloc_parts;
  part_t *parts;

  unsigned long error_code; /* against osal_... */
};


/**************************************************************/
static void
close_current (iin_img_base_t *img_base)
{
  if (img_base->current != NULL)
    { /* close the old input */
      al_free (img_base->al);
      osal_close (img_base->file);
      img_base->current = NULL;
    }
}


/**************************************************************/
int
img_base_add_part (iin_img_base_t *img_base,
		   const char *input_path,
		   size_t length_s,
		   bigint_t skip,
		   size_t device_sector_size)
{
  part_t *prev, *dest;
  size_t len;
  if (img_base->num_parts == img_base->alloc_parts)
    { /* (re)alloc memory */
      size_t bytes = (img_base->alloc_parts + PARTS_GROW) * sizeof (part_t);
      part_t *tmp = (part_t*) osal_alloc (bytes);
      if (tmp != NULL)
	{
	  memset (tmp, 0, bytes);
	  if (img_base->parts != NULL)
	    { /* move old data and release old buffer */
	      memcpy (tmp, img_base->parts, img_base->num_parts * sizeof (part_t));
	      osal_free (img_base->parts);
	    }
	  img_base->parts = tmp;
	  img_base->alloc_parts += PARTS_GROW;
	}
      else
	return (RET_NO_MEM);
    }

  prev = img_base->num_parts > 0 ? img_base->parts + img_base->num_parts - 1 : NULL;
  dest = img_base->parts + img_base->num_parts;
  dest->offset_s = img_base->offset_s;
  dest->length_s = length_s;
  dest->skip = skip;
  len = strlen (input_path);
  dest->input_path = osal_alloc (len + 1);
  if (dest->input_path != NULL)
    strcpy (dest->input_path, input_path);
  else
    return (RET_NO_MEM);
  dest->device_sector_size = device_sector_size;
  img_base->offset_s += length_s;
  ++img_base->num_parts;
  return (RET_OK);
}


/**************************************************************/
void
img_base_add_gap (iin_img_base_t *img_base,
		  size_t length_s)
{
  img_base->offset_s += length_s;
}


/**************************************************************/
static int
img_base_stat (iin_t *iin,
	       size_t *sector_size,
	       size_t *num_sectors)
{
  iin_img_base_t *img_base = (iin_img_base_t*) iin;
  *sector_size = IIN_SECTOR_SIZE;
  if (img_base->num_parts > 0)
    *num_sectors = (img_base->parts [img_base->num_parts - 1].offset_s +
		    img_base->parts [img_base->num_parts - 1].length_s);
  else
    *num_sectors = 0; /* no parts set up */
  return (OSAL_OK);
}


/**************************************************************/
static int
img_base_read (iin_t *iin,
	       size_t start_sector,
	       size_t num_sectors,
	       const char **data,
	       size_t *length)
{
  iin_img_base_t *img_base = (iin_img_base_t*) iin;
  int result = OSAL_OK;

  if (img_base->current != NULL &&
      img_base->current->offset_s <= start_sector &&
      start_sector < img_base->current->offset_s + img_base->current->length_s)
    ; /* current part contains (at least part of) the requested data */
  else
    { /* locate the part containing the requested data */
      part_t *prev = NULL, *part;
      int found = 0, gap = 0;
      size_t i;
      for (i=0; i<img_base->num_parts; ++i)
	{
	  part = img_base->parts + i;
	  if (part->offset_s <= start_sector &&
	      start_sector < part->offset_s + part->length_s)
	    { /* part found */
	      found = 1; break;
	    }
	  else if (start_sector < part->offset_s)
	    { /* a gap found */
	      gap = 1; break;
	    }
	  prev = part;
	}
      if (found)
	{
	  if (img_base->current != NULL &&
	      caseless_compare (img_base->current->input_path, part->input_path))
	    img_base->current = part; /* do not reopen if new input is the same */
	  else
	    { /* open the new input */
	      osal_handle_t in;
	      size_t cache_size =
		((img_base->raw_sector_size * IIN_NUM_SECTORS + img_base->raw_sector_size - 1) /
		 part->device_sector_size);
	      result = osal_open (part->input_path, &in, 1); /* with no cache */
	      if (result == OSAL_OK)
		{
		  aligned_t *al = al_alloc (in, part->device_sector_size, cache_size);
		  if (al != NULL)
		    { /* switch the old input with the new one */
		      close_current (img_base);
		      img_base->al = al;
		      img_base->file = in;
		      img_base->current = part;
		    }
		  else
		    result = RET_NO_MEM;
		  if (result != OSAL_OK)
		    osal_close (in); /* al allocation failed? */
		}
	    }
	}
      else if (gap == 1)
	{ /* a gap between "prev" and "part" */
	  size_t gap_start_s = prev != NULL ? prev->offset_s + prev->length_s : 0;
	  size_t gap_length_s = part->offset_s - gap_start_s;
	  size_t til_end_s = gap_start_s + gap_length_s - start_sector;

	  *length = (num_sectors > til_end_s ? til_end_s : num_sectors) * IIN_SECTOR_SIZE;
	  if (*length > IIN_NUM_SECTORS * IIN_SECTOR_SIZE)
	    *length = IIN_NUM_SECTORS * IIN_SECTOR_SIZE;
	  memset (img_base->buffer, 0, *length);
	  *data = img_base->buffer;
	  return (OSAL_OK);
	}
      else
	{ /* behind the end-of-file */
	  *length = 0;
	  return (OSAL_OK);
	}
    }

  if (result == OSAL_OK)
    {
      bigint_t offset =
	img_base->current->skip +
	(bigint_t) (start_sector - img_base->current->offset_s) * img_base->raw_sector_size;
      size_t len;
      const char *raw_data;
      int result = al_read (img_base->al, offset, &raw_data,
			    num_sectors * img_base->raw_sector_size, &len);
      if (result == OSAL_OK)
	{
	  size_t num_sect = (len + img_base->raw_sector_size - 1) / img_base->raw_sector_size;
	  size_t uncomplete =
	    (img_base->raw_sector_size - len % img_base->raw_sector_size) %
	    img_base->raw_sector_size;

	  if (start_sector + num_sectors > img_base->current->length_s)
	    { /* check if read after the end has not been tried */
	      size_t sectors_til_end =
		img_base->current->offset_s + img_base->current->length_s - start_sector;
	      if (num_sect > sectors_til_end)
		num_sect = sectors_til_end;
	    }

	  /* do not copy data if structure is 2048/plain (would safe some CPU) */
	  if (img_base->raw_sector_size == IIN_SECTOR_SIZE &&
	      img_base->raw_skip_offset == 0)
	    *data = raw_data;
	  else
	    {
	      size_t i;
	      for (i=0; i<num_sect; ++i)
		memcpy (img_base->buffer + i * IIN_SECTOR_SIZE,
			raw_data + i * img_base->raw_sector_size + img_base->raw_skip_offset,
			IIN_SECTOR_SIZE);
	      *data = img_base->buffer;
	    }
	  *length = num_sect * IIN_SECTOR_SIZE;

	  if (uncomplete == 0)
	    ;
	  else
	    { /* fill last sector with zeroes */
	      if (uncomplete > img_base->raw_skip_offset)
		{ /* last sector is incomplete */
		  size_t len_to_zero = uncomplete + img_base->raw_skip_offset;
		  memset ((char*) *data + *length - len_to_zero, 0, len_to_zero);
		}
	      else
		/* remove last sector at all; only header has been readen */
		--(*length);
	    }
	}
      else
	img_base->error_code = osal_get_last_error_code ();
    }
  return (result);
}


/**************************************************************/
static int
img_base_close (iin_t *iin)
{
  iin_img_base_t *img_base = (iin_img_base_t*) iin;
  size_t i;
  close_current (img_base);
  osal_free (img_base->unaligned);
  for (i=0; i<img_base->num_parts; ++i)
    osal_free (img_base->parts [i].input_path);
  osal_free (img_base->parts);
  osal_free (iin);
  return (OSAL_OK);
}


/**************************************************************/
static char*
img_base_last_error (iin_t *iin)
{
  iin_img_base_t *img_base = (iin_img_base_t*) iin;
  return (osal_get_error_msg (img_base->error_code));
}


/**************************************************************/
static void
img_base_dispose_error (iin_t *iin,
			char* error)
{
  osal_dispose_error_msg (error);
}


/**************************************************************/
iin_img_base_t*
img_base_alloc (size_t raw_sector_size,
		size_t raw_skip_offset)
{
  iin_img_base_t *img_base = (iin_img_base_t*) osal_alloc (sizeof (iin_img_base_t));
  if (img_base != NULL)
    {
      iin_t *iin = &img_base->iin;
      size_t buffer_size = (IIN_NUM_SECTORS + 1) * IIN_SECTOR_SIZE;
      char *buffer = osal_alloc (buffer_size);
      if (buffer != NULL)
	{ /* success */
	  memset (img_base, 0, sizeof (iin_img_base_t));
	  iin->stat = &img_base_stat;
	  iin->read = &img_base_read;
	  iin->close = &img_base_close;
	  iin->last_error = &img_base_last_error;
	  iin->dispose_error = &img_base_dispose_error;
	  img_base->unaligned = buffer;
	  img_base->buffer =
	    (void*) (((unsigned long) buffer + IIN_SECTOR_SIZE - 1) & ~(IIN_SECTOR_SIZE - 1));
	  assert (img_base->buffer >= img_base->unaligned);
	  img_base->raw_sector_size = raw_sector_size;
	  img_base->raw_skip_offset = raw_skip_offset;
	}
      else
	{ /* failed */
	  osal_free (img_base);
	  img_base = NULL;
	}
    }
  return (img_base);
}
