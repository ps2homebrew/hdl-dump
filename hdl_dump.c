/*
 * hdl_dump.c
 * $Id: hdl_dump.c,v 1.17 2005/12/08 20:40:48 bobi Exp $
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
#include <signal.h>
#if defined (_BUILD_WIN32)
/* b0rken in cygwin's headers */
#  undef SIGINT
#  define SIGINT 2
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "byteseq.h"
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
#include "dict.h"
#include "aspi_hlio.h"


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
#define CMD_HDL_TOC "hdl_toc"
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
#define CMD_HDL_INSTALL "install"
#define CMD_CDVD_INFO "cdvd_info"
#if defined (INCLUDE_READ_TEST_CMD)
#  define CMD_READ_TEST "read_test"
#endif
#define CMD_POWER_OFF "poweroff"
#if defined (INCLUDE_CHECK_CMD)
#  define CMD_CHECK "check"
#endif
#if defined (INCLUDE_INITIALIZE_CMD)
#  define CMD_INITIALIZE "initialize"
#endif


/**************************************************************/
static void
show_apa_toc (const apa_partition_table_t *table)
{
  u_int32_t i;

  for (i=0; i<table->part_count; ++i)
    {
      const ps2_partition_header_t *part = &table->parts[i].header;
	  
      fprintf (stdout, "%06lx00%c%c %5luMB ",
	       (unsigned long) (get_u32 (&part->start) >> 8),
	       table->parts[i].existing ? '.' : '*',
	       table->parts[i].modified ? '*' : ':',
	       (unsigned long) (get_u32 (&part->length) / 2048));
      if (get_u32 (&part->main) == 0)
	fprintf (stdout, "%4x [%-*s]\n",
		 get_u16 (&part->type), PS2_PART_IDMAX, part->id);
      else
	fprintf (stdout, "      part # %2lu in %06lx00\n",
		 (unsigned long) (get_u32 (&part->number)),
		 (unsigned long) (get_u32 (&part->main) >> 8));
    }

  fprintf (stdout, "Total device size: %uMB, used: %uMB, available: %uMB\n",
	   (unsigned int) table->device_size_in_mb,
	   (unsigned int) (table->allocated_chunks * 128),
	   (unsigned int) (table->free_chunks * 128));
}


/**************************************************************/
#if defined (INCLUDE_MAP_CMD)
static void
show_apa_map (const apa_partition_table_t *table)
{
  /* show device map */
  const char *map = table->chunks_map;
  const u_int32_t GIGS_PER_ROW = 8;
  u_int32_t i, count = 0;

  for (i=0; i<table->total_chunks; ++i)
    {
      if (count == 0)
	fprintf (stdout, "%3uGB: ",
		 (unsigned int) ((i / ((GIGS_PER_ROW * 1024) / 128)) *
				 GIGS_PER_ROW));

      fputc (map[i], stdout);
      if ((count & 0x07) == 0x07)
	fputc (' ', stdout);

      if (++count == ((GIGS_PER_ROW * 1024) / 128)) /* 8G on each row */
	{
	  fputc ('\n', stdout);
	  count = 0;
	}
    }

  fprintf (stdout, "\nTotal device size: %uMB, used: %uMB, available: %uMB\n",
	   (unsigned int) table->device_size_in_mb,
	   (unsigned int) (table->allocated_chunks * 128),
	   (unsigned int) (table->free_chunks * 128));
}
#endif /* INCLUDE_MAP_CMD defined? */


/**************************************************************/
static int
show_toc (const dict_t *config,
	  const char *device_name)
{
  apa_partition_table_t *table;
  int result = apa_ptable_read (config, device_name, &table);
  if (result == RET_OK)
    {
      show_apa_toc (table);
      apa_ptable_free (table);
    }
  return (result);
}


/**************************************************************/
static int
show_hdl_toc (const dict_t *config,
	      const char *device_name)
{
  hio_t *hio;
  int result = hio_probe (config, device_name, &hio);
  if (result == RET_OK)
    {
      hdl_games_list_t *glist;
      result = hdl_glist_read (config, hio, &glist);
      if (result == RET_OK)
	{
	  u_int32_t i, j;
	  printf ("%-4s%9s %-*s %-12s %s\n",
		  "type", "size", MAX_FLAGS * 2 - 1, "flags",
		  "startup", "name");
	  for (i=0; i<glist->count; ++i)
	    {
	      const hdl_game_info_t *game = glist->games + i;
	      char compat_flags[MAX_FLAGS * 2 + 1] = { 0 };
	      for (j=0; j<MAX_FLAGS; ++j)
		if (game->compat_flags & (1 << j))
		  {
		    char buffer[5];
		    sprintf (buffer, "+%u", (unsigned int) (j + 1));
		    strcat (compat_flags, buffer);
		  }
	      printf ("%3s %7luKB %*s %-12s %s\n",
		      game->is_dvd ? "DVD" : "CD ",
		      game->total_size_in_kb,
		      MAX_FLAGS * 2 - 1, compat_flags + 1, /* trim leading + */
		      game->startup,
		      game->name);
	    }
	  printf ("total %uMB, used %uMB, available %uMB\n",
		  (unsigned int) (glist->total_chunks * 128),
		  (unsigned int) ((glist->total_chunks -
				   glist->free_chunks) * 128),
		  (unsigned int) (glist->free_chunks * 128));
	  hdl_glist_free (glist);
	}
      hio->close (hio);
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
	  u_int32_t partition_index;
	  result = apa_find_partition (table, game_name, &partition_index);
	  if (result == RET_NOT_FOUND)
	    { /* use heuristics - look among the HD Loader partitions */
	      char tmp[100];
	      strcpy (tmp, "PP.HDL.");
	      strcat (tmp, game_name);
	      result = apa_find_partition (table, tmp, &partition_index);
	    }

	  if (result == RET_OK)
	    { /* partition found */
	      const u_int32_t PART_SYSDATA_SIZE = 4 * 1024 * 1024;
	      unsigned char *buffer = osal_alloc (PART_SYSDATA_SIZE);
	      if (buffer != NULL)
		{
		  u_int32_t len;
		  result = hio->read (hio,
				      get_u32 (&table->parts[partition_index].header.start),
				      (4 _MB) / HDD_SECTOR_SIZE, buffer, &len);
		  if (result == OSAL_OK)
		    {
		      const char *signature = (char*) buffer + 0x001010ac;
		      const char *hdl_name = (char*) buffer + 0x00101008;
		      u_int32_t type = buffer[0x001010ec];
		      u_int32_t num_parts = buffer[0x001010f0];
		      const u_int32_t *data = (u_int32_t*) (buffer + 0x001010f5);
		      u_int32_t i;

		      if (buffer[0x00101000] == 0xed &&
			  buffer[0x00101001] == 0xfe &&
			  buffer[0x00101002] == 0xad &&
			  buffer[0x00101003] == 0xde)
			{ /* 0xdeadfeed magic found */
			  u_int64_t total_size = 0;

#if 0
			  /* save main partition incomplete header or debug purposes */
			  write_file (signature, buffer, 0x00101200);
#endif

			  fprintf (stdout, "%s: [%s], %s\n",
				   signature, hdl_name, (type == 0x14 ? "DVD" : "CD"));
			  if (buffer[0x1010a8] != 0)
			    {
			      char compat_flags[6 + 1] = { "" };
			      if (buffer[0x1010a8] & 0x01)
				strcat (compat_flags, "+1");
			      if (buffer[0x1010a8] & 0x02)
				strcat (compat_flags, "+2");
			      if (buffer[0x1010a8] & 0x04)
				strcat (compat_flags, "+3");
			      fprintf (stdout, "compatibility flags: %s\n",
				       compat_flags + 1);
			    }
			  for (i=0; i<num_parts; ++i)
			    {
			      unsigned long start = get_u32 (data + (i * 3 + 1));
			      unsigned long length = get_u32 (data + (i * 3 + 2));
			      total_size += ((u_int64_t) length) << 8;
			      fprintf (stdout,
				       "\tpart %2u is from sector 0x%06lx00, "
				       "%7luKB long\n",
				       (unsigned int) (i + 1),
				       start, length / 4);
			    }
			  fprintf (stdout,
				   "Total size: %luKB (%luMB approx.)\n",
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
			     u_int32_t size_in_mb)
{
  apa_partition_table_t *table;
  int result = apa_ptable_read (device_name, &table);
  if (result == RET_OK)
    {
      u_int32_t new_partition_index;
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
delete_partition (const dict_t *config,
		  const char *device_name,
		  const char *name)
{
  apa_partition_table_t *table;
  int result = apa_ptable_read (config, device_name, &table);
  if (result == RET_OK)
    {
      result = apa_delete_partition (table, name);
      if (result == RET_NOT_FOUND)
	{ /* assume `name' is game name, instead of partition name */
	  char partition_id[PS2_PART_IDMAX + 1];
	  result = hdl_lookup_partition (config, device_name, name, partition_id);
	  if (result == RET_OK)
	    result = apa_delete_partition (table, partition_id);
	}

      if (result == RET_OK)
	result = apa_commit (config, device_name, table);

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
	  const u_int32_t BUFF_SIZE = 4 * 1024 * 1024;
	  char *buffer = osal_alloc (BUFF_SIZE);
	  if (buffer != NULL)
	    {
	      u_int32_t read1 = 0, read2 = 0;
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
			      u_int32_t i;
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
	  static const unsigned char ZERO_BYTE = 0x7f;
	  u_int32_t bytes, counter = 0;
	  memset (buffer, ZERO_BYTE, 1 _MB);
	  do
	    {
	      result = osal_write (device, buffer, 1 _MB, &bytes);
	      if ((counter & 0x0f) == 0x0f)
		{
		  fprintf (stdout, "%.2fGB   \r", counter / 1024.0);
		  fflush (stdout);
		}
	      ++counter;
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
query_devices (const dict_t *config)
{
  osal_dlist_t *hard_drives;
  osal_dlist_t *optical_drives;

  int result = osal_query_devices (&hard_drives, &optical_drives);
  if (result == RET_OK)
    {
      u_int32_t i;

      fprintf (stdout, "Hard drives:\n");
      for (i=0; i<hard_drives->used; ++i)
	{
	  const osal_dev_t *dev = hard_drives->device + i;
	  fprintf (stdout, "\t%s ", dev->name);
	  if (dev->status == 0)
	    {
	      fprintf (stdout, "%lu MB",
		       (unsigned long) (dev->capacity / 1024) / 1024);
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

#if defined (BUILD_WINDOWS)
  if (dict_get_flag (config, CONFIG_ENABLE_ASPI_FLAG, 0))
    { /* ASPI support is enabled/disabled via config file, now */
      if (aspi_load () == RET_OK)
	{
	  scsi_devices_list_t *dlist;
	  result = aspi_scan_scsi_bus (&dlist);
	  if (result == RET_OK)
	    {
	      u_int32_t i;

	      fprintf (stdout, "\nOptical drives via ASPI:\n");
	      for (i=0; i<dlist->used; ++i)
		{
		  if (dlist->device[i].type == 0x05)
		    { /* MMC device */
		      printf ("\tcd%d:%d:%d  ",
			      dlist->device[i].host,
			      dlist->device[i].scsi_id,
			      dlist->device[i].lun);

		      if (dlist->device[i].name[0] != '\0')
			printf ("(%s):  ", dlist->device[i].name);
		      
		      if (dlist->device[i].size_in_sectors != -1 &&
			  dlist->device[i].sector_size != -1)
			printf ("%lu MB\n",
				(unsigned long) (((u_int64_t) dlist->device[i].size_in_sectors *
						  dlist->device[i].sector_size) / (1024 * 1024)));
		      else
			{
#if 1 /* used to be not really meaningful */
			  const char *error = aspi_get_error_msg (dlist->device[i].status);
			  printf ("%s\n", error);
			  aspi_dispose_error_msg ((char*) error);
#else
			  printf ("Stat failed.\n");
#endif
			}
		    }
		}
	      aspi_dlist_free (dlist);
	    }
	  else
	    fprintf (stderr, "\nError scanning SCSI bus.\n");
	  aspi_unload ();
	}
      else
	fprintf (stderr, "\nUnable to initialize ASPI.\n");
    }
#endif /* BUILD_WINDOWS defined? */

  return (result);
}


/**************************************************************/
static int
cdvd_info (const dict_t *config,
	   const char *path)
{
  iin_t *iin;
  int result = iin_probe (config, path, &iin);
  if (result == OSAL_OK)
    {
      char volume_id[32 + 1];
      char signature[12 + 1];
      u_int32_t num_sectors, sector_size;
      u_int64_t layer_pvd;
      result = iin->stat (iin, &sector_size, &num_sectors);
      if (result == OSAL_OK)
	result = isofs_get_ps_cdvd_details (iin, volume_id, signature, &layer_pvd);
      if (result == OSAL_OK)
	printf ("\"%s\" \"%s\" %s %luKB\n",
		signature, volume_id, (layer_pvd) ? ("dual layer") : (""),
		(unsigned long) (((u_int64_t) num_sectors * sector_size) / 1024));
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
      u_int32_t sector_size, num_sectors;
      result = iin->stat (iin, &sector_size, &num_sectors);
      if (result == OSAL_OK)
	{
	  u_int32_t sector = 0;
	  u_int32_t len;

	  pgs_prepare (pgs, (u_int64_t) num_sectors * sector_size);
	  do
	    {
	      const char *data; /* not used */
	      u_int32_t sectors = (num_sectors > IIN_NUM_SECTORS ? IIN_NUM_SECTORS : num_sectors);
	      /* TODO: would "buffer overflow" if read more than IIN_NUM_SECTORS? */
	      result = iin->read (iin, sector, sectors, &data, &len);
	      if (result == RET_OK)
		{
		  u_int32_t sectors_read = len / IIN_SECTOR_SIZE;
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
compare_iin (const dict_t *config,
	     const char *path1,
	     const char *path2,
	     progress_t *pgs)
{
  iin_t *iin1, *iin2;
  int different = 0;
  int longer_file = 0;

  int result = iin_probe (config, path1, &iin1);
  if (result == OSAL_OK)
    {
      result = iin_probe (config, path2, &iin2);
      if (result == OSAL_OK)
	{
	  u_int32_t len1, len2;
	  const char *data1, *data2;
	  u_int32_t sector = 0;
	  u_int32_t sector_size1, num_sectors1;
	  u_int32_t sector_size2, num_sectors2;
	  u_int64_t size1, size2;

	  result = iin1->stat (iin1, &sector_size1, &num_sectors1);
	  if (result == OSAL_OK)
	    {
	      size1 = (u_int64_t) num_sectors1 * sector_size1;
	      result = iin2->stat (iin2, &sector_size2, &num_sectors2);
	    }
	  if (result == OSAL_OK)
	    {
	      size2 = (u_int64_t) num_sectors2 * sector_size2;
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
	      u_int32_t sectors1 = (num_sectors1 > IIN_NUM_SECTORS ?
				 IIN_NUM_SECTORS : num_sectors1);
	      result = iin1->read (iin1, sector, sectors1, &data1, &len1);
	      if (result == OSAL_OK)
		{
		  u_int32_t sectors2 = (num_sectors2 > IIN_NUM_SECTORS ?
				     IIN_NUM_SECTORS : num_sectors2);
		  result = iin2->read (iin2, sector, sectors2, &data2, &len2);
		  if (result == OSAL_OK)
		    {
		      u_int32_t len = (len1 <= len2 ? len1 : len2); /* lesser from the two */
		      u_int32_t len_s = len / IIN_SECTOR_SIZE;

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
		      pgs_update (pgs, (u_int64_t) sector * IIN_SECTOR_SIZE);
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
int
inject (const dict_t *config,
	const char *output,
	const char *name,
	const char *input,
	const char *startup, /* or NULL */
	unsigned char compat_flags,
	int is_dvd,
	progress_t *pgs)
{
  hdl_game_t game;
  int result = RET_OK;
  iin_t *iin = NULL;
  hio_t *hio = NULL;

  result = iin_probe (config, input, &iin);
  if (result == RET_OK)
    result = hio_probe (config, output, &hio);
  if (result == RET_OK)
    {
      char volume_id[32 + 1], signature[12 + 1];
      u_int64_t layer_pvd;
      memset (&game, 0, sizeof (hdl_game_t));
      memcpy (game.name, name, sizeof (game.name) - 1);
      game.name[sizeof (game.name) - 1] = '\0';
      result = isofs_get_ps_cdvd_details (iin, volume_id, signature, &layer_pvd);
      if (result == RET_OK)
	{
	  if (layer_pvd != 0)
	    game.layer_break = (u_int32_t) layer_pvd - 16;
	  else
	    game.layer_break = 0;
	}
      if (startup != NULL)
	{ /* use given startup file */
	  memcpy (game.startup, startup, sizeof (game.startup) - 1);
	  game.startup[sizeof (game.startup) - 1] = '\0';
	  if (result == RET_NOT_PS_CDVD)
	    /* ... and ignore possible `not a PS2 CD/DVD' error */
	    result = RET_OK;
	}
      else
	{ /* we got startup file from above; fail if not PS2 CD/DVD */
	  if (result == RET_OK)
	    strcpy (game.startup, signature);
	}
      game.compat_flags = compat_flags;
      game.is_dvd = is_dvd;

      if (result == RET_OK)
	result = hdl_inject (config, hio, iin, &game, pgs);
    }

  if (hio != NULL)
    hio->close (hio);
  if (iin != NULL)
    iin->close (iin);

  return (result);
}


/**************************************************************/
static int
install (const dict_t *config,
	 const char *output,
	 const char *input,
	 progress_t *pgs)
{
  hdl_game_t game;
  iin_t *iin = NULL;
  hio_t *hio = NULL;
  int result;
  u_int32_t sector_size, num_sectors;

  result = iin_probe (config, input, &iin);
  if (result == RET_OK)
    result = iin->stat (iin, &sector_size, &num_sectors);
  if (result == RET_OK)
    result = hio_probe (config, output, &hio);
  if (result == RET_OK)
    {
      char volume_id[32 + 1], signature[12 + 1];
      u_int64_t layer_pvd;
      char name[HDL_GAME_NAME_MAX + 1];
      compat_flags_t flags;
      int incompatible;

      result = isofs_get_ps_cdvd_details (iin, volume_id, signature, &layer_pvd);
      if (result == RET_OK)
	result = ddb_lookup (config, signature, name, &flags);

      incompatible = result == RET_DDB_INCOMPATIBLE;
      if (incompatible)
	result = RET_OK;

      if (result == RET_OK)
	{
	  memset (&game, 0, sizeof (hdl_game_t));
	  strcpy (game.name, name);
	  if (layer_pvd != 0)
	    game.layer_break = (u_int32_t) layer_pvd - 16;
	  else
	    game.layer_break = 0;
	  strcpy (game.startup, signature);
	  game.compat_flags = flags;
	  /* TODO: the following math (assumption) might be incorrect */
	  game.is_dvd = ((u_int64_t) sector_size * num_sectors) > (750 _MB);

	  result = hdl_inject (config, hio, iin, &game, pgs);
	}
      result = (result == RET_OK && incompatible ? RET_DDB_INCOMPATIBLE : result);
    }

  if (hio != NULL)
    hio->close (hio);
  if (iin != NULL)
    iin->close (iin);

  return (result);
}

/**************************************************************/
static int
remote_poweroff (const dict_t *config,
		 const char *ip)
{
  hio_t *hio;
  int result = hio_probe (config, ip, &hio);
  if (result == RET_OK)
    {
      result = hio->poweroff (hio);
      hio->close (hio);
    }
  return (result);
}


/**************************************************************/
static volatile int sigint_catched = 0;

void
handle_sigint (int signo)
{
  sigint_catched = 1;
#if defined (_BUILD_WIN32)
  while (1)
    Sleep (1); /* endless loop; will end when main thread ends */
#endif
}

static int
progress_cb (progress_t *pgs, void *data)
{
  static time_t last_flush = 0;
  time_t now = time (NULL);

  if (pgs->remaining != -1)
    fprintf (stdout,
	     "%3d%%, %s remaining, %.2f MB/sec         \r",
	     pgs->pc_completed, pgs->remaining_text,
	     pgs->curr_bps / (1024.0 * 1024.0));
  else
    fprintf (stdout, "%3d%%\r", pgs->pc_completed);

  if (now > last_flush)
    { /* flush about once per second */
      fflush (stdout);
      last_flush = now;
    }

  return (!sigint_catched ? RET_OK : RET_INTERRUPTED);
}

/* progress is allocated, but never freed, which is not a big deal for a CLI app */
progress_t*
get_progress (void)
{
#if 0
  return (NULL);
#else
  progress_t *pgs = pgs_alloc (&progress_cb, NULL);
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
  } help[] =
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
	"hdd1:", "192.168.0.10", 0 },
      { CMD_HDL_TOC, "device",
	"Displays a list of all HD Loader games on the PlayStation 2 HDD.",
	"hdd1:", "192.168.0.10", 0 },
#if defined (INCLUDE_MAP_CMD)
      { CMD_MAP, "device",
	"Displays PlayStation 2 HDD usage map.",
	"hdd1:", NULL, 0 },
#endif
      { CMD_DELETE, "device partition/game",
	"Deletes PlayStation 2 HDD partition. First attempts to locate partition\n"
	"by name, then by game name.",
	"hdd1: \"PP.HDL.Tekken Tag Tournament\"", "hdd1: \"Tekken Tag Tournament\"", 1 },
#if defined (INCLUDE_ZERO_CMD)
      { CMD_ZERO, "device",
	"Fills HDD with zeroes. All information on the HDD will be lost.",
	"hdd1:", NULL, 1 },
#endif
#if defined (INCLUDE_CUTOUT_CMD)
      { CMD_CUTOUT, "device size_in_MB",
	"Displays partition table as if a new partition has been created.",
	"hdd1: 2560", NULL, 0 },
#endif
#if defined (INCLUDE_INFO_CMD)
      { CMD_HDL_INFO, "device partition",
	"Displays information about HD Loader partition.",
	"hdd1: \"tekken tag tournament\"", NULL, 0 },
#endif
      { CMD_HDL_EXTRACT, "device name output_file",
	"Extracts application image from HD Loader partition.",
	"hdd1: \"tekken tag tournament\" c:\\tekken.iso", NULL, 0 },
      { CMD_HDL_INJECT_CD, "target name source [startup] [flags]",
	"Creates a new HD Loader partition from a CD.\n"
	"Supported inputs: plain ISO files, CDRWIN cuesheets, Nero images and tracks,\n"
	"RecordNow! Global images, HD Loader partitions (hdd1:PP.HDL.Xenosaga) and\n"
	"Sony CD/DVD generator IML files (if files are listed with full paths).\n"
	"Startup file and compatibility flags are optional. Flags syntax is\n"
	"`+#[+#[+#]]' or `0xNN', for example `+1', `+2+3', `0x01', `0x03', etc.",
	"192.168.0.10 \"Tekken Tag Tournament\" cd0: SCES_xxx.xx",
	"hdd1: \"Tekken\" c:\\tekken.iso SCES_xxx.xx +1+2", 1 },
      { CMD_HDL_INJECT_DVD, "target name source [startup] [flags]",
	"Creates a new HD Loader partition from a DVD.\n"
	"DVD9 cannot be directly installed from the DVD-ROM drive -\n"
	"use ISO image or IML file instead.\n"
	"Supported inputs: plain ISO files, CDRWIN cuesheets, Nero images and tracks,\n"
	"RecordNow! Global images, HD Loader partitions (hdd1:PP.HDL.Xenosaga) and\n"
	"Sony CD/DVD generator IML files (if files are listed with full paths).\n"
	"Startup file and compatibility flags are optional. Flags syntax is\n"
	"`+#[+#[+#]]' or `0xNN', for example `+1', `+2+3', `0x01', `0x03', etc.",
	"192.168.0.10 \"Gran Turismo 3\" cd0:",
	"hdd1: \"Gran Turismo 3\" c:\\gt3.iso SCES_xxx.xx +2+3", 1 },
      { CMD_HDL_INSTALL, "target source",
	"Creates a new HD Loader partition from a source, that has an entry\n"
	"in compatibility list.",
	"192.168.0.10 cd0:", "hdd1: c:\\gt3.iso", 1 },
      { CMD_CDVD_INFO, "iin_input",
	"Displays signature (startup file), volume label and data size\n"
	"for a CD-/DVD-drive or image file.",
	"c:\\gt3.gi", "\"hdd2:Gran Turismo 3\"", 0 },
      { CMD_POWER_OFF, "ip",
	"Powers-off Playstation 2.",
	"192.168.0.10", NULL, 0 },
#if defined (INCLUDE_READ_TEST_CMD)
      { CMD_READ_TEST, "iin_input",
	"Consecutively reads all sectors from the specified input.",
	"cd0:", NULL, 0 },
#endif /* INCLUDE_READ_TEST_CMD defined? */
#if defined (INCLUDE_CHECK_CMD)
      { CMD_CHECK, "device",
	"Attempts to locate and display partition errors.",
	"hdd1:", "192.168.0.10", 0 },
#endif /* INCLUDE_CHECK_CMD defined? */
#if defined (INCLUDE_INITIALIZE_CMD)
      { CMD_INITIALIZE, "device",
	"Prepares a HDD for HD Loader usage. All information on the HDD will be lost.\n",
	"hdd1:", NULL, 1 },
#endif /* INCLUDE_INITIALIZE_CMD defined? */
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

  fprintf (stdout,
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
			 char output[MAX_PATH])
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

    case RET_INTERRUPTED:
      fprintf (stderr, "\nInterrupted.\n");
      exit (100 + RET_INTERRUPTED);

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

    case RET_CROSS_128GB:
      fprintf (stderr, "Unable to limit HDD size to 128GB - data behind 128GB mark.\n");
      exit (100 + RET_CROSS_128GB);

#if defined (BUILD_WINDOWS)
    case RET_ASPI_ERROR:
      fprintf (stderr, "ASPI error: 0x%08lx (SRB/Sense/ASC/ASCQ) %s\n",
	       aspi_get_last_error_code (),
	       aspi_get_last_error_msg ());
      exit (100 + RET_ASPI_ERROR);
#endif

    case RET_NO_DISC_DB:
      fprintf (stderr, "Disc database file could not be found.\n");
      exit (100 + RET_NO_DISC_DB);

    case RET_NO_DDBENTRY:
      fprintf (stderr, "There is no entry for that game in the disc database.\n");
      exit (100 + RET_NO_DDBENTRY);

    case RET_DDB_INCOMPATIBLE:
      fprintf (stderr, "Game is incompatible, according to disc database.\n");
      exit (100 + RET_DDB_INCOMPATIBLE);

    case RET_TIMEOUT:
      fprintf (stderr, "Network communication timeout.\n");
      exit (100 + RET_TIMEOUT);

    case RET_PROTO_ERR:
      fprintf (stderr, "Network communication protocol error.\n");
      exit (100 + RET_PROTO_ERR);

    default:
      fprintf (stderr, "%s: don't know what the error is: %d.\n", device, result);
      exit (200);
    }
}


int
main (int argc, char *argv[])
{
  dict_t *config = NULL;
  char config_file[256] = { "" };

  /* decide where config file is */
#if defined (_BUILD_WIN32)
  char *profile = getenv ("USERPROFILE");
  if (profile != NULL)
    {
      strcpy (config_file, profile);
      strcat (config_file, "\\Application Data\\hdl_dump.conf");
    }
  else
    strcpy (config_file, "./hdl_dump.conf");
#elif defined (_BUILD_UNIX)
  char *home = getenv ("HOME");
  if (home != NULL)
    {
      strcpy (config_file, home);
      strcat (config_file, "/.hdl_dump.conf");
    }
  else
    strcpy (config_file, "./hdl_dump.conf");
#endif

  config = dict_alloc ();
  if (config != NULL)
    { /* initialize defaults */
      dict_put_flag (config, CONFIG_LIMIT_TO_28BIT_FLAG, 1);
      dict_put_flag (config, CONFIG_ENABLE_ASPI_FLAG, 0);
      dict_put (config, CONFIG_PARTITION_NAMING,
		CONFIG_PARTITION_NAMING_TOXICOS);
#if 0
      dict_put_flag (config, CONFIG_USE_COMPRESSION_FLAG, 1);
#endif
      dict_put (config, CONFIG_DISC_DATABASE_FILE, "./hdl_dump.list");

#if defined (_BUILD_WIN32)
      /* good combinations for Win32:
       *  5/2 = 2,05 MB/sec,
       * 10/3 = 2,06 MB/sec,
       * 14/4 = 2,07 MB/sec */
      dict_put (config, CONFIG_UDP_QUICK_PACKETS, "5");
      dict_put (config, CONFIG_UDP_DELAY_TIME, "2");
#else
      dict_put (config, CONFIG_UDP_QUICK_PACKETS, "7");
      dict_put (config, CONFIG_UDP_DELAY_TIME, "1000");
#endif
      dict_restore (config, config_file);
      dict_store (config, config_file);
    }

  /* handle Ctrl+C gracefully */
  signal (SIGINT, &handle_sigint);

#if 0
  test2 (config);
  return (0);
#endif

  if (argc > 1)
    {
      const char *command_name = argv[1];

      if (caseless_compare (command_name, CMD_QUERY))
	{ /* show all devices */
	  handle_result_and_exit (query_devices (config), NULL, NULL);
	}

#if defined (INCLUDE_DUMP_CMD)
      else if (caseless_compare (command_name, CMD_DUMP))
	{ /* dump CD/DVD-ROM to the HDD */
	  char device_name[MAX_PATH];

	  if (argc != 4)
	    show_usage_and_exit (argv[0], CMD_DUMP);

	  map_device_name_or_exit (argv[2], device_name);

	  handle_result_and_exit (dump_device (device_name, argv[3], 0, get_progress ()),
				  argv[2], NULL);
	}
#endif /* INCLUDE_DUMP_CMD defined? */

#if defined (INCLUDE_COMPARE_CMD)
      else if (caseless_compare (command_name, CMD_COMPARE))
	{ /* compare two files or devices or etc. */
	  char device_name_1[MAX_PATH];
	  char device_name_2[MAX_PATH];
	  int is_device_1 = 0, is_device_2 = 0;

	  if (argc != 4)
	    show_usage_and_exit (argv[0], CMD_COMPARE);

	  is_device_1 = osal_map_device_name (argv[2], device_name_1) == RET_OK ? 1 : 0;
	  is_device_2 = osal_map_device_name (argv[3], device_name_2) == RET_OK ? 1 : 0;

	  handle_result_and_exit (compare (is_device_1 ? device_name_1 : argv[2], is_device_1,
					   is_device_2 ? device_name_2 : argv[3], is_device_2),
				  NULL, NULL);
	}
#endif /* INCLUDE_COMPARE_CMD defined? */

#if defined (INCLUDE_COMPARE_IIN_CMD)
      else if (caseless_compare (command_name, CMD_COMPARE_IIN))
	{ /* compare two iso inputs */
	  if (argc != 4)
	    show_usage_and_exit (argv[0], CMD_COMPARE_IIN);
	  handle_result_and_exit (compare_iin (config, argv[2], argv[3],
					       get_progress ()),
				  NULL, NULL);
	}
#endif /* INCLUDE_COMPARE_IIN_CMD defined? */

      else if (caseless_compare (command_name, CMD_TOC))
	{ /* show TOC of a PlayStation 2 HDD */
	  if (argc != 3)
	    show_usage_and_exit (argv[0], CMD_TOC);
	  handle_result_and_exit (show_toc (config, argv[2]), argv[2], NULL);
	}

      else if (caseless_compare (command_name, CMD_HDL_TOC))
	{ /* show a TOC of HD Loader games only */
	  if (argc != 3)
	    show_usage_and_exit (argv[0], CMD_HDL_TOC);
	  handle_result_and_exit (show_hdl_toc (config, argv[2]), argv[2], NULL);
	}

#if defined (INCLUDE_MAP_CMD)
      else if (caseless_compare (command_name, CMD_MAP))
	{ /* show map of a PlayStation 2 HDD */
	  if (argc != 3)
	    show_usage_and_exit (argv[0], CMD_MAP);

	  handle_result_and_exit (show_map (argv[2]), argv[2], NULL);
	}
#endif /* INCLUDE_MAP_CMD defined? */

      else if (caseless_compare (command_name, CMD_DELETE))
	{ /* delete partition */
	  if (argc != 4)
	    show_usage_and_exit (argv[0], CMD_DELETE);

	  handle_result_and_exit (delete_partition (config, argv[2], argv[3]),
				  argv[2], argv[3]);
	}

#if defined (INCLUDE_ZERO_CMD)
      else if (caseless_compare (command_name, CMD_ZERO))
	{ /* zero HDD */
	  char device_name[MAX_PATH];

	  if (argc != 3)
	    show_usage_and_exit (argv[0], CMD_ZERO);

	  map_device_name_or_exit (argv[2], device_name);

	  handle_result_and_exit (zero_device (device_name),
				  argv[2], NULL);
	}
#endif /* INCLUDE_ZERO_CMD defined? */

#if defined (INCLUDE_INFO_CMD)
      else if (caseless_compare (command_name, CMD_HDL_INFO))
	{ /* show HD Loader game info */
	  if (argc != 4)
	    show_usage_and_exit (argv[0], CMD_HDL_INFO);

	  handle_result_and_exit (show_hdl_game_info (argv[2], argv[3]),
				  argv[2], argv[3]);
	}
#endif /* INCLUDE_INFO_CMD defined? */

      else if (caseless_compare (command_name, CMD_HDL_EXTRACT))
	{ /* extract game image from a HD Loader partition */
	  if (argc != 5)
	    show_usage_and_exit (argv[0], CMD_HDL_EXTRACT);

	  handle_result_and_exit (hdl_extract (config, argv[2], argv[3],
					       argv[4], get_progress ()),
				  argv[2], argv[3]);
	}

      else if (caseless_compare (command_name, CMD_HDL_INJECT_CD) ||
	       caseless_compare (command_name, CMD_HDL_INJECT_DVD))
	{ /* inject game image into a new HD Loader partition */
	  unsigned char compat_flags = 0, have_startup = 1;
	  unsigned char media =
	    caseless_compare (command_name, CMD_HDL_INJECT_CD) ? 0 : 1;

	  if (!(argc >= 5 && argc <= 7))
	    show_usage_and_exit (argv[0], command_name);

	  /* parse compatibility flags */
	  if (argc == 7)
	    /* startup + compatibility flags */
	    compat_flags = parse_compat_flags (argv[6]);
	  else if (argc == 6 &&
		   (argv[5][0] == '+' ||
		    (argv[5][0] == '0' && argv[5][1] == 'x')))
	    { /* compatibility flags only */
	      compat_flags = parse_compat_flags (argv[5]);
	      have_startup = 0;
	    }
	  else if (argc == 5)
	    /* neither */
	    have_startup = 0;
	  if (compat_flags == COMPAT_FLAGS_INVALID)
	    show_usage_and_exit (argv[0], command_name);

	  handle_result_and_exit (inject (config, argv[2], argv[3], argv[4],
					  have_startup ? argv[5] : NULL,
					  compat_flags, media, get_progress ()),
				  argv[2], argv[3]);
	}

      else if (caseless_compare (command_name, CMD_HDL_INSTALL))
	{
	  if (argc != 4)
	    show_usage_and_exit (argv[0], CMD_HDL_INSTALL);

	  handle_result_and_exit (install (config, argv[2], argv[3], get_progress ()),
				  argv[2], argv[3]);
	}

#if defined (INCLUDE_CUTOUT_CMD)
      else if (caseless_compare (command_name, CMD_CUTOUT))
	{ /* calculate and display how to arrange a new HD Loader partition */
	  char device_name[MAX_PATH];

	  if (argc != 4)
	    show_usage_and_exit (argv[0], CMD_CUTOUT);

	  map_device_name_or_exit (argv[2], device_name);

	  handle_result_and_exit (show_apa_cut_out_for_inject (device_name, atoi (argv[3])),
				  argv[2], NULL);
	}
#endif /* INCLUDE_CUTOUT_CMD defined? */

      else if (caseless_compare (command_name, CMD_CDVD_INFO))
	{ /* try to display startup file and volume label for an iin */
	  if (argc != 3)
	    show_usage_and_exit (argv[0], CMD_CDVD_INFO);

	  handle_result_and_exit (cdvd_info (config, argv[2]), argv[2], NULL);
	}

#if defined (INCLUDE_READ_TEST_CMD)
      else if (caseless_compare (command_name, CMD_READ_TEST))
	{
	  if (argc != 3)
	    show_usage_and_exit (argv[0], CMD_READ_TEST);

	  handle_result_and_exit (read_test (argv[2], get_progress ()), argv[2], NULL);
	}
#endif /* INCLUDE_READ_TEST_CMD defined? */

      else if (caseless_compare (command_name, CMD_POWER_OFF))
	{ /* PS2 power-off */
	  if (argc != 3)
	    show_usage_and_exit (argv[0], CMD_POWER_OFF);
	  
	  handle_result_and_exit (remote_poweroff (config, argv[2]), argv[2], NULL);
	}

#if defined (INCLUDE_CHECK_CMD)
      else if (caseless_compare (command_name, CMD_CHECK))
	{ /* attempt to locate and display partition errors */
	  if (argc != 3)
	    show_usage_and_exit (argv[0], CMD_CHECK);

	  handle_result_and_exit (check (argv[2]), argv[2], NULL);
	}
#endif /* INCLUDE_CHECK_CMD defined? */

#if defined (INCLUDE_INITIALIZE_CMD)
      else if (caseless_compare (command_name, CMD_INITIALIZE))
	{ /* prepare a HDD for HD Loader usage */
	  if (argc != 3)
	    show_usage_and_exit (argv[0], CMD_INITIALIZE);

	  handle_result_and_exit (apa_initialize (config, argv[2]), argv[2], NULL);
	}
#endif /* INCLUDE_INITIALIZE_CMD defined? */

      else
	{ /* something else... -h perhaps? */
	  show_usage_and_exit (argv[0], command_name);
	}
    }
  else
    {
      show_usage_and_exit (argv[0], NULL);
    }
  return (0); /* please compiler */
}
