/*
 * iin.h
 * $Id: iin.h,v 1.5 2004/08/20 12:35:17 b081 Exp $
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

#if !defined (_IIN_H)
#define _IIN_H

#include <stddef.h>
#include "config.h"


#define IIN_SECTOR_SIZE 2048 /* CD/DVD sector size */
#define IIN_NUM_SECTORS  128 /* number of sectors to read at once */


/*
 * ISO input interface below
 */
typedef struct iin_type iin_t;


/* if RET_OK is returned source can be handled with that implementation;
   if so, iin is ready to process the input */
typedef int (*iin_probe_path_t) (const char *path,
				 iin_t **iin);

typedef int (*iin_stat_t) (iin_t *iin,
			   size_t *sector_size,
			   size_t *num_sectors);

/* read num_sectors starting from start_sector in an internal buffer;
   number of sectors read = *length / IIN_SECTOR_SIZE */
typedef int (*iin_read_t) (iin_t *iin,
			   size_t start_sector,
			   size_t num_sectors,
			   const char **data,
			   size_t *length);

/* return last error text in a memory buffer, that would be freed by calling iin_dispose_error_t */
typedef char* (*iin_last_error_t) (iin_t *iin);
typedef void (*iin_dispose_error_t) (iin_t *iin,
				     char* error);

/* iin should not be used after close */
typedef int (*iin_close_t) (iin_t *iin);

struct iin_type
{
  iin_stat_t stat;
  iin_read_t read;
  iin_close_t close;
  iin_last_error_t last_error;
  iin_dispose_error_t dispose_error;
  char source_type [36];
};


int iin_probe (const char *path,
	       iin_t **iin);


#endif /* _IIN_H defined? */
