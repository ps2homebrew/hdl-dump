##
## svr/ee/Makefile
## $Id: Makefile,v 1.5 2005/05/06 14:50:35 b081 Exp $
##
## Copyright 2004 Bobi B., w1zard0f07@yahoo.com
##
## This file is part of hdl_dump.
##
## hdl_dump is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## hdl_dump is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with hdl_dump; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##

EE_CFLAGS = -mips3 -ffreestanding -fno-builtin -G0 \
	-fshort-double -mlong64 -mhard-float -mno-abicalls -Os -EL -Wall -s

EE_LDFLAGS = -s

EE_ASFLAGS = -EL

EE_LIBS = -lc -lpatches

EE_BIN = hdld_svr.elf

EE_OBJS = loader.o graph.o font.o hdld_svr_iop.o \
	iomanx.o ps2dev9.o ps2ip.o ps2smap.o ps2atad.o

IRX_PATH = ../irx

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) *.s

load: $(EE_BIN)
	ps2client reset
	sleep 2
	ps2client execee host:$(EE_BIN)

iomanx.s: $(IRX_PATH)/iomanX.irx
	@bin2s $< $@ iomanx_irx

ps2dev9.s: $(IRX_PATH)/ps2dev9.irx
	@bin2s $< $@ ps2dev9_irx

ps2atad.s: $(IRX_PATH)/ps2atad.irx
	@bin2s $< $@ ps2atad_irx

ps2ip.s: $(IRX_PATH)/ps2ip.irx
	@bin2s $< $@ ps2ip_irx

ps2smap.s: $(IRX_PATH)/ps2smap.irx
	@bin2s $< $@ ps2smap_irx

poweroff.s: $(IRX_PATH)/poweroff.irx
	@bin2s $< $@ poweroff_irx

hdld_svr_iop.s: ../iop/hdld_svr.irx
	@bin2s $< $@ hdlsvr_iop_irx

include ../Defs.make
include ../ee-rules.make
