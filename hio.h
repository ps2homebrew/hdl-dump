/*
 * hio.h - PS2 HDD I/O
 * $Id: hio.h,v 1.9 2006/09/01 17:27:43 bobi Exp $
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

#if !defined(_HIO_H)
#define _HIO_H

#include "config.h"
#include <stddef.h>
#include "dict.h"

C_START

/*
 * HD Loader I/O interface below
 */

typedef struct hio_type hio_t;
typedef /*@only@*/ /*@out@*/ /*@null@*/ hio_t *hio_p_t;


typedef int (*hio_stat_t)(hio_t *hio,
                          /*@out@*/ u_int32_t *size_in_kb);

typedef int (*hio_read_t)(hio_t *hio,
                          u_int32_t start_sector,
                          u_int32_t num_sectors,
                          /*@out@*/ void *output,
                          /*@out@*/ u_int32_t *bytes);

typedef int (*hio_write_t)(hio_t *hio,
                           u_int32_t start_sector,
                           u_int32_t num_sectors,
                           const void *input,
                           /*@out@*/ u_int32_t *bytes);

typedef int (*hio_flush_t)(hio_t *hio);

typedef int (*hio_close_t)(/*@special@*/ /*@only@*/ hio_t *hio) /*@releases hio@*/;

/* return last error text in a memory buffer, that would be freed by calling hio_dispose_error_t */
typedef /*@only@*/ char *(*hio_last_error_t)(hio_t *hio);
typedef void (*hio_dispose_error_t)(hio_t *hio,
                                    /*@only@*/ char *error);


struct hio_type
{
    hio_stat_t stat;
    hio_read_t read;
    hio_write_t write;
    hio_flush_t flush;
    hio_close_t close;
    hio_last_error_t last_error;
    hio_dispose_error_t dispose_error;
};


int hio_probe(const dict_t *config,
              const char *path,
              /*@special@*/ hio_p_t *hio) /*@allocates *hio@*/ /*@defines *hio@*/;

C_END

#endif /* _HIO_H defined? */
