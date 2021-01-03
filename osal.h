/*
 * osal.h
 * $Id: osal.h,v 1.13 2006/09/01 17:21:57 bobi Exp $
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

#if !defined(_OSAL_H)
#define _OSAL_H

#include "config.h"

C_START

#if defined(_BUILD_WIN32)
#include <windows.h>

#elif defined(_BUILD_UNIX)
/* NULL */

#else
#error Unsupported platform; Please, modify Makefile
#endif


#include "retcodes.h"
#define OSAL_OK RET_OK
#define OSAL_ERR RET_ERR
#define OSAL_NO_MEM RET_NO_MEM

#define _MB *(1024 * 1024) /* really ugly :-) */


#if defined(_BUILD_WIN32)
typedef HANDLE osal_handle_t;
#define OSAL_HANDLE_INIT 0
#define OSAL_IS_OPENED(x) ((x) != OSAL_HANDLE_INIT)

#elif defined(_BUILD_UNIX)

typedef struct
{
    int desc; /* file descriptor */
} osal_handle_t;

#define OSAL_HANDLE_INIT \
    {                    \
        -1               \
    } /* file descriptor */
#define OSAL_IS_OPENED(x) ((x).desc != -1)

/* This needs to be at least 256 bytes -- see iin_gi_probe_path */
#define MAX_PATH 1024

#endif
typedef /*@special@*/ /*@only@*/ /*@out@*/ osal_handle_t *osal_handle_p_t;


/* pointer should be passed to osal_dispose_error_msg when no longer needed */
unsigned long osal_get_last_error_code(void);
/*@special@*/ /*@only@*/ char *osal_get_last_error_msg(void) /*@allocates result@*/ /*@defines result@*/;
/*@special@*/ /*@only@*/ char *osal_get_error_msg(unsigned long error) /*@allocates result@*/ /*@defines result@*/;
void osal_dispose_error_msg(/*@special@*/ /*@only@*/ char *msg) /*@releases msg@*/;


int osal_open(const char *name,
              /*@special@*/ osal_handle_p_t handle,
              int no_cache) /*@allocates handle@*/ /*@defines *handle@*/;

int osal_open_device_for_writing(const char *device_name,
                                 /*@special@*/ osal_handle_p_t handle) /*@allocates handle@*/ /*@defines *handle@*/;

int osal_create_file(const char *path,
                     /*@special@*/ osal_handle_p_t handle,
                     u_int64_t estimated_size) /*@allocates handle@*/ /*@defines *handle@*/;

int osal_get_estimated_device_size(osal_handle_t handle,
                                   /*@out@*/ u_int64_t *size_in_bytes);

int osal_get_device_size(osal_handle_t handle,
                         /*@out@*/ u_int64_t *size_in_bytes);

int osal_get_device_sect_size(osal_handle_t handle,
                              /*@out@*/ u_int32_t *size_in_bytes);

int osal_get_volume_sect_size(const char *volume_root,
                              /*@out@*/ u_int32_t *size_in_bytes);

int osal_get_file_size_ex(const char *path,
                          /*@out@*/ u_int64_t *size_in_bytes);

int osal_get_file_size(osal_handle_t handle,
                       /*@out@*/ u_int64_t *size_in_bytes);

int osal_seek(osal_handle_t handle,
              u_int64_t abs_pos);

int osal_read(osal_handle_t handle,
              /*@out@*/ void *out,
              u_int32_t bytes,
              /*@out@*/ u_int32_t *stored);

int osal_write(osal_handle_t handle,
               const void *in,
               u_int32_t bytes,
               /*@out@*/ u_int32_t *stored);

int osal_close(/*@special@*/ /*@only@*/ osal_handle_t *handle) /*@releases handle@*/;


/*@special@*/ /*@only@*/ /*@null@*/ void *osal_alloc(u_int32_t bytes) /*@allocates result@*/;
void osal_free(/*@special@*/ /*@only@*/ void *ptr) /*@releases ptr@*/;


/* support for memory-mapped files/devices */
typedef struct osal_mmap_type osal_mmap_t;
typedef /*@only@*/ /*@out@*/ /*@null@*/ osal_mmap_t *osal_mmap_p_t;
int osal_mmap(/*@special@*/ osal_mmap_p_t *mm,
              /*@out@*/ void **p,
              osal_handle_t handle,
              u_int64_t offset,
              u_int32_t length) /*@allocates *mm@*/ /*@defines *mm@*/;

int osal_munmap(/*@special@*/ /*@only@*/ osal_mmap_t *mm) /*@releases *mm@*/;


#define DEV_MAX_NAME_LEN 16

typedef struct osal_dev_type /* device */
{
    char name[DEV_MAX_NAME_LEN];
    u_int64_t capacity; /* -1 if not ready */
    int is_ps2;
    unsigned long status;
} osal_dev_t;

typedef struct osal_dlist_type /* devices list */
{
    u_int32_t allocated, used;
    osal_dev_t *device;
} osal_dlist_t;
typedef /*@only@*/ /*@out@*/ /*@null@*/ osal_dlist_t *osal_dlist_p_t;


int osal_query_hard_drives(/*@special@*/ osal_dlist_p_t *hard_drives) /*@allocates *hard_drives@*/ /*@defines *hard_drives@*/;
int osal_query_optical_drives(/*@special@*/ osal_dlist_p_t *optical_drives) /*@allocates *optical_drives@*/ /*@defines *optical_drives@*/;
int osal_query_devices(/*@special@*/ osal_dlist_p_t *hard_drives,
                       /*@special@*/ osal_dlist_p_t *optical_drives) /*@allocates *hard_drives,*optical_drives@*/ /*@defines *hard_drives,*optical_drives@*/;
void osal_dlist_free(/*@special@*/ /*@only@*/ osal_dlist_t *dlist) /*@releases dlist@*/;

int /* RET_OK, RET_BAD_FORMAT, RET_BAD_DEVICE */
osal_map_device_name(const char *input,
                     /*@out@*/ char output[MAX_PATH]);

C_END

#endif /* _OSAL_H defined? */
