/*
 * retcodes.h
 * $Id: retcodes.h,v 1.6 2004/08/15 16:44:19 b081 Exp $
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

#if !defined (_RETCODES_H)
#define _RETCODES_H


#define RET_ERR         -1 /* error */
#define RET_NO_MEM      -2 /* out-of-memory */

#define RET_OK           0

#define RET_NOT_APA      1 /* not an APA device */
#define RET_NOT_HDL_PART 2 /* not a HD Loader partition */
#define RET_NOT_FOUND    3 /* partition is not found */
#define RET_BAD_FORMAT   4 /* bad device name format */
#define RET_BAD_DEVICE   5 /* unrecognized device */
#define RET_NO_SPACE     6 /* not enough free space */
#define RET_BAD_APA      7 /* something wrong with APA partition */
#define RET_DIFFERENT    8 /* files are different */
#define RET_INTERRUPTED  9 /* operation were interrupted */
#define RET_PART_EXISTS 10 /* partition with such name already exists */
#define RET_BAD_ISOFS   11 /* not an ISO file system */
#define RET_NOT_PS_CDVD 12 /* not a Playstation CD/DVD */
#define RET_BAD_SYSCNF  13 /* system.cnf is not in the expected format */
#define RET_NOT_COMPAT  14 /* iin probe returns "not compatible" */
#define RET_NOT_ALLOWED 15 /* operation is not allowed */
#define RET_BAD_COMPAT  16 /* iin probe is compatible, but the source is bad */
#define RET_SVR_ERR     17 /* server reported error */
#define RET_1ST_LONGER  18 /* compare_iin: first input is longer */
#define RET_2ND_LONGER  19 /* compare_iin: second input is longer */
#define RET_FILE_NOT_FOUND 20 /* pretty obvious */
#define RET_BROKEN_LINK 21 /* missing linked file (to an or CUE for example) */


#endif /* _RETCODES_H defined? */
