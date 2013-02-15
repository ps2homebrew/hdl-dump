/*
 * osal.h
 * $Id: osal.h,v 1.11 2005/07/10 21:06:48 bobi Exp $
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

#if !defined (_OSAL_H)
#define _OSAL_H

#include "config.h"

C_START

#if defined (_BUILD_WIN32)
#  include <windows.h>

#elif defined (_BUILD_UNIX)
/* NULL */

#else
#  error Unsupported platform; Please, modify Makefile
#endif


#define OSAL_OK       0
#define OSAL_ERR     -1
#define OSAL_NO_MEM  -2

#define _MB * (1024 * 1024) /* really ugly :-) */


#if defined (_BUILD_WIN32)
typedef HANDLE osal_handle_t;
#  define OSAL_HANDLE_INIT 0
#  define OSAL_IS_OPENED(x) ((x) != OSAL_HANDLE_INIT)

#elif defined (_BUILD_UNIX)

typedef struct
{
  int desc; /* file descriptor */
} osal_handle_t;

#define OSAL_HANDLE_INIT { -1 } /* file descriptor */
#define OSAL_IS_OPENED(x) ((x).desc != -1)

#define MAX_PATH 256

#endif


/* pointer should be passed to osal_dispose_error_msg when no longer needed */
unsigned long osal_get_last_error_code (void);
char* osal_get_last_error_msg (void);
char* osal_get_error_msg (unsigned long error);
void osal_dispose_error_msg (char *msg);


int osal_open (const char *name,
	       osal_handle_t *handle,
	       int no_cache);

int osal_open_device_for_writing (const char *device_name,
				  osal_handle_t *handle);

int osal_create_file (const char *path,
		      osal_handle_t *handle,
		      u_int64_t estimated_size);

int osal_get_estimated_device_size (osal_handle_t handle,
				    u_int64_t *size_in_bytes);

int osal_get_device_size (osal_handle_t handle,
			  u_int64_t *size_in_bytes);

int osal_get_device_sect_size (osal_handle_t handle,
			       u_int32_t *size_in_bytes);

int osal_get_volume_sect_size (const char *volume_root,
			       u_int32_t *size_in_bytes);

int osal_get_file_size_ex (const char *path,
			   u_int64_t *size_in_bytes);

int osal_get_file_size (osal_handle_t handle,
			u_int64_t *size_in_bytes);

int osal_seek (osal_handle_t handle,
	       u_int64_t abs_pos);

int osal_read (osal_handle_t handle,
	       void *out,
	       u_int32_t bytes,
	       u_int32_t *stored);

int osal_write (osal_handle_t handle,
		const void *in,
		u_int32_t bytes,
		u_int32_t *stored);

int osal_close (osal_handle_t handle);


void* osal_alloc (u_int32_t bytes);
void osal_free (void *ptr);


#define DEV_MAX_NAME_LEN       16

typedef struct osal_dev_type /* device */
{
  char name [DEV_MAX_NAME_LEN];
  u_int64_t capacity; /* -1 if not ready */
  int is_ps2;
  unsigned long status;
} osal_dev_t;

typedef struct osal_dlist_type /* devices list */
{
  u_int32_t allocated, used;
  osal_dev_t *device;
} osal_dlist_t;


int osal_query_hard_drives (osal_dlist_t **hard_drives);
int osal_query_optical_drives (osal_dlist_t **optical_drives);
int osal_query_devices (osal_dlist_t **hard_drives,
			osal_dlist_t **optical_drives);
void osal_dlist_free (osal_dlist_t *dlist);

int /* RET_OK, RET_BAD_FORMAT, RET_BAD_DEVICE */
osal_map_device_name (const char *input,
		      char output [MAX_PATH]);

C_END

#endif /* _OSAL_H defined? */
