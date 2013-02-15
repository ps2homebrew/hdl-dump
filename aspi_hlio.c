/*
 * aspi_hlio.c - ASPI high-level I/O
 * $Id: aspi_hlio.c,v 1.2 2004/08/15 16:44:19 b081 Exp $
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

#include "aspi_hlio.h"
#include <windows.h>
#include <time.h>
#include "wnaspi32.h"
#include "osal.h"
#include "retcodes.h"


/* after this timeout command is aborted */
#define ASPI_TIMEOUT_IN_SEC 10

/* types */
typedef DWORD (*aspi_send_cmd_t) (LPSRB srb);
typedef DWORD (*aspi_get_info_t) (void);

/* globals */
static int aspi_initialized = 0;
static HMODULE aspi_lib = NULL;
static aspi_send_cmd_t aspi_send_cmd;
static aspi_get_info_t aspi_get_info;
static int err_sense = 0, err_asc = 0, err_ascq = 0; /* last sense error */


/**************************************************************/
void
copy_and_reduce_spaces (char *out,
			const char *in)
{
  int last_were_space = 0;
  while (*in != '\0')
    {
      if (*in == ' ')
	{
	  if (last_were_space)
	    ++in;
	  else
	    *out++ = *in++;
	  last_were_space = 1;
	}
      else
	{
	  *out++ = *in++;
	  last_were_space = 0;
	}
    }
  if (last_were_space)
    --out;
  *out = '\0';
}


/**************************************************************/
int
aspi_load (void)
{
  int result;
  if (aspi_initialized)
    return (RET_OK);

#if 0
  /* try loading both sequentially */
  aspi_lib = LoadLibrary ("WNASPINT.DLL");
  if (aspi_lib == NULL)
#endif
    aspi_lib = LoadLibrary ("WNASPI32.DLL");
  if (aspi_lib != NULL)
    {
      aspi_send_cmd = (aspi_send_cmd_t) GetProcAddress (aspi_lib, "SendASPI32Command");
      aspi_get_info = (aspi_get_info_t) GetProcAddress (aspi_lib, "GetASPI32SupportInfo");

      if (aspi_send_cmd != NULL &&
	  aspi_get_info != NULL)
	{
	  DWORD support_info = aspi_get_info ();
	  if (HIBYTE (LOWORD (support_info)) == SS_COMP ||
	      HIBYTE (LOWORD (support_info)) == SS_NO_ADAPTERS)
	    {
	      result = RET_OK;
	      aspi_initialized = 1;
	    }
	  else
	    { /* give-up on error? */
	      result = RET_ERR;
	      aspi_unload ();
	    }
	}
      else
	{ /* no such procedures in the DLL */
	  result = RET_ERR;
	  aspi_unload ();
	}
    }
  else /* DLL not found */
    result = RET_ERR;
  return (result);
}


/**************************************************************/
int
aspi_unload (void)
{
  /* the old ASPI could crash if freed imediately after init */
  Sleep (200);

  /* unload and clean-up */
  aspi_get_info = NULL;
  aspi_send_cmd = NULL;

  if (aspi_lib != NULL)
    {
      FreeLibrary (aspi_lib);
      aspi_lib = NULL;
    }

  aspi_initialized = 0;
  return (RET_OK);
}


/**************************************************************/
static int
aspi_reset_device (int host,
		   int scsi_id,
		   int lun)
{
  SRB_BusDeviceReset reset;
  int result;

  memset (&reset, 0, sizeof (SRB_BusDeviceReset));

  reset.SRB_Cmd = SC_RESET_DEV;
  reset.SRB_HaId = host;
  reset.SRB_Target = scsi_id;
  reset.SRB_Lun = lun;

  result = aspi_send_cmd ((LPSRB) &reset);
  while (reset.SRB_Status == SS_PENDING)
    Sleep (1);
  if (reset.SRB_Status == SS_COMP)
    return (RET_OK);
  else
    return (RET_ERR);
}


/**************************************************************/
static int
aspi_rescan_host (int host)
{
  SRB_RescanPort rescan;
  memset (&rescan, 0, sizeof (SRB_RescanPort));
  rescan.SRB_Cmd = SC_RESCAN_SCSI_BUS;
  rescan.SRB_HaId = host;
  aspi_send_cmd ((LPSRB) &rescan);
  if (rescan.SRB_Status == SS_COMP ||
      rescan.SRB_Status == SS_NO_DEVICE)
    return (RET_OK);
  else
    return (RET_ERR);
}


/**************************************************************/
static int
aspi_exec (SRB_ExecSCSICmd *exec)
{
  HANDLE event = CreateEvent (NULL, TRUE, FALSE, NULL);
  if (event != NULL)
    { /* wait for event */
      exec->SRB_Flags |= SRB_EVENT_NOTIFY;
      exec->SRB_PostProc = (LPVOID) event;
      aspi_send_cmd ((LPSRB) exec);

      if (exec->SRB_Status == SS_PENDING)
	WaitForSingleObject (event, INFINITE);
      CloseHandle (event);
    }
  else
    { /* poll */
      aspi_send_cmd ((LPSRB) exec);
      while (exec->SRB_Status == SS_PENDING)
	Sleep (1);
    }
  /* process the result code */
  if (exec->SRB_Status == SS_COMP)
    return (RET_OK);
  else
    return (RET_ERR);
}


/**************************************************************/
static int
aspi_exec_to (SRB_ExecSCSICmd *exec,
	      DWORD timeout_in_ms)
{
  int abort = 0;
  HANDLE event = CreateEvent (NULL, TRUE, FALSE, NULL);
  if (event != NULL)
    { /* wait for event */
      exec->SRB_Flags |= SRB_EVENT_NOTIFY;
      exec->SRB_PostProc = (LPVOID) event;
      aspi_send_cmd ((LPSRB) exec);

      if (exec->SRB_Status == SS_PENDING)
	{
	  DWORD ret = WaitForSingleObject (event, timeout_in_ms);
	  if (ret == WAIT_OBJECT_0)
	    ; /* ok; signaled before timeout has expired */
	  else if (ret == WAIT_TIMEOUT)
	    abort = 1;
	}
      CloseHandle (event);
    }
  else
    { /* poll */
      DWORD start = GetTickCount ();
      aspi_send_cmd ((LPSRB) exec);
      while (exec->SRB_Status == SS_PENDING)
	{
	  DWORD elapsed = GetTickCount () - start;
	  if (elapsed >= timeout_in_ms)
	    {
	      abort = 1;
	      break;
	    }
	  Sleep (1);
	}
    }

  if (abort)
    { /* operation should be aborted */
      SRB_Abort abort;
      memset (&abort, 0, sizeof (SRB_Abort));
      abort.SRB_Cmd = SC_ABORT_SRB;
      abort.SRB_HaId = exec->SRB_HaId;
      abort.SRB_ToAbort = (void*) exec;
      aspi_send_cmd ((LPSRB) &abort);

      /* abort is issued; however should we wait for operation to be aborted? */
      /*
      while (exec->SRB_Status == SS_PENDING)
	Sleep (1);
      */
    }

  /* process the result code */
  if (exec->SRB_Status == SS_COMP)
    return (RET_OK);
  else
    {
      if (exec->SRB_TargStat == 0x02) /* check status */
	{
	  err_sense = exec->SenseArea [2] & 0x0f;
	  err_asc = exec->SenseArea [12];
	  err_ascq = exec->SenseArea [13];
	}
      return (RET_ERR);
    }
}


/**************************************************************/
static scsi_devices_list_t*
aspi_dlist_alloc (void)
{
  scsi_devices_list_t* list = (scsi_devices_list_t*) osal_alloc (sizeof (scsi_devices_list_t));
  if (list != NULL)
    {
      memset (list, 0, sizeof (scsi_devices_list_t));
    }
  return (list);
}


/**************************************************************/
static int
aspi_dlist_add (scsi_devices_list_t *list,
		int host,
		int scsi_id,
		int lun,
		int type,
		size_t align,
		const char *name)
{
  scsi_device_t *dev;
  if (list->used == list->alloc)
    {
      scsi_device_t *tmp =
	(scsi_device_t*) osal_alloc ((list->alloc + 16) * sizeof (scsi_device_t));
      if (tmp != NULL)
	{
	  if (list->device != NULL)
	    {
	      memcpy (tmp, list->device, list->used * sizeof (scsi_device_t));
	      osal_free (list->device);
	    }
	  list->device = tmp;
	  list->alloc += 16;
	}
      else
	return (RET_NO_MEM);
    }

  dev = list->device + list->used;
  dev->host = host;
  dev->scsi_id = scsi_id;
  dev->lun = lun;
  dev->type = type;
  dev->align = align;
  strcpy (dev->name, name);
  ++list->used;
  return (RET_OK);
}


/**************************************************************/
void
aspi_dlist_free (scsi_devices_list_t *list)
{
  if (list != NULL)
    {
      if (list->device)
	osal_free (list->device);
      osal_free (list);
    }
}


/**************************************************************/
static int
aspi_inquiry (int host,
	      int scsi_id,
	      int lun,
	      char device_name [28 + 1])
{
  char buffer [37];
  SRB_ExecSCSICmd exec;
  memset (&exec, 0, sizeof (SRB_ExecSCSICmd));
  exec.SRB_Cmd = SC_EXEC_SCSI_CMD;
  exec.SRB_HaId = host;
  exec.SRB_Flags = SRB_DIR_IN;
  exec.SRB_Target = scsi_id;
  exec.SRB_Lun = lun;
  exec.SRB_BufLen = sizeof (buffer) - 1;
  exec.SRB_BufPointer = (unsigned char*) buffer;
  exec.SRB_SenseLen = SENSE_LEN;
  exec.SRB_CDBLen = 6;

  /* SPC-R11A.PDF, 7.5 INQUIRY command; mandatory */
  exec.CDBByte [0] = SCSI_INQUIRY;
  exec.CDBByte [4] = sizeof (buffer) - 1;

  if (aspi_exec_to (&exec, ASPI_TIMEOUT_IN_SEC * 1000) == RET_OK &&
      exec.SRB_Status == SS_COMP)
    { /* 8-15: vendor Id; 16-31: product Id; 32-35: product revision */
      buffer [32] = '\0';
      copy_and_reduce_spaces (device_name, buffer + 8);
      return (RET_OK);
    }
  else
    return (RET_ERR);
}


/**************************************************************/
int
aspi_scan_scsi_bus (scsi_devices_list_t **list)
{
  int result = RET_OK;
  SRB_HAInquiry inq;
  *list = aspi_dlist_alloc ();
  if (*list == NULL)
    return (RET_NO_MEM);

  memset (&inq, 0, sizeof (SRB_HAInquiry));
  inq.SRB_Cmd = SC_HA_INQUIRY;
  inq.SRB_HaId = 0;
  aspi_send_cmd ((LPSRB) &inq);
  if (inq.SRB_Status == SS_COMP)
    { /* enumerate host adapters and get all devices attached */
      int host_adapter, host_adapters_cnt = inq.HA_Count;
      for (host_adapter=0; result == RET_OK && host_adapter<host_adapters_cnt; ++host_adapter)
	{
	  /*
	  aspi_rescan_host (host_adapter);
	  */

	  memset (&inq, 0, sizeof (SRB_HAInquiry));
	  inq.SRB_Cmd = SC_HA_INQUIRY;
	  inq.SRB_HaId = host_adapter;
	  aspi_send_cmd ((LPSRB) &inq);
	  if (inq.SRB_Status == SS_COMP)
	    {
	      int scsi_id, scsi_ids_cnt = inq.HA_Unique [3] == 0 ? 8 : inq.HA_Unique [3];
	      size_t alignment_mask = (inq.HA_Unique [1] << 8) | inq.HA_Unique [0];

	      switch (alignment_mask)
		{
		case 0x0000: alignment_mask = 1; break; /* byte alignment */
		case 0x0001: alignment_mask = 2; break; /* word alignment */
		case 0x0002: alignment_mask = 4; break; /* double-word alignment */
		case 0x0007: alignment_mask = 8; break; /* 8-bytes alignment */
		default:     ++alignment_mask;
		}

	      for (scsi_id=0; result == RET_OK && scsi_id<scsi_ids_cnt; ++scsi_id)
		{
		  int lun, lun_cnt = 8;
		  for (lun=0; result == RET_OK && lun<lun_cnt; ++lun)
		    {
		      SRB_GDEVBlock dtype;
		      memset (&dtype, 0, sizeof (SRB_GDEVBlock));
		      dtype.SRB_Cmd = SC_GET_DEV_TYPE;
		      dtype.SRB_HaId = host_adapter;
		      dtype.SRB_Target = scsi_id;
		      dtype.SRB_Lun = lun;
		      aspi_send_cmd ((LPSRB) &dtype);
		      if (dtype.SRB_Status == SS_COMP)
			{ /* device found */
			  char device_name [28 + 1];
#if 1
			  result = aspi_inquiry (host_adapter, scsi_id, lun, device_name);
#else
			  result = RET_ERR;
#endif
			  if (result == RET_OK)
			    result = aspi_dlist_add (*list, host_adapter, scsi_id, lun,
						     dtype.SRB_DeviceType, alignment_mask,
						     device_name);
			  else
			    result = aspi_dlist_add (*list, host_adapter, scsi_id, lun,
						     dtype.SRB_DeviceType, alignment_mask, "???");
#if 0 /* left to see how it is done */
			  const char *type;
			  char device_name [42];
			  switch (dtype.SRB_DeviceType)
			    {
			    case DTYPE_DASD:  type = "Direct access storage device"; break;
			    case DTYPE_SEQD:  type = "Sequential access storage device"; break;
			    case DTYPE_PRNT:  type = "Printer device"; break;
			    case DTYPE_PROC:  type = "Processor device"; break;
			    case DTYPE_WORM:  type = "WORM device"; break;
			    case DTYPE_CDROM: type = "CD-ROM device"; break;
			    case DTYPE_SCAN:  type = "Scanner device"; break;
			    case DTYPE_OPTI:  type = "Optical memory device"; break;
			    case DTYPE_JUKE:  type = "Medium changer device"; break;
			    case DTYPE_COMM:  type = "Communication device"; break;
			    default: type = "Unknown type";
			    }
			  printf ("%d:%d:%d ",
				  host_adapter, scsi_id, lun);
			  if (aspi_inquiry (host_adapter, scsi_id, lun, device_name) == RET_OK)
			    printf ("\t%s: %s\n", device_name, type);
			  else
			    printf ("\t???: %s\n", type);
#endif
			}
		    } /* LUN loop */
		} /* SCSI ID loop */
	    }
	} /* host adapters loop */
    }
  return (result);
}


/**************************************************************/
int
aspi_stat (int host,
	   int scsi_id,
	   int lun,
	   size_t *size_in_sectors,
	   size_t *sector_size)
{
  SRB_ExecSCSICmd exec;
  unsigned char capacity [8];
  int result;

  memset (&exec, 0, sizeof (SRB_ExecSCSICmd));
  exec.SRB_Cmd = SC_EXEC_SCSI_CMD;
  exec.SRB_HaId = host;
  exec.SRB_Flags = SRB_DIR_IN;
  exec.SRB_Target = scsi_id;
  exec.SRB_Lun = lun;
  exec.SRB_BufLen = sizeof (capacity);
  exec.SRB_BufPointer = capacity;
  exec.SRB_SenseLen = SENSE_LEN;
  exec.SRB_CDBLen = 10;

  /* MMC2R11A.PDF, 6.1.17 READ CAPACITY; mandatory */
  exec.CDBByte [0] = SCSI_RD_CAPAC;

  result = aspi_exec_to (&exec, ASPI_TIMEOUT_IN_SEC * 1000);
  if (result == RET_OK)
    {
      *size_in_sectors = (capacity [0] << 24 |
			  capacity [1] << 16 |
			  capacity [2] <<  8 |
			  capacity [3] <<  0) + 1;
      *sector_size = (capacity [4] << 24 |
		      capacity [5] << 16 |
		      capacity [6] <<  8 |
		      capacity [7] <<  0);
    }
  return (result);
}


/**************************************************************/
int
aspi_mmc_read_cd (int host,
		  int scsi_id,
		  int lun,
		  size_t start_sector,
		  size_t num_sectors,
		  size_t sector_size,
		  void *output)
{
  SRB_ExecSCSICmd exec;

  /* only those sector sizes are supported */
  if (sector_size != 2048 &&
      sector_size != 2352)
    return (RET_ERR);

  memset (&exec, 0, sizeof (SRB_ExecSCSICmd));
  exec.SRB_Cmd = SC_EXEC_SCSI_CMD;
  exec.SRB_HaId = host;
  exec.SRB_Flags = SRB_DIR_IN;
  exec.SRB_Target = scsi_id;
  exec.SRB_Lun = lun;
  exec.SRB_BufLen = num_sectors * sector_size;
  exec.SRB_BufPointer = (unsigned char*) output;
  exec.SRB_SenseLen = SENSE_LEN;

  /* MMC-R10A.PDF, 5.1.8 READ CD command; mandatory for CD-devices;
     suitable for CD-medias; some devices support it for DVD-medias */
  exec.SRB_CDBLen = 12;
  exec.CDBByte [ 0] = 0xBE;
  exec.CDBByte [ 1] = 0x00;
  exec.CDBByte [ 2] = (BYTE) ((start_sector >> 24) & 0xff);
  exec.CDBByte [ 3] = (BYTE) ((start_sector >> 16) & 0xff);
  exec.CDBByte [ 4] = (BYTE) ((start_sector >>  8) & 0xff);
  exec.CDBByte [ 5] = (BYTE) ((start_sector >>  0) & 0xff);
  exec.CDBByte [ 6] = (BYTE) ((num_sectors >> 16) & 0xff);
  exec.CDBByte [ 7] = (BYTE) ((num_sectors >>  8) & 0xff);
  exec.CDBByte [ 8] = (BYTE) ((num_sectors >>  0) & 0xff);
  exec.CDBByte [ 9] = sector_size == 2352 ? 0xF8 : 0x10;
  exec.CDBByte [10] = 0x00;
  exec.CDBByte [11] = 0x00;

  return (aspi_exec_to (&exec, ASPI_TIMEOUT_IN_SEC * 1000));
}


/**************************************************************/
int
aspi_read_10 (int host,
	      int scsi_id,
	      int lun,
	      size_t start_sector,
	      size_t num_sectors,
	      void *output)
{
  SRB_ExecSCSICmd exec;

  memset (&exec, 0, sizeof (SRB_ExecSCSICmd));
  exec.SRB_Cmd = SC_EXEC_SCSI_CMD;
  exec.SRB_HaId = host;
  exec.SRB_Flags = SRB_DIR_IN;
  exec.SRB_Target = scsi_id;
  exec.SRB_Lun = lun;
  exec.SRB_BufLen = num_sectors * 2048; /* sector size is assumed to be 2048-bytes */
  exec.SRB_BufPointer = (unsigned char*) output;
  exec.SRB_SenseLen = SENSE_LEN;

  /* SBC-R08C.PDF, 6.1.5 READ(10) command; mandatory for CD- and DVD-devices;
     suitable for CD- and DVD-medias */
  exec.SRB_CDBLen = 12;
  exec.CDBByte [ 0] = 0x28;
  exec.CDBByte [ 1] = 0x00;
  exec.CDBByte [ 2] = (BYTE) ((start_sector >> 24) & 0xff);
  exec.CDBByte [ 3] = (BYTE) ((start_sector >> 16) & 0xff);
  exec.CDBByte [ 4] = (BYTE) ((start_sector >>  8) & 0xff);
  exec.CDBByte [ 5] = (BYTE) ((start_sector >>  0) & 0xff);
  exec.CDBByte [ 6] = 0x00;
  exec.CDBByte [ 7] = (BYTE) ((num_sectors >>  8) & 0xff);
  exec.CDBByte [ 8] = (BYTE) ((num_sectors >>  0) & 0xff);
  exec.CDBByte [ 9] = 0x00;
  exec.CDBByte [10] = 0x00;
  exec.CDBByte [11] = 0x00;

  return (aspi_exec_to (&exec, ASPI_TIMEOUT_IN_SEC * 1000));
}
