#include "include/system.h"
#include "include/usbld.h"
#include "include/hddsupport.h"

#define COMPAT_MODE_1 		0x01
#define COMPAT_MODE_2 		0x02
#define COMPAT_MODE_3 		0x04
#define COMPAT_MODE_4 		0x08
#define COMPAT_MODE_5 		0x10
#define COMPAT_MODE_6 		0x20
#define COMPAT_MODE_7 		0x40
#define COMPAT_MODE_8 		0x80

extern unsigned char hdd_cdvdman_irx[];
extern unsigned int size_hdd_cdvdman_irx;

extern unsigned char hdd_pcmcia_cdvdman_irx[];
extern unsigned int size_hdd_pcmcia_cdvdman_irx;

extern unsigned char cdvdfsv_irx[];
extern unsigned int size_cdvdfsv_irx;

extern unsigned char poweroff_irx[];
extern unsigned int size_poweroff_irx;

extern unsigned char ps2dev9_irx[];
extern unsigned int size_ps2dev9_irx;

extern unsigned char ps2atad_irx[];
extern unsigned int size_ps2atad_irx;

extern unsigned char ps2hdd_irx[];
extern unsigned int size_ps2hdd_irx;

extern unsigned char iomanx_irx[];
extern unsigned int size_iomanx_irx;

extern unsigned char filexio_irx[];
extern unsigned int size_filexio_irx;

int timer = 43;

int hddGetHDLGameInfo(const char *Partition, hdl_game_info_t *ginfo);

static inline const char *GetMountParams(const char *command, char *BlockDevice){
	const char *MountPath;
	int BlockDeviceNameLen;

	if((MountPath=strchr(&command[5], ':'))!=NULL){
		BlockDeviceNameLen=(unsigned int)MountPath-(unsigned int)command;
		strncpy(BlockDevice, command, BlockDeviceNameLen);
		BlockDevice[BlockDeviceNameLen]='\0';

		MountPath++;	//This is the location of the mount path;
	}

	return MountPath;
}

int main(int argc, char *argv[]){
	char PartitionName[33], BlockDevice[38];
	unsigned char gid[5];
	gDisableDebug=1;
	gExitPath[0]='\0';
	gHDDSpindown=0;

	int i, size_irx = 0, result;
	unsigned char *irx = NULL;
	char filename[32];

	hdl_game_info_t GameInfo;

	SifInitRpc(0);

	/* Do as many things as possible while the IOP slowly resets itself. */
	if(argc==2){	/* Argument 1 will contain the name of the partition containing the game. */
		/*	Unfortunately, it'll mean that some homebrew loader was most likely used to launch this program... and it might already have IOMANX loaded. That thing can't register devices that are added via IOMAN after it gets loaded.
			Reset the IOP to clear out all these stupid homebrew modules... */
		while(!SifIopReset(NULL, 0)){};

		if(strlen(argv[1])<=32){
			strncpy(PartitionName, argv[1], sizeof(PartitionName)-1);
			PartitionName[sizeof(PartitionName)-1]='\0';
			result=0;
		}
		else result=-1;

		while(!SifIopSync()){};

		SifInitRpc(0);

		if(result<0) goto BootError;
	}
	else{
		if(GetMountParams(argv[0], BlockDevice)!=NULL){
			strncpy(PartitionName, &BlockDevice[5], sizeof(PartitionName)-1);
			PartitionName[sizeof(PartitionName)-1]='\0';
		}
		else goto BootError;
	}

	SifInitIopHeap();
	SifLoadFileInit();

	sbv_patch_enable_lmb(); 

	SifExecModuleBuffer(poweroff_irx, size_poweroff_irx, 0, NULL, NULL);
	SifExecModuleBuffer(ps2dev9_irx, size_ps2dev9_irx, 0, NULL, NULL);

	SifExecModuleBuffer(iomanx_irx, size_iomanx_irx, 0, NULL, NULL);
	SifExecModuleBuffer(filexio_irx, size_filexio_irx, 0, NULL, NULL);

	fileXioInit();

	SifExecModuleBuffer(ps2atad_irx, size_ps2atad_irx, 0, NULL, NULL);
	SifExecModuleBuffer(ps2hdd_irx, size_ps2hdd_irx, 0, NULL, NULL);
	hddSetIdleTimeout(gHDDSpindown * 12); // gHDDSpindown [0..20] -> spindown [0..240] -> seconds [0..1200]

	SifLoadFileExit();
	SifExitIopHeap();

	DPRINTF("Retrieving game information...\n");

	if((result=hddGetHDLGameInfo(PartitionName, &GameInfo))>=0){
		DPRINTF("Partition name: %s \nTitle: %s \nStartup: %s\n", PartitionName, GameInfo.name, GameInfo.startup);

		DPRINTF("Configuring core...\n");
		//configGetDiscIDBinary(GameInfo.startup, gid);
		memset(gid, 0, sizeof(gid));

		int dmaType = 0x40, dmaMode = 4, compatMode = 0;
		
		compatMode = GameInfo.ops2l_compat_flags;
		dmaType = GameInfo.dma_type;
		dmaMode = GameInfo.dma_mode;
		
		hddSetTransferMode(dmaType, dmaMode);
		hddSetIdleTimeout(gHDDSpindown * 12);

		if (sysPcmciaCheck()) {
			DPRINTF("CXD9566 detected.\n");
			size_irx = size_hdd_pcmcia_cdvdman_irx;
			irx = hdd_pcmcia_cdvdman_irx;
		}
		else {
			DPRINTF("CXD9611 detected.\n");
			size_irx = size_hdd_cdvdman_irx;
			irx = hdd_cdvdman_irx;
		}

		for (i=0;i<size_irx;i++){
			if(!strcmp((const char*)((u32)irx+i),"######    GAMESETTINGS    ######")){
				break;
			}
		}

		// patch 48bit flag
		u8 flag_48bit = hddIs48bit() & 0xff;
		memcpy((void*)((u32)irx + i + 34),&flag_48bit, 1);
		
		if (compatMode & COMPAT_MODE_2) {
			u8 alt_read_mode = 1;
			memcpy((void*)((u32)irx + i + 35),&alt_read_mode,1);
		}
		if (compatMode & COMPAT_MODE_7) {
			u8 use_threading_hack = 1;
			memcpy((void*)((u32)irx + i + 36),&use_threading_hack, 1);
		}
		if (compatMode & COMPAT_MODE_5) {
			u8 no_dvddl = 1;
			memcpy((void*)((u32)irx + i + 37),&no_dvddl,1);
		}
		if (compatMode & COMPAT_MODE_4) {
			u16 no_pss = 1;
			memcpy((void*)((u32)irx + i + 38),&no_pss,2);
		}
		
		// patch cdvdman timer
			u32 cdvdmanTimer = timer * 250;
			memcpy((void*)((u32)irx + i + 40), &cdvdmanTimer, 4);

		// patch start_sector
		memcpy((void*)((u32)irx + i + 44),&GameInfo.start_sector, 4);

		for (i=0;i<size_irx;i++){
			if(!strcmp((const char*)((u32)irx + i),"B00BS")){
				break;
			}
		}
		// game id
		memcpy((void*)((u32)irx + i), &gid, 5);

		DPRINTF("Launching game...\n");	

		sprintf(filename, "%s", GameInfo.startup);
		
		sysLaunchLoaderElf(GameInfo.start_sector, filename, "HDD_MODE", size_irx, irx, compatMode, compatMode & COMPAT_MODE_1);
		result=-1;
	}

	DPRINTF("Error loading game: %s, code: %d\n", PartitionName, result);

BootError:
	SifExitRpc();

	char *args[2];
	args[0]="BootError";
	args[1]=NULL;
	ExecOSD(1, args);

	return 0;
}

