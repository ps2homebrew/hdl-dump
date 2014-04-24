/*
  Copyright 2009, Ifcaro
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/usbld.h"
#include "include/system.h"
#include "include/ioprp.h"

extern unsigned char udnl_irx[];
extern unsigned int size_udnl_irx;

extern unsigned char imgdrv_irx[];
extern unsigned int size_imgdrv_irx;

extern unsigned char cdvdfsv_irx[];
extern unsigned int size_cdvdfsv_irx;

extern unsigned char eecore_elf[];
extern unsigned int size_eecore_elf;

extern unsigned char IOPRP_img[];
extern unsigned int size_IOPRP_img;

#ifdef __DECI2_DEBUG
extern unsigned char drvtif_irx[];
extern unsigned int size_drvtif_irx;

extern unsigned char *tifinet_irx;
extern unsigned int size_tifinet_irx;
#endif

#define ELF_MAGIC		0x464c457f
#define ELF_PT_LOAD		1

typedef struct {
	u8	ident[16];	// struct definition for ELF object header
	u16	type;
	u16	machine;
	u32	version;
	u32	entry;
	u32	phoff;
	u32	shoff;
	u32	flags;
	u16	ehsize;
	u16	phentsize;
	u16	phnum;
	u16	shentsize;
	u16	shnum;
	u16	shstrndx;
} elf_header_t;

typedef struct {
	u32	type;		// struct definition for ELF program section header
	u32	offset;
	void	*vaddr;
	u32	paddr;
	u32	filesz;
	u32	memsz;
	u32	flags;
	u32	align;
} elf_pheader_t;

typedef struct {
	void *irxaddr;
	unsigned int irxsize;
} irxptr_t;

// structs for DEVCTL commands
typedef struct
{
	u32 lba;
	u32 size;
	u8 data[0];
} hddAtaTransfer_t; 

//
// DEVCTL commands
//
#define APA_DEVCTL_ATA_READ		0x00006836// arg  = hddAtaTransfer_t

int sysPcmciaCheck(void) {
	/* fileXioDevctl returns 0 if the DEV9 device is the CXD9566 (PCMCIA), and 1 if it's the CXD9611 (Expansion bay). */
	return(fileXioDevctl("dev9x0:", 0x4401, NULL, 0, NULL, 0)==0?1:0);
}


#define IRX_NUM 8

static inline void sendIrxKernelRAM(int size_cdvdman_irx, void *cdvdman_irx) { // Send IOP modules that core must use to Kernel RAM
	irxptr_t *irxptr_tab;
	void *irxsrc[IRX_NUM];
	void *irxptr;
	int i, n;
	unsigned int irxsize, curIrxSize;
	void *ioprp_image;
	unsigned int size_ioprp_image;

	irxptr_tab=(irxptr_t*)0x00088004;
	ioprp_image=malloc(size_IOPRP_img+size_cdvdman_irx+size_cdvdfsv_irx+256);
	size_ioprp_image=patch_IOPRP_image(ioprp_image, cdvdman_irx, size_cdvdman_irx);

	n = 0;
	irxptr_tab[n++].irxsize = size_udnl_irx;
	irxptr_tab[n++].irxsize = size_ioprp_image;
	irxptr_tab[n++].irxsize = size_imgdrv_irx;
	irxptr_tab[n++].irxsize = 0;	//usbd
	irxptr_tab[n++].irxsize = 0;	//smsmap
#ifdef __DECI2_DEBUG
	irxptr_tab[n++].irxsize = size_drvtif_irx;
	irxptr_tab[n++].irxsize = size_tifinet_irx;
#else
	irxptr_tab[n++].irxsize = 0;	//udptty
	irxptr_tab[n++].irxsize = 0;	//ioptrap
#endif
	irxptr_tab[n++].irxsize = 0;	//smstcpip

	n = 0;
	irxsrc[n++] = (void *)&udnl_irx;
	irxsrc[n++] = ioprp_image;
	irxsrc[n++] = (void *)&imgdrv_irx;
	irxsrc[n++] = NULL;
	irxsrc[n++] = NULL;
#ifdef __DECI2_DEBUG
	irxsrc[n++] = (void *)&drvtif_irx;
	irxsrc[n++] = (void *)&tifinet_irx;
#else
	irxsrc[n++] = NULL;
	irxsrc[n++] = NULL;
#endif
	irxsrc[n++] = NULL;

	irxsize = 0;

	*(irxptr_t**)0x00088000 = irxptr_tab;
	irxptr = (void *)((((unsigned int)irxptr_tab+sizeof(irxptr_t)*IRX_NUM)+0xF)&~0xF);

	#ifdef __DECI2_DEBUG
	//For DECI2 debugging mode, the UDNL module will have to be stored within kernel RAM because there isn't enough space below user RAM.
	irxptr_tab[0].irxaddr=(void*)0x00033000;
	/*	LOG("SYSTEM DECI2 UDNL address start: %p end: %p\n", irxptr_tab[0].irxaddr, irxptr_tab[0].irxaddr+irxptr_tab[0].irxsize);
	*/
	DI();
	ee_kmode_enter();
	memcpy((void*)(0x80000000|(unsigned int)irxptr_tab[0].irxaddr), irxsrc[0], irxptr_tab[0].irxsize);
	ee_kmode_exit();
	EI();

	for (i = 1; i< IRX_NUM; i++) {
#else
	for (i = 0; i < IRX_NUM; i++) {
#endif
		curIrxSize = irxptr_tab[i].irxsize;
		irxptr_tab[i].irxaddr = irxptr;

		if (curIrxSize > 0) {
	/*	LOG("SYSTEM IRX address start: %p end: %p\n", irxptr_tab[i].irxaddr, irxptr_tab[i].irxaddr+curIrxSize);
			*/
			if(irxptr+curIrxSize>=(void*)0x000B3F00){	//Sanity check.
			/*	LOG("*** OVERFLOW DETECTED. HALTED.\n");*/
				asm volatile("break\n");
			}

			memcpy(irxptr_tab[i].irxaddr, irxsrc[i], curIrxSize);

			irxptr += ((curIrxSize+0xF)&~0xF);
			irxsize += curIrxSize;
		}
	}

	free(ioprp_image);
}

#ifdef __DECI2_DEBUG
/*
	Look for the start of the EE DECI2 manager initialization function.

	The stock EE kernel has no reset function, but the EE kernel is most likely already primed to self-destruct and in need of a good reset.
	What happens is that the OSD initializes the EE DECI2 TTY protocol at startup, but the EE DECI2 manager is never aware that the OSDSYS ever loads other programs.

	As a result, the EE kernel crashes immediately when the EE TTY gets used (when the IOP side of DECI2 comes up), when it invokes whatever that exists at the OSD's old ETTY handler's location. :(
*/
static int ResetDECI2(void){
	int result;
	unsigned int i, *ptr;
	void (*pDeci2ManagerInit)(void);
	static const unsigned int Deci2ManagerInitPattern[]={
		0x3c02bf80,	//lui v0, $bf80
		0x3c04bfc0,	//lui a0, $bfc0
		0x34423800,	//ori v0, v0, $3800
		0x34840102	//ori a0, a0, $0102
	};

	DI();
	ee_kmode_enter();

	result=-1;
	ptr=(void*)0x80000000;
	for(i=0; i<0x20000/4; i++){
		if(	ptr[i+0]==Deci2ManagerInitPattern[0] &&
			ptr[i+1]==Deci2ManagerInitPattern[1] &&
			ptr[i+2]==Deci2ManagerInitPattern[2] &&
			ptr[i+3]==Deci2ManagerInitPattern[3]
			){
			pDeci2ManagerInit=(void*)&ptr[i-14];
			pDeci2ManagerInit();
			result=0;
			break;
		}
	}

	ee_kmode_exit();
	EI();

	return result;
}
#endif

void sysLaunchLoaderElf(unsigned long int StartLBA, char *filename, char *mode_str, int size_cdvdman_irx, void *cdvdman_irx, unsigned int compatflags) {
	u8 *boot_elf = NULL;
	elf_header_t *eh;
	elf_pheader_t *eph;
	void *pdata;
	int i, NumArgs;
	char *argv[6];
	char config_str[128];
	char FullELFPath[22];	/* cdrom0:\\SXXX_YYY.ZZ;1" */

	if (gExitPath[0] == '\0')
		strncpy(gExitPath, "Browser", 32);

	memset((void*)0x00082000, 0, 0x00100000-0x00082000);

	sendIrxKernelRAM(size_cdvdman_irx, cdvdman_irx);

#ifdef __DECI2_DEBUG
	ResetDECI2();
#endif

	// NB: LOADER.ELF is embedded

	boot_elf = (u8 *)&eecore_elf;
	eh = (elf_header_t *)boot_elf;
	if (_lw((u32)&eh->ident) != ELF_MAGIC)
		while (1);

	eph = (elf_pheader_t *)(boot_elf + eh->phoff);

	// Scan through the ELF's program headers and copy them into RAM, then
	// zero out any non-loaded regions.
	for (i = 0; i < eh->phnum; i++) {
		if (eph[i].type != ELF_PT_LOAD)
		continue;

		pdata = (void *)(boot_elf + eph[i].offset);
		memcpy(eph[i].vaddr, pdata, eph[i].filesz);

		if (eph[i].memsz > eph[i].filesz)
			memset(eph[i].vaddr + eph[i].filesz, 0, eph[i].memsz - eph[i].filesz);
	}

	// Let's go.
	sprintf(config_str, "%s %d %s %d %d %d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d", mode_str, gDisableDebug, gExitPath, 0, gHDDSpindown, \
		0, 0, 0, 0, \
		0, 0, 0, 0, \
		0, 0, 0, 0);

	char cmask[10];
	snprintf(cmask, 10, "%d", compatflags);
	argv[0] = config_str;
	argv[1] = filename;
	argv[2] = cmask;

	filename[11]=0x00; // fix for 8+3 filename.
	sprintf(FullELFPath, "cdrom0:\\%s;1", filename);

	printf("Starting EE core.\n");

#if 0
	argv[3] = "rom0:PS2LOGO";
	argv[4] = FullELFPath;
	argv[5] = NULL;
	NumArgs=5;
#endif
	argv[3] = FullELFPath;
	argv[4] = NULL;
	NumArgs=4;

	fileXioStop();
	SifExitRpc();

	ExecPS2((void *)eh->entry, 0, NumArgs, argv);
}


