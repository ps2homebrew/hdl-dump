#include <tamtypes.h>

#include "include/system.h"
#include "include/opl.h"
#include "include/hddsupport.h"
#include "modules/iopcore/common/cdvd_config.h"

extern unsigned char hdd_cdvdman_irx[];
extern unsigned int size_hdd_cdvdman_irx;

extern unsigned char cdvdfsv_irx[];
extern unsigned int size_cdvdfsv_irx;

extern unsigned char poweroff_irx[];
extern unsigned int size_poweroff_irx;

extern unsigned char ps2dev9_irx[];
extern unsigned int size_ps2dev9_irx;

extern unsigned char ps2atad_irx[];
extern unsigned int size_ps2atad_irx;

extern unsigned char ps2hdd_irx[];
extern unsigned int size_ps2hdd_irx;

extern unsigned char iomanx_irx[];
extern unsigned int size_iomanx_irx;

extern unsigned char filexio_irx[];
extern unsigned int size_filexio_irx;

int hddGetHDLGameInfo(const char *Partition, hdl_game_info_t *ginfo);

static inline const char *GetMountParams(const char *command, char *BlockDevice)
{
    const char *MountPath;
    int BlockDeviceNameLen;

    if ((MountPath = strchr(&command[5], ':')) != NULL) {
        BlockDeviceNameLen = (unsigned int)MountPath - (unsigned int)command;
        strncpy(BlockDevice, command, BlockDeviceNameLen);
        BlockDevice[BlockDeviceNameLen] = '\0';

        MountPath++; //This is the location of the mount path;
    }

    return MountPath;
}

static int ConfigureOPL(hdl_game_info_t *game, int size_cdvdman, void *cdvdman_irx)
{
    int i;
    unsigned int compatmask;
    static const struct cdvdman_settings_common cdvdman_settings_common_sample = {
        0x69, 0x69,
        0x1234,
        0x39393939,
        "B00BS"};
    struct cdvdman_settings_hdd *settings;

    for (i = 0, settings = NULL; i < size_cdvdman; i += 4) {
        if (!memcmp((void *)((u8 *)cdvdman_irx + i), &cdvdman_settings_common_sample, sizeof(cdvdman_settings_common_sample))) {
            settings = (struct cdvdman_settings_hdd *)((u8 *)cdvdman_irx + i);
            break;
        }
    }
    if (settings == NULL)
        return -1; //Internal error - should not happen

    settings->common.flags = 0;
    compatmask = game->ops2l_compat_flags;

    if (compatmask & COMPAT_MODE_1) {
        settings->common.flags |= IOPCORE_COMPAT_ACCU_READS;
    }

    if (compatmask & COMPAT_MODE_2) {
        settings->common.flags |= IOPCORE_COMPAT_ALT_READ;
    }

    if (compatmask & COMPAT_MODE_4) {
        settings->common.flags |= IOPCORE_COMPAT_0_PSS;
    }

    if (compatmask & COMPAT_MODE_5) {
        settings->common.flags |= IOPCORE_COMPAT_EMU_DVDDL;
    }

    if (compatmask & COMPAT_MODE_6) {
        settings->common.flags |= IOPCORE_ENABLE_POFF;
    }

    settings->common.media = hddIs48bit() & 0xff;

    // patch start_sector
    settings->lba_start = game->start_sector;

    return 0;
}

int main(int argc, char *argv[])
{
    char PartitionName[33], BlockDevice[38];
    int result;

    hdl_game_info_t GameInfo;

    SifInitRpc(0);

    /* Do as many things as possible while the IOP slowly resets itself. */
    if (argc == 2) { /* Argument 1 will contain the name of the partition containing the game. */
        /*	Unfortunately, it'll mean that some homebrew loader was most likely used to launch this program... and it might already have IOMANX loaded. That thing can't register devices that are added via IOMAN after it gets loaded.
			Reset the IOP to clear out all these stupid homebrew modules... */
        while (!SifIopReset(NULL, 0)) {
        };

        if (strlen(argv[1]) <= 32) {
            strncpy(PartitionName, argv[1], sizeof(PartitionName) - 1);
            PartitionName[sizeof(PartitionName) - 1] = '\0';
            result = 0;
        } else
            result = -1;

        while (!SifIopSync()) {
        };

        SifInitRpc(0);

        if (result < 0)
            goto BootError;
    } else {
        if (GetMountParams(argv[0], BlockDevice) != NULL) {
            strncpy(PartitionName, &BlockDevice[5], sizeof(PartitionName) - 1);
            PartitionName[sizeof(PartitionName) - 1] = '\0';
        } else
            goto BootError;
    }

    SifInitIopHeap();
    SifLoadFileInit();

    sbv_patch_enable_lmb();

    SifExecModuleBuffer(poweroff_irx, size_poweroff_irx, 0, NULL, NULL);
    SifExecModuleBuffer(ps2dev9_irx, size_ps2dev9_irx, 0, NULL, NULL);

    SifExecModuleBuffer(iomanx_irx, size_iomanx_irx, 0, NULL, NULL);
    SifExecModuleBuffer(filexio_irx, size_filexio_irx, 0, NULL, NULL);

    fileXioInit();

    SifExecModuleBuffer(ps2atad_irx, size_ps2atad_irx, 0, NULL, NULL);
    SifExecModuleBuffer(ps2hdd_irx, size_ps2hdd_irx, 0, NULL, NULL);
    hddSetIdleTimeout(0); // gHDDSpindown [0..20] -> spindown [0..240] -> seconds [0..1200] */

    SifLoadFileExit();
    SifExitIopHeap();

    DPRINTF("Retrieving game information...\n");

    if ((result = hddGetHDLGameInfo(PartitionName, &GameInfo)) < 0)
        PartitionName[1] = 'C'; // Checks if PP. is HDL and if not, convert target name from "PP." to "PC." (Child partition)

    if ((result = hddGetHDLGameInfo(PartitionName, &GameInfo)) >= 0) {
        DPRINTF("Partition name: %s \nTitle: %s \nStartup: %s\n", PartitionName, GameInfo.name, GameInfo.startup);

        DPRINTF("Configuring core...\n");
        gDisableDebug = 1;
        gExitPath[0] = '\0';

        gHDDSpindown = 0;

        hddSetTransferMode(GameInfo.dma_type, GameInfo.dma_mode);
        hddSetIdleTimeout(0);

        if ((result = ConfigureOPL(&GameInfo, size_hdd_cdvdman_irx, hdd_cdvdman_irx)) == 0) {
            DPRINTF("Launching game...\n");

            sysLaunchLoaderElf(GameInfo.start_sector, GameInfo.startup, "HDD_MODE", size_hdd_cdvdman_irx, hdd_cdvdman_irx, GameInfo.ops2l_compat_flags);
            result = -1;
        }
    }

    DPRINTF("Error loading game: %s, code: %d\n", PartitionName, result);

BootError:
    SifExitRpc();

    char *args[2];
    args[0] = "BootError";
    args[1] = NULL;
    ExecOSD(1, args);

    return 0;
}
