/*-----------------------------------------------------------------------------
	Paper Plane cUI			Combo Window ToolBar
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <commctrl.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPCOMBO.H"
#pragma hdrstop

#define TOOLBAR_CMDID 0x1000

#define _TBSTYLE_CUSTOMERASE 0x2000

void ComboCreateToolBar(HWND hWnd)
{
	RECT box;
	UINT ID = TOOLBAR_CMDID;
	HWND hBarWnd;

	if ( UseCCDrawBack == 0 ){
		UseCCDrawBack = PPxCommonExtCommand(K_DRAWCCBACK, 0) ? 2 : 1;
	}
	hBarWnd = CreateToolBar(&thGuiWork, hWnd, &ID, T("B_cdef"), PPcPath,
			(UseCCDrawBack > 1) ? _TBSTYLE_CUSTOMERASE : 0);
	Combo.ToolBar.hWnd = hBarWnd;
	if ( hBarWnd != NULL ){
		GetWindowRect(hBarWnd, &box);
		Combo.ToolBar.Height = box.bottom - box.top;
	}
}

void ComboToolbarCommand(int id, int orcode)
{
	RECT box;
	POINT pos;
	TCHAR *cmd;

	SendMessage(Combo.ToolBar.hWnd, TB_GETRECT, (WPARAM)id, (LPARAM)&box);
	pos.x = box.left;
	pos.y = box.bottom;
	ClientToScreen(Combo.ToolBar.hWnd, &pos);

	cmd = GetToolBarCmd(Combo.ToolBar.hWnd, &thGuiWork, id);
	if ( cmd == NULL ) return;
	if ( orcode ){
		WORD key;

		key = *(WORD *)(cmd + 1) | (WORD)orcode;
		if ( key == (K_s | K_raw | K_bs) ){
			key = K_raw | K_c | K_bs;
		}
		SendMessage(hComboFocus, WM_PPXCOMMAND,
				TMAKELPARAM(K_POPOPS, PPT_SAVED), TMAKELPARAM(pos.x, pos.y));
		SendMessage(hComboFocus, WM_PPXCOMMAND, key, 0);
	}else{
		int i;

		i = GetComboShowIndex(hComboFocus);
		if ( i < 0 ) return;
		if ( Combo.base[Combo.show[i].baseNo].cinfo == NULL ) return;
		SendMessage(hComboFocus, WM_PPCEXEC, (WPARAM)cmd, TMAKELPARAM(pos.x, pos.y));
	}
}
