# -*- makefile -*-
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004.
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

# Include directories
EE_INCS := \
	-I$(PS2SDK)/ee/include \
	-I$(PS2SDK)/common/include \
	-I$(PS2DEV)/sbv-1.0-lite/include \
	$(EE_INCS)

# C compiler flags
EE_CFLAGS := -D_EE -O2 -G0 -Wall $(EE_CFLAGS)

# C++ compiler flags
EE_CXXFLAGS := -D_EE -O2 -G0 -Wall $(EE_CXXFLAGS)

# Linker flags
EE_LDFLAGS := \
	-L$(PS2SDK)/ee/lib \
	-L$(PS2DEV)/sbv-1.0-lite/lib \
	$(EE_LDFLAGS)

# Assembler flags
EE_ASFLAGS += -G0

# Link with following libraries.  This is a special case, and instead of
# allowing the user to override the library order, we always make sure
# libkernel is the last library to be linked.
EE_LIBS += -lkernel -lc -lsyscall

# Externally defined variables: EE_BIN, EE_OBJS, EE_LIB

# These macros can be used to simplify certain build rules.
EE_C_COMPILE = $(EE_CC) $(EE_CFLAGS) $(EE_INCS)
EE_CXX_COMPILE = $(EE_CC) $(EE_CXXFLAGS) $(EE_INCS)

%.o : %.c
	@echo -e "\tEE-CC  $<"
	@$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o : %.cpp
	@echo -e "\tEE-CXX $<"
	@$(EE_CXX) $(EE_CXXFLAGS) $(EE_INCS) -c $< -o $@

%.o : %.S
	@echo -e "\tEE-CC  $<"
	@$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o : %.s
	@echo -e "\tEE-AS  $<"
	@$(EE_AS) $(EE_ASFLAGS) $< -o $@

$(EE_BIN) : $(EE_OBJS) $(PS2SDK)/ee/startup/crt0.o
	@echo -e "\tEE-LNK $@"
	@$(EE_CC) -nostartfiles -T$(PS2SDK)/ee/startup/linkfile $(EE_LDFLAGS) \
		-o $(EE_BIN) $(PS2SDK)/ee/startup/crt0.o $(EE_OBJS) $(EE_LIBS)

$(EE_LIB) : $(EE_OBJS)
	@echo -e "\tEE-LIB $@"
	@$(EE_AR) cru $(EE_LIB) $(EE_OBJS)
