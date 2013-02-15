/*
 * svr/net_server.c
 * $Id: net_server.c,v 1.7 2005/07/10 21:06:48 bobi Exp $
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

/*
 * _BUILD_WIN32 or _BUILD_PS2 controls some specific net I/O calls
 * NOTE: stack size should be at least 4KB + about 1KB
 */

#if !defined (_BUILD_WIN32) && !defined (_BUILD_UNIX) && !defined (_BUILD_PS2)
#  error One of _BUILD_WIN32, _BUILD_UNIX or _BUILD_PS2 should be defined
#endif

#if defined (_BUILD_WIN32)
#  include <winsock.h>
#  include <string.h>
#  if !defined (EWX_FORCEIFHUNG)
#    define EWX_FORCEIFHUNG 0x00000010
#  endif
#endif

#if defined (_BUILD_UNIX)
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <stdio.h>
#  include <string.h>
#endif

#if defined (_BUILD_PS2)
#  include <tamtypes.h>
#  include <stdio.h>
#  include <thbase.h>
#  include <ps2ip.h>
#  include <sysclib.h>
/* IPPROTO_TCP borrowed from ps2sdk/iop/tcpip/lwip/src/include/lwip/tcp.h */
#  ifndef TCP_NODELAY
#    define TCP_NODELAY 0x01
#  endif
/* IPPROTO_TCP borrowed from ps2sdk/iop/tcpip/lwip/src/include/lwip/tcp.h */
#  ifndef TCP_ACKNODELAY
#    define TCP_ACKNODELAY 0x04
#  endif
#  define setsockopt lwip_setsockopt
#  define getsockopt lwip_getsockopt
#endif /* _BUILD_PS2 defined? */

#include "../net_io.h"
#include "../hio.h"
#include "../retcodes.h"
#include "../byteseq.h"


/**************************************************************/
static int /* returns 0 if all data has been sent, or -1 on error */
respond (int s,
	 unsigned long command,
	 unsigned long sector,
	 unsigned long num_sect,
	 unsigned long response,
	 const char *data) /* if != NULL should be exactly num_sect * HDD_SECTOR_SIZE bytes */
{
  unsigned char __attribute__((aligned(16))) resp [NET_IO_CMD_LEN];
  int bytes;

  set_u32 (resp +  0, command);
  set_u32 (resp +  4, sector);
  set_u32 (resp +  8, num_sect);
  set_u32 (resp + 12, response);
  bytes = send (s, (char*) resp, NET_IO_CMD_LEN, 0);
  if (bytes == NET_IO_CMD_LEN)
    {
      if (data != NULL)
	{
	  bytes = send (s, (/* why not const? */ char*) data,
			num_sect * HDD_SECTOR_SIZE, 0);
	  if (bytes == num_sect * HDD_SECTOR_SIZE)
	    ; /* success */
	  else
	    return (RET_ERR);
	}
      return (RET_OK);
    }
  else
    return (RET_ERR);
}


/**************************************************************/
static int /* returns RET_OK or RET_ERR */
recv_exact (int s,
	    char *buffer,
	    int len,
	    int flags)
{
  int result = 1;

  while (len > 0 && result > 0)
    {
      result = recv (s, buffer, len, flags);
      if (result > 0)
	{
	  buffer += result;
	  len -= result;
	}
    }
  return (result > 0 ? RET_OK : RET_ERR);
}


/**************************************************************/
static unsigned char compressed [HDD_SECTOR_SIZE * NET_NUM_SECTORS * 2];
static unsigned char unaligned_buffer [HDD_SECTOR_SIZE * (NET_NUM_SECTORS + 1)];
static unsigned char *sect_data = NULL;

static void
handle_client (int s,
	       hio_t *hio,
	       volatile int *interrupt_flag)
{
  unsigned char __attribute__((aligned(16))) cmd_buff [NET_IO_CMD_LEN];
  int game_over = 0;

  /* this buffer is aligned @ sector size */
  sect_data = (unsigned char*) (((long) &unaligned_buffer [HDD_SECTOR_SIZE - 1]) & ~(HDD_SECTOR_SIZE - 1));

  do
    {
      int result = recv_exact (s, (char*) cmd_buff, NET_IO_CMD_LEN, 0);
      if (result == RET_OK)
	{
	  unsigned long command = get_u32 (cmd_buff + 0);
	  unsigned long sector = get_u32 (cmd_buff + 4);
	  unsigned long num_sect = get_u32 (cmd_buff + 8);

	  switch (command)
	    {
	    case CMD_HIO_STAT:
	      { /* get unit size */
		u_int32_t size_in_kb;
		result = hio->stat (hio, &size_in_kb);
		if (result == RET_OK)
		  /* success */
		  result = respond (s, command, sector, num_sect,
				    size_in_kb, NULL);
		else
		  result = respond (s, command, sector, num_sect,
				    (unsigned long) -1, NULL);
		if (result != RET_OK)
		  game_over = 1; /* if data cannot be send disconnect client */
		break;
	      }

	    case CMD_HIO_READ:
	      { /* read sector(s) */
		u_int32_t bytes_read;
		result = hio->read (hio, sector, num_sect, sect_data,
				    &bytes_read);
		if (result == RET_OK)
		  {
		    size_t sectors_read = bytes_read / HDD_SECTOR_SIZE;
		    result = respond (s, command, sector, num_sect,
				      sectors_read, (char*) sect_data);
		  }
		else
		  result = respond (s, command, sector, num_sect,
				    (unsigned long) -1, NULL);
		if (result != RET_OK)
		  game_over = 1; /* if data cannot be send disconnect client */
		break;
	      }

	    case CMD_HIO_WRITE:       /* write sector, send ACK */
	    case CMD_HIO_WRITE_NACK:  /* write sector, no ACK, quit on error */
	    case CMD_HIO_WRITE_RACK:  /* send dummy ACK, write sector */
	    case CMD_HIO_WRITE_QACK:  /* send dummy ACK before decompr. */
	      { /* write sector(s) */
		int compressed_data = (num_sect & 0xffffff00) != 0;

		if (command == CMD_HIO_WRITE_QACK)
		  /* send back dummy ACK before recv & decompressing data */
		  result = respond (s, command, sector, num_sect, 0, NULL);

		if (compressed_data)
		  { /* accept compressed data into a temp buffer
		       and expand to target one */
		    size_t bytes = num_sect >> 8;
		    result = recv_exact (s, (char*) compressed, bytes, 0);
		    if (result == RET_OK)
		      { /* expand data */
			u_int32_t data_len;
			rle_expand (compressed, bytes, sect_data, &data_len);
			result = (data_len == (num_sect & 0xff) * HDD_SECTOR_SIZE ?
				  RET_OK : RET_ERR);
		      }
		  }
		else /* accept RAW (uncompressed) data */
		  result = recv_exact (s, (char*) sect_data,
				       num_sect * HDD_SECTOR_SIZE, 0);

		if (result == RET_OK)
		  {
		    u_int32_t bytes_written, sectors_written;

		    if (command == CMD_HIO_WRITE_RACK)
		      /* send back dummy ACK to shake the line */
		      result = respond (s, command, sector, num_sect, 0, NULL);

#if 1 /* if 0, measure RAW net transfer speed */
		    if (result == RET_OK)
		      result = hio->write (hio, sector, num_sect & 0xff,
					   sect_data, &bytes_written);
#else
		    bytes_written = (num_sect & 0xff) * HDD_SECTOR_SIZE;
#endif
		    sectors_written = bytes_written / HDD_SECTOR_SIZE;

		    if (result == RET_OK &&
			command == CMD_HIO_WRITE)
		      /* send back real ACK */
		      result = respond (s, command, sector, num_sect,
					(result == RET_OK ? sectors_written :
					 (unsigned long) -1),
					NULL);
		    if (result != RET_OK)
		      game_over = 1; /* disconnect client on error */
		  }
		else
		  /* received less bytes than expected; terminate connection */
		  game_over = 1;
		break;
	      }

	    case CMD_HIO_G_TCPNODELAY:
	      { /* get TCP_NODELAY sock option */
	      	unsigned long ret;
	      	unsigned int len = sizeof (ret);
		result = getsockopt (s, IPPROTO_TCP, TCP_NODELAY,
				     (void*) &ret, (void*) &len);
		if (result != 0 || len != sizeof (ret))
		  ret = -2;
		result = respond (s, command, sector, num_sect, ret, NULL);
		break;
	      }

	    case CMD_HIO_S_TCPNODELAY:
	      { /* set TCP_NODELAY sock option */
		unsigned long ret = setsockopt (s, IPPROTO_TCP, TCP_NODELAY,
						(void*) &sector,
						sizeof (sector));
		result = respond (s, command, sector, num_sect, ret, NULL);
		break;
	      }

	    case CMD_HIO_G_ACKNODELAY:
	      { /* get ACK_NODELAY sock option */
	      	unsigned long ret = 0;
#if defined (_BUILD_PS2)
	      	unsigned int len = sizeof (ret);
		result = getsockopt (s, IPPROTO_TCP, TCP_ACKNODELAY,
				     (void*) &ret, &len);
		if (result != RET_OK || len != sizeof (ret))
		  ret = -2;
#endif
		if (respond (s, command, sector, num_sect, ret, NULL) != RET_OK)
		  game_over = 1;
		break;
	      }

	    case CMD_HIO_S_ACKNODELAY:
	      { /* set ACK_NODELAY sock option */
		unsigned long ret = 0;
#if defined (_BUILD_PS2)
		ret = setsockopt (s, IPPROTO_TCP, TCP_ACKNODELAY,
				  (void*) &sector, sizeof (sector));
#endif
		if (respond (s, command, sector, num_sect, ret, NULL) != RET_OK)
		  game_over = 1;
		break;
	      }

	    case CMD_HIO_POWEROFF:
	      { /* poweroff system */
		hio->flush (hio);
		hio->poweroff (hio);
		break;
	      }
	    }
	}
      else
	/* received less bytes than expected; terminate connection */
	game_over = 1;
    }
  while (!game_over && !*interrupt_flag);

  /* in any case make sure buffer has been flushed */
  hio->flush (hio);

  /* disconnect and return */
#if defined (_BUILD_WIN32)
  shutdown (s, SD_SEND | SD_RECEIVE);
  closesocket (s);
#endif
#if defined (_BUILD_PS2)
  disconnect (s);
#endif
}


/**************************************************************/
int
run_server (unsigned short port,
	    hio_t *hio,
	    volatile int *interrupt_flag)
{
  int s;
  int result;

  s = socket (PF_INET, SOCK_STREAM, 0);
  if (!(s < 0))
    {
      struct sockaddr_in sa;
      memset (&sa, 0, sizeof (sa));
      sa.sin_family = AF_INET;
      sa.sin_addr.s_addr = INADDR_ANY;
      sa.sin_port = htons (port);
      result = bind (s, (struct sockaddr*) &sa, sizeof (sa));
      if (result == 0)
	{
	  result = listen (s, 1);
	  if (result == 0)
	    {
	      do
		{ /* wait for a client to connect and handle connection */
		  int addr_len = sizeof (sa);
		  int transport = accept (s, (struct sockaddr*) &sa, &addr_len);
		  if (transport > 0)
		    handle_client (transport, hio, interrupt_flag);
		}
	      while (!*interrupt_flag); /* loop forever */
	    }
	}

      /* clean-up */
#if defined (_BUILD_WIN32)
      shutdown (s, SD_SEND | SD_RECEIVE);
      closesocket (s);
#endif
#if defined (_BUILD_PS2)
      disconnect (s);
#endif
    }
  else
    result = -1;

  return (result);
}
