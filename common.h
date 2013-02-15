/*
 * common.h
 * $Id: common.h,v 1.14 2006/06/18 13:08:26 bobi Exp $
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

#include "config.h"
#include "dict.h"
#include "osal.h"
#include "progress.h"
#include "iin.h"
#include "hio.h"
#include "hdl.h"

C_START

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
	       u_int64_t bytes,
	       u_int32_t buff_size,
	       progress_t *pgs);

/* data buffer is zero-terminated */
int read_file (const char *file_name,
	       char **data,
	       u_int32_t *len);

int write_file (const char *file_name,
		const void *data,
		u_int32_t len);

int dump_device (const char *device_name,
		 const char *output_file,
		 u_int64_t max_size,
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
	      u_int32_t start_sector,
	      u_int32_t num_sectors,
	      progress_t *pgs);

int iin_copy_ex (iin_t *iin,
		 hio_t *hio,
		 u_int32_t input_start_sector,
		 u_int32_t output_start_sector,
		 u_int32_t num_sectors,
		 progress_t *pgs);

const char *get_config_file (void);
void set_config_defaults (dict_t *config);

compat_flags_t parse_compat_flags (const char *flags);

int ddb_lookup (const dict_t *config,
		const char *startup,
		char name[HDL_GAME_NAME_MAX + 1],
		compat_flags_t *flags);

int ddb_update (const dict_t *config,
		const char *startup,
		const char *name,
		compat_flags_t flags);

C_END

#endif /* _COMMON_H defined? */
