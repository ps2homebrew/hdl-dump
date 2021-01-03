/*
 * iin.h
 * $Id: iin.h,v 1.10 2006/09/01 17:26:23 bobi Exp $
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

#if !defined(_IIN_H)
#define _IIN_H

#include "config.h"
#include <stddef.h>
#include "dict.h"

C_START

#define IIN_SECTOR_SIZE 2048u /* CD/DVD sector size */
#define IIN_NUM_SECTORS 512u  /* number of sectors to read at once */


/*
 * ISO input interface below
 */

typedef struct iin_type iin_t;
typedef /*@only@*/ /*@out@*/ /*@null@*/ iin_t *iin_p_t;


typedef int (*iin_stat_t)(iin_t *iin,
                          /*@out@*/ u_int32_t *sector_size,
                          /*@out@*/ u_int32_t *num_sectors);

/* read num_sectors starting from start_sector in an internal buffer;
   number of sectors read = *length / IIN_SECTOR_SIZE */
typedef int (*iin_read_t)(iin_t *iin,
                          u_int32_t start_sector,
                          u_int32_t num_sectors,
                          /*@out@*/ const char **data,
                          /*@out@*/ u_int32_t *length);

/* return last error text in a memory buffer, that would be freed by calling iin_dispose_error_t */
typedef /*@only@*/ char *(*iin_last_error_t)(iin_t *iin);
typedef void (*iin_dispose_error_t)(iin_t *iin,
                                    /*@only@*/ char *error);

/* iin should not be used after close */
typedef int (*iin_close_t)(/*@special@*/ /*@only@*/ iin_t *iin) /*@releases iin@*/;

struct iin_type
{
    iin_stat_t stat;
    iin_read_t read;
    iin_close_t close;
    iin_last_error_t last_error;
    iin_dispose_error_t dispose_error;
    char source_type[36];
};

int iin_probe(const dict_t *dict,
              const char *path,
              /*@special@*/ iin_p_t *iin) /*@allocates *iin@*/ /*@defines *iin@*/;

C_END

#endif /* _IIN_H defined? */
