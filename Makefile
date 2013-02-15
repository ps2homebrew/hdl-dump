##
## Makefile
## $Id: Makefile,v 1.18 2005/12/08 20:39:43 bobi Exp $
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

# include icon in the executable (`yes') or look for an extenal icon (other)
BUILTIN_ICON ?= yes

# `yes' - debug build; something else - release build
DEBUG ?= yes
# `RELEASE=yes make' makes a release build no matter what DEBUG flag is
RELEASE ?= no

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
VER_MINOR = 8
VER_PATCH = 3

# configuration end
###############################################################################


CFLAGS = -Wall -ansi -pedantic -Wno-long-long
CXXFLAGS = -Wall -ansi -pedantic -Wno-long-long

LDFLAGS =

SOURCES = hdl_dump.c apa.c common.c progress.c hdl.c isofs.c \
	iin_img_base.c iin_optical.c iin_iso.c iin_hdloader.c iin_cdrwin.c \
	iin_nero.c iin_gi.c iin_iml.c iin_probe.c iin_net.c aligned.c \
	hio_probe.c hio_win32.c net_io.c net_common.c \
	byteseq.c dict.c hio_udpnet.c # hio_net.c

# "autodetect" Windows builds
ifdef SYSTEMROOT
  WINDOWS = yes
else
  WINDOWS = no
endif


# Windows/Unix/Linux build
ifeq ($(WINDOWS), yes)
  SOURCES += osal_win32.c
  OBJECTS += iin_aspi.o aspi_hlio.o rsrc.o
  CFLAGS += -mno-cygwin -D_BUILD_WIN32
  CXXFLAGS += -mno-cygwin -D_BUILD_WIN32
  LDFLAGS += -lwsock32 -lwinmm
  EXESUF = .exe

  # make it compile with latest cygwin/mingw
  # however, that would probably not work under older versions of Windows...?
  CFLAGS += -D_WIN32_WINNT=0x0500
else
  SOURCES += osal_unix.c
  CFLAGS += -D_GNU_SOURCE -D_BUILD_UNIX
  CXXFLAGS += -D_GNU_SOURCE -D_BUILD_UNIX
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


# version number
VERSION = -DVER_MAJOR=$(VER_MAJOR) \
	-DVER_MINOR=$(VER_MINOR) \
	-DVER_PATCH=$(VER_PATCH)
VERSION += -DVERSION=\"$(VER_MAJOR).$(VER_MINOR).$(VER_PATCH)\"
CFLAGS += $(VERSION)
CXXFLAGS += $(VERSION)


# built-in icon support
ifeq ($(BUILTIN_ICON), yes)
  CFLAGS += -DBUILTIN_ICON
  CXXFLAGS += -DBUILTIN_ICON
endif


# ACK/NOACK support
ifeq ($(SEND_NOACK), yes)
  CFLAGS += -DNET_SEND_NOACK
  CXXFLAGS += -DNET_SEND_NOACK
endif
ifeq ($(DUMMY_ACK), yes)
  CFLAGS += -DNET_DUMMY_ACK
  CXXFLAGS += -DNET_DUMMY_ACK
endif
ifeq ($(QUICK_ACK), yes)
  CFLAGS += -DNET_QUICK_ACK
  CXXFLAGS += -DNET_QUICK_ACK
endif


# compressed/uncompressed data transfer support
ifeq ($(COMPRESS_DATA), yes)
  CFLAGS += -DCOMPRESS_DATA
  CXXFLAGS += -DCOMPRESS_DATA
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
ifeq ($(RELEASE), yes)
	@upx -9 $@
endif


%.o : %.c
	@echo -e "\tCC  $<"
	@$(CC) -c $(CFLAGS) -o $@ $<


%.d : %.c
	@echo -e "\tDEP $<"
	@$(CC) -MM $(CFLAGS) $< > $@


%.o : %.cpp
	@echo -e "\tC++ $<"
	@$(CXX) -c $(CXXFLAGS) -o $@ $<


%.d : %.cpp
	@echo -e "\tDEP $<"
	@$(CXX) -MM $(CXXFLAGS) $< > $@


ifneq ($(MAKECMDGOAL),rmdeps)
  include $(DEPENDS)
endif
