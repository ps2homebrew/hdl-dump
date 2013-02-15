/*
 * hio_trace.c - decorator to trace HIO access
 * $Id: hio_trace.c,v 1.2 2006/09/01 17:27:18 bobi Exp $
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

#include "hio_trace.h"
#include "hio.h"
#include "net_io.h"
#include "osal.h"
#include "retcodes.h"
#include <stdio.h>
#include <string.h>


typedef struct hio_trace_type
{
  hio_t hio;
  FILE *log;
  int close_log;
  hio_t *real;
} hio_trace_t;


/**************************************************************/
static int
trace_stat (hio_t *hio,
	    /*@out@*/ u_int32_t *size_in_kb)
{
  hio_trace_t *trace = (hio_trace_t*) hio;
  int result;
  fprintf (trace->log, "hio->stat (%p, ", (void*) trace->real);
  result = trace->real->stat (trace->real, size_in_kb);
  fprintf (trace->log, "%lu) = %d\n",
	   (unsigned long) *size_in_kb, result);
  return (result);
}


/**************************************************************/
static int
trace_read (hio_t *hio,
	    u_int32_t start_sector,
	    u_int32_t num_sectors,
	    /*@out@*/ void *output,
	    /*@out@*/ u_int32_t *bytes)
{
  hio_trace_t *trace = (hio_trace_t*) hio;
  int result;
  fprintf (trace->log, "hio->read (%p, 0x%08lx, %lu, %p, ",
	   (void*) trace->real, (unsigned long) start_sector,
	   (unsigned long) num_sectors, output);
  result = trace->real->read (trace->real, start_sector, num_sectors,
			      output, bytes);
  fprintf (trace->log, "%lu) = %d\n", (unsigned long) *bytes, result);
  return (result);
}


/**************************************************************/
static int
trace_write (hio_t *hio,
	     u_int32_t start_sector,
	     u_int32_t num_sectors,
	     const void *input,
	     /*@out@*/ u_int32_t *bytes)
{
  hio_trace_t *trace = (hio_trace_t*) hio;
  int result;
  fprintf (trace->log, "hio->write (%p, 0x%08lx, %lu, %p, ",
	   (void*) trace->real, (unsigned long) start_sector,
	   (unsigned long) num_sectors, input);
  result = trace->real->write (trace->real, start_sector, num_sectors,
			       input, bytes);
  fprintf (trace->log, "%lu) = %d\n", (unsigned long) *bytes, result);
  return (result);
}


/**************************************************************/
static int
trace_poweroff (hio_t *hio)
{
  hio_trace_t *trace = (hio_trace_t*) hio;
  int result;
  fprintf (trace->log, "hio->poweroff (%p", (void*) trace->real);
  result = trace->real->poweroff (trace->real);
  fprintf (trace->log, ") = %d\n", result);
  return (result);
}


/**************************************************************/
static int
trace_flush (hio_t *hio)
{
  hio_trace_t *trace = (hio_trace_t*) hio;
  int result;
  fprintf (trace->log, "hio->flush (%p", (void*) trace->real);
  result = trace->real->poweroff (trace->real);
  fprintf (trace->log, ") = %d\n", result);
  return (result);
}


/**************************************************************/
static int
trace_close (/*@special@*/ /*@only@*/ hio_t *hio) /*@releases hio@*/
{
  hio_trace_t *trace = (hio_trace_t*) hio;
  int result;
  fprintf (trace->log, "hio->close (%p", (void*) trace->real);
  result = trace->real->close (trace->real);
  fprintf (trace->log, ") = %d\n", result);
  if (trace->close_log)
    fclose (trace->log);
  osal_free (trace);
  return (result);
}


/**************************************************************/
static char*
trace_last_error (hio_t *hio)
{
  hio_trace_t *trace = (hio_trace_t*) hio;
  char *err;
  fprintf (trace->log, "hio->last_error (%p", (void*) trace->real);
  err = trace->real->last_error (trace->real);
  fprintf (trace->log, ") = \"%s\"\n", err);
  return (err);
}


/**************************************************************/
static void
trace_dispose_error (hio_t *hio,
		     /*@only@*/ char* error)
{
  hio_trace_t *trace = (hio_trace_t*) hio;
  fprintf (trace->log, "hio->dispose_error (%p, \"%s\")\n",
	   (void*) trace->real, error);
  trace->real->dispose_error (trace->real, error);
}


/**************************************************************/
static hio_t*
trace_alloc (hio_t *real)
{
  hio_trace_t *trace = (hio_trace_t*) osal_alloc (sizeof (hio_trace_t));
  if (trace != NULL)
    {
      memset (trace, 0, sizeof (hio_trace_t));
      trace->hio.stat = &trace_stat;
      trace->hio.read = &trace_read;
      trace->hio.write = &trace_write;
      trace->hio.flush = &trace_flush;
      trace->hio.close = &trace_close;
      trace->hio.poweroff = &trace_poweroff;
      trace->hio.last_error = &trace_last_error;
      trace->hio.dispose_error = &trace_dispose_error;

      trace->log = stdout;
      trace->close_log = 0;
      trace->real = real;
    }
  return ((hio_t*) trace);
}


/**************************************************************/
int
hio_trace_probe (const dict_t *config,
		 const char *path,
		 hio_t **hio)
{
  static const char *MONIKER = "trace:";
  int result = RET_NOT_COMPAT;
  if (memcmp (path, MONIKER, strlen (MONIKER)) == 0)
    {
      hio_t *real;
      result = hio_probe (config, path + strlen (MONIKER), &real);
      if (result == RET_OK)
	{
	  *hio = trace_alloc (real);
	  if (*hio == NULL)
	    {
	      real->close (real);
	      result = RET_NO_MEM;
	    }
	}
    }
  return (result);
}
