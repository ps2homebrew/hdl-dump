/*
 * hdl.h
 * $Id: hdl.h,v 1.3 2004/08/15 16:44:19 b081 Exp $
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

#if !defined (_HDL_H)
#define _HDL_H

#include "progress.h"

int hdl_extract (const char *device_name,
		 const char *game_name,
		 const char *output_file,
		 progress_t *pgs);

int hdl_inject (const char *device_name,
		const char *game_name,
		const char *game_signature,
		const char *input_path, /* iin_* probed */
		int input_is_dvd,
		progress_t *pgs);

#endif /* _HDL_H defined? */
