/****************************************************************************
 *
 * Name:            SCSIDEFS.H
 *
 * Description: SCSI definitions ('C' Language)
 *
 * borrowed from PEOp.S. CDVD plug-in source code
 * http://sourceforge.net/projects/peops/
 *
 ***************************************************************************/

#if !defined (_SCSIDEFS_H)
#  define _SCSIDEFS_H

#include <windows.h>

typedef struct
{
  USHORT Length;
  UCHAR  ScsiStatus;
  UCHAR  PathId;
  UCHAR  TargetId;
  UCHAR  Lun;
  UCHAR  CdbLength;
  UCHAR  SenseInfoLength;
  UCHAR  DataIn;
  ULONG  DataTransferLength;
  ULONG  TimeOutValue;
  ULONG  DataBufferOffset;
  ULONG  SenseInfoOffset;
  UCHAR  Cdb[16];
} SCSI_PASS_THROUGH;

typedef struct
{
  USHORT Length;
  UCHAR  ScsiStatus;
  UCHAR  PathId;
  UCHAR  TargetId;
  UCHAR  Lun;
  UCHAR  CdbLength;
  UCHAR  SenseInfoLength;
  UCHAR  DataIn;
  ULONG  DataTransferLength;
  ULONG  TimeOutValue;
  PVOID  DataBuffer;
  ULONG  SenseInfoOffset;
  UCHAR  Cdb[16];
} SCSI_PASS_THROUGH_DIRECT;

typedef struct
{
  SCSI_PASS_THROUGH_DIRECT spt;
  ULONG Filler;
  UCHAR ucSenseBuf[32];
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

typedef struct {
  ULONG Length;
  UCHAR PortNumber;
  UCHAR PathId;
  UCHAR TargetId;
  UCHAR Lun;
} SCSI_ADDRESS;

/*
 * constants for DataIn member of SCSI_PASS_THROUGH* structures
 */
#define  SCSI_IOCTL_DATA_OUT          0
#define  SCSI_IOCTL_DATA_IN           1
#define  SCSI_IOCTL_DATA_UNSPECIFIED  2

/*
 * Standard IOCTL define
 */
#define CTL_CODE( DevType, Function, Method, Access ) (                 \
    ((DevType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)


/*
 * method codes
 */
#define  METHOD_BUFFERED     0
#define  METHOD_IN_DIRECT    1
#define  METHOD_OUT_DIRECT   2
#define  METHOD_NEITHER      3

/*
 * file access values
 */
#define  FILE_ANY_ACCESS      0
#define  FILE_READ_ACCESS     (0x0001)
#define  FILE_WRITE_ACCESS    (0x0002)

#define IOCTL_SCSI_BASE    0x00000004

#define IOCTL_SCSI_PASS_THROUGH_DIRECT  \
  CTL_CODE (IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED, \
            FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_SCSI_GET_ADDRESS  \
  CTL_CODE (IOCTL_SCSI_BASE, 0x0406, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif /* _SCSIDEFS_H defined? */
