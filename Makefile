##
## Makefile
## $Id: Makefile,v 1.22 2007-05-12 20:13:04 bobi Exp $
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

# NOTE: this Makefile REQUIRES GNU make (gmake)

###############################################################################
# configuration start
# NOTE: don't forget, that changing some options REQUIRES `make clean' next!

# `yes' - debug build; something else - release build
# `RELEASE=yes make' makes a release build no matter what DEBUG flag is
DEBUG ?= yes
RELEASE ?= no

# whether to use memory-mapped I/O when reading from optical drives
# currently appears to work on Linux only
IIN_OPTICAL_MMAP ?= no

# whether to use iin (ISO inputs) tuned for "streaming" (obsoletes mmap)
USE_THREADED_IIN ?= yes

# hdl_dump current version/release
VER_MAJOR = 0
VER_MINOR = 9
VER_PATCH = 2

# https://mxe.cc/
MXE_TARGETS ?= i686-w64-mingw32.static

# configuration end
###############################################################################

CFLAGS = -Wall -ansi -pedantic -Wno-long-long

LDFLAGS =

# iin_hdloader.c iin_net.c
SOURCES = hdl_dump.c \
	apa.c common.c progress.c hdl.c isofs.c aligned.c \
	iin_img_base.c iin_optical.c iin_iso.c iin_cdrwin.c \
	iin_nero.c iin_gi.c iin_iml.c iin_probe.c iin_hio.c \
	hio_probe.c hio_win32.c hio_dbg.c hio_trace.c \
	net_common.c byteseq.c dict.c hio_udpnet2.c

# "autodetect" Windows builds
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
  CC = $(MXE_TARGETS)-gcc
  WINDRES = $(MXE_TARGETS)-windres
endif


# Windows/Unix/Linux build
ifeq ($(WINDOWS), yes)
  SOURCES += iin_spti.c iin_aspi.c aspi_hlio.c osal_win32.c
  OBJECTS += rsrc.o
  CFLAGS += -D_BUILD_WIN32
  CXXFLAGS += -D_BUILD_WIN32
  LDFLAGS += -lwsock32 -lwinmm
  EXESUF = .exe

  # make it compile with latest cygwin/mingw
  # however, that would probably not work under older versions of Windows...?
  CFLAGS += -D_WIN32_WINNT=0x0500

  # it looks like Windows doesn't support memory-mapping on optical drives
  IIN_OPTICAL_MMAP = no
else
  SOURCES += osal_unix.c
  CFLAGS += -D_GNU_SOURCE -D_BUILD_UNIX
  CXXFLAGS += -D_GNU_SOURCE -D_BUILD_UNIX
  LDFLAGS += -lpthread
  EXESUF = 
endif


# whether to make debug or release build
ifeq ($(RELEASE), yes)
  DEBUG = no
endif
ifeq ($(DEBUG), yes)
  CFLAGS += -O0 -g -D_DEBUG
  CXXFLAGS += -O0 -g -D_DEBUG
else
  CFLAGS += -O2 -s -DNDEBUG
  CXXFLAGS += -O2 -s -DNDEBUG
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
CXXFLAGS += $(VERSION)


ifeq ($(IIN_OPTICAL_MMAP), yes)
  CFLAGS += -DIIN_OPTICAL_MMAP
endif


OBJECTS += $(SOURCES:.c=.o)
DEPENDS += $(SOURCES:.c=.d)
OBJECTS := $(OBJECTS:.cpp=.o)
DEPENDS := $(DEPENDS:.cpp=.d)

BINARY = hdl_dump$(EXESUF)


###############################################################################
# make commands below...

.PHONY: all clean rmdeps

all: $(BINARY)


clean:
	@rm -f $(BINARY) $(OBJECTS)
	@rm -f $(DEPENDS)
	@rm -f *.d *.o *.exe

rmdeps:
	@rm -f $(DEPENDS)


LINT_OFF = +posixlib +unixlib \
	-fixedformalarray -exitarg -predboolint -boolops +boolint +partial \
	+matchanyintegral
lint:
	for src in $(SOURCES:osal_unix.c=); do \
		@splint -D_LINT -D_BUILD_UNIX -DVERSION="" $(LINT_OFF) $$src; \
	done

lint2:
	@splint -D_LINT -D_BUILD_UNIX -DVERSION="" $(LINT_OFF) \
		-weak -bufferoverflowhigh +longintegral +ignoresigns -ifempty \
		-varuse -initallelements $(SOURCES:osal_unix.c=)


# rules below
rsrc.o: rsrc.rc
	@echo -e "\tRES $<"
	@$(WINDRES) $(VERSION) -o $@ -i $<


$(BINARY): $(OBJECTS)
	@echo -e "\tLNK $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.c
	@echo -e "\tCC  $<"
	@$(CC) -c $(CFLAGS) -o $@ $<


%.d : %.c
	@echo -e "\tDEP $<"
	@$(CC) -MM $(CFLAGS) $< > $@


ifneq ($(MAKECMDGOAL),rmdeps)
  -include $(DEPENDS)
endif
