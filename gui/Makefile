##
## Makefile
## $Id: Makefile,v 1.6 2005/07/10 21:06:48 bobi Exp $
##
## Copyright 2004 Bobi B., w1zard0f07@yahoo.com
##
## This file is part of hdl_dumb.
##
## hdl_dumb is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## hdl_dumb is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with hdl_dumb; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##

###############################################################################
# configuration start
# NOTE: don't forget, that changing some options REQUIRES `make clean' next!

# include icon in the executable (`yes') or look for an extenal icon (other)
BUILTIN_ICON ?= yes

# `yes' - debug build; something else - release build
# `RELEASE=yes make' makes a release build no matter what DEBUG flag is
RELEASE ?= no
DEBUG ?= yes

# `yes' - don't expect ACK when streaming data to the PS2 (works faster?)
#
# combination RAW_speed compressed_dummy_file_speed
# no/no/no    0,68MBps  2,90MBps
# no/no/yes   0,76MBps  2,95MBps
# no/yes/yes  0,80MBps  3,00MBps
SEND_NOACK ?= no
QUICK_ACK ?= yes
DUMMY_ACK ?= yes

# hdl_dump current version/release
VER_MAJOR = 0
VER_MINOR = 8
VER_PATCH = 2

# configuration end
###############################################################################


CFLAGS = -Wall -Wno-long-long

LDFLAGS =

VPATH = ./:../

SOURCES = gui_main.c osal_win32.c apa.c common.c progress.c \
	hdl.c isofs.c aligned.c \
	iin_probe.c iin_img_base.c iin_iso.c iin_optical.c iin_cdrwin.c \
	iin_hdloader.c iin_iml.c iin_nero.c iin_gi.c iin_net.c net_io.c \
	hio_probe.c hio_win32.c hio_net.c byteseq.c dict.c


# "autodetect" Windows builds
# since this is a _Windows_ GUI, there are two cases here:
# either it is build with CYGWIN gcc or against winelib
ifdef SYSTEMROOT
  WINDOWS = yes
else
  WINDOWS = no
endif


CFLAGS += -mwindows -D_BUILD_WIN32
LDFLAGS += -lwsock32 -lcomctl32

# whether to include ASPI support or not
ifeq ($(WINDOWS), yes)
  CFLAGS += -D_WITH_ASPI -mno-cygwin
  CFLAGS += -ansi -pedantic

  OBJECTS += iin_aspi.o aspi_hlio.o
  OBJECTS += rsrc.o
  EXESUF = .exe
else
  # assume it is winelib/wine build
  CFLAGS += -D_BUILD_WINE

  OBJECTS += rsrc.res
  EXESUF = 

  CC = winegcc
  RC = wrc
  WINEBUILD = winebuild
endif

# make it compile with latest cygwin/mingw
# however, that would probably not work under older versions of Windows
CFLAGS += -D_WIN32_WINNT=0x0500


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


# built-in icon support
ifeq ($(BUILTIN_ICON), yes)
  CFLAGS += -DBUILTIN_ICON
endif


# ACK/NOACK support
ifeq ($(SEND_NOACK), yes)
  CFLAGS += -DNET_SEND_NOACK
endif
ifeq ($(DUMMY_ACK), yes)
  CFLAGS += -DNET_DUMMY_ACK
endif
ifeq ($(QUICK_ACK), yes)
  CFLAGS += -DNET_QUICK_ACK
endif


OBJECTS += $(SOURCES:.c=.o)
DEPENDS += $(SOURCES:.c=.d)

BINARY = hdl_dumb$(EXESUF)


###############################################################################
# make commands below...

all: $(BINARY)


clean:
	rm -f $(BINARY) $(OBJECTS)

rmdeps:
	rm -f $(DEPENDS)


rsrc.o: rsrc.rc rsrc.h rsrc/app.ico rsrc/manifest.bin \
	rsrc/ps2_cd.bmp rsrc/ps2_dvd.bmp rsrc/ps2_dvd9.bmp
	@echo -e "\tRES $<"
	@windres $(VERSION) -o $@ -i $<


$(BINARY): $(OBJECTS)
	@echo -e "\tLNK $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)


.SUFFIXES: .o .exe .c


%.o : %.c
	@echo -e "\tCC  $<"
	@$(CC) -c $(CFLAGS) -o $@ $<


%.d : %.c
	@echo -e "\tDEP $<"
	@$(CC) -MM $(CFLAGS) $< > $@


# for wine
%.res : %.rc
	@echo -e "\tRC  $<"
	@echo '#include "winres.h"' > afxres.h
	@$(RC) $(RCFLAGS) $(RCEXTRA) $(DEFINCL) -fo$@ $<
	@rm -f afxres.h


ifneq ($(MAKECMDGOAL),rmdeps)
  include $(DEPENDS)
endif
