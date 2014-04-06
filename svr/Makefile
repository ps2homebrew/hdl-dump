EE_BIN = IOP_PKTDRV.elf

IRX_PATH = $(PS2SDK)/iop/irx
PKTDRV_PATH = ./pktdrv

IOP_OBJS = IOMANX_irx.o FILEXIO_irx.o POWEROFF_irx.o DEV9_irx.o ATAD_irx.o HDD_irx.o SMAP_irx.o NETMAN_irx.o PKTDRV_irx.o
EE_OBJS = main.o ipconfig.o $(IOP_OBJS)

EE_INCS := -I$(PS2SDK)/sbv/include
EE_LDFLAGS := -L$(PS2SDK)/sbv/lib -s
EE_LIBS = -lpatches -ldebug -lfileXio -lnetman
EE_CFLAGS += -mgpopt -G2

all:
	$(MAKE) -C $(PKTDRV_PATH)
	$(MAKE) $(EE_BIN)

clean:
	$(MAKE) -C $(PKTDRV_PATH) clean
	rm -f $(EE_BIN) $(EE_OBJS) *_irx.c

IOMANX_irx.c: $(IRX_PATH)/iomanX.irx
	bin2c $(IRX_PATH)/iomanX.irx IOMANX_irx.c IOMANX_irx

FILEXIO_irx.c: $(IRX_PATH)/fileXio.irx
	bin2c $(IRX_PATH)/fileXio.irx FILEXIO_irx.c FILEXIO_irx

POWEROFF_irx.c: $(IRX_PATH)/poweroff.irx
	bin2c $(IRX_PATH)/poweroff.irx POWEROFF_irx.c POWEROFF_irx

DEV9_irx.c: $(IRX_PATH)/ps2dev9.irx
	bin2c $(IRX_PATH)/ps2dev9.irx DEV9_irx.c DEV9_irx

ATAD_irx.c: $(IRX_PATH)/ps2atad.irx
	bin2c $(IRX_PATH)/ps2atad.irx ATAD_irx.c ATAD_irx

HDD_irx.c: $(IRX_PATH)/ps2hdd.irx
	bin2c $(IRX_PATH)/ps2hdd.irx HDD_irx.c HDD_irx

SMAP_irx.c: $(IRX_PATH)/smap.irx
	bin2c $(IRX_PATH)/smap.irx SMAP_irx.c SMAP_irx

NETMAN_irx.c: $(IRX_PATH)/netman.irx
	bin2c $(IRX_PATH)/netman.irx NETMAN_irx.c NETMAN_irx

PKTDRV_irx.c: $(PKTDRV_PATH)/pktdrv.irx
	bin2c $(PKTDRV_PATH)/pktdrv.irx PKTDRV_irx.c PKTDRV_irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
