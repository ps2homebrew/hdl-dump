/*
 * net_io.h
 * $Id: net_io.h,v 1.7 2005/12/08 20:41:59 bobi Exp $
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

#include "config.h"
#include <stddef.h>

C_START

#define NET_HIO_SERVER_PORT 0xb0b1  /* port where server would listen */

/* commands */
#define CMD_HIO_STAT          0x73746174 /* 'stat'; return HDD unit size in sectors */
#define CMD_HIO_READ          0x72656164 /* 'read'; read sectors from the HDD unit */
#define CMD_HIO_WRITE         0x77726974 /* 'writ'; write sectors to the HDD unit */
#define CMD_HIO_WRITE_NACK    0x7772696e /* 'wrin'; write sectors to the HDD with no ACK */
#define CMD_HIO_WRITE_RACK    0x77726972 /* 'wrir'; send dummy ACK 1st, then write sectors */
#define CMD_HIO_WRITE_QACK    0x77726971 /* 'wriq'; send dummy ACK before decompr. */
#define CMD_HIO_WRITE_STAT    0x77726973 /* 'wris'; what's the status of write operation? (UDP) */
#define CMD_HIO_FLUSH         0x666c7368 /* 'flsh'; flush write buff (svr's hio is persistent) */
#define CMD_HIO_G_TCPNODELAY  0x67746e64 /* 'gtnd'; get remote TCP_NODELAY sock option */
#define CMD_HIO_S_TCPNODELAY  0x73746e64 /* 'stnd'; set remote TCP_NODELAY sock option */
#define CMD_HIO_G_ACKNODELAY  0x67616e64 /* 'gand'; get remote ACK_NODELAY sock option */
#define CMD_HIO_S_ACKNODELAY  0x73616e64 /* 'sand'; set remote ACK_NODELAY sock option */
#define CMD_HIO_POWEROFF      0x706f7778 /* 'powx'; poweroff system */

#define HDD_SECTOR_SIZE 512    /* HDD sector size in bytes */
#define HDD_NUM_SECTORS  32    /* number of sectors to write at once */
#define NET_NUM_SECTORS 2048   /* max number of sectors to transfer via network at once */
#define NET_IO_CMD_LEN (4 * 4) /* command length in bytes in networking I/O */


void rle_compress (const unsigned char *input,
		   u_int32_t ilength,
		   unsigned char *output,
		   u_int32_t *olength); /* should have at least one byte extra for each 128 bytes */

void rle_expand (const unsigned char *input,
		 u_int32_t ilength,
		 unsigned char *output,
		 u_int32_t *olength);

C_END

#endif /* _NET_IO_H defined? */
