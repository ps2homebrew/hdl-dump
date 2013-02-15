/*
 * hio_win32.h - Win32 interface to locally connected PS2 HDD
 * $Id: hio_win32.h,v 1.6 2006/09/01 17:26:29 bobi Exp $
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

#if !defined (_HIO_WIN32_H)
#define _HIO_WIN32_H

#include "config.h"
#include "hio.h"
#include "dict.h"

C_START

/* accepts paths in the following form:
   "hdd?:" or "hdd??:" where ? is 0-based device index */
int hio_win32_probe (const dict_t *config,
		     const char *path,
		     /*@special@*/ hio_p_t *hio) /*@allocates *hio@*/ /*@defines *hio@*/;

C_END

#endif /* _HIO_WIN32_H defined? */
