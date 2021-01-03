#ifndef __USBLD_H
#define __USBLD_H

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <fileio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <string.h>
#include <loadfile.h>
#include <stdio.h>
#include <sbv_patches.h>
#include <malloc.h>
#include <fileXio_rpc.h>
#ifdef VMC
#include <sys/fcntl.h>
#endif

#ifdef __EESIO_DEBUG
#include <sio.h>
#define DPRINTF(args...) sio_printf(args)
#define DINIT() sio_init(38400, 0, 0, 0, 0)
#define LOG(args...) sio_printf(args)
#else
#define LOG(args...)
#define DPRINTF(args...)
#define DINIT()
#endif

//// Settings
int gHDDSpindown;

// Exit path
char gExitPath[32];
// Disable Debug Colors
int gDisableDebug;
#endif
