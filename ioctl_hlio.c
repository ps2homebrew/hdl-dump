/*
 * ioctl_hlio.c
 * $Id$
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
#include "ntddscsi.h"
#include "wnaspi32.h" /* for SCSI_xxx */
#include "retcodes.h"
#include "osal.h"


typedef unsigned long ioctl_handle_t;


/**************************************************************/
int
ioctl_open (const char *device,
	    ioctl_handle_t *handle)
{
  HANDLE dev_h = CreateFile (device, GENERIC_READ | GENERIC_WRITE,
			     FILE_SHARE_READ | FILE_SHARE_WRITE,
			     NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
  if (dev_h != INVALID_HANDLE_VALUE)
    {
      *handle = (ioctl_handle_t) dev_h;
      return (RET_OK);
    }
  else
    return (RET_ERR);
}


/**************************************************************/
int
ioctl_close (ioctl_handle_t handle)
{
  HANDLE dev_h = (HANDLE) handle;
  return (CloseHandle (dev_h) ? RET_OK : RET_ERR);
}


/**************************************************************/
int
ioctl_exec (ioctl_handle_t handle,
	    int host,
	    int scsi_id,
	    int lun,
	    const char cdb [16],
	    size_t cdb_len,
	    char *data,
	    size_t data_len,
	    int write_data) /* data direction: non-zero - write data; zero - read data */
{
  HANDLE dev_h = (HANDLE) handle;
  /* int host = 0, scsi_id = 0, lun = 0; */
  char spt_req [sizeof (SCSI_PASS_THROUGH) + 12];
  DWORD bytes_ret = 0;

  SCSI_PASS_THROUGH_DIRECT *spt = (SCSI_PASS_THROUGH_DIRECT*) spt_req;
  memset (spt_req, 0, sizeof (spt_req));
  spt->Length = sizeof (SCSI_PASS_THROUGH_DIRECT);
  spt->PathId = host;
  spt->TargetId = scsi_id;
  spt->Lun = lun;
  spt->CdbLength = cdb_len;
  spt->SenseInfoLength = 12;
  spt->DataIn = write_data ? SCSI_IOCTL_DATA_IN : SCSI_IOCTL_DATA_OUT;
  spt->DataTransferLength = data_len;
  spt->TimeOutValue = 10;
  spt->DataBuffer = data;
  spt->SenseInfoOffset = sizeof (SCSI_PASS_THROUGH_DIRECT);
  memcpy (spt->Cdb, cdb, cdb_len);

  if (DeviceIoControl (dev_h, IOCTL_SCSI_PASS_THROUGH_DIRECT, spt_req, sizeof (spt_req),
		       spt_req, sizeof (spt_req), &bytes_ret, NULL))
    { /* success */
      return (RET_OK);
    }
  else
    { /* failed */
      unsigned long err_no = osal_get_last_error_code ();
      return (RET_ERR);
    }
}


/**************************************************************/
/* NOTE: currently that would kill your Windows, so don't run as is */
int
ioctl_bus_scan (void)
{
  int result;
  char device_name [50];
  int index = 0;
  ioctl_handle_t handle;

  do
    {
      sprintf (device_name, "\\\\.\\CdRom%d", index); /* Scsi%d: */
      result = ioctl_open (device_name, &handle);
      if (result == RET_OK)
	{
	  char cdb [16];
	  char buffer [37];
	  int host, scsi_id, lun;

	  memset (cdb, 0, sizeof (cdb));
	  cdb [0] = SCSI_INQUIRY;
	  cdb [4] = sizeof (buffer) - 1;

	  for (host=0; host<16; ++host)
	    for (scsi_id=0; scsi_id<16; ++scsi_id)
	      for (lun=0; lun<8; ++lun)
		{
		  result = ioctl_exec (handle, host, scsi_id, lun, cdb, 6, buffer, sizeof (buffer) - 1, 0);
		  if (result == RET_OK)
		    {
		      printf ("%d:%d:%d\n", host, scsi_id, lun);
		    }
		}

	  ioctl_close (handle);
	}

      ++index;
    }
  while (result == RET_OK);

  return (0);
}
