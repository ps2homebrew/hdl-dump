##
## Makefile
## $Id: Makefile,v 1.13 2004/09/26 19:39:39 b081 Exp $
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
# NOTE: don't forget, that changing some options REQUIRES `make clean' next!

# ASPI is supported on Windows platform, only
# however, running `hdl_dump query' while burning a CD/DVD would make a coaster
# and it kills/freezes some CD/DVD drives (like `Yamaha-8424S')
INCLUDE_ASPI ?= no

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

# `yes' - compress data being sent during network transfers
# sends less data, but makes additional load on the IOP
COMPRESS_DATA ?= yes

# hdl_dump current version/release
VER_MAJOR = 0
VER_MINOR = 7
VER_PATCH = 3

# configuration end
###############################################################################


CFLAGS = -Wall -ansi -pedantic -Wno-long-long

LDFLAGS =

SOURCES = hdl_dump.c apa.c common.c progress.c hdl.c isofs.c \
	iin_img_base.c iin_optical.c iin_iso.c iin_hdloader.c iin_cdrwin.c \
	iin_nero.c iin_gi.c iin_iml.c iin_probe.c iin_net.c aligned.c \
	hio_probe.c hio_win32.c hio_net.c net_io.c


# "autodetect" Windows builds
ifdef SYSTEMROOT
  WINDOWS = yes
else
  WINDOWS = no
endif


# Windows/Unix/Linux build
ifeq ($(WINDOWS), yes)
  SOURCES += osal_win32.c
  OBJECTS += rsrc.o
  CFLAGS += -mno-cygwin -D_BUILD_WIN32
  LDFLAGS += -lwsock32
  EXESUF = .exe

  # whether to include ASPI support or not
  ifeq ($(INCLUDE_ASPI), yes)
    CFLAGS += -D_WITH_ASPI
    OBJECTS += iin_aspi.o aspi_hlio.o
  endif

  # make it compile with latest cygwin/mingw
  # however, that would probably not work under older versions of Windows
  CFLAGS += -D_WIN32_WINNT=0x0500
else
  SOURCES += osal_unix.c
  CFLAGS += -D_GNU_SOURCE -D_BUILD_UNIX
  EXESUF = 
endif


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


# compressed/uncompressed data transfer support
ifeq ($(COMPRESS_DATA), yes)
  CFLAGS += -DCOMPRESS_DATA
endif


OBJECTS += $(SOURCES:.c=.o)
DEPENDS += $(SOURCES:.c=.d)

BINARY = hdl_dump$(EXESUF)


###############################################################################
# make commands below...

.PHONY: all clean rmdeps

all: $(BINARY)


clean:
	rm -f $(BINARY) $(OBJECTS)


rmdeps:
	rm -f $(DEPENDS)


# rules below
rsrc.o: rsrc.rc
	@echo -e "\tRES $<"
	@windres $(VERSION) -o $@ -i $<


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
  include $(DEPENDS)
endif
