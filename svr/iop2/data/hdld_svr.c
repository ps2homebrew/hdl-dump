/*
 * hdld_svr.c
 * $Id: hdld_svr.c,v 1.2 2005/12/08 20:43:31 bobi Exp $
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

#include "byteseq.h"
#include "hio.h"
#include "retcodes.h"
#include "net_io.h"


/* dummy writes to measure network throughput */
/* #define DUMMY_WRITES */


#define MAX_RESP_SIZE (1460 * 2)

#define HDD_SECTOR_SIZE 512    /* HDD sector size in bytes */
#define HDD_NUM_SECTORS  32    /* number of sectors to write at once */
#define NET_NUM_SECTORS  32    /* max number of sectors to transfer via network at once */
#define NET_IO_CMD_LEN (4 * 4) /* command length in bytes in networking I/O */


static hio_t *hio_;
static int clients_ = 0;

typedef struct state_type
{
  struct tcp_pcb *pcb;

  enum
    {
      step_avail = 0,         /* expecting command */
      step_busy_cmd = 1,      /* expecting the rest of the command */
      step_busy_respond = 2,  /* sending back response (and readen data) */
      step_busy_write = 3     /* expecting write data */
    } step;

  int response_pending;
  unsigned char response[NET_IO_CMD_LEN];

  /* while receiving write buffer or while sending read buffer */
  size_t pos, total;
  unsigned char *data;
  unsigned long command, sector, num_sect; /* for pending write operation */

  hio_t *hio;

  size_t bytes_passed, recv_calls;

  unsigned char _buffer[HDD_SECTOR_SIZE * (NET_NUM_SECTORS + 1)];
} state_t;


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
  size_t remaining;

  if (state->step != step_busy_respond)
    return; /* no pending responses */

  if (state->response_pending)
    {
      result = tcp_write (state->pcb, state->response, NET_IO_CMD_LEN, 0);
      if (result == ERR_OK)
	{
	  state->response_pending = 0;
	}
      else
	return; /* unable to send */
    }

  remaining = state->total - state->pos;
  while (remaining > 0 && result == ERR_OK)
    {
      size_t bytes = remaining > MAX_RESP_SIZE ? MAX_RESP_SIZE : remaining;
      result = tcp_write (state->pcb, state->data + state->pos, bytes, 0);
      if (result == ERR_OK)
	{ /* `bytes' successfully sent */
	  state->pos += bytes;
	  remaining -= bytes;
	}
    }

  if (!state->response_pending &&
      state->pos == state->total)
    /* all queued data has been sent; expecting next command */
    state->step = step_avail;
}


/**************************************************************/
/* copy up to (state->total - state->pos) bytes from pbuf to state->data;
   return number of bytes copied */
static size_t
append (state_t *state,
	struct pbuf *p,
	size_t skip)
{
  size_t old_pos = state->pos;
  size_t need = state->total - state->pos;

  struct pbuf *q;
  unsigned char *outp = state->data + state->pos;
  for (q = p; q; q = q->next)
    {
      /* for this pbuf: */
      unsigned char *start = q->payload;
      size_t len = q->len;

      if (skip > 0)
	{ /* skip part/all of current pbuf */
	  size_t advance = skip > len ? len : skip;
	  start += advance;
	  len -= advance;
	  skip -= advance;
	}

      if (len > 0)
	{ /* copy data from current pbuf to our buffer */
	  size_t chunk = need > len ? len : need;
	  memcpy (outp, start, chunk);
	  outp += chunk;
	  state->pos += chunk;
	  need -= chunk;
	  if (need == 0)
	    break; /* we've got all we need */
	}
    }
  return (state->pos - old_pos);
}


/**************************************************************/
/* queues a response entry */
static void
queue_response (state_t *state,
		unsigned long command,
		unsigned long sector,
		unsigned long num_sect,
		unsigned long response,
		size_t data_len)
{
  state->step = step_busy_respond;

  /* response */
  state->response_pending = 1;
  set_u32 (state->response +  0, command);
  set_u32 (state->response +  4, sector);
  set_u32 (state->response +  8, num_sect);
  set_u32 (state->response + 12, response);

  /* attached data */
  state->pos = 0;
  state->total = data_len;
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
		  (result == RET_OK ? size_in_kb : (unsigned long) -1), 0);
}


/**************************************************************/
static void
init_read (state_t *state,
	   unsigned long command,
	   unsigned long sector,
	   unsigned long num_sect)
{
  u_int32_t bytes_read;
  int result = state->hio->read (state->hio, sector, num_sect, state->data, &bytes_read);
  if (result == RET_OK)
    {
      size_t sectors_read = bytes_read / HDD_SECTOR_SIZE;
      queue_response (state, command, sector, num_sect, sectors_read, bytes_read);
    }
  else
    queue_response (state, command, sector, num_sect, (unsigned long) -1, 0);
}


/**************************************************************/
static void
init_write (state_t *state,
	    unsigned long command,
	    unsigned long sector,
	    unsigned long num_sect)
{
  state->step = step_busy_write;
  state->pos = 0;
  state->total = num_sect * HDD_SECTOR_SIZE;
  state->command = command;
  state->sector = sector;
  state->num_sect = num_sect;
}


/**************************************************************/
static void
commit_write (state_t *state)
{
  u_int32_t bytes_written, sectors_written;
#if !defined (DUMMY_WRITES)
  int result = state->hio->write (state->hio, state->sector, state->num_sect,
				  state->data, &bytes_written);
#else
  /* fake write */
  int result = RET_OK;
  bytes_written = state->num_sect * HDD_SECTOR_SIZE;
#endif
  sectors_written = bytes_written / HDD_SECTOR_SIZE;
  if (state->command != CMD_HIO_WRITE_NACK)
    queue_response (state, state->command, state->sector, state->num_sect,
		    result == RET_OK ? sectors_written : (unsigned long) -1, 0);
  else
    state->step = step_avail;
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

  if (err == ERR_OK && p != NULL)
    { /* got data */
      const size_t total_bytes = p->tot_len;
      size_t offset = 0;

      if (state->step == step_busy_respond)
	return (bail_out (pcb, state, p, "received req, but response has not been sent yet"));

      do
	{
	  if (state->step == step_avail)
	    { /* start waiting for 16 bytes long command to execute */
	      state->step = step_busy_cmd;
	      state->total = NET_IO_CMD_LEN;
	      state->pos = 0;
	    }
	  if (state->step == step_busy_cmd)
	    {
	      offset += append (state, p, offset);
	      if (state->pos == state->total)
		{ /* schedule command for execution */
		  unsigned long command = get_u32 (state->data + 0);
		  unsigned long sector = get_u32 (state->data + 4);
		  unsigned long num_sect = get_u32 (state->data + 8);

		  /* multiple queued commands are unsupported */
		  if (offset != total_bytes &&
		      (command != CMD_HIO_WRITE &&
		       command != CMD_HIO_WRITE_NACK &&
		       command != CMD_HIO_WRITE_RACK &&
		       command != CMD_HIO_WRITE_QACK))
		    return (bail_out (pcb, state, p, "got command followed by extra data"));

		  switch (command)
		    {
		    case CMD_HIO_STAT:
		      init_stat (state, command, sector, num_sect);
		      break;

		    case CMD_HIO_READ:
		      init_read (state, command, sector, num_sect);
		      break;

		    case CMD_HIO_WRITE:
		    case CMD_HIO_WRITE_NACK:
		    case CMD_HIO_WRITE_RACK:
		    case CMD_HIO_WRITE_QACK:
		      if ((num_sect & 0xffffff00) != 0)
			return (bail_out (pcb, state, p, "compressed data found"));
		      init_write (state, command, sector, num_sect);
		      break;

		    case CMD_HIO_POWEROFF:
		      init_shutdown (state);
		      break;
		    }
		}
	    }
	  else if (state->step == step_busy_write)
	    { /* accept write data */
	      offset += append (state, p, offset);
	      if (state->pos == state->total)
		{ /* all data is here; write and send response */
		  commit_write (state);
#if 0
		  if (offset != total_bytes)
		    return (bail_out (pcb, state, p, "got extra data after commit_write"));
#endif
		}
	    }
	  else
	    {
	      return (bail_out (pcb, state, p, "got data, but don't know how to handle it"));
	    }
	}
      while (offset < total_bytes);

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
  else if (err == ERR_OK && p == NULL)
    { /* remote host closed connection */
      bail_out (pcb, state, p, "bye");
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
  state->step = step_avail;
  state->data = (unsigned char*) (((long) &state->_buffer[HDD_SECTOR_SIZE - 1]) &
				  ~(HDD_SECTOR_SIZE - 1));

  state->hio = hio_;

  /* tcp_setprio (pcb, TCP_PRIO_MIN); */

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
