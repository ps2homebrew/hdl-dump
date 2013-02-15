/*
 * svr/ee/loader.c
 * $Id: loader.c,v 1.12 2007-05-12 20:18:51 bobi Exp $
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
#define VERSION "0.9.0a"


#if 1 /* debugging */
#  define printf scr_printf
#else
#  define printf(...)
#  define init_scr(...)
#endif


#define RESET_IOP
#define LOAD_MRBROWN_PATCHES


extern u8 *iomanx_irx;
extern int size_iomanx_irx;

extern u8 *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern u8 *pktdrv_irx;
extern int size_pktdrv_irx;

extern u8 *ps2atad_irx;
extern int size_ps2atad_irx;


#define IPCONF_MAX_LEN (3 * 16)
char __attribute__((aligned(16))) if_conf [IPCONF_MAX_LEN];
size_t if_conf_len;

const char *default_ip = "192.168.0.10";
const char *default_mask = "255.255.255.0";
const char *default_gateway = "192.168.0.1";


/**************************************************************/
int /* "192.168.0.10 255.255.255.0 192.168.0.1" */
setup_ip (const char *ipconfig_dat_path,
	  char outp [IPCONF_MAX_LEN], size_t *length)
{
  int result = 0;
  int conf_ok = 0;

  int fd = fioOpen (ipconfig_dat_path, O_RDONLY);
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
	      *length = len;
	    }
	  else
	    /* bad format */
	    result = -2;
	}
    }
  else
    /* not found */
    result = -1;

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
      *length = pos;
    }

  return (result);
}


/**************************************************************/
static int
load_modules ()
{
  const char *STEP_OK = "*";
  const char *FAILED = "failed to load with";
  int ret, ipcfg_ret = 0;

  size_t i;
  const char *IPCONFIG_DAT_PATHS[] =
    {
      "mc0:/BIDATA-SYSTEM/IPCONFIG.DAT", /* japan */
      "mc0:/BADATA-SYSTEM/IPCONFIG.DAT", /* us */
      "mc0:/BEDATA-SYSTEM/IPCONFIG.DAT", /* europe */
      "mc0:/SYS-CONF/IPCONFIG.DAT", /* old location */
      NULL
    };

  sbv_patch_enable_lmb ();
  sbv_patch_disable_prefix_check ();
  scr_printf (STEP_OK);

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

  SifExecModuleBuffer (&ps2atad_irx, size_ps2atad_irx, 0, NULL, &ret);
  if (ret == 0)
    scr_printf (STEP_OK);
  else
    {
      scr_printf ("PS2ATAD.IRX %s %d\n", FAILED, ret);
      return (-1);
    }

  ipcfg_ret = -1;
  for (i = 0; ipcfg_ret != 0 && IPCONFIG_DAT_PATHS[i] != NULL; ++i)
    {
      ipcfg_ret = setup_ip (IPCONFIG_DAT_PATHS[i], if_conf, &if_conf_len);
    }

  SifExecModuleBuffer (&pktdrv_irx, size_pktdrv_irx,
		       if_conf_len, if_conf, &ret);
  if (ret == 0)
    scr_printf (STEP_OK);
  else
    {
      scr_printf ("PKTDRV.IRX %s %d\n", FAILED, ret);
      return (-1);
    }

  scr_printf ("\n");

  switch (ipcfg_ret)
    {
    case 0:
      scr_printf ("\nusing %s\n", IPCONFIG_DAT_PATHS[i - 1]);
      break;

    case -1:
      scr_printf ("\nuse one of the following locations to set IP address:\n");
      for (i = 0; IPCONFIG_DAT_PATHS[i] != NULL; ++i)
	scr_printf ("   %s\n", IPCONFIG_DAT_PATHS[i]);
      break;

    case -2:
      scr_printf ("\nusing %s\n", IPCONFIG_DAT_PATHS[i - 1]);
      scr_printf ("\ninvalid configuration file format; use:\n"
		  "ip_address network_mask gateway_ip\n"
		  "separated by a single space; for example:"
		  "192.168.0.10 255.255.255.0 192.168.0.1\n\n");
      break;
    }
  scr_printf ("Playstation 2 IP address: %s\n", if_conf);

  return (0);
}


/**************************************************************/
int
main (int argc,
      char *argv [])
{
  SifInitRpc (0);

  init_scr ();
  scr_printf (APP_NAME "-" VERSION "\n");

  /* decide whether to load TCP/IP or it is already loaded */
  SifExitIopHeap ();
  SifLoadFileExit ();
  SifExitRpc ();

  SifIopReset (NULL /* "rom0:UDNL rom0:EELOADCNF" */, 0);
  while (SifIopSync ())
    ;
  SifInitRpc (0);

  if (load_modules () == 0)
    {
      scr_printf ("Ready\n");
    }
  else
    scr_printf ("Failed to load\n");

  /* our job is done; IOP would handle the rest */
  SleepThread ();

  return (0);
}
