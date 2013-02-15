/*
 * aspi_hlio.c - ASPI high-level I/O
 * $Id: aspi_hlio.c,v 1.3 2004/08/20 12:35:17 b081 Exp $
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
#include <stdio.h>
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
static int aspi_initialized = 0; /* reference counter */
static HMODULE aspi_lib = NULL;
static aspi_send_cmd_t aspi_send_cmd;
static aspi_get_info_t aspi_get_info;

/* NOTICE: those are not thread-safe */
static int err_srb_status = 0, err_sense = 0, err_asc = 0, err_ascq = 0;


/**************************************************************/
static void
aspi_set_last_error (int srb_status,
		     int sense_key,
		     int asc,
		     int ascq)
{
  /* NOTICE: that is not thread-safe */
  err_srb_status = srb_status;
  err_sense = sense_key;
  err_asc = asc;
  err_ascq = ascq;
}


/**************************************************************/
static void
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
    {
      ++aspi_initialized;
      return (RET_OK);
    }

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
  if (aspi_initialized)
    {
      --aspi_initialized;
      if (aspi_initialized == 0)
	{
	  /* the old ASPI could crash if freed imediately after init;
	     borrowed from don't remember where */
	  Sleep (200);

	  /* unload and clean-up */
	  aspi_get_info = NULL;
	  aspi_send_cmd = NULL;

	  if (aspi_lib != NULL)
	    {
	      FreeLibrary (aspi_lib);
	      aspi_lib = NULL;
	    }
	  return (RET_OK);
	}
    }
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
    {
      aspi_set_last_error (reset.SRB_Status, 0, 0, 0);
      return (RET_ERR);
    }
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
    {
      aspi_set_last_error (rescan.SRB_Status, 0, 0, 0);
      return (RET_ERR);
    }
}


/**************************************************************/
#if 0 /* not used */
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
	Sleep (1); /* don't 100% CPU, but Sleep (1) usually == Sleep (10) */
    }
  /* process the result code */
  if (exec->SRB_Status == SS_COMP)
    return (RET_OK);
  else
    { /* keep error details */
      int sense = exec->SenseArea [2] & 0x0f;
      int asc = exec->SenseArea [12];
      int ascq = exec->SenseArea [13];
      aspi_set_last_error (exec->SRB_Status, sense, asc, ascq);
      return (RET_ERR);
    }
}
#endif


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
	  Sleep (1); /* don't 100% CPU, but Sleep (1) usually == Sleep (10) */
	}
    }

  if (abort)
    { /* operation should be aborted */
      SRB_Abort abort;
      memset (&abort, 0, sizeof (SRB_Abort));
      abort.SRB_Cmd = SC_ABORT_SRB;
      abort.SRB_HaId = exec->SRB_HaId;
      abort.SRB_ToAbort = (void*) exec;
      /* abort is synchronious - would not return until operation is aborted */
      aspi_send_cmd ((LPSRB) &abort);
    }

  /* process the result code */
  if (exec->SRB_Status == SS_COMP)
    return (RET_OK);
  else
    { /* keep error details */
      int sense = exec->SenseArea [2] & 0x0f;
      int asc = exec->SenseArea [12];
      int ascq = exec->SenseArea [13];
      aspi_set_last_error (exec->SRB_Status, sense, asc, ascq);
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
		const char *name,
		size_t sector_size,
		size_t size_in_sectors)
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
  dev->sector_size = sector_size;
  dev->size_in_sectors = size_in_sectors;
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
    return (RET_ERR); /* aspi_exec_to would track error by itself */
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
			  size_t sector_size, size_in_sectors;

			  /* prepare for an error */
			  strcpy (device_name, "???");
			  sector_size = size_in_sectors = -1;
#if 1
			  /* return codes are intentionately ignored */
			  aspi_inquiry (host_adapter, scsi_id, lun, device_name);
			  aspi_stat (host_adapter, scsi_id, lun, &sector_size, &size_in_sectors);
#endif
			  result = aspi_dlist_add (*list, host_adapter, scsi_id, lun,
						   dtype.SRB_DeviceType, alignment_mask,
						   device_name, sector_size, size_in_sectors);

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
	   size_t *sector_size,
	   size_t *size_in_sectors)
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


/**************************************************************/
static const char*
aspi_srb_status_meaning (int status)
{
  switch (status)
    { /* from wnaspi32.h: */
    case SS_PENDING:                return ("SRB being processed");
    case SS_COMP:                   return ("SRB completed without error");
    case SS_ABORTED:                return ("SRB aborted");
    case SS_ABORT_FAIL:             return ("Unable to abort SRB");
    case SS_ERR:                    return ("SRB completed with error");
    case SS_INVALID_CMD:            return ("Invalid ASPI command");
    case SS_INVALID_HA:             return ("Invalid host adapter number");
    case SS_NO_DEVICE:              return ("SCSI device not installed");
    case SS_INVALID_SRB:            return ("Invalid parameter set in SRB");
    case SS_BUFFER_ALIGN:           return ("Buffer not aligned");
    case SS_ILLEGAL_MODE:           return ("Unsupported Windows mode");
    case SS_NO_ASPI:                return ("No ASPI managers resident");
    case SS_FAILED_INIT:            return ("ASPI for windows failed init");
    case SS_ASPI_IS_BUSY:           return ("No resources available to execute cmd");
    case SS_BUFFER_TO_BIG:          return ("Buffer size to big to handle");
    case SS_MISMATCHED_COMPONENTS:  return ("The DLLs/EXEs of ASPI don't version check");
    case SS_NO_ADAPTERS:            return ("No host adapters to manage");
    case SS_INSUFFICIENT_RESOURCES: return ("Couldn't allocate resources needed to init");
    case SS_ASPI_IS_SHUTDOWN:       return ("Call came to ASPI after PROCESS_DETACH");
    case SS_BAD_INSTALL:            return ("DLL or other components are installed wrong");
    default:                        return ("Unknown");
    }
}


/**************************************************************/
static const char*
aspi_sense_key_meaning (int sense)
{
  switch (sense)
    { /* from SPC-R11A.PDF: */
    case 0x00: return ("No sense");
    case 0x01: return ("Recovered error");
    case 0x02: return ("Not ready");
    case 0x03: return ("Medium error");
    case 0x04: return ("Hadrware error");
    case 0x05: return ("Illegal request");
    case 0x06: return ("Unit attention");
    case 0x07: return ("Data protect");
    case 0x08: return ("Blank check");
    case 0x09: return ("Vendor specific");
    case 0x0a: return ("Copy aborted");
    case 0x0b: return ("Aborted command");
    case 0x0c: return ("Obsolete");
    case 0x0d: return ("Volume overflow");
    case 0x0e: return ("Miscompare");
    case 0x0f: return ("Reserved");
    default:   return ("Unknown");
    }
}


/**************************************************************/
static const char*
aspi_sense_asc_ascq_meaning (int asc, int ascq)
{
  /* NOTICE: using a static buffer is not a thread-safe */
  static char message [100];

  /* from SPC-R11A.PDF: */
  /**/ if (asc == 0x00 && ascq == 0x00) return ("No additional sense information");
  else if (asc == 0x00 && ascq == 0x06) return ("I/O process terminated");
  else if (asc == 0x00 && ascq == 0x11) return ("Audio play operation in progress");
  else if (asc == 0x00 && ascq == 0x12) return ("Audio play operation paused");
  else if (asc == 0x00 && ascq == 0x13) return ("Audio play operation successfully completed");
  else if (asc == 0x00 && ascq == 0x14) return ("Audio play operation stopped due to error");
  else if (asc == 0x00 && ascq == 0x16) return ("Operation in progress");
  else if (asc == 0x00 && ascq == 0x17) return ("Cleaning requested");
  else if (asc == 0x04 && ascq == 0x00) return ("Logical unit not ready, cause not reportable");
  else if (asc == 0x04 && ascq == 0x01) return ("Logical unit is in process of becoming ready");
  else if (asc == 0x04 && ascq == 0x02) return ("Logical unit not ready, initializing cmd. reqd");
  else if (asc == 0x04 && ascq == 0x03) return ("Logical unit not ready, manual intervention reqd");
  else if (asc == 0x04 && ascq == 0x07) return ("Logical unit not ready, operation in progress");
  else if (asc == 0x04 && ascq == 0x08) return ("Logical unit not ready, long write in progress");
  else if (asc == 0x05 && ascq == 0x00) return ("Logical unit does not respond to selection");
  else if (asc == 0x08 && ascq == 0x00) return ("Logical unit communication failure");
  else if (asc == 0x08 && ascq == 0x01) return ("Logical unit communication time-out");
  else if (asc == 0x08 && ascq == 0x02) return ("Logical unit communication parity error");
  else if (asc == 0x08 && ascq == 0x03) return ("Logical unit communication CRC error");
  else if (asc == 0x09 && ascq == 0x00) return ("Track following error");
  else if (asc == 0x09 && ascq == 0x01) return ("Track servo failure");
  else if (asc == 0x10 && ascq == 0x00) return ("ID CRC or ECC error");
  else if (asc == 0x11 && ascq == 0x00) return ("Unrecovered read error");
  else if (asc == 0x11 && ascq == 0x06) return ("CIRC unrecovered error");
  else if (asc == 0x11 && ascq == 0x11) return ("Read error - loss of streaming");
  else if (asc == 0x15 && ascq == 0x00) return ("Random positioning error");
  else if (asc == 0x16 && ascq == 0x00) return ("Data synchronization mark error");
  else if (asc == 0x16 && ascq == 0x01) return ("Data sync error - data rewritten");
  else if (asc == 0x16 && ascq == 0x02) return ("Data sync error - reccomend rewrite");
  else if (asc == 0x16 && ascq == 0x03) return ("Data sync error - data auto-reallocated");
  else if (asc == 0x16 && ascq == 0x04) return ("Data sync error - reccomend reassignment");
  else if (asc == 0x1a && ascq == 0x00) return ("Parameter list length error");
  else if (asc == 0x1b && ascq == 0x00) return ("Synchronious data transfer error");
  else if (asc == 0x20 && ascq == 0x00) return ("Invalid command operation code");
  else if (asc == 0x21 && ascq == 0x00) return ("LBA (logical block address) out of range");
  else if (asc == 0x21 && ascq == 0x01) return ("Invalid element address");
  else if (asc == 0x24 && ascq == 0x00) return ("Invalid field in CDB");
  else if (asc == 0x26 && ascq == 0x00) return ("Invalid field in parameter list");
  else if (asc == 0x26 && ascq == 0x01) return ("Parameter not supported");
  else if (asc == 0x26 && ascq == 0x02) return ("Parameter value invalid");
  else if (asc == 0x29 && ascq == 0x04) return ("Device internal reset");
  else if (asc == 0x2b && ascq == 0x00) return ("Copy cannot execute since host cannot disconnect");
  else if (asc == 0x2c && ascq == 0x00) return ("Command sequence error");
  else if (asc == 0x2f && ascq == 0x00) return ("Commands cleared by another initiator");
  else if (asc == 0x30 && ascq == 0x00) return ("Incompatible medium installed");
  else if (asc == 0x30 && ascq == 0x01) return ("Cannot read medium - unknown format");
  else if (asc == 0x30 && ascq == 0x02) return ("Cannot read medium - incompatible format");
  else if (asc == 0x30 && ascq == 0x07) return ("Cleaning failure");
  else if (asc == 0x3a && ascq == 0x00) return ("Medium not present");
  else if (asc == 0x3a && ascq == 0x01) return ("Medium not present - tray closed");
  else if (asc == 0x3a && ascq == 0x02) return ("Medium not present - tray open");
  else if (asc == 0x3b && ascq == 0x04) return ("End of medium reached");
  else if (asc == 0x3d && ascq == 0x00) return ("Invalid bits in identify message");
  else if (asc == 0x3f && ascq == 0x02) return ("Changed operating definition");
  else if (asc == 0x3f && ascq == 0x03) return ("Inquiry data has changed");
  else if (asc == 0x40)
    {
      sprintf (message, "Diagnostic failure on component %02x", ascq);
      return (message);
    }
  else if (asc == 0x44 && ascq == 0x00) return ("Internal target failure");
  else if (asc == 0x47 && ascq == 0x00) return ("SCSI parity error");
  else if (asc == 0x49 && ascq == 0x00) return ("Invalid message error");
  else if (asc == 0x4a && ascq == 0x00) return ("Command phase error");
  else if (asc == 0x4b && ascq == 0x00) return ("Data phase error");
  else if (asc == 0x63 && ascq == 0x00) return ("End of user data encountered on this track");
  else if (asc == 0x64 && ascq == 0x00) return ("Illegal mode for this track");
  else if (asc == 0x64 && ascq == 0x01) return ("Invalid packet size");
  else if (asc == 0x73 && ascq == 0x00) return ("CD control error");

  else
    {
      sprintf (message, "ASC %02x, ASCQ %02x", asc, ascq);
      return (message);
    }
}


/**************************************************************/
unsigned long
aspi_get_last_error_code (void)
{
  /* NOTICE: that is not thread safe */
  return ((err_srb_status << 24) |
	  (err_sense << 16) |
	  (err_asc << 8) |
	  (err_ascq));
}


/**************************************************************/
const char*
aspi_get_last_error_msg (void)
{
  return (aspi_get_error_msg (aspi_get_last_error_code ()));
}


/**************************************************************/
const char*
aspi_get_error_msg (unsigned long aspi_error_code)
{
  int srb_status = (aspi_error_code >> 24) & 0xff;
  int sense_key = (aspi_error_code >> 16) & 0xff;
  int asc = (aspi_error_code >> 8) & 0xff;
  int ascq = aspi_error_code & 0xff;

  /**/ if (asc != 0x00 && ascq != 0x00)
    return (aspi_sense_asc_ascq_meaning (asc, ascq));
  else if (srb_status != SS_COMP)
    return (aspi_srb_status_meaning (srb_status));
  else
    return (aspi_sense_key_meaning (sense_key));
}


/**************************************************************/
void
aspi_dispose_error_msg (char *msg)
{
  /* do nothing */
  msg = NULL;
}
