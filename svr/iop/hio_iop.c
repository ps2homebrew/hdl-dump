/*
 * hio_iop.c
 * $Id: hio_iop.c,v 1.5 2005/08/06 12:03:13 bobi Exp $
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

#include <sysmem.h>
#include <sysclib.h>
#include <atad.h>
#include <dev9.h>
#include "net_io.h"
#include "hio_iop.h"
#include "retcodes.h"


typedef struct hio_iop_type
{
  hio_t hio;
  int unit;
  size_t size_in_sectors;
} hio_iop_t;


/**************************************************************/
static int
iop_stat (hio_t *hio,
	  u_int32_t *size_in_kb)
{
  hio_iop_t *iop = (hio_iop_t*) hio;
  *size_in_kb = iop->size_in_sectors / 2;
  return (RET_OK);
}


/**************************************************************/
static int
iop_read (hio_t *hio,
	  u_int32_t start_sector,
	  u_int32_t num_sectors,
	  void *output,
	  u_int32_t *bytes)
{
  hio_iop_t *iop = (hio_iop_t*) hio;
  int result = ata_device_dma_transfer (iop->unit, output,
					start_sector, num_sectors, ATA_DIR_READ);
  if (result == 0)
    {
      *bytes = num_sectors * HDD_SECTOR_SIZE;
      return (RET_OK);
    }
  else
    return (RET_ERR);
}


/**************************************************************/
static int
iop_write (hio_t *hio,
	   u_int32_t start_sector,
	   u_int32_t num_sectors,
	   const void *input,
	   u_int32_t *bytes)
{
  hio_iop_t *iop = (hio_iop_t*) hio;
  int result = ata_device_dma_transfer (iop->unit, (char*) input,
					start_sector, num_sectors, ATA_DIR_WRITE);
  if (result == 0)
    {
      *bytes = num_sectors * HDD_SECTOR_SIZE;
      return (RET_OK);
    }
  return (RET_ERR);
}


/**************************************************************/
static int
iop_flush (hio_t *hio)
{
  hio_iop_t *iop = (hio_iop_t*) hio;
  int result = ata_device_flush_cache (iop->unit);
  return (result);
}


/**************************************************************/
static int
iop_close (hio_t *hio)
{
  FreeSysMemory (hio);
  return (RET_OK);
}


/**************************************************************/
static int
iop_poweroff (hio_t *hio)
{
  /* dev9 shutdown; borrowed from ps2link */
  dev9IntrDisable (-1);
  dev9Shutdown ();

  *((unsigned char *) 0xbf402017) = 0x00;
  *((unsigned char *) 0xbf402016) = 0x0f;
  return (RET_OK);
}


/**************************************************************/
static hio_t*
iop_alloc (int unit,
	   size_t size_in_sectors)
{
  hio_iop_t *iop = AllocSysMemory (ALLOC_FIRST, sizeof (hio_iop_t), NULL);
  if (iop != NULL)
    {
      hio_t *hio = &iop->hio;
      hio->stat = &iop_stat;
      hio->read = &iop_read;
      hio->write = &iop_write;
      hio->flush = &iop_flush;
      hio->close = &iop_close;
      hio->poweroff = &iop_poweroff;
      iop->unit = unit;
      iop->size_in_sectors = size_in_sectors;
    }
  return ((hio_t*) iop);
}


/**************************************************************/
int
hio_iop_probe (const char *path,
	       hio_t **hio)
{
  if (path[0] == 'h' &&
      path[1] == 'd' &&
      path[2] == 'd' &&
      (path[3] >= '0' && path[3] <= '9') &&
      path[4] == ':' &&
      path[5] == '\0')
    {
      int unit = path [3] - '0';
      ata_devinfo_t *dev_info = ata_get_devinfo (unit);
      if (dev_info != NULL && dev_info->exists)
	{
	  *hio = iop_alloc (unit, dev_info->total_sectors);
	  if (*hio != NULL)
	    return (RET_OK);
	  else
	    return (RET_NO_MEM);
	}
    }
  return (RET_NOT_COMPAT);
}
