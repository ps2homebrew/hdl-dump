/*
 * iin_spti.c
 * $Id: iin_spti.c,v 1.1 2006/09/01 17:37:58 bobi Exp $
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

/*
 * written with the great help of PEOp.S. CDVD plug-in source code
 * http://sourceforge.net/projects/peops/
 */

#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include "scsidefs.h"
#include "wnaspi32.h"
#include "aspi_hlio.h"
#include "iin_spti.h"
#include "osal.h"
#include "retcodes.h"


typedef struct iin_spti_type
{
    iin_t iin;
    HANDLE dev;
    int host, scsi_id, lun;

    u_int8_t buf[IIN_NUM_SECTORS * IIN_SECTOR_SIZE];
    u_int32_t buf_len;
    unsigned long error_code; /* against osal_... */
} iin_spti_t;


/* NOTICE: those are not thread-safe */
static int err_srb_status = 0, err_sense = 0, err_asc = 0, err_ascq = 0;


/**************************************************************/
static void
spti_set_last_error(int srb_status,
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
unsigned long
spti_get_last_error_code(void)
{
    /* NOTICE: that is not thread safe */
    return ((err_srb_status << 24) |
            (err_sense << 16) |
            (err_asc << 8) |
            (err_ascq));
}


/**************************************************************/
const char *
spti_get_last_error_msg(void)
{
    return (spti_get_error_msg(spti_get_last_error_code()));
}


/**************************************************************/
const char *
spti_get_error_msg(unsigned long spti_error_code)
{
    return (aspi_get_error_msg(spti_error_code));
}


/**************************************************************/
static int
spti_exec(HANDLE dev, SRB_ExecSCSICmd *exec)
{
    SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sptd;
    DWORD retv = 0;
    BOOL ok;

    memset(&sptd, 0, sizeof(sptd));

    sptd.spt.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptd.spt.CdbLength = exec->SRB_CDBLen;
    sptd.spt.DataTransferLength = exec->SRB_BufLen;
    sptd.spt.TimeOutValue = 60;
    sptd.spt.DataBuffer = exec->SRB_BufPointer;
    sptd.spt.SenseInfoLength = 14;
    sptd.spt.TargetId = exec->SRB_Target;
    sptd.spt.Lun = exec->SRB_Lun;
    sptd.spt.SenseInfoOffset =
        offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);

    if (exec->SRB_Flags & SRB_DIR_IN)
        sptd.spt.DataIn = SCSI_IOCTL_DATA_IN;
    else if (exec->SRB_Flags & SRB_DIR_OUT)
        sptd.spt.DataIn = SCSI_IOCTL_DATA_OUT;
    else
        sptd.spt.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;

    memcpy(sptd.spt.Cdb, exec->CDBByte, exec->SRB_CDBLen);

    ok = DeviceIoControl(dev, IOCTL_SCSI_PASS_THROUGH_DIRECT, &sptd,
                         sizeof(sptd), &sptd, sizeof(sptd), &retv, NULL);

    /* copy sense info */
    memcpy(exec->SenseArea, sptd.ucSenseBuf, sizeof(exec->SenseArea));
    spti_set_last_error(0, exec->SenseArea[2] & 0x0f, exec->SenseArea[12],
                        exec->SenseArea[13]);

    if (ok) {
        if (err_sense == 0x00 /* no sense */ ||
            err_sense == 0x01 /* recovered */) {
            exec->SRB_Status = err_srb_status = SS_COMP;
            return (RET_OK);
        } else { /* SCSI error */
            exec->SRB_Status = err_srb_status = SS_ERR;
            return (RET_SPTI_ERROR);
        }
    } else { /* problem */
        exec->SRB_Status = err_srb_status = SS_ERR;
        return (RET_ERR);
    }
}


/**************************************************************/
static int
spti_stat(iin_t *iin,
          /*@out@*/ u_int32_t *sector_size,
          /*@out@*/ u_int32_t *num_sectors)
{
    const iin_spti_t *spti = (const iin_spti_t *)iin;
    SRB_ExecSCSICmd stat;
    int result;
    u_int8_t capacity[8];
    (void)aspi_prepare_stat(spti->host, spti->scsi_id, spti->lun,
                            capacity, &stat);

    result = spti_exec(spti->dev, &stat);
    if (result == RET_OK) {
        *num_sectors = ((u_int32_t)capacity[0] << 24 |
                        (u_int32_t)capacity[1] << 16 |
                        (u_int32_t)capacity[2] << 8 |
                        (u_int32_t)capacity[3] << 0) +
                       1;
        *sector_size = ((u_int32_t)capacity[4] << 24 |
                        (u_int32_t)capacity[5] << 16 |
                        (u_int32_t)capacity[6] << 8 |
                        (u_int32_t)capacity[7] << 0);
    }
    return (result);
}


/**************************************************************/
static int
spti_read(iin_t *iin,
          u_int32_t start_sector,
          u_int32_t num_sectors,
          /*@out@*/ const char **data,
          /*@out@*/ u_int32_t *length)
{
    const u_int32_t MAX_SECTORS_AT_ONCE = 32;
    iin_spti_t *spti = (iin_spti_t *)iin;
    int result = RET_OK;
    u_int32_t curr = start_sector;
    const u_int32_t end = start_sector + num_sectors;
    u_int8_t *p = spti->buf;

    while (result == RET_OK && curr < end) {
        const u_int32_t remaining = end - curr;
        const u_int32_t count = (remaining > MAX_SECTORS_AT_ONCE ?
                                     MAX_SECTORS_AT_ONCE :
                                     remaining);
        SRB_ExecSCSICmd read;
        (void)aspi_prepare_read_10(spti->host, spti->scsi_id, spti->lun,
                                   curr, count, p, &read);
        result = spti_exec(spti->dev, &read);
        if (result == RET_OK) {
            curr += count;
            p += count * IIN_SECTOR_SIZE;
        }
    }
    if (result == RET_OK) {
        *data = (const void *)spti->buf;
        *length = num_sectors * IIN_SECTOR_SIZE;
    }
    return (result);
}


/**************************************************************/
static int
spti_close(/*@special@*/ /*@only@*/ iin_t *iin) /*@releases iin@*/
{
    iin_spti_t *io = (iin_spti_t *)iin;
    (void)CloseHandle(io->dev);
    osal_free(iin);
    return (0);
}


/**************************************************************/
static char *
spti_last_error(iin_t *iin)
{
    iin_spti_t *io = (iin_spti_t *)iin;
    return (osal_get_error_msg(io->error_code));
}


/**************************************************************/
static void
spti_dispose_error(/*@unused@*/ iin_t *iin,
                   /*@only@*/ char *error)
{
    osal_dispose_error_msg(error);
}


/**************************************************************/
static iin_spti_t *
spti_alloc(HANDLE dev, int host, int scsi_id, int lun)
{
    iin_spti_t *ret = (iin_spti_t *)osal_alloc(sizeof(iin_spti_t));
    if (ret != NULL) {
        memset(ret, 0, sizeof(iin_spti_t));
        ret->iin.stat = &spti_stat;
        ret->iin.read = &spti_read;
        ret->iin.close = &spti_close;
        ret->iin.last_error = &spti_last_error;
        ret->iin.dispose_error = &spti_dispose_error;
        strcpy(ret->iin.source_type, "SPTI CDVD");

        ret->dev = dev;
        ret->host = host;
        ret->scsi_id = scsi_id;
        ret->lun = lun;

        ret->buf_len = sizeof(ret->buf);
    }
    return (ret);
}


/**************************************************************/
int iin_spti_probe_path(const char *path,
                        /*@special@*/ iin_p_t *iin) /*@allocates *hio@*/ /*@defines *hio@*/
{
    int result;
    *iin = NULL;
    if (isalpha(path[0]) && path[1] == ':' && path[2] == '\0') { /* check if drive is optical */
        char fn[20];
        sprintf(fn, "%c:\\", path[0]);
        if (GetDriveType(fn) == DRIVE_CDROM) {
            HANDLE dev;
            sprintf(fn, "\\\\.\\%c:", path[0]);
            dev = CreateFile(fn, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, 0, NULL);
            if (dev != INVALID_HANDLE_VALUE) { /* successfully opened */
                u_int8_t buf[1024] = {'\0'};
                DWORD retv = 0;
                SCSI_ADDRESS *sa = (SCSI_ADDRESS *)buf;
                sa->Length = sizeof(SCSI_ADDRESS);

                if (DeviceIoControl(dev, IOCTL_SCSI_GET_ADDRESS, NULL, 0,
                                    sa, sizeof(SCSI_ADDRESS), &retv, NULL)) {
#if 0
		  printf ("%c: @ %d:%d:%d using SPTI\n",
			  path[0], sa->PortNumber,
			  sa->TargetId, sa->Lun);
#endif
                    *iin = (iin_t *)spti_alloc(dev, sa->PortNumber,
                                               sa->TargetId, sa->Lun);
                    if (*iin)
                        return (RET_OK); /* success exit point */
                    else
                        result = RET_NO_MEM;
                } else
                    /* cannot get SCSI host/id/lun */
                    result = RET_ERR;

                (void)CloseHandle(dev);
            } else
                /* unable to open... */
                result = RET_ERR;
        } else
            result = RET_NOT_COMPAT; /* not an optical drive */
    } else
        result = RET_NOT_COMPAT; /* not in the correct format */
    return (result);
}
