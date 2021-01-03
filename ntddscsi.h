/*
 * borrowed from http://members.aol.com/plscsi/tools/gccscsi/
 *
 * ntddscsi.h
 *
 * This source file for Win 2K/XP exists to make:
 *
 *  #include "ntddscsi.h"
 *
 * work, with both Microsoft Visual C++, and
 * also in the gcc of:
 *
 *  http://www.mingw.org/
 *
 * Most of the names and the values equated here
 * match those you can find in the version of
 * Microsoft's "ntddk/inc/ntddscsi.h" that is tagged:
 *
 *  BUILD Version: 0001
 *  Copyright (c) 1990-1999  Microsoft Corporation
 *
 * That version in turn matches what you can view
 * in such places as:
 *
 *  Windows DDK: System Support for Buses: DeviceIoControl struct SCSI_PASS_THROUGH
 *  http://msdn.microsoft.com/library/en-us/buses/hh/buses/scsi_struct_34tu.asp
 *
 *  Windows DDK: System Support for Buses: DeviceIoControl struct SCSI_PASS_THROUGH_DIRECT
 *  http://msdn.microsoft.com/library/en-us/buses/hh/buses/scsi_struct_3qnm.asp
 *
 * Microsoft's ntddk/ version is more complete.  This
 * version contains just enough names to make
 * DeviceIoControl work, and may be more closely
 * tied to the 32-bit versions of Windows.
 *
 * Microsoft's version also implicitly depends on:
 *
 *  #include <devioctl.h>
 *
 * We did try the alternative: -I /ntddk/inc/
 * but we saw an explosion of gcc -Wall complaints
 * in <ctype.h>, in <jni.h>, etc.
 */

#ifndef NTDDSCSI_H
#define NTDDSCSI_H NTDDSCSI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Define enough to talk IOCTL_SCSI_PASS_THROUGH. */

#define IOCTL_SCSI_PASS_THROUGH 0x4D004

#define SCSI_IOCTL_DATA_OUT 0
#define SCSI_IOCTL_DATA_IN 1
#define SCSI_IOCTL_DATA_UNSPECIFIED 2

typedef struct ScsiPassThrough
{
    unsigned short Length;           /* [x00] */
    unsigned char ScsiStatus;        /* [x01] */
    unsigned char PathId;            /* [x02] */
    unsigned char TargetId;          /* [x03] */
    unsigned char Lun;               /* [x04] */
    unsigned char CdbLength;         /* [x05] */
    unsigned char SenseInfoLength;   /* [x06] */
    unsigned char DataIn;            /* [x07] */
    unsigned int DataTransferLength; /* [x0B:0A:09:08] */
    unsigned int TimeOutValue;       /* [x0F:0E:0D:0C] */
    unsigned int DataBufferOffset;   /* [x13:12:11:10] */
    unsigned int SenseInfoOffset;    /* [x17:16:15:14] */
    unsigned char Cdb[16];           /* [x18...x27] */
} SCSI_PASS_THROUGH;

/* Define enough more to talk IOCTL_SCSI_PASS_THROUGH_DIRECT. */

#define IOCTL_SCSI_PASS_THROUGH_DIRECT 0x4D014

typedef struct ScsiPassThroughDirect
{
    unsigned short Length;
    unsigned char ScsiStatus;
    unsigned char PathId;
    unsigned char TargetId;
    unsigned char Lun;
    unsigned char CdbLength;
    unsigned char SenseInfoLength;
    unsigned char DataIn;
    unsigned int DataTransferLength;
    unsigned int TimeOutValue;
    void *DataBuffer; /* [x13:12:11:10] */
    unsigned int SenseInfoOffset;
    unsigned char Cdb[16];
} SCSI_PASS_THROUGH_DIRECT;

#ifdef __cplusplus
}
#endif

#endif /* NTDDSCSI_H defined? */
