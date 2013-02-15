/*
 * config.h
 * $Id: config.h,v 1.8 2005/02/17 17:51:04 b081 Exp $
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

#if !defined (_CONFIG_H)
#define _CONFIG_H

/* MacOS X support patch */
#if defined (__APPLE__)
#  define _BUILD_UNIX
#  define lseek64 lseek
#  define stat64 stat
#  define open64 open
#  define off64_t off_t
#  define fstat64 fstat
#  define O_LARGEFILE 0
#endif
/* end of MacOS X support patch */

#if !defined (_BUILD_WIN32) && !defined (_BUILD_UNIX) && !defined (_BUILD_PS2)
#  error One of _BUILD_WIN32, _BUILD_UNIX or _BUILD_PS2 should be defined
#endif

#if defined (_BUILD_WIN32)
typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned long u_int32_t;
#  if defined (_MSC_VER) && defined (_WIN32)
typedef unsigned __int64 u_int64_t; /* Microsoft Visual C/C++ compiler */
#  else
typedef unsigned long long u_int64_t; /* GNU C/C++ compiler */
#  endif

#elif defined (_BUILD_UNIX)
#  include <sys/types.h>

#elif defined (_BUILD_PS2)
#  if defined (_EE)
#    include <tamtypes.h>
#  else
#    include <types.h>
#  endif
typedef  u8  u_int8_t;
typedef u16 u_int16_t;
typedef u32 u_int32_t;
typedef u64 u_int64_t;
#endif


/* control whether infrequently-used commands to be built */
#undef INCLUDE_DUMP_CMD
#undef INCLUDE_COMPARE_CMD
#define INCLUDE_COMPARE_IIN_CMD
#define INCLUDE_MAP_CMD
#define INCLUDE_INFO_CMD
#undef INCLUDE_ZERO_CMD
#undef INCLUDE_CUTOUT_CMD
#undef INCLUDE_READ_TEST_CMD
#undef INCLUDE_CHECK_CMD


/*
 * networking options below:
 * - high-level ACK: none, normal, dummy reverse
 * - local TCP_NODELAY: yes/no
 * - remote TCP_NODELAY: yes/no
 * - remote TCP_ACKNODELAY: yes/no
 */

#endif /* _CONFIG_H defined? */
