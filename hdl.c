/*
 * hdl.c
 * $Id: hdl.c,v 1.18 2007-05-12 20:15:19 bobi Exp $
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "byteseq.h"
#include "iin.h"
#include "hdl.h"
#include "apa.h"
#include "retcodes.h"
#include "osal.h"
#include "common.h"
#include "hio.h"

#include "kelf.h"
#include "icon.h"

typedef int iconIVECTOR[4];
typedef float iconFVECTOR[4];

typedef struct McIconSys
/*struct mcIcon*/
{
    unsigned char  head[4];
    unsigned short type;
    unsigned short nlOffset;
    unsigned unknown2;
    unsigned trans;
    iconIVECTOR bgCol[4];
    iconFVECTOR lightDir[3];
    iconFVECTOR lightCol[3];
    iconFVECTOR lightAmbient;
    unsigned short title[34];
    unsigned char view[64];
    unsigned char copy[64];
    unsigned char del[64];
    unsigned char unknown3[512];
} McIcon;

struct IconSysData{
	wchar_t title0[HDL_GAME_NAME_MAX+1];
	wchar_t title1[HDL_GAME_NAME_MAX+1];
	unsigned char bgcola;
	unsigned char bgcol0[3];
	unsigned char bgcol1[3];
	unsigned char bgcol2[3];
	unsigned char bgcol3[3];
	float lightdir0[3];
	float lightdir1[3];
	float lightdir2[3];
	unsigned char lightcolamb[3];
	unsigned char lightcol0[3];
	unsigned char lightcol1[3];
	unsigned char lightcol2[3];
	wchar_t uninstallmes0[61];
	wchar_t uninstallmes1[61];
	wchar_t uninstallmes2[61];
};

/*
 * notice: that code would only run on a big-endian machine
 */


static const char *HDL_HDR0 =
"PS2ICON3D";

static const char *HDL_HDR1 =
"BOOT2 = PATINFO\n"
"VER = 1.20\n"
"VMODE = NTSC\n"
"HDDUNITPOWER = NICHDD\n";

static const char *HDL_HDR2 =
"PS2X\n"
"title0 = miniOPL\n"				/* "HD Loader" */
"title1 = %s\n"						/* game title */
"bgcola = %u\n"						/* transparency=64 */
"bgcol0 = %u,%u,%u\n"				/* 22, 47, 92 */
"bgcol1 = %u,%u,%u\n"				/* 3, 10, 28 */
"bgcol2 = %u,%u,%u\n"				/* 3, 10, 28 */
"bgcol3 = %u,%u,%u\n"				/* 22, 47, 92 */
"lightdir0 = %1.4f,%1.4f,%1.4f\n"	/* 0.5, 0.5, 0.5 */
"lightdir1 = %1.4f,%1.4f,%1.4f\n"	/* 0.0, -0.4, -1.0 */
"lightdir2 = %1.4f,%1.4f,%1.4f\n"	/* 0.5, -0.5, 0.5 */
"lightcolamb = %u,%u,%u\n"			/* 31, 31, 31 */
"lightcol0 = %u,%u,%u\n"			/* 62, 62, 55 */
"lightcol1 = %u,%u,%u\n"			/* 33, 42, 64 */
"lightcol2 = %u,%u,%u\n"			/* 18, 18, 49 */
"uninstallmes0 =This will delete the game.\n"
"uninstallmes1 =\n"
"uninstallmes2 =\n";

static const char *HDL_HDR3 =
"title = %s\n"				/* "Game Name" */
"title_id = %s\n"						/* startup */
"title_sub_id = 0\n"						
"release_date = 20000101\n"				
"developer_id =\n"				
"publisher_id = miniOPL\n"				/* hdloader*/
"note =\n"				/* 22, 47, 92 */
"content_web =\n"
"image_topviewflag = 0\n"
"image_type = 0\n"
"image_count = 1\n"
"image_viewsec = 600\n"
"copyright_viewflag = 1\n"
"copyright_imgcount = 1\n"
"genre =\n"
"parental_lock = 1\n"
"effective_date = 0\n"
"expire_date = 0\n"
"violence_flag = 0\n"
"content_type = 255\n"
"content_subtype = 0\n";


/**************************************************************/
static int
prepare_main (const hdl_game_t *details,
	      const apa_toc_t *toc,
	      int slice_index,
	      u_int32_t starting_partition_sector,
	      u_int32_t size_in_kb,
	      /*@out@*/ u_int8_t *buffer_4m)
{
  int result = OSAL_OK;
  u_int32_t i;
  
  const char *patinfo_header = (const char*) hdloader_kelf_header;
  const char *patinfo_footer = (const char*) hdloader_kelf_footer;
  u_int32_t patinfo_header_length = HDLOADER_KELF_HEADER_LEN;
  u_int32_t patinfo_footer_length = HDLOADER_KELF_FOOTER_LEN;
  char *patinfo = NULL;
  u_int32_t patinfo_length=0;
  u_int32_t patinfo_kelf_length = KELF_LENGTH;
  
  char *icon = NULL;
  u_int32_t icon_length=0;
  char *iconsys = NULL;
  u_int32_t iconsys_length=0;

  char icon_props[1024];
  const ps2_partition_header_t *part;
  const apa_slice_t *slice = toc->slice + slice_index;

  FILE *out = fopen ("./info.sys", "wb");
  if (out != NULL)
	{
	  char info_props[1024];
	  sprintf(info_props, HDL_HDR3, details->name, details->startup);
	  fwrite (info_props, 1, strlen (info_props), out);
	}
  (void) fclose (out);
	
  part = NULL;

  for (i = 0; i < slice->part_count; ++i)
	if (get_u32 (&slice->parts[i].header.start) == starting_partition_sector)
	{ /* locate starting partition index */
	  part = &slice->parts[i].header;
	  break;
	}

  if (part == NULL)
    result = RET_NOT_FOUND;

  if (result == OSAL_OK && part != NULL)
    {
      const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
      u_int32_t *tmp;
      u_int32_t offset;
      u_int32_t size_remaining_in_kb;
      u_int32_t partition_usable_size_in_kb;
      u_int32_t partition_data_len_in_kb;
      u_int32_t partitions_used = 0;
      u_int32_t sector;

	  /* read miniopl (optional) */
	  result = read_file ("./boot.elf", &patinfo, &patinfo_length);
	  if (result != OSAL_OK)
	  {
		patinfo = NULL;
		patinfo_length = 0;
		result = OSAL_OK;
	  }

	  /* read icon (optional if skipped - used hdloader icon) */
	  result = read_file ("./list.ico", &icon, &icon_length);
	  if (result != OSAL_OK)
	  {
		icon = (char*) hdloader_icon;
		icon_length = HDLOADER_ICON_LEN;
		result = OSAL_OK;
	  }  

      /* PS2 partition header */
      memset (buffer_4m, 0, 4 * 1024 * 1024);
      memcpy (buffer_4m, part, 1024);
	  
	  /* read icon.sys in memory card format
	   * or in hdd format
	   * if in another format or skipped used
	   * default data for HDLoader icon 
	   */
	  result = read_file ("./icon.sys", &iconsys, &iconsys_length);
	  if ((result == OSAL_OK) && (!strncmp(iconsys, "PS2D", 4)))
	  {
		McIcon *mcIcon_details;
		mcIcon_details=(McIcon*)iconsys;
		sprintf(icon_props, HDL_HDR2,
		details->name,
		mcIcon_details->trans,
		mcIcon_details->bgCol[0][0]/2, mcIcon_details->bgCol[0][1]/2, mcIcon_details->bgCol[0][2]/2,
		mcIcon_details->bgCol[1][0]/2, mcIcon_details->bgCol[1][1]/2, mcIcon_details->bgCol[1][2]/2,
		mcIcon_details->bgCol[2][0]/2, mcIcon_details->bgCol[2][1]/2, mcIcon_details->bgCol[2][2]/2,
		mcIcon_details->bgCol[3][0]/2, mcIcon_details->bgCol[3][1]/2, mcIcon_details->bgCol[3][2]/2,
		mcIcon_details->lightDir[0][0], mcIcon_details->lightDir[0][1], mcIcon_details->lightDir[0][2],
		mcIcon_details->lightDir[1][0], mcIcon_details->lightDir[1][1], mcIcon_details->lightDir[1][2],
		mcIcon_details->lightDir[2][0], mcIcon_details->lightDir[2][1], mcIcon_details->lightDir[2][2],
		(unsigned char)(mcIcon_details->lightAmbient[0]*128),
		(unsigned char)(mcIcon_details->lightAmbient[1]*128),
		(unsigned char)(mcIcon_details->lightAmbient[2]*128),
		(unsigned char)(mcIcon_details->lightCol[0][0]*128),
		(unsigned char)(mcIcon_details->lightCol[0][1]*128),
		(unsigned char)(mcIcon_details->lightCol[1][0]*128),
		(unsigned char)(mcIcon_details->lightCol[1][1]*128),
		(unsigned char)(mcIcon_details->lightCol[1][2]*128),
		(unsigned char)(mcIcon_details->lightCol[2][0]*128),
		(unsigned char)(mcIcon_details->lightCol[2][1]*128),
		(unsigned char)(mcIcon_details->lightCol[2][2]*128)	);
/*		McIconSys*/
	  }
	  else if ((result == OSAL_OK) && (!strncmp(iconsys, "PS2X", 4)))
	  {
		strcpy (icon_props,iconsys);
	  }
	  else
	  {
		sprintf (icon_props, HDL_HDR2, details->name, 64,
		  22,47,92, 3,10,28, 3,10,28, 22,47,92,
		  0.5,0.5,0.5, 0.0,-0.4,-1.0, 0.5,-0.5,0.5,
		  31,31,31, 62,62,55, 33,42,64, 18,18,49);
		result = OSAL_OK;
	  }

      /*
       *  1000: 50 53 32 49 43 4f 4e 33  44 00 00 00 00 00 00 00  PS2ICON3D.......
       */
      memcpy (buffer_4m + 0x001000, HDL_HDR0, strlen (HDL_HDR0));

      /*
	   *         +--+--+--+- system.cnf offset relative to 0x1000
       *         |  |  |  |  +--+--+--+- system.cnf section length in bytes
       *         |  |  |  |  |  |  |  |   +--+--+--+- icon.sys offset relative to 0x1000
       *         |  |  |  |  |  |  |  |   |  |  |  |  +--+--+--+- icon.sys length in bytes
       *         v  v  v  v  v  v  v  v   v  v  v  v  v  v  v  v
       *  1010: 00 02 00 00 4b 00 00 00  00 04 00 00 70 01 00 00
       *  1020: 00 08 00 00 58 81 00 00  00 08 00 00 58 81 00 00
       *         ^  ^  ^  ^  ^  ^  ^  ^   ^  ^  ^  ^  ^  ^  ^  ^
       *         |  |  |  |  |  |  |  |   |  |  |  |  +--+--+--+- delete icon length in bytes
       *         |  |  |  |  |  |  |  |   +--+--+--+- delete icon offset relative to 0x1000
       *         |  |  |  |  +--+--+--+- main icon length in bytes
       *         +--+--+--+- main icon offset relative to 0x1000
       */
      tmp = (u_int32_t*) (buffer_4m + 0x001010);
      set_u32 (tmp++, 0x0200);
      set_u32 (tmp++, strlen (HDL_HDR1));
      set_u32 (tmp++, 0x0400);
      set_u32 (tmp++, strlen (icon_props));
      set_u32 (tmp++, 0x0800);
      set_u32 (tmp++, icon_length);
      set_u32 (tmp++, 0x0800);
      set_u32 (tmp++, icon_length);
	  
	  /*  
       *  1030: 00 00 11 00 58 81 00 00  00 00 00 00 00 00 00 00
       *         ^  ^  ^  ^  ^  ^  ^  ^
       *         |  |  |  |  +--+--+--+- BE, PATINFO.KELF length in bytes
       *         +--+--+--+- PATINFO.KELF offset relative to 0x1000
       */
	  if (patinfo_length != 0)/* no boot.elf */
	  {
		set_u32 (tmp++, 0x110000);
		set_u32 (tmp++, patinfo_kelf_length);
	  }

      /*
       *  1200: 42 4f 4f 54 32 20 3d 20 50 41 54 49 4e 46 4f 0a  BOOT2 = PATINFO.
       *  1210: 56 45 52 20 3D 20 31 2E 30 30 0A 56 4D 4F 44 45  VER = 1.00.VMODE
       *  1220: 20 3d 20 50 41 4c 0a 48 44 44 55 4e 49 54 50 4f   = PAL.HDDUNITPO
       *  1230: 57 45 52 20 3d 20 4e 49 43 48 44 44 0a 00 00 00  WER = NICHDD....
       */
      memcpy (buffer_4m + 0x001200, HDL_HDR1, strlen (HDL_HDR1));

      /*
       *  1400: 50 53 32 58 0a 74 69 74  6c 65 30 20 3d 20 48 44  PS2X.title0 = HD
       *  1410: 20 4c 6f 61 64 65 72 0a  74 69 74 6c 65 31 20 3d   Loader.title1 =
       *  1420: 20 54 77 69 73 74 65 64  20 4d 65 74 61 6c 3a 20   Twisted Metal: 
       *        ...
       */
      memcpy (buffer_4m + 0x001400, icon_props, strlen (icon_props));

      /*
       *  1800: 00 00 01 00 01 00 00 00  07 00 00 00 00 00 80 3f  ...............?
       *        ... icon ...
       *  9950: 00 04 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
       */
      memcpy (buffer_4m + 0x001800, icon, icon_length);

      /*
       *101000: ed fe ad de 00 00 01 00  54 77 69 73 74 65 64 20  ........Twisted 
       *101010: 4d 65 74 61 6c 3a 20 42  6c 61 63 6b 00 00 00 00  Metal: Black....
       */
      set_u32 (buffer_4m + 0x101000, 0xdeadfeed);
      buffer_4m[0x101006] = 0x01;
      memcpy (buffer_4m + 0x101008, details->name, strlen (details->name));

      /*
       *                                  +- compatibility modes
       *                                  v
       *1010a0: 00 00 00 00 00 00 00 00  00 00 00 00 53 43 45 53  ............SCES
       *1010b0: 5f 35 30 33 2e 36 30 00  00 00 00 00 00 00 00 00  _503.60.........
       */
      set_u8(buffer_4m + 0x1010a9, (u_int8_t) details->compat_flags);
      set_u16(buffer_4m + 0x1010aa, (u_int16_t) details->dma);
      memcpy (buffer_4m + 0x1010ac, details->startup, strlen (details->startup));

      /*
       *	 +- DVD9 layer break offset
       *	 |           +- media type? 0x14 for DVD, 0x12 for CD, 0x10 for PSX CD (w/o audio)?
       *	 |           |           +- number of partitions
 	   *     v           v           v
   *1010e4: xx xx xx xx 14 00 00 00 05 00 00 00
       *    00 00 00 00 00 20 50 00 00 00 c0 1f	;; 0x00500000, 512MB
       *	00 f8 03 00 00 08 60 00 00 00 f0 1f	;; 0x00600000, 512MB
       *	00 f6 07 00 00 08 70 00 00 00 f0 1f	;; 0x00700000, 512MB
       *	00 f4 0b 00 00 08 44 00 00 00 f0 07	;; 0x00440000, 128MB
       *	00 f2 0c 00 00 08 48 00 00 00 25 01	;; 0x00480000, 128MB
       *	    ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^
       *	    +--+--+--+  +--+--+--+  +--+--+- BE, x / 4 = part size in kilobytes[!]
       *	             |           +- x << 8 = data start in HDD-sectors
       *	             +- BE, x * 512 = part offset in megabytes, incremental counter[!]
       */

      set_u32 (buffer_4m + 0x1010e8, details->layer_break);
	  set_u32 (buffer_4m + 0x1010ec, details->is_dvd ? 0x00000014 : 0x00000012);
      /* that is aerial acrobatics :-) */
      buffer_4m[0x1010f0] = (u_int8_t) (get_u32 (&part->nsub) + 1);
      tmp = (u_int32_t*) (buffer_4m + 0x1010f5);

      /*
       *111000: 01 00 00 01 00 03 00 4a  00 01 02 19 00 00 00 56 
       *        ... PATINFO.KELF ...
       *179640: 7e bd 13 b2 4e 1f 26 08  29 53 97 37 13 c3 71 1c 
       */
	  if (patinfo_length != 0)
	  {
		memcpy (buffer_4m + 0x111000, patinfo_header, patinfo_header_length);
		memcpy (buffer_4m + 0x111000 + patinfo_header_length, patinfo, patinfo_length);
		memcpy (buffer_4m + 0x111000 + patinfo_kelf_length - patinfo_footer_length, patinfo_footer, patinfo_footer_length);
	  }

      partition_usable_size_in_kb = (get_u32 (&part->length) - 0x2000) / 2; /* 2 sectors == 1K */
      partition_data_len_in_kb =
	size_in_kb < partition_usable_size_in_kb ?
	size_in_kb : partition_usable_size_in_kb;

      /* offset, start, data length */
      set_u32 (tmp++, 0); /* offset */
      /* 0x2000 sec = 4M (offs for 1st part.) */
      sector = get_u32 (&part->start) + 0x2000 + SLICE_2_OFFS * slice_index;
      set_u32 (tmp++, sector >> 8);
      set_u32 (tmp++, partition_data_len_in_kb * 4);
      ++partitions_used;

      offset = (get_u32 (&part->length) - 0x2000) / 1024;
      size_remaining_in_kb = size_in_kb - partition_data_len_in_kb;
      for (i=0; size_remaining_in_kb>0 && i<get_u32 (&part->nsub); ++i)
	  {
		  partition_usable_size_in_kb =
			(get_u32 (&part->subs[i].length) - 0x0800) / 2;
		  partition_data_len_in_kb =
			size_remaining_in_kb < partition_usable_size_in_kb ?
			size_remaining_in_kb : partition_usable_size_in_kb;

		  /* offset, start, data length */
		  set_u32 (tmp++, offset);
		  /* 0x0800 sec = 1M (sub part. offs) */
		  sector = (get_u32 (&part->subs[i].start) + 0x0800 +
				SLICE_2_OFFS * slice_index);
		  set_u32 (tmp++, sector >> 8);
		  set_u32 (tmp++, partition_data_len_in_kb * 4);
		  ++partitions_used;

		  offset += (get_u32 (&part->subs[i].length) - 0x0800) / 1024;
		  size_remaining_in_kb -= partition_data_len_in_kb;
	  }

      buffer_4m[0x1010f0] = (u_int8_t) partitions_used;
    }

  if (icon != NULL)
    osal_free (icon);

  if (patinfo != NULL)
    osal_free (patinfo);
	
  if (iconsys != NULL)
    osal_free (iconsys);

  return (result);
}


/**************************************************************/
int
hdd_inject_header (hio_t *hio,
	      apa_toc_t *toc,
	      int slice_index,
	      u_int32_t starting_partition_sector)
{
  int result;
  
  const char *patinfo_header = (const char*) hdloader_kelf_header;
  const char *patinfo_footer = (const char*) hdloader_kelf_footer;
  u_int32_t patinfo_header_length = HDLOADER_KELF_HEADER_LEN;
  u_int32_t patinfo_footer_length = HDLOADER_KELF_FOOTER_LEN;
  char *patinfo = NULL;
  u_int32_t patinfo_length=0;
  u_int32_t patinfo_kelf_length = KELF_LENGTH;
  
  char *syscnf = NULL;
  u_int32_t syscnf_length;
  char *iconsys = NULL;
  u_int32_t iconsys_length;
  char *icon = NULL;
  u_int32_t icon_length;
  char *del = NULL;
  u_int32_t del_length;
  char *kirx = NULL;
  u_int32_t kirx_length=0;

  apa_partition_t *part = NULL;
  const apa_slice_t *slice = toc->slice + slice_index;
  const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
  u_int32_t sector, dummy = 0;
  char icon_props[1024];

  u_int32_t i;
  char *buffer_4m;
  for (i = 0; part == NULL && i < slice->part_count; ++i)
	if (get_u32 (&slice->parts[i].header.start) == starting_partition_sector)
	  /* starting partition index located */
	  part = slice->parts + i;
  if (part == NULL)
	result = RET_NOT_FOUND;

  buffer_4m = osal_alloc (4 _MB + 24576);
  if (buffer_4m == NULL)
	result = RET_NO_MEM;

  sector = (get_u32 (&part->header.start) + slice_index * SLICE_2_OFFS);
  result = hio->read (hio, sector, (4 _MB + 24576) / 512, buffer_4m, &dummy);

  if (result == RET_OK && part != NULL && buffer_4m != NULL)
    {
      /*
       *  1000: 50 53 32 49 43 4f 4e 33  44 00 00 00 00 00 00 00  PS2ICON3D.......
       *
	   *         +--+--+--+- system.cnf offset relative to 0x1000
       *         |  |  |  |  +--+--+--+- system.cnf section length in bytes
       *         |  |  |  |  |  |  |  |   +--+--+--+- icon.sys offset relative to 0x1000
       *         |  |  |  |  |  |  |  |   |  |  |  |  +--+--+--+- icon.sys len in bytes
       *         v  v  v  v  v  v  v  v   v  v  v  v  v  v  v  v
       *  1010: 00 02 00 00 4b 00 00 00  00 04 00 00 70 01 00 00
       *  1020: 00 08 00 00 58 81 00 00  00 08 00 00 58 81 00 00
       *         ^  ^  ^  ^  ^  ^  ^  ^   ^  ^  ^  ^  ^  ^  ^  ^
       *         |  |  |  |  |  |  |  |   |  |  |  |  +--+--+--+- delete icon length in bytes
       *         |  |  |  |  |  |  |  |   +--+--+--+- delete icon offset relative to 0x1000
       *         |  |  |  |  +--+--+--+- main icon length in bytes
       *         +--+--+--+- main icon offset relative to 0x1000
       *  1030: 00 00 11 00 58 81 00 00  00 00 00 00 00 00 00 00
       *         ^  ^  ^  ^  ^  ^  ^  ^   ^  ^  ^  ^  ^  ^  ^  ^
       *         |  |  |  |  |  |  |  |   |  |  |  |  +--+--+--+- PATINFO.KIRX length in bytes
       *         |  |  |  |  |  |  |  |   +--+--+--+- PATINFO.KIRX offset relative to 0x1000 (optional)
       *         |  |  |  |  |  |  |  |   
       *         |  |  |  |  +--+--+--+- PATINFO.KELF length in bytes
       *         +--+--+--+- PATINFO.KELF offset relative to 0x1000 (optional)
       */
      memcpy (buffer_4m + 0x001000, HDL_HDR0, strlen (HDL_HDR0));

      /* system.cnf
       *  1200: 42 4f 4f 54 32 20 3d 20 50 41 54 49 4e 46 4f 0a  BOOT2 = PATINFO.
       *  1210: 56 45 52 20 3D 20 31 2E 30 30 0A 56 4D 4F 44 45  VER = 1.00.VMODE
       *  1220: 20 3d 20 50 41 4c 0a 48 44 44 55 4e 49 54 50 4f   = PAL.HDDUNITPO
       *  1230: 57 45 52 20 3d 20 4e 49 43 48 44 44 0a 00 00 00  WER = NICHDD....
       */

	  result = read_file ("./system.cnf", &syscnf, &syscnf_length);
	  if (result == OSAL_OK)
	  {	  
		if (syscnf_length > 0x200)
		  fprintf (stdout, "system.cnf is larger than 512 Kbytes\n");
	    else
		{
		  set_u32 (buffer_4m + 0x001010, 0x0200);
		  set_u32 (buffer_4m + 0x001014, syscnf_length);
		  memcpy (buffer_4m + 0x001200, syscnf, syscnf_length);
		  fprintf (stdout, "Succesfully read system.cnf\n");
		}
	  }
	  else
	  {
		fprintf (stdout, "Skipped system.cnf\n");
	  }
	  if (syscnf != NULL)
		osal_free (syscnf); 

      /* icon.sys (HDD format) auto converted from MC format
       *  1400: 50 53 32 58 0a 74 69 74  6c 65 30 20 3d 20 48 44  PS2X.title0 = HD
       *  1410: 20 4c 6f 61 64 65 72 0a  74 69 74 6c 65 31 20 3d   Loader.title1 =
       *  1420: 20 54 77 69 73 74 65 64  20 4d 65 74 61 6c 3a 20   Twisted Metal: 
       *        ...
       */

	  result = read_file ("./icon.sys", &iconsys, &iconsys_length);
	  if ((result == OSAL_OK) && (!strncmp(iconsys, "PS2X", 4)))		  
	  {
		if (iconsys_length > 0x200)
		  fprintf (stdout, "icon.sys is larger than 512 Kbytes\n");
	    else
		{
		  set_u32 (buffer_4m + 0x001018, 0x0400);
		  set_u32 (buffer_4m + 0x00101C, iconsys_length);
		  memcpy (buffer_4m + 0x001400, iconsys, iconsys_length);
		  fprintf (stdout, "Succesfully read icon.sys\n");
		}
	  }
	  else if ((result == OSAL_OK) && (!strncmp(iconsys, "PS2D", 4)))
	  {
		McIcon *mcIcon_details;
		mcIcon_details=(McIcon*)iconsys;
		sprintf(icon_props, HDL_HDR2,
		"HDLoader game",
		mcIcon_details->trans,
		mcIcon_details->bgCol[0][0]/2, mcIcon_details->bgCol[0][1]/2, mcIcon_details->bgCol[0][2]/2,
		mcIcon_details->bgCol[1][0]/2, mcIcon_details->bgCol[1][1]/2, mcIcon_details->bgCol[1][2]/2,
		mcIcon_details->bgCol[2][0]/2, mcIcon_details->bgCol[2][1]/2, mcIcon_details->bgCol[2][2]/2,
		mcIcon_details->bgCol[3][0]/2, mcIcon_details->bgCol[3][1]/2, mcIcon_details->bgCol[3][2]/2,
		mcIcon_details->lightDir[0][0], mcIcon_details->lightDir[0][1], mcIcon_details->lightDir[0][2],
		mcIcon_details->lightDir[1][0], mcIcon_details->lightDir[1][1], mcIcon_details->lightDir[1][2],
		mcIcon_details->lightDir[2][0], mcIcon_details->lightDir[2][1], mcIcon_details->lightDir[2][2],
		(unsigned char)(mcIcon_details->lightAmbient[0]*128),
		(unsigned char)(mcIcon_details->lightAmbient[1]*128),
		(unsigned char)(mcIcon_details->lightAmbient[2]*128),
		(unsigned char)(mcIcon_details->lightCol[0][0]*128),
		(unsigned char)(mcIcon_details->lightCol[0][1]*128),
		(unsigned char)(mcIcon_details->lightCol[1][0]*128),
		(unsigned char)(mcIcon_details->lightCol[1][1]*128),
		(unsigned char)(mcIcon_details->lightCol[1][2]*128),
		(unsigned char)(mcIcon_details->lightCol[2][0]*128),
		(unsigned char)(mcIcon_details->lightCol[2][1]*128),
		(unsigned char)(mcIcon_details->lightCol[2][2]*128)	);
		/* McIconSys*/
		set_u32 (buffer_4m + 0x001018, 0x0400);
		set_u32 (buffer_4m + 0x00101C, strlen (icon_props));
		memcpy (buffer_4m + 0x001400, icon_props, strlen (icon_props));
		fprintf (stdout, "Succesfully converted icon.sys into HDD format\n");		
	  }
	  else
	  {
		fprintf (stdout, "Skipped icon.sys\n");
	  }
	  if (iconsys != NULL)
		osal_free (iconsys); 

      /*
       *  1800: 00 00 01 00 01 00 00 00  07 00 00 00 00 00 80 3f  ...............?
       *        ... main icon ...
       *  9950: 00 04 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
       */

	  result = read_file ("./list.ico", &icon, &icon_length);
	  if (result == OSAL_OK)
	  {		  
		if (icon_length > 0x3F800)
		  fprintf (stdout, "list.ico is larger than 260 096 Kbytes\n");
	    else
		{
		  set_u32 (buffer_4m + 0x001020, 0x0800);
		  set_u32 (buffer_4m + 0x001024, icon_length);
		  set_u32 (buffer_4m + 0x001028, 0x0800);
		  set_u32 (buffer_4m + 0x00102C, icon_length);
		  memcpy (buffer_4m + 0x001800, icon, icon_length);
		  fprintf (stdout, "Succesfully read list.ico\n");
		}
	  }
	  else
	  {
		fprintf (stdout, "Skipped list.ico\n");
	  }
	  if (icon != NULL)
		osal_free (icon);

      /*
       *  41000: 00 00 01 00 01 00 00 00  07 00 00 00 00 00 80 3f  ...............?
       *        ... delete icon ...
       *  49150: 00 04 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
       */

	  result = read_file ("./del.ico", &del, &del_length);
	  if (result == OSAL_OK)
	  {
		if (del_length > 0xC0000)
		  fprintf (stdout, "del.ico is larger than 786 432 Kbytes\n");
	    else
		{
		  set_u32 (buffer_4m + 0x001028, 0x040000);
		  set_u32 (buffer_4m + 0x00102C, del_length);
		  memcpy (buffer_4m + 0x041000, del, del_length);
		  fprintf (stdout, "Succesfully read del.ico\n");
		}
	  }
	  else
	  {
		fprintf (stdout, "Skipped del.ico\n");
	  }
	  if (del != NULL)
		osal_free (del);

	  /*
       *  111000: 01 00 00 01 00 03 00 4a  00 01 02 19 00 00 00 56 
       *        ... PATINFO.KELF ...
       *  179640: 7e bd 13 b2 4e 1f 26 08  29 53 97 37 13 c3 71 1c 
       *
	   * read kelf or elf 
	   */

	  fprintf (stdout, "Skipped boot.kelf. Trying to inject boot.elf\n");
	  if (patinfo != NULL)
		osal_free (patinfo);

	  result = read_file ("./boot.elf", &patinfo, &patinfo_length);
	  if (result == OSAL_OK)
	  {
		if (patinfo_length > 0x1EEBE0)
		  fprintf (stdout, "boot.elf is larger than 2 026 464 Kbytes\n");
	    else
		{
		  set_u32 (buffer_4m + 0x001030, 0x110000);
		  set_u32 (buffer_4m + 0x001034, patinfo_kelf_length);
		  memcpy (buffer_4m + 0x111000, patinfo_header, patinfo_header_length);
		  memcpy (buffer_4m + 0x111000 + patinfo_header_length, patinfo, patinfo_length);
		  memcpy (buffer_4m + 0x111000 + patinfo_kelf_length - patinfo_footer_length, patinfo_footer, patinfo_footer_length);
		  fprintf (stdout, "Succesfully read boot.elf\n");
		}
	  }
	  else
	  {
		fprintf (stdout, "Skipped boot.elf\n");
	  }
	  if (patinfo != NULL)
		osal_free (patinfo);

	  result = read_file ("./boot.kelf", &patinfo, &patinfo_length);
	  if (result == OSAL_OK)
	  {
		if (patinfo_length > 0x1F0000)
		  fprintf (stdout, "boot.kelf is larger than 2 031 616 Kbytes\n");
	    else
		{
		  if (patinfo_length != 0)
		  {
			set_u32 (buffer_4m + 0x001030, 0x110000);
			set_u32 (buffer_4m + 0x001034, patinfo_length);
			memcpy (buffer_4m + 0x111000, patinfo, patinfo_length);
			fprintf (stdout, "Succesfully read boot.kelf\n");
		  } else {
		/* if want to completely remove kelf - we need zero-sized boot.kelf 
		 * For some reason HDD Browser 2.00 ignores system.cnf boot2
		 * and loads boot.kelf if it is not zerosized 
		 */
		  set_u32 (buffer_4m + 0x001030, 0x000000);
		  set_u32 (buffer_4m + 0x001034, patinfo_length);
		  fprintf (stdout, "Boot.kelf was %lui-sized - clear all patinfo data\n",patinfo_length);
		  }
		}
	  }
	  if (patinfo != NULL)
		osal_free (patinfo);
	
	  /*
       *  301000: 
       *        ... PATINFO.KIRX ...
	   *  400000:
	   */
	  result = read_file ("./boot.kirx", &kirx, &kirx_length);
	  if (result == OSAL_OK)
	  {
		if (kirx_length > 0xFF000)
		  fprintf (stdout, "boot.kirx is larger than 1 044 480 Kbytes\n");
	    else
		{
		  if (kirx_length != 0)
		  {
			set_u32 (buffer_4m + 0x001038, 0x300000);
			set_u32 (buffer_4m + 0x00103C, kirx_length);
			memcpy (buffer_4m + 0x301000, kirx, kirx_length);
			fprintf (stdout, "Succesfully read boot.kirx\n");
		  } else {
			set_u32 (buffer_4m + 0x001038, 0x000000);
			set_u32 (buffer_4m + 0x00103C, kirx_length);
			fprintf (stdout, "Boot.kirx was zero-sized - clear all kirx data\n");
		  }
		}
	  }
	  else
	  {
		fprintf (stdout, "Skipped boot.kirx\n");
	  }
	  if (kirx != NULL)
		osal_free (kirx);
	}
  result = hio->write (hio, sector, (4 _MB + 24576) / 512, buffer_4m, &dummy);
  if (result == RET_OK)
	result = apa_commit_ex (hio, toc);
  if (buffer_4m != NULL)
    osal_free (buffer_4m);

  return (result);
}


/**************************************************************/
void
hdl_pname (const char *startup_name, const char *name,
	   char partition_name[PS2_PART_IDMAX + 1])
{
  u_int32_t game_name_len = 0;
  char *p;

  game_name_len = strlen (name) < game_name_len ? strlen (name) : game_name_len;
  if (name[0] != '_' && name[1] != '_')
  {
	game_name_len = PS2_PART_IDMAX - 1 - 3 - 10 - 5; /* limit partition name length */
	strcpy (partition_name, "PP.");
	memmove (partition_name + 3, "SLUS-00000", 10);/*if startup file name absent*/  
	if (startup_name != NULL) {
	  memmove (partition_name + 3, startup_name, 4);/*we will copy first 4 symbols*/
	  partition_name[8] = startup_name[5];
	  partition_name[9] = startup_name[6];
	  partition_name[10] = startup_name[7];
	  partition_name[11] = startup_name[9];
	  partition_name[12] = startup_name[10];
	  /*we will copy last 5 digits*/
	}
	memmove (partition_name + 13, ".HDL.", 5);
	memmove (partition_name + 18, name, game_name_len);
	partition_name[18 + game_name_len] = '\0';
	p = partition_name + 18; /* len ("PP.XXXX-xxxx.HDL.") */
  } else {
	game_name_len = PS2_PART_IDMAX - 1;
	memmove (partition_name, name, game_name_len); /* __.linux.nr */
  }
  while (*p != '\0')
    {
      if (!isalnum (*p) && *p != ' ' && *p != '.')
	*p = '_'; /* escape non-alphanumeric characters with `_' */
      ++p;
    }
}


/**************************************************************/
int
hdl_extract_ex (hio_t *hio,
		const char *game_name,
		const char *output_file,
		progress_t *pgs)
{
  /*@only@*/ apa_toc_t *toc = NULL;
  int result = apa_toc_read_ex (hio, &toc);
  if (result == RET_OK && toc != NULL)
    {
      int slice_index = -1;
      u_int32_t partition_index;
      result = apa_find_partition (toc, game_name, &slice_index,
				   &partition_index);
      if (result == RET_NOT_FOUND)
	{ /* assume it is `game_name' and not a partition name */
	  char partition_id[PS2_PART_IDMAX + 8];
	  result = hdl_lookup_partition_ex (hio, game_name, partition_id);
	  if (result == RET_OK)
	    result = apa_find_partition (toc, partition_id,
					 &slice_index, &partition_index);
	}

      if (result == RET_OK)
	{ /* partition found */
	  hdl_game_alloc_table_t gat;
	  unsigned char *buffer =
	    osal_alloc ((IIN_NUM_SECTORS + 1) * IIN_SECTOR_SIZE);
	  result = (buffer != NULL ? RET_OK : RET_NO_MEM);
	  if (result == RET_OK)
	    {
	      result = hdl_read_game_alloc_table (hio, toc, slice_index,
						  partition_index, &gat);
	      if (result == RET_OK)
		{
		  /*@only@*/ osal_handle_t file = OSAL_HANDLE_INIT;
		  u_int64_t total_size = (u_int64_t) gat.size_in_kb * 1024;

		  pgs_prepare (pgs, total_size);

		  /* create file and copy data */
		  result = osal_create_file (output_file, &file, total_size);
		  if (result == OSAL_OK)
		    {
		      u_int32_t i;
		      u_int8_t *aligned =
			(void*) (((long) buffer + 511) & ~511);
		      for (i = 0; result == OSAL_OK && i < gat.count; ++i)
			{ /* seek and copy */
			  u_int32_t start_s = gat.part[i].start;
			  u_int32_t length_s = gat.part[i].len;
			  u_int64_t curr = 0;
			  while (length_s > 0 && result == OSAL_OK)
			    {
			      u_int32_t len;
			      u_int32_t count_s = (length_s < IIN_NUM_SECTORS ?
						   length_s : IIN_NUM_SECTORS);
			      result = hio->read (hio, start_s, count_s,
						  aligned, &len);
			      if (result == OSAL_OK)
				{
				  u_int32_t written;
				  result = osal_write (file, aligned,
						       count_s * 512,
						       &written);
				  if (result == OSAL_OK)
				    {
				      start_s += count_s;
				      length_s -= count_s;
				      curr += count_s * 512;
				      result = pgs_update (pgs, curr);
				    }
				}
			    }
			  pgs_chunk_complete (pgs);
			} /* for */
		      result = (osal_close (&file) == OSAL_OK ?
				result : OSAL_ERR);
		    }
		}
	    }
	  if (buffer != NULL)
	    osal_free (buffer);
	}
      apa_toc_free (toc);
    }
  return (result);
}


/**************************************************************/
int
hdl_extract (const dict_t *config,
	     const char *device_name,
	     const char *game_name,
	     const char *output_file,
	     progress_t *pgs)
{
  /*@only@*/ hio_t *hio = NULL;
  int result = hio_probe (config, device_name, &hio);
  if (result == OSAL_OK && hio != NULL)
    {
      result = hdl_extract_ex (hio, game_name, output_file, pgs);
      (void) hio->close (hio), hio = NULL;
    }
  return (result);
}


/**************************************************************/
static int
inject_data (hio_t *hio,
	     const apa_toc_t *toc,
	     int slice_index,
	     u_int32_t starting_partition_sector,
	     iin_t *iin,
	     u_int32_t size_in_kb,
	     const hdl_game_t *details,
	     progress_t *pgs)
{
  int result;
  u_int32_t i;
  const ps2_partition_header_t *part;
  const apa_slice_t *slice = toc->slice + slice_index;
  char *buffer;

  part = NULL;
  result = RET_NOT_FOUND;
  for (i = 0; i < slice->part_count; ++i)
    if (get_u32 (&slice->parts[i].header.start) == starting_partition_sector)
      { /* locate starting partition index */
	part = &slice->parts[i].header;
	result = RET_OK;
	break;
      }
  if (part == NULL)
    return (RET_INVARIANT);

  buffer = osal_alloc (4 _MB);
  if (buffer == NULL)
    result = RET_NO_MEM;
  if (result == RET_OK && part != NULL && buffer != NULL)
    {
      const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
      result = prepare_main (details, toc, slice_index,
			     starting_partition_sector,
			     size_in_kb, (u_int8_t*) buffer);
      if (result == RET_OK)
	{
	  u_int32_t kb_remaining = size_in_kb;
	  u_int32_t offset = 0;
	  u_int32_t bytes;
	  u_int32_t sector;
	  const u_int32_t MAIN_HDR_SIZE_S = (4 _MB) / 512;

	  pgs_prepare (pgs, (u_int64_t) (size_in_kb + 4 * 1024) * 1024);

	  /* first: fill-in 1st partition data, but exclude header for now */
	  if (result == RET_OK)
	    {
	      u_int64_t part_size =
		((u_int64_t) get_u32 (&part->length)) * 512 - (4 _MB);
	      u_int64_t chunk_length = /* in bytes */
		((u_int64_t) kb_remaining) * 1024 < part_size ?
		((u_int64_t) kb_remaining) * 1024 : part_size;

	      u_int32_t start = (get_u32 (&part->start) + MAIN_HDR_SIZE_S +
				 SLICE_2_OFFS * slice_index);
	      u_int32_t count = (u_int32_t) (chunk_length / IIN_SECTOR_SIZE);

	      result = iin_copy_ex (iin, hio, offset, start, count, pgs);
	      pgs_chunk_complete (pgs);
	      offset += count;
	      kb_remaining -= (u_int32_t) (chunk_length / 1024);
	    }

	  for (i = 0; result == OSAL_OK && kb_remaining > 0 && i != get_u32 (&part->nsub); ++i)
	    { /* next: fill-in remaining partitions */
	      u_int32_t SUB_PART_HDR_SIZE_S = (1 _MB) / 512;
	      u_int64_t part_size =
		((u_int64_t) get_u32 (&part->subs[i].length)) * 512 - (1 _MB);
	      u_int64_t chunk_length = /* in bytes */
		((u_int64_t) kb_remaining) * 1024 < part_size ?
		((u_int64_t) kb_remaining) * 1024 : part_size;

	      u_int32_t start = (get_u32 (&part->subs[i].start) +
				 SUB_PART_HDR_SIZE_S +
				 SLICE_2_OFFS * slice_index);
	      u_int32_t count = (u_int32_t) (chunk_length / IIN_SECTOR_SIZE);

	      result = iin_copy_ex (iin, hio, offset,
				    start, count, pgs);
	      pgs_chunk_complete (pgs);
	      offset += count;
	      kb_remaining -= (u_int32_t) (chunk_length / 1024);
	    }

	  if (result == RET_OK && kb_remaining != 0)
	    result = RET_NO_SPACE; /* the game does not fit in the allocated space... why? */

	  /* NOTE: when this used to be the first operation, in cases where
	   * there is a gap and first partition header overwrites an __empty
	   * partition, if transfer is interrupted APA gets broken
	   * (as sub-partitions are not neccessarily there);
	   * therefore, main partition header is written here, at the end,
	   * where transfer can no longer be interrupted by the user */

	  if (result == RET_OK)
	    { /* last: write main partition header (4MB total) */
	      sector = get_u32 (&part->start) + SLICE_2_OFFS * slice_index;
	      result = hio->write (hio, sector, MAIN_HDR_SIZE_S,
				   buffer, &bytes);
	      if (result == OSAL_OK)
		{
		  result = bytes == 4 _MB ? OSAL_OK : OSAL_ERR;
		  /* track header,
		   * otherwise it would influence progress calculation */
		  (void) pgs_update (pgs, 4 _MB);
		  pgs_chunk_complete (pgs);
		}
	    }
	  osal_free (buffer), buffer = NULL;

	  /* finally: commit partition table; non-interruptable,
	   * except Ctrl+C might interrupt socket operation */
	  if (result == OSAL_OK)
	    result = apa_commit_ex (hio, toc);
	}
    }

  if (buffer != NULL)
    osal_free (buffer);

  return (result);
}


/**************************************************************/
int
hdl_inject (hio_t *hio,
	    iin_t *iin,
	    hdl_game_t *details,
	    int slice_index,
	    progress_t *pgs)
{
  /*@only@*/ apa_toc_t *toc = NULL;
  int result = apa_toc_read_ex (hio, &toc);
  if (result == OSAL_OK && toc != NULL)
    {
      u_int32_t sector_size, num_sectors;
      result = iin->stat (iin, &sector_size, &num_sectors);
      if (result == OSAL_OK)
	{
	  u_int64_t input_size = (u_int64_t) num_sectors * sector_size;
	  u_int32_t size_in_kb = (u_int32_t) (input_size / 1024);
	  u_int32_t size_in_mb =
	    (u_int32_t) ((input_size + (1 _MB - 1)) / (1 _MB));
	  u_int32_t new_partition_start;

	  if (details->partition_name[0] == '\0')
	    { /* partition naming is now auto-detected
		   * Toxic OS partition naming: "PP.HDL.STARTUP" doesnt work
		   * if (toc->is_toxic)
			   hdl_pname (details->startup, details->startup, details->partition_name);
		   * else
		   *
		   * PP.XXXX-xxxxx.HDL.name */
		hdl_pname (details->startup, details->name, details->partition_name);
	    }

	  result = apa_allocate_space (toc, details->partition_name,
				       size_in_mb, &slice_index,
				       &new_partition_start,
				       0); /* order by size desc */
	  if (result == RET_OK)
	    {
	      result = inject_data (hio, toc, slice_index, new_partition_start,
				    iin, size_in_kb, details, pgs);
	    }
	}
    }

  if (toc != NULL)
    apa_toc_free (toc);

  return (result);
}


/**************************************************************/
static int
hdl_ginfo_read (hio_t *hio,
		int slice_index,
		const ps2_partition_header_t *part,
		hdl_game_info_t *ginfo)
{
  u_int32_t i, size;
  /* data we're interested in starts @ 0x101000 and is header
   * plus information for up to 65 partitions
   * (1 main + 64 sub) by 12 bytes each */
  const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
  const u_int32_t HDL_HEADER_OFFS = 0x101000;
  char buffer[1024];
  int result;
  u_int32_t bytes;
  u_int32_t sector = (get_u32 (&part->start) + HDL_HEADER_OFFS / 512 +
		      slice_index * SLICE_2_OFFS);

  result = hio->read (hio, sector, 2, buffer, &bytes);
  if (result == RET_OK)
    {
      if (bytes == 1024)
	{
	  u_int32_t num_parts = (u_int32_t) buffer[0x00f0];
	  const u_int32_t *data = (u_int32_t*) (buffer + 0x00f5);

	  /* calculate allocated size */
	  size = get_u32 (&part->length);
	  for (i = 0; i < get_u32 (&part->nsub); ++i)
	    size += get_u32 (&part->subs[i].length);
	  ginfo->alloc_size_in_kb = size / 2;

	  size = 0;
	  for (i = 0; i < num_parts; ++i)
	    size += get_u32 (data + (i * 3 + 2));
	  ginfo->raw_size_in_kb = size / 4;

	  memcpy (ginfo->partition_name, part->id, PS2_PART_IDMAX);
	  ginfo->partition_name[PS2_PART_IDMAX] = '\0';
	  strcpy (ginfo->name, buffer + 0x0008);
	  strcpy (ginfo->startup, buffer + 0x00ac);
	  ginfo->compat_flags = (compat_flags_t) get_u8 (buffer + 0x00a9);
	  ginfo->dma = (unsigned short) get_u16 (buffer + 0x00aa);
	  ginfo->is_dvd = (buffer[0x00ec] == 0x14);
	  ginfo->slice_index = slice_index;
	  ginfo->start_sector = get_u32 (&part->start);
	}
      else
	result = RET_ERR;
    }

  return (result);
}


/**************************************************************/
static size_t
hdl_games_count (const apa_slice_t *slice)
{
  size_t i, count = 0;
  for (i = 0; i < slice->part_count; ++i)
    count += (get_u16 (&slice->parts[i].header.flags) == 0x00 &&
	      get_u16 (&slice->parts[i].header.type) == 0x1337);
  return (count);
}


/**************************************************************/
static int
hdl_glist_read_slice (hio_t *hio,
		      hdl_games_list_t *glist,
		      const apa_toc_t *toc,
		      int slice_index)
{
  size_t i;
  int result = RET_OK;
  for (i = 0; result == RET_OK && i < toc->slice[slice_index].part_count; ++i)
    {
      const ps2_partition_header_t *part =
	&toc->slice[slice_index].parts[i].header;
      if (get_u16 (&part->flags) == 0x00 &&
	  get_u16 (&part->type) == 0x1337)
	result = hdl_ginfo_read (hio, slice_index, part,
				 glist->games + glist->count++);
    }
  return (result);
}


/**************************************************************/
int
hdl_glist_read (hio_t *hio,
		hdl_games_list_t **glist)
{
  /*@only@*/ apa_toc_t *toc = NULL;
  int result = apa_toc_read_ex (hio, &toc);
  if (result == RET_OK && toc != NULL)
    {
      size_t count;
      void *tmp;

      count = (hdl_games_count (toc->slice + 0) +
	       hdl_games_count (toc->slice + 1));

      tmp = osal_alloc (sizeof (hdl_game_info_t) * count);
      if (tmp != NULL)
	{
	  memset (tmp, 0, sizeof (hdl_game_info_t) * count);
	  *glist = osal_alloc (sizeof (hdl_games_list_t));
	  if (*glist != NULL)
	    {
	      memset (*glist, 0, sizeof (hdl_games_list_t));
	      (*glist)->count = 0;
	      (*glist)->games = tmp;
	      (*glist)->total_chunks = (toc->slice[0].total_chunks +
					toc->slice[1].total_chunks);
	      (*glist)->free_chunks = (toc->slice[0].free_chunks +
				       toc->slice[1].free_chunks);

	      result = hdl_glist_read_slice (hio, *glist, toc, 0);
	      if (result == RET_OK)
		result = hdl_glist_read_slice (hio, *glist, toc, 1);

	      if (result != RET_OK)
		osal_free (*glist);
	    }
	  else
	    result = RET_NO_MEM;
	  if (result != RET_OK)
	    osal_free (tmp);
	}
      else
	result = RET_NO_MEM;

      apa_toc_free (toc);
    }
  return (result);
}


/**************************************************************/
void
hdl_glist_free (hdl_games_list_t *glist)
{
  if (glist != NULL)
    {
      osal_free (glist->games);
      osal_free (glist);
    }
}


/**************************************************************/
int
hdl_lookup_partition_ex (hio_t *hio,
			 const char *game_name,
			 char partition_id[PS2_PART_IDMAX + 1])
{
  /*@only@*/ hdl_games_list_t *games = NULL;
  int result;
  *partition_id = '\0';
  result = hdl_glist_read (hio, &games);
  if (result == RET_OK && games != NULL)
    {
      size_t i;
      int found = 0;
      for (i = 0; i < games->count; ++i)
	if (caseless_compare (game_name, games->games[i].name))
	  { /* found */
	    strcpy (partition_id, games->games[i].partition_name);
	    found = 1;
	    break;
	  }
      if (!found)
	result = RET_NOT_FOUND;

      hdl_glist_free (games);
    }
  return (result);
}


/**************************************************************/
int
hdl_lookup_partition (const dict_t *config,
		      const char *device_name,
		      const char *game_name,
		      char partition_id[PS2_PART_IDMAX + 1])
{
  /*@only@*/ hio_t *hio = NULL;
  int result;
  *partition_id = '\0';
  result = hio_probe (config, device_name, &hio);
  if (result == RET_OK && hio != NULL)
    {
      result = hdl_lookup_partition_ex (hio, game_name, partition_id);
      (void) hio->close (hio);
    }
  return (result);
}


/**************************************************************/
int
hdl_read_game_alloc_table (hio_t *hio,
			   const apa_toc_t *toc,
			   int slice_index,
			   u_int32_t partition_index,
			   hdl_game_alloc_table_t *gat)
{
  const apa_slice_t *slice = toc->slice + slice_index;
  const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
  unsigned char buffer[1024];
  u_int32_t len;
  u_int32_t sect = (get_u32 (&slice->parts[partition_index].header.start) +
		    0x00101000 / 512 + slice_index * SLICE_2_OFFS);
  int result = hio->read (hio, sect, 2, buffer, &len);
  if (result == OSAL_OK)
    {
      if (get_u32 (buffer) == 0xdeadfeed)
	{ /* 0xdeadfeed magic found */
	  const u_int32_t *data = (u_int32_t*) (buffer + 0x00f5);
	  u_int32_t i;

	  gat->count = buffer[0x00f0];
	  gat->size_in_kb = 0;
	  for (i = 0; i < gat->count; ++i)
	    {
	      gat->part[i].start = get_u32 (data + (i * 3 + 1)) << 8;
	      gat->part[i].len = get_u32 (data + (i * 3 + 2)) / 2;
	      gat->size_in_kb += gat->part[i].len / 2;
	    }
	}
      else
	result = RET_NOT_HDL_PART;
    }
  return (result);
}


/**************************************************************/
int
hdl_modify_game (hio_t *hio,
		 apa_toc_t *toc,
		 int slice_index,
		 u_int32_t starting_partition_sector,
		 const char *new_name, /* or NULL */
		 compat_flags_t new_compat_flags, /* or COMPAT_FLAGS_INVALID */
		 unsigned short new_dma) /* or 0 */
{
  apa_slice_t *slice = toc->slice + slice_index;
  const u_int32_t SLICE_2_OFFS = 0x10000000; /* sectors */
  apa_partition_t *part = NULL;
  u_int32_t i;
  u_int8_t hdl_hdr[1024];
  u_int32_t sector, bytes = 0;
  int result;

  for (i = 0; part == NULL && i < slice->part_count; ++i)
    if (get_u32 (&slice->parts[i].header.start) == starting_partition_sector)
      /* starting partition index located */
      part = slice->parts + i;
  if (part == NULL)
    return (RET_NOT_FOUND);

  /* BUG: hdl_modify_game wouldn't change part name in Sony HDD browser */
  sector = (get_u32 (&part->header.start) +
	    0x00101000 / 512 + slice_index * SLICE_2_OFFS);
  result = hio->read (hio, sector, 2, hdl_hdr, &bytes);
  if (result == RET_OK)
    {
      if (new_name != NULL && !toc->is_toxic)
	{ /* HD Loader partition naming: "PP.HDL.Game name" */
	  char part_id[PS2_PART_IDMAX];
	  int tmp_slice_index = 0;
	  u_int32_t tmp_partition_index = 0;
	  hdl_pname (NULL, new_name, part_id);
	  result = apa_find_partition (toc, part_id, &tmp_slice_index,
				       &tmp_partition_index);
	  if (result == RET_NOT_FOUND)
	    {
	      strcpy (part->header.id, part_id);
	      set_u32 (&part->header.checksum,
		       apa_partition_checksum (&part->header));
	      part->modified = 1;
	      result = RET_OK;
	    }
	  else if (result == RET_OK)
	    /* partition with such name already exists */
	    result = RET_PART_EXISTS;
	}

      if (result == RET_OK)
	{
	  if (new_name != NULL)
	    {
	      memset (hdl_hdr + 0x08, 0, 0xa0);
	      memcpy (hdl_hdr + 0x08, new_name, strlen (new_name));
	    }
	  if (new_compat_flags != COMPAT_FLAGS_INVALID)
	    set_u8 (hdl_hdr + 0xa9, new_compat_flags);
	  if (new_dma != 0)
	    set_u16 (hdl_hdr + 0xaa, new_dma);
	  result = hio->write (hio, sector, 2, hdl_hdr, &bytes);
	}
      if (result == RET_OK)
	result = apa_commit_ex (hio, toc);
    }
  return (result);
}
