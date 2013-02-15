/*
 * hdld_svr_iop.c
 * $Id: hdld_svr_iop.c,v 1.4 2004/09/26 19:39:40 b081 Exp $
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

#include <tamtypes.h>
#include <stdio.h>
#include <thbase.h>
#include <thsemap.h>
#include <ps2ip.h>

#include "irx_imports.h"
#include "net_io.h"
#include "retcodes.h"
#include "hio_iop.h"


#define MODULE_NAME "HDLD_SVR.IRX"
#define UNIT_PATH "hdd0:"

volatile int interrupt_flag = 0;


int run_server (unsigned short port,
		hio_t *hio,
		volatile int *interrupt_flag);

/**************************************************************/
void
serve (void *arg)
{
  hio_t *hio;
  int result = hio_iop_probe (UNIT_PATH, &hio);
  if (result == RET_OK)
    {
      printf (MODULE_NAME ": starting\n");
      result = run_server (NET_HIO_SERVER_PORT, hio, &interrupt_flag);
      printf (MODULE_NAME ": stopped\n");
    }
  else
    printf (MODULE_NAME ": unable to mount " UNIT_PATH "\n");
}


/**************************************************************/
int
_start (int argc,
	char **argv)
{
  iop_thread_t t;
  int tid;

  sceSifInitRpc (0);

  t.attr = 0x02000000; /* TH_C */
  t.option = 0;
  t.thread = serve;
  t.stacksize = 0x1000;
  t.priority = 0x20; /* 0x09; */
  tid = CreateThread (&t);
  if (tid >= 0)
    StartThread (tid, NULL);
  else
    printf (MODULE_NAME ": StartThread failed: %d\n", tid);

  return 0;
}
