/*-----------------------------------------------------------------------------
	Paper Plane xUI	customizer									AddonÉVÅ[Ég
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPCUST.H"
#pragma hdrstop

const SUSIE_DLL *susiedll_list = NULL;
BYTE *sustrings;
int susiedll_items;
CONFIGURATIONDLG susieconfig = NULL;
int SusieIndex = -1;

void AddOnSusieSetting(HWND hDlg)
{
	if ( susieconfig == NULL ) return;
	susieconfig(hDlg, 1);
}

void AddOnGetSusieExt(HWND hDlg)
{
	const SUSIE_DLL *sudll;
	char buf[VFPS];
	int index = 2;
	size_t offset = 0;

	if ( (susiedll_list == NULL) || (SusieIndex < 0) || (SusieIndex >= susiedll_items) ){
		return;
	}
	sudll = susiedll_list + SusieIndex;
	if ( sudll->hadd == NULL ) return;

	buf[0] = '\0';
	if ( sudll->GetPluginInfo != NULL ){
		for (;;){
			if ( sudll->GetPluginInfo(index, buf + offset, (int)sizeof(buf) - offset - 1) <= 0 ){
				buf[offset] = '\0';
				break;
			}
			buf[sizeof(buf) - 1] = '\0';
			if ( buf[offset] == '\0' ) break;
			if ( offset != 0 ) buf[offset - 1] = ';';
			offset += strlen(buf + offset);
			if ( buf[offset - 1] == ';' ){
				buf[offset - 1] = '\0';
			}else{
				offset++;
			}
			if ( offset >= (VFPS - 64) ) break;
			index += 2;
		}
	}
	SendDlgItemMessageA(hDlg, IDE_AOSMASK, WM_SETTEXT, 0, (LPARAM)buf);
	EnableDlgWindow(hDlg, IDB_ADDSETEXT, TRUE);
}

void AddonListSusie(HWND hDlg)
{
	const SUSIE_DLL *sudll;
	int i;

	susiedll_items = VFSGetSusieList(&susiedll_list, &sustrings);
	SendDlgItemMessage(hDlg, IDL_AOSUSIE, LB_RESETCONTENT, 0, 0);
	sudll = susiedll_list;
	for ( i = 0 ; i < susiedll_items ; i++, sudll++ ){
		TCHAR *p;

		p = (TCHAR *)(sustrings + sudll->DllNameOffset);
		SendDlgItemMessage(hDlg, IDL_AOSUSIE, LB_ADDSTRING, 0, (LPARAM)p);
	}
}

void AddonSelectSusie(HWND hDlg)
{
	const SUSIE_DLL *sudll;
	char buf[VFPS];
	SUSIE_DLLSTRINGS sp;

	if ( (susiedll_list == NULL) || (SusieIndex < 0) || (SusieIndex >= susiedll_items) ){
		return;
	}
	sudll = susiedll_list + SusieIndex;

	sp.filemask[0] = '\0';
	sp.flags = VFSSUSIE_BMP | VFSSUSIE_ARC;
	GetCustTable(T("P_susie"), (TCHAR *)(sustrings + sudll->DllNameOffset),
			&sp, sizeof(DWORD) + VFPS);

	strcpy(buf, "* Plug-in load error *");
	if ( (sudll->hadd != NULL) && (sudll->GetPluginInfo != NULL) ){
		strcpy(buf, "* GetPluginInfo error *");
		sudll->GetPluginInfo(1, buf, TSIZEOFA(buf));
	}
	SendDlgItemMessageA(hDlg, IDE_AOSINFO, WM_SETTEXT, 0, (LPARAM)buf);
	CheckDlgButton(hDlg, IDX_AOSUSE, sp.flags & (VFSSUSIE_BMP | VFSSUSIE_ARC));
	CheckDlgButton(hDlg, IDX_AOSDETECT, !(sp.flags & VFSSUSIE_NOAUTODETECT) );
	SendDlgItemMessage(hDlg, IDE_AOSMASK, WM_SETTEXT, 0, (LPARAM)sp.filemask);
	susieconfig = (sudll->hadd == NULL) ? NULL :
		(CONFIGURATIONDLG)GetProcAddress( sudll->hadd, "ConfigurationDlg" );
	EnableDlgWindow(hDlg, IDB_AOSSETTING, (susieconfig != NULL));
	EnableDlgWindow(hDlg, IDB_ADDSETEXT, FALSE);
}

void AddonSaveSusie(HWND hDlg)
{
	const SUSIE_DLL *sudll;
	SUSIE_DLLSTRINGS sp;

	if ( (susiedll_list == NULL) || (SusieIndex < 0) || (SusieIndex >= susiedll_items) ){
		return;
	}
	sudll = susiedll_list + SusieIndex;
	sp.flags = IsDlgButtonChecked(hDlg, IDX_AOSUSE) ?
					(VFSSUSIE_BMP | VFSSUSIE_ARC) : 0;
	if ( ! IsDlgButtonChecked(hDlg, IDX_AOSDETECT) ){
		setflag(sp.flags, VFSSUSIE_NOAUTODETECT);
	}
	GetControlText(hDlg, IDE_AOSMASK, sp.filemask, VFPS);
	SetCustTable(T("P_susie"), (TCHAR *)(sustrings + sudll->DllNameOffset),
			&sp, sizeof(DWORD) + TSTRSIZE(sp.filemask));
	Changed(hDlg);
	EnableDlgWindow(hDlg, IDB_ADDSETEXT, FALSE);
}

#pragma argsused
INT_PTR CALLBACK AddonPage(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	UnUsedParam(lParam);

	switch (msg){
		case WM_DESTROY:
			VFSOff();
			return DlgSheetProc(hDlg, msg, wParam, lParam, IDD_ADDON);

		case WM_INITDIALOG:
			InitPropSheetsUxtheme(hDlg);
			VFSOn(VFS_DIRECTORY | VFS_BMP | VFS_ALL);
			SendDlgItemMessage(hDlg, IDE_AOSMASK, EM_LIMITTEXT, MAX_PATH - 1, 0);
			SendDlgItemMessage(hDlg, IDL_AOSUSIE, LB_ADDSTRING, 0, (LPARAM)T("loading..."));
			InvalidateRect(hDlg, NULL, TRUE);
			UpdateWindow(hDlg);
			PostMessage(hDlg, WM_COMMAND, K_FIRSTCMD, 0);
			return FALSE;

		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDL_AOSUSIE:
					if ( GetListCursorIndex(wParam, lParam, &SusieIndex) > 0 ){
						AddonSelectSusie(hDlg);
					}
					break;
				case IDB_AOSSETTING:
					AddOnSusieSetting(hDlg);
					break;
				case IDB_ADDDEFEXT:
					AddOnGetSusieExt(hDlg);
					break;
				case IDE_AOSMASK:
					if ( HIWORD(wParam) == EN_CHANGE ){
						EnableDlgWindow(hDlg, IDB_ADDSETEXT, TRUE);
					}
					break;
				case IDB_ADDSETEXT:
				case IDX_AOSUSE:
				case IDX_AOSDETECT:
					AddonSaveSusie(hDlg);
					break;
				case K_FIRSTCMD:
					AddonListSusie(hDlg);
					break;
			}
			break;

		case WM_NOTIFY:
			if ( ((NMHDR *)lParam)->code == PSN_SETACTIVE ){
				InitWndIcon(hDlg, IDE_AOSINFO);
			}
		// default Ç÷
		default:
			return DlgSheetProc(hDlg, msg, wParam, lParam, IDD_ADDON);
	}
	return TRUE;
}
