/*
 * hio_udpnet2.c - TCP/IP networking access to PS2 HDD
 * $Id: hio_udpnet2.c,v 1.1 2007-05-12 20:16:32 bobi Exp $
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
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "osal.h"
#include "hio_udpnet.h"
#include "byteseq.h"
#include "net_common.h"
#include "retcodes.h"
#include "progress.h"
#include "net_io.h"
#include "svr/pktdrv/svr.h"
#include "svr/pktdrv/nettypes.h"


typedef struct hio_net_type
{
  hio_t hio;
  SOCKET udp;
  unsigned long error_code;
} hio_net_t;

#if defined (_BUILD_WIN32)
static int net_init = 0;
#endif

const nt_dword_t svr_magic = { 'A', 'o', 'E', '\0' };
static unsigned char seq_no = 0;

/* maybe those should go into the configuration file? */
static const size_t NET_RETRY_COUNT = 100;
static const size_t NET_SELECT_WAIT = 100; /* in milliseconds */
static const size_t NET_READ_OUTSTANDING_CHUNKS = 4;
static const size_t NET_WRITE_OUTSTANDING_CHUNKS = 4;

static int net_flush (hio_t *hio);

/**************************************************************/
static int
query_result (hio_net_t *net,
	      unsigned char command,
	      u_int32_t *retv)
{
  size_t count = 0;

  svr_packet packet;
  /*COPY_NT_DWORD (packet.magic, svr_magic);*/
  packet.magic [0] = svr_magic [0];
  packet.magic [1] = svr_magic [1];
  packet.magic [2] = svr_magic [2];
  packet.seq_no = (unsigned char) ++seq_no;
  packet.command = (unsigned char) command;
  packet.count = 0;
  SET_NT_DWORD (packet.start, 0);
  SET_NT_DWORD (packet.result, 0);

  do
    { /* send request */
      int result = send (net->udp, (const void*) &packet,
			 sizeof (packet) - 1, 0);
      struct timeval tv;
      fd_set fd;

      if (result == -1 || result != sizeof (packet) - 1)
	return (RET_ERR);

      /* await for results */
      FD_ZERO (&fd);
      FD_SET (net->udp, &fd);
      tv.tv_sec = 0;
      tv.tv_usec = NET_SELECT_WAIT * 1000;
      result = select (net->udp + 1, &fd, NULL, NULL, &tv);
      if (result == 1)
	{ /* got a reply */
	  unsigned char buf[1500];
	  int len = recv (net->udp, (void*) buf, sizeof (buf), 0);
	  if (len == sizeof (svr_packet) - 1)
	    {
	      const svr_packet *reply = (const svr_packet*) buf;
	      if (SVR_TEST_MAGIC (reply->magic) &&
		  reply->seq_no == packet.seq_no &&
		  reply->command == packet.command &&
		  reply->count == packet.count)
		{
		  *retv = GET_NT_DWORD (reply->result);
		  return (RET_OK);
		}
	    }
	  else if (len == -1)
	    return (RET_ERR);
	}
    }
  while (++count < 100);
  return (RET_TIMEOUT);
}


/**************************************************************/
static int
net_stat (hio_t *hio,
	  /*@out@*/ u_int32_t *size_in_kb)
{
  hio_net_t *net = (hio_net_t*) hio;
  u_int32_t retv = 0;
  int result = query_result (net, cmd_stat, &retv);
  if (result == RET_OK)
    *size_in_kb = retv / 2; /* retv is in sectors */
  return (result);
}


/**************************************************************/
/* process up to 32kB at a time */
static int
net_read_32k (hio_net_t *net,
	      u_int32_t start_sector,
	      u_int32_t num_sectors,
	      /*@out@*/ void *output,
	      /*@out@*/ u_int32_t *bytes)
{
  int result = 0;
  size_t count = 0, i;
  unsigned char *p = (unsigned char*) output;

  /* it is much quicker to queue several outstanding packets */
  /* build chunks map => divide output on # of chunks, 1KB each */
  struct rd_chunk
  {
    unsigned char *ptr;
    u_int32_t start;
    unsigned char count;
    short seq_no;
    short done;
  } chunk[32];
  size_t chunks_count = (num_sectors + 1) / 2, next_chunk = 0;
  size_t chunks_remaining = chunks_count;
  size_t chunks_outstanding = 0; /* sent and not ackd, yet */
  *bytes = num_sectors * 512;
  assert (num_sectors <= 32);
  assert (chunks_count <= sizeof (chunk) / sizeof (chunk[0]));
  for (i = 0; i < chunks_count; ++i)
    {
      chunk[i].ptr = p;
      chunk[i].start = start_sector;
      chunk[i].count = num_sectors >= 2 ? 2 : 1;
      chunk[i].seq_no = ++seq_no;
      chunk[i].done = 0;

      /* advance */
      p += 1024;
      start_sector += 2;
      num_sectors -= chunk[i].count;
    }
  assert (num_sectors == 0);

  count = 0;
  do
    {
      int result;
      struct timeval tv;
      fd_set fd;

      /* queue outstanding requests */
      while (chunks_outstanding < NET_READ_OUTSTANDING_CHUNKS &&
	     chunks_outstanding < chunks_remaining)
	{
	  struct rd_chunk *ck = NULL;
	  for (i = 0; i < chunks_count; ++i)
	    {
	      ck = &chunk[(next_chunk + i) % chunks_count];
	      if (!ck->done)
		{
		  next_chunk = (next_chunk + i + 1) % chunks_count;
		  break;
		}
	    }
	  if (ck && !ck->done)
	    { /* send request */
	      svr_packet packet;
		  /*COPY_NT_DWORD (packet.magic, svr_magic);*/
		  packet.magic [0] = svr_magic [0];
		  packet.magic [1] = svr_magic [1];
		  packet.magic [2] = svr_magic [2];
	      packet.seq_no = ck->seq_no;
	      SET_NT_DWORD (packet.start, ck->start);
	      SET_NT_DWORD (packet.result, 0);
	      packet.command = (unsigned char) cmd_read;
	      packet.count = ck->count;
	      result = send (net->udp, (const void*) &packet,
			     sizeof (svr_packet) - 1, 0);
	      if (result == -1 || result != sizeof (svr_packet) - 1)
		return (RET_ERR);
	      ++chunks_outstanding;
	    }
	}

      /* wait for results */
      FD_ZERO (&fd);
      FD_SET (net->udp, &fd);
      tv.tv_sec = 0;
      tv.tv_usec = NET_SELECT_WAIT * 1000;
      result = select (net->udp + 1, &fd, NULL, NULL, &tv);
      if (result == 1)
	{ /* got a reply */
	  unsigned char buf[1500];
	  int len = recv (net->udp, (void*) buf, sizeof (buf), 0);
	  if (len == sizeof (svr_packet) - 1 + 512 ||
	      len == sizeof (svr_packet) - 1 + 1024)
	    {
	      const svr_packet *reply = (const svr_packet*) buf;
	      if (SVR_TEST_MAGIC (reply->magic))
		{ /* try to locate matching request */
		  for (i = 0; i < chunks_count; ++i)
		    {
		      struct rd_chunk *ck = &chunk[i];
		      if (reply->seq_no == ck->seq_no &&
			  reply->command == cmd_read &&
			  GET_NT_DWORD (reply->start) == ck->start &&
			  reply->count == ck->count)
			{ /* got next 1 or 2 sectors */
			  if (GET_NT_DWORD (reply->result) != 0)
			    return (RET_PROTO_ERR); /* read failed? */

			  if (ck->done == 0)
			    { /* don't decrease counters twice! */
			      memcpy (ck->ptr, reply->data, ck->count * 512);
			      ck->done = 1;
			      --chunks_remaining;
			      --chunks_outstanding;
			    }
			  count = 0; /* reset */
			  break;
			}
		    }
		}
	    }
	  else if (len == -1)
	    return (RET_ERR);
	}
      else
	--chunks_outstanding; /* no ack; queue another request */
    }
  while (++count < NET_RETRY_COUNT && chunks_remaining > 0);

  if (chunks_remaining > 0)
    result = RET_TIMEOUT;

  return (result);
}


/**************************************************************/
static int
net_read (hio_t *hio,
	  u_int32_t start_sector,
	  u_int32_t num_sectors,
	  /*@out@*/ void *output,
	  /*@out@*/ u_int32_t *bytes)
{
  hio_net_t *net = (hio_net_t*) hio;
  int result = RET_OK;
  *bytes = 0;
  while (num_sectors > 0 && result == RET_OK)
    { /* divide request to 32kB-size chunks */
      u_int32_t count = (num_sectors > 32 ? 32 : num_sectors), n = 0;
      result = net_read_32k (net, start_sector, count, output, &n);
      if (result == RET_OK)
	{
	  start_sector += count;
	  num_sectors -= count;
	  output = (char*) output + count * 512;
	  *bytes += n;
	}
    }
  return (result);
}


/**************************************************************/
/* process up to 32kB at a time */
static int
net_write_32k (hio_net_t *net,
	       u_int32_t start_sector,
	       u_int32_t num_sectors,
	       const void *input,
	       /*@out@*/ u_int32_t *bytes)
{
  int result = 0;
  size_t count = 0, i;
  const unsigned char *p = (const unsigned char*) input;

  struct wr_chunk
  {
    const unsigned char *ptr;
    u_int32_t start;
    unsigned char count;
    short seq_no;
    short done;
  } chunk[32];
  size_t chunks_count = (num_sectors + 1) / 2, next_chunk = 0;
  size_t chunks_remaining = chunks_count;
  size_t chunks_outstanding = 0; /* sent and not ackd, yet */
  *bytes = num_sectors * 512;
  assert (num_sectors <= 32);
  assert (chunks_count <= sizeof (chunk) / sizeof (chunk[0]));
  for (i = 0; i < chunks_count; ++i)
    {
      chunk[i].ptr = p;
      chunk[i].start = start_sector;
      chunk[i].count = num_sectors >= 2 ? 2 : 1;
      chunk[i].seq_no = ++seq_no;
      chunk[i].done = 0;

      /* advance */
      p += 1024;
      start_sector += 2;
      num_sectors -= chunk[i].count;
    }
  assert (num_sectors == 0);

  count = 0;
  do
    {
      int result;
      struct timeval tv;
      fd_set fd;

      /* queue outstanding requests */
      while (chunks_outstanding < NET_WRITE_OUTSTANDING_CHUNKS &&
	     chunks_outstanding < chunks_remaining)
	{
	  struct wr_chunk *ck = NULL;
	  for (i = 0; i < chunks_count; ++i)
	    {
	      ck = &chunk[(next_chunk + i) % chunks_count];
	      if (!ck->done)
		{
		  next_chunk = (next_chunk + i + 1) % chunks_count;
		  break;
		}
	    }
	  if (ck && !ck->done)
	    { /* send request */
	      char buf[1500];
	      const size_t bytes = ck->count * 512;
	      svr_packet *packet = (svr_packet*) buf;
	      /*COPY_NT_DWORD (packet->magic, svr_magic);*/
		  packet->magic [0] = svr_magic [0];
		  packet->magic [1] = svr_magic [1];
		  packet->magic [2] = svr_magic [2];
	      packet->seq_no = ck->seq_no;
	      SET_NT_DWORD (packet->start, ck->start);
	      SET_NT_DWORD (packet->result, 0);
	      packet->command = (unsigned char) cmd_write;
	      packet->count = ck->count;
	      memcpy (packet->data, ck->ptr, ck->count * 512);
	      result = send (net->udp, (const void*) packet,
			     sizeof (svr_packet) - 1 + bytes, 0);
	      if (result == -1 || result != sizeof (svr_packet) - 1 + bytes)
		return (RET_ERR);
	      ++chunks_outstanding;
	    }
	}

      /* wait for results */
      FD_ZERO (&fd);
      FD_SET (net->udp, &fd);
      tv.tv_sec = 0;
      tv.tv_usec = NET_SELECT_WAIT * 1000;
      result = select (net->udp + 1, &fd, NULL, NULL, &tv);
      if (result == 1)
	{ /* got a reply */
	  unsigned char buf[1500];
	  int len = recv (net->udp, (void*) buf, sizeof (buf), 0);
	  if (len == sizeof (svr_packet) - 1)
	    {
	      const svr_packet *reply = (const svr_packet*) buf;
	      if (SVR_TEST_MAGIC (reply->magic))
		{ /* try to locate matching request */
		  for (i = 0; i < chunks_count; ++i)
		    {
		      struct wr_chunk *ck = &chunk[i];
		      if (reply->seq_no == ck->seq_no &&
			  reply->command == cmd_write &&
			  GET_NT_DWORD (reply->start) == ck->start &&
			  reply->count == ck->count)
			{ /* done with next 1 or 2 sectors */
			  if (GET_NT_DWORD (reply->result) != 0)
			    return (RET_PROTO_ERR); /* read failed? */

			  if (ck->done == 0)
			    { /* don't decrease counters twice! */
			      ck->done = 1;
			      --chunks_remaining;
			      --chunks_outstanding;
			    }
			  count = 0; /* reset */
			  break;
			}
		    }
		}
	    }
	  else if (len == -1)
	    return (RET_ERR);
	}
      else
	--chunks_outstanding;
    }
  while (++count < NET_RETRY_COUNT && chunks_remaining > 0);
  return (result);
}


/**************************************************************/
static int
net_write (hio_t *hio,
	   u_int32_t start_sector,
	   u_int32_t num_sectors,
	   const void *input,
	   /*@out@*/ u_int32_t *bytes)
{
  hio_net_t *net = (hio_net_t*) hio;
  int result = RET_OK;
  *bytes = 0;
  while (num_sectors > 0 && result == RET_OK)
    { /* divide request to 32kB-size chunks */
      u_int32_t count = (num_sectors > 32 ? 32 : num_sectors), n = 0;
      result = net_write_32k (net, start_sector, count, input, &n);
      if (result == RET_OK)
	{
	  start_sector += count;
	  num_sectors -= count;
	  input = (char*) input + count * 512;
	  *bytes += n;
	}
    }
  if (result == RET_OK)
    /* flush after each request */
    result = net_flush (hio);
  return (result);
}


/**************************************************************/
static int
net_poweroff (hio_t *hio)
{
  hio_net_t *net = (hio_net_t*) hio;
  size_t i;

  svr_packet packet;
  /*COPY_NT_DWORD (packet.magic, svr_magic);*/
  packet.magic [0] = svr_magic [0];
  packet.magic [1] = svr_magic [1];
  packet.magic [2] = svr_magic [2];
  packet.seq_no = (unsigned char) ++seq_no;
  packet.command = (unsigned char) cmd_shutdown;
  packet.count = 0;
  SET_NT_DWORD (packet.start, 0);
  SET_NT_DWORD (packet.result, 0);

  for (i = 0; i < 10; ++i)
    (void) send (net->udp, (const void*) &packet, sizeof (packet) - 1, 0);

  return (0);
}


/**************************************************************/
static int
net_flush (hio_t *hio)
{
  hio_net_t *net = (hio_net_t*) hio;
  u_int32_t retv = 0;
  int result = query_result (net, cmd_sync, &retv);
  if (result == RET_OK && retv != 0)
    result = RET_PROTO_ERR; /* remote problem of some sort? */
  return (result);
}


/**************************************************************/
static int
net_close (/*@special@*/ /*@only@*/ hio_t *hio) /*@releases hio@*/
{
  hio_net_t *net = (hio_net_t*) hio;
#if defined (_BUILD_WIN32)
  closesocket (net->udp);
#elif defined (_BUILD_UNIX)
  close (net->udp);
#endif
  osal_free (hio);

  return (RET_OK);
}


/**************************************************************/
static char*
net_last_error (hio_t *hio)
{
  hio_net_t *net = (hio_net_t*) hio;
  return (osal_get_error_msg (net->error_code));
}


/**************************************************************/
static void
net_dispose_error (hio_t *hio,
		   /*@only@*/ char* error)
{
  osal_dispose_error_msg (error);
}


/**************************************************************/
static hio_t*
net_alloc (const dict_t *config,
	   SOCKET udp)
{
  hio_net_t *net = (hio_net_t*) osal_alloc (sizeof (hio_net_t));
  if (net != NULL)
    {
      memset (net, 0, sizeof (hio_net_t));
      net->hio.stat = &net_stat;
      net->hio.read = &net_read;
      net->hio.write = &net_write;
      net->hio.flush = &net_flush;
      net->hio.close = &net_close;
      net->hio.poweroff = &net_poweroff;
      net->hio.last_error = &net_last_error;
      net->hio.dispose_error = &net_dispose_error;
      net->udp = udp;
    }
  return ((hio_t*) net);
}


/**************************************************************/
int
hio_udpnet2_probe (const dict_t *config,
		   const char *path,
		   hio_t **hio)
{
  int result = RET_NOT_COMPAT;
  char *endp;
  int a, b, c, d;

#if defined (_BUILD_WIN32)
  /* only Windows requires sockets initialization */
  if (!net_init)
    {
      WORD version = MAKEWORD (2, 2);
      WSADATA wsa_data;
      int result = WSAStartup (version, &wsa_data);
      if (result == 0)
	net_init = 1; /* success */
      else
	return (RET_ERR);
    }
#endif

  a = strtol (path, &endp, 10);
  if (a > 0 && a <= 255 && *endp == '.')
    { /* there is a chance */
      b = strtol (endp + 1, &endp, 10);
      if (b >= 0 && b <= 255 && *endp == '.')
	{
	  c = strtol (endp + 1, &endp, 10);
	  if (c >= 0 && c <= 255 && *endp == '.')
	    {
	      d = strtol (endp + 1, &endp, 10);
	      if (d >= 0 && d <= 255 && *endp == '\0')
		{
		  SOCKET udp = socket (PF_INET, SOCK_DGRAM, 0);
		  if (udp != INVALID_SOCKET)
		    { /* generally there ain't clean-up below, but that is
		       * not really fatal for such class application */
		      struct sockaddr_in sa;
		      memset (&sa, 0, sizeof (sa));
		      sa.sin_family = AF_INET;
		      sa.sin_addr.s_addr = htonl ((a << 24) | (b << 16) | (c << 8) | (d));
		      sa.sin_port = htons (NET_HIO_SERVER_PORT);
		      result = connect (udp, (const struct sockaddr*) &sa,
					sizeof (sa)) == 0 ? RET_OK : RET_ERR;
		      if (result == 0)
			{ /* socket connected */
			  *hio = net_alloc (config, udp);
			  if (*hio != NULL)
			    ; /* success */
			  else
			    result = RET_NO_MEM;
			}
		      else
			result = RET_ERR;

		      if (result != RET_OK)
			{ /* close socket on error */
#if defined (_BUILD_WIN32)
			  DWORD err = GetLastError ();
			  closesocket (udp);
			  SetLastError (err);
#elif defined (_BUILD_UNIX)
			  int err = errno;
			  close (udp);
			  errno = err;
#endif
			}
		    }
		}
	    }
	}
    }
  return (result);
}
