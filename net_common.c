/*
 * net_common.c
 * $Id: net_common.c,v 1.2 2006/09/01 17:22:24 bobi Exp $
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

#if defined (_BUILD_WIN32)
#  if defined (_MSC_VER) && defined (_WIN32)
#    include <winsock2.h> /* Microsoft Visual C/C++ compiler */
#  else
#    include <winsock.h> /* GNU C/C++ compiler */
#  endif
#  include <windows.h>
#elif defined (_BUILD_UNIX)
#  include <errno.h>
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#endif
#include <string.h>
#include "byteseq.h"
#include "net_common.h"
#include "retcodes.h"


/**************************************************************/
int
recv_exact (int s,
	    void *buf,
	    u_int32_t bytes,
	    int flags)
{
  ssize_t total = 0, result = 1;
  while (bytes > 0 && result > 0)
    {
      result = recv (s, buf, bytes, flags);
      if (result > 0)
	{
	  buf = (char*) buf + result;
	  total += result;
	  bytes -= result;
	}
    }
  return ((int) (result >= 0 ? total : result));
}


/**************************************************************/
int
send_exact (int s,
	    const void *buf,
	    u_int32_t bytes,
	    int flags)
{
  ssize_t total = 0, result = 1;
  while (bytes > 0 && result > 0)
    {
      result = send (s, buf, bytes, flags);
      if (result > 0)
	{
	  buf = (char*) buf + result;
	  total += result;
	  bytes -= result;
	}
    }
  return ((int) (result >= 0 ? total : result));
}
