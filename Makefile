##
## Makefile
## $Id: Makefile,v 1.12 2004/09/12 17:25:26 b081 Exp $
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

###############################################################################
# configuration start

# ASPI is supported on Windows platform, only
# however, running `hdl_dump query' while burning a CD/DVD would make a coaster
# and it kills/freezes CD/DVD drives (like `Yamaha-8424S')
INCLUDE_ASPI = no

# `yes' - debug build; something else - release build
# `RELEASE=yes make' would make a release build no matter what DEBUG flag is
DEBUG = yes

# hdl_dump current version/release
VER_MAJOR = 0
VER_MINOR = 7
VER_PATCH = 2

# configuration end
###############################################################################


CFLAGS = -Wall -ansi -pedantic -Wno-long-long

LDFLAGS =

OBJECTS = hdl_dump.o apa.o common.o progress.o hdl.o isofs.o \
	iin_img_base.o iin_optical.o iin_iso.o iin_hdloader.o iin_cdrwin.o \
	iin_nero.o iin_gi.o iin_iml.o iin_probe.o iin_net.o aligned.o \
	hio_probe.o hio_win32.o hio_net.o net_io.o


# "autodetect" Windows builds
ifdef SYSTEMROOT
  WINDOWS = yes
else
  WINDOWS = no
endif


# Windows/Unix/Linux build
ifeq ($(WINDOWS), yes)
  OBJECTS += rsrc.o osal_win32.o
  CFLAGS += -mno-cygwin -D_BUILD_WIN32
  LDFLAGS += -lwsock32
  EXESUF = .exe

  # whether to include ASPI support or not
  ifeq ($(INCLUDE_ASPI), yes)
    CFLAGS += -D_WITH_ASPI
    OBJECTS += iin_aspi.o aspi_hlio.o
  endif
else
  OBJECTS += osal_unix.o
  CFLAGS += -D_GNU_SOURCE -D_BUILD_UNIX
  EXESUF = 
endif

BINARY = hdl_dump$(EXESUF)

# whether to make debug or release build
ifeq ($(RELEASE), yes)
  DEBUG = no
endif
ifeq ($(DEBUG), yes)
  CFLAGS += -O0 -g -D_DEBUG
else
  CFLAGS += -O2 -s -DNDEBUG
endif

# version number
VERSION = -DVER_MAJOR=$(VER_MAJOR) \
	-DVER_MINOR=$(VER_MINOR) \
	-DVER_PATCH=$(VER_PATCH)
VERSION += -DVERSION=\"$(VER_MAJOR).$(VER_MINOR).$(VER_PATCH)\"
CFLAGS += $(VERSION)


all: $(BINARY)


clean:
	rm -f $(BINARY) $(OBJECTS)


rsrc.o: rsrc.rc
	@echo -e "\tR $<"
	@windres $(VERSION) -o $@ -i $<


$(BINARY): $(OBJECTS)
	@echo -e "\tL $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)


%.o : %.c
	@echo -e "\tC $<"
	@$(CC) -c $(CFLAGS) -o $@ $<


include ./.depends
