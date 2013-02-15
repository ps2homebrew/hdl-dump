/*
 * hdl_dump.c
 * $Id: hdl_dump.c,v 1.11 2004/09/12 17:25:26 b081 Exp $
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

#if defined (_BUILD_WIN32)
#  include <windows.h>
#endif
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "retcodes.h"
#include "osal.h"
#include "apa.h"
#include "common.h"
#include "progress.h"
#include "hdl.h"
#include "isofs.h"
#include "iin.h"
#include "aligned.h"
#include "hio_probe.h"
#include "net_io.h"
#if defined (_WITH_ASPI)
#  include "aspi_hlio.h"
#endif


/* command names */
#define CMD_QUERY "query"
#if defined (INCLUDE_DUMP_CMD)
#  define CMD_DUMP "dump"
#endif
#if defined (INCLUDE_COMPARE_CMD)
#  define CMD_COMPARE "compare"
#endif
#if defined (INCLUDE_COMPARE_IIN_CMD)
#  define CMD_COMPARE_IIN "compare_iin"
#endif
#define CMD_TOC "toc"
#if defined (INCLUDE_MAP_CMD)
#  define CMD_MAP "map"
#endif
#define CMD_DELETE "delete"
#if defined (INCLUDE_ZERO_CMD)
#  define CMD_ZERO "zero"
#endif
#if defined (INCLUDE_CUTOUT_CMD)
#  define CMD_CUTOUT "cutout"
#endif
#if defined (INCLUDE_INFO_CMD)
#  define CMD_HDL_INFO "info"
#endif
#define CMD_HDL_EXTRACT "extract"
#define CMD_HDL_INJECT_CD "inject_cd"
#define CMD_HDL_INJECT_DVD "inject_dvd"
#define CMD_CDVD_INFO "cdvd_info"
#if defined (INCLUDE_READ_TEST_CMD)
#  define CMD_READ_TEST "read_test"
#endif


/**************************************************************/
static void
show_apa_toc (const apa_partition_table_t *table)
{
  size_t i;

  for (i=0; i<table->part_count; ++i)
    {
      const ps2_partition_header_t *part = &table->parts [i].header;
	  
      fprintf (stdout, "%06lx00%c%c %5luMB ",
	       (unsigned long) (part->start >> 8),
	       table->parts [i].existing ? '.' : '*',
	       table->parts [i].modified ? '*' : ':',
	       (unsigned long) (part->length / 2048));
      if (part->main == 0)
	fprintf (stdout, "%4x [%-*s]\n",
		 part->type, PS2_PART_IDMAX, part->id);
      else
	fprintf (stdout, "      part # %2lu in %06lx00\n",
		 (unsigned long) (part->number),
		 (unsigned long) (part->main >> 8));
    }

  fprintf (stdout, "Total device size: %uMB, used: %uMB, available: %uMB\n",
	   table->device_size_in_mb, table->allocated_chunks * 128, table->free_chunks * 128);
}


/**************************************************************/
#if defined (INCLUDE_MAP_CMD)
static void
show_apa_map (const apa_partition_table_t *table)
{
  /* show device map */
  const char *map = table->chunks_map;
  const size_t GIGS_PER_ROW = 8;
  size_t i, count = 0;

  for (i=0; i<table->total_chunks; ++i)
    {
      if (count == 0)
	fprintf (stdout, "%3uGB: ", (i / ((GIGS_PER_ROW * 1024) / 128)) * GIGS_PER_ROW);

      fputc (map [i], stdout);
      if ((count & 0x07) == 0x07)
	fputc (' ', stdout);

      if (++count == ((GIGS_PER_ROW * 1024) / 128)) /* 8G on each row */
	{
	  fputc ('\n', stdout);
	  count = 0;
	}
    }

  fprintf (stdout, "\nTotal device size: %uMB, used: %uMB, available: %uMB\n",
	   table->device_size_in_mb, table->allocated_chunks * 128, table->free_chunks * 128);
}
#endif /* INCLUDE_MAP_CMD defined? */


/**************************************************************/
static int
show_toc (const char *device_name)
{
  apa_partition_table_t *table;
  int result = apa_ptable_read (device_name, &table);
  if (result == RET_OK)
    {
      show_apa_toc (table);
      apa_ptable_free (table);
    }
  return (result);
}


/**************************************************************/
#if defined (INCLUDE_MAP_CMD)
static int
show_map (const char *device_name)
{
  apa_partition_table_t *table;
  int result = apa_ptable_read (device_name, &table);
  if (result == RET_OK)
    {
      show_apa_map (table);
      apa_ptable_free (table);
    }
  return (result);
}
#endif /* INCLUDE_MAP_CMD defined? */


/**************************************************************/
#if defined (INCLUDE_INFO_CMD)
static int
show_hdl_game_info (const char *device_name,
		    const char *game_name)
{
  hio_t *hio;
  int result = hio_probe (device_name, &hio);
  if (result == RET_OK)
    {
      apa_partition_table_t *table;
      result = apa_ptable_read_ex (hio, &table);
      if (result == RET_OK)
	{
	  size_t partition_index;
	  result = apa_find_partition (table, game_name, &partition_index);
	  if (result == RET_NOT_FOUND)
	    { /* use heuristics - look among the HD Loader partitions */
	      char tmp [100];
	      strcpy (tmp, "PP.HDL.");
	      strcat (tmp, game_name);
	      result = apa_find_partition (table, tmp, &partition_index);
	    }

	  if (result == RET_OK)
	    { /* partition found */
	      const size_t PART_SYSDATA_SIZE = 4 * 1024 * 1024;
	      unsigned char *buffer = osal_alloc (PART_SYSDATA_SIZE);
	      if (buffer != NULL)
		{
		  size_t len;
		  result = hio->read (hio, table->parts [partition_index].header.start,
				      (4 _MB) / HDD_SECTOR_SIZE, buffer, &len);
		  if (result == OSAL_OK)
		    {
		      const char *signature = (char*) buffer + 0x001010ac;
		      const char *hdl_name = (char*) buffer + 0x00101008;
		      size_t type = buffer [0x001010ec];
		      size_t num_parts = buffer [0x001010f0];
		      const size_t *data = (size_t*) (buffer + 0x001010f5);
		      size_t i;

		      if (buffer [0x00101000] == 0xed &&
			  buffer [0x00101001] == 0xfe &&
			  buffer [0x00101002] == 0xad &&
			  buffer [0x00101003] == 0xde)
			{ /* 0xdeadfeed magic found */
			  bigint_t total_size = 0;

			  fprintf (stdout, "%s: [%s], %s\n",
				   signature, hdl_name, (type == 0x14 ? "DVD" : "CD"));
			  for (i=0; i<num_parts; ++i)
			    {
			      total_size += ((LONGLONG) data [i * 3 + 2]) << 8;
			      fprintf (stdout,
				       "\tpart %2u is from sector 0x%06x00, %7uKB long\n",
				       i + 1, data [i * 3 + 1], data [i * 3 + 2] / 4);
			    }
			  fprintf (stdout, "Total size: %luKB (%luMB approx.)\n",
				   (unsigned long) (total_size / 1024),
				   (unsigned long) (total_size / (1024 * 1024)));
			}
		      else
			result = RET_NOT_HDL_PART;
		    }
		  osal_free (buffer);
		}
	      else
		result = RET_NO_MEM;
	    }
	  apa_ptable_free (table);
	}
      hio->close (hio);
    }
  return (result);
}
#endif /* INCLUDE_INFO_CMD defined? */


/**************************************************************/
#if defined (INCLUDE_CUTOUT_CMD)
static int
show_apa_cut_out_for_inject (const char *device_name,
			     size_t size_in_mb)
{
  apa_partition_table_t *table;
  int result = apa_ptable_read (device_name, &table);
  if (result == RET_OK)
    {
      size_t new_partition_index;
      result = apa_allocate_space (table,
				   "<new partition>",
				   size_in_mb,
				   &new_partition_index,
				   0);
      if (result == RET_OK)
	{
	  show_apa_toc (table);
	  show_apa_map (table);
	}

      apa_ptable_free (table);
    }
  return (result);
}
#endif /* INCLUDE_CUTOUT_CMD defined? */


/**************************************************************/
static int
delete_partition (const char *device_name,
		  const char *name)
{
  apa_partition_table_t *table;
  int result = apa_ptable_read (device_name, &table);
  if (result == RET_OK)
    {
      result = apa_delete_partition (table, name);

      if (result == RET_OK)
	result = apa_commit (device_name, table);

      apa_ptable_free (table);
    }
  return (result);
}


/**************************************************************/
#if defined (INCLUDE_COMPARE_CMD)
static int
compare (const char *file_name_1,
	 int file_name_1_is_device,
	 const char *file_name_2,
	 int file_name_2_is_device)
{
  osal_handle_t file1 = OSAL_HANDLE_INIT, file2 = OSAL_HANDLE_INIT;
  int different = 0;
  int result = osal_open (file_name_1, &file1, 1);
  if (result == OSAL_OK)
    {
      result = osal_open (file_name_2, &file2, 1);
      if (result == OSAL_OK)
	{
	  const size_t BUFF_SIZE = 4 * 1024 * 1024;
	  char *buffer = osal_alloc (BUFF_SIZE);
	  if (buffer != NULL)
	    {
	      size_t read1 = 0, read2 = 0;
	      do
		{
		  result = osal_read (file1, buffer + 0 * BUFF_SIZE / 2,
				      BUFF_SIZE / 2, &read1);
		  if (result == OSAL_OK)
		    {
		      result = osal_read (file2, buffer + 1 * BUFF_SIZE / 2,
					  BUFF_SIZE / 2, &read2);
		      if (result == OSAL_OK)
			{
			  different = read1 != read2;
			  if (!different)
			    {
			      size_t i;
			      const char *p1 = buffer + 0 * BUFF_SIZE / 2;
			      const char *p2 = buffer + 1 * BUFF_SIZE / 2;
			      for (i=0; !different && i<read1; ++i)
				different = *p1++ != *p2++;
			    }
			}
		    }
		}
	      while (result == OSAL_OK && read1 > 0 && read2 > 0 && !different);
	    }
	  else
	    result = RET_NO_MEM;
	  osal_close (file2);
	}
      osal_close (file1);
    }
  return (result == RET_OK ? (!different ? RET_OK : RET_DIFFERENT) : result);
}
#endif /* INCLUDE_COMPARE_CMD defined? */


/**************************************************************/
#if defined (INCLUDE_ZERO_CMD)
static int
zero_device (const char *device_name)
{
  osal_handle_t device;
  int result = osal_open_device_for_writing (device_name, &device);
  if (result == OSAL_OK)
    {
      void *buffer = osal_alloc (1 _MB);
      if (buffer != NULL)
	{
	  size_t bytes;
	  memset (buffer, 0, 1 _MB);
	  do
	    {
	      result = osal_write (device, buffer, 1 _MB, &bytes);
	    }
	  while (result == OSAL_OK && bytes > 0);
	}
      else
	result = RET_NO_MEM;
      osal_close (device);
    }
  return (result);
}
#endif /* INCLUDE_ZERO_CMD defined? */


/**************************************************************/
static int
query_devices (void)
{
  osal_dlist_t *hard_drives;
  osal_dlist_t *optical_drives;

  int result = osal_query_devices (&hard_drives, &optical_drives);
  if (result == RET_OK)
    {
      size_t i;

      fprintf (stdout, "Hard drives:\n");
      for (i=0; i<hard_drives->used; ++i)
	{
	  const osal_dev_t *dev = hard_drives->device + i;
	  fprintf (stdout, "\t%s ", dev->name);
	  if (dev->status == 0)
	    {
	      fprintf (stdout, "%lu MB", (unsigned long) (dev->capacity / 1024) / 1024);
	      if (dev->is_ps2 == RET_OK)
		fprintf (stdout, ", formatted Playstation 2 HDD\n");
	      else
		fprintf (stdout, "\n");
	    }
	  else
	    {
	      char *error = osal_get_error_msg (dev->status);
	      if (error != NULL)
		{
		  fprintf (stdout, error);
		  osal_dispose_error_msg (error);
		}
	      else
		fprintf (stdout, "error querying device.\n");
	    }
	}

      fprintf (stdout, "\nOptical drives:\n");
      for (i=0; i<optical_drives->used; ++i)
	{
	  const osal_dev_t *dev = optical_drives->device + i;
	  fprintf (stdout, "\t%s ", dev->name);
	  if (dev->status == 0)
	    fprintf (stdout, "%lu MB\n", (unsigned long) (dev->capacity / 1024) / 1024);
	  else
	    {
	      char *error = osal_get_error_msg (dev->status);
	      if (error != NULL)
		{
		  fprintf (stdout, error);
		  osal_dispose_error_msg (error);
		}
	      else
		fprintf (stdout, "error querying device.\n");
	    }
	}

      osal_dlist_free (optical_drives);
      osal_dlist_free (hard_drives);
    }

#if defined (_WITH_ASPI)
  if (aspi_load () == RET_OK)
    {
      scsi_devices_list_t *dlist;
      result = aspi_scan_scsi_bus (&dlist);
      if (result == RET_OK)
	{
	  size_t i;

	  fprintf (stdout, "\nDrives via ASPI:\n");
	  for (i=0; i<dlist->used; ++i)
	    {
	      switch (dlist->device [i].type)
		{
		case 0x00: /* HDD most likely */
		  printf ("\thdd%d:%d:%d ",
			  dlist->device [i].host,
			  dlist->device [i].scsi_id,
			  dlist->device [i].lun);
		  break;

		case 0x05: /* MMC device */
		  printf ("\tcd%d:%d:%d  ",
			  dlist->device [i].host,
			  dlist->device [i].scsi_id,
			  dlist->device [i].lun);
		  break;
		}
	      if (dlist->device [i].size_in_sectors != -1 &&
		  dlist->device [i].sector_size != -1)
		printf ("%lu MB\n",
			(unsigned long) (((bigint_t) dlist->device [i].size_in_sectors *
					  dlist->device [i].sector_size) / (1024 * 1024)));
	      else
		printf ("Stat failed.\n");
	    }
	  aspi_dlist_free (dlist);
	}
      else
	fprintf (stderr, "\nError scanning SCSI bus.\n");
      aspi_unload ();
    }
  else
    fprintf (stderr, "\nUnable to initialize ASPI.\n");
#endif /* _WITH_ASPI */

  return (result);
}


/**************************************************************/
static int
cdvd_info (const char *path)
{
  iin_t *iin;
  int result = iin_probe (path, &iin);
  if (result == OSAL_OK)
    {
      char volume_id [32 + 1];
      char signature [12 + 1];
      size_t num_sectors, sector_size;
      result = iin->stat (iin, &sector_size, &num_sectors);
      if (result == OSAL_OK)
	result = isofs_get_ps_cdvd_details (iin, volume_id, signature);
      if (result == OSAL_OK)
	printf ("\"%s\" \"%s\" %luKB\n",
		signature, volume_id,
		(unsigned long) (((bigint_t) num_sectors * sector_size) / 1024));
      iin->close (iin);
    }
  return (result);
}


/**************************************************************/
#if defined (INCLUDE_READ_TEST_CMD)
static int
read_test (const char *path,
	   progress_t *pgs)
{
  iin_t *iin;
  int result = iin_probe (path, &iin);
  if (result == OSAL_OK)
    {
      size_t sector_size, num_sectors;
      result = iin->stat (iin, &sector_size, &num_sectors);
      if (result == OSAL_OK)
	{
	  size_t sector = 0;
	  size_t len;

	  pgs_prepare (pgs, (bigint_t) num_sectors * sector_size);
	  do
	    {
	      const char *data; /* not used */
	      size_t sectors = (num_sectors > IIN_NUM_SECTORS ? IIN_NUM_SECTORS : num_sectors);
	      /* TODO: would "buffer overflow" if read more than IIN_NUM_SECTORS? */
	      result = iin->read (iin, sector, sectors, &data, &len);
	      if (result == RET_OK)
		{
		  size_t sectors_read = len / IIN_SECTOR_SIZE;
		  sector += sectors_read;
		  num_sectors -= sectors_read;
		  pgs_update (pgs, sector * IIN_SECTOR_SIZE);
		}
	    }
	  while (result == OSAL_OK && num_sectors > 0 && len > 0);
	}
      
      iin->close (iin);
    }
  return (result);
}
#endif /* INCLUDE_READ_TEST_CMD defined? */


/**************************************************************/
#if defined (INCLUDE_COMPARE_IIN_CMD)
static int
compare_iin (const char *path1,
	     const char *path2,
	     progress_t *pgs)
{
  iin_t *iin1, *iin2;
  int different = 0;
  int longer_file = 0;

  int result = iin_probe (path1, &iin1);
  if (result == OSAL_OK)
    {
      result = iin_probe (path2, &iin2);
      if (result == OSAL_OK)
	{
	  size_t len1, len2;
	  const char *data1, *data2;
	  size_t sector = 0;
	  size_t sector_size1, num_sectors1;
	  size_t sector_size2, num_sectors2;
	  bigint_t size1, size2;

	  result = iin1->stat (iin1, &sector_size1, &num_sectors1);
	  if (result == OSAL_OK)
	    {
	      size1 = (bigint_t) num_sectors1 * sector_size1;
	      result = iin2->stat (iin2, &sector_size2, &num_sectors2);
	    }
	  if (result == OSAL_OK)
	    {
	      size2 = (bigint_t) num_sectors2 * sector_size2;
	      if (sector_size1 == IIN_SECTOR_SIZE &&
		  sector_size2 == IIN_SECTOR_SIZE)
		/* progress indicator is set up against the shorter file */
		pgs_prepare (pgs, size1 < size2 ? size1 : size2);
	      else
		/* unable to compare with different sector sizes */
		result = RET_DIFFERENT;
	    }

	  len1 = len2 = IIN_SECTOR_SIZE;
	  while (result == OSAL_OK &&
		 len1 / IIN_SECTOR_SIZE > 0 &&
		 len2 / IIN_SECTOR_SIZE > 0)
	    {
	      size_t sectors1 = (num_sectors1 > IIN_NUM_SECTORS ?
				 IIN_NUM_SECTORS : num_sectors1);
	      result = iin1->read (iin1, sector, sectors1, &data1, &len1);
	      if (result == OSAL_OK)
		{
		  size_t sectors2 = (num_sectors2 > IIN_NUM_SECTORS ?
				     IIN_NUM_SECTORS : num_sectors2);
		  result = iin2->read (iin2, sector, sectors2, &data2, &len2);
		  if (result == OSAL_OK)
		    {
		      size_t len = (len1 <= len2 ? len1 : len2); /* lesser from the two */
		      size_t len_s = len / IIN_SECTOR_SIZE;

		      if (memcmp (data1, data2, len) != 0)
			{
			  different = 1;
			  break;
			}

		      if (len1 != len2 &&
			  (len1 == 0 || len2 == 0))
			{ /* track which file is longer */
			  if (len2 == 0)
			    longer_file = 1;
			  else if (len1 == 0)
			    longer_file = 2;
			}

		      num_sectors1 -= len_s;
		      num_sectors2 -= len_s;
		      sector += len_s;
		      pgs_update (pgs, (bigint_t) sector * IIN_SECTOR_SIZE);
		    }
		}
	    } /* loop */
	  iin2->close (iin2);
	}
      iin1->close (iin1);
    }

  /* handle result code */
  if (result == OSAL_OK)
    { /* no I/O errors */
      if (different == 0)
	{ /* contents are the same */
	  switch (longer_file)
	    {
	    case 0: return (RET_OK); /* neither */
	    case 1: return (RET_1ST_LONGER);
	    case 2: return (RET_2ND_LONGER);
	    default: return (RET_ERR); /* should not be here */
	    }
	}
      else
	/* contents are different */
	return (RET_DIFFERENT);
    }
  else
    return (result);
}
#endif /* INCLUDE_COMPARE_IIN_CMD defined? */


/**************************************************************/
static int
progress_cb (progress_t *pgs)
{
  static time_t last_flush = 0;
  time_t now = time (NULL);

  if (pgs->remaining != -1)
    fprintf (stdout,
	     "%3d%%, %s remaining (est.), avg %.2f MBps, "
	     "curr %.2f MBps         \r",
	     pgs->pc_completed, pgs->remaining_text,
	     pgs->avg_bps / (1024.0 * 1024.0),
	     pgs->curr_bps / (1024.0 * 1024.0));
  else
    fprintf (stdout, "%3d%%\r", pgs->pc_completed);

  if (now > last_flush)
    { /* flush about once per second */
      fflush (stdout);
      last_flush = now;
    }

  return (RET_OK);
}

/* progress is allocated, but never freed, which is not a big deal for a CLI app */
progress_t*
get_progress (void)
{
#if 0
  return (NULL);
#else
  progress_t *pgs = pgs_alloc (&progress_cb);
  return (pgs);
#endif
}


/**************************************************************/
void
show_usage_and_exit (const char *app_path,
		     const char *command)
{
  int command_found;
  static const struct help_entry_t
  {
    const char *command_name;
    const char *command_arguments;
    const char *description;
    const char *example1, *example2;
    int dangerous;
  } help [] =
    {
      { CMD_QUERY, NULL,
	"Displays a list of all hard- and optical drives.",
	NULL, NULL, 0 },
#if defined (INCLUDE_DUMP_CMD)
      { CMD_DUMP, "device file",
	"Makes device image (AKA ISO-image).",
	"cd0: c:\\tekken.iso", NULL, 0 },
#endif
#if defined (INCLUDE_COMPARE_CMD)
      { CMD_COMPARE, "file_or_device1 file_or_device2",
	"Compares two files or devices.",
	"cd0: c:\\tekken.iso", NULL, 0 },
#endif
#if defined (INCLUDE_COMPARE_IIN_CMD)
      { CMD_COMPARE_IIN, "iin1 iin2",
	"Compares two ISO inputs.",
	"c:\\tekken.cue cd0:", "c:\\gt3.gi hdd1:GT3", 0 },
#endif
      { CMD_TOC, "device",
	"Displays PlayStation 2 HDD TOC.",
	"hdd1:", NULL, 0 },
#if defined (INCLUDE_MAP_CMD)
      { CMD_MAP, "device",
	"Displays PlayStation 2 HDD usage map.",
	"hdd1:", NULL, 0 },
#endif
      { CMD_DELETE, "device partition",
	"Deletes PlayStation 2 HDD partition. Use \"map\" to find exact partition name.",
	"hdd1: \"PP.HDL.Tekken Tag Tournament\"", NULL, 1 },
#if defined (INCLUDE_ZERO_CMD)
      { CMD_ZERO, "device",
	"Fills HDD with zeroes. All information on the HDD will be lost.",
	"hdd1:", NULL, 1 },
#endif
#if !defined (CRIPPLED_INJECTION) && defined (INCLUDE_CUTOUT_CMD)
      { CMD_CUTOUT, "device size_in_MB",
	"Displays partition table as if a new partition has been created.",
	"hdd1: 2560", NULL, 0 },
#endif
#if defined (INCLUDE_INFO_CMD)
      { CMD_HDL_INFO, "device partition",
	"Displays information about HD Loader partition.",
	"hdd1: \"tekken tag tournament\"", NULL, 0 },
#endif
      { CMD_HDL_EXTRACT, "device partition output_file",
	"Extracts application image from HD Loader partition.",
	"hdd1: \"tekken tag tournament\" c:\\tekken.iso", NULL, 0 },
      { CMD_HDL_INJECT_CD, "device partition file signature",
	"Creates a new HD Loader partition from a CD.\n"
	"Supported inputs: plain ISO files, CDRWIN cuesheets, Nero images and tracks,\n"
	"RecordNow! Global images, HD Loader partitions (hdd1:PP.HDL.Xenosaga) and\n"
	"Sony CD/DVD generator IML files (if files are listed with full paths).",
	"hdd1: \"Tekken Tag Tournament\" cd0: SCES_xxx.xx",
	"hdd1: \"Tekken Tag Tournament\" c:\\tekken.iso SCES_xxx.xx", 1 },
      { CMD_HDL_INJECT_DVD, "device partition file signature",
	"Creates a new HD Loader partition from a DVD.\n"
	"DVD9 cannot be directly installed from the DVD-ROM drive -\n"
	"use ISO image or IML file instead.\n"
	"Supported inputs: plain ISO files, CDRWIN cuesheets, Nero images and tracks,\n"
	"RecordNow! Global images, HD Loader partitions (hdd1:PP.HDL.Xenosaga) and\n"
	"Sony CD/DVD generator IML files (if files are listed with full paths).",
	"hdd1: \"Gran Turismo 3\" cd0: SCES_xxx.xx",
	"hdd1: \"Gran Turismo 3\" c:\\gt3.iso SCES_xxx.xx", 1 },
      { CMD_CDVD_INFO, "iin_input",
	"Displays signature (startup file), volume label and data size\n"
	"for a CD-/DVD-drive or image file.",
	"c:\\gt3.gi", "\"hdd2:Gran Turismo 3\"", 0 },
#if defined (INCLUDE_READ_TEST_CMD)
      { CMD_READ_TEST, "iin_input",
	"Consecutively reads all sectors from the specified input.",
	"cd0:", NULL, 0 },
#endif /* INCLUDE_READ_TEST_CMD defined? */
      { NULL, NULL,
	NULL,
	NULL, NULL, 0 }
    };
  const char *app;
  if (strrchr (app_path, '/') != NULL)
    app = strrchr (app_path, '/') + 1;
  else if (strrchr (app_path, '\\') != NULL)
    app = strrchr (app_path, '\\') + 1;
  else
    app = app_path;

  fprintf (stderr,
	   "hdl_dump-" VERSION " by The W1zard 0f 0z (AKA b...)\n"
	   "http://hdldump.ps2-scene.org/ w1zard0f07@yahoo.com\n"
	   "\n");

  command_found = 0;
  if (command != NULL)
    { /* display particular command help */
      const struct help_entry_t *h = help;
      while (h->command_name != NULL)
	{
	  if (strcmp (command, h->command_name) == 0)
	    {
	      fprintf (stdout,
		       "Usage:\t%s %s\n"
		       "\n"
		       "%s\n",
		       h->command_name, h->command_arguments,
		       h->description);
	      if (h->example1 != NULL)
		fprintf (stdout,
			 "\n"
			 "Example:\n"
			 "%s %s %s\n",
			 app, h->command_name, h->example1);
	      if (h->example2 != NULL)
		fprintf (stdout,
			 "\tor\n"
			 "%s %s %s\n",
			 app, h->command_name, h->example2);
	      if (h->dangerous)
		fprintf (stdout,
			 "\n"
			 "Warning: This command does write on the HDD\n"
			 "         and could cause corruption. Use with care.\n");
	      command_found = 1;
	      break;
	    }
	  ++h;
	}
    }

  if (command == NULL ||
      !command_found)
    { /* display all commands only */
      const struct help_entry_t *h = help;
      int is_first = 1, count = 0;

      fprintf (stdout,
	       "Usage:\n"
	       "%s command arguments\n"
	       "\n"
	       "Where command is one of:\n",
	       app);

      while (h->command_name != NULL)
	{
	  if (!is_first)
	    {
	      if ((count % 5) == 0)
		fprintf (stdout, ",\n");
	      else
		fprintf (stdout, ", ");
	    }
	  else
	    is_first = 0;
	  fprintf (stdout, "%s", h->command_name);
	  if (h->dangerous)
	    fprintf (stdout, "*");
	  ++h;
	  ++count;
	}
      fprintf (stdout, "\n");

      fprintf (stdout,
	       "\n"
	       "Use: %s command\n"
	       "to show \"command\" help.\n"
	       "\n"
	       "Warning: Commands, marked with * (asterisk) does write on the HDD\n"
	       "         and could cause corruption. Use with care.\n"
	       "\n"
	       "License: You are only allowed to use this program with a software\n"
	       "         you legally own. Use at your own risk.\n",
	       app);

      if (command != NULL && !command_found)
	{
	  fprintf (stdout,
		   "\n"
		   "%s: unrecognized command.\n",
		   command);
	}
    }

  exit (100);
}

void
map_device_name_or_exit (const char *input,
			 char output [MAX_PATH])
{
  int result = osal_map_device_name (input, output);
  switch (result)
    {
    case RET_OK: return;
    case RET_BAD_FORMAT: fprintf (stderr, "%s: bad format.\n", input); exit (100 + RET_BAD_FORMAT);
    case RET_BAD_DEVICE: fprintf (stderr, "%s: bad device.\n", input); exit (100 + RET_BAD_DEVICE);
    }
}

void
handle_result_and_exit (int result,
			const char *device,
			const char *partition)
{
  switch (result)
    {
    case RET_OK:
      exit (0);

    case RET_ERR:
      {
	unsigned long err_code = osal_get_last_error_code ();
	char *error = osal_get_last_error_msg ();
	if (error != NULL)
	  {
	    fprintf (stderr, "%08lx (%lu): %s\n", err_code, err_code, error);
	    osal_dispose_error_msg (error);
	  }
	else
	  fprintf (stderr, "%08lx (%lu): Unknown error.\n", err_code, err_code);
      }
      exit (1);

    case RET_NO_MEM:
      fprintf (stderr, "Out of memory.\n");
      exit (2);

    case RET_NOT_APA:
      fprintf (stderr, "%s: not a PlayStation 2 HDD.\n", device);
      exit (100 + RET_NOT_APA);

    case RET_NOT_HDL_PART:
      fprintf (stderr, "%s: not a HD Loader partition", device);
      if (partition != NULL)
	fprintf (stderr, ": \"%s\".\n", partition);
      else
	fprintf (stderr, ".\n");
      exit (100 + RET_NOT_HDL_PART);

    case RET_NOT_FOUND:
      fprintf (stderr, "%s: partition not found", device);
      if (partition != NULL)
	fprintf (stderr, ": \"%s\".\n", partition);
      else
	fprintf (stderr, ".\n");
      exit (100 + RET_NOT_FOUND);

    case RET_NO_SPACE:
      fprintf (stderr, "%s: not enough free space.\n", device);
      exit (100 + RET_NO_SPACE);

    case RET_BAD_APA:
      fprintf (stderr, "%s: APA partition is broken; aborting.\n", device);
      exit (100 + RET_BAD_APA);

    case RET_DIFFERENT:
      fprintf (stderr, "Contents are different.\n");
      exit (100 + RET_DIFFERENT);

    case RET_PART_EXISTS:
      fprintf (stderr, "%s: partition with such name already exists: \"%s\".\n",
	       device, partition);
      exit (100 + RET_PART_EXISTS);

    case RET_BAD_ISOFS:
      fprintf (stderr, "%s: bad ISOFS.\n", device);
      exit (100 + RET_BAD_ISOFS);

    case RET_NOT_PS_CDVD:
      fprintf (stderr, "%s: not a Playstation CD-ROM/DVD-ROM.\n", device);
      exit (100 + RET_NOT_PS_CDVD);

    case RET_BAD_SYSCNF:
      fprintf (stderr, "%s: SYSTEM.CNF is not in the expected format.\n", device);
      exit (100 + RET_BAD_SYSCNF);

    case RET_NOT_COMPAT:
      fprintf (stderr, "Input or output is unsupported.\n");
      exit (100 + RET_NOT_COMPAT);

    case RET_NOT_ALLOWED:
      fprintf (stderr, "Operation is not allowed.\n");
      exit (100 + RET_NOT_ALLOWED);

    case RET_BAD_COMPAT:
      fprintf (stderr, "Input or output is supported, but invalid.\n");
      exit (100 + RET_BAD_COMPAT);

    case RET_SVR_ERR:
      fprintf (stderr, "Server reported error.\n");
      exit (100 + RET_SVR_ERR);

    case RET_1ST_LONGER:
      fprintf (stderr, "First input is longer, but until then the contents are the same.\n");
      exit (100 + RET_1ST_LONGER);

    case RET_2ND_LONGER:
      fprintf (stderr, "Second input is longer, but until then the contents are the same.\n");
      exit (100 + RET_2ND_LONGER);

    case RET_FILE_NOT_FOUND:
      fprintf (stderr, "File not found.\n");
      exit (100 + RET_FILE_NOT_FOUND);

    case RET_BROKEN_LINK:
      fprintf (stderr, "Broken link (linked file not found).\n");
      exit (100 + RET_BROKEN_LINK);

    default:
      fprintf (stderr, "%s: don't know what the error is: %d.\n", device, result);
      exit (200);
    }
}


int
main (int argc, char *argv [])
{
  /*
  go_aspi ();
  return (0);
  */

  if (argc > 1)
    {
      const char *command_name = argv [1];

      if (caseless_compare (command_name, CMD_QUERY))
	{ /* show all devices */
	  handle_result_and_exit (query_devices (), NULL, NULL);
	}

#if defined (INCLUDE_DUMP_CMD)
      else if (caseless_compare (command_name, CMD_DUMP))
	{ /* dump CD/DVD-ROM to the HDD */
	  char device_name [MAX_PATH];

	  if (argc != 4)
	    show_usage_and_exit (argv [0], CMD_DUMP);

	  map_device_name_or_exit (argv [2], device_name);

	  handle_result_and_exit (dump_device (device_name, argv [3], 0, get_progress ()),
				  argv [2], NULL);
	}
#endif /* INCLUDE_DUMP_CMD defined? */

#if defined (INCLUDE_COMPARE_CMD)
      else if (caseless_compare (command_name, CMD_COMPARE))
	{ /* compare two files or devices or etc. */
	  char device_name_1 [MAX_PATH];
	  char device_name_2 [MAX_PATH];
	  int is_device_1 = 0, is_device_2 = 0;

	  if (argc != 4)
	    show_usage_and_exit (argv [0], CMD_COMPARE);

	  is_device_1 = osal_map_device_name (argv [2], device_name_1) == RET_OK ? 1 : 0;
	  is_device_2 = osal_map_device_name (argv [3], device_name_2) == RET_OK ? 1 : 0;

	  handle_result_and_exit (compare (is_device_1 ? device_name_1 : argv [2], is_device_1,
					   is_device_2 ? device_name_2 : argv [3], is_device_2),
				  NULL, NULL);
	}
#endif /* INCLUDE_COMPARE_CMD defined? */

#if defined (INCLUDE_COMPARE_IIN_CMD)
      else if (caseless_compare (command_name, CMD_COMPARE_IIN))
	{ /* compare two iso inputs */
	  if (argc != 4)
	    show_usage_and_exit (argv [0], CMD_COMPARE_IIN);
	  handle_result_and_exit (compare_iin (argv [2], argv [3], get_progress ()),
				  NULL, NULL);
	}
#endif /* INCLUDE_COMPARE_IIN_CMD defined? */

      else if (caseless_compare (command_name, CMD_TOC))
	{ /* show TOC of a PlayStation 2 HDD */
	  if (argc != 3)
	    show_usage_and_exit (argv [0], CMD_TOC);

	  handle_result_and_exit (show_toc (argv [2]), argv [2], NULL);
	}

#if defined (INCLUDE_MAP_CMD)
      else if (caseless_compare (command_name, CMD_MAP))
	{ /* show map of a PlayStation 2 HDD */
	  if (argc != 3)
	    show_usage_and_exit (argv [0], CMD_MAP);

	  handle_result_and_exit (show_map (argv [2]), argv [2], NULL);
	}
#endif /* INCLUDE_MAP_CMD defined? */

      else if (caseless_compare (command_name, CMD_DELETE))
	{ /* delete partition */
	  if (argc != 4)
	    show_usage_and_exit (argv [0], CMD_DELETE);

	  handle_result_and_exit (delete_partition (argv [2], argv [3]),
				  argv [2], argv [3]);
	}

#if defined (INCLUDE_ZERO_CMD)
      else if (caseless_compare (command_name, CMD_ZERO))
	{ /* zero HDD */
	  char device_name [MAX_PATH];

	  if (argc != 3)
	    show_usage_and_exit (argv [0], CMD_ZERO);

	  map_device_name_or_exit (argv [2], device_name);

	  handle_result_and_exit (zero_device (device_name),
				  argv [2], NULL);
	}
#endif /* INCLUDE_ZERO_CMD defined? */

#if defined (INCLUDE_INFO_CMD)
      else if (caseless_compare (command_name, CMD_HDL_INFO))
	{ /* show HD Loader game info */
	  if (argc != 4)
	    show_usage_and_exit (argv [0], CMD_HDL_INFO);

	  handle_result_and_exit (show_hdl_game_info (argv [2], argv [3]),
				  argv [2], argv [3]);
	}
#endif /* INCLUDE_INFO_CMD defined? */

      else if (caseless_compare (command_name, CMD_HDL_EXTRACT))
	{ /* extract game image from a HD Loader partition */
	  if (argc != 5)
	    show_usage_and_exit (argv [0], CMD_HDL_EXTRACT);

	  handle_result_and_exit (hdl_extract (argv [2], argv [3], argv [4],
					       get_progress ()),
				  argv [2], argv [3]);
	}

      else if (caseless_compare (command_name, CMD_HDL_INJECT_CD))
	{ /* inject game image into a new HD Loader partition */
	  if (argc != 6)
	    show_usage_and_exit (argv [0], CMD_HDL_INJECT_CD);

	  handle_result_and_exit (hdl_inject (argv [2], argv [3], argv [5],
					      argv [4], 0, get_progress ()),
				  argv [2], argv [3]);
	}

      else if (caseless_compare (command_name, CMD_HDL_INJECT_DVD))
	{ /* inject game image into a new HD Loader partition */
	  if (argc != 6)
	    show_usage_and_exit (argv [0], CMD_HDL_INJECT_DVD);

	  handle_result_and_exit (hdl_inject (argv [2], argv [3], argv [5],
					      argv [4], 1, get_progress ()),
				  argv [2], argv [3]);
	}

#if defined (INCLUDE_CUTOUT_CMD)
      else if (caseless_compare (command_name, CMD_CUTOUT))
	{ /* calculate and display how to arrange a new HD Loader partition */
	  char device_name [MAX_PATH];

	  if (argc != 4)
	    show_usage_and_exit (argv [0], CMD_CUTOUT);

	  map_device_name_or_exit (argv [2], device_name);

	  handle_result_and_exit (show_apa_cut_out_for_inject (device_name, atoi (argv [3])),
				  argv [2], NULL);
	}
#endif /* INCLUDE_CUTOUT_CMD defined? */

      else if (caseless_compare (command_name, CMD_CDVD_INFO))
	{ /* try to display startup file and volume label for an iin */
	  if (argc != 3)
	    show_usage_and_exit (argv [0], CMD_CDVD_INFO);

	  handle_result_and_exit (cdvd_info (argv [2]), argv [2], NULL);
	}

#if defined (INCLUDE_READ_TEST_CMD)
      else if (caseless_compare (command_name, CMD_READ_TEST))
	{
	  if (argc != 3)
	    show_usage_and_exit (argv [0], CMD_READ_TEST);

	  handle_result_and_exit (read_test (argv [2], get_progress ()), argv [2], NULL);
	}
#endif /* INCLUDE_READ_TEST_CMD defined? */

      else
	{ /* something else... -h perhaps? */
	  show_usage_and_exit (argv [0], command_name);
	}
    }
  else
    {
      show_usage_and_exit (argv [0], NULL);
    }
  return (0); /* please compiler */
}
