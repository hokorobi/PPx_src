/*-----------------------------------------------------------------------------
	Paper Plane cUI														Sub
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <winioctl.h>
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPCOMBO.H"
#pragma hdrstop

#define PPcEnumInfoFunc(func, str, work) ((work)->buffer = str, cinfo->info.Function(&cinfo->info, func, work))

const TCHAR LFMARK_STR[] = T(",M:1");
const TCHAR LFSIZE_STR[] = T(",Size,");
const TCHAR LFCREATE_STR[] = T(",Create,");
const TCHAR LFLASTWRITE_STR[] = T(",Last Write,");
const TCHAR LFLASTACCESS_STR[] = T(",Last Access,");
#define CopyAndSkipString(dest, string) memcpy(dest, string, SIZEOFTSTR(string)); dest += TSIZEOFSTR(string);

#define GFSIZE 0x1000

const TCHAR DFO_title[] = MES_TDFO;
const int DispAttributeTable[] = {DE_ATTRTABLE};
#define CFMT_OLDHEADERSIZE (sizeof(DWORD) * 5)

int RefreshAfterListModes[] = {K_ENDATTR, K_ENDCOPY, K_ENDDEL};

typedef int (__stdcall *CREATEPICTURE)(LPCTSTR filepath, unsigned int flag, HANDLE *pHBInfo, HANDLE *pHBm, void *lpInfo, void *progressCallback, LONG_PTR lData);
HMODULE hTSusiePlguin = NULL;
CREATEPICTURE CreatePicture;

#ifdef _WIN64
	#ifdef _M_ARM64
		#define TGDIPNAME T("iftgdip.spha")
		#define TWICNAME T("iftwic.spha")
	#else
		#define TGDIPNAME T("iftgdip.sph")
		#define TWICNAME T("iftwic.sph")
	#endif
#else
	#ifdef _M_ARM
		#define TGDIPNAME T("iftgdip.spia")
		#define TWICNAME T("iftwic.spia")
	#else
		#define TGDIPNAME T("iftgdip.spi")
		#define TWICNAME T("iftwic.spi")
	#endif
#endif

#ifdef UNICODE
	#define TGDIPCREATENAME "CreatePictureW"
#else
	#define TGDIPCREATENAME "CreatePicture"
#endif

#define MAXCLIPMEMO 22

#define PPCLC_RUNCUST 2
#define PPCLC_WINDOW 3
#define PPCLC_TREE 4
#define PPCLC_MENU 5
#define PPCLC_LOG 6
#define PPCLC_JOBLIST 7
#define PPCLC_TOUCH 8
#define PPCLC_XWIN 32 // ～ +32
#define PPCLC_XCOMBOS 64 // ～ +32
#define PPCLC_MAX 96

const TCHAR lstr_title[] = MES_LYTI;
const TCHAR lstr_menu[] = MES_LYME;
const TCHAR lstr_status[] = MES_LYSL;
const TCHAR lstr_info[] = MES_LYIL;
const TCHAR lstr_toolbar[] = MES_LYST;
const TCHAR lstr_address[] = MES_LYSA;
const TCHAR lstr_caption[] = MES_LYCA;
const TCHAR lstr_header[] = MES_LYHA;
const TCHAR lstr_tree[] = MES_LYTR;
const TCHAR lstr_scroll[] = MES_LYSC;
const TCHAR lstr_swapscroll[] = MES_LYSS;
const TCHAR lstr_docktop[] = MES_LYDT;
const TCHAR lstr_dockbottom[] = MES_LYDB;
const TCHAR lstr_windowmenu[] = MES_LYWM;
const TCHAR lstr_log[] = MES_LYLG;
const TCHAR lstr_cust[] = MES_LYOT;
const TCHAR lstr_joblist[] = MES_LYJL;

typedef struct {
	HMENU hMenu;
	DWORD *index;
	ThSTRUCT *TH;
} AppendMenuS;

int GetEntryDepth(const TCHAR *src, const TCHAR **last)
{
	int depth = 0;
	const TCHAR *rp, *tp;

	rp = VFSGetDriveType(src, NULL, NULL);	// ドライブ指定をスキップ
	if ( rp == NULL ) rp = src;	// ドライブ指定が無い
	if ( *rp != '\0' ){ // root なら *rp == 0
		tp = FindPathSeparator( ((*rp == '\\') || (*rp == '/')) ? rp + 1 : rp);
		if ( tp != NULL ){
			do {
				depth++;
				rp = tp;
				tp = FindPathSeparator(rp + 1);
			} while( tp != NULL);
		}
	}
	if ( last != NULL ) *last = ((*rp == '\\') || (*rp == '/')) ? rp : NULL;
	return depth;
}

// 現在ディレクトリ/カーソル位置をヒストリに保存 ------------------------------
void SetPPcDirPos(PPC_APPINFO *cinfo)
{
	SavePPcDir(cinfo, FALSE);
	cinfo->OrgPath[0] = '\0';
}

// 現在ディレクトリ/カーソル位置をヒストリに保存・履歴順を更新 ----------------
void SavePPcDir(PPC_APPINFO *cinfo, BOOL newdir)
{
	int tmp[4];
	BYTE *hist;
	TCHAR buf[VFPS], buf2[VFPS], *path;
	WORD histdatasize;

	if ( cinfo->e.cellIMax <= 0 ) return; // エントリが無い

	tmp[0] = cinfo->e.cellN;
	tmp[1] = cinfo->cellWMin;

	if ( CEL(tmp[0]).type == ECT_SYSMSG ){
		if ( CEL(tmp[0]).f.nFileSizeLow != ERROR_FILE_NOT_FOUND ){
			return; // エラー
		}
		// ERROR_FILE_NOT_FOUND のときディレクトリ自体はあるので続行
	}
	if ( CEL(tmp[0]).attr & ECA_THIS ) tmp[0]++; // 「.」上にはしない

	// 書庫内書庫
	if ( cinfo->OrgPath[0] ){
		path = cinfo->OrgPath;
	}else{
		path = cinfo->path;
	}
	// 書庫内dir
	if ( cinfo->UseArcPathMask != ARCPATHMASK_OFF ){
		VFSFullPath(buf, cinfo->ArcPathMask, path);
		path = buf;
	}
	// Listfile(ベース指定あり)
	if ( (cinfo->e.Dtype.mode == VFSDT_LFILE) &&
		 (cinfo->e.Dtype.BasePath[0] != '\0') &&
		 ((tstrlen(path) + tstrlen(cinfo->e.Dtype.BasePath) + 3) < VFPS) ){
		wsprintf(buf2, T(":<%s>%s"), cinfo->e.Dtype.BasePath, cinfo->path);
		path = buf2;
	}
	UsePPx();
	hist = (BYTE *)SearchHistory(PPXH_PPCPATH, path);
	if ( hist == NULL ){
		histdatasize = 0;
	}else{
		histdatasize = GetHistoryDataSize(hist);
	}
		// 位置データの更新のみで済む？
	if ( (histdatasize >= (sizeof(int) * 2)) && (newdir == FALSE) ){
		memcpy(GetHistoryData(hist), &tmp, sizeof(int) * 2);
	}else{ // 書き込み(履歴順の変化)が必要
		if ( histdatasize > (sizeof(int) * 2) ){ // サイズキャッシュ有り
			memcpy(&tmp[2], GetHistoryData(hist) + sizeof(int) * 2, sizeof(int) * 2);
		}else{
			histdatasize = sizeof(int) * 2;
		}
		WriteHistory(PPXH_PPCPATH, path, histdatasize, &tmp);
	}
	FreePPx();
}

// マークファイル取得の初期化 -------------------------------------------------
void InitEnumMarkCell(PPC_APPINFO *cinfo, int *work)
{
	if ( cinfo->e.markC == 0 ){			// マーク無し
		if ( CEL(cinfo->e.cellN).state >= ECS_NORMAL ){
			*work = CELLENUMID_ONCURSOR;
		}else{				// SysMsgやDeletedなのでだめ
			*work = CELLENUMID_END;
		}
	}else{
		*work = cinfo->e.markTop;
	}
}

// マークファイルを順次取得 ---------------------------------------------------
ENTRYCELL *EnumMarkCell(PPC_APPINFO *cinfo, int *work)
{
	int cell;

	if ( *work == CELLENUMID_ONCURSOR ){		// マーク無し
		*work = CELLENUMID_END;
		return &CEL(cinfo->e.cellN);
	}
	if ( *work < 0 ) return NULL;
	cell = *work;
	*work = CELdata(*work).mark_fw;
	while ( *work >= 0 ){
		if ( CELdata(*work).state >= ECS_NORMAL ) break;
							// SysMsgやDeletedなのでだめ→次へ
		cell = *work;
		*work = CELdata(*work).mark_fw;
	}
	return &CELdata(cell);
}

void GetCellRightRanges(PPC_APPINFO *cinfo, CELLRIGHTRANGES *ranges)
{
	int RightEnd;
	ranges->TouchWidth = 0;

	//==== 右側反応端を決定
	if ( TouchMode & TOUCH_LARGEWIDTH ){
		ranges->TouchWidth = cinfo->FontDPI;
		if ( ranges->TouchWidth != 0 ){
			ranges->TouchWidth = (ranges->TouchWidth * 120) >> 8; // 12.0mm ( 120 / 254 ) の近似値
			// iOS:7mm Android:9mm(7-10mm)
		}
	}
//	if ( RightEnd > cinfo->wnd.Area.cx ) RightEnd = cinfo->wnd.Area.cx;
	ranges->RightBorder = cinfo->fontX;
	RightEnd = cinfo->BoxEntries.right;
	if ( (RightEnd - cinfo->BoxEntries.left) > (cinfo->fontX * 16) ){
		ranges->RightBorder *= 2;
		if ( ranges->RightBorder < ranges->TouchWidth ){
			ranges->RightBorder = ranges->TouchWidth;
		}
	}
	ranges->RightBorder = RightEnd - ranges->RightBorder;
	// 右側反応端からのTail位置を決定
	ranges->TailRightOffset = cinfo->fontX + (cinfo->fontX / 2);
	ranges->TailWidth = X_stip[TIP_TAIL_WIDTH];
}

// クライアント座標から、項目の種類を求める -----------------------------------
int GetItemTypeFromPoint(PPC_APPINFO *cinfo, POINT *pos, ENTRYINDEX *ItemNo)
{
	ENTRYINDEX cell = -1;
	int rc;

	int x, y;

	x = pos->x;
	y = pos->y;
	if ( (y < 0) || (x < 0) ||
		 (x >= cinfo->wnd.Area.cx) || (y >= cinfo->wnd.Area.cy) ){
		return PPCR_UNKNOWN;	// 範囲外
	}

									// Path / Status line =================
	if ( y < cinfo->BoxStatus.top ) return PPCR_PATH;	// Path line
	if ( y < cinfo->BoxStatus.bottom ) return PPCR_STATUS;	// Status line

	if ( y < cinfo->BoxInfo.bottom ){	// 情報行 =================
		if ( x < cinfo->BoxInfo.left + cinfo->iconR ){ // Info icon
			rc = PPCR_INFOICON;
			cell = cinfo->e.cellN;
		}else{							// Info line / Hidden Menu
			int items;

			x = (x - (cinfo->BoxInfo.left + cinfo->iconR)) / cinfo->fontX;
			items = (cinfo->HiddenMenu.item + 1) >> 1;
			if ( (x < (5 * items - 1)) && !((TouchMode & TOUCH_DISABLEHIDDENMENU) )){	// Self Popup Menu
				rc = PPCR_MENU;
				cell = (x / 5) + ((y - (cinfo->BoxInfo.bottom - (cinfo->fontY * 2))) / cinfo->fontY) * items;
				if ( cell < -1 ) return PPCR_INFOTEXT;
			}else{							// Info line
				return PPCR_INFOTEXT;
			}
		}
	}else{								// cell 一覧 ==========================
		x -= cinfo->BoxEntries.left;
#if FREEPOSMODE
		{
			ENTRYINDEX i;
			int tx, ty;
			POINT *cpos;

			// ●1.1x -1 を足す必要がある問題を調べること！
			// ●ソートをしたとき、cinfo->FreePosEntries = 0、NOFREEPOS の再設定が必要

			tx = x + (cinfo->cellWMin / cinfo->cel.Area.cy - 1) * cinfo->cel.Size.cx;
			ty = y + (cinfo->cellWMin % cinfo->cel.Area.cy - 1) * cinfo->cel.Size.cy - cinfo->BoxEntries.top;
//			ty = y + CalcFreePosOffY(cinfo);
			for ( i = 0 ; i < cinfo->FreePosEntries ; i++ ){
				cpos = &CEL(cinfo->FreePosList[i].index).pos;
				if ( (cpos->x >= tx) && (cpos->x < (tx + cinfo->cel.Size.cx)) &&
					 (cpos->y >= ty) && (cpos->y < (ty + cinfo->cel.Size.cy)) ){
					if (ItemNo != NULL) *ItemNo = cinfo->FreePosList[i].index;
					return PPCR_CELLTEXT;
				}
			}
		}
#endif
		y = ( y - cinfo->BoxEntries.top ) / cinfo->cel.Size.cy;
		if ( x < 0 ) {
			rc = PPCR_UNKNOWN;
		}else if ( y < cinfo->cel.Area.cy ){
			CELLRIGHTRANGES ranges;
			int xd;

			GetCellRightRanges(cinfo, &ranges);
			rc = PPCR_CELLBLANK;
			//==== 検知対象 cell を決定
			xd = x / cinfo->cel.Size.cx;
			cell = cinfo->cellWMin + y + xd * cinfo->cel.Area.cy;
			x = x - (xd * cinfo->cel.Size.cx);

			// 右端は、セルの範囲であっても空欄扱いにする
			if ( (x > (cinfo->fontX * 3)) && (pos->x >= ranges.RightBorder) ){
				if ( ItemNo != NULL ) *ItemNo = -1;
				return PPCR_CELLBLANK;
			}
#if FREEPOSMODE
	#define FREEPOSCONDITION(cell) && (CEL(cell).pos.x == NOFREEPOS)
#else
	#define FREEPOSCONDITION(cell)
#endif
			if ( (cell < cinfo->e.cellIMax) && (cell >= cinfo->cellWMin) FREEPOSCONDITION(cell) ){
				int TailRight = cinfo->cel.Size.cx - ranges.TailRightOffset;

				if ( xd >= (cinfo->cel.VArea.cx - 1) ){ // 右端用の調整
					TailRight = ranges.RightBorder % cinfo->cel.Size.cx;
				}

				if ( (x > (TailRight - ranges.TailWidth)) && (x < TailRight) ){
					rc = PPCR_CELLTAIL;
				}else if ( (DWORD)x < cinfo->CellNameWidth ){ // マーク部分の判定
					BYTE *fmtp;
					int markX;

					// 書式頭出し
					fmtp = cinfo->celF.fmt;
					if ( (*fmtp == DE_WIDEV) || (*fmtp == DE_WIDEW) ){
						fmtp += DE_WIDEV_SIZE;
					}

					if ( *fmtp == DE_BLANK ){
						int blankX = 0;
						do {
							blankX += cinfo->fontX;
							fmtp++;
						} while ( *fmtp == DE_BLANK );
						x -= blankX;
						if ( x < 0 ){
							if ( ItemNo != NULL ) *ItemNo = cell;
							return PPCR_CELLBLANK;
						}
					}
					// マークチェック
					if ( (*fmtp == DE_ICON) || (*fmtp == DE_CHECK) || (*fmtp == DE_CHECKBOX)){
						markX = cinfo->fontX * 2 + MARKOVERX;
						fmtp += DE_ICON_SIZE; // DE_CHECK_SIZE / DE_CHECKBOX_SIZE
					}else if ( *fmtp == DE_ICON2 ){
						markX = *(fmtp + 1) + ICONBLANK + MARKOVERX;
						fmtp += DE_ICON2_SIZE;
					}else{
						markX = cinfo->fontX + MARKOVERX;
					}
					if ( markX < ranges.TouchWidth ) markX = ranges.TouchWidth;

					if ( x < markX ){
						rc = PPCR_CELLMARK;
					}else{ // エントリチェック
						if ( (XC_limc == 2) && (CEL(cell).f.cFileName[0] != '>') ){
							if ( *fmtp == DE_MARK ) fmtp += DE_MARK_SIZE;
							if ( (*fmtp == DE_LFN) || (*fmtp == DE_SFN) ){
								int namewidth;

								if ( UsePFont ){
									HDC hDC = GetDC(cinfo->info.hWnd);
									SIZE boxsize;
									HGDIOBJ hOldFont;
									hOldFont = SelectObject(hDC, cinfo->hBoxFont);

									GetTextExtentPoint32(hDC,
											CEL(cell).f.cFileName,
											CEL(cell).ext, &boxsize);
									namewidth = boxsize.cx + ((cinfo->fontX * 3) / 2) + MARKOVERX;
									SelectObject(hDC, hOldFont);	// フォント
									ReleaseDC(cinfo->info.hWnd, hDC);
								}else{
									namewidth = (CEL(cell).ext + 1) * cinfo->fontX + MARKOVERX;
								}
								if ( namewidth < (cinfo->fontX * 2) ){
									namewidth = (cinfo->fontX * 2);
								}
								if ( namewidth < ranges.TouchWidth ){
									namewidth = ranges.TouchWidth;
								}
								if ( x < namewidth ) rc = PPCR_CELLTEXT;
							}else{
								rc = PPCR_CELLTEXT;
							}
						}else{
							rc = PPCR_CELLTEXT;
						}
					}
				}
			}
		}else{
			rc = PPCR_CELLBLANK;
		}
	}
	if ( ItemNo != NULL ) *ItemNo = cell;
	return rc;
}

HWND USEFASTCALL GetJoinWnd(PPC_APPINFO *cinfo)
{
	HWND hPairWnd;

	hPairWnd = PPcGetWindow(cinfo->RegNo, CGETW_PAIR);
	if ( hPairWnd == BADHWND ) hPairWnd = NULL;
	return hPairWnd;
}

HWND USEFASTCALL GetPairWnd(PPC_APPINFO *cinfo)
{
	HWND hPairWnd;

	if ( cinfo->combo ){
		hPairWnd = (HWND)SendMessage(cinfo->hComboWnd, WM_PPXCOMMAND,
				KCW_getpairwnd, (LPARAM)cinfo->info.hWnd);
		if ( hPairWnd != NULL ) return hPairWnd;
	}
	hPairWnd = PPcGetWindow(cinfo->RegNo, CGETW_PAIR);
	if ( hPairWnd != NULL ) return hPairWnd;
	if ( !(cinfo->swin & SWIN_JOIN) ){
		hPairWnd = PPcGetWindow(cinfo->RegNo, CGETW_PREV);
		if ( hPairWnd != BADHWND ) return hPairWnd;
	}
	return NULL;
}

void USEFASTCALL GetPairPath(PPC_APPINFO *cinfo, TCHAR *path)
{
	if ( cinfo->combo ){
		*(HWND *)path = cinfo->info.hWnd;
		if ( SENDCOMBO_OK == SendMessage(cinfo->hComboWnd,
				WM_PPXCOMMAND, KCW_getpath, (LPARAM)path) ){
			return;
		}
	}
	PPcOldPath(cinfo->RegNo, cinfo->swin & SWIN_JOIN, path);
	return;
}

void SetReportTextMain(HWND hLogWnd, const TCHAR *text)
{
	DWORD lines;
	DWORD lP, wP;

	if ( X_log & B14 ) XMessage(NULL, NULL, XM_INFOlog, T("%s"), text);

	lines = SendMessage(hLogWnd, EM_GETLINECOUNT, 0, 0);
	if ( lines > LOGLINES_MAX ){
		SendMessage(hLogWnd, EM_SETSEL,
				0, SendMessage(hLogWnd, EM_LINEINDEX, (WPARAM)LOGLINES_DELETE, 0));
		SendMessage(hLogWnd, EM_REPLACESEL, 0, (LPARAM)NilStr);
		lines = SendMessage(hLogWnd, EM_GETLINECOUNT, 0, 0);
	}
	SendMessage(hLogWnd, EM_SETSEL, EC_LAST, EC_LAST);
	SendMessage(hLogWnd, EM_GETSEL, (WPARAM)&wP, (LPARAM)&lP);
	if ( (DWORD)SendMessage(hLogWnd, EM_LINEINDEX, (WPARAM)lines-1, 0) < lP ){
		SendMessage(hLogWnd, EM_REPLACESEL, 0, (LPARAM)T("\r\n"));
	}

	SendMessage(hLogWnd, EM_REPLACESEL, 0, (LPARAM)text);
	SendMessage(hLogWnd, EM_SCROLL, SB_LINEDOWN, 0);
	SendMessage(hLogWnd, EM_SETMODIFY, FALSE, 0);
}

void SetReportText(const TCHAR *text)
{
	if ( Combo.hWnd == NULL ){
		if ( (hCommonLog == NULL) || (IsWindow(hCommonLog) == FALSE) ){
			HWND hWnd;

			hWnd = PPEui(PPcGetWindow(0, CGETW_GETFOCUS), MES_TPCL, NilStr);
			hCommonLog = (HWND)SendMessage(hWnd, WM_PPXCOMMAND, KE_getHWND, 0);
		}
		if ( text != NULL ) SetReportTextMain(hCommonLog, text);
	}else{
		SendMessage(Combo.hWnd, WM_PPXCOMMAND, K_WINDDOWLOG, (LPARAM)text);
	}
}

// １行メッセージ表示の設定 ---------------------------------------------------
void USEFASTCALL SetPopMsg(PPC_APPINFO *cinfo, ERRORCODE err, const TCHAR *msg)
{
	TCHAR msgbuf[CMDLINESIZE];

	if ( (err > POPMSG_MSG) && (err <= POPMSG_GETLASTERROR) ){
		TCHAR *msgptr;

		if ( err == POPMSG_GETLASTERROR ) err = PPERROR_GETLASTERROR;
		if ( msg != NULL ){
			tstrlimcpy(msgbuf, MessageText(msg), CMDLINESIZE);
			msgptr = msgbuf + tstrlen(msgbuf);
			*msgptr++ = ':';
		}else{
			msgptr = msgbuf;
		}
		PPErrorMsg(msgptr, err);
		err = POPMSG_MSG;
		msg = msgbuf;
	}

	if ( !(err & POPMSG_NOLOGFLAG) && (Combo.Report.hWnd != NULL) ){
		if ( msg != NULL ){
			tstrlimcpy(msgbuf, MessageText(msg), CMDLINESIZE);
		}else{
			PPErrorMsg(msgbuf, err);
		}
		SetReportText(msgbuf);
		return;
	}

	if ( msg != NULL ){
		tstrlimcpy(cinfo->PopMsgStr, MessageText(msg), CMDLINESIZE);
	}else{
		PPErrorMsg(cinfo->PopMsgStr, err);
	}
	RefleshStatusLine(cinfo);

	if ( err < POPMSG_PROGRESSMSG ){
		cinfo->PopMsgFlag = PMF_WAITTIMER | PMF_WAITKEY;
		cinfo->PopMsgTimer = 3;
	}else{
		#if DRAWMODE == DRAWMODE_D3D
		if ( err == POPMSG_PROGRESSBUSYMSG ){
			if ( cinfo->PopMsgFlag & PMF_BUSY ){
				DxSetMotion(cinfo->DxDraw, DXMOTION_Busy);
			}
			cinfo->PopMsgFlag = PMF_PROGRESS | PMF_BUSY;
		}else
		#endif
		{
			cinfo->PopMsgFlag = PMF_PROGRESS;
		}
	}

	if ( !(err & POPMSG_NOLOGFLAG) ){
		if ( hCommonLog != NULL ){
			if ( IsWindow(hCommonLog) == FALSE ){
				hCommonLog = NULL;
			}else{
				SetReportText(cinfo->PopMsgStr);
			}
		}

		if ( X_evoc == 1 ){
			PPxCommonExtCommand(K_FLASHWINDOW, (WPARAM)cinfo->info.hWnd);
			setflag(cinfo->PopMsgFlag, PMF_FLASH);
		}
		XMessage(NULL, NULL, XM_ETCl, cinfo->PopMsgStr);
	}
}
// メッセージ表示の停止 -------------------------------------------------------
void StopPopMsg(PPC_APPINFO *cinfo, int mask)
{
	if ( cinfo->PopMsgFlag & mask & PMF_FLASH ){ // 優先順位考慮不要
		PPxCommonExtCommand(K_STOPFLASHWINDOW, (WPARAM)cinfo->info.hWnd);
		resetflag(cinfo->PopMsgFlag, PMF_FLASH);
	}
	if ( cinfo->PopMsgFlag & PMF_DISPLAYMASK ){
		#if DRAWMODE == DRAWMODE_D3D
			if ( cinfo->PopMsgFlag & mask & PMF_BUSY ){ // 優先順位考慮不要
				DxSetMotion(cinfo->DxDraw, DXMOTION_StopBusy);
			}
		#endif
		resetflag(cinfo->PopMsgFlag, mask);
		// 表示が無くなるので描画する
		if ( !(cinfo->PopMsgFlag & PMF_DISPLAYMASK) ){
			setflag(cinfo->DrawTargetFlags, DRAWT_STATUSLINE);
			RefleshStatusLine(cinfo);
		}
	}
}

BOOL WriteReport(const TCHAR *title, const TCHAR *name, ERRORCODE errcode)
{
	TCHAR buf[CMDLINESIZE], *p;

	if ( Combo.Report.hWnd == NULL ){
		if ( hCommonLog == NULL ) return FALSE;
		if ( IsWindow(hCommonLog) == FALSE ){
			hCommonLog = NULL;
			return FALSE;
		}
	}
	p = buf;

	if ( title != NULL ){
		tstrcpy(p, title);
		p += tstrlen(p);
	}
	if ( name != NULL ){
		tstrcpy(p, name);
		p += tstrlen(p);
		if ( errcode != NO_ERROR ){
			*p++ = ' ';
			*p++ = ':';
			*p++ = ' ';
		}
	}
	*p = '\0';
	if ( errcode != NO_ERROR ){
		if ( errcode == REPORT_GETERROR ) errcode = GetLastError();
		PPErrorMsg(p, errcode);
	}
	SetReportText(buf);
	return TRUE;
}

// PIDL list をまとめて解放 ---------------------------------------------------
#if !NODLL
void FreePIDLS(LPITEMIDLIST *pidls, int cnt)
{
	LPMALLOC pMA;

	if ( FAILED(SHGetMalloc(&pMA)) ) return;
	while ( cnt != 0 ){
		pMA->lpVtbl->Free(pMA, *pidls);
		pidls++;
		cnt--;
	}
	pMA->lpVtbl->Release(pMA);
}
#endif

const TCHAR StrExtractCheck[] = T("Extract file to temporary dir ?");
// Shell Context Menu を実行 --------------------------------------------------
ERRORCODE SCmenu(PPC_APPINFO *cinfo, const TCHAR *action)
{
	PPXCMDENUMSTRUCT work;
	LPITEMIDLIST *pidls;
	LPSHELLFOLDER pSF;
	TCHAR buf1[VFPS], buf2[VFPS];

	if ( (cinfo->UnpackFix == FALSE) &&
		((cinfo->e.Dtype.mode == VFSDT_UN) ||
		 (cinfo->e.Dtype.mode == VFSDT_SUSIE) ||
		 (cinfo->e.Dtype.mode == VFSDT_CABFOLDER) ||
		 (cinfo->e.Dtype.mode == VFSDT_ZIPFOLDER)) ){
		if ( PMessageBox(cinfo->info.hWnd, StrExtractCheck, T("Menu"),
					MB_APPLMODAL | MB_OKCANCEL | MB_DEFBUTTON1 |
					MB_ICONQUESTION) != IDOK ){
			return ERROR_CANCELLED;
		}
		OnArcPathMode(cinfo);
	}

	PPcEnumInfoFunc(PPXCMDID_STARTENUM, buf1, &work);
	PPcEnumInfoFunc('C', buf2, &work);
	PPcEnumInfoFunc(PPXCMDID_ENDENUM, buf1, &work);
	if ( *buf2 == '\0' ){
		OffArcPathMode(cinfo);
		return ERROR_FILE_NOT_FOUND;
	}
					// ファイルが１個のみ or SHN でない
	if ( (cinfo->e.markC <= 1) && (cinfo->e.Dtype.mode != VFSDT_SHN) ){
		if ( PPcSHContextMenu(cinfo, buf2, action) == FALSE ){
			SetPopMsg(cinfo, POPMSG_MSG, T("ContextMenu Error"));
			OffArcPathMode(cinfo);
			return ERROR_ACCESS_DENIED;
		}
	}else{									// 2以上
		int count;

		count = MakePIDLTable(cinfo, &pidls, &pSF);
		if ( count ){
			POINT pos;

			GetPopupPosition(cinfo, &pos);
			SHContextMenu(cinfo->info.hWnd, &pos, pSF, pidls, count, action);
			FreePIDLS(pidls, count);
			pSF->lpVtbl->Release(pSF);
			HeapFree( hProcessHeap, 0, pidls);
		}else{
			SetPopMsg(cinfo, POPMSG_MSG, T("ContextMenu Error"));
			OffArcPathMode(cinfo);
			return ERROR_ACCESS_DENIED;
		}
	}
	OffArcPathMode(cinfo);
	return NO_ERROR;
}
#if !NODLL
DWORD GetIpdlSum(LPITEMIDLIST pidl)
{
	DWORD sum = 0;
	USHORT size = (USHORT)(pidl->mkid.cb - sizeof(USHORT));
	BYTE *idlptr = (BYTE *)pidl->mkid.abID;

	for(;;){
		sum += *idlptr++;
		if ( --size == 0 ) break;
	}
	// idlist が複数あるときは対象外にする
	if ( *((USHORT*)idlptr) != 0 ) sum = 0;
	return sum;
}
#endif
// PIDL list を作成 -----------------------------------------------------------
int MakePIDLTable(PPC_APPINFO *cinfo, LPITEMIDLIST **pidls, LPSHELLFOLDER *pSF)
{
	HANDLE heap;
	LPSHELLFOLDER pParentFolder;
	LPITEMIDLIST *PIDLs, *idls_cell = NULL;
	LPITEMIDLIST *lps;
	ENTRYCELL *cell;
	int cnt = 0, marks;
	int work;
	TCHAR buf[VFPS];

	heap = hProcessHeap;

	marks = cinfo->e.markC ? cinfo->e.markC : 1;
	PIDLs = HeapAlloc(heap, 0, marks * sizeof(LPITEMIDLIST *));
	if ( PIDLs == NULL ) return 0;

	pParentFolder = VFPtoIShell(cinfo->info.hWnd,
		  (cinfo->e.Dtype.mode != VFSDT_LFILE) ? cinfo->path : T("#:\\"), NULL);
	if ( pParentFolder == NULL ) goto fin;

				// filesystem でないので、１回のenumで作成できるように処理する
	if ( IsNodirShnPath(cinfo) ){
		LPENUMIDLIST pEID;
		DWORD enumflags;
		LPITEMIDLIST idl;
		int index = 0;

		idls_cell = HeapAlloc(heap, 0, marks * sizeof(LPITEMIDLIST *));
		if ( idls_cell == NULL ) goto fin;

		cinfo->CellHashType = CELLHASH_NONE;
		InitEnumMarkCell(cinfo, &work);
		while ( (cell = EnumMarkCell(cinfo, &work)) != NULL ){
			cell->cellhash = 0;
		}

		enumflags = (OSver.dwMajorVersion >= 6) ?
				ENUMOBJECTSFORFOLDERFLAG_VISTA : ENUMOBJECTSFORFOLDERFLAG_XP;
		if ( S_OK == pParentFolder->lpVtbl->EnumObjects(pParentFolder, cinfo->info.hWnd, enumflags, &pEID) ){ // S_FALSE のときは、pEID = NULL
			LPMALLOC pMA;
			TCHAR name[VFPS];

			SHGetMalloc(&pMA);
			for ( ; ; ){
				DWORD getsi;

				if ( pEID->lpVtbl->Next(pEID, 1, &idl, &getsi) != S_OK ) break;
				if ( getsi == 0 ) break;

				if ( IsTrue(PIDL2DisplayNameOf(name, pParentFolder, idl)) ){
					InitEnumMarkCell(cinfo, &work);
					for (;;) {
						if ( (cell = EnumMarkCell(cinfo, &work)) == NULL ){
							if ( idl != NULL ) pMA->lpVtbl->Free(pMA, idl);
							break;
						}
						if ( (cell->cellhash == 0) && (tstrcmp(name, cell->f.cFileName) == 0) && (GetIpdlSum(idl) == cell->f.dwReserved1) ){
							idls_cell[index] = idl;
							cell->cellhash = ++index;
							break;
						}
					}
				}
			}
			pMA->lpVtbl->Release(pMA);
			pEID->lpVtbl->Release(pEID);
		}
	}
	// cellに対応するidlを設定/取得
	lps = PIDLs;
	InitEnumMarkCell(cinfo, &work);
	while ( (cell = EnumMarkCell(cinfo, &work)) != NULL ){
		if ( cinfo->e.Dtype.mode == VFSDT_SHN ){
			if ( idls_cell && (cell->cellhash != 0) ){
				*lps = idls_cell[cell->cellhash - 1];
			}else{
				*lps = BindIShellAndFdata(pParentFolder, &cell->f);
			}
		}else{
			*lps = BindIShellAndFname(pParentFolder, GetCellFileName(cinfo, cell, buf));
		}

		if ( *lps == NULL ){
			XMessage(NULL, NULL, XM_NiERRld, T("MakePIDLTable bind error %s"), cell->f.cFileName);
			break;
		}else{
			lps++;
			cnt++;
		}
	}
	if ( idls_cell != NULL ) HeapFree(heap, 0, idls_cell);
fin:
	if ( cnt == 0 ){
		if ( pParentFolder != NULL ){
			pParentFolder->lpVtbl->Release(pParentFolder);
		}
		HeapFree(heap, 0, PIDLs);
	}else{
		*pidls = PIDLs;
		*pSF = pParentFolder;
	}
	return cnt;
}

// XC_swin を保存／取得 -------------------------------------------------------
void IOX_win(PPC_APPINFO *cinfo, BOOL save)
{
	TCHAR id[2] = T("A");

	id[0] += (TCHAR)((cinfo->RegCID[1] - (TCHAR)'A') & (TCHAR)0x7e);
	if ( IsTrue(save) ){
		if ( !cinfo->combo ){
			SetCustTable(T("XC_swin"), id, &cinfo->swin, sizeof(cinfo->swin));
		}
	}else{
		GetCustTable(T("XC_swin"), id, &cinfo->swin, sizeof(cinfo->swin));
	}
}

// XC_swin の更新を対窓に連絡／対窓を起動 -------------------------------------
void SendX_win(PPC_APPINFO *cinfo)
{
	HWND hPairWnd;

	if ( cinfo->combo ) return;
	IOX_win(cinfo, TRUE);
	if ( CheckReady(cinfo) == FALSE ) return;
	hPairWnd = PPcGetWindow(cinfo->RegNo, CGETW_PAIR);
	if ( hPairWnd == NULL ){
		if ( cinfo->swin & SWIN_WBOOT ) BootPairPPc(cinfo);
	}else if ( hPairWnd != BADHWND ){
		SendMessage(hPairWnd, WM_PPXCOMMAND, KC_Join, 0);
	}
}

// ダミー用エントリの値を設定する ---------------------------------------------
void USEFASTCALL SetDummyCell(ENTRYCELL *cell, const TCHAR *lfn)
{
	cell->mark_fw = NO_MARK_ID;
	cell->mark_bk = NO_MARK_ID;
	cell->extC = C_AUTO;
	cell->type   = ECT_SYSMSG;
	cell->state  = ECS_MESSAGE;
	cell->ext = (WORD)tstrlen(lfn);
	cell->comment = EC_NOCOMMENT;
//	cell->cellhash = 0;
	cell->cellcolumn = CCI_NODATA;
	cell->icon = ICONLIST_NOINDEX;
	cell->attr = ECA_ETC;
	cell->attrsortkey = 0;
	// dwFileAttributes, ftCreationTime, ftLastAccessTime, ftLastWriteTime, nFileSize
	memset(&cell->f, 0, (sizeof(DWORD) * 3) + (sizeof(FILETIME) * 3));
#if FREEPOSMODE
	cell->pos.x = NOFREEPOS;
#endif
	tstrcpy(cell->f.cFileName, lfn);
	cell->f.cAlternateFileName[0] = '\0';
}

// エントリ一覧を作成する -----------------------------------------------------

TCHAR *GetFilesNextAlloc(TCHAR *nowptr, TCHAR **baseptr, TCHAR **nextptr)
{
	TCHAR *np, *lbaseptr = *baseptr;
	size_t newsize;

	newsize = *nextptr - lbaseptr + GFSIZE;
	np = HeapReAlloc(hProcessHeap, 0, lbaseptr, TSTROFF(newsize) + VFPS);
	if ( np != NULL ){
		*nextptr = np + newsize;
		*baseptr = np;
		#pragma warning(suppress:6001) // サイズ計算のみにlbaseptrを使用
		return np + (nowptr - lbaseptr);
	}
	*baseptr = NULL;
	xmessage(XM_FaERRd, T("ReAllocError"));
	HeapFree(hProcessHeap, 0, lbaseptr);
	return nowptr;
}

TCHAR *GetFilesSHN(PPC_APPINFO *cinfo, TCHAR *nameheap)
{
	LPITEMIDLIST *pidls;
	LPSHELLFOLDER pSF;
	LPDATAOBJECT  pDO;
	int items;
	TCHAR *names = nameheap, *p = nameheap;
	TCHAR *next;

	items = MakePIDLTable(cinfo, &pidls, &pSF);
	if ( items == 0 ) return NULL;

	next = names + GFSIZE - VFPS;
									// IDataObject を取得 ----------------
	if ( SUCCEEDED(pSF->lpVtbl->GetUIObjectOf(pSF, cinfo->info.hWnd, items,
			(LPCITEMIDLIST *)pidls, &IID_IDataObject, NULL, (void **)&pDO)) ){
		FORMATETC fmtetc;
		STGMEDIUM medium;

		fmtetc.cfFormat = CF_HDROP;
		fmtetc.ptd      = NULL;
		fmtetc.dwAspect = DVASPECT_CONTENT;
		fmtetc.lindex   = -1;
		fmtetc.tymed    = TYMED_HGLOBAL;

		if ( SUCCEEDED(pDO->lpVtbl->GetData(pDO, &fmtetc, &medium)) ){
			char *sDrop;
			DROPFILES *pDrop;

			pDrop = (DROPFILES *)GlobalLock(UNIONNAME0(medium, hGlobal));
			if ( pDrop != NULL ){
				sDrop = (char *)pDrop + pDrop->pFiles;

				if ( *sDrop ){
					if ( pDrop->fWide ){ // UNICODE ver
						while( *(WCHAR *)sDrop ){
							if ( p >= next ){				// メモリの追加
								p = GetFilesNextAlloc(p, &names, &next);
								if ( names == NULL ) break;
							}
							#ifdef UNICODE
								tstrcpy(p, (WCHAR *)sDrop);
								sDrop += TSTRSIZE((WCHAR *)sDrop);
							#else
								UnicodeToAnsi((WCHAR *)sDrop, p, VFPS);
								while( *(WCHAR *)sDrop ) sDrop += 2;
								sDrop += 2;
							#endif
							p += tstrlen(p) + 1;
						}
					}else{ // ANSI ver
						while( *sDrop ){
							if ( p >= next ){				// メモリの追加
								p = GetFilesNextAlloc(p, &names, &next);
								if ( names == NULL ) break;
							}
							#ifdef UNICODE
								AnsiToUnicode(sDrop, p, VFPS);
							#else
								tstrcpy(p, sDrop);
							#endif
							sDrop += strlen(sDrop) + 1;
							p += tstrlen(p) + 1;
						}
					}
				}
				GlobalUnlock(UNIONNAME0(medium, hGlobal));
			}
			ReleaseStgMedium(&medium);
			pDO->lpVtbl->Release(pDO);
		}
	}
	FreePIDLS(pidls, items);
	pSF->lpVtbl->Release(pSF);
	HeapFree(hProcessHeap, 0, pidls);
	*p++ = 0;
	*p = 0;
	if ( (names != NULL) && (*names == '\0') ){
		HeapFree(hProcessHeap, 0, names);
		names = NULL;
	}
	return names;
}

TCHAR *GetFiles(PPC_APPINFO *cinfo, int flags)
{
	TCHAR *names, *namep, *next;
	TCHAR buf[VFPS], dir[VFPS];
	PPXCMDENUMSTRUCT work;
										// 保存用のメモリを確保する
	names = HeapAlloc(hProcessHeap, 0, TSTROFF(GFSIZE));
	if ( names == NULL ){
		xmessage(XM_FaERRd, T("GetFiles - AllocError"));
		return NULL;
	}
	namep = names;
	next = names + GFSIZE - VFPS;
	if ( flags & GETFILES_DATAINDEX ) next -= sizeof(ENTRYDATAOFFSET);

	if ( cinfo->e.Dtype.mode == VFSDT_SHN ){
		return GetFilesSHN(cinfo, names);
	}

	PPcEnumInfoFunc(PPXCMDID_STARTENUM, buf, &work);
	if ( flags & GETFILES_FULLPATH ) PPcEnumInfoFunc('1', dir, &work);
	if ( flags & GETFILES_SETPATHITEM ){
		PPcEnumInfoFunc('1', namep, &work);
		namep += tstrlen(namep) + 1;
	}
	for( ; ; ){
		PPcEnumInfoFunc('C', buf, &work);
		if ( *buf == '\0' ) break;
		if ( IsParentDir(buf) == FALSE ){
			if ( namep >= next ){				// メモリの追加
				namep = GetFilesNextAlloc(namep, &names, &next);
				if ( names == NULL ) break;
			}
			if ( flags & GETFILES_FULLPATH ){
				#pragma warning(suppress: 6054) // PPcEnumInfoFunc('1', dir, &work);
				VFSFullPath(namep, buf, dir);
				if ( flags & GETFILES_REALPATH ){
					int mode;

					if ( VFSGetDriveType(namep, &mode, NULL) != NULL ){
						if ( (mode <= VFSPT_SHN_DESK) || (mode == VFSPT_SHELLSCHEME) ){
							VFSGetRealPath(NULL, namep, namep);
						}
					}
				}
			}else{
				tstrcpy(namep, buf);
			}
			namep += tstrlen(namep) + 1;
			if ( flags & GETFILES_DATAINDEX ){
				ENTRYDATAOFFSET *ep = (ENTRYDATAOFFSET *)namep;
				*ep++ = (work.enumID == CELLENUMID_ONCURSOR) ?
					CELt(cinfo->e.cellN) : (ENTRYDATAOFFSET)work.enumID;
				namep = (TCHAR *)ep;
			}
		}
		if ( PPcEnumInfoFunc(PPXCMDID_NEXTENUM, buf, &work) == 0 ) break;
	}
	PPcEnumInfoFunc(PPXCMDID_ENDENUM, buf, &work);
	*namep = '\0';
	*(namep + 1) = '\0';

	if ( (names != NULL) && (*names == '\0') ){
		HeapFree(hProcessHeap, 0, names);
		names = NULL;
	}
	return names;
}

TCHAR *GetFilesForListfile(PPC_APPINFO *cinfo)
{
	TCHAR *names, *namep, *next, *top;
	TCHAR buf[VFPS], dir[VFPS];
	PPXCMDENUMSTRUCT work;
	int baselen;
										// 保存用のメモリを確保する
	names = HeapAlloc(hProcessHeap, 0, TSTROFF(GFSIZE));
	if ( names == NULL ){
		xmessage(XM_FaERRd, T("GetFiles - AllocError"));
		return NULL;
	}
	namep = names;
	next = names + GFSIZE - VFPS;

	PPcEnumInfoFunc(PPXCMDID_STARTENUM, buf, &work);
	if ( cinfo->e.Dtype.BasePath[0] != '\0' ){
		CatPath(dir, cinfo->e.Dtype.BasePath, NilStr);
		baselen = tstrlen32(dir);
	}else{
		baselen = 0;
	}
	for( ; ; ){
		PPcEnumInfoFunc('C', buf, &work);
		if ( *buf == '\0' ){
			break;
		}
		if ( IsParentDir(buf) == FALSE ){
			top = buf;
			if ( baselen && ( memcmp(buf, dir, baselen * sizeof(TCHAR)) == 0 ) ){
				top += baselen;
			}
			if ( namep >= next ){				// メモリの追加
				namep = GetFilesNextAlloc(namep, &names, &next);
				if ( names == NULL ) break;
			}
			tstrcpy(namep, top);
			namep += tstrlen(namep) + 1;
		}
		if ( PPcEnumInfoFunc(PPXCMDID_NEXTENUM, buf, &work) == 0 ) break;
	}
	PPcEnumInfoFunc(PPXCMDID_ENDENUM, buf, &work);
	*namep = '\0';
	*(namep + 1) = '\0';
	return names;
}

LPVOID USEFASTCALL PPcHeapAlloc(DWORD dwBytes)
{
	return HeapAlloc(hProcessHeap, 0, dwBytes);
}
BOOL USEFASTCALL PPcHeapFree(LPVOID mem)
{
	return HeapFree(hProcessHeap, 0, mem);
}
TCHAR * USEFASTCALL PPcStrDup(const TCHAR *string)
{
	SIZE32_T size;
	TCHAR *dupstring;

	size = TSTRSIZE32(string);
	dupstring = PPcHeapAlloc(size);
	memcpy(dupstring, string, size);
	return dupstring;
}

// 指定エントリ名のセルへ移動 -------------------------------------------------
BOOL FindCell(PPC_APPINFO *cinfo, const TCHAR *name)
{
	int i, newN;

	for ( i = 0 ; i < cinfo->e.cellIMax ; i++ ){
		if ( !tstricmp(CEL(i).f.cFileName, name) ){
			MoveCellCsr(cinfo, i - cinfo->e.cellN, NULL);
			return TRUE;
		}
	}
	// 該当無し…「..」にカーソルが当たるように調整
	newN = cinfo->e.cellN;
	if ( (CEL(newN).attr & ECA_THIS) && ((newN + 1) < cinfo->e.cellIMax) ) newN++;
	MoveCellCsr(cinfo, newN - cinfo->e.cellN, NULL);
	return FALSE;
}

void USEFASTCALL HideOneEntry(PPC_APPINFO *cinfo, int index)
{
	if ( index < (cinfo->e.cellIMax - 1) ){
		if ( CEL(index).f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
			if ( CEL(index).attr & (ECA_PARENT | ECA_THIS) ){
				cinfo->e.RelativeDirs--;
			}else{
				cinfo->e.Directories--;
			}
		}
		memmove(&CELt(index), &CELt(index + 1),
				sizeof(ENTRYINDEX) * (cinfo->e.cellIMax + cinfo->e.cellStack - 1 - index));
	}
	cinfo->e.cellIMax--;
}

void USEFASTCALL FixHideEntry(PPC_APPINFO *cinfo)
{
	if ( cinfo->e.cellIMax ) return;

		// ※TM_checkでcellアドレスが変わる場合有
	TM_check(&cinfo->e.CELLDATA, sizeof(ENTRYCELL) * (cinfo->e.cellDataMax + 2) );
	TM_check(&cinfo->e.INDEXDATA, sizeof(ENTRYINDEX) * (cinfo->e.cellDataMax + 2));

	SetDummyCell(&CELdata(cinfo->e.cellDataMax), MessageText(StrNoEntries));
	CELdata(cinfo->e.cellDataMax).f.nFileSizeLow = ERROR_FILE_NOT_FOUND;
	CELt(cinfo->e.cellIMax) = cinfo->e.cellDataMax;
	cinfo->e.cellDataMax++;
	cinfo->e.cellIMax++;
}

// 変更情報の表示を消去する -------------------------------------------------
void ClearChangeState(PPC_APPINFO *cinfo)
{
	ENTRYDATAOFFSET offset;
	ENTRYINDEX index;

	if ( StartCellEdit(cinfo) ) return;
	if ( cinfo->CellModified == CELLMODIFY_MODIFIED ){
		cinfo->CellModified = CELLMODIFY_FIXED;
	}
							// エントリの調節
	for ( offset = 0; offset < cinfo->e.cellDataMax ; offset++ ){
		BYTE state;

		state = CELdata(offset).state;
		if ( (state == ECS_GRAY) ||
		   (state == ECS_CHANGED) ||
		   (state == ECS_ADDED) ){
			CELdata(offset).state = ECS_NORMAL;
			continue;
		}
	}
	for ( index = 0; index < cinfo->e.cellIMax ; ){ // インデックステーブルの調節
		if ( CEL(index).state == ECS_DELETED ){
			HideOneEntry(cinfo, index);
		}else{
			index++;
		}
	}
	FixHideEntry(cinfo);
	EndCellEdit(cinfo);
	cinfo->DrawTargetFlags = DRAWT_ALL;
	MoveCellCsr(cinfo, 0, NULL);
	Repaint(cinfo);
}

// 作業終了後の一覧更新処理 -------------------------------------------------
void SetRefreshAfterList(PPC_APPINFO *cinfo, int actiontype, TCHAR drivename)
{
	if ( XC_alac ){ // 全PPcに通知
		if ( drivename == '\0' ) drivename = cinfo->RealPath[0];
		DEBUGLOGC("SetRefreshAfterList all", 0);
		PPxPostMessage(WM_PPXCOMMAND, RefreshAfterListModes[actiontype], drivename);
	}else{
		DEBUGLOGC("SetRefreshAfterList start", 0);
		RefreshAfterList(cinfo, actiontype);
		DEBUGLOGC("SetRefreshAfterList end", 0);
	}
}

#define DEFADDFLAG (RENTRY_SAVEOFF | RENTRY_MODIFYUP)
void RefreshAfterList(PPC_APPINFO *cinfo, int actiontype)
{
	int addflag, mode;

	if ( TinyCheckCellEdit(cinfo) ) return;

	mode = XC_alst[actiontype];
	if ( mode < ALSTV_UPD ) return; // なにもしない
	if ( actiontype == ALST_ACTIVATE ){ // アクティブ時更新
		if ( cinfo->FDirWrite == FDW_NORMAL ){
			if ( (mode == ALSTV_RELOAD) || ((mode >= ALSTV_UPD_AND_RELOAD) && !cinfo->e.markC) ){ // 再読込
				if ( cinfo->CellModified == CELLMODIFY_NONE ) return; // 済み
			}else{ // 更新
				// 非表示が不要 || 非表示済み なら何もしなくてよい
				if ( !(mode & 1) || (cinfo->CellModified <= CELLMODIFY_FIXED)){
					return;
				}
				mode = ALSTV_NONE; // 更新しないで非表示処理をさせる
			}
			addflag = RENTRY_SAVEOFF; // RENTRY_MODIFYUP を付ける程、最新にしない
		}else{
			addflag = DEFADDFLAG;
		}
	}else if ( actiontype == ALST_RENAME ){ // 名前変更
		addflag =
			(CEL(cinfo->e.cellN).f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ?
			DEFADDFLAG | RETRY_FLAGS_NEWDIR :
			DEFADDFLAG | RETRY_FLAGS_NEWFILE;
		if ( !(mode & 1) ) setflag(addflag, RENTRY_JUMPNAME); // 非表示以外はjumpname
	}else{ // 属性変更、コピー、移動、削除
		addflag = DEFADDFLAG;
	}

	if ( XC_acsr[0] ){ // カーソル位置をエントリ名に維持する
		if ( actiontype != ALST_RENAME ){ // 名前変更の時は場所指定済み
			tstrcpy(cinfo->Jfname, CEL(cinfo->e.cellN).f.cFileName);
		}
		setflag(addflag, RENTRY_JUMPNAME);
	}

	if ( (mode == ALSTV_RELOAD) || ((mode >= ALSTV_UPD_AND_RELOAD) && !cinfo->e.markC) ){ // 読み込み処理
		read_entry(cinfo, addflag);
		return;
	}
	// 更新処理
	if ( mode >= ALSTV_UPD ){
		// 単なる更新ではカーソル位置が変化しないので、RENTRY_JUMPNAMEを無効に
		read_entry(cinfo, RENTRY_UPDATE |
				((actiontype == ALST_RENAME) ?
					addflag : (addflag & ~RENTRY_JUMPNAME)) );
	}
	// mode == ALSTV_NONE, ALSTV_UPD_HIDE, ALSTV_UPD_HIDE_AND_RELOAD  非表示処理
	if ( mode & 1 ){
			ClearChangeState(cinfo);
	}
	if ( (mode & 1) || (actiontype == ALST_RENAME) ){
		// 非表示・名前変更によりエントリ数が変化し、カーソル位置がずれるので調整
		if ( (addflag & RENTRY_JUMPNAME) &&
			 (tstricmp(cinfo->Jfname, CEL(cinfo->e.cellN).f.cFileName) != 0) ){
			FindCell(cinfo, cinfo->Jfname);
		}
	}
}

// GetCustTable/GetCustData のDWORD専用版+初期値有り --------------------------
DWORD GetCustXDword(const TCHAR *kword, const TCHAR *subkword, DWORD defaultvalue)
{
	if ( subkword != NULL ){
		GetCustTable(kword, subkword, &defaultvalue, sizeof(DWORD));
	}else{
		GetCustData(kword, &defaultvalue, sizeof(DWORD));
	}
	return defaultvalue;
}

// 指定番目のパス履歴を使ってディレクトリ表示する -----------------------------
void JumpPathTrackingList(PPC_APPINFO *cinfo, int dest)
{
	DWORD top;
	TCHAR *p;

	if ( !cinfo->PathTrackingList.top ) return;	// 蓄積無し
	dest--;

	top = cinfo->PathTrackingList.top;
	while( dest < 0 ){
		if ( !top ) return;
		top = BackPathTrackingList(cinfo, top);
		dest++;
	}
	while( dest > 0 ){
		p = (TCHAR *)(cinfo->PathTrackingList.bottom + top);
		if ( *p == '\0' ) return;
		top += TSTRSIZE32(p);
		dest--;
	}
	p = (TCHAR *)(cinfo->PathTrackingList.bottom + top);
	if ( *p == '\0' ) return;
	cinfo->PathTrackingList.top = top + TSTRSIZE32(p);

	SetPPcDirPos(cinfo);
	tstrcpy(cinfo->path, p);
	read_entry(cinfo, RENTRY_NOFIXDIR);
}

// パス履歴を一つ逆戻ったオフセットを求める -----------------------------
DWORD BackPathTrackingList(PPC_APPINFO *cinfo, DWORD top)
{
	top -= TSTROFF(2);
	for ( ; top ; top -= sizeof(TCHAR) ){	// 一つ前に移動
		if ( *(TCHAR *)(cinfo->PathTrackingList.bottom + top) == '\0' ){
			top += sizeof(TCHAR);
			break;
		}
	}
	return top;
}

int PPctInput(PPC_APPINFO *cinfo, const TCHAR *title, TCHAR *string, int maxlen, WORD readhist, WORD writehist)
{
	TINPUT tinput;

	tinput.hOwnerWnd= cinfo->info.hWnd;
	tinput.hRtype	= readhist;
	tinput.hWtype	= writehist;
	tinput.title	= title;
	tinput.buff		= string;
	tinput.size		= maxlen;
	tinput.flag		= TIEX_USEINFO;
	tinput.info		= &cinfo->info;
	if ( writehist & (PPXH_NUMBER | PPXH_FILENAME | PPXH_PATH | PPXH_SEARCH | PPXH_MASK) ){
		setflag(tinput.flag, TIEX_SINGLEREF);
	}
	if ( writehist == PPXH_DIR ){
		setflag(tinput.flag, TIEX_USESELECT | TIEX_REFTREE | TIEX_SINGLEREF | TIEX_INSTRSEL);
		tinput.firstC = EC_LAST;
		tinput.lastC = EC_LAST;
	}
	return tInputEx(&tinput);
}

ERRORCODE InputTargetDir(PPC_APPINFO *cinfo, const TCHAR *title, TCHAR *string, int maxlen)
{
	if ( PPctInput(cinfo, title, string, maxlen, PPXH_DIR_R, PPXH_DIR) <= 0 ){
		return ERROR_CANCELLED;
	}
	if ( VFSFixPath(NULL, string, cinfo->path, VFSFIX_PATH) == NULL ){
		SetPopMsg(cinfo, POPMSG_MSG, MES_EPTH);
		return ERROR_BAD_COMMAND;
	}
	return NO_ERROR;
}

ENTRYINDEX GetFirstMarkCell(PPC_APPINFO *cinfo)
{
	ENTRYINDEX celln, last, bottom;

	last = cinfo->e.cellIMax;
	bottom = cinfo->e.markTop;
	for ( celln = 0 ; celln < last ; celln++ ){
		if ( CELt(celln) == bottom ) return celln;
	}
	return -1;
}

ENTRYINDEX GetNextMarkCell(PPC_APPINFO *cinfo, ENTRYINDEX cellindex)
{
	ENTRYINDEX celln, last;
	ENTRYDATAOFFSET target;

	target = GetCellData_HS(cinfo, cellindex)->mark_fw;
	if ( target < 0 ) return -1;

	last = cinfo->e.cellIMax;
	for ( celln = 0 ; celln < last ; celln++ ){
		if ( CELt(celln) == target ) return celln;
	}
	return -1;
}

ENTRYINDEX UpSearchMarkCell(PPC_APPINFO *cinfo, ENTRYINDEX cellindex)
{
	ENTRYINDEX celln;

	celln = cellindex;
	for (;;){
		if ( celln <= 0 ) return -1;
		celln--;
		if ( IsCEL_Marked(celln) ) return celln;
	}
}

ENTRYINDEX DownSearchMarkCell(PPC_APPINFO *cinfo, ENTRYINDEX cellindex)
{
	ENTRYINDEX celln, last;

	celln = cellindex;
	last = cinfo->e.cellIMax - 1;
	for (;;){
		if ( celln >= last ) return -1;
		celln++;
		if ( IsCEL_Marked(celln) ) return celln;
	}
}

BOOL WINAPI WriteTextNative(WriteTextStruct *sts, const TCHAR *text, size_t textlen)
{
	DWORD tmp;

	return WriteFile(sts->hFile, text, TSTROFF(textlen), &tmp, NULL);
}

BOOL WINAPI WriteTextUTF8(WriteTextStruct *sts, const TCHAR *text, size_t textlen)
{
	DWORD tmp;
	int len;
	char utf8text[CMDLINESIZE * 5];

#ifdef UNICODE
	len = WideCharToMultiByte(CP_UTF8, 0, text, textlen, utf8text, sizeof(utf8text), NULL, NULL);
#else
	WCHAR utf16text[CMDLINESIZE * 2];

	len = MultiByteToWideChar(CP_ACP, 0, text, textlen, utf16text, TSIZEOFW(utf16text));
	len = WideCharToMultiByte(CP_UTF8, 0, utf16text, len, utf8text, sizeof(utf8text), NULL, NULL);
#endif
	if ( len <= 0 ) return (textlen > 0) ? FALSE : TRUE;
	return WriteFile(sts->hFile, utf8text, len, &tmp, NULL);
}

BOOL USEFASTCALL CreateHandleForListFile(PPC_APPINFO *cinfo, WriteTextStruct *sts, const TCHAR *filename, int flags)
{
	TCHAR buf[VFPS + 32], *p;
	int len;
	DWORD tmp;

	p = tstrstr(filename, T("::listfile"));
	if ( p != NULL ){
		tstrcpy(buf, filename);
		buf[p - filename] = '\0';
		filename = buf;
	}
	sts->hFile = CreateFileL(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( sts->hFile == INVALID_HANDLE_VALUE ) return FALSE;
	sts->wlfc_flags = flags;
	sts->Write = (flags & WLFC_UTF8) ? WriteTextUTF8 : WriteTextNative;

	if ( flags & WLFC_BOM ){
		WriteFile(sts->hFile, ListFileHeaderStr8, sizeof(UTF8HEADER) - 1, &tmp, NULL);
	}
	if ( !(flags & WLFC_NOHEADER) ){
		if ( flags & WLFC_UTF8 ){
			WriteFile(sts->hFile, ListFileHeaderStrA, ListFileHeaderSizeA, &tmp, NULL);
		}else{
			WriteFile(sts->hFile, ListFileHeaderStr, ListFileHeaderSize, &tmp, NULL);
		}

		// listfileでないか、listfileでBasePath指定有りのときは、BasePathを出力
		if ( (cinfo->e.Dtype.mode != VFSDT_LFILE) ||
			 (cinfo->e.Dtype.BasePath[0] != '\0') ){
			len = wsprintf(buf, T(";Base=%s|%d\r\n"),
					( (cinfo->e.Dtype.mode == VFSDT_LFILE) &&
					(cinfo->e.Dtype.BasePath[0] != '\0') ) ?
					cinfo->e.Dtype.BasePath : cinfo->path,  cinfo->e.Dtype.mode);
			sts->Write(sts, buf, len);
		}
	}
	return TRUE;
}

BOOL WriteLFcell(PPC_APPINFO *cinfo, WriteTextStruct *sts, ENTRYCELL *cell)
{
	TCHAR buf[VFPS + CMDLINESIZE], *dest;
	int len;

	dest = buf;
	*dest++ = T('\"');
	len = TSTRLENGTH32(cell->f.cFileName);
	memcpy(dest, cell->f.cFileName, len);
	dest += len / sizeof(TCHAR);

	if ( sts->wlfc_flags & WLFC_NAMEONLY ){
		*dest++ = '\"';
	}else{
		dest += wsprintf(dest, ListFileFormatStr,
				cell->f.cAlternateFileName, cell->f.dwFileAttributes,
				cell->f.ftCreationTime.dwHighDateTime,
				cell->f.ftCreationTime.dwLowDateTime,
				cell->f.ftLastAccessTime.dwHighDateTime,
				cell->f.ftLastAccessTime.dwLowDateTime,
				cell->f.ftLastWriteTime.dwHighDateTime,
				cell->f.ftLastWriteTime.dwLowDateTime,
				cell->f.nFileSizeHigh, cell->f.nFileSizeLow);
	}

	if ( (sts->wlfc_flags & WLFC_WITHMARK) && IsCellPtrMarked(cell) ){
		CopyAndSkipString(dest, LFMARK_STR);
	}

	if ( (cell->comment != EC_NOCOMMENT) && (sts->wlfc_flags & WLFC_COMMENT) ){
		TCHAR *ptr;

		ptr = ThPointerT(&cinfo->EntryComments, cell->comment);
		len = tstrlen32(ptr);
		*dest++ = ',';
		*dest++ = 'T';
		*dest++ = ':';
		*dest++ = '\"';
		if ( len > (CMDLINESIZE - 100) ) len = CMDLINESIZE - 100;
		memcpy(dest, ptr, TSTROFF(len));
		dest[len] = '\0';
		while( (ptr = tstrchr(dest, '\"')) != NULL ) *ptr = '`';
		while( (ptr = tstrchr(dest, '\r')) != NULL ) *ptr = '/';
		while( (ptr = tstrchr(dest, '\n')) != NULL ) *ptr = ' ';
		dest += len;
		*dest++ = '\"';
	}

	if ( sts->wlfc_flags & WLFC_OPTIONSTR ){
		CopyAndSkipString(dest, LFSIZE_STR);
		FormatNumber(dest, 0, 26, cell->f.nFileSizeLow, cell->f.nFileSizeHigh);
		dest += tstrlen(dest);
		CopyAndSkipString(dest, LFCREATE_STR);
		dest += CnvDateTime(dest, NULL, NULL, &cell->f.ftCreationTime);
		CopyAndSkipString(dest, LFLASTWRITE_STR);
		dest += CnvDateTime(dest, NULL, NULL, &cell->f.ftLastWriteTime);
		CopyAndSkipString(dest, LFLASTACCESS_STR);
		dest += CnvDateTime(dest, NULL, NULL, &cell->f.ftLastAccessTime);
	}

	*dest++ = '\r';
	*dest++ = '\n';
	return sts->Write(sts, buf, dest - buf);
}

// キャッシュ・内部向けListFileを出力…エントリ読み込み順で出力
BOOL WriteListFileForRaw(PPC_APPINFO *cinfo, const TCHAR *filename)
{
	WriteTextStruct sts;
	ENTRYDATAOFFSET offset;

	if ( FALSE == CreateHandleForListFile(cinfo, &sts, filename, WLFC_DETAIL | WLFC_COMMENT) ){
		return FALSE;
	}

	for ( offset = 0 ; offset < cinfo->e.cellDataMax ; offset++ ){
		ENTRYCELL *cell;

		cell = &CELdata(offset);
		if ( cell->attr & (ECA_PARENT | ECA_THIS) ) continue;
		if ( cell->state == ECS_DELETED ) continue;
		if ( WriteLFcell(cinfo, &sts, cell) == FALSE ) break;
	}
	CloseHandle(sts.hFile);
	return TRUE;
}

// ユーザ向けListFileを出力…表示エントリ順で出力
void WriteListFileForUser(PPC_APPINFO *cinfo, const TCHAR *filename, int wlfc_flags)
{
	WriteTextStruct sts;
	ENTRYINDEX i;

	if ( FALSE == CreateHandleForListFile(cinfo, &sts, filename, wlfc_flags) ){
		SetPopMsg(cinfo, POPMSG_GETLASTERROR, NULL);
		return;
	}

	for ( i = 0 ; i < cinfo->e.cellIMax ; i++ ){
		ENTRYCELL *cell;

		cell = &CEL(i);
		if ( cell->type <= ECT_NOFILEDIRMAX ){
			if ( !(wlfc_flags & WLFC_SYSMSG) || (cell->type != ECT_SYSMSG) ) continue;
			cell->f.dwFileAttributes = FILE_ATTRIBUTEX_MESSAGE;
		}
		if ( (wlfc_flags & WLFC_MARKEDONLY) && !IsCellPtrMarked(cell) ){
			continue;
		}
		if ( WriteLFcell(cinfo, &sts, cell) == FALSE ) break;
	}
	CloseHandle(sts.hFile);
	return;
}

void USEFASTCALL InitJobinfo(JOBINFO *jinfo)
{
	jinfo->StartTime = jinfo->OldTime = GetTickCount();
	jinfo->count = 0;
}

void USEFASTCALL FinishJobinfo(PPC_APPINFO *cinfo, JOBINFO *jinfo, ERRORCODE result)
{
	PPxCommonExtCommand(K_TBB_STOPPROGRESS, (WPARAM)cinfo->info.hWnd);
	if ( result != NO_ERROR ){
		SetPopMsg(cinfo, result, NULL);
		return;
	}
	if ( (GetTickCount() - jinfo->StartTime) >= 1000 ){ // overflow対策兼用
		SetPopMsg(cinfo, POPMSG_NOLOGMSG, MES_COMP);
	}else{
		StopPopMsg(cinfo, PMF_DISPLAYMASK);
	}
}

BOOL BreakCheck(PPC_APPINFO *cinfo, JOBINFO *jinfo, const TCHAR *memo)
{
	MSG msg;
	DWORD NewTime;
	TASKBARBUTTONPROGRESSINFO tbbpi;
	TCHAR buf[VFPS + 16];

	NewTime = GetTickCount();
	if ( (NewTime - jinfo->OldTime) >= dispNwait ){ // overflow対策兼用
		jinfo->OldTime = NewTime;
		wsprintf(buf, T("%d %s"), jinfo->count, memo);
		SetPopMsg(cinfo, POPMSG_PROGRESSBUSYMSG, buf);
		UpdateWindow_Part(cinfo->info.hWnd);

		tbbpi.hWnd = cinfo->info.hWnd;
		tbbpi.nowcount = 1; // TBPF_INDETERMINATE jinfo->count % 100;
		tbbpi.maxcount = 0;
		PPxCommonExtCommand(K_TBB_PROGRESS, (WPARAM)&tbbpi);
	}

	while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) || cinfo->BreakFlag ){
		if ( cinfo->BreakFlag ||
			 (msg.message == WM_RBUTTONUP) ||
			 ((msg.message == WM_KEYDOWN) &&
			 (((int)msg.wParam == VK_ESCAPE)||((int)msg.wParam == VK_PAUSE)))){
			cinfo->BreakFlag = FALSE;
			if ( PMessageBox(cinfo->info.hWnd, MES_QABO, T("Break check"),
						MB_APPLMODAL | MB_OKCANCEL | MB_DEFBUTTON1 |
						MB_ICONQUESTION) == IDOK ){
				return TRUE;
			}
		}
		if ( msg.message == WM_QUIT ) break;
		if (((msg.message >= WM_MOUSEFIRST) && (msg.message <= WM_MOUSELAST))||
			((msg.message >= WM_KEYFIRST) && (msg.message <= WM_KEYLAST)) ||
			(msg.message == WM_COMMAND) || (msg.message == WM_PPXCOMMAND) ){
			continue;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return FALSE;
}

void DelayedFileOperation(PPC_APPINFO *cinfo)
{
	int count = 0;
	TCHAR operation[VFPS];
	TCHAR param[CMDLINESIZE];
	int sel;

	if ( CountCustTable(T("_Delayed")) <= 0 ) return;

	sel = PMessageBox(cinfo->info.hWnd, MES_QDDO, DFO_title,
			MB_APPLMODAL | MB_YESNOCANCEL | MB_DEFBUTTON1 | MB_ICONQUESTION);
	if ( sel != IDYES ){
		if ( sel == IDCANCEL ){
			if ( IDYES == PMessageBox(cinfo->info.hWnd, MES_QDDL, DFO_title,
					MB_APPLMODAL | MB_YESNO | MB_DEFBUTTON2 |MB_ICONQUESTION)){
					DeleteCustData(T("_Delayed"));
			}
		}
		return;
	}

	while( EnumCustTable(count, T("_Delayed"), operation, param, sizeof(param)) > 0 ){
		if ( !tstrcmp(operation, T("delete")) ){
			DWORD atr;
			BOOL result;

			atr = GetFileAttributesL(param);
			if ( atr == BADATTR ){
				DeleteCustTable(T("_Delayed"), NULL, count);
				continue;
			}
			SetFileAttributesL(param, FILE_ATTRIBUTE_NORMAL);
			if ( atr & FILE_ATTRIBUTE_DIRECTORY ){
				result = RemoveDirectoryL(param);
				if ( IsTrue(result) ){
					SHChangeNotify(SHCNE_RMDIR, SHCNF_PATH, param, NULL);
				}
			}else{
				result = DeleteFileL(param);
			}
			if ( result == FALSE ){
				XMessage(cinfo->info.hWnd, DFO_title, XM_FaERRl, MES_EDDL, param);
				count++;
			}else{
				DeleteCustTable(T("_Delayed"), NULL, count);
			}
		}else if ( !tstrcmp(operation, T("execute")) ){
			PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, param, NULL, 0);
			DeleteCustTable(T("_Delayed"), NULL, count);
		}else{
			XMessage(cinfo->info.hWnd, DFO_title, XM_FaERRl, T("Delayed FO: Unknown command:%s"), operation);
		}
	}

	if ( CountCustTable(T("_Delayed")) <= 0 ){
		DeleteCustData(T("_Delayed"));
	}
}

BOOL HdropdataToFiles(HGLOBAL hDrop, TMS_struct *files)
{
	DROPFILES *pDrop;
	TCHAR *sDrop;

	pDrop = (DROPFILES *)GlobalLock(hDrop);
	if ( pDrop == NULL ) return FALSE;
	sDrop = (TCHAR *)((char *)pDrop + pDrop->pFiles);

	TMS_reset(files);
	if ( pDrop->fWide ){
		#ifdef UNICODE
			while( *sDrop != '\0' ){
				TMS_set(files, sDrop);
				sDrop += tstrlen(sDrop) + 1;
			}
		#else
			char temp[VFPS];

			while( *(WCHAR *)sDrop != '\0' ){
				UnicodeToAnsi((WCHAR *)sDrop, temp, sizeof temp);
				while( *(WCHAR *)sDrop ) sDrop += sizeof(WCHAR);
				sDrop += sizeof(WCHAR);
				TMS_set(files, temp);
			}
		#endif
	}else{
		#ifdef UNICODE
		WCHAR temp[VFPS];

		while( *(char *)sDrop != '\0' ){
			AnsiToUnicode((char *)sDrop, temp, TSIZEOF(temp));
			TMS_set(files, temp);
			sDrop =
				(TCHAR *)(char *)((char *)sDrop + strlen((char *)sDrop) + 1);
		}
		#else
		while( *sDrop != '\0' ){
			TMS_set(files, sDrop);
			sDrop += tstrlen(sDrop) + 1;
		}
		#endif
	}
	TMS_set(files, NilStr);
	GlobalUnlock(hDrop);
	return TRUE;
}

void LoadCFMT(XC_CFMT *cfmt, const TCHAR *name, const TCHAR *sub, const XC_CFMT *defaultdata)
{
	DWORD size;
	int attr = 0;

	if ( sub != NULL ){	// 各エントリの書式
		if ( NO_ERROR != GetCustData(T("XC_celD"), cfmt, sizeof(XC_CFMT)) ){
			*cfmt = *defaultdata;
		}
		size = GetCustTableSize(name, sub);
		if ( (size == MAX32) || (size < 1) ){
			cfmt->fmtbase = NULL;
			cfmt->fmt = defaultdata->fmt;
			cfmt->width = defaultdata->width;
			size = 32;
		}else{
			cfmt->fmtbase = PPcHeapAlloc(size);
			GetCustTable(name, sub, cfmt->fmtbase, size);
			cfmt->fmt = cfmt->fmtbase + sizeof(DWORD);
			cfmt->width = *(DWORD *)cfmt->fmtbase;
			size -= sizeof(DWORD);
		}
	}else{	// 情報欄系の書式
		size = GetCustDataSize(name);
		if ( (size == MAX32) || (size <= CFMT_OLDHEADERSIZE) ){
			*cfmt = *defaultdata;
			size = 32;
		}else{
			cfmt->fmtbase = PPcHeapAlloc(size);
			GetCustData(name, cfmt->fmtbase, size);
			memcpy(cfmt, cfmt->fmtbase, CFMT_OLDHEADERSIZE);
			cfmt->fmt = cfmt->fmtbase + CFMT_OLDHEADERSIZE;
			size -= CFMT_OLDHEADERSIZE;
		}
	}

	if ( cfmt->csr > 7 ) cfmt->csr = 4;

	cfmt->nextline = HIWORD(cfmt->width);
	cfmt->width = LOWORD(cfmt->width);
	cfmt->height = 1;
	{	// 各行に DE_WIDEV / DE_WIDEW が無いか調べる
		BYTE *fmt = cfmt->fmt;
		int offset = cfmt->nextline;

		for ( ; ; ){
			if ( *fmt == DE_WIDEV ){
				attr = DE_ATTR_WIDEV;
			}else if ( *fmt == DE_WIDEW ){
				attr = DE_ATTR_WIDEW;
			}
			if ( offset == 0 ) break;
			fmt += offset;
			offset = *(WORD *)(fmt - 2);
			cfmt->height++;
		}
	}

	{	// 情報欄系の書式／表示する項目を把握する & 破損チェック
		BYTE *fmt, *maxfmt, id;

		fmt = cfmt->fmt;
		maxfmt = fmt + size;
		while( (id = *fmt) != 0 ){
			attr |= DispAttributeTable[id];
			fmt += GetDispFormatSkip(fmt);
			if ( fmt >= maxfmt ){
				*cfmt = *defaultdata;
				if ( fmt > maxfmt ){
					XMessage(NULL, NULL, XM_GrERRld, T("%s is broken %d"),
						(sub != NULL) ? sub : name, fmt - maxfmt);
				}
				return;
			}
		}
	}
	cfmt->attr = attr;
}

void FreeCFMT(XC_CFMT *cfmt)
{
	if ( cfmt->fmtbase != NULL ){
		PPcHeapFree(cfmt->fmtbase);
		cfmt->fmtbase = NULL;
	}
}

// ポップアップに使用する座標を得る -------------------------------------------
void GetPopupPosition(PPC_APPINFO *cinfo, POINT *pos)
{
	switch ( cinfo->PopupPosType ){
		case PPT_MOUSE:
			GetCursorPos(pos);
			pos->x++;
			break;

		case PPT_SAVED:
			*pos = cinfo->PopupPos;
			pos->x++;
			break;

//		case PPT_FOCUS: // default兼用
		default: {
			int deltaNo;

			deltaNo = cinfo->e.cellN - cinfo->cellWMin;
			pos->x = CalcCellX(cinfo, deltaNo);
			pos->y = (deltaNo % cinfo->cel.Area.cy + 1) *
								cinfo->cel.Size.cy + cinfo->BoxEntries.top;
			ClientToScreen(cinfo->info.hWnd, pos);
			break;
		}
	}
}

// TrackPopupMenu の簡易版 ----------------------------------------------------
int PPcTrackPopupMenu(PPC_APPINFO *cinfo, HMENU hMenu)
{
	POINT pos;

	GetPopupPosition(cinfo, &pos);
	return TrackPopupMenu(hMenu, TPM_TDEFAULT,
			pos.x, pos.y, 0, cinfo->info.hWnd, NULL);
}

void ExistCheck(TCHAR *dst, const TCHAR *path, const TCHAR *name)
{
	VFSFullPath(dst, (TCHAR *)name, path);
	GetUniqueEntryName(dst);
}

#if !NODLL
const TCHAR SusiePath[] = T("Software\\Takechin\\Susie\\Plug-in");
const TCHAR StrPath[] = T("PATH");
#else
extern const TCHAR SusiePath[];
extern const TCHAR StrPath[];
#endif
BOOL LoadImageSaveAPI(void)
{
	TCHAR dir[MAX_PATH], path[MAX_PATH];

	dir[0] = '\0';
	GetCustData(T("P_susieP"), dir, sizeof(dir));
	if ( dir[0] != '\0' ){
		VFSFixPath(NULL, dir, PPcPath, VFSFIX_FULLPATH | VFSFIX_REALPATH);
	}else if ( GetRegString(HKEY_CURRENT_USER,	// 2)susie が認識する dir
			SusiePath, StrPath, dir, TSIZEOF(dir)) ){
	}else{										// 3)ppx dir
		tstrcpy(dir, PPcPath);
	}
	VFSFullPath(path, TWICNAME, dir);
	hTSusiePlguin = LoadLibrary(path);
	if ( hTSusiePlguin == NULL ){
		VFSFullPath(path, TGDIPNAME, dir);
		hTSusiePlguin = LoadLibrary(path);
	}
	if ( hTSusiePlguin == NULL ){
		hTSusiePlguin = LoadLibrary(TWICNAME);
		if ( hTSusiePlguin == NULL ) hTSusiePlguin = LoadLibrary(TGDIPNAME);
	}

	if ( hTSusiePlguin != NULL ){
		CreatePicture = (CREATEPICTURE)GetProcAddress(hTSusiePlguin, TGDIPCREATENAME);
		if ( CreatePicture != NULL ) return TRUE;
		FreeLibrary(hTSusiePlguin);
		hTSusiePlguin = NULL;
	}
	return FALSE;
}

int ImageSaveByAPI(BITMAPINFO *bfh, DWORD bfhsize, char *bmp, size_t bmpsize, const TCHAR *filename)
{
	HLOCAL hHeader, hBitmap;
	int result;
	DWORD ProfileData = 0;

	if ( bfh->bmiHeader.biSize == 0x7c /*BITMAPV5HEADER*/ ){
		ProfileData = *(DWORD *)((BYTE *)bfh + 0x70); //bV5ProfileData
	}

	if ( ProfileData > bfhsize ){
		DWORD ProfileSize = *(DWORD *)((BYTE *)bfh + 0x74); //bV5ProfileSize

		hHeader = LocalAlloc(0, bfhsize + ProfileSize);
		if ( hHeader == NULL ) return SUSIEERROR_EMPTYMEMORY;
		memcpy((char *)hHeader + bfhsize, (BYTE *)bfh + ProfileData, ProfileSize);
		memcpy((char *)hHeader, bfh, bfhsize);
		*(DWORD *)((BYTE *)hHeader + 0x70) /*bV5ProfileData */ = bfhsize;
	}else{
		hHeader = LocalAlloc(0, bfhsize);
		if ( hHeader == NULL ) return SUSIEERROR_EMPTYMEMORY;
		memcpy((char *)hHeader, bfh, bfhsize);
	}

	hBitmap = LocalAlloc(0, bmpsize);
	if ( hBitmap == NULL ) return SUSIEERROR_EMPTYMEMORY;
	memcpy((char *)hBitmap, bmp, bmpsize);

	result = CreatePicture(filename, B28, &hHeader, &hBitmap, NULL, NULL, 0);
	LocalFree(hBitmap);
	LocalFree(hHeader);

	FreeLibrary(hTSusiePlguin);
	hTSusiePlguin = NULL;
	return result;
}

const TCHAR Ext_text[] = T(".txt");

void MakeClipboardDataName(UINT orgtype, TCHAR *name, const char *data, int size)
{
	const TCHAR *ext = T(".bin");
	GetClipboardTypeName(name, orgtype);

	{	// text/html 等の名前対策
		TCHAR *ptr = name;

		while ( (ptr = tstrchr(ptr, '/')) != NULL ) *ptr = '-';
	}
	if ( orgtype == CF_TEXT ){
		int left = MAXCLIPMEMO - 2;
#ifdef UNICODE
		char buf[MAXCLIPMEMO], *DEST = buf;
#else
#define DEST name
#endif
		ext = Ext_text;

		name += tstrlen(name);
		// テキストの一部をファイル名に付与する
		*name++ = '-';
		while ( size && left ){
			if ( IskanjiA(*data) && ((BYTE)(*(data + 1)) >= 0x40) ){
				if ( (size < 2) || (left < 2) ) break;
				*DEST++ = *data++;
				*DEST++ = *data;
				size--;
				left -= 2;
			}else if ( IsalnumA(*data) ){
				*DEST++ = *data;
				left--;
			}else if ( *data == '\0' ){
				break;
			}
			data++;
			size--;
		}
		*DEST = '\0';
#ifdef UNICODE
		AnsiToUnicode(buf, name, MAXCLIPMEMO);
#endif
	}
#ifdef UNICODE
	if ( orgtype == CF_UNICODETEXT ){
		int left = MAXCLIPMEMO - 2;

		ext = Ext_text;
		name += tstrlen(name);
		// テキストの一部をファイル名に付与する
		*name++ = '-';
		while ( size && left ){
			UTCHAR chr;

			chr = *(UTCHAR *)data;
			if ( IsalnumW(chr) || (chr > 0x200) ){
				*name++ = chr;
				left--;
			}else if ( chr == '\0' ){
				break;
			}
			data += sizeof(TCHAR);
			size -= sizeof(TCHAR);
		}
		*name = '\0';
	}
#else
	if ( orgtype == CF_UNICODETEXT ) ext = Ext_text;
#endif
	if ( (orgtype == CF_DIB) || (orgtype == CF_DIBV5) ){
		ext = T(".bmp");
		if ( LoadImageSaveAPI() != FALSE ) ext = T(".png");
	}
	tstrcat(name, ext);
}

void SaveBmpData(HANDLE hFile, char *dumpdata, DWORD size)
{
	BITMAPFILEHEADER bfh;
	DWORD offset, tmp;

	offset = CalcBmpHeaderSize((BITMAPINFOHEADER *)dumpdata);
	bfh.bfType = 'B' + ('M' << 8);
	bfh.bfSize = size;
	bfh.bfReserved1 = 0;
	bfh.bfReserved2 = 0;
	bfh.bfOffBits = offset + sizeof(BITMAPFILEHEADER);
	WriteFile(hFile, &bfh, sizeof(bfh), &tmp, NULL);
}

void SaveClipboardData(HGLOBAL hGlobal, UINT cpdtype, PPC_APPINFO *cinfo)
{
	TCHAR name[VFPS], type[CMDLINESIZE], *entry;
	char *dumpdata, *tempbufdata = NULL;
	HANDLE hFile;
	DWORD size;

	dumpdata = (char *)GlobalLock(hGlobal);
	if ( dumpdata == NULL ){
		SetPopMsg(cinfo, POPMSG_MSG, T("Save error"));
		return;
	}
	size = (DWORD)GlobalSize(hGlobal);

	MakeClipboardDataName(cpdtype, type, dumpdata, size);
	ExistCheck(name, cinfo->RealPath, type);

	if ( (cpdtype == CF_DIB) || (cpdtype == CF_DIBV5) ){
		if ( hTSusiePlguin != NULL ){
			DWORD infosize;

			infosize = CalcBmpHeaderSize((BITMAPINFOHEADER *)dumpdata);
			if ( ImageSaveByAPI((BITMAPINFO *)dumpdata, infosize, dumpdata + infosize, size - infosize, name) != SUSIEERROR_NOERROR ){
				SetPopMsg(cinfo, POPMSG_MSG, T("Save error"));
			}else{
				entry = FindLastEntryPoint(name);
				tstrcpy(cinfo->Jfname, entry);
			}
			return;
		}
	}

	hFile = CreateFileL(name, GENERIC_WRITE, 0, NULL, CREATE_NEW,
			FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hFile != INVALID_HANDLE_VALUE ){
		entry = FindLastEntryPoint(name);
		tstrcpy(cinfo->Jfname, entry);

		if ( (cpdtype == CF_DIB) || (cpdtype == CF_DIBV5) ){
			SaveBmpData(hFile, dumpdata, size);
		}else if ( size ){
			if ( cpdtype == CF_TEXT ){
				size = strlen32(dumpdata);
			}else if ( cpdtype == CF_UNICODETEXT ){
				DWORD tempsize;

				tempsize = UnicodeToUtf8((WCHAR *)dumpdata, NULL, 0);
				tempbufdata = (char *)PPcHeapAlloc(tempsize);
				if ( tempbufdata != NULL ){
					size = UnicodeToUtf8((WCHAR *)dumpdata, tempbufdata, tempsize);
					if ( size ) size--; // '\0' 除去
					dumpdata = tempbufdata;
				}
			}
		}
		WriteFile(hFile, dumpdata, size, &size, NULL);
		CloseHandle(hFile);
		if ( tempbufdata != NULL ) PPcHeapFree(tempbufdata);
	}else{
		cinfo->Jfname[0] = '\0';
		SetPopMsg(cinfo, POPMSG_GETLASTERROR, NULL);
	}
	GlobalUnlock(hGlobal);
}

void ToggleMenuBar(PPC_APPINFO *cinfo)
{
	if ( !cinfo->combo ){
		cinfo->X_win ^= XWIN_MENUBAR;
		SetCustTable(T("X_win"), cinfo->RegCID, &cinfo->X_win, sizeof(cinfo->X_win));
		SetMenu(cinfo->info.hWnd, (cinfo->X_win & XWIN_MENUBAR) ? cinfo->DynamicMenu.hMenuBarMenu : NULL);
	}else{
		X_combos[0] ^= CMBS_NOMENU;
		SetCustData(T("X_combos"), &X_combos, sizeof X_combos);
		SendMessage(Combo.hWnd, WM_PPXCOMMAND, KCW_setmenu, (LPARAM)X_combos[0]);
	}
}

void ChangeTitleBar(HWND hWnd)
{
	SetWindowLong(hWnd, GWL_STYLE, GetWindowLong(hWnd, GWL_STYLE) ^
			(WS_OVERLAPPEDWINDOW ^ WS_NOTITLEOVERLAPPED));
	SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
}


void AppendLauoutMenu(AppendMenuS *ams, int id, const TCHAR *name, int check)
{
	TCHAR buf[100];

	AppendMenuCheckString(ams->hMenu, *ams->index, name, check);
	wsprintf(buf, T("*layout %d"), id);
	ThAddString(ams->TH, buf);
	(*ams->index)++;
}

HMENU MakeLayoutMenu(PPC_APPINFO *cinfo, HMENU hPopupMenu, DWORD *index, ThSTRUCT *TH)
{
	AppendMenuS ams;

	if ( hPopupMenu == NULL ) hPopupMenu = CreatePopupMenu();
	ams.hMenu = hPopupMenu;
	ams.index = index;
	ams.TH = TH;

	if ( cinfo->combo ){
		AppendLauoutMenu(&ams, PPCLC_XCOMBOS + 11, lstr_title, !(X_combos[0] & CMBS_NOTITLE));
	}else{
		AppendLauoutMenu(&ams, PPCLC_XWIN + 8, lstr_title, !(cinfo->X_win & XWIN_NOTITLE));
	}
	AppendLauoutMenu(&ams, PPCLC_MENU, lstr_menu,
			(!cinfo->combo ? (cinfo->X_win & XWIN_MENUBAR) :
					!(X_combos[0] & CMBS_NOMENU)) );
	AppendLauoutMenu(&ams, PPCLC_XWIN + 5, lstr_status,
			!(cinfo->X_win & XWIN_NOSTATUS));
	if ( !cinfo->combo || !(X_combos[0] & CMBS_COMMONINFO) ){
		AppendLauoutMenu(&ams, PPCLC_XWIN + 6, lstr_info,
				!(cinfo->X_win & XWIN_NOINFO));
	}
	if ( cinfo->combo ){
		AppendLauoutMenu(&ams, PPCLC_XCOMBOS + 8, lstr_toolbar, X_combos[0] & CMBS_TOOLBAR);
		AppendLauoutMenu(&ams, PPCLC_XCOMBOS + 1, lstr_address, X_combos[0] & CMBS_COMMONADDR);
		AppendLauoutMenu(&ams, PPCLC_XCOMBOS + 10, lstr_caption, !(X_combos[0] & CMBS_NOCAPTION));
		AppendLauoutMenu(&ams, PPCLC_XCOMBOS + 9, lstr_header, X_combos[0] & CMBS_HEADER);
	}else{
		AppendLauoutMenu(&ams, PPCLC_XWIN + 4, lstr_toolbar
			, 	cinfo->X_win & XWIN_TOOLBAR);
		AppendLauoutMenu(&ams, PPCLC_XWIN + 7, lstr_header
				, cinfo->X_win & XWIN_HEADER);
	}
	if ( cinfo->combo && (X_combos[0] & CMBS_COMMONTREE) ){
		AppendLauoutMenu(&ams, PPCLC_TREE, lstr_tree
				, Combo.hTreeWnd != NULL);
	}else{
		AppendLauoutMenu(&ams, PPCLC_TREE, lstr_tree
				, cinfo->hTreeWnd != NULL);
	}
	AppendLauoutMenu(&ams, PPCLC_LOG, lstr_log,
			( (Combo.hWnd != NULL) ? (Combo.Report.hWnd != NULL) :
				((hCommonLog != NULL) && IsWindow(hCommonLog)) ) );

	AppendLauoutMenu(&ams, PPCLC_JOBLIST, lstr_joblist,
			PPxCommonCommand(NULL, 0, K_GETJOBWINDOW) );

	AppendLauoutMenu(&ams, PPCLC_XWIN + 2, lstr_scroll
			, !(cinfo->X_win & XWIN_HIDESCROLL));
	if ( !(cinfo->X_win & XWIN_HIDESCROLL) ){
		AppendLauoutMenu(&ams, PPCLC_XWIN + 3, lstr_swapscroll, FALSE);
	}

//	AppendLauoutMenu(&ams, PPCLC_TOUCH, T("Touch mode"), TouchMode);

//	AppendLauoutMenu(&ams, cinfo->X_win & XWIN_HIDETASK, PPCLC_XWIN + 1, T("Simple t&ask bar(tiny)"));
	AppendMenu(hPopupMenu, MF_SEPARATOR, 0, NULL);
	{
		PPXDOCKS *d;
		d = ( cinfo->combo ) ? &comboDocks : &cinfo->docks;
		AppendMenu(hPopupMenu, MF_EPOP,
				(UINT_PTR)MakeDockMenu(&d->t, NULL, ams.index, ams.TH),
				MessageText(lstr_docktop));
		AppendMenu(hPopupMenu, MF_EPOP,
				(UINT_PTR)MakeDockMenu(&d->b, NULL, ams.index, ams.TH),
				MessageText(lstr_dockbottom));
	}
	AppendLauoutMenu(&ams, PPCLC_WINDOW, lstr_windowmenu, FALSE);
	AppendLauoutMenu(&ams, PPCLC_RUNCUST, lstr_cust, FALSE);
	return hPopupMenu;
}

void PPcEnterTabletMode(PPC_APPINFO *cinfo)
{
	TouchMode = ~X_pmc[1];
	PPxCommonCommand(cinfo->info.hWnd, 0, K_E_TABLET);
	if ( cinfo->combo  ){
		SendMessage(Combo.hWnd, WM_PPXCOMMAND, K_E_TABLET, 0);
	}else{
		if ( cinfo->hTreeWnd != NULL ){
			SendMessage(cinfo->hTreeWnd, VTM_CHANGEDDISPDPI, TMAKEWPARAM(0, cinfo->FontDPI), 0);
		}
	}
}

void PPcLayoutCommand(PPC_APPINFO *cinfo, const TCHAR *param)
{
	int id;

	id = GetNumber(&param);
	if ( (id <= 0) || (id >= PPCLC_MAX) ){
		PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, T("%M?layoutmenu"), NULL, 0);
		return;
	}
	if ( id == PPCLC_RUNCUST ){		// detail ****************
		PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, T("%Obqs *ppcust /:X_combo="), NULL, 0);
	}else if ( id == PPCLC_WINDOW ){		// Window Option ****************
		PPC_window(cinfo->info.hWnd);
	}else if ( id == PPCLC_TREE ){		// Tree ****************
		PPC_Tree(cinfo, PPCTREE_SYNC);
	}else if ( id == PPCLC_LOG ){		// Log ****************
		if ( Combo.hWnd != NULL ){
			resetflag(X_combos[0], CMBS_COMMONREPORT);
			if ( Combo.Report.hWnd == NULL ) setflag(X_combos[0], CMBS_COMMONREPORT);
			SetCustData(T("X_combos"), &X_combos, sizeof X_combos);
			SendMessage(Combo.hWnd, WM_PPXCOMMAND, K_WINDDOWLOG, (LPARAM)INVALID_HANDLE_VALUE);
		}else{
			if ( (hCommonLog == NULL) || (IsWindow(hCommonLog) == FALSE) ){
				DockAddBar(cinfo->info.hWnd, &cinfo->docks.b, RMENUSTR_LOG);
			}else{
				if ( DockCommands(cinfo->info.hWnd, &cinfo->docks.b, dock_delete, RMENUSTR_LOG) == FALSE ){
					PostMessage(GetParent(hCommonLog), WM_CLOSE, 0, 0);
				}
				hCommonLog = NULL;
			}
		}
	}else if ( id == PPCLC_JOBLIST ){	// Job list
		if ( cinfo->combo == 0 ){
			if ( DockAddBar(cinfo->info.hWnd, &cinfo->docks.b, RMENUSTR_JOB) == FALSE ){
				DockCommands(cinfo->info.hWnd, &cinfo->docks.b, dock_delete, RMENUSTR_JOB);
			}
		}else{
			SendMessage(Combo.hWnd, WM_PPXCOMMAND, KCW_showjoblist, 0);
		}
	}else if ( id == PPCLC_MENU ){
		ToggleMenuBar(cinfo);
	}else if ( id == PPCLC_TOUCH ){
		TouchMode = TouchMode ? 0 : 1;
		if ( TouchMode ) PPcEnterTabletMode(cinfo);
		InitCli(cinfo);
		Repaint(cinfo);
	}else if ( id >= PPCLC_XCOMBOS ){
		X_combos[0] ^= 1 << (id - PPCLC_XCOMBOS);
		SetCustData(T("X_combos"), &X_combos, sizeof X_combos);
		if ( id == PPCLC_XCOMBOS + 9 ){
			PPxPostMessage(WM_PPXCOMMAND,  K_Lcust,  GetTickCount());
		}else{
			SendMessage(cinfo->hComboWnd,  WM_PPXCOMMAND,  K_Lcust, (LPARAM)cinfo->info.hWnd);
		}
		if ( id == PPCLC_XCOMBOS + 11 ) ChangeTitleBar(cinfo->hComboWnd);
		PeekLoop(); // この中で K_Lcust が実行される
		SendMessage(cinfo->hComboWnd, WM_PPXCOMMAND, KCW_layout, 0);
	}else if ( id >= PPCLC_XWIN ){		// X_win ****************
		cinfo->X_win ^= 1 << (id - PPCLC_XWIN);
		SetCustTable(T("X_win"), cinfo->RegCID, &cinfo->X_win, sizeof cinfo->X_win);
		if ( (id == PPCLC_XWIN + 2) || (id == PPCLC_XWIN + 3) ){ // scrollbar
			// XWIN_HIDESCROLL || XWIN_SWAPSCROLL
			cinfo->ScrollBarHV = cinfo->X_win & XWIN_SWAPSCROLL ? SB_VERT : SB_HORZ;
			if ( XC_page ) cinfo->ScrollBarHV ^= (SB_HORZ | SB_VERT);
			HideScrollBar(cinfo);
			cinfo->oldspos.nPage = (UINT)-1; // スクロールバー更新させる
			if ( cinfo->hScrollBarWnd != NULL ){
				DestroyWindow(cinfo->hScrollBarWnd);
				CreateScrollBar(cinfo);
			}
		}
		if ( (id == PPCLC_XWIN + 4) || (id == PPCLC_XWIN + 7) ){ // toolbar/hdr
			// XWIN_TOOLBAR || XWIN_HEADER
			CloseGuiControl(cinfo);
			InitGuiControl(cinfo);
		}
		// XWIN_NOTITLE
		if ( id == PPCLC_XWIN + 8 ) ChangeTitleBar(cinfo->info.hWnd);
	}
	WmWindowPosChanged(cinfo);
}

void WriteStdoutChooseName(TCHAR *name)
{
	SIZE32_T len;
	char *bufA = NULL, *src;
	DWORD tmp;
	#ifdef UNICODE
		if ( (X_ChooseMode == CHOOSEMODE_CON_UTF16) ||
			 (X_ChooseMode == CHOOSEMODE_MULTICON_UTF16) ){
			src = (char *)name;
			len = strlenW32(name) * (SIZE32_T)sizeof(WCHAR);
		}else{
			UINT codepage = CP_ACP;

			if ( (X_ChooseMode == CHOOSEMODE_CON_UTF8) ||
				 (X_ChooseMode == CHOOSEMODE_MULTICON_UTF8) ){
				 codepage = CP_UTF8;
			}
			len = WideCharToMultiByte(codepage, 0, name, -1, NULL, 0, NULL, NULL) + 1;
			bufA = (char *)PPcHeapAlloc( len );
			if ( bufA == NULL ) return;
			src = bufA;
			len = WideCharToMultiByte(codepage, 0, name, -1, bufA, len, NULL, NULL);
			if ( len != 0 ) len--;
		}
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), src, len, &tmp, NULL);
	#else
		WCHAR *bufW = NULL;
		DWORD lengthA, lengthW;

		if ( (X_ChooseMode == CHOOSEMODE_CON) ||
			 (X_ChooseMode == CHOOSEMODE_MULTICON) ){
			src = (char *)name;
			len = strlen(name);
		}else{ // UTF16/UTF8
			lengthW = AnsiToUnicode(name, NULL, 0) + 1;
			bufW = (WCHAR *)PPcHeapAlloc( lengthW * sizeof(WCHAR) );
			if ( bufW == NULL ) return;
			lengthW = AnsiToUnicode(name, bufW, lengthW);
			src = (char *)bufW;
			len = (lengthW != 0) ? ((lengthW - 1) * sizeof(WCHAR)) : 0;
			if ( (X_ChooseMode == CHOOSEMODE_CON_UTF8) ||
				 (X_ChooseMode == CHOOSEMODE_MULTICON_UTF8) ){
				lengthA = UnicodeToUtf8(bufW, NULL, 0) + 1;
				bufA = (char *)PPcHeapAlloc( lengthA );
				if ( bufA != NULL ){
					src = bufA;
					len = UnicodeToUtf8(bufW, bufA, lengthA);
					if ( len != 0 ) len--;
				}
			}
		}
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), src, len, &tmp, NULL);
		if ( bufW != NULL ) PPcHeapFree(bufW);
	#endif
	if ( bufA != NULL ) PPcHeapFree(bufA);
}

void DoChooseResult(PPC_APPINFO *cinfo, TCHAR *Param)
{
	TCHAR buf[VFPS];
	ERRORCODE result = NO_ERROR;

	ThGetString(NULL, T("CHOOSE"), buf, VFPS);
	if ( buf[0] == '\0' ) tstrcpy(buf, T("%#FCD"));
	PP_InitLongParam(Param);

	switch(X_ChooseMode){
		case CHOOSEMODE_EDIT:
			result = PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, buf, Param, XEO_DISPONLY | XEO_EXTRACTLONG);
			if ( result == ERROR_PARTIAL_COPY ){
				wsprintf(buf, T("Text length (%d) seems long, continue?"), tstrlen(PP_GetLongParamRAW(Param)) );
				if ( IDYES != PMessageBox(cinfo->info.hWnd, buf, NULL, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) ){
					break;
				}
			}
			SendMessage(hChooseWnd, WM_SETTEXT, 0, (LPARAM)PP_GetLongParam(Param, result) );
			break;

		case CHOOSEMODE_DD:
			StartAutoDD(cinfo, hChooseWnd, NULL, DROPTYPE_LEFT | DROPTYPE_HOOK);
			break;

		case CHOOSEMODE_CON_UTF8:
		case CHOOSEMODE_CON_UTF16:
		case CHOOSEMODE_CON: {
			result = PP_ExtractMacro(cinfo->info.hWnd, &cinfo->info, NULL, buf, Param, XEO_DISPONLY | XEO_EXTRACTLONG);
			WriteStdoutChooseName(PP_GetLongParam(Param, result));
			break;
		}

		case CHOOSEMODE_MULTICON_UTF8:
		case CHOOSEMODE_MULTICON_UTF16:
		case CHOOSEMODE_MULTICON: {
			ENTRYCELL *cell;
			int work;

			InitEnumMarkCell(cinfo, &work);
			while ( (cell = EnumMarkCell(cinfo, &work)) != NULL ){
				VFSFullPath(buf, (TCHAR *)GetCellFileName(cinfo, cell, buf), cinfo->RealPath);
				if ( tstrchr(buf, ' ') ){
					wsprintf(Param, T("\"%s\"\r\n"), buf);
				}else{
					wsprintf(Param, T("%s\r\n"), buf);
				}
				WriteStdoutChooseName(Param);
			}
			break;
		}
	}
	PP_FreeLongParam(Param, result);
	PostMessage(cinfo->info.hWnd, WM_CLOSE, 0, 0);
}

void GetDriveVolumeName(PPC_APPINFO *cinfo)
{
	TCHAR path[VFPS];

	if ( cinfo->RequestVolumeLabel == FALSE ) return;
	cinfo->RequestVolumeLabel = FALSE;
	cinfo->VolumeLabel[0] = '\0';

	GetDriveName(path, cinfo->RealPath);
	GetVolumeInformation(path, cinfo->VolumeLabel, MAX_PATH, NULL, NULL, NULL, NULL, 0);
}

#if !NODLL
TCHAR * FindLastEntryPoint(const TCHAR *src)
{
	const TCHAR *rp, *tp;

	rp = VFSGetDriveType(src, NULL, NULL);	// ドライブ指定をスキップ
	if ( rp == NULL ) rp = src;	// ドライブ指定が無い
	if ( *rp ){				// root なら *rp == 0
		tp = rp;
		for ( ;; ){
			TCHAR type;

			type = *tp;
			if ( type == '\\' ){
				rp = tp + 1;
			}else if ( type == '\0' ){
				break;
#ifndef UNICODE
			}else{
				if ( Iskanji(type) ) tp++;
#endif
			}
			tp++;
		}
	}
	return (TCHAR *)rp;
}
#endif

void USEFASTCALL PeekLoop(void)
{
	MSG msg;

	while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ){
		if ( msg.message == WM_QUIT ) break;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

// ,  区切りのパラメータを１つ取得する ※PPD_CMDL.Cにも
#if !NODLL
UTCHAR GetCommandParameter(LPCTSTR *commandline, TCHAR *param, size_t paramlen)
{
	const TCHAR *src;
	TCHAR *dest, *destmax;
	UTCHAR firstcode, code;

	firstcode = SkipSpace(commandline);
	if ( (firstcode == '\0') || (firstcode == ',') ){ // パラメータ無し
		*param = '\0';
		return firstcode;
	}
	if ( firstcode == '\"' ) return GetLineParam(commandline, param);
	src = *commandline + 1;
	dest = param;
	destmax = dest + paramlen - 1;
	code = firstcode;
	for ( ;; ){
		if ( dest < destmax ) *dest++ = code;
		code = *src;
		if ( (code == ',') || // (code == ' ') ||
			 ((code < ' ') && ((code == '\0') || (code == '\t') ||
							   (code == '\r') || (code == '\n'))) ){
			break;
		}
		src++;
	}
	while ( (dest > param) && (*(dest - 1) == ' ') ) dest--;
	*dest = '\0';
	*commandline = src;
	return firstcode;
}
#endif

UTCHAR GetCommandParameterDual(LPCTSTR *commandline, TCHAR *param, size_t paramlen)
{
	const TCHAR *src;
	TCHAR *dest, *destmax;
	UTCHAR firstcode, code;

	firstcode = SkipSpace(commandline);
	if ( (firstcode == '\0') || (firstcode == ',') ){ // パラメータ無し
		*param = '\0';
		return firstcode;
	}
	if ( firstcode == '\"' ) return GetLineParam(commandline, param);
	src = *commandline + 1;
	dest = param;
	destmax = dest + paramlen - 1;
	code = firstcode;
	for ( ;; ){
		*dest++ = code;
		code = *src;
		if ( code == ' ' ){
			if ( tstrchr(src, ',') == NULL ) break;
			src++;
			continue;
		}
		if ( (dest >= destmax) || (code == ',') ||
			 ((code < ' ') && ((code == '\0') || (code == '\t') ||
							   (code == '\r') || (code == '\n'))) ){
			break;
		}
		src++;
	}
	while ( (dest > param) && (*(dest - 1) == ' ') ) dest--;
	*dest = '\0';
	*commandline = src;
	return firstcode;
}

void PPcChangeDirectory(PPC_APPINFO *cinfo, const TCHAR *newpath, DWORD flags)
{
	if ( IsTrue(cinfo->ChdirLock) && !(flags & RENTRY_NOLOCK) ){
		PPCuiWithPathForLock(cinfo, newpath);
		return;
	}
	SetPPcDirPos(cinfo);
	tstrcpy(cinfo->path, newpath);
	read_entry(cinfo, flags);
}

void JumpPathEntry(PPC_APPINFO *cinfo, const TCHAR *newpath, DWORD flags)
{
	TCHAR *p, path[VFPS];
	TCHAR cmdline[CMDLINESIZE];

	VFSFullPath(path, (TCHAR *)newpath,
			( (cinfo->e.Dtype.mode == VFSDT_LFILE) &&
			  (cinfo->e.Dtype.BasePath[0] != '\0') ) ?
				cinfo->e.Dtype.BasePath : cinfo->path );
	if ( IsTrue(cinfo->ChdirLock) ){
		wsprintf(cmdline, T("/k *jumppath \"%s\" /entry /nolock"), path);
		if ( cinfo->combo != 0 ){
			CallPPcParam(Combo.hWnd, cmdline);
		}else{
			PPCui(cinfo->info.hWnd, cmdline);
		}
		return;
	}
	p = VFSFindLastEntry(path);
	tstrcpy(cinfo->Jfname, (*p == '\\') ? p + 1 : p);
	*p = '\0';
	PPcChangeDirectory(cinfo, path, flags);
}

#if !NODLL
DWORD GetReparsePath(const TCHAR *path, TCHAR *pathbuf)
{
	HANDLE hFile;
	REPARSE_DATA_IOBUFFER rdio;
	DWORD size;

	*pathbuf = '\0';
	hFile = CreateFileL(path, 0, 0, NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ) return 0;

	if ( IsTrue(DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT,
			NULL, 0, &rdio, sizeof(rdio), &size, NULL)) ){
		WCHAR *src;

		src = rdio.ReparseGuid.PathBuffer + (rdio.ReparseGuid.SubstituteNameOffset / sizeof(WCHAR));
		if ( rdio.ReparseTag == IO_REPARSE_TAG_SYMLINK ) src += 2;

		if ( (((BYTE *)src - (BYTE *)&rdio) + rdio.ReparseGuid.SubstituteNameLength) >= sizeof(rdio) ){
			rdio.ReparseTag = 0;
		}else{
			src[rdio.ReparseGuid.SubstituteNameLength / sizeof(WCHAR)] = '\0';
			if ( memcmp(src, L"\\??\\", 4 * sizeof(WCHAR)) == 0 ) src += 4;
			strcpyWToT(pathbuf, src, VFPS);
		}
	}
	CloseHandle(hFile);
	return rdio.ReparseTag;
}
#else
extern DWORD GetReparsePath(const TCHAR *path, TCHAR *pathbuf);
#endif

void USEFASTCALL tstrlimcpy(TCHAR *dest, const TCHAR *src, DWORD maxlength)
{
	TCHAR code;
	const TCHAR *srcmax;

	srcmax = src + maxlength - 1;
	while( src < srcmax ){
		code = *src++;
		if ( code == '\0' ) break;
		*dest++ = code;
	}
	*dest = '\0';
}

#pragma argsused
void WINAPI DummyNotifyWinEvent(DWORD event, HWND hwnd, LONG idObject, LONG idChild)
{
	UnUsedParam(event);UnUsedParam(hwnd);UnUsedParam(idObject);UnUsedParam(idChild);
}

void USEFASTCALL CreateNewPane(const TCHAR *param)
{
	if ( (param == NULL) || (*param == '\0') ){
		CallPPcParam(Combo.hWnd, T("-pane"));
	}else{
		TCHAR buf[CMDLINESIZE];

		wsprintf(buf, T("-pane %s"), param);
		CallPPcParam(Combo.hWnd, buf);
	}
}

// 画面内にマークがなければ警告する
int CheckOffScreenMark(PPC_APPINFO *cinfo, const TCHAR *title)
{
	ENTRYINDEX maxentries;
	ENTRYINDEX n;

	if ( cinfo->e.markC == 0 ) return IDYES; // マーク無し

	maxentries = n = cinfo->cellWMin;
	maxentries += cinfo->cel.Area.cx * cinfo->cel.Area.cy;
	if ( maxentries > cinfo->e.cellIMax ) maxentries = cinfo->e.cellIMax;
	for ( ; n < maxentries ; n++ ){
		if ( IsCEL_Marked(n) ) return IDYES; // 画面内にマークあり
	}
	return PMessageBox(cinfo->info.hWnd, MES_QCOM, title, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
}

void BackupLog(void)
{
	DWORD size;

	if ( hCommonLog == NULL ) return;

	size = GetWindowTextLength(hCommonLog) + 1;
	CommonLogBackup = PPcHeapAlloc(TSTROFF(size));
	if ( CommonLogBackup != NULL ){
		GetWindowText(hCommonLog, CommonLogBackup, size);
	}
}
void RestoreLog(void)
{
	if ( CommonLogBackup == NULL ) return;
	if ( hCommonLog != NULL ){
		SetWindowText(hCommonLog, CommonLogBackup);
		SendMessage(hCommonLog, EM_SETSEL, EC_LAST, EC_LAST);
		SendMessage(hCommonLog, EM_SCROLLCARET, 0, 0);
	}
	PPcHeapFree(CommonLogBackup);
	CommonLogBackup = NULL;
}

// ●VFSFindLastEntry に再統合予定
TCHAR *FindBothLastEntry(const TCHAR *path)
{
	TCHAR *sep, sepchar, *src;

	sep = FindPathSeparator(path);
	if ( sep == NULL ) return (TCHAR *)(path + tstrlen(path));
	sepchar = *sep;
	src = sep;
	for ( ; ; ){
		TCHAR type;

		type = *src;
		if ( type == sepchar ) sep = src;
		if ( type == '\0' ) return (TCHAR *)sep;
#ifndef UNICODE
		if ( Iskanji(type) ) src++;
#endif
		src++;
	}
}

void AppendPath(TCHAR *path, const TCHAR *appendpath, TCHAR sepchar)
{
	TCHAR *sep;	// 「\」の位置を覚えておく
	TCHAR *readp;
	TCHAR sepchr = sepchar;
	int len;

	sep = path;

	readp = path;
	while ( *readp ){
		if ( *readp == sepchr ) sep = readp + 1;
		#ifndef UNICODE
			if ( IskanjiA(*readp++) ){
				if ( *readp ) readp++;
			}
		#else
			readp++;
		#endif
	}
	if ( sep != readp ) *(readp++) = sepchr;

	len = tstrlen32(appendpath) + 1;
	if ( (readp - path + len) >= VFPS ){
		tstrcpy(path, T("<too long>"));
		return;
	}
	memcpy(readp, appendpath, TSTROFF(len));
}

void TinyGetMenuPopPos(HWND hWnd, POINT *pos)
{
	RECT box;

	GetMessagePosPoint(*pos);
	if ( hWnd == NULL ) return;
	GetWindowRect(hWnd, &box);
	if ( (box.left > pos->x) || (box.top > pos->y) ||
		 (box.right < pos->x) || (box.bottom < pos->y) ){
		pos->x = (box.left + box.right) / 2;
		pos->y = (box.top + box.bottom) / 2;
	}
}

const TCHAR BlockKey[] = T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Blocked");

BOOL IsShellExBlocked(const TCHAR *ClsID)
{
	TCHAR buf[64];

	if ( (GetRegString(HKEY_LOCAL_MACHINE, BlockKey, ClsID, buf, TSIZEOF(buf)) == FALSE) &&
		 (GetRegString(HKEY_CURRENT_USER, BlockKey, ClsID, buf, TSIZEOF(buf)) == FALSE) ){
		return FALSE;
	}
	return TRUE;
}

#if !NODLL
void tstrreplace(TCHAR *text, const TCHAR *targetword, const TCHAR *replaceword)
{
	TCHAR *p;
	size_t tlen, rlen;

	if ( (p = tstrstr(text, targetword)) == NULL ) return;
	tlen = tstrlen(targetword);
	rlen = tstrlen(replaceword);

	for (;;){
		if ( tlen != rlen ) memmove(p + rlen, p + tlen, TSTRSIZE(p + tlen));
		memcpy(p, replaceword, TSTROFF(rlen));
		text = p + rlen;
		if ( (p = tstrstr(text, targetword)) != NULL ) continue;
		break;
	}
}

BOOL IsShowButtonMenu(void)
{
	if ( (GetTickCount() - ButtonMenuTick) >= SKIPBUTTONMENUTIME ) return TRUE;
	ButtonMenuTick = 0;
	return FALSE;
}

void EndButtonMenu(void)
{
	ButtonMenuTick = (GetAsyncKeyState(VK_LBUTTON) & KEYSTATE_PUSH) ? GetTickCount() : 0;
}

#ifndef __BORLANDC__
char *stpcpyA(char *deststr, const char *srcstr)
{
	char *destptr = deststr;
	const char *srcptr = srcstr;

	for(;;){
		char code;

		code = *srcptr;
		*destptr = code;
		if ( code == '\0' ) return destptr;
		srcptr++;
		destptr++;
	}
}
#endif

WCHAR *stpcpyW(WCHAR *deststr, const WCHAR *srcstr)
{
	WCHAR *destptr = deststr;
	const WCHAR *srcptr = srcstr;

	for(;;){
		WCHAR code;

		code = *srcptr;
		*destptr = code;
		if ( code == '\0' ) return destptr;
		srcptr++;
		destptr++;
	}
}
#endif
