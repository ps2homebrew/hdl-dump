#ifndef __SYSTEM_H
#define __SYSTEM_H

#define SYS_LOAD_MC_MODULES	0x01
#define SYS_LOAD_PAD_MODULES	0x02

int sysGetDiscID(char *discID);
int sysPcmciaCheck(void);
void sysGetCDVDFSV(void **data_irx, int *size_irx);
#ifdef VMC
void sysLaunchLoaderElf(unsigned long int StartLBA, char *filename, char *mode_str, int size_cdvdman_irx, void *cdvdman_irx, int size_mcemu_irx, void *mcemu_irx, int compatflags, int alt_ee_core);
#else
void sysLaunchLoaderElf(unsigned long int StartLBA, char *filename, char *mode_str, int size_cdvdman_irx, void *cdvdman_irx, int compatflags, int alt_ee_core);
#endif
#ifdef VMC
int sysCheckVMC(const char* prefix, const char* sep, char* name, int createSize);
#endif
#endif

