#include "include/usbld.h"
#include "include/hddsupport.h"

//#define TEST_WRITES

typedef struct				// size = 1024
{
	u32	checksum;		// HDL uses 0xdeadfeed magic here
	u32	magic;
	char	gamename[160];
	u8  	hdl_compat_flags;
	u8  	ops2l_compat_flags;
	u8	dma_type;
	u8	dma_mode;
	char	startup[60];
	u32 	layer1_start;
	u32 	discType;
	int 	num_partitions;
	struct {
		u32 	part_offset;	// in MB
		u32 	data_start;	// in sectors
		u32 	part_size;	// in KB
	} part_specs[65];
} hdl_apa_header;

//
// DEVCTL commands
//
#define APA_DEVCTL_MAX_SECTORS		0x00004801	// max partition size(in sectors)
#define APA_DEVCTL_TOTAL_SECTORS	0x00004802
#define APA_DEVCTL_IDLE			0x00004803
#define APA_DEVCTL_FLUSH_CACHE		0x00004804
#define APA_DEVCTL_SWAP_TMP		0x00004805
#define APA_DEVCTL_DEV9_SHUTDOWN	0x00004806
#define APA_DEVCTL_STATUS		0x00004807
#define APA_DEVCTL_FORMAT		0x00004808
#define APA_DEVCTL_SMART_STAT		0x00004809
#define APA_DEVCTL_GETTIME		0x00006832
#define APA_DEVCTL_SET_OSDMBR		0x00006833// arg = hddSetOsdMBR_t
#define APA_DEVCTL_GET_SECTOR_ERROR	0x00006834
#define APA_DEVCTL_GET_ERROR_PART_NAME	0x00006835// bufp = namebuffer[0x20]
#define APA_DEVCTL_ATA_READ		0x00006836// arg  = hddAtaTransfer_t
#define APA_DEVCTL_ATA_WRITE		0x00006837// arg  = hddAtaTransfer_t
#define APA_DEVCTL_SCE_IDENTIFY_DRIVE	0x00006838// bufp = buffer for atadSceIdentifyDrive 
#define APA_DEVCTL_IS_48BIT		0x00006840
#define APA_DEVCTL_SET_TRANSFER_MODE	0x00006841

//-------------------------------------------------------------------------
u32 hddGetTotalSectors(void)
{
	return fileXioDevctl("hdd0:", APA_DEVCTL_TOTAL_SECTORS, NULL, 0, NULL, 0);
}

//-------------------------------------------------------------------------
int hddIs48bit(void)
{
	return fileXioDevctl("hdd0:", APA_DEVCTL_IS_48BIT, NULL, 0, NULL, 0);
}

//-------------------------------------------------------------------------
int hddSetTransferMode(int type, int mode)
{
	u8 args[16];

	*(u32 *)&args[0] = type;
	*(u32 *)&args[4] = mode;

	return fileXioDevctl("hdd0:", APA_DEVCTL_SET_TRANSFER_MODE, args, 8, NULL, 0);
}

//-------------------------------------------------------------------------
int hddSetIdleTimeout(int timeout)
{
	// From hdparm man:
	// A value of zero means "timeouts  are  disabled":  the
	// device will not automatically enter standby mode.  Values from 1
	// to 240 specify multiples of 5 seconds, yielding timeouts from  5
	// seconds to 20 minutes.  Values from 241 to 251 specify from 1 to
	// 11 units of 30 minutes, yielding timeouts from 30 minutes to 5.5
	// hours.   A  value  of  252  signifies a timeout of 21 minutes. A
	// value of 253 sets a vendor-defined timeout period between 8  and
	// 12  hours, and the value 254 is reserved.  255 is interpreted as
	// 21 minutes plus 15 seconds.  Note that  some  older  drives  may
	// have very different interpretations of these values.

	u8 args[16];

	*(u32 *)&args[0] = timeout & 0xff;

	return fileXioDevctl("hdd0:", APA_DEVCTL_IDLE, args, 4, NULL, 0);
}

#define HDL_GAME_DATA_OFFSET 0x100000	/* Sector 0x800 in the user data area. */

//-------------------------------------------------------------------------
int hddGetHDLGameInfo(const char *Partition, hdl_game_info_t *ginfo)
{
	u32 size;
	static char buf[1024] ALIGNED(64);
	int fd, ret;
	iox_stat_t PartStat;
	char *PathToPart;

	DPRINTF("Partition: %s\n", Partition);

	PathToPart=malloc(strlen(Partition)+6);
	sprintf(PathToPart, "hdd0:%s", Partition);

	DPRINTF("Path: %s\n", PathToPart);

	if((fd=fileXioOpen(PathToPart, O_RDONLY, 666))>=0){
		fileXioLseek(fd, HDL_GAME_DATA_OFFSET, SEEK_SET);
		ret=fileXioRead(fd, buf, 1024);
		fileXioClose(fd);

		if(ret == 1024) {
			fileXioGetStat(PathToPart, &PartStat);

			hdl_apa_header *hdl_header = (hdl_apa_header *)buf;

			// calculate total size
			size = PartStat.size;
			if (PartStat.mode != 0x1337) {// Check if partition type is HD Loader (0x1337)
				free(PathToPart);
				return -1;
			}

			strncpy(ginfo->partition_name, Partition, sizeof(ginfo->partition_name)-1);
			ginfo->partition_name[sizeof(ginfo->partition_name)-1]='\0';
			strncpy(ginfo->name, hdl_header->gamename, sizeof(ginfo->name)-1);
			ginfo->name[sizeof(ginfo->name)-1]='\0';
			strncpy(ginfo->startup, hdl_header->startup, sizeof(ginfo->startup)-1);
			ginfo->startup[sizeof(ginfo->startup)-1]='\0';
			ginfo->hdl_compat_flags = hdl_header->hdl_compat_flags;
			ginfo->ops2l_compat_flags = hdl_header->ops2l_compat_flags;
			ginfo->dma_type = hdl_header->dma_type;
			ginfo->dma_mode = hdl_header->dma_mode;
			ginfo->layer_break = hdl_header->layer1_start;
			ginfo->disctype = hdl_header->discType;
			ginfo->start_sector = PartStat.private_5 + (HDL_GAME_DATA_OFFSET+4096)/512;	/* Note: The APA specification states that there is a 4KB area used for storing the partition's information, before the extended attribute area. */
			ginfo->total_size_in_kb = size / 2;
		}
		else ret=-EIO;
	}
	else ret = fd;

	free(PathToPart);

 	return ret;
}
