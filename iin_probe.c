/*
 * iin_probe.c
 * $Id: iin_probe.c,v 1.5 2004/09/12 17:25:27 b081 Exp $
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
#include "iin_aspi.h"
#include "iin_hdloader.h"
#include "iin_net.h"
#include "iin_nero.h"
#include "iin_cdrwin.h"
#include "iin_gi.h"
#include "iin_iso.h"
#include "iin_iml.h"
#include "retcodes.h"


int
iin_probe (const char *path,
	   iin_t **iin)
{
  int result = RET_NOT_COMPAT;

  /* prefix-driven inputs first */
  if (result == RET_NOT_COMPAT)
    result = iin_optical_probe_path (path, iin);
#if defined (_BUILD_WIN32) && defined (_WITH_ASPI)
  if (result == RET_NOT_COMPAT)
    result = iin_aspi_probe_path (path, iin);
#endif
  if (result == RET_NOT_COMPAT)
    result = iin_hdloader_probe_path (path, iin);
  if (result == RET_NOT_COMPAT)
    result = iin_net_probe_path (path, iin);

  /* file-driven inputs next, ordered by accuracy */
  if (result == RET_NOT_COMPAT)
    result = iin_nero_probe_path (path, iin);
  if (result == RET_NOT_COMPAT)
    result = iin_cdrwin_probe_path (path, iin);
  if (result == RET_NOT_COMPAT)
    result = iin_gi_probe_path (path, iin);
  if (result == RET_NOT_COMPAT)
    result = iin_iso_probe_path (path, iin);
  if (result == RET_NOT_COMPAT)
    result = iin_iml_probe_path (path, iin);
  return (result);
}
