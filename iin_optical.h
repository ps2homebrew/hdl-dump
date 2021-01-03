/*
 * iin_optical.h
 * $Id: iin_optical.h,v 1.7 2006/09/01 17:24:08 bobi Exp $
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

#if !defined(_IIN_OPTICAL_H)
#define _IIN_OPTICAL_H

#include "config.h"
#include "iin.h"

C_START

/* would accept "cd*:" */
int iin_optical_probe_path(const char *path,
                           /*@special@*/ iin_p_t *iin) /*@allocates *iin@*/ /*@defines *iin@*/;

C_END

#endif /* _IIN_OPTICAL_H defined? */
