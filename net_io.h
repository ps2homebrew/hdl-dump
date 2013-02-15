/*
 * net_io.h
 * $Id: net_io.h,v 1.9 2007-05-12 20:17:30 bobi Exp $
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

#define NET_HIO_SERVER_PORT 12345  /* port where server would listen */

/* commands */
#define CMD_HIO_STAT          0x73746174 /* 'stat'; get HDD size in sectors */
#define CMD_HIO_READ          0x72656164 /* 'read'; read sectors from HDD */
#define CMD_HIO_WRITE         0x77726974 /* 'writ'; write sectors to HDD */
#define CMD_HIO_WRITE_STAT    0x77726973 /* 'wris'; get last write status */
#define CMD_HIO_FLUSH         0x666c7368 /* 'flsh'; flush write buff */
#define CMD_HIO_POWEROFF      0x706f7778 /* 'powx'; poweroff system */

#define HDD_SECTOR_SIZE  512 /* HDD sector size in bytes */
#define HDD_NUM_SECTORS   32 /* number of sectors to write at once */
#define NET_NUM_SECTORS 2048 /* max # of sectors to move via network */
#define NET_IO_CMD_LEN    16 /* command length in bytes in networking I/O */

C_END

#endif /* _NET_IO_H defined? */
