/*
 * aligned.c
 * $Id: aligned.c,v 1.8 2006/09/01 17:18:31 bobi Exp $
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
#include <stdlib.h>
#include <string.h>

#include "aligned.h"


struct aligned_type
{
    osal_handle_t in;
    char /*@owned@*/ *unaligned;
    char /*@dependent@*/ *buffer;
    u_int32_t sector_size, buffer_size, data_length;
    u_int64_t offset;
};


/**************************************************************/
aligned_t *
al_alloc(osal_handle_t in,
         u_int32_t sector_size,
         u_int32_t buffer_size_in_sectors)
{
    aligned_t *al = osal_alloc(sizeof(aligned_t));
    if (al != NULL) {
        memset(al, 0, sizeof(aligned_t));
        al->buffer_size = sector_size * buffer_size_in_sectors;
        al->unaligned = osal_alloc(al->buffer_size + sector_size);
        if (al->unaligned != NULL) {
            al->in = in;
            /* FIX: 64-bit OS compatibility (not to trim ptr to 32-bit) */
            al->buffer = (void *)(((unsigned long)al->unaligned + sector_size - 1) & ~((unsigned long)sector_size - 1));
            assert(al->buffer >= al->unaligned);
            al->sector_size = sector_size;
            al->offset = (u_int64_t)-1;
        } else {
            osal_free(al);
            al = NULL;
        }
    }
    return (al);
}


/**************************************************************/
void al_free(aligned_t *al)
{
    if (al != NULL) {
        osal_free(al->unaligned);
        osal_free(al);
    }
}


/**************************************************************/
int al_read(aligned_t *al,
            u_int64_t offset,
            const char **data,
            u_int32_t bytes,
            u_int32_t *data_length)
{
    int result = OSAL_OK;

    /* check if buffer contains all requested data */
    if (al->offset <= offset && offset + bytes <= al->offset + al->data_length) { /* return data from the buffer */
        *data = al->buffer + (offset - al->offset);
        *data_length = bytes;
    } else { /* buffer does not contains all or any of the requested data */
        u_int64_t aligned_offset = offset & ~((u_int64_t)al->sector_size - 1);
        u_int32_t correction = 0;
        assert(aligned_offset <= offset);

        /* check whether cache contains some usable data */
        if (al->offset <= aligned_offset &&
            aligned_offset < al->offset + al->data_length) { /* arrange data inside the cache and correct the offset */
            u_int32_t usable_data_offs = (u_int32_t)(aligned_offset - al->offset);
            u_int32_t usable_data_len = (u_int32_t)(al->offset + al->data_length - aligned_offset);
            memmove(al->buffer, al->buffer + usable_data_offs, usable_data_len);
            correction = usable_data_len;
        }

        result = osal_seek(al->in, aligned_offset + correction);
        if (result == OSAL_OK) {
            result = osal_read(al->in, al->buffer + correction,
                               al->buffer_size - correction, &al->data_length);
            al->data_length += correction;
#if 0
	  printf ("osal_read (%d, %luKB, %d, -> %d) = %d\n",
		  al->in, (long) (aligned_offset / 1024), al->buffer_size, al->data_length, result);
#endif
        }
        if (result == OSAL_OK) { /* success */
            u_int32_t skip = (u_int32_t)(offset - aligned_offset);
            al->offset = aligned_offset;
            *data = al->buffer + skip;
            *data_length = bytes > al->data_length - skip ? al->data_length - skip : bytes;
        }
    }
    return (result);
}
