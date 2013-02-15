##
## Makefile
## $Id: Makefile,v 1.11 2004/08/20 12:35:17 b081 Exp $
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

CFLAGS = -O0 -g -Wall -ansi -pedantic -Wno-long-long -mno-cygwin \
	-D_BUILD_WIN32 -D_DEBUG # -D_WITH_ASPI #-DCRIPPLED_INJECTION
LDFLAGS = -lwsock32

all: hdl_dump.exe

# that is ugly
release: rsrc.o
	$(CC) -Wall -ansi -pedantic -Wno-long-long -mno-cygwin -D_BUILD_WIN32 -O2 \
		-o hdl_dump.exe *.c $^ $(LDFLAGS)
	strip hdl_dump.exe

clean:
	rm -f hdl_dump.exe *.o

rsrc.o: rsrc.rc
	@echo -e "\tR $<"
	@windres -o $@ -i $<

# +ioctl_hlio.o 
hdl_dump.exe: hdl_dump.o rsrc.o osal_win32.o apa.o common.o progress.o hdl.o isofs.o \
		iin_img_base.o iin_optical.o iin_iso.o iin_hdloader.o iin_cdrwin.o \
		iin_nero.o iin_gi.o iin_iml.o iin_probe.o iin_net.o aligned.o \
		hio_probe.o hio_win32.o hio_net.o net_io.o iin_aspi.o aspi_hlio.o
	@echo -e "\tL $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.c
	@echo -e "\tC $<"
	@$(CC) -c $(CFLAGS) -o $@ $<

include ./.depends
