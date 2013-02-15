/*
 * svr/win32/hdld_svr_win32.c
 * $Id: hdld_svr_win32.c,v 1.2 2004/08/15 16:44:20 b081 Exp $
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

#include <winsock.h>
#include <signal.h>
#include <stdio.h>
#include "../net_server.h"
#include "../../net_io.h"
#include "../../hio_probe.h"
#include "../../retcodes.h"


static volatile int interrupt_flag = 0;


void
handle_int (int sig)
{
  /* TODO: I should read some more... */
  fprintf (stderr, "SIGINT...\n"); fflush (stderr);
  interrupt_flag = 1;
}


int
main (int argc,
      char *argv [])
{
  signal (SIGINT, handle_int);

  if (argc > 1)
    {
      hio_t *hio;
      int result = hio_probe (argv [1], &hio);
      if (result == RET_OK)
	{
	  WORD version = MAKEWORD (2, 2);
	  WSADATA wsa_data;
	  int result = WSAStartup (version, &wsa_data);
	  if (result == 0)
	    {
	      result = run_server (NET_HIO_SERVER_PORT, hio, &interrupt_flag);
	      if (result == 0)
		{
		  fprintf (stdout, "Server stopped.\n");
		}
	      else
		{
		  fprintf (stderr, "Unable to initialize.\n");
		}
	    }
	  else
	    {
	      fprintf (stderr, "Unable to initialize WinSock.\n");
	    }

	  hio->close (hio);
	}
      else
	{
	  fprintf (stderr, "%s: Unable to initialize HDD Input.\n", argv [1]);
	}
    }
  else
    {
      fprintf (stdout, "Usage: hdld_svr device_name\n");
    }

  return (0);
}
