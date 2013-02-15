##
## svr/iop/Makefile
## $Id: Makefile,v 1.4 2004/12/04 10:28:45 b081 Exp $
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

IOP_BIN = hdld_svr.irx

IOP_OBJS = hdld_svr_iop.o hio_iop.o imports.o net_server.o net_io.o byteseq.o

IOP_LIBS =

IOP_INCS += -I../../

IOP_CFLAGS += -Wall -D_BUILD_PS2

all: $(IOP_BIN) copy_irx

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

net_server.o: ../net_server.c
	@echo -e "\tIOP-CC $<"
	@$(IOP_CC) $(IOP_CFLAGS) $< -o $@

net_io.o: ../../net_io.c
	@echo -e "\tIOP-CC $<"
	@$(IOP_CC) $(IOP_CFLAGS) $< -o $@

byteseq.o: ../../byteseq.c
	@echo -e "\tIOP-CC $<"
	@$(IOP_CC) $(IOP_CFLAGS) $< -o $@

copy_irx: $(IOP_BIN)
	@cp $(IOP_BIN) ../irx/

include $(PS2SDK)/Defs.make
include $(PS2SDK)/iop-rules.make
