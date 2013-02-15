/*
 * progress.h
 * $Id: progress.h,v 1.5 2004/08/15 16:44:19 b081 Exp $
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

#if !defined (_PROGRESS_H)
#define _PROGRESS_H

#include "config.h"
#include <time.h>


/* the shorter that is, the faster it will respond to the changes */
#define PG_HIST_SIZE 128


typedef struct progress_type progress_t;

/* returns 0 to continue, other to interrupt */
typedef int (*progress_cb_t) (progress_t *);

/* TODO: check overflow with big files */
struct progress_type
{ /* "private" */
  clock_t start_, elapsed_; /* in clocks */
  bigint_t offset_; /* of the current block, absolute */
  progress_cb_t progress_cb_;
  int last_elapsed_; /* last time when the estimated has been calculated */

  /* history/histogram to track current speed */
  struct hist_t
  { 
    size_t how_much;
    clock_t when;
  } history_ [PG_HIST_SIZE];
  size_t hist_pos_;
  bigint_t hist_sum_; /* = select sum (how_much) from history_ */

  /* last major values when callback has been called */
  int call_pc_completed_, call_elapsed_, call_estimated_, call_remaining_;

  /* "public" */
  bigint_t total, curr; /* in bytes */
  long avg_bps, curr_bps; /* average and current bytes per seconds (since the begining) */
  int pc_completed; /* in % */
  int elapsed, estimated, remaining; /* in seconds or -1 */
  char elapsed_text [20], estimated_text [20], remaining_text [20];
};


progress_t* pgs_alloc (progress_cb_t progress_cb);

void pgs_free (progress_t *pgs);

void pgs_prepare (progress_t *pgs,
		  bigint_t total);

void pgs_chunk_complete (progress_t *pgs);

int pgs_update (progress_t *pgs,
		bigint_t curr);


#endif /* _PROGRESS_H defined? */
