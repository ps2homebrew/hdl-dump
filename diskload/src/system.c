/*
  Copyright 2009, Ifcaro
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/system.h"
#include "include/ioprp.h"
#include "../ee_core/include/modules.h"

extern unsigned char udnl_irx[];
extern unsigned int size_udnl_irx;

extern unsigned char IOPRP_img[];
extern unsigned int size_IOPRP_img;

extern unsigned char imgdrv_irx[];
extern unsigned int size_imgdrv_irx;

extern unsigned char eesync_irx[];
extern unsigned int size_eesync_irx;

extern unsigned char cdvdfsv_irx[];
extern unsigned int size_cdvdfsv_irx;

extern unsigned char eecore_elf[];
extern unsigned int size_eecore_elf;

#define ELF_MAGIC 0x464c457f
#define ELF_PT_LOAD 1

typedef struct
{
    u8 ident[16]; // struct definition for ELF object header
    u16 type;
    u16 machine;
    u32 version;
    u32 entry;
    u32 phoff;
    u32 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} elf_header_t;

typedef struct
{
    u32 type; // struct definition for ELF program section header
    u32 offset;
    void *vaddr;
    u32 paddr;
    u32 filesz;
    u32 memsz;
    u32 flags;
    u32 align;
} elf_pheader_t;

//Module bits
#define CORE_IRX_USB 0x01
#define CORE_IRX_ETH 0x02
#define CORE_IRX_SMB 0x04
#define CORE_IRX_HDD 0x08
#define CORE_IRX_VMC 0x10
#define CORE_IRX_DEBUG 0x20
#define CORE_IRX_DECI2 0x40

static unsigned int sendIrxKernelRAM(unsigned int modules, void *ModuleStorage, int size_cdvdman_irx, void **cdvdman_irx)
{ // Send IOP modules that core must use to Kernel RAM
    irxtab_t *irxtable;
    irxptr_t *irxptr_tab;
    void *irxptr, *ioprp_image;
    int i, modcount;
    unsigned int curIrxSize, size_ioprp_image, total_size;

    irxtable = (irxtab_t *)ModuleStorage;
    irxptr_tab = (irxptr_t *)((unsigned char *)irxtable + sizeof(irxtab_t));
    ioprp_image = malloc(size_IOPRP_img + size_cdvdman_irx + size_cdvdfsv_irx + 256);
    size_ioprp_image = patch_IOPRP_image(ioprp_image, cdvdman_irx, size_cdvdman_irx);

    modcount = 0;
    //Basic modules
    irxptr_tab[modcount].info = size_udnl_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_UDNL);
    irxptr_tab[modcount++].ptr = (void *)&udnl_irx;
    irxptr_tab[modcount].info = size_ioprp_image | SET_OPL_MOD_ID(OPL_MODULE_ID_IOPRP);
    irxptr_tab[modcount++].ptr = ioprp_image;
    irxptr_tab[modcount].info = size_imgdrv_irx | SET_OPL_MOD_ID(OPL_MODULE_ID_IMGDRV);
    irxptr_tab[modcount++].ptr = (void *)&imgdrv_irx;

    irxtable->modules = irxptr_tab;
    irxtable->count = modcount;
    total_size = (sizeof(irxtab_t) + sizeof(irxptr_t) * modcount + 0xF) & ~0xF;
    irxptr = (void *)((((unsigned int)irxptr_tab + sizeof(irxptr_t) * modcount) + 0xF) & ~0xF);

    for (i = 0; i < modcount; i++) {
        curIrxSize = GET_OPL_MOD_SIZE(irxptr_tab[i].info);

        if (curIrxSize > 0) {
            LOG("SYSTEM IRX %u address start: %p end: %p\n", GET_OPL_MOD_ID(irxptr_tab[i].info), irxptr, irxptr + curIrxSize);
            memcpy(irxptr, irxptr_tab[i].ptr, curIrxSize);

            irxptr_tab[i].ptr = irxptr;
            irxptr += ((curIrxSize + 0xF) & ~0xF);
            total_size += ((curIrxSize + 0xF) & ~0xF);
        } else {
            irxptr_tab[i].ptr = NULL;
        }
    }

    free(ioprp_image);

    LOG("SYSTEM IRX STORAGE %p - %p\n", ModuleStorage, (u8 *)ModuleStorage + total_size);

    return total_size;
}

void sysLaunchLoaderElf(u32 StartLBA, char *filename, char *mode_str, int size_cdvdman_irx, void *cdvdman_irx, int compatflags)
{
    unsigned int modules, ModuleStorageSize;
    void *ModuleStorage;
    u8 *boot_elf = NULL;
    elf_header_t *eh;
    elf_pheader_t *eph;
    void *pdata;
    int i;
    char *argv[6];
    char config_str[256], ModStorageConfig[32];

    if (gExitPath[0] == '\0')
        strncpy(gExitPath, "Browser", 32);

    memset((void *)0x00082000, 0, 0x00100000 - 0x00082000);
    modules = CORE_IRX_HDD;
    ModuleStorage = (void *)((compatflags & COMPAT_MODE_7) ? OPL_MOD_STORAGE_HI : OPL_MOD_STORAGE);
    ModuleStorageSize = (sendIrxKernelRAM(modules, ModuleStorage, size_cdvdman_irx, cdvdman_irx) + 0x3F) & ~0x3F;
    sprintf(ModStorageConfig, "%u %u", (unsigned int)ModuleStorage, (unsigned int)ModuleStorage + ModuleStorageSize);

    // NB: LOADER.ELF is embedded
    boot_elf = (u8 *)&eecore_elf;
    eh = (elf_header_t *)boot_elf;
    if (_lw((u32)&eh->ident) != ELF_MAGIC)
        while (1)
            ;

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
    sprintf(config_str, "%s %d %s %d %u.%u.%u.%u %u.%u.%u.%u %u.%u.%u.%u %d",
            mode_str, gDisableDebug, gExitPath, gHDDSpindown,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0);

    char cmask[10];
    snprintf(cmask, 10, "%d", compatflags);
    argv[0] = config_str;
    argv[1] = ModStorageConfig;
    argv[2] = filename;
    argv[3] = cmask;

    fileXioExit();
    SifExitRpc();

    ExecPS2((void *)eh->entry, NULL, 4, argv);
}
