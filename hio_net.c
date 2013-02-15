/*
 * hio_net.c - TCP/IP networking access to PS2 HDD
 * $Id: hio_net.c,v 1.7 2004/12/04 10:20:52 b081 Exp $
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
typedef int SOCKET;
#  define INVALID_SOCKET (-1)
#endif
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "osal.h"
#include "hio_net.h"
#include "byteseq.h"
#include "net_io.h"
#include "retcodes.h"


typedef struct hio_net_type
{
  hio_t hio;
  SOCKET sock;
  unsigned char *compressed;
  u_int32_t compr_alloc, compr_used;
  unsigned long error_code;
} hio_net_t;


#if defined (_BUILD_WIN32)
static int net_init = 0;
#endif


/**************************************************************/
static int
recv_exact (SOCKET s,
	    char *outp,
	    u_int32_t bytes,
	    int flags)
{
  int total = 0, result = 1;
  while (bytes > 0 && result > 0)
    {
      result = recv (s, outp + total, bytes, flags);
      if (result > 0)
	{
	  total += result;
	  bytes -= result;
	}
    }
  return (result >= 0 ? total : result);
}


/**************************************************************/
/* execute a command and wait for a reply */
static int
query (SOCKET s,
       unsigned long command,
       unsigned long sector,
       unsigned long num_sectors,
       unsigned long *response,
       const char input [HDD_SECTOR_SIZE * NET_NUM_SECTORS],
       char output [HDD_SECTOR_SIZE * NET_NUM_SECTORS]) /* or NULL */
{
  unsigned char cmd [NET_IO_CMD_LEN + HDD_SECTOR_SIZE * NET_NUM_SECTORS];
  u_int32_t cmd_length = NET_IO_CMD_LEN;
  int result;
  set_u32 (cmd +  0, command);
  set_u32 (cmd +  4, sector);
  set_u32 (cmd +  8, num_sectors);
  set_u32 (cmd + 12, 0x003a2d29);
  if (input != NULL)
    { /* probably a write operation */
      int compressed_data = (num_sectors & 0xffffff00) != 0;
      u_int32_t bytes = compressed_data ? num_sectors >> 8 : num_sectors * HDD_SECTOR_SIZE;
      memcpy (cmd + NET_IO_CMD_LEN, input, bytes);
      cmd_length += bytes;
    }

  result = send (s, (const char*) cmd, cmd_length, 0);
  if (result == cmd_length)
    { /* command successfully sent */
      result = recv_exact (s, (char*) cmd, NET_IO_CMD_LEN, 0);
      if (result == NET_IO_CMD_LEN)
	{ /* response successfully received */
	  *response = get_u32 (cmd + 12);
	  if (output != NULL &&
	      *response != (u_int32_t) -1)
	    { /* receive additional information */
	      result = recv_exact (s, output, HDD_SECTOR_SIZE * num_sectors, 0);
	      result = result == HDD_SECTOR_SIZE * num_sectors ? RET_OK : RET_ERR;
	    }
	  else
	    result = RET_OK;
	}
      else
	result = RET_ERR;
    }
  else
    result = RET_ERR;

  return (result);
}


/**************************************************************/
/* execute a command with no reply */
static int
execute (SOCKET s,
	 unsigned long command,
	 unsigned long sector,
	 unsigned long num_sectors,
	 const char input [HDD_SECTOR_SIZE * NET_NUM_SECTORS])
{
  unsigned char cmd [NET_IO_CMD_LEN + HDD_SECTOR_SIZE * NET_NUM_SECTORS];
  u_int32_t cmd_length = NET_IO_CMD_LEN;
  int result;
  set_u32 (cmd +  0, command);
  set_u32 (cmd +  4, sector);
  set_u32 (cmd +  8, num_sectors);
  set_u32 (cmd + 12, 0x7d3a2d29);
  if (input != NULL)
    { /* probably a write operation */
      int compressed_data = (num_sectors & 0xffffff00) != 0;
      u_int32_t bytes = compressed_data ? num_sectors >> 8 : num_sectors * HDD_SECTOR_SIZE;
      memcpy (cmd + NET_IO_CMD_LEN, input, bytes);
      cmd_length += bytes;
    }

  result = send (s, (const char*) cmd, cmd_length, 0);
  return (result == cmd_length ? RET_OK : RET_ERR);
}


/**************************************************************/
static int
net_stat (hio_t *hio,
	  u_int32_t *size_in_kb)
{
  hio_net_t *net = (hio_net_t*) hio;
  unsigned long size_in_kb2;
  int result = query (net->sock, CMD_HIO_STAT, 0, 0, &size_in_kb2, NULL, NULL);
  if (result == OSAL_OK)
    *size_in_kb = size_in_kb2;
  else
    net->error_code = osal_get_last_error_code ();
  return (result);
}


/**************************************************************/
static int
net_read (hio_t *hio,
	  u_int32_t start_sector,
	  u_int32_t num_sectors,
	  void *output,
	  u_int32_t *bytes)
{
  hio_net_t *net = (hio_net_t*) hio;
  unsigned long response;
  int result;
  char *outp = (char*) output;

  *bytes = 0;
  do
    {
      u_int32_t at_once_s = num_sectors > NET_NUM_SECTORS ? NET_NUM_SECTORS : num_sectors;
      result = query (net->sock, CMD_HIO_READ, start_sector, at_once_s,
		      &response, NULL, outp);
      if (result == OSAL_OK)
	{
	  if (response == at_once_s)
	    {
	      start_sector += at_once_s;
	      num_sectors -= at_once_s;
	      *bytes += HDD_SECTOR_SIZE * at_once_s;
	      outp += HDD_SECTOR_SIZE * at_once_s;
	    }
	  else
	    /* server reported an error; give up */
	    result = RET_SVR_ERR;
	}
      else
	net->error_code = osal_get_last_error_code ();
    }
  while (result == OSAL_OK && num_sectors > 0);
  return (result);
}


/**************************************************************/
static int
net_write (hio_t *hio,
	   u_int32_t start_sector,
	   u_int32_t num_sectors,
	   const void *input,
	   u_int32_t *bytes)
{
  hio_net_t *net = (hio_net_t*) hio;
  unsigned long response;
  int result = RET_OK;
  char *inp = (char*) input;
  /* hm... the overhead should be at most 1 byte on each 128 bytes... but just to be sure */
  u_int32_t avg_compressed_len = NET_NUM_SECTORS * HDD_SECTOR_SIZE * 2;

  if (net->compr_alloc < avg_compressed_len)
    { /* allocate memory to keep compressed data */
      unsigned char *tmp = osal_alloc (avg_compressed_len);
      if (tmp != NULL)
	{
	  if (net->compressed != NULL)
	    osal_free (net->compressed);
	  net->compressed = tmp;
	  net->compr_alloc = avg_compressed_len;
	}
      else
	result = RET_NO_MEM;
    }

  if (result == RET_OK)
    {
      *bytes = 0;
      do
	{
	  u_int32_t at_once_s = num_sectors > NET_NUM_SECTORS ? NET_NUM_SECTORS : num_sectors;
	  const void *data_to_send;
	  u_int32_t sectors_to_send;
#if defined (COMPRESS_DATA)
	  u_int32_t compr_len;
	  const double SMALLEST_PC_FOR_COMPRESSION = 0.64;

	  /* compress input data */
	  rle_compress ((const unsigned char*) inp, at_once_s * HDD_SECTOR_SIZE,
			net->compressed, &compr_len);
	  assert (compr_len < net-> compr_alloc);
	  if (compr_len < (u_int32_t) ((at_once_s * HDD_SECTOR_SIZE) * SMALLEST_PC_FOR_COMPRESSION))
	    { /* < 100% remaining => send compressed */
	      data_to_send = net->compressed;
	      sectors_to_send = compr_len << 8 | at_once_s;
	    }
	  else
#endif /* COMPRESS_DATA defined? */
	    { /* unable to compress => send RAW */
	      data_to_send = inp;
	      sectors_to_send = at_once_s;
	    }

#if defined (NET_SEND_NOACK)
	  result = execute (net->sock, CMD_HIO_WRITE_NACK, start_sector,
			    sectors_to_send, data_to_send);
	  response = at_once_s; /* assume success; server would kick us on error */
#else
#  if defined (NET_QUICK_ACK)
	  result = query (net->sock, CMD_HIO_WRITE_QACK, start_sector,
			  sectors_to_send, &response, data_to_send, NULL);
	  response = at_once_s;
#  elif defined (NET_DUMMY_ACK)
	  result = query (net->sock, CMD_HIO_WRITE_RACK, start_sector,
			  sectors_to_send, &response, data_to_send, NULL);
	  response = at_once_s;
#  else
	  result = query (net->sock, CMD_HIO_WRITE, start_sector,
			  sectors_to_send, &response, data_to_send, NULL);
#  endif
#endif
	  if (result == OSAL_OK)
	    {
	      if (response == at_once_s)
		{
		  start_sector += at_once_s;
		  num_sectors -= at_once_s;
		  *bytes += HDD_SECTOR_SIZE * at_once_s;
		  inp += HDD_SECTOR_SIZE * at_once_s;
		}
	      else
		/* server reported an error; give up */
		result = RET_SVR_ERR;
	    }
	  else
	    net->error_code = osal_get_last_error_code ();
	}
      while (result == OSAL_OK && num_sectors > 0);
    }
  return (result);
}


/**************************************************************/
static int
net_poweroff (hio_t *hio)
{
  hio_net_t *net = (hio_net_t*) hio;
  int result = execute (net->sock, CMD_HIO_POWEROFF, 0, 0, NULL);
  return (result);
}


/**************************************************************/
static int
net_flush (hio_t *hio)
{
  hio_net_t *net = (hio_net_t*) hio;
  unsigned long response;
  int result = query (net->sock, CMD_HIO_FLUSH, 0, 0, &response, NULL, NULL);
  return (result);
}


/**************************************************************/
static int
net_close (hio_t *hio)
{
  hio_net_t *net = (hio_net_t*) hio;
#if defined (_BUILD_WIN32)
  shutdown (net->sock, SD_RECEIVE | SD_SEND);
  closesocket (net->sock);
#elif defined (_BUILD_UNIX)
  close (net->sock);
#endif
  if (net->compressed != NULL)
    osal_free (net->compressed);
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
		   char* error)
{
  osal_dispose_error_msg (error);
}


/**************************************************************/
static hio_t*
net_alloc (SOCKET s)
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
      net->sock = s;
    }
  return ((hio_t*) net);
}


/**************************************************************/
int
hio_net_probe (const char *path,
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
		  SOCKET s = socket (PF_INET, SOCK_STREAM, 0);
		  if (s != INVALID_SOCKET)
		    {
		      struct sockaddr_in sa;
		      memset (&sa, 0, sizeof (sa));
		      sa.sin_family = AF_INET;
		      sa.sin_addr.s_addr = htonl ((a << 24) | (b << 16) | (c << 8) | (d));
		      sa.sin_port = htons (NET_HIO_SERVER_PORT);
		      result = connect (s, (const struct sockaddr*) &sa,
					sizeof (sa)) == 0 ? RET_OK : RET_ERR;
		      if (result == RET_OK)
			{ /* socket connected */
#if defined (_BUILD_WIN32) && 0
			  /* alter Nagle algorithm; testing purposes only */
			  /* inject Rez, 196246KB
			   * real    5m19.347s
			   * user    0m0.020s
			   * sys     0m0.020s
			   */
#if defined (_BUILD_WIN32)
			  BOOL value = 1;
#else
			  int value = 1;
#endif
			  /* return value is ignored */
			  /* void */ setsockopt (s, IPPROTO_TCP, TCP_NODELAY,
						 (void*) &value, sizeof (value));
#endif
			  *hio = net_alloc (s);
			  if (*hio != NULL)
			    ; /* success */
			  else
			    result = RET_NO_MEM;
			}

		      if (result != RET_OK)
			{ /* close socket on error */
#if defined (_BUILD_WIN32)
			  DWORD err = GetLastError ();
			  shutdown (s, SD_RECEIVE | SD_SEND);
			  closesocket (s);
			  SetLastError (err);
#elif defined (_BUILD_UNIX)
			  int err = errno;
			  close (s);
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
