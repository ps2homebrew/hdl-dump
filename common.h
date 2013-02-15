/*
 * common.h
 * $Id: common.h,v 1.10 2004/09/26 19:39:39 b081 Exp $
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

#if !defined (_COMMON_H)
#define _COMMON_H

#include "osal.h"
#include "progress.h"
#include "iin.h"
#include "hio.h"

#if !defined (MAX_PATH)
#  define MAX_PATH 128
#endif


char* ltrim (char *text);

char* rtrim (char *text);

/* nonzero if same, zero if different */
int caseless_compare (const char *s1,
		      const char *s2);

/* would copy until EOF if bytes == 0 */
int copy_data (osal_handle_t in,
	       osal_handle_t out,
	       bigint_t bytes,
	       size_t buff_size,
	       progress_t *pgs);

/* data buffer is zero-terminated */
int read_file (const char *file_name,
	       char **data,
	       size_t *len);

int write_file (const char *file_name,
		const void *data,
		size_t len);

int dump_device (const char *device_name,
		 const char *output_file,
		 bigint_t max_size,
		 progress_t *pgs);

/* nonzero if file can be opened for reading, zero if none or on error */
int file_exists (const char *path);

/* checks whether original_file exists; if not, checks for the same filename,
   in the directory of secondary file; returns RET_OK on success;
   modifies original_file if necessary */
int lookup_file (char original_file [MAX_PATH],
		 const char *secondary_file);

int iin_copy (iin_t *iin,
	      osal_handle_t out,
	      size_t start_sector,
	      size_t num_sectors,
	      progress_t *pgs);

int iin_copy_ex (iin_t *iin,
		 hio_t *hio,
		 size_t input_start_sector,
		 size_t output_start_sector,
		 size_t num_sectors,
		 progress_t *pgs);

#endif /* _COMMON_H defined? */
