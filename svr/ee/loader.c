/*
 * svr/ee/loader.c
 * $Id: loader.c,v 1.3 2004/08/15 16:44:19 b081 Exp $
 *
 * Copyright 2004 Bobi B., w1zard0f07@yahoo.com
 *
 * This file is part of hdl_dump.
 *
 * hdl_dump is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * hdl_dump is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with hdl_dump; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <fileio.h>
#include <iopcontrol.h>
#include <libmc.h>
#include <ps2ip.h>
#include <iopheap.h>
#include <sbv_patches.h>

#include <fileXio_rpc.h>
#include <libhdd.h>

#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "graph.h"


#define APP_NAME "hdld_svr"
#define VERSION "0.7"


#if 1 /* debugging */
#  define printf scr_printf
#else
#  define printf(...)
#  define init_scr(...)
#endif


#define RESET_IOP
#define LOAD_MRBROWN_PATCHES
#define LOAD_SIOMAN_AND_MC
/* #define LOAD_POWEROFF */
#define LOAD_PS2ATAD /* if not loaded server.irx refuses to load?!? */
#define LOAD_SERVER


extern u8 *iomanx_irx;			// (c) 2003 Marcus R. Brown <mrbrown@0xd6.org> IOP module
extern int size_iomanx_irx;		// from PS2DRV to handle 'standard' PS2 device IO

extern u8 *ps2dev9_irx;			// (c) 2003 Marcus R. Brown <mrbrown@0xd6.org> IOP module
extern int size_ps2dev9_irx;		// from PS2DRV to handle low-level HDD device

extern u8 *ps2ip_irx;
extern int size_ps2ip_irx;

extern u8 *ps2smap_irx;
extern int size_ps2smap_irx;

extern u8 *ps2atad_irx;			// (c) 2003 Marcus R. Brown <mrbrown@0xd6.org> IOP module
extern int size_ps2atad_irx;		// from PS2DRV to handle low-level ATA for HDD

extern u8 *poweroff_irx;		// (c) 2003 Vector IOP module to handle PS2 reset/shutdown
extern int size_poweroff_irx;		// from LIBHDD v1.0

extern u8 *hdlsvr_iop_irx;
extern int size_hdlsvr_iop_irx;


#define IPCONF_MAX_LEN (3 * 16)
char __attribute__((aligned(16))) if_conf [IPCONF_MAX_LEN];
int if_conf_len;

const char *default_ip = "192.168.0.10";
const char *default_mask = "255.255.255.0";
const char *default_gateway = "192.168.0.1";

/**************************************************************/
size_t /* "192.168.0.10 255.255.255.0 192.168.0.1" */
setup_ip (char outp [IPCONF_MAX_LEN])
{
  size_t result = 0;
  int conf_ok = 0;
#if defined (LOAD_SIOMAN_AND_MC)
  int fd = fioOpen ("mc0:/SYS-CONF/IPCONFIG.DAT", O_RDONLY);
  if (!(fd < 0))
    { /* configuration file found */
      char tmp [IPCONF_MAX_LEN];
      int len = fioRead (fd, tmp, IPCONF_MAX_LEN - 1);
      fioClose (fd);

      if (len > 0)
	{
	  int data_ok = 1;
	  int i;
	  tmp [len] = '\0';
	  for (i=0; data_ok && i<len; ++i)
	    if (isdigit (tmp [i]) || tmp [i] == '.')
	      ;
	    else if (isspace (tmp [i]))
	      tmp [i] = '\0';
	    else
	      data_ok = 0;

	  if (data_ok)
	    {
	      memcpy (outp, tmp, IPCONF_MAX_LEN);
	      conf_ok = 1;
	      result = len;
	    }
	  else
	    printf ("IPCONFIG.DAT format (use a single space as a separator):\n"
		    "ip_address network_mask gateway_ip\n");
	}
    }
  else
    printf ("use mc0:/SYS-CONF/IPCONFIG.DAT to configure IP address\n");
#endif

  if (!conf_ok)
    { /* configuration file not found; use hard-coded defaults */
      int len, pos = 0;

      len = strlen (default_ip);
      memcpy (outp + pos, default_ip, len); pos += len;
      *(outp + pos++) = '\0';

      len = strlen (default_mask);
      memcpy (outp + pos, default_mask, len); pos += len;
      *(outp + pos++) = '\0';

      len = strlen (default_gateway);
      memcpy (outp + pos, default_gateway, len); pos += len;
      *(outp + pos++) = '\0';
      result = pos;
    }

  return (result);
}


/**************************************************************/
void
load_modules (int init_tcpip)
{
#if defined (LOAD_LIBHDD) && defined (LOAD_PS2HDD)
  static const char *hddarg = "-o\0" "4\0" "-n\0" "20";
#endif
#if defined (LOAD_PS2FS)
  static const char *pfsarg = "-m\0" "4\0" "-o\0" "10\0" "-n\0" "40";
#endif
  int ret;

#if defined (LOAD_SIOMAN_AND_MC)
  printf ("SIO2MAN");
  ret = SifLoadModule ("rom0:SIO2MAN", 0, NULL);
  printf (": %d\n", ret);
  printf ("MCMAN");
  ret = SifLoadModule ("rom0:MCMAN", 0, NULL);
  printf (": %d\n", ret);
  printf ("MCSERV");
  ret = SifLoadModule ("rom0:MCSERV", 0, NULL);
  printf (": %d\n", ret);
#endif

#if defined (LOAD_POWEROFF)
  printf ("POWEROFF.IRX");
  SifExecModuleBuffer (&poweroff_irx, size_poweroff_irx, 0, NULL, &ret);
  printf (": %d\n", ret);
#endif

  printf ("IOMANX.IRX");
  SifExecModuleBuffer (&iomanx_irx, size_iomanx_irx, 0, NULL, &ret);
  printf (": %d\n", ret);

  printf ("PS2DEV9.IRX", size_ps2dev9_irx);
  SifExecModuleBuffer (&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
  printf (": %d\n", ret);

#if defined (LOAD_PS2ATAD)
  printf ("PS2ATAD.IRX");
  SifExecModuleBuffer (&ps2atad_irx, size_ps2atad_irx, 0, NULL, &ret);
  printf (": %d\n", ret);
#endif

  if (init_tcpip)
    {
      int pos1, pos2;
      printf ("PS2IP.IRX");
      SifExecModuleBuffer (&ps2ip_irx, size_ps2ip_irx, 0, NULL, &ret);
      printf (": %d\n", ret);

      if_conf_len = setup_ip (if_conf);

      pos1 = strlen (if_conf) + 1;
      pos2 = strlen (if_conf + pos1) + 1;
      printf ("Playstation 2 IP address: %s\n",
	      if_conf, if_conf + pos1, if_conf + pos1 + pos2, if_conf_len);

      printf ("PS2SMAP.IRX");
      SifExecModuleBuffer (&ps2smap_irx, size_ps2smap_irx, if_conf_len, if_conf, &ret);
      printf (": %d\n", ret);
    }
  else
    printf ("Assuming TCP/IP is already initialized\n");

#if defined (LOAD_SERVER)
  printf ("HDLD_SVR.IRX");
  SifExecModuleBuffer (&hdlsvr_iop_irx, size_hdlsvr_iop_irx, 0, NULL, &ret);
  printf (": %d\n", ret);
#endif
}


/**************************************************************/
int
main (int argc,
      char *argv [])
{
  int init_tcpip = 0;

  SifInitRpc (0);
  init_scr ();

  scr_printf (APP_NAME "-" VERSION "\n");

  /* decide whether to load TCP/IP or it is already loaded */
#if !defined (RESET_IOP)
  if (argc == 0) /* Naplink */
    init_tcpip = 1;
  else
    {
      if (strncmp (argv [0], "host:", 5) == 0)
	init_tcpip = 0; /* assume loading from PS2LINK */
      if (strncmp (argv [0], "mc0:", 4) == 0 || /* loading from memory card */
	  strncmp (argv [0], "cdrom", 5) == 0)  /* loading from CD */
	init_tcpip = 1;
    }
#else
  init_tcpip = 1;
#endif

#if defined (RESET_IOP)
  SifExitIopHeap ();
  SifLoadFileExit ();
  SifExitRpc ();

  SifIopReset (NULL /* "rom0:UDNL rom0:EELOADCNF" */, 0);
  while (SifIopSync ())
    ;
  SifInitRpc (0);
#endif

#if defined (LOAD_MRBROWN_PATCHES)
  printf ("MR Brown patches\n");
  sbv_patch_enable_lmb ();
  sbv_patch_disable_prefix_check ();
#endif

  load_modules (init_tcpip);

  /* our job is done; IOP would handle the rest */
  SleepThread ();

  return (0);
}
