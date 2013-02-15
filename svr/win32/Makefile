##
## svr/win32/Makefile
## $Id: Makefile,v 1.5 2004/12/04 10:28:47 b081 Exp $
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
##

CFLAGS = -Wall -ansi -pedantic -Wno-long-long -O0 -g

LDFLAGS =

OBJECTS = hdld_svr_win32.o ../net_server.o ../../hio_probe.o \
	../../hio_win32.o ../../hio_net.o \
	../../net_io.o ../../apa.o ../../common.o ../../progress.o ../../byteseq.o

# "autodetect" Windows builds
ifdef SYSTEMROOT
  WINDOWS = yes
else
  WINDOWS = no
endif

# Windows/Unix/Linux build
ifeq ($(WINDOWS), yes)
  OBJECTS += ../../osal_win32.o
  CFLAGS += -mno-cygwin -D_BUILD_WIN32
  LDFLAGS += -lwsock32
  EXESUF = .exe

  # make it compile with latest cygwin/mingw
  # however, that would probably not work under older versions of Windows
  CFLAGS += -D_WIN32_WINNT=0x0500
else
  OBJECTS += ../../osal_unix.o
  CFLAGS += -D_BUILD_UNIX -D_GNU_SOURCE
  EXESUF =
endif

BINARY = hdld_svr$(EXESUF)

all: $(BINARY)

clean:
	rm -f $(BINARY) $(OBJECTS)

$(BINARY): $(OBJECTS)
	@echo -e "\tLNK $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

define COMP
$(CC) $(CFLAGS) -c -o $@ $^
endef

%.o : %.c
	@echo -e "\tCC  $<"
	@$(CC) $(CFLAGS) -c -o $@ $^
