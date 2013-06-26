/*
 * byteseq.c
 * $Id: byteseq.c,v 1.2 2004/12/04 10:20:53 b081 Exp $
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

#include "byteseq.h"


/**************************************************************/
u_int32_t
get_u32 (const void *buffer)
{
  const u_int8_t *p = buffer;
  return ((u_int32_t) p [3] << 24 |
	  (u_int32_t) p [2] << 16 |
	  (u_int32_t) p [1] <<  8 |
	  (u_int32_t) p [0] <<  0);
}


/**************************************************************/
void
set_u32 (void *buffer,
	 u_int32_t val)
{
  u_int8_t *p = buffer;
  p [3] = (u_int8_t) (val >> 24);
  p [2] = (u_int8_t) (val >> 16);
  p [1] = (u_int8_t) (val >>  8);
  p [0] = (u_int8_t) (val >>  0);
}


/**************************************************************/
u_int16_t
get_u16 (const void *buffer)
{
  const u_int8_t *p = buffer;
  return ((u_int16_t) p [1] << 8 |
	  (u_int16_t) p [0] << 0);
}


/**************************************************************/
void
set_u16 (void *buffer,
	 u_int16_t val)
{
  u_int8_t *p = buffer;
  p [1] = (u_int8_t) (val >> 8);
  p [0] = (u_int8_t) (val >> 0);
}

/**************************************************************/
u_int8_t
get_u8 (const void *buffer)
{
  const u_int8_t *p = buffer;
  return ((u_int8_t) p [0] << 0);
}


/**************************************************************/
void
set_u8 (void *buffer,
	 u_int8_t val)
{
  u_int8_t *p = buffer;
  p [0] = (u_int8_t) (val >> 0);
}
