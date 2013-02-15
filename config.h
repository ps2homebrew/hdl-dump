/*
 * config.h
 * $Id: config.h,v 1.5 2004/09/12 17:25:26 b081 Exp $
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

#if !defined (_BUILD_WIN32) && !defined (_BUILD_UNIX)
#  error Either _BUILD_WIN32 or _BUILD_UNIX should be defined
#endif


typedef long long bigint_t;

/* control whether infrequently-used commands to be built */
#define INCLUDE_DUMP_CMD
/* #define INCLUDE_COMPARE_CMD */
#define INCLUDE_COMPARE_IIN_CMD
/* #define INCLUDE_MAP_CMD */
/* #define INCLUDE_INFO_CMD */
/* #define INCLUDE_ZERO_CMD */
/* #define INCLUDE_CUTOUT_CMD */
/* #define INCLUDE_READ_TEST_CMD */


#endif /* _CONFIG_H defined? */
