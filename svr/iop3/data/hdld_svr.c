/*
 * hdld_svr.c
 * $Id: hdld_svr.c,v 1.1 2005/12/08 20:44:22 bobi Exp $
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

#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"

#include "byteseq.h"
#include "hio.h"
#include "retcodes.h"
#include "net_io.h"


/* dummy writes to measure network throughput */
/* #define DUMMY_WRITES */
/* #define FAKE_WRITES */


#define MAX_RESP_SIZE (1460 * 2)

#define HDD_SECTOR_SIZE  512   /* HDD sector size in bytes */
#define HDD_NUM_SECTORS   32   /* number of sectors to write at once */
#define NET_NUM_SECTORS 2048   /* max number of sectors to transfer via network at once */
#define NET_IO_CMD_LEN (4 * 4) /* command length in bytes in networking I/O */


#define SETBIT(mask, bit) (mask)[(bit) / 32] |= 1 << ((bit) % 32)
#define GETBIT(mask, bit) ((mask)[(bit) / 32] & (1 << ((bit) % 32)))


static hio_t *hio_;
static int clients_ = 0;

typedef struct state_type
{
  struct tcp_pcb *pcb;
  struct udp_pcb *udp;

  enum
    {
      state_avail = 0,         /* expecting command */
      state_busy_respond = 1,  /* sending back response (and readen data) */
      state_busy_write = 2,    /* expecting write data */
      state_write_stat = 3     /* sending back write stat response */
    } state;

  /* response */
  int resp_cmd_queued;
  unsigned char resp_cmd[NET_IO_CMD_LEN];
  size_t out_pos, out_total;
  unsigned char *out;

  /* incoming data */
  unsigned char *in;
  u_int32_t bitmask[(NET_NUM_SECTORS + 31) / 32];
  unsigned long command, start, num_sect;

  size_t bytes_passed, recv_calls; /* TCP/IP window delayed free statistics */

  hio_t *hio;

  unsigned char _buffer[HDD_SECTOR_SIZE * (NET_NUM_SECTORS + 1)];
} state_t;


typedef struct udp_packet_t
{
  unsigned char sector[HDD_SECTOR_SIZE * 2];
  unsigned long command, start;
} udp_packet_t;


/**************************************************************/
static void
handle_udp_recv (void *arg,
		 struct udp_pcb *pcb,
		 struct pbuf *p,
		 struct ip_addr *ip,
		 u16_t port)
{
  state_t *state = (state_t*) arg;
  struct pbuf *q;
  for (q = p; q; q = q->next)
    {
      udp_packet_t *packet = (udp_packet_t*) q->payload;
      size_t len = q->len;
      if (len == sizeof (udp_packet_t) &&
	  get_u32 (&packet->command) == state->command)
	{ /* check if sect between start & end */
	  unsigned long sect = get_u32 (&packet->start);
	  if (state->start <= sect && sect < state->start + state->num_sect)
	    {
	      unsigned long start = sect - state->start;
	      if (!GETBIT (state->bitmask, start))
		{
		  memcpy (state->in + start * 512,
			  packet, HDD_SECTOR_SIZE * 2);

		  SETBIT (state->bitmask, start);
		  SETBIT (state->bitmask, start + 1);
		}
	    }
	}
    }
  pbuf_free (p);
}


/**************************************************************/
static err_t
bail_out (struct tcp_pcb *pcb,
	  state_t *state,
	  struct pbuf *p,
	  const char *error)
{
  if (pcb != NULL)
    {
      tcp_arg (pcb, NULL);
      tcp_sent (pcb, NULL);
      tcp_recv (pcb, NULL);

      /* attempt to return back error message */
      tcp_write (pcb, error, strlen (error), 1);

      tcp_close (pcb);
    }
  if (state != NULL)
    {
      if (state->udp != NULL)
	udp_remove (state->udp);

      state->hio->flush (state->hio);
      state->hio->close (state->hio);
      FreeSysMemory (state);
    }
  if (p != NULL)
    pbuf_free (p);
  --clients_;
  return (ERR_OK);
}


/**************************************************************/
/* sends as many bytes as possible from the queued response */
static void
send_response (state_t *state)
{
  err_t result = ERR_OK;

  if (state->state == state_busy_respond)
    {
      size_t remaining;
      if (state->resp_cmd_queued)
	{ /* send queued command */
	  result = tcp_write (state->pcb, state->resp_cmd, NET_IO_CMD_LEN, 0);
	  if (result == ERR_OK)
	    state->resp_cmd_queued = 0;
	  else
	    return; /* unable to send */
	}

      remaining = state->out_total - state->out_pos;
      while (remaining > 0 && result == ERR_OK)
	{
	  size_t bytes = remaining > MAX_RESP_SIZE ? MAX_RESP_SIZE : remaining;
	  result = tcp_write (state->pcb, state->out + state->out_pos, bytes, 0);
	  if (result == ERR_OK)
	    { /* `bytes' successfully sent */
	      state->out_pos += bytes;
	      remaining -= bytes;
	    }
	}

      if (!state->resp_cmd_queued &&
	  state->out_pos == state->out_total)
	/* all queued data has been sent; expecting next command */
	state->state = state_avail;
    }
}


/**************************************************************/
/* queues a response entry */
static void
queue_response (state_t *state,
		unsigned long command,
		unsigned long sector,
		unsigned long num_sect,
		unsigned long response,
		unsigned char *buf,
		size_t data_len)
{
  state->state = state_busy_respond;

  /* response */
  state->resp_cmd_queued = 1;
  set_u32 (state->resp_cmd +  0, command);
  set_u32 (state->resp_cmd +  4, sector);
  set_u32 (state->resp_cmd +  8, num_sect);
  set_u32 (state->resp_cmd + 12, response);

  /* attached data */
  state->out = buf;
  state->out_pos = 0;
  state->out_total = data_len;
}


/**************************************************************/
static void
init_stat (state_t *state,
	   unsigned long command,
	   unsigned long sector,
	   unsigned long num_sect)
{
  u_int32_t size_in_kb;
  int result = state->hio->stat (state->hio, &size_in_kb);
  queue_response (state, command, sector, num_sect,
		  (result == RET_OK ? size_in_kb : (unsigned long) -1),
		  NULL, 0);
}


/**************************************************************/
static void
init_read (state_t *state,
	   unsigned long command,
	   unsigned long sector,
	   unsigned long num_sect)
{
  u_int32_t bytes_read;
  int result;

  state->out = (unsigned char*) (((long) &state->_buffer[HDD_SECTOR_SIZE - 1]) &
				 ~(HDD_SECTOR_SIZE - 1));
  result = state->hio->read (state->hio, sector, num_sect, state->out, &bytes_read);
  if (result == RET_OK)
    { /* success */
      size_t sectors_read = bytes_read / HDD_SECTOR_SIZE;
      queue_response (state, command, sector, num_sect, sectors_read,
		      state->out, bytes_read);
    }
  else
    queue_response (state, command, sector, num_sect, (unsigned long) -1,
		    NULL, 0);
}


/**************************************************************/
static int
init_write (state_t *state,
	    unsigned long command,
	    unsigned long sector,
	    unsigned long num_sect)
{
  queue_response (state, command, sector, num_sect,
		  0, NULL, 0); /* confirm write */
  send_response (state);
  if (state->state != state_avail)
    return (-1); /* assume send_response is always able to send 16 bytes of data */

  state->state = state_busy_write;
  state->command = command;
  state->start = sector;
  state->num_sect = num_sect;

  /* set-up buffer and clear bitmask */
  state->in = (unsigned char*) (((long) &state->_buffer[HDD_SECTOR_SIZE - 1]) &
				~(HDD_SECTOR_SIZE - 1));
  memset (state->bitmask, 0, sizeof (state->bitmask));
  return (0);
}


/**************************************************************/
static void
commit_write (state_t *state)
{
  u_int32_t bytes_written = 0, sectors_written;

#if defined (FAKE_WRITES)
  int result = state->hio->read (state->hio, state->start, state->num_sect,
				 state->in, &bytes_written);
#else
#  if !defined (DUMMY_WRITES)
  int result = state->hio->write (state->hio, state->start, state->num_sect,
				  state->in, &bytes_written);
#  else
  /* fake write */
  int result = RET_OK;
  bytes_written = state->num_sect * HDD_SECTOR_SIZE;
#  endif
#endif
  sectors_written = bytes_written / HDD_SECTOR_SIZE;
  queue_response (state, state->command, state->start, state->num_sect,
		  result == RET_OK ? sectors_written : (unsigned long) -1,
		  NULL, 0);
}


/**************************************************************/
static int
init_write_stat (state_t *state,
		 unsigned long command,
		 unsigned long sector,
		 unsigned long num_sect)
{
  /* check if all write data is here */
  int ok = 1;
  size_t i;
  for (i = 0; i < num_sect; ++i)
    if (!GETBIT (state->bitmask, i))
      {
	ok = 0;
	break;
      }
  if (ok)
    { /* all data here -- commit */
      state->command = CMD_HIO_WRITE_STAT;
      commit_write (state);
    }
  else
    { /* some parts are missing; ask for retransmit */
      unsigned long *out = (unsigned long*) (((long) &state->_buffer[HDD_SECTOR_SIZE - 1]) &
					     ~(HDD_SECTOR_SIZE - 1));
      for (i = 0; i < (NET_NUM_SECTORS + 31) / 32; ++i)
	set_u32 (out + i, state->bitmask[i]);
      queue_response (state, command, sector, num_sect, 0,
		      (void*) out, sizeof (state->bitmask));
      send_response (state);
      if (state->state != state_avail)
	return (-1); /* assume send_response is always able to send 16 + 256 bytes of data */
      state->state = state_busy_write;
    }
  return (0);
}


/**************************************************************/
static void
init_shutdown (state_t *state)
{
  state->hio->flush (state->hio);
  state->hio->poweroff (state->hio);
}


/**************************************************************/
/* handle acknowledge */
static err_t
handle_ack (void *arg, struct tcp_pcb *pcb, u16_t len)
{
  state_t *state = (state_t*) arg;

  /* maybe there is more data waiting to be send to the client */
  send_response (state);

  return (ERR_OK);
}


/**************************************************************/
static err_t
handle_recv (void *arg,
	     struct tcp_pcb *pcb,
	     struct pbuf *p,
	     err_t err)
{
  state_t *state = (state_t*) arg;

  if (err == ERR_OK && p == NULL)
    /* remote host closed connection */
    bail_out (pcb, state, p, "bye");
  if (p->len != NET_IO_CMD_LEN)
    return (bail_out (pcb, state, p, "handle_recv: invalid packet received"));

  if (err == ERR_OK && p != NULL)
    { /* got data */
      const size_t total_bytes = p->tot_len;
      unsigned char *payload = p->payload;
      unsigned long command = get_u32 (payload + 0);
      unsigned long sector = get_u32 (payload + 4);
      unsigned long num_sect = get_u32 (payload + 8);

      if (state->state == state_avail)
	{
	  if (command == CMD_HIO_WRITE_STAT)
	    return (bail_out (pcb, state, p, "write stat denied"));
	}
      else
	{
	  if (!(state->state == state_busy_write &&
		command == CMD_HIO_WRITE_STAT))
	    return (bail_out (pcb, state, p, "busy"));
	}

      switch (command)
	{
	case CMD_HIO_STAT:
	  init_stat (state, command, sector, num_sect);
	  break;

	case CMD_HIO_READ:
	  init_read (state, command, sector, num_sect);
	  break;

	case CMD_HIO_WRITE:
	  if (init_write (state, command, sector, num_sect) == -1)
	    return (bail_out (pcb, state, p, "init_write b0rked"));
	  break;

	case CMD_HIO_WRITE_STAT:
	  if (sector != state->start ||
	      num_sect != state->num_sect)
	    return (bail_out (pcb, state, p, "invalid write stat"));
	  if (init_write_stat (state, command, sector, num_sect) == -1)
	    return (bail_out (pcb, state, p, "init_write_stat failed"));
	  break;

	case CMD_HIO_POWEROFF:
	  init_shutdown (state);
	  break;

	default:
	  return (bail_out (pcb, state, p, "handle_recv: unknown command"));
	}

      pbuf_free (p);

      ++state->recv_calls;
      state->bytes_passed += total_bytes;
      if (state->bytes_passed > 1024 * 16)
	{ /* delay window size update */
	  tcp_recved (pcb, state->bytes_passed);
	  state->bytes_passed = 0;
	}

      send_response (state);
    }
  return (ERR_OK);
}


/**************************************************************/
static err_t
accept (void *arg, struct tcp_pcb *pcb, err_t err)
{
  state_t *state;

  if (clients_ > 0)
    {
      bail_out (pcb, NULL, NULL, "only one client supported");
      return (ERR_MEM);
    }

  state = AllocSysMemory (ALLOC_FIRST, sizeof (state_t), NULL);
  if (state == NULL)
    {
      bail_out (pcb, NULL, NULL, "out of memory");
      return (ERR_MEM);
    }

  /* initialize */
  memset (state, 0, sizeof (state_t));
  state->pcb = pcb;
  state->state = state_avail;

  state->hio = hio_;

  /* bind UDP socket, too */
  state->udp = udp_new ();
  udp_bind (state->udp, IP_ADDR_ANY, NET_HIO_SERVER_PORT);
  udp_recv (state->udp, handle_udp_recv, state);

  /* attach callbacks */
  tcp_arg (pcb, state);
  tcp_sent (pcb, &handle_ack);
  tcp_recv (pcb, &handle_recv);
  ++clients_;

  return (ERR_OK);
}


/**************************************************************/
int hio_iop_probe (const char *path,
		   hio_t **hio);

void
hdld_svr_init (void)
{
  struct tcp_pcb *pcb;

  int result = hio_iop_probe ("hdd0:", &hio_);
  if (result == RET_OK)
    {
      pcb = tcp_new ();
      tcp_bind (pcb, IP_ADDR_ANY, NET_HIO_SERVER_PORT);
      pcb = tcp_listen (pcb);
      tcp_accept (pcb, accept);
    }
}
