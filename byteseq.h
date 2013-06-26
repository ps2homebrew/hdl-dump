/*
 * byteseq.h
 * $Id: byteseq.h,v 1.4 2006/09/01 17:32:03 bobi Exp $
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

#if !defined (_BYTESEQ_H)
#define _BYTESEQ_H

#include "config.h"

C_START

u_int32_t get_u32 (const void *buffer);
void set_u32 (/*@out@*/ void *buffer, u_int32_t val);

u_int16_t get_u16 (const void *buffer);
void set_u16 (/*@out@*/ void *buffer, u_int16_t val);

u_int8_t get_u8 (const void *buffer);
void set_u8 (/*@out@*/ void *buffer, u_int8_t val);

C_END

#endif /* _BYTESEQ_H defined? */
