/*
 * svr/ee/loader.c
 * $Id: loader.c,v 1.9 2005/12/08 20:43:11 bobi Exp $
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


/*
 * this program were supposed to produce the following output:
 * hdld_svr-x.y.z
 * MR Brown patches
 * SIO2MAN: 25
 * MCMAN: 26
 * MCSERV: 27
 * IOMANX.IRX: 0
 * PS2DEV9.IRX: 0
 * PS2ATAD.IRX: 0
 * PS2IP.IRX: 0
 * message for config file
 * Playstation 2 IP address: a.b.c.d
 * PS2SMAP.IRX: 0
 * HDLD_SVR.IRX: 0
 */

#define APP_NAME "hdld_svr"
#define VERSION "0.8.3"


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
#define LOAD_PS2ATAD
/*#define LOAD_SERVER*/


extern u8 *iomanx_irx;
extern int size_iomanx_irx;

extern u8 *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern u8 *ps2ip_irx;
extern int size_ps2ip_irx;

extern u8 *ps2smap_irx;
extern int size_ps2smap_irx;

extern u8 *ps2atad_irx;
extern int size_ps2atad_irx;

extern u8 *poweroff_irx;
extern int size_poweroff_irx;

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
setup_ip (char outp [IPCONF_MAX_LEN], int *retval)
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
	    {
	      *retval = -2; /* bad format */
	    }
	}
    }
  else
    {
      *retval = -1;
    }
#else
  *retval = 0;
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
static int
load_modules (int init_tcpip)
{
  const char *STEP_OK = "*";
  const char *FAILED = "failed to load with";
#if defined (LOAD_LIBHDD) && defined (LOAD_PS2HDD)
  static const char *hddarg = "-o\0" "4\0" "-n\0" "20";
#endif
#if defined (LOAD_PS2FS)
  static const char *pfsarg = "-m\0" "4\0" "-o\0" "10\0" "-n\0" "40";
#endif
  int ret, ipcfg_ret = 0;

#if defined (LOAD_MRBROWN_PATCHES)
  sbv_patch_enable_lmb ();
  sbv_patch_disable_prefix_check ();
  scr_printf (STEP_OK);
#endif

#if defined (LOAD_SIOMAN_AND_MC)
  ret = SifLoadModule ("rom0:SIO2MAN", 0, NULL);
  if (ret > 0)
    scr_printf (STEP_OK);
  else
    {
      scr_printf ("\nrom0:SIO2MAN %s %d\n", FAILED, ret);
      return (-1);
    }

  ret = SifLoadModule ("rom0:MCMAN", 0, NULL);
  if (ret > 0)
    scr_printf (STEP_OK);
  else
    {
      scr_printf ("\nrom0:MCMAN %s %d\n", FAILED, ret);
      return (-1);
    }

  ret = SifLoadModule ("rom0:MCSERV", 0, NULL);
  if (ret > 0)
    scr_printf (STEP_OK);
  else
    {
      scr_printf ("\nrom0:MCSERV %s %d\n", FAILED, ret);
      return (-1);
    }
#endif

  SifExecModuleBuffer (&iomanx_irx, size_iomanx_irx, 0, NULL, &ret);
  if (ret == 0)
    scr_printf (STEP_OK);
  else
    {
      scr_printf ("IOMANX.IRX %s %d\n", FAILED, ret);
      return (-1);
    }

  SifExecModuleBuffer (&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
  if (ret == 0)
    scr_printf (STEP_OK);
  else
    {
      scr_printf ("PS2DEV9.IRX %s %d\n", FAILED, ret);
      return (-1);
    }

#if defined (LOAD_PS2ATAD)
  SifExecModuleBuffer (&ps2atad_irx, size_ps2atad_irx, 0, NULL, &ret);
  if (ret == 0)
    scr_printf (STEP_OK);
  else
    {
      scr_printf ("PS2ATAD.IRX %s %d\n", FAILED, ret);
      return (-1);
    }
#endif

  if (init_tcpip)
    {
      SifExecModuleBuffer (&ps2ip_irx, size_ps2ip_irx, 0, NULL, &ret);
      if (ret == 0)
	scr_printf (STEP_OK);
      else
	{
	  scr_printf ("PS2IP.IRX %s %d\n", FAILED, ret);
	  return (-1);
	}

      if_conf_len = setup_ip (if_conf, &ipcfg_ret);

      SifExecModuleBuffer (&ps2smap_irx, size_ps2smap_irx,
			   if_conf_len, if_conf, &ret);
      if (ret == 0)
	scr_printf (STEP_OK);
      else
	{
	  scr_printf ("PS2SMAP.IRX %s %d\n", FAILED, ret);
	  return (-1);
	}
    }
  else
    printf ("Assuming TCP/IP is already initialized\n");

#if defined (LOAD_SERVER)
  SifExecModuleBuffer (&hdlsvr_iop_irx, size_hdlsvr_iop_irx, 0, NULL, &ret);
  if (ret == 0)
    scr_printf (STEP_OK);
  else
    {
      scr_printf ("HDLD_SVR.IRX %s %d\n", FAILED, ret);
      return (-1);
    }
#endif

  scr_printf ("\n");

  if (ipcfg_ret == -1)
    scr_printf ("\nuse mc0:/SYS-CONF/IPCONFIG.DAT to configure IP address\n\n");

  if (ipcfg_ret == -2)
    scr_printf ("\nIPCONFIG.DAT format is:\n"
		"ip_address network_mask gateway_ip\n"
		"(use a single space as separator)\n\n");

  if (ipcfg_ret != 0)
    scr_printf ("Playstation 2 IP address: %s\n", if_conf);

  return (0);
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

  if (load_modules (init_tcpip) == 0)
    {
      scr_printf ("Ready\n");
    }
  else
    scr_printf ("Failed to load\n");

  /* our job is done; IOP would handle the rest */
  SleepThread ();

  return (0);
}
