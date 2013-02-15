/*
 * ps2_hdd.h
 * $Id: ps2_hdd.h,v 1.2 2004/08/15 16:44:19 b081 Exp $
 *
 * borrowed from ps2fdisk
 */

#if !defined (_PS2_HDD_H)
#define _PS2_HDD_H

typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned long u_int32_t;

/* Various PS2 partition constants */
#define PS2_PARTITION_MAGIC	0x00415041	/* "APA\0" */
#define PS2_PART_IDMAX		32
#define PS2_PART_NAMEMAX	128
#define PS2_PART_MAXSUB		64	/* Maximum # of sub-partitions */
#define PS2_PART_FLAG_SUB	0x0001	/* Is partition a sub-partition? */
#define PS2_MBR_VERSION		2	/* Current MBR version */

/* Partition types */
#define PS2_MBR_PARTITION	0x0001
#define PS2_SWAP_PARTITION	0x0082
#define PS2_LINUX_PARTITION	0x0083
#define PS2_GAME_PARTITION	0x0100

/* Date/time descriptor used in on-disk partition header */
typedef struct ps2fs_datetime_type
{
  u_int8_t unused;
  u_int8_t sec;
  u_int8_t min;
  u_int8_t hour;
  u_int8_t day;
  u_int8_t month;
  u_int16_t year;
} ps2fs_datetime_t;

/* On-disk partition header for a partition */
typedef struct ps2_partition_header_type
{
  u_int32_t checksum;	/* Sum of all 256 words, assuming checksum==0 */
  u_int32_t magic;	/* PS2_PARTITION_MAGIC */
  u_int32_t next;	/* Sector address of next partition */
  u_int32_t prev;	/* Sector address of previous partition */
  char id [PS2_PART_IDMAX];
  char unknown1 [16];
  u_int32_t start;	/* Sector address of this partition */
  u_int32_t length;	/* Sector count */
  u_int16_t type;
  u_int16_t flags;	/* PS2_PART_FLAG_* */
  u_int32_t nsub;	/* No. of sub-partitions (stored in main partition) */
  ps2fs_datetime_t created;
  u_int32_t main;	/* For sub-partitions, main partition sector address */
  u_int32_t number;	/* For sub-partitions, sub-partition number */
  u_int16_t unknown2;
  char unknown3 [30];
  char name [PS2_PART_NAMEMAX];
  struct
  {
    char magic [32];	/* Copyright message in MBR */
    char unknown_0x02;
    char unknown1 [7];
    ps2fs_datetime_t created; /* Same as for the partition, it seems*/
    u_int32_t data_start;	/* Some sort of MBR data; position in sectors*/
    u_int32_t data_len;	/* Length also in sectors */
    char unknown2 [200];
  } mbr;
  struct
  {		/* Sub-partition data */
    u_int32_t start;/* Sector address */
    u_int32_t length;/* Sector count */
  } subs [PS2_PART_MAXSUB];
} ps2_partition_header_t;

#endif /* _PS2_HDD_H defined? */
