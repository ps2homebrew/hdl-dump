/*
 * iin_img_base.h
 * $Id: iin_img_base.h,v 1.6 2005/07/10 21:06:48 bobi Exp $
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

#if !defined (_IIN_IMG_BASE_H)
#define _IIN_IMG_BASE_H

#include "config.h"
#include "iin.h"
#include "osal.h"

C_START

typedef struct iin_img_base_type iin_img_base_t;

iin_img_base_t*
img_base_alloc (u_int32_t raw_sector_size,
		u_int32_t raw_skip_offset);

/* input is guaranteed to be upsized to one sector only (that is if input size is not
   aligned on sector size, the rest up to sector size would be zero-filled) */
int img_base_add_part (iin_img_base_t *img_base,
		       const char *input_path,
		       u_int32_t length_s, /* input length in sectors */
		       u_int64_t skip,   /* bytes to skip in the begining of the input */
		       u_int32_t device_sector_size); /* to align reads on */

/* gap is only allowed IN THE BEGINING OR BETWEEN files and cannot exist behind the last file */
void img_base_add_gap (iin_img_base_t *img_base,
		       u_int32_t length_s);

C_END

#endif /* _IIN_IMG_BASE_H defined? */
