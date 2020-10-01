/*-----------------------------------------------------------------------------
	Paper Plane cUI								各種ダイアログボックス
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPX_64.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#pragma hdrstop

UINT CWIN_JXX_GROUP[] = {IDX_CWIN_DBL, IDX_CWIN_DAC, 0};
UINT CWIN_JLR_GROUP[] = {IDX_CWIN_JUD, IDX_CWIN_JP, 0};
UINT CWIN_JUD_GROUP[] = {IDX_CWIN_JLR, IDX_CWIN_JP, 0};
UINT CWIN_JP_GROUP[] = {IDX_CWIN_JLR, IDX_CWIN_JUD, 0};
UINT CWIN_FP_GROUP[] = {IDX_CWIN_FUD, 0};
UINT CWIN_FUD_GROUP[] = {IDX_CWIN_FP, 0};

UINT CWIN_CMB_GROUP[] = {IDX_CWIN_JLR, IDX_CWIN_JUD, IDX_CWIN_JP, IDX_CWIN_JF, IDX_CWIN_FP, IDX_CWIN_FUD, IDX_CWIN_DBL, IDX_CWIN_CLO, IDX_CWIN_DAC, 0};

const TCHAR *SortItems[] = {
	T("SORG|No sort"),			// -1

	T("SRTN|N Name"),			// 0
	T("SRTE|E Ext"),			// 1
	T("SRTS|S file Size"),		// 2
	T("SRTT|T Time stamp"),		// 3
	T("SRTC|C Create time"),	// 4
	T("SRTA|A Access time"),	// 5
	T("SRTM|M Mark"),			// 6
	T("SRTH|H Changed"),		// 7

	T("SRTO|O Comment"),		// 17
	T("SRTX|X Ext color"),		// 20

	T("STnR|n R)Name"),			// 8
	T("STeR|e R)Ext"),			// 9
	T("STsR|s R)file Size"),	// 10
	T("STtR|t R)Time stamp"),	// 11
	T("STcR|c R)Create time"),	// 12
	T("STaR|a R)Access time"),	// 13
	T("STmR|m R)Mark"),			// 14
	T("SThR|h R)Changed"),		// 15

	T("SToR|o R)Comment"),		// 18
	T("STxR|x R)Ext color"),	// 21

	T("SRTP|P Plain"),			// 19
	T("SRTR|R Attributes"),		// 16
	T("SRTD|D Directory"),		// 22
	T("STdR|d R)Directory"),	// 23
	NULL
};
char sortIDtable[] =
	{ -1, 0, 1, 2, 3, 4, 5, 6, 7,		17, 20,
	   8, 9, 10, 11, 12, 13, 14, 15,	18, 21, 19, 16, 22, 23 };

void GetSortSettings(HWND hDlg, XC_SORT *xs);

#ifndef FILE_READ_ONLY_VOLUME
#define FILE_READ_ONLY_VOLUME				0x00080000
#endif
#ifndef FILE_SEQUENTIAL_WRITE_ONCE
#define FILE_SEQUENTIAL_WRITE_ONCE			0x00100000
#define FILE_SUPPORTS_TRANSACTIONS			0x00200000
#endif
#ifndef FILE_SUPPORTS_HARD_LINKS
#define FILE_SUPPORTS_HARD_LINKS			0x00400000
#define FILE_SUPPORTS_EXTENDED_ATTRIBUTES	0x00800000
#define FILE_SUPPORTS_OPEN_BY_FILE_ID		0x01000000
#define FILE_SUPPORTS_USN_JOURNAL			0x02000000
#endif
#ifndef FILE_SUPPORTS_INTEGRITY_STREAMS
#define FILE_SUPPORTS_INTEGRITY_STREAMS		0x04000000
#endif
#ifndef FILE_SUPPORTS_BLOCK_REFCOUNTING
#define FILE_SUPPORTS_BLOCK_REFCOUNTING		0x08000000
#endif
#ifndef FILE_SUPPORTS_SPARSE_VDL
#define FILE_SUPPORTS_SPARSE_VDL		    0x10000000
#endif
#ifndef FILE_DAX_VOLUME
#define FILE_DAX_VOLUME					    0x20000000
#endif

struct DISKINFOSTRUCT {
	DWORD flag, ID;
}  DiskInfoFlags[] = {
// x
	{FS_CASE_SENSITIVE,			IDS_DSKI_FCS}, // FILE_CASE_SENSITIVE_SEARCH
	{FS_CASE_IS_PRESERVED,		IDS_DSKI_FCP}, // FILE_CASE_PRESERVED_NAMES
	{FS_UNICODE_STORED_ON_DISK,	IDS_DSKI_FUS}, // FILE_UNICODE_ON_DISK
	{FS_PERSISTENT_ACLS,		IDS_DSKI_FPA}, // FILE_PERSISTENT_ACLS
// x0
	{FS_FILE_COMPRESSION,		IDS_DSKI_FFC}, // FILE_FILE_COMPRESSION
	{FILE_VOLUME_QUOTAS,		IDS_DSKI_FQT},
	{FILE_SUPPORTS_SPARSE_FILES, IDS_DSKI_FSP},
	{FILE_SUPPORTS_REPARSE_POINTS, IDS_DSKI_FRP},
// x00
	{FILE_SUPPORTS_REMOTE_STORAGE, IDS_DSKI_FRE},
	// FILE_RETURNS_CLEANUP_RESULT_INFO		0x00000200
	// FILE_SUPPORTS_POSIX_UNLINK_RENAME	0x00000400
// x000
	{FS_VOL_IS_COMPRESSED,		IDS_DSKI_FVC}, // FILE_VOLUME_IS_COMPRESSED
// x0000
	{FILE_SUPPORTS_OBJECT_IDS,	IDS_DSKI_FOI},
	{FS_FILE_ENCRYPTION,		IDS_DSKI_FEC}, // FILE_SUPPORTS_ENCRYPTION
	{FILE_NAMED_STREAMS,		IDS_DSKI_FNS},
	{FILE_READ_ONLY_VOLUME,				IDS_DSKI_READONLY},
// x00000
	{FILE_SEQUENTIAL_WRITE_ONCE,		IDS_DSKI_SEQWRITEONCE},
	{FILE_SUPPORTS_TRANSACTIONS,		IDS_DSKI_TRANSACTIONS},
	{FILE_SUPPORTS_HARD_LINKS,			IDS_DSKI_HARD_LINKS}, // Win7
	{FILE_SUPPORTS_EXTENDED_ATTRIBUTES,	IDS_DSKI_EXT_ATTRIBUTES}, // Win7
// x000000
	{FILE_SUPPORTS_OPEN_BY_FILE_ID,		IDS_DSKI_OPENBY_FILEID}, // Win7
	{FILE_SUPPORTS_USN_JOURNAL,			IDS_DSKI_USN_JOURNAL}, // Win7
	{FILE_SUPPORTS_INTEGRITY_STREAMS,	IDS_DSKI_INTEGRITY_STREAMS},
	{FILE_SUPPORTS_BLOCK_REFCOUNTING,	IDS_DSKI_BLOCK_REFCOUNTING},
// x0000000
	{FILE_SUPPORTS_SPARSE_VDL,			IDS_DSKI_SPARSE_VDL},
	{FILE_DAX_VOLUME,					IDS_DSKI_DAX_VOLUME}, // Win10 1607
	// FILE_SUPPORTS_GHOSTING				0x40000000
	{0, 0}
};
const TCHAR Disktype_Remote[] = MES_DIRE;
const TCHAR Disktype_CDROM[] = MES_DICD;
const TCHAR Disktype_RAMDISK[] = MES_DIRA;
const TCHAR Disktype_REMOVABLE[] = MES_DIRM;
const TCHAR Disktype_FIXED[] = MES_DIFI;

#if !NODLL
void EnableDlgWindow(HWND hDlg, int id, BOOL state)
{
	HWND hControlWnd;

	hControlWnd = GetDlgItem(hDlg, id);
	if ( hControlWnd == NULL ) return;
	EnableWindow(hControlWnd, state);
}
#endif

void CenterPPcDialog(HWND hDlg, PPC_APPINFO *cinfo)
{
	if ( X_combos[1] & CMBS1_DIALOGNOPANE ){
		CenterWindow(hDlg);
	}else{
		MoveCenterWindow(hDlg, cinfo->info.hWnd);
	}
}

DWORD GetAttibuteSettings(HWND hDlg)
{
	DWORD attr;

	attr =
		(IsDlgButtonChecked(hDlg, IDX_FOP_RONLY) ? FILE_ATTRIBUTE_READONLY : 0) |
		(IsDlgButtonChecked(hDlg, IDX_FOP_HIDE) ?  FILE_ATTRIBUTE_HIDDEN : 0) |
		(IsDlgButtonChecked(hDlg, IDX_FOP_SYSTEM) ? FILE_ATTRIBUTE_SYSTEM : 0) |
		(IsDlgButtonChecked(hDlg, IDX_FOP_ARC) ?   FILE_ATTRIBUTE_ARCHIVE : 0);
	if ( GetDlgItem(hDlg, IDX_FOP_DIR) != NULL ){
		if ( IsDlgButtonChecked(hDlg, IDX_FOP_DIR) ){
			setflag(attr, FILE_ATTRIBUTE_DIRECTORY);
		}
	}
	return attr;
}

void SetAttibuteSettings(HWND hDlg, DWORD attr)
{
	CheckDlgButton(hDlg, IDX_FOP_RONLY,	attr & FILE_ATTRIBUTE_READONLY);
	CheckDlgButton(hDlg, IDX_FOP_HIDE,	attr & FILE_ATTRIBUTE_HIDDEN);
	CheckDlgButton(hDlg, IDX_FOP_SYSTEM,	attr & FILE_ATTRIBUTE_SYSTEM);
	CheckDlgButton(hDlg, IDX_FOP_ARC,	attr & FILE_ATTRIBUTE_ARCHIVE);
	if ( GetDlgItem(hDlg, IDX_FOP_DIR) != NULL ){
		CheckDlgButton(hDlg, IDX_FOP_DIR, attr & FILE_ATTRIBUTE_DIRECTORY);
	}
}

// Window Option --------------------------------------------------------------
void SetWindowOption(HWND hDlg)
{
	PPC_APPINFO *cinfo;
	int new_combo, doubleboot;

	cinfo = (PPC_APPINFO *)GetWindowLongPtr(hDlg, DWLP_USER);
	new_combo = IsDlgButtonChecked(hDlg, IDX_CWIN_CMB);
	if ( new_combo != X_combo ){
		SetPopMsg(cinfo, POPMSG_MSG, MES_RRPC);
		SetCustData(T("X_combo"), &new_combo, sizeof(new_combo));
	}
	doubleboot = IsDlgButtonChecked(hDlg, IDX_CWIN_DBL);
	if ( new_combo ){
		int X_mpane[2] = {32, 1};

		GetCustData(T("X_mpane"), X_mpane, sizeof(X_mpane));
		X_mpane[1] = doubleboot ? max(X_mpane[1], 2) : 1;
		SetCustData(T("X_mpane"), X_mpane, sizeof(X_mpane));
		doubleboot = 0;
	}
	cinfo->swin = (cinfo->swin & ~0xffff) |
	 (IsDlgButtonChecked(hDlg, IDX_CWIN_JLR) ? (SWIN_JOIN | SWIN_LRJOIN): 0) |
	 (IsDlgButtonChecked(hDlg, IDX_CWIN_JUD) ? (SWIN_JOIN | SWIN_UDJOIN): 0) |
	 (IsDlgButtonChecked(hDlg, IDX_CWIN_JP)  ? (SWIN_JOIN | SWIN_PILEJOIN): 0) |
	 (IsDlgButtonChecked(hDlg, IDX_CWIN_JF)  ? SWIN_FIT : 0) |
	 (doubleboot							 ? SWIN_WBOOT : 0) |
	 (IsDlgButtonChecked(hDlg, IDX_CWIN_CLO) ? SWIN_WQUIT : 0) |
	 (IsDlgButtonChecked(hDlg, IDX_CWIN_FP ) ? SWIN_FIXACTIVE : 0) |
	 (IsDlgButtonChecked(hDlg, IDX_CWIN_FUD) ? (SWIN_FIXACTIVE | SWIN_FIXPOSIZ) : 0) |
	 (IsDlgButtonChecked(hDlg, IDX_CWIN_DAC) ? SWIN_WACTIVE : 0);
	SendX_win(cinfo);
	if (cinfo->swin & SWIN_JOIN){
		JoinWindow(cinfo);
		if ( !(cinfo->swin & SWIN_WBOOT) ) BootPairPPc(cinfo);
	}
}

void CheckDlgButtonGroup(HWND hDlg, UINT *checkgroup, int Check, int Enable)
{
	while ( *checkgroup ){
		HWND hControlWnd;

		hControlWnd = GetDlgItem(hDlg, *checkgroup++);
		if ( hControlWnd == NULL ) continue;
		if ( Check >= 0 ) SendMessage(hControlWnd, BM_SETCHECK, (WPARAM)Check, 0);
		if ( Enable >= 0 ) EnableWindow(hControlWnd, Enable);
	}
}

BOOL UnCheckFix(HWND hDlg, UINT ID, UINT *checkgroup)
{
	if ( IsDlgButtonChecked(hDlg, ID) == FALSE ) return FALSE;
	CheckDlgButtonGroup(hDlg, checkgroup, FALSE, -1);
	return TRUE;
}

INT_PTR CALLBACK WindowDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch(iMsg){
		case WM_INITDIALOG: {
			PPC_APPINFO *cinfo;

			cinfo = (PPC_APPINFO *)lParam;
			SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)cinfo);
			CenterPPcDialog(hDlg, cinfo);
			LocalizeDialogText(hDlg, IDD_CWINDOW);
			CheckDlgButton(hDlg, IDX_CWIN_DBL, cinfo->swin & SWIN_WBOOT);
			CheckDlgButton(hDlg, IDX_CWIN_CLO, cinfo->swin & SWIN_WQUIT);
			if ( X_combo != COMBO_OFF ){
				CheckDlgButton(hDlg, IDX_CWIN_CMB, TRUE);
				CheckDlgButtonGroup(hDlg, CWIN_CMB_GROUP, -1, FALSE);
			}
			CheckDlgButton(hDlg, IDX_CWIN_DAC, cinfo->swin & SWIN_WACTIVE);
			if (cinfo->swin & SWIN_JOIN){
				CheckDlgButton(hDlg,
					IDX_CWIN_JLR + ((cinfo->swin & SWIN_JOINMASK) >> 6), TRUE);
			}
			CheckDlgButton(hDlg, IDX_CWIN_JF, (cinfo->swin & SWIN_FIT));
			if (cinfo->swin & SWIN_FIXACTIVE){
				CheckDlgButton(hDlg, IDX_CWIN_FP, !(cinfo->swin & SWIN_FIXPOSIZ));
				CheckDlgButton(hDlg, IDX_CWIN_FUD, (cinfo->swin & SWIN_FIXPOSIZ));
			}
			return TRUE;
		}
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDOK:
					SetWindowOption(hDlg);
					EndDialog(hDlg, 1);
					break;

				case IDCANCEL:
					EndDialog(hDlg, 1);
					break;

				case IDX_CWIN_JLR:
					if ( UnCheckFix(hDlg, IDX_CWIN_JLR, CWIN_JLR_GROUP) ){
						CheckDlgButtonGroup(hDlg, CWIN_JXX_GROUP, TRUE, -1);
					}
					break;
				case IDX_CWIN_JUD:
					if ( UnCheckFix(hDlg, IDX_CWIN_JUD, CWIN_JUD_GROUP) ){
						CheckDlgButtonGroup(hDlg, CWIN_JXX_GROUP, TRUE, -1);
					}
					break;
				case IDX_CWIN_JP:
					UnCheckFix(hDlg, IDX_CWIN_JP, CWIN_JP_GROUP);
					break;
				case IDX_CWIN_FP:
					UnCheckFix(hDlg, IDX_CWIN_FP, CWIN_FP_GROUP);
					break;
				case IDX_CWIN_FUD:
					UnCheckFix(hDlg, IDX_CWIN_FUD, CWIN_FUD_GROUP);
					break;
				case IDX_CWIN_CMB:
					if (IsDlgButtonChecked(hDlg, IDX_CWIN_CMB)){
						CheckDlgButtonGroup(hDlg, CWIN_CMB_GROUP, FALSE, FALSE);
					}else{
						CheckDlgButtonGroup(hDlg, CWIN_CMB_GROUP, -1, TRUE);
					}
					break;

				case IDHELP:
					return PPxDialogHelper(hDlg, WM_HELP, wParam, lParam);

				case IDQ_GETDIALOGID:
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT, (LONG_PTR)IDD_CWINDOW);
					break;
			}
			break;

		default:
			return PPxDialogHelper(hDlg, iMsg, wParam, lParam);
	}
	return TRUE;
}

// Disk[I]nfo -----------------------------------------------------------------
DWORD CalRate(ULARGE_INTEGER *value, ULARGE_INTEGER *total)
{
	DWORD sizeL, sizeH, totalper;

	sizeL = value->u.LowPart;
	sizeH = value->u.HighPart;

	if ( (total->u.LowPart & 0xffe00000) || total->u.HighPart ){
		if ( total->u.HighPart >= 0x20 ){
			if ( sizeH & 0xffffe000 ){ // 64T over
				if ( sizeH & 0xffe00000 ){ // 16P over
					totalper = ((sizeH >> 12) * 1000) / (total->u.HighPart >> 12);
				}else{ // 64T over
					totalper = (sizeH * 1000) / total->u.HighPart;
				}
			}else{ // 0x0004 0000 0000(16GB) ～ 0x3fff ffff ffff(64T)

				totalper =
						( ((((DWORD)sizeH) << 8) | (sizeL >> 24)) * 1000) /
						( ((((DWORD)total->u.HighPart) << 8) |
							(total->u.LowPart >> 24)) );
			}
		}else{	// 0x0040 0000(4M) ～ 0x3 ffff ffff(16G)
			totalper =
					( ((((DWORD)sizeH) << 20) | (sizeL >> 12)) * 1000) /
					( ((((DWORD)total->u.HighPart) << 20) |
						(total->u.LowPart >> 12)) );
		}
	}else{		// 0x0000 0001      ～  0x003f ffff(4M)
		if ( total->u.LowPart ){
			totalper = (sizeL * 1000) / total->u.LowPart;
		}else{	// 0x0000 0000
			totalper = 1000;
		}
	}
	return totalper;
}

void SetDriveSizeItem(HWND hDlg, UINT bytes, UINT rate, ULARGE_INTEGER *value, int rate_value)
{
	TCHAR buf[VFPS];

	FormatNumber(buf, XFN_SEPARATOR, 18, value->u.LowPart, value->u.HighPart);
	SetDlgItemText(hDlg, bytes, buf);

	wsprintf(buf, T("%d.%d%%"), rate_value / 10, rate_value % 10);
	SetDlgItemText(hDlg, rate, buf);
}

INT_PTR CALLBACK DiskinfoDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch(iMsg){
		case WM_INITDIALOG: {
			TCHAR drv[VFPS], vname[MAX_PATH + 1], fs[MAX_PATH], buf[VFPS];
			const TCHAR *typep;
			int cluster;
			DWORD i1, i2, i3, i4, BpC;
			ULARGE_INTEGER UserFree, Total, Free;
			PPC_APPINFO *cinfo;

			cinfo = (PPC_APPINFO *)lParam;
			Total.u.LowPart = Total.u.HighPart = 0;

			CenterPPcDialog(hDlg, cinfo);
			LocalizeDialogText(hDlg, IDD_DSKI);
			GetDriveName(drv, cinfo->RealPath);

			SetDlgItemText(hDlg, IDS_DSKI_DN, drv);
			if ( Isalpha(drv[0]) ){
				wsprintf(vname, T("Network\\%c"), drv[0]);
				buf[0] = fs[0] = '\0';
				GetRegString(HKEY_CURRENT_USER, vname, RMPATHSTR, fs, TSIZEOF(fs));
				if ( fs[0] != '\0' ){
					wsprintf(buf, T("%s (%s)"), drv, fs);
				}else{
					wsprintf(vname, T("%c:"), drv[0]);
					GetRegString(HKEY_LOCAL_MACHINE, RegDeviceNamesStr, vname, fs, TSIZEOF(fs));
					if ( (tstrlen(fs) > 5) && (fs[2] == '?') ){
						wsprintf(buf, T("%s (%s)"), drv, fs + 4);
					}
				}
				if ( buf[0] != '\0' ){
					SetDlgItemText(hDlg, IDS_DSKI_DN, buf);
				}
			}

			if ( IsTrue(GetVolumeInformation(drv, vname, MAX_PATH,
					&i1, &i2, &i3, fs, MAX_PATH)) ){
				int i;

				SetDlgItemText(hDlg, IDE_DSKI_VN, vname );
				EnableDlgWindow(hDlg, IDB_APPLY, FALSE);
				wsprintf(buf, T("%04X-%04X"), HIWORD(i1), LOWORD(i1));
				SetDlgItemText(hDlg, IDS_DSKI_SN, buf);
				FormatNumber(buf, XFN_SEPARATOR, 8, i2, 0);
				SetDlgItemText(hDlg, IDS_DSKI_MF, buf);
				SetDlgItemText(hDlg, IDS_DSKI_FS, fs);
				for ( i = 0 ; DiskInfoFlags[i].flag ; i++ ){
					if ( (DiskInfoFlags[i].ID >= IDS_DSKI_HARD_LINKS) && (OSver.dwMajorVersion < 6) ){
						// Vista 以前でも有効だが、フラグがなかったので非表示
						ShowWindow(GetDlgItem(hDlg, DiskInfoFlags[i].ID), SW_HIDE);
					}else if ( i3 & DiskInfoFlags[i].flag ){
						EnableDlgWindow(hDlg, DiskInfoFlags[i].ID, TRUE);
					}
				}
			}
			BpC = cluster = i1 = i2 = i3 = i4 = 0;
			if ( IsTrue(GetDiskFreeSpace(drv, &i1, &i2, &i3, &i4)) ){
				cluster = 1;
				BpC = i1 * i2;
				DDmul(BpC, i3, &UserFree.u.LowPart, &UserFree.u.HighPart);
				DDmul(BpC, i4, &Total.u.LowPart, &Total.u.HighPart);
			}
			if (
#ifndef UNICODE
					DGetDiskFreeSpaceEx &&
#endif
					DGetDiskFreeSpaceEx(GetNT_9xValue(cinfo->RealPath, drv),
							&UserFree, &Total, &Free) ){
				cluster = 1;
				SetDriveSizeItem(hDlg, IDS_DSKI_NRF, IDS_DSKI_RRF,
						&Free, CalRate(&Free, &Total));
			}
			if ( cluster ){
				int free_rate;

				free_rate = CalRate(&UserFree, &Total);
				SetDriveSizeItem(hDlg, IDS_DSKI_NFS, IDS_DSKI_RFS, &UserFree, free_rate);
				DDmul(BpC, i4 - i3, &UserFree.u.LowPart, &UserFree.u.HighPart);
				SetDriveSizeItem(hDlg, IDS_DSKI_NUS, IDS_DSKI_RUS, &UserFree, 1000 - free_rate);

				FormatNumber(buf, XFN_SEPARATOR, 18,
						Total.u.LowPart, Total.u.HighPart);
				SetDlgItemText(hDlg, IDS_DSKI_NTS, buf);

				FormatNumber(buf, XFN_SEPARATOR, 18, i2, 0);
				SetDlgItemText(hDlg, IDS_DSKI_NBS, buf);
				FormatNumber(buf, XFN_SEPARATOR, 18, BpC, 0);
				SetDlgItemText(hDlg, IDS_DSKI_NBC, buf);
				FormatNumber(buf, XFN_SEPARATOR, 13, i3, 0);
				SetDlgItemText(hDlg, IDS_DSKI_CFS, buf);
				FormatNumber(buf, XFN_SEPARATOR, 13, i4 - i3, 0);
				SetDlgItemText(hDlg, IDS_DSKI_CUS, buf);
				FormatNumber(buf, XFN_SEPARATOR, 13, i4, 0);
				SetDlgItemText(hDlg, IDS_DSKI_CTS, buf);
			}
			switch( GetDriveType(drv) ){
				case DRIVE_REMOTE:
					typep = Disktype_Remote;
					break;
				case DRIVE_CDROM:
					typep = Disktype_CDROM;
					break;
				case DRIVE_RAMDISK:
					typep = Disktype_RAMDISK;
					break;
				case DRIVE_REMOVABLE:
					typep = Disktype_REMOVABLE;
					break;
				case DRIVE_FIXED:
					typep = Disktype_FIXED;
					break;
				default:
					typep = T("?");
			}
			SetDlgItemText(hDlg, IDS_DSKI_DT, MessageText(typep));
			break;
		}
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDOK:
					if ( IsWindowEnabled(GetDlgItem(hDlg, IDB_APPLY)) ){
						if( DiskinfoDlgBox(hDlg, iMsg, IDB_APPLY, lParam) == FALSE){
							break;
						}
					}
					EndDialog(hDlg, 1);
					break;

				case IDCANCEL:
					EndDialog(hDlg, 1);
					break;

				case IDB_APPLY:
					if ( IsWindowEnabled((HWND)lParam) ){
						TCHAR dir[MAX_PATH], vname[MAX_PATH];

						GetDlgItemText(hDlg, IDS_DSKI_DN, dir, MAX_PATH);
						GetDlgItemText(hDlg, IDE_DSKI_VN, vname, MAX_PATH);
						if (SetVolumeLabel(dir, vname) == FALSE){
							PPErrorBox(hDlg, T("Label"), PPERROR_GETLASTERROR);
							return FALSE;
						}else{
							EnableDlgWindow(hDlg, IDB_APPLY, FALSE);
						}
					}
					break;

				case IDE_DSKI_VN:
					if ( HIWORD(wParam) == EN_CHANGE ){
						EnableDlgWindow(hDlg, IDB_APPLY, TRUE);
					}
					break;

				case IDHELP:
					return PPxDialogHelper(hDlg, WM_HELP, wParam, lParam);

				case IDQ_GETDIALOGID:
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT, (LONG_PTR)IDD_DSKI);
					break;
			}
			break;

		default:
			return PPxDialogHelper(hDlg, iMsg, wParam, lParam);
	}
	return TRUE;
}

/*-----------------------------------------------------------------------------
	File[M]ask / Wildcard ダイアログボックス/コールバック
-----------------------------------------------------------------------------*/
#define TMIN_HIGHLIGHTOFF 3
#define TMIN_REVERSE 2
const TCHAR *MarkItemName[] = {MES_LMAK, MES_LUMA, MES_LRMA, MES_LHIO};

void SetMarkTarget(HWND hDlg, FILEMASKDIALOGSTRUCT *PFS)
{
	TCHAR buf[0x40], buf2[0x40];
	const TCHAR *typename = NilStr;

	if ( PFS->mode >= MARK_HIGHLIGHT1 ){
		typename = buf2;
		wsprintf(buf2, T("%s %d"), MessageText(MES_LHIL), PFS->mode - MARK_HIGHLIGHT1 + 1);
	}else if ( PFS->mode >= MARK_REVERSE ){
		typename = MessageText(MarkItemName[
				(PFS->mode == MARK_HIGHLIGHTOFF) ?
					TMIN_HIGHLIGHTOFF :
					TMIN_REVERSE - PFS->mode - 1]);
	}

	wsprintf(buf, T("%s %s"), MessageText(MES_FBAC), typename);
	SetDlgItemText(hDlg, IDX_MASK_MODE, buf);
}

#define ID_MARKMIN 1
// unmark:2 reverse:3
#define ID_HIGHTLIGHTOFF 4
#define ID_HIGHTLIGHT1 5
void MarkTargetMenu(HWND hDlg, HWND hButtonWnd, FILEMASKDIALOGSTRUCT *PFS)
{
	RECT box;
	HMENU hMenu;
	int index, i;
	TCHAR buf[VFPS];

	if ( IsShowButtonMenu() == FALSE ) return;

	GetWindowRect(hButtonWnd, &box);
	hMenu = CreatePopupMenu();

	for ( i = 0 ; i <= TMIN_HIGHLIGHTOFF ; i++ ){
		AppendMenuString(hMenu,
				(i == TMIN_HIGHLIGHTOFF) ? ID_HIGHTLIGHTOFF :
				1 - MARK_REVERSE - i + ID_MARKMIN, MarkItemName[i]);
		XMessage(NULL,NULL,XM_DbgLOG,T("%s %s"),MarkItemName[i],MessageText(MarkItemName[i]));
	}
	for ( i = 0 ; i < PPC_HIGHLIGHT_COLORS ; i++ ){
		wsprintf(buf, T("%s &%d"), MessageText(MES_LHIL), i + 1);
		AppendMenuString(hMenu, i + ID_HIGHTLIGHT1, buf);
		XMessage(NULL,NULL,XM_DbgLOG,T("%s"),buf);
	}

	CheckMenuRadioItem(hMenu, ID_MARKMIN, ID_HIGHTLIGHT1 + PPC_HIGHLIGHT_COLORS - 1, PFS->mode - MARK_REVERSE + 1, MF_BYCOMMAND);

	index = TrackPopupMenu(hMenu, TPM_TDEFAULT, box.left, box.bottom, 0, hDlg, NULL);
	DestroyMenu(hMenu);
	EndButtonMenu();
	if ( index > 0 ){
		index = index - ID_MARKMIN + MARK_REVERSE;
		if ( PFS->mode != index ){
			PFS->mode = index;
			SetMarkTarget(hDlg, PFS);
		}
	}
}

DWORD USEFASTCALL GetFileMaskAttr(HWND hDlg)
{
	return
		(GetAttibuteSettings(hDlg) ^
			(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
			 FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE)) |
		(IsDlgButtonChecked(hDlg, IDX_MASK_DIR) ? MASKEXTATTR_DIR : 0) |
		(IsDlgButtonChecked(hDlg, IDX_MASK_WORDMATCH) ? MASKEXTATTR_WORDMATCH : 0);
}

#define REALTIMEMASKLIMIT(cinfo) ((cinfo)->e.cellDataMax > (int)(OSver.dwMajorVersion * 3000 - 8000)) // Ver4:4000 5:7000 6:10000

const DWORD HideMaskItemList[] = {
		// FindMark の時は隠す
		IDX_MASK_RTM, /*IDX_MASK_MODE,*/
		// FindMark / FindMask で一時等以外の時は隠す
		IDG_FOP_ATTR, IDX_FOP_RONLY, IDX_FOP_SYSTEM, IDX_FOP_HIDE,
		IDX_FOP_ARC, IDX_MASK_DIR, 0};

void HideMaskItem(HWND hDlg, FILEMASKDIALOGSTRUCT *PFS)
{
	const DWORD *item = HideMaskItemList;
	int showstate = SW_HIDE;

	if ( IsTrue(PFS->maskmode) ){ // ファイルマスク
		item += 1; // 2;
		if ( (PFS->mode == DSMD_TEMP) || (PFS->mode == DSMD_REGID) ){
			showstate = SW_SHOWNOACTIVATE;
		}
	}
	for ( ; *item != 0 ; item++ ){
		ShowWindow(GetDlgItem(hDlg, *item), showstate);
	}
}

// FindMark / FindMask ダイアログ
INT_PTR CALLBACK FileMaskDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	FILEMASKDIALOGSTRUCT *PFS;

	PFS = (FILEMASKDIALOGSTRUCT *)GetWindowLongPtr(hDlg, DWLP_USER);
	switch (message){
							// ダイアログ初期化 (w:focus, l:User からの param)--
		case WM_INITDIALOG:
			PFS = (FILEMASKDIALOGSTRUCT *)lParam;
			SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)lParam);
			CenterPPcDialog(hDlg, PFS->cinfo);
			LocalizeDialogText(hDlg, 0);
			SetWindowText(hDlg, MessageText((TCHAR *)PFS->title));

			GetCustData(PFS->settingkey, &PFS->option, sizeof(PFS->option));
			if ( (PFS->mode == DSMD_TEMP) && PFS->option.dirmask ){
				setflag(PFS->mask->attr, MASKEXTATTR_DIR);
			}

			CheckDlgButton(hDlg, IDX_MASK_DIR, PFS->mask->attr & MASKEXTATTR_DIR);
			CheckDlgButton(hDlg, IDX_MASK_WORDMATCH, PFS->option.wordmatch);

			if ( IsTrue(PFS->maskmode) ){ // ファイルマスク
				SetAttibuteSettings(hDlg, PFS->mask->attr ^ BADATTR);
				CheckDlgButton(hDlg, IDX_MASK_RTM, PFS->option.realtime);
				EnableDlgWindow(hDlg, IDX_MASK_RTM, !REALTIMEMASKLIMIT(PFS->cinfo));
				SetMaskTarget(hDlg, PFS);
			}else{ // ファイルマーク
				SetMarkTarget(hDlg, PFS);
			}
			HideMaskItem(hDlg, PFS);

			PFS->hEdWnd = PPxRegistExEdit(&PFS->cinfo->info,
					GetDlgItem(hDlg, IDE_INPUT_LINE), TSIZEOF(PFS->mask->file),
					PFS->mask->file, PPXH_WILD_R, PPXH_MASK,
					PPXEDIT_COMBOBOX | PPXEDIT_WANTEVENT |
					PPXEDIT_REFTREE | PPXEDIT_SINGLEREF /*| PPXEDIT_INSTRSEL combobox 未対応*/);
			return TRUE;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDE_INPUT_LINE:
					if ( (PFS->maskmode == FALSE) ||
						 (PFS->option.realtime == FALSE) ||
						 REALTIMEMASKLIMIT(PFS->cinfo) ){
						break;
					}
					if ( (HIWORD(wParam) == CBN_SELCHANGE) ||
						 (HIWORD(wParam) == CBN_EDITUPDATE) ){
						TCHAR *writep = PFS->mask->file;

						tstrcpy(PFS->mask->file, PFS->option.wordmatch ?
								StrWordMatchWildCard : StrMaskDlalogWildCard);
						writep += tstrlen(writep);

						if ( HIWORD(wParam) == CBN_EDITUPDATE ){
							GetWindowText((HWND)lParam, writep,
									TSIZEOF(PFS->mask->file) - TSIZEOFSTR(StrWordMatchWildCard) );
						}else{
							SendMessage((HWND)lParam, CB_GETLBTEXT,
									SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0),
											(LPARAM)writep);
						}
						if ( *writep == '\0' ) PFS->mask->file[0] = '\0';
						PFS->mask->attr = GetFileMaskAttr(hDlg);
						if ( MaskEntryMain(PFS->cinfo, PFS->mask, PFS->filename) ){
							SetMessageOnCaption(hDlg, NULL);
						}else{
							SetMessageOnCaption(hDlg, T("bad wildcard attribute"));
							PostMessage(PFS->hEdWnd, WM_PPXCOMMAND, KE_closeUList, 0);
						}
						break;
					}
					break;

				case IDOK:
					GetMaskTextFromEdit(hDlg, IDE_INPUT_LINE, PFS->mask->file, PFS->option.wordmatch);
					PFS->mask->attr = GetFileMaskAttr(hDlg);
					SetCaption(PFS->cinfo);
					EndDialog(hDlg, 1);
					break;

				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;

				case IDB_REF:
					SendMessage(PFS->hEdWnd, WM_PPXCOMMAND,
							K_raw | K_s | K_c | 'I', 0);
					break;

				case IDX_MASK_DIR:
					if ( PFS->mode != DSMD_TEMP ) break; // 保持ソートでは記憶しない
					PFS->option.dirmask = IsDlgButtonChecked(hDlg, IDX_MASK_DIR);
				// IDX_MASK_WORDMATCH/IDX_MASK_RTM へ
				case IDX_MASK_WORDMATCH:
					PFS->option.wordmatch = IsDlgButtonChecked(hDlg, IDX_MASK_WORDMATCH);
				// IDX_MASK_RTM へ
				case IDX_MASK_RTM:
					PFS->option.realtime = IsDlgButtonChecked(hDlg, IDX_MASK_RTM);
					SetCustData(PFS->settingkey, &PFS->option, sizeof(PFS->option));

					FileMaskDialog(hDlg, WM_COMMAND,
							TMAKEWPARAM(IDE_INPUT_LINE, CBN_EDITUPDATE),
							(LPARAM)GetDlgItem(hDlg, IDE_INPUT_LINE) );
					break;

				case IDX_MASK_MODE:
					if ( HIWORD(wParam) == BN_CLICKED ){
						if ( IsTrue(PFS->maskmode) ){ // ファイルマスク
							MaskTargetMenu(hDlg, (HWND)lParam, PFS);
							HideMaskItem(hDlg, PFS);
						}else{ // ファイルマーク
							MarkTargetMenu(hDlg, (HWND)lParam, PFS);
						}
					}
					break;

				case IDQ_GETDIALOGID:
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT, (LONG_PTR)IDD_MASK);
					break;

				case IDB_WILDFORMAT:
					if ( HIWORD(wParam) == BN_CLICKED ){
						ButtonHelpMenu(PFS->cinfo, hDlg, (HWND)lParam, IDE_INPUT_LINE, -1);
					}
					break;
			}
			break;

		default:
			return PPxDialogHelper(hDlg, message, wParam, lParam);
	}
	return TRUE;
}


void BinString(TCHAR *dest, DWORD data, int bits)
{
	int i;

	*dest++ = 'B';
	for ( i = 1 ; i <= bits ; i++ ){
		*dest++ = (TCHAR)((data & (1 << (bits - i))) ? '1':'0');
	}
	*dest = '\0';
}

void SaveSortSettings(HWND hDlg)
{
	TCHAR itemname[MAX_PATH], itemparam[MAX_PATH], attr[16], opt[36];
	XC_SORT xsort;

	tstrcpy(itemname, T("name"));
	if ( tInput(hDlg, MES_TINS, itemname, TSIZEOF(itemname),
			PPXH_GENERAL, PPXH_GENERAL) <= 0 ){
		return;
	}
	GetSortSettings(hDlg, &xsort);
	BinString(attr, xsort.atr, 6);
	BinString(opt, xsort.option, 26);

	wsprintf(itemparam, T("%d,%d,-1,%s,%s"), xsort.mode.dat[0], xsort.mode.dat[1], attr, opt);
	SetCustStringTable(T("MC_sort"), itemname, itemparam, 0);
}

BYTE GetSortID(HWND hDlg, int id)
{
	int index = (int)SendDlgItemMessage(hDlg, id, CB_GETCURSEL, 0, 0);
	int data = (int)SendDlgItemMessage(hDlg, id, CB_GETITEMDATA, index, 0);
	if ( data > 0 ){
		return (BYTE)(data + SORT_COLUMNTYPE - 2);
	}else{
		return sortIDtable[index];
	}
}

void GetSortSettings(HWND hDlg, XC_SORT *xs)
{
	xs->mode.dat[0] = GetSortID(hDlg, IDC_SORT_TYPE1);
	xs->mode.dat[1] = GetSortID(hDlg, IDC_SORT_TYPE2);
	xs->mode.dat[2] = GetSortID(hDlg, IDC_SORT_TYPE3);
	xs->mode.dat[3] = -1;
	xs->option =
		(IsDlgButtonChecked(hDlg, IDX_SORT_CASE) ?	0 : SORTE_IGNORECASE) |
		(IsDlgButtonChecked(hDlg, IDX_SORT_KANA) ?	SORTE_KANATYPE : 0) |
		(IsDlgButtonChecked(hDlg, IDX_SORT_SPACE) ?	SORTE_NONSPACE : 0) |
		(IsDlgButtonChecked(hDlg, IDX_SORT_SYM) ?	SORTE_SYMBOLS : 0) |
		(IsDlgButtonChecked(hDlg, IDX_SORT_WIDE) ?	SORTE_WIDE : 0) |
		(IsDlgButtonChecked(hDlg, IDX_SORT_NUM) ?	SORTE_NUMBER : 0) |
		(IsDlgButtonChecked(hDlg, IDX_SORT_LE) ?	SORTE_LASTENTRY : 0) |
		(IsDlgButtonChecked(hDlg, IDX_SORT_SS) ?	SORT_STRINGSORT : 0);
	xs->atr = GetAttibuteSettings(hDlg) | FILE_ATTRIBUTE_LABEL;
}

void SendDlgSortCB(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	SendDlgItemMessage(hDlg, IDC_SORT_TYPE1, msg, wParam, lParam);
	SendDlgItemMessage(hDlg, IDC_SORT_TYPE2, msg, wParam, lParam);
	SendDlgItemMessage(hDlg, IDC_SORT_TYPE3, msg, wParam, lParam);
}

void ReverseSortType(HWND hDlg, int typeID)
{
	int index = (int)SendDlgItemMessage(hDlg, typeID, CB_GETCURSEL, 0, 0);

	if ( (index >= 1) && (index <= 10) ){
		index += 10;
	}else if ( (index >= 11) && (index <= 20) ){
		index -= 10;
	}
	SendDlgItemMessage(hDlg, typeID, CB_SETCURSEL, index, 0);
}

//  --------------------------------------------------------------
INT_PTR CALLBACK SortDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch (iMsg){
		case WM_INITDIALOG: {
			int CommentID, index;
			PPC_APPINFO *cinfo;
			XC_SORT *xc;

			CenterWindow(hDlg);
			LocalizeDialogText(hDlg, IDD_SORT);
			xc = ((PPCSORTDIALOGPARAM *)lParam)->xc;
			SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)xc);
			for ( index = 0 ; SortItems[index] ; index++ ){
				SendDlgSortCB(hDlg, CB_ADDSTRING,
						0, (LPARAM)MessageText(SortItems[index]));
			}

			cinfo = ((PPCSORTDIALOGPARAM *)lParam)->cinfo;
			for ( CommentID = 1 ; CommentID <= 10 ; CommentID++ ){
				TCHAR buf[100];

				if ( !((cinfo->UseCommentsFlag) & (1 << CommentID)) ){
					wsprintf(buf, T("COMMENTEVENT%d"), CommentID);
					if ( IsExistCustTable(T("KC_main"), buf) == FALSE ) continue;
				}
				wsprintf(buf, T("%d Ex Comment #%d"), CommentID, CommentID);
				SendDlgSortCB(hDlg, CB_ADDSTRING, 0, (LPARAM)buf );
				SendDlgSortCB(hDlg, CB_SETITEMDATA, (WPARAM)index, (LPARAM)(CommentID * 2));
				wsprintf(buf, T("%d R)Ex Comment #%d"), CommentID, CommentID);
				SendDlgSortCB(hDlg, CB_ADDSTRING, 0, (LPARAM)buf );
				SendDlgSortCB(hDlg, CB_SETITEMDATA, (WPARAM)(index + 1), (LPARAM)(CommentID * 2 + 1));
				index += 2;
			}
			{
				int type;

				type = xc->mode.dat[0] + 1;
				if ( type == 0 ) type = 1;
				SendDlgItemMessage(hDlg, IDC_SORT_TYPE1, CB_SETCURSEL, type, 0);
			}
			SendDlgItemMessage(hDlg, IDC_SORT_TYPE2, CB_SETCURSEL,
				(WPARAM)(xc->mode.dat[1] + 1), 0);
			SendDlgItemMessage(hDlg, IDC_SORT_TYPE3, CB_SETCURSEL,
				(WPARAM)(xc->mode.dat[2] + 1), 0);
			CheckDlgButton(hDlg, IDX_SORT_CASE,
					!(xc->option & SORTE_IGNORECASE));
			CheckDlgButton(hDlg, IDX_SORT_KANA,
					(xc->option & SORTE_KANATYPE));
			CheckDlgButton(hDlg, IDX_SORT_SPACE,
					(xc->option & SORTE_NONSPACE));
			CheckDlgButton(hDlg, IDX_SORT_SYM,
					(xc->option & SORTE_SYMBOLS));
			CheckDlgButton(hDlg, IDX_SORT_WIDE,
					(xc->option & SORTE_WIDE));
			CheckDlgButton(hDlg, IDX_SORT_NUM,
					(xc->option & SORTE_NUMBER));
			CheckDlgButton(hDlg, IDX_SORT_LE,
					(xc->option & SORTE_LASTENTRY));
			CheckDlgButton(hDlg, IDX_SORT_SS,
					(xc->option & SORT_STRINGSORT));

			SetAttibuteSettings(hDlg, xc->atr);
			return TRUE;
		}
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDOK:
					GetSortSettings(hDlg,
							(XC_SORT *)GetWindowLongPtr(hDlg, DWLP_USER));
					EndDialog(hDlg, 1);
					break;

				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;

				case IDB_SAVE:
					SaveSortSettings(hDlg);
					break;

				case IDX_SORT_REV1:
					ReverseSortType(hDlg, IDC_SORT_TYPE1);
					break;
				case IDX_SORT_REV2:
					ReverseSortType(hDlg, IDC_SORT_TYPE2);
					break;
				case IDX_SORT_REV3:
					ReverseSortType(hDlg, IDC_SORT_TYPE3);
					break;

				case IDHELP:
					return PPxDialogHelper(hDlg, WM_HELP, wParam, lParam);

				case IDQ_GETDIALOGID:
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT, (LONG_PTR)IDD_SORT);
					break;
			}
			break;

		default:
			return PPxDialogHelper(hDlg, iMsg, wParam, lParam);
	}
	return TRUE;
}

