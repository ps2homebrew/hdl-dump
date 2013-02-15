/*
 * net_io.h
 * $Id: net_io.h,v 1.3 2004/08/15 16:44:19 b081 Exp $
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

#if !defined (_NET_IO_H)
#define _NET_IO_H

#include <stddef.h>


#define NET_HIO_SERVER_PORT 0x8081  /* port where server would listen */

/* commands */
#define CMD_STAT_UNIT    0x73746174 /* 'stat'; return HDD unit size in sectors (512-bytes one) */
#define CMD_READ_SECTOR  0x72656164 /* 'read'; read sectors from the HDD unit */
#define CMD_WRITE_SECTOR 0x77726974 /* 'writ'; write sectors to the HDD unit */
#define CMD_POWEROFF     0x706f7778 /* 'powx'; poweroff system */

#define HDD_SECTOR_SIZE 512    /* HDD sector size in bytes */
#define HDD_NUM_SECTORS  32    /* number of sectors to write at once */
#define NET_NUM_SECTORS  32    /* max number of sectors to transfer via network at once */
#define NET_IO_CMD_LEN (4 * 4) /* command length in bytes in networking I/O */

#if (NET_NUM_SECTORS > 255)
#  error NET_NUM_SECTORS should fit in a byte
#endif


/* pack & unpack doublewords */
unsigned long get_ulong (unsigned char buffer [4]);

void put_ulong (unsigned char buffer [4],
		unsigned long val);

void rle_compress (const unsigned char *input,
		   size_t ilength,
		   unsigned char *output,
		   size_t *olength); /* should have at least one byte extra for each 128 bytes */

void rle_expand (const unsigned char *input,
		 size_t ilength,
		 unsigned char *output,
		 size_t *olength);

#endif /* _NET_IO_H defined? */
