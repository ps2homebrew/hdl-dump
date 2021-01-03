#ifndef __SYSTEM_H
#define __SYSTEM_H

//From iosupport.h of OPL
#define COMPAT_MODE_1 0x01 // Accurate Reads
#define COMPAT_MODE_2 0x02 // Alternative data read method (Synchronous)
#define COMPAT_MODE_3 0x04 // Unhook Syscalls
#define COMPAT_MODE_4 0x08 // 0 PSS mode
#define COMPAT_MODE_5 0x10 // Emulate DVD-DL
#define COMPAT_MODE_6 0x20 // Disable IGR
#define COMPAT_MODE_7 0x40 // High Module Storage
#define COMPAT_MODE_8 0x80 // Hide DEV9 module

//From iosupport.h of OPL
#define OPL_MOD_STORAGE 0x00097000    //(default) Address of the module storage region
#define OPL_MOD_STORAGE_HI 0x01C00000 //Alternate address of the module storage region

void sysLaunchLoaderElf(u32 StartLBA, char *filename, char *mode_str, int size_cdvdman_irx, void *cdvdman_irx, int compatflags);

#endif
