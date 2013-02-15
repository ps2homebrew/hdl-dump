/*
 * dict.c
 * $Id: dict.c,v 1.2 2005/12/08 20:40:11 bobi Exp $
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dict.h"


typedef struct key_value_pair_type
{
  char *key;
  char *value;
} kvpair_t;

struct dict_type
{
  kvpair_t *first;
  size_t alloc, used;
};


#define DICT_GROW_CNT 1


/**************************************************************/
static char*
str_dup (const char *src)
{
  if (src != NULL)
    {
      size_t len = strlen (src);
      char *tmp = malloc (len + 1);
      if (tmp != NULL)
	memcpy (tmp, src, len + 1);
      return (tmp);
    }
  return (NULL);
}


/**************************************************************/
dict_t*
dict_alloc (void)
{
  dict_t *retval = (dict_t*) malloc (sizeof (dict_t));
  if (retval != NULL)
    {
      retval->first = NULL;
      retval->alloc = retval->used = 0;
    }
  return (retval);
}


/**************************************************************/
void
dict_free (dict_t *dict)
{
  if (dict != NULL)
    {
      size_t i;
      for (i = 0; i < dict->used; ++i)
	{
	  kvpair_t *p = dict->first + i;
	  if (p)
	    {
	      if (p->key) free (p->key);
	      if (p->value) free (p->value);
	    }
	}
      free (dict);
    }
}


/**************************************************************/
static int
dict_cmp (const void *ck, const void *ce)
{
  const char *key = (const char*) ck;
  const kvpair_t *elem = (const kvpair_t*) ce;
  return (strcmp (key, elem->key));
}


/**************************************************************/
int
dict_put (dict_t *dict,
	  const char *key,
	  const char *value)
{
  size_t i;
  for (i = 0; i < dict->used; ++i)
    { /* linear search */
      kvpair_t *pair = dict->first + i;
      int result = strcmp (key, pair->key);
      if (result == 0)
	{ /* inplace replace */
	  if (pair->value != NULL)
	    free (pair->value);
	  pair->value = str_dup (value);
	  return (1);
	}
      else if (result < 0)
	break; /* new entry should be inserted at index `i' */
    }

  /* if we're here, a new entry should be created at index `i' */
  if (dict->used == dict->alloc)
    { /* allocate more memory */
      size_t new_cnt = dict->used + DICT_GROW_CNT;
      kvpair_t *tmp = (kvpair_t*) malloc (sizeof (kvpair_t) * new_cnt);
      if (tmp != NULL)
	{
	  if (dict->used > 0)
	    memcpy (tmp, dict->first, sizeof (kvpair_t) * dict->used);
	  dict->first = tmp;
	  dict->alloc = new_cnt;
	}
      else
	return (0); /* out of memory */
    }

  if (i < dict->used)
    memmove (dict->first + i + 1, dict->first + i, sizeof (kvpair_t) * (dict->used - i));
  dict->first[i].key = str_dup (key);
  dict->first[i].value = str_dup (value);
  ++dict->used;

  return (1);
}


/**************************************************************/
int
dict_put_flag (dict_t *dict,
	       const char *key,
	       int value)
{
  return (dict_put (dict, key, value ? "yes" : "no"));
}


/**************************************************************/
int
dict_get_flag (const dict_t *dict,
	       const char *key,
	       int default_value)
{
  const char *value = dict_lookup (dict, key);
  if (value != NULL)
    {
      return (strcmp (value, "yes") == 0 ||
	      strcmp (value, "true") == 0 ||
	      strcmp (value, "1") == 0 ? 1 : 0);
    }
  else
    return (default_value);
}


/**************************************************************/
int
dict_get_numeric (const dict_t *dict,
		  const char *key,
		  int default_value)
{
  const char *value = dict_lookup (dict, key);
  if (value != NULL)
    return (strtol (value, NULL, 0));
  else
    return (default_value);
}


/**************************************************************/
const char*
dict_lookup (const dict_t *dict,
	     const char *key)
{
  void *ret = bsearch (key, dict->first, dict->used,
		       sizeof (kvpair_t), &dict_cmp);
  if (ret != NULL)
    return (((kvpair_t*) ret)->value);
  else
    return (NULL);
}


/**************************************************************/
static int
escape_and_store (FILE *out,
		  const char *p)
{
  static const char *TO_ESCAPE = "\\\"\r\n\t";
  const char *start = p;

  if (fputc ('\"', out) == EOF)
    return (-1);
  if (p != NULL)
    {
      while (*p != '\0')
	{
	  char ch = *p;
	  int to_escape = (strchr (TO_ESCAPE, ch) != NULL);
	  if (to_escape)
	    { /* current character should be escaped */
	      /* flush anything collected so far */
	      if (start != p)
		if (fwrite (start, 1, p - start, out) != p - start)
		  return (-1);
	      if (fputc ('\\', out) == EOF)
		return (-1);
	      switch (*p)
		{
		case '\\': ch = '\\'; break;
		case '\"': ch = '\"'; break;
		case '\r': ch = 'r'; break;
		case '\n': ch = 'n'; break;
		case '\t': ch = 't'; break;
		}
	      if (fputc (ch, out) == EOF)
		return (-1);
	      start = p + 1;
	    }
	  ++p;
	}
    }
  if (start < p)
    if (fwrite (start, 1, p - start, out) != p - start)
      return (-1);
  if (fputc ('\"', out) == EOF)
    return (-1);
  return (0);
}


/**************************************************************/
int
dict_store (const dict_t *dict,
	    const char *filename)
{
  int result = 0;
  FILE *out = fopen (filename, "wb");
  if (out != NULL)
    {
      size_t i;
      for (i = 0; i < dict->used && result == 0; ++i)
	{
	  const kvpair_t *pair = dict->first + i;
	  result = escape_and_store (out, pair->key);
	  if (result == 0)
	    result = fwrite (" = ", 1, 3, out) == 3 ? 0 : -1;
	  if (result == 0)
	    result = escape_and_store (out, pair->value);
	  if (result == 0)
	    result = fputc ('\n', out) != EOF ? 0 : -1;
	}
      fclose (out);
    }
  else
    result = -1;
  return (result);
}


/**************************************************************/
/* 
 * reads a string from an input stream
 * string might be quoted (if first non-white input character is \")
 * otherwise string is `term' terminated; `term' is put back in stream;
 * returns string length in characters
 */
static int
read_and_unescape (FILE *in,
		   char *buffer,
		   size_t size,
		   char term)
{
  int ch, is_quoted = 0;
  char *start = buffer;

  while (isspace (ch = fgetc (in)))
    ; /* skip leading whitespace */

  --size; /* reserve space for terminating zero */
  is_quoted = (ch == '\"');
  while (!ferror (in) && !feof (in) && size > 0)
    {
      ch = fgetc (in);
      if ((is_quoted && ch == '\"') ||
	  (ch == term))
	break; /* end of stream */
      else if (ch != '\\')
	{ /* non-escaped character */
	  *buffer++ = ch;
	  --size;
	}
      else
	{ /* escaped character */
	  ch = fgetc (in);
	  switch (ch)
	    {
	    case '\\': ch = '\\'; break;
	    case '\"': ch = '\"'; break;
	    case 'n':  ch = '\n'; break;
	    case 'r':  ch = '\r'; break;
	    case 't':  ch = '\t'; break;
	    default:
	      if (ch == EOF)
		return (-1);
	    }
	  *buffer++ = ch;
	  --size;
	}
    }
  if (!is_quoted)
    ungetc (ch, in);

  if (!is_quoted)
    { /* trim trailing whitespace when string is unquoted */
      --buffer;
      while (buffer >= start && isspace (*buffer))
	--buffer, ++size;
      ++buffer;
    }
  *buffer = '\0';

  return (buffer - start);
}


/**************************************************************/
dict_t*
dict_restore (dict_t *src,
	      const char *filename)
{
  int init = (src == NULL);
  if (src == NULL)
    src = dict_alloc ();
  if (src != NULL)
    {
      FILE *in = fopen (filename, "rb");
      if (in != NULL)
	{
	  int result = 0;
	  char key[1024], value[1024];
	  while (!ferror (in) && !feof (in) && result >= 0)
	    {
	      result = read_and_unescape (in, key, sizeof (key), '=');
	      if (result > 0)
		{ /* skip equal sign */
		  while (!ferror (in) && !feof (in) && fgetc (in) != '=')
		    ;
		  result = !ferror (in) && !feof (in) ? 0 : -1;
		}
	      if (result == 0)
		result = read_and_unescape (in, value, sizeof (value), '\n');
	      if (result > 0)
		result = dict_put (src, key, value) != 0 ? 0 : -1;
	    }
	  if (!(result >= 0) && init)
	    {
	      dict_free (src);
	      src = NULL;
	    }
	  fclose (in);
	}
    }
  return (src);
}


/**************************************************************/
int
dict_merge (dict_t *dst,
	    const dict_t *src)
{
  int result = 1;
  size_t i;
  for (i = 0; i < src->used && result == 1; ++i)
    result = dict_put (dst, src->first[i].key, src->first[i].value);
  return (result);
}


/**************************************************************/
#if defined (_DEBUG)
void
dict_dump (const dict_t *dict)
{
  size_t i;
  for (i = 0; i < dict->used; ++i)
    {
      const kvpair_t *pair = dict->first + i;
      printf ("\"%s\" = \"%s\"\n", pair->key, pair->value);
    }
}
#endif
