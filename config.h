/*
 * config.h
 * $Id: config.h,v 1.15 2007-05-12 20:14:17 bobi Exp $
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

/* MacOS X support patch; there is more in osal_unix.c */
#if defined (__APPLE__) || defined (__FreeBSD__)
#  undef _BUILD_UNIX
#  define _BUILD_UNIX
#  define lseek64 lseek
#  define stat64 stat
#  define open64 open
#  define off64_t off_t
#  define fstat64 fstat
#  define mmap64 mmap
#  define O_LARGEFILE 0
#endif
/* end of MacOS X support patch */

#if !defined (_BUILD_WIN32) && !defined (_BUILD_UNIX) && !defined (_BUILD_PS2)
#  error One of _BUILD_WIN32, _BUILD_UNIX or _BUILD_PS2 should be defined
#endif

#if defined (_BUILD_WIN32)
#  if !defined (_BUILD_WINE)
typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned long u_int32_t;
#    if defined (_MSC_VER) && defined (_WIN32)
typedef unsigned __int64 u_int64_t; /* Microsoft Visual C/C++ compiler */
#    else
typedef unsigned long long u_int64_t; /* GNU C/C++ compiler */
/* typedef signed int ssize_t; => #include <sys/types.h> */
#    endif
#  else
#    include <sys/types.h>
#  endif

#elif defined (_BUILD_UNIX)
#  include <sys/types.h>
#  if defined (_LINT)
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
#  endif

#elif defined (_BUILD_PS2)
#  if defined (_EE)
#    include <tamtypes.h> /* EE */
#  else
#    include <types.h> /* IOP */
#  endif
typedef  u8  u_int8_t;
typedef u16 u_int16_t;
typedef u32 u_int32_t;
typedef u64 u_int64_t;
#endif

#if defined (_BUILD_UNIX)
typedef int SOCKET;
#  define INVALID_SOCKET (-1)
#endif

/* maximum number of compatibility flags (in bits);
   should fit in the following type */
#define MAX_FLAGS 8
typedef unsigned char compat_flags_t;
static const compat_flags_t COMPAT_FLAGS_INVALID = (compat_flags_t) -1;

/* control whether infrequently-used commands to be built */
#define INCLUDE_DUMP_CMD
#define INCLUDE_COMPARE_IIN_CMD
#define INCLUDE_MAP_CMD
#define INCLUDE_INFO_CMD
#define INCLUDE_ZERO_CMD
#define INCLUDE_CUTOUT_CMD
#define INCLUDE_INITIALIZE_CMD
#define INCLUDE_DUMP_MBR_CMD
#define INCLUDE_DELETE_CMD
#define INCLUDE_BACKUP_TOC_CMD
#define INCLUDE_RESTORE_TOC_CMD
#define INCLUDE_DIAG_CMD
#define INCLUDE_MODIFY_CMD
#define INCLUDE_COPY_HDD_CMD
#undef INCLUDE_HIDE_CMD /*Hide function is malfunction*/

/* option names and values for the config file */
#define CONFIG_ENABLE_ASPI_FLAG           "enable_aspi"
#define CONFIG_DISC_DATABASE_FILE         "disc_database_file"
#define CONFIG_LAST_IP                    "last_ip"
#define CONFIG_TARGET_KBPS                "target_kbps"
#define CONFIG_AUTO_THROTTLE              "auto_throttle"


#if defined (__cplusplus)
#  define C_START extern "C" {
#  define C_END } // extern "C"
#else
#  define C_START
#  define C_END
#endif

#endif /* _CONFIG_H defined? */
