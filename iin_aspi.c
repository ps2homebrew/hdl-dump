/*
 * iin_aspi.c
 * $Id: iin_aspi.c,v 1.3 2004/08/15 16:44:19 b081 Exp $
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

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "wnaspi32.h"
#include "iin_aspi.h"
#include "retcodes.h"
#include "iin.h"
#include "aspi_hlio.h"


typedef struct iin_aspi_type
{
  iin_t iin;
  int host, scsi_id, lun;
} iin_aspi_t;










/**************************************************************/
#if 0
int
go_aspi (void)
{
  scsi_devices_list_t *aspi_devices_list;
  int result = aspi_load ();
  if (result == RET_OK)
    {
      size_t sector;
      size_t size_in_sectors;
#define ASPI_NUM_SECTORS 16
      char buffer [2048 * ASPI_NUM_SECTORS];
      int host = 1, scsi_id = 1, lun = 0;

      result = aspi_scan_scsi_bus (&aspi_devices_list);
      if (result == RET_OK)
	{
	  size_t i;
	  for (i=0; i<aspi_devices_list->used; ++i)
	    printf ("[%d:%d:%d] \"%s\" : %d, %d bytes alignment reqd\n",
		    aspi_devices_list->device [i].host,
		    aspi_devices_list->device [i].scsi_id,
		    aspi_devices_list->device [i].lun,
		    aspi_devices_list->device [i].name,
		    aspi_devices_list->device [i].type,
		    aspi_devices_list->device [i].align);
	}

#if 0
      result = aspi_stat (host, scsi_id, lun, &size_in_sectors);
      if (result == RET_OK)
	{
	  clock_t start = clock ();

	  printf ("size: %d sectors\n", size_in_sectors);

	  sector = 0 /* 2000000 */;
	  do
	    {
	      size_t sectors_to_read = ASPI_NUM_SECTORS;

	      if (sector + ASPI_NUM_SECTORS > size_in_sectors)
		sectors_to_read = size_in_sectors - sector;
	      result = aspi_read_10 (host, scsi_id, lun, sector, sectors_to_read, buffer);
	      if (result == RET_OK)
		{
		  clock_t elapsed = clock () - start;
		  sector += sectors_to_read;
		  printf ("sector %7d, %3.1f sec, %2.1fMBps\r",
			  sector, (double) elapsed / CLOCKS_PER_SEC,
			  ((double) sector * 2048) / ((double) elapsed / CLOCKS_PER_SEC) / (1024.0 * 1024.0));
		}
	    }
	  while (result == RET_OK);
	}
#endif

      aspi_unload ();
    }
}
#endif
