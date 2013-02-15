/*
 * progress.c
 * $Id: progress.c,v 1.5 2004/08/15 16:44:19 b081 Exp $
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
#include "progress.h"
#include "osal.h"


/**************************************************************/
progress_t*
pgs_alloc (progress_cb_t progress_cb)
{
  progress_t *pgs = osal_alloc (sizeof (progress_t));
  if (pgs != NULL)
    {
      memset (pgs, 0, sizeof (progress_t));
      pgs->progress_cb_ = progress_cb;
    }
  return (pgs);
}


/**************************************************************/
void
pgs_free (progress_t *pgs)
{
  if (pgs != NULL)
    osal_free (pgs);
}


/**************************************************************/
void
pgs_prepare (progress_t *pgs,
	     bigint_t total)
{
  if (pgs != NULL)
    {
      progress_cb_t progress_cb = pgs->progress_cb_;
      memset (pgs, 0, sizeof (progress_t));

      pgs->start_ = clock ();

      pgs->total = total;

      pgs->estimated = -1;
      pgs->remaining = -1;
      pgs->progress_cb_ = progress_cb;

      if (pgs->progress_cb_ != NULL)
	pgs->progress_cb_ (pgs);
    }
}


/**************************************************************/
void
pgs_chunk_complete (progress_t *pgs)
{
  if (pgs != NULL)
    pgs->offset_ = pgs->curr;
}


/**************************************************************/
char*
fmt_time (char *buffer,
	  int seconds)
{
  if (seconds >= 60)
    {
      if ((seconds % 60) != 0)
	/* minutes + seconds */
	sprintf (buffer, "%d min, %d sec", seconds / 60, seconds % 60);
      else /* minutes only */
	sprintf (buffer, "%d min", seconds / 60);
    }
  else if (seconds > 0)
    /* seconds only */
    sprintf (buffer, "%d sec", seconds);
  else /* seconds <= 0 */
    strcpy (buffer, "0 sec");
  return (buffer);
}


/**************************************************************/
int
pgs_update (progress_t *pgs,
	    bigint_t curr)
{
  if (pgs != NULL && pgs->total > 0)
    {
      clock_t now = clock ();
      struct hist_t *hist = pgs->history_ + pgs->hist_pos_;
      bigint_t prev = pgs->curr;

      pgs->curr = pgs->offset_ + curr;

      pgs->pc_completed = (int) (pgs->curr * 100 / pgs->total);

      /* calculate current speed */
      if (hist->when > 0)
	pgs->curr_bps = (size_t) ((pgs->hist_sum_ * CLOCKS_PER_SEC) / (now - hist->when + 1));
      else
	pgs->curr_bps = (size_t) ((pgs->hist_sum_ * CLOCKS_PER_SEC) / (now - pgs->start_ + 1));
      pgs->hist_sum_ += (pgs->curr - prev) - hist->how_much;
      hist->how_much = (pgs->curr - prev);
      hist->when = now;
      pgs->hist_pos_ = (++pgs->hist_pos_) % PG_HIST_SIZE;

      /* elapsed/estimated time */
      pgs->elapsed_ = now - pgs->start_;
      if (pgs->elapsed_ > 0)
	{
	  pgs->avg_bps = (long) (pgs->curr * 1000 / pgs->elapsed_);
	  pgs->elapsed = pgs->elapsed_ / CLOCKS_PER_SEC;
	  fmt_time (pgs->elapsed_text, pgs->elapsed);

	  if (((pgs->elapsed > 10 && pgs->pc_completed > 0) ||
	       pgs->pc_completed > 10) &&
	      pgs->elapsed > pgs->last_elapsed_)
	    { /* calculate estimated and remaining, format texts */
	      pgs->estimated = (int) (((pgs->elapsed_ * pgs->total) / pgs->curr) / CLOCKS_PER_SEC);
	      pgs->remaining = pgs->estimated - pgs->elapsed + 1;
	      pgs->last_elapsed_ = pgs->elapsed;
	      fmt_time (pgs->estimated_text, pgs->estimated);
	      fmt_time (pgs->remaining_text, pgs->remaining);
	    }
	}

      if (pgs->progress_cb_ != NULL)
	{ /* skip unnecessary updates */
	  if (pgs->call_pc_completed_ != pgs->pc_completed ||
	      pgs->call_elapsed_ != pgs->elapsed ||
	      pgs->call_estimated_ != pgs->estimated ||
	      pgs->call_remaining_ != pgs->remaining)
	    {
	      pgs->call_pc_completed_ = pgs->pc_completed;
	      pgs->call_elapsed_ = pgs->elapsed;
	      pgs->call_estimated_ = pgs->estimated;
	      pgs->call_remaining_ = pgs->remaining;
	      return (pgs->progress_cb_ (pgs)); /* let callback decide whether to continue */
	    }
	  else
	    return (0); /* continue */
	}
      else
	return (0); /* continue */
    }
  else
    return (0); /* continue */
}
