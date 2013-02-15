/*
 * hio_udpnet.h - TCP/IP+UDP networking access to PS2 HDD
 * $Id: hio_udpnet.h,v 1.1 2005/12/08 20:46:02 bobi Exp $
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

#if !defined (_HIO_UDPNET_H)
#define _HIO_UDPNET_H

#include "config.h"
#include "hio.h"

C_START

/* accepts paths of the following form: "udp:a.b.c.d",
   where a.b.c.d is a valid IP address */
int hio_udpnet_probe (const dict_t *config,
		      const char *path,
		      hio_t **hio);

C_END

#endif /* _HIO_UDPNET_H defined? */
