/*
 * dict.h
 * $Id: dict.h,v 1.1 2005/07/10 21:06:48 bobi Exp $
 *
 * Copyright 2005 Bobi B., w1zard0f07@yahoo.com
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

#if !defined (_DICT_H)
#define _DICT_H

#include "config.h"

C_START

typedef struct dict_type dict_t;

/* returns NULL when out of memory */
dict_t* dict_alloc (void);

void dict_free (dict_t *dict);

/* set or replace; returns non-zero on success */
int dict_put (dict_t *dict,
	      const char *key,
	      const char *value);

/* query; returns NULL if not found */
const char* dict_lookup (const dict_t *dict,
			 const char *key);

int dict_put_flag (dict_t *dict,
		   const char *key,
		   int value);

int dict_get_flag (const dict_t *dict,
		   const char *key,
		   int default_value);

int dict_store (const dict_t *dict,
		const char *filename);

/* returns NULL on error or bad file format; src might be NULL */
dict_t* dict_restore (dict_t *src,
		      const char *filename);

/* returns non-zero on success */
int dict_merge (dict_t *dst,
		const dict_t *src);

#if defined (_DEBUG)
void dict_dump (const dict_t *dict);
#endif

C_END

#endif /* _DICT_H defined? */
