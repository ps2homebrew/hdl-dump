/*
 * iin_probe.c
 * $Id: iin_probe.c,v 1.9 2006/09/01 17:24:01 bobi Exp $
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

#include "iin.h"
#include "iin_optical.h"
#include "iin_spti.h"
#include "iin_aspi.h"
#include "iin_nero.h"
#include "iin_cdrwin.h"
#include "iin_gi.h"
#include "iin_iso.h"
#include "iin_iml.h"
#include "iin_hio.h"
#include "dict.h"
#include "config.h"
#include "retcodes.h"
#if defined(USE_THREADED_IIN)
#include "thd_iin.h"
#endif


int iin_probe(const dict_t *config,
              const char *path,
              iin_t **iin)
{
    int result = RET_NOT_COMPAT;

    /* prefix-driven inputs first */
    if (result == RET_NOT_COMPAT)
        result = iin_hio_probe_path(config, path, iin);
    if (result == RET_NOT_COMPAT)
        result = iin_optical_probe_path(path, iin);
#if defined(_BUILD_WIN32)
    if (result == RET_NOT_COMPAT)
        result = iin_spti_probe_path(path, iin);
    /* assume ASPI support enabled */
    if (result == RET_NOT_COMPAT)
        result = iin_aspi_probe_path(path, iin);
#endif

    /* file-driven inputs next, ordered by accuracy */
    if (result == RET_NOT_COMPAT)
        result = iin_nero_probe_path(path, iin);
    if (result == RET_NOT_COMPAT)
        result = iin_cdrwin_probe_path(path, iin);
    if (result == RET_NOT_COMPAT)
        result = iin_gi_probe_path(path, iin);
    if (result == RET_NOT_COMPAT)
        result = iin_iso_probe_path(path, iin);
    if (result == RET_NOT_COMPAT)
        result = iin_iml_probe_path(path, iin);

#if defined(USE_THREADED_IIN)
    if (result == RET_OK) { /* wrap in threaded delegate */
        iin_t *tmp = thd_create(*iin);
        if (tmp != NULL)
            *iin = tmp;
    }
#endif /* _BUILD_WIN32 defined? */

    return (result);
}
