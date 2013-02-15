##
## svr/win32/Makefile
## $Id: Makefile,v 1.2 2004/08/15 16:50:17 b081 Exp $
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

CFLAGS = -Wall -ansi -pedantic -Wno-long-long -O0 -g -D_BUILD_WIN32 -mno-cygwin
LDFLAGS = -lwsock32

OBJS = hdld_svr_win32.o ../net_server.o ../../hio_probe.o ../../hio_win32.o ../../hio_net.o \
	../../osal_win32.o ../../net_io.o ../../apa.o ../../common.o ../../progress.o
BIN_OUT = hdld_svr.exe

all: $(BIN_OUT)

clean:
	rm -f *.o $(BIN_OUT)

$(BIN_OUT): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

define COMP
$(CC) $(CFLAGS) -c -o $@ $^
endef

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $^

include ../../.depends
