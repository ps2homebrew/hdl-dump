/*
 * aligned.h
 * $Id: aligned.h,v 1.4 2005/07/10 21:06:48 bobi Exp $
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

#if !defined (_ALIGNED_H)
#define _ALIGNED_H

#include "config.h"
#include "osal.h"

C_START

/*
 * Windowz: when caching is turned off, all file I/O should start on a multiple of sector size
 * (typically 512 bytes for HDD and 2048 bytes for an optical drive)
 * and the number of bytes involved should also be multiple of sector size;
 * aaahhh... and the buffer should be aligned to a multiple of sector size;
 * would read up to sector_size * buffer_size_in_sectors bytes at once
 */
typedef struct aligned_type aligned_t;

aligned_t*
al_alloc (osal_handle_t in,
	  u_int32_t sector_size, /* device sector size - each file I/O should be aligned */
	  u_int32_t buffer_size_in_sectors); /* 32 is good enough */

int
al_read (aligned_t *al,
	 u_int64_t offset,
	 const char **data, /* internal buffer, managed by aligned I/O */
	 u_int32_t dest_size,
	 u_int32_t *length);

void
al_free (aligned_t *al);

C_END

#endif /* _ALIGNED_H defined? */
