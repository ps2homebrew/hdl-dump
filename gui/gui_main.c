/*
 * gui_main.c
 * $Id: gui_main.c,v 1.11 2007-05-12 20:18:17 bobi Exp $
 *
 * Copyright 2004 Bobi B., w1zard0f07@yahoo.com
 *
 * This file is part of hdl_dumb.
 *
 * hdl_dumb is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * hdl_dumb is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with hdl_dumb; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define _WIN32_IE 0x0400

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "rsrc.h"

#include "../progress.h"
#include "../osal.h"
#include "../retcodes.h"
#include "../apa.h"
#include "../hdl.h"
#include "../isofs.h"
#include "../iin.h"
#include "../hio.h"
#include "../dict.h"
#include "../common.h"


/*
 * I'm not too proud with this source, but it is supposed to run fine :-)
 */

/* whether to use SPTI or old-style optical drives access */
#define USE_SPTI


#if defined(_DEBUG)
#define DEFAULT_IP MAKEIPADDRESS(127, 0, 0, 1)
#else
#define DEFAULT_IP MAKEIPADDRESS(192, 168, 0, 10)
#endif


static HINSTANCE inst;
static osal_dlist_t *hard_drives_ = NULL;
#if !defined(USE_SPTI)
static osal_dlist_t *optical_drives_ = NULL;
#endif
static HWND progress_dlg;
static int interrupted;

static HIMAGELIST iml = NULL;

static hdl_games_list_t *games_ = NULL;
static hio_t *hio_ = NULL;
static dict_t *config_ = NULL;

static const DWORD MODE_IDC[] =
    {
        IDC_MODE1, IDC_MODE2, IDC_MODE3, IDC_MODE4,
        IDC_MODE5, IDC_MODE6, IDC_MODE7, IDC_MODE8};

static const char *device_name = NULL;


/**************************************************************/
/* two static buffers are to be used concurrently */
static char *
get_string(int id, int buffer_id)
{
#define MAX_MESSAGE_LEN 2048
    static char buffer[2][MAX_MESSAGE_LEN];
    if (buffer_id < 0)
        buffer_id = 0;
    if (buffer_id > 1)
        buffer_id = 1;
    if (LoadString(inst, id, buffer[buffer_id], MAX_MESSAGE_LEN) == 0)
        sprintf(buffer[buffer_id], "String resource #%d not found.", id);
    return (buffer[buffer_id]);
}


/**************************************************************/
static void
show_error(HWND parent)
{
    char *error = osal_get_last_error_msg();
    if (error != NULL) { /* OS error */
        MessageBox(parent, error,
                   get_string(IDS_ERROR_TITLE, 0),
                   MB_OK | MB_ICONSTOP);
        osal_dispose_error_msg(error);
    } else
        MessageBox(parent, get_string(IDS_UNKNOWN_ERR, 0),
                   get_string(IDS_ERROR_TITLE, 1),
                   MB_OK | MB_ICONSTOP);
}


/**************************************************************/
static void
show_error_2(HWND parent,
             int result)
{
    int id;
    switch (result) { /* TODO: revise missing codes; add SPTI errors */
        case RET_NOT_APA:
            id = IDS_NOT_APA_ERR;
            break;
        case RET_NOT_HDL_PART:
            id = IDS_NOT_HDL_PART_ERR;
            break;
        case RET_NOT_FOUND:
            id = IDS_NOT_FOUND_ERR;
            break;
        case RET_BAD_FORMAT:
            id = IDS_BAD_FORMAT_ERR;
            break;
        case RET_BAD_DEVICE:
            id = IDS_BAD_DEVICE_ERR;
            break;
        case RET_NO_SPACE:
            id = IDS_NO_SPACE_ERR;
            break;
        case RET_BAD_APA:
            id = IDS_BAD_APA_ERR;
            break;
        case RET_NO_MEM:
            id = IDS_NO_MEM_ERR;
            break;
        case RET_PART_EXISTS:
            id = IDS_PART_EXISTS_ERR;
            break;
        case RET_NOT_COMPAT:
            id = IDS_NOT_COMPAT_ERR;
            break;
        case RET_NOT_ALLOWED:
            id = IDS_NOT_ALLOWED_ERR;
            break;
        case RET_CROSS_128GB:
            id = IDS_DATA_CROSS_128GB;
            break;
        case RET_ERR:
            show_error(parent);
            return; /* OS error */
        default:
            id = IDS_UNKNOWN_ERR;
            break;
    }
    MessageBox(parent, get_string(id, 0),
               get_string(IDS_ERROR_TITLE, 1),
               MB_OK | MB_ICONSTOP);
}


/**************************************************************/
static char *
dlg_get_target(HWND dlg, char target[MAX_PATH])
{
    if (IsDlgButtonChecked(dlg, IDC_PS2HDD_LOCAL) == BST_CHECKED) { /* locally connected HDD */
        HWND cbo = GetDlgItem(dlg, IDC_PS2HDD);
        int index = SendMessage(cbo, CB_GETCURSEL, 0, 0);
        SendMessage(cbo, CB_GETLBTEXT, index, (LPARAM)target);
    } else { /* networking server */
        DWORD ip;
        SendMessage(GetDlgItem(dlg, IDC_PS2IP),
                    IPM_GETADDRESS, 0, (LPARAM)(void *)&ip);
        sprintf(target, "%u.%u.%u.%u",
                (unsigned int)((ip >> 24) & 0xff),
                (unsigned int)((ip >> 16) & 0xff),
                (unsigned int)((ip >> 8) & 0xff),
                (unsigned int)((ip >> 0) & 0xff));
    }
    return (target);
}


/**************************************************************/
static char *
dlg_get_source(HWND dlg, char source[MAX_PATH])
{
    if (IsDlgButtonChecked(dlg, IDC_ISO_OPT) == BST_CHECKED)
        GetWindowText(GetDlgItem(dlg, IDC_ISO_PATH), source, MAX_PATH);
    else { /* if there are no optical drives radio button would be disabled */
        HWND cbo = GetDlgItem(dlg, IDC_SOURCE_DRIVE);
        int index = SendMessage(cbo, CB_GETCURSEL, 0, 0);
        SendMessage(cbo, CB_GETLBTEXT, index, (LPARAM)source);
    }
    return (source);
}


/**************************************************************/
static void
dlg_init_contents_list(HWND dlg)
{
    HWND lvw = GetDlgItem(dlg, IDC_CONTENTS);
    LVCOLUMN col;
    RECT rc;
    int total_w, name_pc, flags_pc, size_pc;

    /* set-up icons for the games list */
    iml = ImageList_Create(15, 15, ILC_COLOR4 | ILC_MASK, 4, 4);
    if (iml != NULL) {
        HBITMAP bmp;
        const COLORREF mask = RGB(255, 0, 255);
        bmp = LoadBitmap(inst, MAKEINTRESOURCE(IDB_PS2_CD));
        if (bmp != NULL)
            ImageList_AddMasked(iml, bmp, mask);
        bmp = LoadBitmap(inst, MAKEINTRESOURCE(IDB_PS2_DVD));
        if (bmp != NULL)
            ImageList_AddMasked(iml, bmp, mask);
        bmp = LoadBitmap(inst, MAKEINTRESOURCE(IDB_PS2_DVD9));
        if (bmp != NULL)
            ImageList_AddMasked(iml, bmp, mask);

        ListView_SetImageList(lvw, iml, LVSIL_SMALL);
    }

    ListView_SetExtendedListViewStyle(lvw,
                                      ListView_GetExtendedListViewStyle(lvw) |
                                          LVS_EX_FULLROWSELECT |
                                          LVS_EX_GRIDLINES);

    GetWindowRect(lvw, &rc);
    total_w = rc.right - rc.left - 25; /* 25 is approx. scroll width */

    memset(&col, 0, sizeof(LVCOLUMN));
    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;

    flags_pc = 10 + 2 * MAX_FLAGS;
    size_pc = 20;
    name_pc = 100 - flags_pc - size_pc;

    /* partition name */
    col.fmt = LVCFMT_LEFT;
    col.cx = (total_w * name_pc) / 100;
    col.pszText = get_string(IDS_NAME_LBL, 0);
    ListView_InsertColumn(lvw, 0, &col);

    /* flags */
    col.fmt = LVCFMT_RIGHT;
    col.cx = (total_w * flags_pc) / 100;
    col.pszText = get_string(IDS_FLAGS_LBL, 0);
    ListView_InsertColumn(lvw, 1, &col);

    /* size */
    col.fmt = LVCFMT_RIGHT;
    col.cx = (total_w * size_pc) / 100;
    col.pszText = get_string(IDS_SIZE_LBL, 0);
    ListView_InsertColumn(lvw, 2, &col);
}


/**************************************************************/
static void
dlg_switch_operation(HWND dlg)
{
    BOOL inject_mode = IsDlgButtonChecked(dlg, IDC_INSTALL_OPT) == BST_CHECKED;

    int inject_show = !inject_mode ? SW_HIDE : SW_SHOW;
    int examine_show = inject_mode ? SW_HIDE : SW_SHOW;
    size_t i;

    ShowWindow(GetDlgItem(dlg, IDC_SOURCE_LBL), inject_show);

    ShowWindow(GetDlgItem(dlg, IDC_ISO_OPT), inject_show);
    ShowWindow(GetDlgItem(dlg, IDC_ISO_PATH), inject_show);
    ShowWindow(GetDlgItem(dlg, IDC_BROWSE_FOR_ISO), inject_show);

    ShowWindow(GetDlgItem(dlg, IDC_OPTICAL_OPT), inject_show);
    ShowWindow(GetDlgItem(dlg, IDC_SOURCE_DRIVE), inject_show);

    ShowWindow(GetDlgItem(dlg, IDC_TYPE_LBL), inject_show);
    ShowWindow(GetDlgItem(dlg, IDC_SOURCE_TYPE), inject_show);
    ShowWindow(GetDlgItem(dlg, IDC_SOURCE_INFO), inject_show);

    ShowWindow(GetDlgItem(dlg, IDC_GAME_LBL), inject_show);

    ShowWindow(GetDlgItem(dlg, IDC_GAMENAME_LBL), inject_show);
    ShowWindow(GetDlgItem(dlg, IDC_GAMENAME), inject_show);

    ShowWindow(GetDlgItem(dlg, IDC_SIGNATURE_LBL), inject_show);
    ShowWindow(GetDlgItem(dlg, IDC_SIGNATURE), inject_show);

    ShowWindow(GetDlgItem(dlg, IDC_FLAGS_LBL), inject_show);
    for (i = 0; i < MAX_FLAGS; ++i)
        ShowWindow(GetDlgItem(dlg, MODE_IDC[i]), inject_show && MAX_FLAGS > i);

    ShowWindow(GetDlgItem(dlg, IDC_DMA_LBL), inject_show);
    ShowWindow(GetDlgItem(dlg, IDC_DMA_TYPE), inject_show);
#if 0
    SetWindowText (GetDlgItem (dlg, IDC_ACTION),
    get_string (inject_show ? IDS_INSTALL_LBL : IDS_DELETE_LBL, 0));
#endif
    ShowWindow(GetDlgItem(dlg, IDC_CONTENTS), examine_show);
}


/**************************************************************/
static void
dlg_switch_target(HWND dlg)
{
    BOOL local_mode = IsDlgButtonChecked(dlg, IDC_PS2HDD_LOCAL) == BST_CHECKED ? TRUE : FALSE;
    EnableWindow(GetDlgItem(dlg, IDC_PS2HDD), local_mode);
    EnableWindow(GetDlgItem(dlg, IDC_PS2IP), !local_mode);
    EnableWindow(GetDlgItem(dlg, IDC_NET_U2LINK), !local_mode);
}


/**************************************************************/
static void
dlg_switch_source(HWND dlg)
{
    BOOL iso_mode = IsDlgButtonChecked(dlg, IDC_ISO_OPT) == BST_CHECKED ? TRUE : FALSE;
    EnableWindow(GetDlgItem(dlg, IDC_ISO_PATH), iso_mode);
    EnableWindow(GetDlgItem(dlg, IDC_BROWSE_FOR_ISO), iso_mode);
    EnableWindow(GetDlgItem(dlg, IDC_SOURCE_DRIVE), !iso_mode);
}


/**************************************************************/
int fill_ps2hdd_combo(HWND dlg)
{
    HWND cbo = GetDlgItem(dlg, IDC_PS2HDD);
    int result;
    osal_dlist_free(hard_drives_);

    if (device_name == NULL) {
        result = osal_query_hard_drives(&hard_drives_);
        if (result == RET_OK) {
            int count = 0;
            size_t i;
            SendMessage(cbo, CB_RESETCONTENT, 0, 0);
            for (i = 0; i < hard_drives_->used; ++i) {
                const osal_dev_t *dev = hard_drives_->device + i;
                if (dev->is_ps2 == RET_OK) {
                    SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)dev->name);
                    ++count;
                }
            }
            if (count > 0) /* select the first one by default */
                SendMessage(cbo, CB_SETCURSEL, 0, 0);
            return (count > 0 ? RET_OK : RET_NOT_FOUND);
        } else
            return (result);
    } else { /* get device name from the command-line (dbg:xxx maybe) */
        SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)device_name);
        SendMessage(cbo, CB_SETCURSEL, 0, 0);
        return (RET_OK);
    }
}


/**************************************************************/
int fill_opticals_combo(HWND dlg)
{
    HWND cbo = GetDlgItem(dlg, IDC_SOURCE_DRIVE);
#if !defined(USE_SPTI) /* old-style opticals ("cd0:", "cd1:",...) */
    int result;
    osal_dlist_free(optical_drives_);
    result = osal_query_optical_drives(&optical_drives_);
    if (result == RET_OK) {
        int count = 0;
        size_t i;
        SendMessage(cbo, CB_RESETCONTENT, 0, 0);
        for (i = 0; i < optical_drives_->used; ++i) {
            const osal_dev_t *dev = optical_drives_->device + i;
            SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)dev->name);
            ++count;
        }
        if (count > 0) /* select the first one by default */
            SendMessage(cbo, CB_SETCURSEL, 0, 0);
        return (count > 0 ? RET_OK : RET_NOT_FOUND);
    } else
        return (result);

#else
    char path[4] = {'?', ':', '\\'};
    char iin_path[3] = {'?', ':'};
    size_t count = 0;

    SendMessage(cbo, CB_RESETCONTENT, 0, 0);
    for (path[0] = 'c'; path[0] <= 'z'; ++path[0]) {
        if (GetDriveType(path) == DRIVE_CDROM) {
            iin_path[0] = path[0];
            SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)iin_path);
            ++count;
        }
    }
    fprintf(stdout, "\n");
    if (count > 0) /* select the first one by default */
        SendMessage(cbo, CB_SETCURSEL, 0, 0);
    return (count > 0 ? RET_OK : RET_NOT_FOUND);
#endif /* USE_SPTI defined? */
}


/**************************************************************/
static int
dlg_refresh_hdd_info(HWND dlg)
{
    HWND hdd_info = GetDlgItem(dlg, IDC_PS2HDD_INFO);
    HWND progress = GetDlgItem(dlg, IDC_PROGRESS);
    HWND lvw = GetDlgItem(dlg, IDC_CONTENTS);
    char hdd[MAX_PATH];
    HCURSOR old_cursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    int result;
    DWORD last_win_err = 0;

    /* clean-up */
    hdl_glist_free(games_);
    games_ = NULL;
    ListView_DeleteAllItems(lvw);
    if (hio_ != NULL)
        /* close current if any */
        hio_->close(hio_), hio_ = NULL;

    dlg_get_target(dlg, hdd);
    if (hdd[0] != '\0') {
        result = hio_probe(config_, hdd, &hio_);
        if (result == RET_OK) {
            result = hdl_glist_read(hio_, &games_);
            if (result == RET_OK) { /* show free space info */
                char details[100];
                size_t i, count = 0;
                sprintf(details, "%ld MB of %ld MB free",
                        (long)(games_->free_chunks * 128),
                        (long)(games_->total_chunks * 128));
                SendMessage(hdd_info, WM_SETTEXT, 0, (LPARAM)details);

                SendMessage(progress, PBM_SETRANGE, 0,
                            MAKELPARAM(0, games_->total_chunks));
                SendMessage(progress, PBM_SETPOS,
                            games_->total_chunks - games_->free_chunks, 0);

                /* setup games list */
                for (i = 0; i < games_->count; ++i) {
                    const hdl_game_info_t *game = &games_->games[i];
                    int index;
                    LVITEM lv;
                    size_t j;

                    lv.mask = LVIF_TEXT | LVIF_IMAGE;
                    lv.iItem = count;
                    lv.iSubItem = 0;
                    lv.pszText = (char *)game->name;
                    lv.iImage = (!game->is_dvd ? 0 :
                                                 game->raw_size_in_kb / 1024 <= 4608 ? 1 : 2);
                    index = ListView_InsertItem(lvw, &lv);

                    details[0] = details[1] = '\0';
                    for (j = 0; j < MAX_FLAGS; ++j) { /* build compatibility flags text */
                        if (game->compat_flags & (1 << j)) {
                            char tmp[10];
                            sprintf(tmp, "+%u", j + 1);
                            strcat(details, tmp);
                        }
                    }
                    if (game->dma % 256 == 32) {
                        int dma_dummy = 0;
                        dma_dummy = ((unsigned short)game->dma - 32) / 256;
                        if (dma_dummy < 3) {
                            char tmp[6];
                            sprintf(tmp, "%s", " MDMA");
                            strcat(details, tmp);
                            sprintf(tmp, "%u", (unsigned int)dma_dummy);
                            strcat(details, tmp);
                        }
                    }
                    if (game->dma % 256 == 64) {
                        int dma_dummy = 0;
                        dma_dummy = ((unsigned short)game->dma - 64) / 256;
                        if (dma_dummy < 5) {
                            char tmp[6];
                            sprintf(tmp, "%s", " UDMA");
                            strcat(details, tmp);
                            sprintf(tmp, "%u", (unsigned int)dma_dummy);
                            strcat(details, tmp);
                        }
                    }
                    ListView_SetItemText(lvw, index, 1, details + 1);

                    sprintf(details, "%lu MB",
                            (unsigned long)game->raw_size_in_kb / 1024);
                    ListView_SetItemText(lvw, index, 2, details);

                    ++count;
                } /* for */

                /* disable attach button on successful network connection and keep the IP */
                if (IsDlgButtonChecked(dlg, IDC_PS2HDD_NETWORK) == BST_CHECKED) {
                    EnableWindow(GetDlgItem(dlg, IDC_NET_U2LINK), FALSE);
                    dict_put(config_, CONFIG_LAST_IP, hdd);
                }
            } /* games list loaded? */
        }     /* hio_probe? */
        else
            last_win_err = GetLastError(); /* preserve last windows error */
    } else
        result = RET_NOT_FOUND;

    EnableWindow(GetDlgItem(dlg, IDC_ACTION), result == RET_OK);
    SetCursor(old_cursor);

    if (result == RET_ERR && last_win_err != 0)
        SetLastError(last_win_err); /* restore last error after GUI calls */

    return (result);
}


/**************************************************************/
int /* returns 1 if DVD, 0 if CD, -1 if fails */
fill_name_and_signature(HWND dlg,
                        iin_t *iin)
{
    ps2_cdvd_info_t info;
    int result = isofs_get_ps2_cdvd_info(iin, &info);
    if (result == OSAL_OK) { /* automatically fill game name and startup file */
        if (strlen(info.startup_elf) > 0) {
            compat_flags_t flags;
            char volume_id[32 + 1];
            result = ddb_lookup(config_, info.startup_elf,
                                volume_id, &flags);
            if (result == RET_OK) {
                size_t i;
                for (i = 0; i < MAX_FLAGS; ++i)
                    CheckDlgButton(dlg, MODE_IDC[i],
                                   (flags & (1 << i) ?
                                        BST_CHECKED :
                                        BST_UNCHECKED));
                strcpy(info.volume_id, volume_id);
            } else if (result == RET_DDB_INCOMPATIBLE) { /* marked as incompatible; warn */
                MessageBox(dlg, get_string(IDS_INCOMPATIBLE_GAME, 0),
                           get_string(IDS_INCOMPATIBLE_GAME_TITLE, 1),
                           MB_OK | MB_ICONWARNING);
                strcpy(info.volume_id, volume_id);
            }
        }

        if (strlen(info.volume_id) > 0)
            SetWindowText(GetDlgItem(dlg, IDC_GAMENAME), info.volume_id);
        if (strlen(info.startup_elf) > 0)
            SetWindowText(GetDlgItem(dlg, IDC_SIGNATURE), info.startup_elf);
        return (info.media_type == mt_dvd ? 1 : 0);
    }
    return (-1);
}


/**************************************************************/
void show_source_info(HWND dlg)
{
    HWND source_info = GetDlgItem(dlg, IDC_SOURCE_INFO);
    u_int64_t file_size;
    int result;
    int skip = 1, is_dvd = -1;
    char source[MAX_PATH] = {"\0"};
    HCURSOR old_cursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    dlg_get_source(dlg, source);
    if (strlen(source) > 0) {
        iin_t *iin;
        result = iin_probe(config_, source, &iin);
        if (result == OSAL_OK) {
            u_int32_t sector_size, num_sectors;
            result = iin->stat(iin, &sector_size, &num_sectors);
            if (result == OSAL_OK) {
                skip = 0;
                file_size = (u_int64_t)num_sectors * (u_int64_t)sector_size;
                is_dvd = fill_name_and_signature(dlg, iin);
            }
            iin->close(iin);
        }
    }

    if (!skip) {
        if (result == OSAL_OK) { /* display source length */
            HWND cbo = GetDlgItem(dlg, IDC_SOURCE_TYPE);
            char temp[100];
            sprintf(temp, "%ld MB, approx. %ld MB reqd.",
                    (long)(file_size / (1024 * 1024)),
                    (long)((file_size / (1024 * 1024) + 127 + 4) / 128) * 128);
            SetWindowText(source_info, temp);

            /* automagically select CD-/DVD-ROM source */
            if (is_dvd == 1 || (is_dvd == -1 && file_size / (1024 * 1024) > 800))
                SendMessage(cbo, CB_SETCURSEL, 0 /* DVD is first */, 0);
            else
                SendMessage(cbo, CB_SETCURSEL, 1 /* CD is second */, 0);
        } else {
            char *error = osal_get_last_error_msg();
            if (error != NULL) {
                SetWindowText(source_info, error);
                osal_dispose_error_msg(error);
            } else
                SetWindowText(source_info, get_string(IDS_UNKNOWN_ERR, 0));
        }
    } /* skip */

    SetCursor(old_cursor);
}


/**************************************************************/
void browse_for_iso(HWND dlg)
{
    HWND iso_textbox = GetDlgItem(dlg, IDC_ISO_PATH);
    char file_name[MAX_PATH];
    OPENFILENAME ofn;

    GetWindowText(iso_textbox, file_name, sizeof(file_name));

    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = dlg;
    ofn.hInstance = inst;
    ofn.lpstrFilter = ("All supported formats\0*.iso;*.gi;*.cue;*.iml;*.nrg\0"
                       "ISO images\0*.iso\0"
                       "CDRWIN cuesheets\0*.cue\0"
                       "Nero images\0*.nrg\0"
                       "Global images\0*.gi\0"
                       "IML files\0*.iml\0"
                       "\0");
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 0;
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = sizeof(file_name);
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = NULL;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = ".iso";
    if (GetOpenFileName(&ofn)) { /* file selected */
        SetWindowText(iso_textbox, file_name);
        show_source_info(dlg);
    }
}


/**************************************************************/
INT_PTR CALLBACK
progress_dlg_proc(HWND dlg,
                  UINT msg,
                  WPARAM wparam,
                  LPARAM lparam)
{
    switch (msg) {
        case WM_COMMAND:
            if (HIWORD(wparam) == BN_CLICKED) { /* button clicked */
                switch (LOWORD(wparam))         /* control ID */
                {
                    case IDCANCEL:
                        interrupted = 1; /* global variable */
                        return (TRUE);
                }
            }
            break;

        case WM_CLOSE:
            interrupted = 1; /* global variable */
            return (TRUE);
    }
    return (FALSE);
}


/**************************************************************/
int progress_cb(progress_t *pgs, void *data)
{
    HWND progress_ind_wnd = GetDlgItem(progress_dlg, IDC_PROGRESS);
    HWND elapsed_wnd = GetDlgItem(progress_dlg, IDC_ELAPSED);
    HWND estimated_wnd = GetDlgItem(progress_dlg, IDC_ESTIMATED);
    HWND avg_data_rate = GetDlgItem(progress_dlg, IDC_AVGSPEED);
    HWND curr_data_rate = GetDlgItem(progress_dlg, IDC_CURRSPEED);
    HWND dlg = GetParent(progress_dlg);
    MSG msg;
    int message_found;

    SendMessage(progress_ind_wnd, PBM_SETRANGE, 0, MAKELONG(0, 1000));
    SendMessage(progress_ind_wnd, PBM_SETPOS, (pgs->curr * 1000) / pgs->total, 0);
    SetWindowText(elapsed_wnd, pgs->elapsed_text);

    if (pgs->estimated != -1) { /* estimated available */
        char tmp[20];
        SetWindowText(estimated_wnd, pgs->estimated_text);
        sprintf(tmp, "%.2f MBps", pgs->avg_bps / (1024.0 * 1024.0));
        SetWindowText(avg_data_rate, tmp);
        sprintf(tmp, "%.2f MBps", pgs->curr_bps / (1024.0 * 1024.0));
        SetWindowText(curr_data_rate, tmp);
    } else { /* estimated not available */
        SetWindowText(estimated_wnd, get_string(IDS_ESTIMATED_UNKNOWN, 0));
        SetWindowText(avg_data_rate, get_string(IDS_DATA_RATE_UNKNOWN, 0));
        SetWindowText(curr_data_rate, get_string(IDS_DATA_RATE_UNKNOWN, 0));
    }

    do /* process messages queue */
    {
        message_found = 0;
        if (PeekMessage(&msg, dlg, 0, 0, PM_REMOVE)) {
            message_found = 1;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (PeekMessage(&msg, progress_dlg, 0, 0, PM_REMOVE)) {
            message_found = 1;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    } while (message_found);

    return (!interrupted ? RET_OK : RET_INTERRUPTED); /* todo: check for interrupt */
}


progress_t *
get_progress(HWND dlg)
{
    progress_t *pgs = pgs_alloc(&progress_cb, NULL);
    if (pgs != NULL) {
        progress_dlg = CreateDialog(inst, MAKEINTRESOURCE(IDD_PROGRESS_DLG),
                                    dlg, progress_dlg_proc);
        if (progress_dlg != NULL) {
            interrupted = 0;
            EnableWindow(dlg, FALSE);
            ShowWindow(progress_dlg, SW_SHOW);
            SendMessage(progress_dlg, WM_PAINT, 0, 0);
        } else {
            pgs_free(pgs);
            pgs = NULL;
        }
    }
    return (pgs);
}


void dispose_progress(progress_t *pgs)
{
    HWND dlg = GetParent(progress_dlg);
    DestroyWindow(progress_dlg);
    EnableWindow(dlg, TRUE);
    SetFocus(dlg);
    pgs_free(pgs);
}


/**************************************************************/
static void
install(HWND dlg)
{
    char source[MAX_PATH + 1] = {'\0'};
    char target[MAX_PATH + 1] = {'\0'};
    char game_name[HDL_GAME_NAME_MAX + 1] = {'\0'};
    char signature[8 + 1 + 3 + 1] = {'\0'};
    int input_is_dvd = SendMessage(GetDlgItem(dlg, IDC_SOURCE_TYPE), CB_GETCURSEL, 0, 0) == 0;
    int input_dma = SendMessage(GetDlgItem(dlg, IDC_DMA_TYPE), CB_GETCURSEL, 0, 0);

    dlg_get_source(dlg, source);
    dlg_get_target(dlg, target);

    GetWindowText(GetDlgItem(dlg, IDC_GAMENAME), game_name, sizeof(game_name));
    GetWindowText(GetDlgItem(dlg, IDC_SIGNATURE), signature, sizeof(signature));

    if (strlen(target) > 0 &&
        strlen(game_name) > 0 &&
        strlen(signature) > 0 &&
        strlen(source) > 0) {
        iin_t *iin;
        int result = iin_probe(config_, source, &iin);
        if (result == RET_OK) {
            progress_t *pgs;
            hdl_game_t game;
            size_t i;
            ps2_cdvd_info_t info;

            memset(&game, 0, sizeof(hdl_game_t));
            strcpy(game.name, game_name);
            strcpy(game.startup, signature);
            game.compat_flags = 0;
            for (i = 0; i < MAX_FLAGS; ++i)
                game.compat_flags |= (IsDlgButtonChecked(dlg, MODE_IDC[i]) == BST_CHECKED ? 1 << i : 0);
            game.is_dvd = input_is_dvd;
            game.layer_break = 0; /* unsupported w/o ASPI */
            if (input_dma < 3)
                game.dma = input_dma * 256 + 32;
            else
                game.dma = (input_dma - 3) * 256 + 64;

            result = isofs_get_ps2_cdvd_info(iin, &info);
            if (result == RET_OK) {
                if (info.layer_pvd != 0)
                    game.layer_break = (u_int32_t)info.layer_pvd - 16;
            }

            if (result == RET_OK)
                /* update compatibility database */
                ddb_update(config_, game.startup, game.name, game.compat_flags);

            pgs = get_progress(dlg);
            result = hdl_inject(hio_, iin, &game, -1, 0, pgs);
            dispose_progress(pgs);

            iin->close(iin);
        }

        if (result != RET_OK &&
            result != RET_INTERRUPTED)
            show_error_2(dlg, result);
        dlg_refresh_hdd_info(dlg); /* always reload HDD info */
    } else
        return; /* all are required */
}

#if 0
/**************************************************************/
static void
delete (HWND dlg)
{
  char device [MAX_PATH] = { '\0' };
  dlg_get_target (dlg, device);
  if (device [0] != '\0')
  {
      apa_toc_t *toc;
      int result = apa_toc_read_ex (hio_, &toc);
      if (result == RET_OK)
      {
      /* TODO: re-read games_ */
          HWND lvw = GetDlgItem (dlg, IDC_CONTENTS);
          if (ListView_GetSelectedCount (lvw) > 0 &&
              MessageBox (dlg, get_string (IDS_CONFIRM_DELETE, 0),
                          get_string (IDS_CONFIRM_DELETE_TITLE, 1),
                          MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES)
          {
              int i, count = ListView_GetItemCount (lvw);
              for (i=0; result == RET_OK && i<count; ++i)
              {
                  int state = ListView_GetItemState (lvw, i, LVIS_SELECTED);
                  if (state == LVIS_SELECTED)
                  { /* lookup by game name */
                      char buffer [100];
                      int j;
                      ListView_GetItemText (lvw, i, 0, buffer, sizeof (buffer));
                      for (j=0; j<games_->count; ++j)
                          if (strcmp (buffer, games_->games [j].name) == 0)
                          {
                              result = apa_delete_partition (toc, games_->games [j].partition_name);
                              break;
                          }
                  }
              }
              if (result == RET_OK)
                  result = apa_commit_ex (hio_, toc);
          } /* confirmation */
          apa_toc_free (toc);
      } /* ptable_read */

      if (result != RET_OK)
          show_error_2 (dlg, result);
      dlg_refresh_hdd_info (dlg); /* always reload HDD info */
  }
}
#endif

/**************************************************************/
void install_or_delete(HWND dlg)
{
    install(dlg);
}


/**************************************************************/
static BOOL
dlg_init(HWND dlg)
{
    /* assign icon */
    HICON icon = LoadIcon(inst, MAKEINTRESOURCE(IDI_APPL));
    if (icon != NULL)
        SetClassLong(dlg, GCL_HICON, (LONG)icon);

    { /* set-up default/last IP address */
        const char *ip_tmp = dict_lookup(config_, CONFIG_LAST_IP);
        DWORD ip;
        if (ip_tmp == NULL)
            ip_tmp = "192.168.0.10";
        ip = ntohl(inet_addr(ip_tmp)); /* cast to host byte order */
        SendMessage(GetDlgItem(dlg, IDC_PS2IP), IPM_SETADDRESS, 0, ip);
    }

    /* try to attach to a local HDD */
    if (fill_ps2hdd_combo(dlg) == RET_OK)
        CheckDlgButton(dlg, IDC_PS2HDD_LOCAL, BST_CHECKED);
    else { /* disable "local HDD" at all if not found */
        CheckDlgButton(dlg, IDC_PS2HDD_NETWORK, BST_CHECKED);
        EnableWindow(GetDlgItem(dlg, IDC_PS2HDD_LOCAL), FALSE);
    }
    dlg_switch_target(dlg);

    /* initialize default operation to "install" */
    CheckDlgButton(dlg, IDC_INSTALL_OPT, BST_CHECKED);
    dlg_switch_operation(dlg);

    /* initialize default "install" ISO */
    CheckDlgButton(dlg, IDC_ISO_OPT, BST_CHECKED);
    dlg_switch_source(dlg);

    { /* add CD and DVD labels to the source type combo box */
        HWND cbo = GetDlgItem(dlg, IDC_SOURCE_TYPE);
        SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)get_string(IDS_DVD, 0));
        SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)get_string(IDS_CD, 0));
        SendMessage(cbo, CB_SETCURSEL, 0, 0);
    }

    { /* LIST DMA MODES */
        HWND cbo = GetDlgItem(dlg, IDC_DMA_TYPE);
        SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)get_string(IDS_MDMA0, 0));
        SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)get_string(IDS_MDMA1, 0));
        SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)get_string(IDS_MDMA2, 0));
        SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)get_string(IDS_UDMA0, 0));
        SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)get_string(IDS_UDMA1, 0));
        SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)get_string(IDS_UDMA2, 0));
        SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)get_string(IDS_UDMA3, 0));
        SendMessage(cbo, CB_ADDSTRING, 0, (LPARAM)get_string(IDS_UDMA4, 0));
        SendMessage(cbo, CB_SETCURSEL, 7, 0);
    }

    if (fill_opticals_combo(dlg) != RET_OK)
        /* disable combo box with optical drives if none are found */
        EnableWindow(GetDlgItem(dlg, IDC_OPTICAL_OPT), 0);

    /* setup list view */
    dlg_init_contents_list(dlg);

    /* attempt to show HDD details; for locally connected HDDs only */
    if (IsDlgButtonChecked(dlg, IDC_PS2HDD_LOCAL) == BST_CHECKED) {
        int result = dlg_refresh_hdd_info(dlg);
        if (result != RET_OK) {
            show_error_2(dlg, result);
            /* disable "local HDD" */
            CheckDlgButton(dlg, IDC_PS2HDD_LOCAL, BST_UNCHECKED);
            CheckDlgButton(dlg, IDC_PS2HDD_NETWORK, BST_CHECKED);
            EnableWindow(GetDlgItem(dlg, IDC_PS2HDD_LOCAL), FALSE);
            dlg_switch_target(dlg);
        }
    }

    /* limit maximal number of characters for the text boxes */
    SendMessage(GetDlgItem(dlg, IDC_GAMENAME), EM_LIMITTEXT,
                HDL_GAME_NAME_MAX, 0);
    SendMessage(GetDlgItem(dlg, IDC_SIGNATURE), EM_LIMITTEXT,
                8 + 1 + 3 + 1, 0);

    return (TRUE);
}


/**************************************************************/
INT_PTR CALLBACK
main_dlg_proc(HWND dlg,
              UINT msg,
              WPARAM wparam,
              LPARAM lparam)
{
    switch (msg) {
        case WM_INITDIALOG:
            return (dlg_init(dlg));

        case WM_CLOSE:
            EndDialog(dlg, IDCANCEL);
            return (TRUE);

        case WM_COMMAND:
            if (HIWORD(wparam) == BN_CLICKED) { /* button clicked */
                switch (LOWORD(wparam))         /* control ID */
                {
                    case IDC_PS2HDD_LOCAL:
                    case IDC_PS2HDD_NETWORK:
                        dlg_switch_target(dlg);
                        dlg_refresh_hdd_info(dlg);
                        break;

                    case IDC_INSTALL_OPT:
                    case IDC_BROWSE_OPT:
                        dlg_switch_operation(dlg);
                        break;

                    case IDC_ISO_OPT:
                    case IDC_OPTICAL_OPT:
                        dlg_switch_source(dlg);
                        show_source_info(dlg);
                        break;

                    case IDC_BROWSE_FOR_ISO:
                        browse_for_iso(dlg);
                        break;

                    case IDC_NET_U2LINK: { /* attempt to connect to the networking server */
                        int result = dlg_refresh_hdd_info(dlg);
                        if (result != RET_OK)
                            show_error_2(dlg, result);
                    } break;

                    case IDC_ACTION:
                        if (hio_ != NULL)
                            install_or_delete(dlg);
                        break;

                    case IDCANCEL:
                        SendMessage(dlg, WM_CLOSE, 0, 0);
                        break;
                }
            } else if (HIWORD(wparam) == CBN_SELCHANGE) {
                switch (LOWORD(wparam)) /* control ID */
                {
                    case IDC_PS2HDD: {
                        int result = dlg_refresh_hdd_info(dlg);
                        if (result != RET_OK)
                            show_error_2(dlg, result);
                    } break;

                    case IDC_SOURCE_DRIVE:
                        show_source_info(dlg);
                        break;
                }
            } else if (HIWORD(wparam) == EN_KILLFOCUS) {
                switch (LOWORD(wparam)) {
                    case IDC_ISO_PATH:
                        show_source_info(dlg);
                        break;
                }
            }
            return (TRUE);
            /* end of WM_COMMAND */

        case WM_NOTIFY: {
            LPNMHDR nmh = (LPNMHDR)lparam;
            switch (nmh->idFrom) {
                case IDC_PS2IP:
                    /* IP address has changed */
                    EnableWindow(GetDlgItem(dlg, IDC_NET_U2LINK), TRUE);
                    break;

                case IDC_CONTENTS:
                    /* to enable rename append LVS_EDITLABELS to IDC_CONTENTS'
                     * window styles (in rsrc.rc) */
                    switch (nmh->code) {
                        case LVN_BEGINLABELEDIT:
                            return (TRUE); /* label edit allowed */
                        case LVN_ENDLABELEDIT: {
                            LPNMLVDISPINFOW info = (LPNMLVDISPINFOW)lparam;
                            if (info->item.pszText != NULL) { /* label of iItem changed */
                                /* TODO: do rename */
                                MessageBox(dlg, info->item.pszText, "Blah", MB_OK);
                            }
                            return (TRUE);
                        }
                    }
                    break;
            }
        }
            return (TRUE);

        default:
            return (FALSE);
    }
}


/**************************************************************/
int WINAPI
WinMain(HINSTANCE curr_inst,
        HINSTANCE prev_inst,
        LPSTR cmd_line,
        int show_state)
{
    int result;
    INITCOMMONCONTROLSEX iccx;

    /* load configuration */
    config_ = dict_alloc();
    if (config_ != NULL) {
        set_config_defaults(config_);
        dict_restore(config_, get_config_file());
        dict_store(config_, get_config_file());
    }

    /* that way of common controls init should work fine with Windows 2000 */
    iccx.dwSize = sizeof(iccx);
    iccx.dwICC = ICC_WIN95_CLASSES | ICC_INTERNET_CLASSES;
    InitCommonControlsEx(&iccx);

    /* accept a single device name from the command-line */
    if (cmd_line != NULL && *cmd_line != '\0')
        device_name = cmd_line;

    inst = curr_inst;
    result = DialogBox(curr_inst, (LPCTSTR)MAKEINTRESOURCE(IDD_MAIN_DLG),
                       NULL, &main_dlg_proc);
    if (result == -1)
        show_error(NULL);

    if (hio_ != NULL)
        hio_->close(hio_);

    dict_store(config_, get_config_file());
    dict_free(config_);

    return (0);
}
