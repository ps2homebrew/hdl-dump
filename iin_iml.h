/*
 * iin_iml.h
 * $Id: iin_iml.h,v 1.5 2005/07/10 21:06:48 bobi Exp $
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

#if !defined (_IIN_IML_H)
#define _IIN_IML_H

#include "config.h"
#include "iin.h"

C_START

/* would accept IML file */
int iin_iml_probe_path (const dict_t *config,
			const char *path,
			iin_t **iin);

C_END

#endif /* _IIN_IML_H defined? */
