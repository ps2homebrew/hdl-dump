/*
 * hio_probe.c
 * $Id: hio_probe.c,v 1.8 2007-05-12 20:16:14 bobi Exp $
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

#include "hio_win32.h"
#include "hio_udpnet2.h"
#include "hio_dbg.h"
#include "hio_trace.h"
#include "retcodes.h"


/**************************************************************/
int hio_probe(const dict_t *config,
              const char *path,
              hio_t **hio)
{
    int result = RET_NOT_COMPAT;
    if (result == RET_NOT_COMPAT)
        result = hio_trace_probe(config, path, hio);
    if (result == RET_NOT_COMPAT)
        result = hio_dbg_probe(config, path, hio);
    if (result == RET_NOT_COMPAT)
        result = hio_udpnet2_probe(config, path, hio);
    if (result == RET_NOT_COMPAT)
        result = hio_win32_probe(config, path, hio);
    return (result);
}
