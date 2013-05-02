#include <fileio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <loadfile.h>
#include <malloc.h>
#include <sbv_patches.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <stdio.h>
#include <string.h>
#include <fileXio_rpc.h>
#include <debug.h>

#ifdef USING_NETIF_RPC
#include <netman.h>

#ifdef USING_LWIP_STACK
#include <ps2ip141.h>
#endif

#ifdef USING_LWIP_STACK
#include "hdldsvr.h"
#endif
#endif

#include "main.h"
#include "ipconfig.h"

extern unsigned char IOMANX_irx[];
extern unsigned int size_IOMANX_irx;

extern unsigned char FILEXIO_irx[];
extern unsigned int size_FILEXIO_irx;

extern unsigned char POWEROFF_irx[];
extern unsigned int size_POWEROFF_irx;

extern unsigned char DEV9_irx[];
extern unsigned int size_DEV9_irx;

extern unsigned char ATAD_irx[];
extern unsigned int size_ATAD_irx;

extern unsigned char FILEXIO_irx[];
extern unsigned int size_FILEXIO_irx;

extern unsigned char HDD_irx[];
extern unsigned int size_HDD_irx;

extern unsigned char SMAP_irx[];
extern unsigned int size_SMAP_irx;

extern unsigned char NETMAN_irx[];
extern unsigned int size_NETMAN_irx;

#ifndef USING_NETIF_RPC
#ifndef USING_LWIP_STACK
#ifndef USING_SMSTCPIP_STACK
extern unsigned char PKTDRV_irx[];
extern unsigned int size_PKTDRV_irx;
#else
extern unsigned char SMSTCPIP_irx[];
extern unsigned int size_SMSTCPIP_irx;

extern unsigned char HDLDSVR_irx[];
extern unsigned int size_HDLDSVR_irx;
#endif
#else
extern unsigned char PS2IP141_irx[];
extern unsigned int size_PS2IP141_irx;

extern unsigned char HDLDSVR_irx[];
extern unsigned int size_HDLDSVR_irx;
#endif
#endif

int main(int argc, char* argv[]){
#ifdef USING_LWIP_STACK
#ifdef USING_NETIF_RPC
	struct ip_addr IP, NM, GW;
#endif
#endif
	char ip_address_str[16], subnet_mask_str[16], gateway_str[16];
	unsigned char ip_address[4], subnet_mask[4], gateway[4];

	SifInitRpc(0);

	while(!SifIopReset(NULL, 0)){};
	while(!SifIopSync()){};

	SifInitRpc(0);
	SifInitIopHeap();
	SifLoadFileInit();
	fioInit();
	sbv_patch_enable_lmb();

	init_scr();

	scr_printf("\n\tSMAP TEST platform built on "__DATE__" "__TIME__"\n");

	SifLoadStartModule("rom0:SIO2MAN", 0, NULL, NULL);
	SifLoadStartModule("rom0:MCMAN", 0, NULL, NULL);

	scr_printf("# Parsing IP configuration...");

	if(ParseConfig("mc0:/SYS-CONF/IPCONFIG.DAT", ip_address_str, subnet_mask_str, gateway_str)!=0){
		if(ParseConfig("mc1:/SYS-CONF/IPCONFIG.DAT", ip_address_str, subnet_mask_str, gateway_str)!=0){
			strcpy(ip_address_str, "192.168.0.10");
			strcpy(subnet_mask_str, "255.255.255.0");
			strcpy(gateway_str, "192.168.0.1");
		}
	}

	ParseNetAddr(ip_address_str, ip_address);
	ParseNetAddr(subnet_mask_str, subnet_mask);
	ParseNetAddr(gateway_str, gateway);

	scr_printf(	"\tdone!\n"	\
			"\t\tIP address:\t%u.%u.%u.%u\n"	\
			"\t\tSubnet mask:\t%u.%u.%u.%u\n"	\
			"\t\tGateway:\t\t%u.%u.%u.%u\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3], subnet_mask[0], subnet_mask[1], subnet_mask[2], subnet_mask[3], gateway[0], gateway[1], gateway[2], gateway[3]);

	scr_printf("\t# Loading modules...");

	SifExecModuleBuffer(IOMANX_irx, size_IOMANX_irx, 0, NULL, NULL);
	SifExecModuleBuffer(FILEXIO_irx, size_FILEXIO_irx, 0, NULL, NULL);
	SifExecModuleBuffer(POWEROFF_irx, size_POWEROFF_irx, 0, NULL, NULL);
	SifExecModuleBuffer(DEV9_irx, size_DEV9_irx, 0, NULL, NULL);
	SifExecModuleBuffer(NETMAN_irx, size_NETMAN_irx, 0, NULL, NULL);
	SifExecModuleBuffer(SMAP_irx, size_SMAP_irx, 0, NULL, NULL);
	SifExecModuleBuffer(ATAD_irx, size_ATAD_irx, 0, NULL, NULL);
	SifExecModuleBuffer(HDD_irx, size_HDD_irx, 0, NULL, NULL);
#ifndef USING_NETIF_RPC
	char module_args[128];
	unsigned int args_offset=0;
	sprintf(module_args, "-ip=%s", ip_address_str);
	args_offset+=strlen(&module_args[args_offset])+1;
	sprintf(&module_args[args_offset], "-netmask=%s", subnet_mask_str);
	args_offset+=strlen(&module_args[args_offset])+1;
	sprintf(&module_args[args_offset], "-gateway=%s", gateway_str);
	args_offset+=strlen(&module_args[args_offset])+1;
	module_args[args_offset+1]='\0';
#ifdef USING_LWIP_STACK
	SifExecModuleBuffer(PS2IP141_irx, size_PS2IP141_irx, args_offset, module_args, NULL);
	SifExecModuleBuffer(HDLDSVR_irx, size_HDLDSVR_irx, 0, NULL, NULL);
#elif USING_SMSTCPIP_STACK
	SifExecModuleBuffer(SMSTCPIP_irx, size_SMSTCPIP_irx, args_offset, module_args, NULL);
	SifExecModuleBuffer(HDLDSVR_irx, size_HDLDSVR_irx, 0, NULL, NULL);
#else
	SifExecModuleBuffer(PKTDRV_irx, size_PKTDRV_irx, args_offset, module_args, NULL);
#endif
#endif

	scr_printf("done!\n");

	SifLoadFileExit();
	SifExitIopHeap();
	fileXioInit();

	scr_printf(	"\t# System initialized.\n"	\
			"\t# Initializing network protocol stack...");

#ifdef USING_NETIF_RPC
#ifdef USING_LWIP_STACK
	IP4_ADDR(&IP, ip_address[0],ip_address[1],ip_address[2],ip_address[3]);
	IP4_ADDR(&NM, subnet_mask[0],subnet_mask[1],subnet_mask[2],subnet_mask[3]);
	IP4_ADDR(&GW, gateway[0],gateway[1],gateway[2],gateway[3]);

	InitPS2IP(&IP, &NM, &GW);

	StartServer();
#else
	InitPS2IP(ip_address, subnet_mask, gateway);
#endif
#endif
	scr_printf(	"done!\n"	\
			"\t# Startup complete.\n");

	SleepThread();

#ifdef USING_NETIF_RPC
#ifdef USING_LWIP_STACK
	ShutdownServer();
#endif
	DeinitPS2IP();
#endif

	fioExit();
	SifExitRpc();
	return 0;
}
