.SILENT:

EE_BIN = diskload.elf
EE_SRC_DIR = src/
EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/

FRONTEND_OBJS = system.o bootgame.o hdd.o ioprp.o

EECORE_OBJS =	ee_core.o IOPRP_img.o udnl.o \
				imgdrv.o eesync.o \
				hdd_cdvdman.o cdvdfsv.o \
				ps2dev9.o ps2atad.o poweroff.o ps2hdd.o \
				iomanx.o filexio.o \

EE_OBJS = $(FRONTEND_OBJS) $(GFX_OBJS) $(EECORE_OBJS)
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

EE_LIBS = -L$(PS2SDK)/ports/lib -L$(GSKIT)/lib -lpoweroff -lfileXio -lpatches -lmc -lcdvd
EE_INCS += -I$(PS2SDK)/ports/include
EE_CFLAGS := -O2 -G8192 -mgpopt

all:
	@mkdir -p obj
	@mkdir -p asm

	echo "Building Open PS2 Loader..."
	echo "    * Interface"
	$(MAKE) $(EE_BIN)
	
	echo "Stripping..."
	ee-strip $(EE_BIN)

clean:  sclean

sclean:
	echo "Cleaning..."
	echo "    * Interface"
	rm -f -r $(EE_BIN) $(EE_ASM_DIR) $(EE_OBJS_DIR)
	echo "    * EE core"
	$(MAKE) -C ee_core clean
	echo "    * udnl.irx"
	$(MAKE) -C modules/iopcore/udnl clean
	echo "    * imgdrv.irx"
	$(MAKE) -C modules/iopcore/imgdrv clean
	echo "    * eesync.irx"
	$(MAKE) -C modules/iopcore/eesync clean
	echo "    * cdvdman.irx"
	$(MAKE) -C modules/iopcore/cdvdman -f Makefile.hdd clean
	echo "    * cdvdfsv.irx"
	$(MAKE) -C modules/iopcore/cdvdfsv clean
	echo "    * ps2dev9.irx"
	$(MAKE) -C modules/dev9 clean
	echo "    * ps2hdd.irx"
	$(MAKE) -C modules/hdd/apa clean
	echo "    * ps2atad.irx"
	$(MAKE) -C modules/hdd/atad clean

rebuild: clean all

ee_core.s:
	echo "    * EE core"
	$(MAKE) -C ee_core clean
	$(MAKE) $(VMC_FLAGS) $(EECORE_DEBUG_FLAGS) -C ee_core
	bin2s ee_core/ee_core.elf $(EE_ASM_DIR)ee_core.s eecore_elf

udnl.s:
	echo "    * udnl.irx"
	$(MAKE) $(UDNL_CFLAGS) -C modules/iopcore/udnl
	bin2s modules/iopcore/udnl/udnl.irx $(EE_ASM_DIR)udnl.s udnl_irx

IOPRP_img.s:
	bin2s modules/iopcore/IOPRP.img $(EE_ASM_DIR)IOPRP_img.s IOPRP_img

imgdrv.s:
	echo "    * imgdrv.irx"
	$(MAKE) -C modules/iopcore/imgdrv
	bin2s modules/iopcore/imgdrv/imgdrv.irx $(EE_ASM_DIR)imgdrv.s imgdrv_irx

eesync.s:
	echo "    * eesync.irx"
	$(MAKE) -C modules/iopcore/eesync
	bin2s modules/iopcore/eesync/eesync.irx $(EE_ASM_DIR)eesync.s eesync_irx

hdd_cdvdman.s:
	echo "    * hdd_cdvdman.irx"
	$(MAKE) $(VMC_FLAGS) $(CDVDMAN_DEBUG_FLAGS) -C modules/iopcore/cdvdman -f Makefile.hdd rebuild
	bin2s modules/iopcore/cdvdman/cdvdman.irx $(EE_ASM_DIR)hdd_cdvdman.s hdd_cdvdman_irx

cdvdfsv.s:
	echo "    * cdvdfsv.irx"
	$(MAKE) -C modules/iopcore/cdvdfsv
	bin2s modules/iopcore/cdvdfsv/cdvdfsv.irx $(EE_ASM_DIR)cdvdfsv.s cdvdfsv_irx

ps2dev9.s:
	echo "    * ps2dev9.irx"
	$(MAKE) -C modules/dev9
	bin2s modules/dev9/ps2dev9.irx $(EE_ASM_DIR)ps2dev9.s ps2dev9_irx

ps2atad.s:
	echo "    * ps2atad.irx"
	$(MAKE) -C modules/hdd/atad
	bin2s modules/hdd/atad/ps2atad.irx $(EE_ASM_DIR)ps2atad.s ps2atad_irx

poweroff.s:
	bin2s $(PS2SDK)/iop/irx/poweroff.irx $(EE_ASM_DIR)poweroff.s poweroff_irx

ps2hdd.s:
	echo "    * ps2hdd.irx"
	$(MAKE) -C modules/hdd/apa
	bin2s modules/hdd/apa/ps2hdd.irx $(EE_ASM_DIR)ps2hdd.s ps2hdd_irx

iomanx.s:
	bin2s $(PS2SDK)/iop/irx/iomanX.irx $(EE_ASM_DIR)iomanx.s iomanx_irx

filexio.s:
	bin2s $(PS2SDK)/iop/irx/fileXio.irx $(EE_ASM_DIR)filexio.s filexio_irx

$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o : %.s
	$(EE_AS) $(EE_ASFLAGS) $(EE_ASM_DIR)$< -o $@


include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
