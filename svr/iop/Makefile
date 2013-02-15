IOP_BIN = hdld_svr.irx
IOP_OBJS = hdld_svr_iop.o hio_iop.o imports.o net_server.o net_io.o
IOP_LIBS =
IOP_INCS += -I../../
IOP_CFLAGS += -D_BUILD_PS2 -O2

all: $(IOP_BIN) copy_irx

clean:
	rm -f *.irx *.o *.a *.s

net_server.o: ../net_server.c
	$(IOP_CC) $(IOP_CFLAGS) $< -o $@

net_io.o: ../../net_io.c
	$(IOP_CC) $(IOP_CFLAGS) $< -o $@

copy_irx: $(IOP_BIN)
	cp $(IOP_BIN) ../irx/

include $(PS2SDK)/Defs.make
include $(PS2SDK)/iop-rules.make
