##
## Makefile
## $Id: Makefile,v 1.10 2007-05-12 20:18:11 bobi Exp $
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

# NOTE: this Makefile REQUIRES GNU make (gmake)

###############################################################################
# configuration start
# NOTE: don't forget, that changing some options REQUIRES `make clean' next!

# include icon in the executable (`yes') or look for an extenal icon (other)
BUILTIN_ICON ?= yes

# `yes' - debug build; something else - release build
# `RELEASE=yes make' makes a release build no matter what DEBUG flag is
DEBUG ?= yes
RELEASE ?= no

# whether to use memory-mapped I/O when reading from optical drives
IIN_OPTICAL_MMAP ?= no

# whether to use iin (ISO inputs) tuned for "streaming" (obsoletes mmap)
USE_THREADED_IIN ?= yes

# hdl_dump current version/release
VER_MAJOR = 0
VER_MINOR = 9
VER_PATCH = 0

# configuration end
###############################################################################


CFLAGS = -Wall -Wno-long-long

LDFLAGS =

VPATH = ./:../

# iin_hdloader.c iin_net.c 
SOURCES = gui_main.c \
	apa.c common.c progress.c hdl.c isofs.c aligned.c \
	iin_img_base.c iin_optical.c iin_iso.c iin_cdrwin.c \
	iin_nero.c iin_gi.c iin_iml.c iin_spti.c iin_probe.c \
	iin_hio.c thd_iin.c iin_aspi.c aspi_hlio.c \
	hio_probe.c hio_win32.c hio_dbg.c hio_trace.c \
	osal_win32.c net_common.c byteseq.c dict.c hio_udpnet2.c


# "autodetect" Windows builds
# since this is a _Windows_ GUI, there are two cases here:
# either it is build with CYGWIN gcc or against winelib
ifdef SYSTEMROOT
  WINDOWS = yes
else
  WINDOWS = no
endif

# Windows cross-compilation with mingw32* on Debian
# make XC=win ...
WINDRES = windres
ifeq ($(XC), win)
  WINDOWS = yes
  CC = i586-mingw32msvc-gcc
  WINDRES = i586-mingw32msvc-windres
endif


CFLAGS += -mwindows -D_BUILD_WIN32
LDFLAGS += -lwsock32 -lcomctl32 -lwinmm

# whether to include ASPI support or not
ifeq ($(WINDOWS), yes)
  CFLAGS += -D_WITH_ASPI -mno-cygwin
  CFLAGS += -ansi -pedantic

  SOURCES += 
  OBJECTS += rsrc.o
  EXESUF = .exe

  # it looks like Windows doesn't support memory-mapping on optical drives
  IIN_OPTICAL_MMAP = no
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

ifeq ($(USE_THREADED_IIN), yes)
  SOURCES += thd_iin.c
  CFLAGS += -DUSE_THREADED_IIN
  CXXFLAGS += -DUSE_THREADED_IIN
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


ifeq ($(IIN_OPTICAL_MMAP), yes)
  CFLAGS += -DIIN_OPTICAL_MMAP
endif


OBJECTS += $(SOURCES:.c=.o)
DEPENDS += $(SOURCES:.c=.d)

BINARY = hdl_dumb$(EXESUF)


###############################################################################
# make commands below...

all: $(BINARY)


clean:
	@rm -f $(BINARY) $(OBJECTS)
ifeq ($(WINDOWS), no)
	@rm -f $(BINARY).exe.so
endif

rmdeps:
	@rm -f $(DEPENDS)


rsrc.o: rsrc.rc rsrc.h rsrc/app.ico rsrc/manifest.bin \
	rsrc/ps2_cd.bmp rsrc/ps2_dvd.bmp rsrc/ps2_dvd9.bmp
	@echo -e "\tRES $<"
	@echo '#include <windows.h>' > afxres.h
	@echo '#define IDC_STATIC -1' >> afxres.h
	@$(WINDRES) $(VERSION) -o $@ -i $<


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
  -include $(DEPENDS)
endif
