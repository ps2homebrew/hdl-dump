/*
 * isofs.h
 * $Id: isofs.h,v 1.6 2005/07/10 21:06:48 bobi Exp $
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

#if !defined (_ISOFS_H)
#define _ISOFS_H

#include "config.h"
#include "iin.h"

C_START

int isofs_get_ps_cdvd_details (iin_t *iin,
			       char volume_id [32 + 1],
			       char signature [12 + 1],
			       u_int64_t *layer_pvd);

C_END

#endif /* _ISOFS_H defined? */
