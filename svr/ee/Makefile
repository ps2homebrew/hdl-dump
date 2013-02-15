##
## svr/ee/Makefile
## $Id: Makefile,v 1.2 2004/08/15 16:44:19 b081 Exp $
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

EE_INCS := $(EE_INCS) -I$(LIBHDD)/ee/include -I$(LIBHDD)/common/include \
	   -I$(PS2DRV)/common/include -I$(PS2DRV)/ee/libfileXio \
	   -I$(PS2LIB)/ee/include -I$(PS2LIB)/common/include \
	   -I$(PS2DEV)/sbv-1.0-lite/include -I$(PS2DEV)/libcdvd/ee \
	   -I$(PS2DEV)/libcdvd/common -I$(PS2DEV)/libito/include -I. \
	   -I$(SBVLITE)/include

EE_CFLAGS  := -mips3 -ffreestanding -fno-builtin -G0 \
		-fshort-double -mlong64 -mhard-float -mno-abicalls -O2 -EL -Wall \
		-c $(EE_INCS) $(EE_CFLAGS)
EE_ASFLAGS := -EL -G0 $(EE_ASFLAGS)

EE_LDFLAGS := $(EE_LDFLAGS) -s -L$(LIBHDD)/ee/lib -L$(PS2DRV)/ee/libfileXio \
		-L$(PS2LIB)/ee/lib -L$(PS2DEV)/sbv-1.0-lite/lib \
		-L$(PS2DEV)/libcdvd/lib -L$(PS2DEV)/libito/lib \
		-L$(SBVLITE)/lib \
		-T$(PS2LIB)/ee/startup/linkfile -nostartfiles

EE_LIBS = -lhdd -lfileXio -lps2ip -lc -lsbv_patches

EE_BIN = hdld_svr.elf

EE_OBJS = loader.o graph.o font.o hdld_svr_iop.o \
	iomanx.o ps2dev9.o ps2ip.o ps2smap.o ps2atad.o

IRX_PATH = ../irx

all: $(EE_BIN)

clean:
	rm -f *.elf *.o *.a *.s

iomanx.s: $(IRX_PATH)/iomanx.irx
	bin2s $< $@ iomanx_irx

filexio.s: $(IRX_PATH)/fileXio.irx
	bin2s $< $@ filexio_irx

ps2atad.s: $(IRX_PATH)/ps2atad.irx
	bin2s $< $@ ps2atad_irx

ps2dev9.s: $(IRX_PATH)/ps2dev9.irx
	bin2s $< $@ ps2dev9_irx

ps2ip.s: $(IRX_PATH)/ps2ip.irx
	bin2s $< $@ ps2ip_irx

ps2ips.s: $(IRX_PATH)/ps2ips.irx
	bin2s $< $@ ps2ips_irx

ps2smap.s: $(IRX_PATH)/ps2smap.irx
	bin2s $< $@ ps2smap_irx

ps2fs.s: $(IRX_PATH)/ps2fs.irx
	bin2s $< $@ ps2fs_irx

ps2hdd.s: $(IRX_PATH)/ps2hdd.irx
	bin2s $< $@ ps2hdd_irx

poweroff.s: $(IRX_PATH)/poweroff.irx
	bin2s $< $@ poweroff_irx

hdld_svr_iop.s: ../iop/hdld_svr.irx
	bin2s $< $@ hdlsvr_iop_irx

include $(PS2SDK)/Defs.make
include $(PS2SDK)/ee-rules.make
