IOP_BIN = pktdrv.irx
IOP_OBJS = main.o arp.o eth.o icmp.o ip.o nic.o udp.o svr.o ipconfig.o imports.o

IOP_INCS += -I$(PS2SDK)/iop/include
#IOP_CFLAGS += -Wall -fno-builtin -D_IOP
IOP_CFLAGS += -Wall -fno-builtin -D_IOP -mgpopt -G8192
IOP_LDFLAGS += -s

all: $(IOP_BIN)

clean:
	rm -f $(IOP_OBJS) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
#include Rules.make
