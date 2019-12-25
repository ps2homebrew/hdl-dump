/*
 * thd_iin.c
 * $Id: thd_iin.c,v 1.1 2006/09/01 17:37:58 bobi Exp $
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
#  include <windows.h>
#  include "sema.h" /* POSIX semaphores for win32 */
#else
#  include <pthread.h>
#if defined (__MACH__)
#  include <dispatch/dispatch.h>
#else
#  include <semaphore.h>
#endif
#endif
#include <stdio.h>
#include <string.h>
#include "iin.h"
#include "osal.h"
#include "retcodes.h"


typedef struct buffer_type
{
  /* input */
  u_int32_t start_sector, num_sectors;

  /* output */
  int result;
  char buf[IIN_SECTOR_SIZE * IIN_NUM_SECTORS];
  u_int32_t bytes;
} buffer_t;

typedef struct job_type
{
  u_int32_t start_sector, num_sectors;
  buffer_t *dest;
} job_t;


typedef struct threaded_decorator_type
{
  iin_t iin;

  iin_t *worker;
  u_int32_t total_sectors;

  job_t job;

  /* both are exclusive => at any moment only one is acquirable */
#if defined (__MACH__)
  dispatch_semaphore_t worker_lock, master_lock;
#else
  sem_t worker_lock, master_lock;
#endif

  buffer_t buf[2];
  size_t active_buf; /* <= % 2 == index of the current buffer */

  volatile int done;
} threaded_decorator_t;


/**************************************************************/
#if defined (_BUILD_WIN32)
static DWORD WINAPI
thd_loop (void *p)
#else
static void*
thd_loop (void *p)
#endif
{
  threaded_decorator_t *thd = (threaded_decorator_t*) p;

  do
    {
#if defined(__MACH__)
      int result = dispatch_semaphore_wait (thd->worker_lock, DISPATCH_TIME_FOREVER);
#else
      int result = sem_wait (&thd->worker_lock);
#endif
      if (result == 0 && !thd->done)
	{
	  job_t *job = &thd->job;
	  buffer_t *dest = job->dest;
	  const char *data = NULL;

	  /* execute queued read and copy data */
	  dest->result = thd->worker->read (thd->worker, job->start_sector,
					    job->num_sectors,
					    &data, &dest->bytes);
	  if (dest->result == RET_OK)
	    memcpy (dest->buf, data, dest->bytes);

	  /* complete */
	  dest->start_sector = job->start_sector;
	  dest->num_sectors = job->num_sectors;

	  /* unlock master */
#if defined(__MACH__)
    dispatch_semaphore_signal (thd->master_lock);
#else
	  sem_post (&thd->master_lock);
#endif
	}
    }
  while (!thd->done);

  /* notify master */
#if defined(__MACH__)
  dispatch_semaphore_signal (thd->master_lock);
#else
  sem_post (&thd->master_lock);
#endif
  return (0);
}


/**************************************************************/
static int
thd_read (iin_t *iin,
	  u_int32_t start_sector,
	  u_int32_t num_sectors,
	  /*@out@*/ const char **data,
	  /*@out@*/ u_int32_t *length)
{
  threaded_decorator_t *thd = (threaded_decorator_t*) iin;
  int result;

  do
    {
#if defined(__MACH__)
      result = dispatch_semaphore_wait (thd->master_lock, DISPATCH_TIME_FOREVER);
#else
      result = sem_wait (&thd->master_lock);
#endif
      if (result == 0)
	{ /* worker is idle */
	  const buffer_t *buf = thd->buf + (thd->active_buf % 2);
	  if (start_sector == buf->start_sector &&
	      num_sectors == buf->num_sectors)
	    { /* data is currently available */
	      *data = buf->buf;
	      *length = buf->bytes;
	      result = buf->result;
	      break;
	    }
	  else
	    { /* data not available; schedule and wait untill its ready */
	      thd->job.start_sector = start_sector;
	      thd->job.num_sectors = num_sectors;
	      thd->job.dest = thd->buf + (thd->active_buf % 2);
#if defined(__MACH__)
        dispatch_semaphore_signal (thd->worker_lock);
#else
	      sem_post (&thd->worker_lock);
#endif
	    }
	}
    }
  while (1);

  if (result == RET_OK &&
      num_sectors == IIN_NUM_SECTORS &&
      start_sector + num_sectors < thd->total_sectors)
    { /* schedule pre-fetch for the next group of sectors */
      thd->job.start_sector = start_sector + num_sectors;
      thd->job.num_sectors = IIN_NUM_SECTORS;
      thd->job.dest = thd->buf + (++thd->active_buf % 2);
#if defined(__MACH__)
        dispatch_semaphore_signal (thd->worker_lock);
#else
        sem_post (&thd->worker_lock);
#endif
    }
  else
    /* since there is no pre-fetch scheduled,
       unlock master or the next read call would block forever */
#if defined(__MACH__)
    dispatch_semaphore_signal (thd->master_lock);
#else
    sem_post (&thd->master_lock);
#endif

  return (result);
}


/**************************************************************/
static int
thd_stat (iin_t *iin,
	  /*@out@*/ u_int32_t *sector_size,
	  /*@out@*/ u_int32_t *num_sectors)
{
  threaded_decorator_t *thd = (threaded_decorator_t*) iin;
  *sector_size = IIN_SECTOR_SIZE;
  *num_sectors = thd->total_sectors;
  return (RET_OK);
}


/**************************************************************/
static int
thd_close (iin_t *iin)
{
  threaded_decorator_t *thd = (threaded_decorator_t*) iin;
  int result;

  /* wait for worker to finish... */
#if defined(__MACH__)
  dispatch_semaphore_wait (thd->master_lock, DISPATCH_TIME_FOREVER);
#else
  sem_wait (&thd->master_lock);
#endif
  thd->done = 1;
#if defined(__MACH__)
  dispatch_semaphore_signal (thd->worker_lock);
  dispatch_semaphore_wait (thd->master_lock, DISPATCH_TIME_FOREVER);
#else
  sem_post (&thd->worker_lock);
  sem_wait (&thd->master_lock);
#endif

  /* clean-up */
#if defined(__MACH__)
  dispatch_release (thd->worker_lock);
  dispatch_release (thd->master_lock);
#else
  sem_destroy (&thd->worker_lock);
  sem_destroy (&thd->master_lock);
#endif
  result = thd->worker->close (thd->worker);
  thd->worker = NULL;

  osal_free (thd), thd = NULL;
  return (result);
}


/**************************************************************/
static char*
thd_last_error (iin_t *iin)
{
  threaded_decorator_t *thd = (threaded_decorator_t*) iin;
  return (thd->worker->last_error (thd->worker));
}


/**************************************************************/
static void
thd_dispose_error (iin_t *iin,
		   /*@only@*/ char* error)
{
  threaded_decorator_t *thd = (threaded_decorator_t*) iin;
  thd->worker->dispose_error (thd->worker, error);
}


/**************************************************************/
iin_t*
thd_create (iin_t *worker)
{
  u_int32_t total_sectors, sector_size;
  threaded_decorator_t *thd;

  if (worker->stat (worker, &sector_size, &total_sectors) != RET_OK)
    return (NULL);

  thd = (threaded_decorator_t*) osal_alloc (sizeof (threaded_decorator_t));
  if (thd != NULL)
    {
      memset (thd, 0, sizeof (threaded_decorator_t));
      thd->iin.stat = &thd_stat;
      thd->iin.read = &thd_read;
      thd->iin.close = &thd_close;
      /* TODO: preserve errors? */
      thd->iin.last_error = &thd_last_error;
      thd->iin.dispose_error = &thd_dispose_error;
      strcpy (thd->iin.source_type, "Threaded decorator");

      thd->worker = worker;
      thd->total_sectors = total_sectors;
      thd->active_buf = 0;
      thd->done = 0;

      /* master is unlocked, worker is locked; would run on 1st need */
#if defined(__MACH__)
      thd->worker_lock = dispatch_semaphore_create(0);
      if (thd->worker_lock != NULL)
#else
      if (sem_init (&thd->worker_lock, 0, 0) == 0)
#endif
	{
#if defined(__MACH__)
    thd->master_lock = dispatch_semaphore_create(1);
    if (thd->master_lock != NULL)
#else
	  if (sem_init (&thd->master_lock, 0, 1) == 0)
#endif
	    {
#if defined (_BUILD_WIN32)
	      DWORD thread_id = 0;
	      HANDLE h = CreateThread (NULL, 0, &thd_loop, thd, 0, &thread_id);
	      if (h != NULL)
		return ((iin_t*) thd); /* win32: success */

#else
	      pthread_t thread_id = 0;
	      if (pthread_create (&thread_id, NULL, &thd_loop, thd) == 0)
		return ((iin_t*) thd); /* unix: success */
#endif

#if defined(__MACH__)
        dispatch_release (thd->master_lock);
#else
	      (void) sem_destroy (&thd->master_lock);
#endif
	    }
#if defined(__MACH__)
    dispatch_release (thd->worker_lock);
#else
	  (void) sem_destroy (&thd->worker_lock);
#endif
	}
      osal_free (thd), thd = NULL;
    }
  return (NULL);
}
