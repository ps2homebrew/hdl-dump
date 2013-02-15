/*
 * net_io.c
 * $Id: net_io.c,v 1.3 2004/08/15 16:44:19 b081 Exp $
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

#include "net_io.h"


/**************************************************************/
unsigned long
get_ulong (unsigned char buffer [4])
{
  return ((unsigned long) buffer [0] << 24 |
	  (unsigned long) buffer [1] << 16 |
	  (unsigned long) buffer [2] <<  8 |
	  (unsigned long) buffer [3] <<  0);
}


/**************************************************************/
void
put_ulong (unsigned char buffer [4],
	   unsigned long val)
{
  buffer [0] = (unsigned char) (val >> 24);
  buffer [1] = (unsigned char) (val >> 16);
  buffer [2] = (unsigned char) (val >>  8);
  buffer [3] = (unsigned char) (val >>  0);
}


/**************************************************************/
void /* sort-of optimal RLE compression (will not change mode unless there would be some gain) */
rle_compress (const unsigned char *input,
	      size_t ilength,
	      unsigned char *output,
	      size_t *olength) /* should have at least one byte extra for each 128 bytes */
{
  const unsigned char *run_start = input;
  enum curr_mode_type { cm_diff, cm_repeat } new_mode, last_mode;
  unsigned char *out = output;
  size_t run_len;

  /* prepare */
  run_len = 0;
  new_mode = last_mode = cm_diff;

  while (ilength)
    { /* 2 bytes look-ahead */
      int flush = 0;

      if (last_mode == cm_diff)
	{
	  if (ilength >= 3)
	    new_mode = (input [0] == input [1] && input [0] == input [2]) ? cm_repeat : cm_diff;
	  else /* not enough data to judge */
	    { /* include whatever remains in the diff */
	      run_len += ilength;
	      flush = 1;
	      ilength = 0;
	    }
	}
      else
	new_mode = *input != *run_start ? cm_diff : cm_repeat;

      flush = flush || new_mode != last_mode || ilength == 1;
      if (flush && run_len > 0)
	{ /* need to flush */
	  if (last_mode == cm_diff)
	    {
	      while (run_len > 0)
		{
		  size_t len = run_len > 128 ? 128 : run_len;
		  size_t i;
		  *out++ = 0x00 | (len - 1);
		  for (i=0; i<len; ++i)
		    *out++ = *run_start++;
		  run_len -= len;
		}
	    }
	  else if (last_mode == cm_repeat)
	    {
	      while (run_len > 0)
		{
		  size_t len = run_len > 128 ? 128 : run_len;
		  *out++ = 0x80 | (len - 1);
		  *out++ = *run_start;
		  run_len -= len;
		}
	    }

	  run_start = input;
	  run_len = 0;
	  new_mode = last_mode = cm_diff;
	  continue; /* choose next mode */
	}
      last_mode = new_mode;
      --ilength;
      ++input;
      ++run_len;
    } /* loop */

  *olength = out - output;
}


/**************************************************************/
void /* RLE decompression */
rle_expand (const unsigned char *input,
	    size_t ilength,
	    unsigned char *output,
	    size_t *olength)
{
  const unsigned char *in = input, *end = input + ilength;
  unsigned char *out = output;

  while (in < end)
    {
      unsigned char rpt_byte;
      int mode = *in++;
      size_t i, length = (mode & ~0x80) + 1;
      switch (mode & 0x80)
	{
	case 0x00: /* difference */
	  for (i=0; i<length; ++i)
	    *out++ = *in++;
	  break;

	case 0x80: /* repetition */
	  rpt_byte = *in++;
	  for (i=0; i<length; ++i)
	    *out++ = rpt_byte;
	  break;
	} /* switch */
    } /* main loop */
  *olength = out - output;
}
