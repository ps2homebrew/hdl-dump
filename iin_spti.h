/*
 * iin_spti.h
 * $Id: iin_spti.h,v 1.1 2006/09/01 17:37:58 bobi Exp $
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

#if !defined (_IIN_SPTI_H)
#define _IIN_SPTI_H

#include "config.h"
#include "iin.h"

C_START

unsigned long spti_get_last_error_code (void);

const char* spti_get_last_error_msg (void);

const char* spti_get_error_msg (unsigned long spti_error_code);

/* would accept a drive letter ("d:", "e:",...) of an optical drive */
int iin_spti_probe_path (const char *path,
			 /*@special@*/ iin_p_t *iin) /*@allocates *iin@*/ /*@defines *iin@*/;

C_END

#endif /* _IIN_OPTICAL_H defined? */
