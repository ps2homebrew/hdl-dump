#ifndef __HDD_SUPPORT_H
#define __HDD_SUPPORT_H

#define PS2PART_IDMAX     32
#define HDL_GAME_NAME_MAX 64

// APA Partition
#define APA_MAGIC            0x00415041 // 'APA\0'
#define APA_IDMAX            PS2PART_IDMAX
#define APA_MAXSUB           64 // Maximum # of sub-partitions
#define APA_PASSMAX          8
#define APA_FLAG_SUB         0x0001
#define APA_MBR_VERSION      2
#define APA_IOCTL2_GETHEADER 0x6836
#define PFS_IOCTL2_GET_INODE 0x7007

typedef struct
{
    char partition_name[PS2PART_IDMAX + 1];
    char name[HDL_GAME_NAME_MAX + 1];
    char startup[8 + 1 + 3 + 1];
    u8 hdl_compat_flags;
    u8 ops2l_compat_flags;
    u8 dma_type;
    u8 dma_mode;
    u32 layer_break;
    int disctype;
    u32 start_sector;
    u32 total_size_in_kb;
} hdl_game_info_t;

typedef struct
{
    u32 count;
    hdl_game_info_t *games;
    u32 total_chunks;
    u32 free_chunks;
} hdl_games_list_t;

typedef struct
{
    u8 unused;
    u8 sec;
    u8 min;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
} ps2time_t;

typedef struct
{
    u32 checksum;
    u32 magic; // APA_MAGIC
    u32 next;
    u32 prev;
    char id[APA_IDMAX];     // 16
    char rpwd[APA_PASSMAX]; // 48
    char fpwd[APA_PASSMAX]; // 56
    u32 start;              // 64
    u32 length;             // 68
    u16 type;               // 72
    u16 flags;              // 74
    u32 nsub;               // 76
    ps2time_t created;      // 80
    u32 main;               // 88
    u32 number;             // 92
    u32 modver;             // 96
    u32 pading1[7];         // 100
    char pading2[128];      // 128
    struct
    { // 256
        char magic[32];
        u32 version;
        u32 nsector;
        ps2time_t created;
        u32 osdStart;
        u32 osdSize;
        char pading3[200];
    } mbr;
    struct
    {
        u32 start;
        u32 length;
    } subs[APA_MAXSUB];
} apa_header;

int hddIs48bit(void);
int hddSetTransferMode(int type, int mode);
int hddSetIdleTimeout(int timeout);

#endif
