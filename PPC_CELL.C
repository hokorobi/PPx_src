/*-----------------------------------------------------------------------------
	Paper Plane cUI											対 Cell 処理
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "PPCUI.RH"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPC_DD.H"
#pragma hdrstop
						// 通常の指定に使う設定
const CURSORMOVER DefCM_page = { 0, 0, OUTTYPE_PAGE, 0, OUTTYPE_STOP, OUTHOOK_EDGE};
const CURSORMOVER DefCM_scroll = { 0, 0, OUTTYPE_LINESCROLL, 0, OUTTYPE_STOP, OUTHOOK_EDGE};

DWORD GetFileHeader(const TCHAR *filename, BYTE *header, DWORD headersize)
{
	HANDLE hFile;
	DWORD fsize;

	hFile = CreateFileL(filename, GENERIC_READ,
			FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ) return 0;

	fsize = ReadFileHeader(hFile, header, headersize);
	CloseHandle(hFile);
	return fsize;
}

// ファイルをディレクトリ扱いにするか判定 -------------------------------------
BOOL IsFileDir(PPC_APPINFO *cinfo, const TCHAR *filename, TCHAR *newpath, TCHAR *newjumpname, HMENU hPopupMenu)
{
	BYTE header[VFS_check_size];
	DWORD fsize;					// 読み込んだ大きさ
	int i;
	TCHAR pathbuf[VFPS];

	if ( VFSFullPath(pathbuf, (TCHAR *)filename, cinfo->RealPath) == NULL ){
		if ( cinfo->RealPath[0] != '?' ) return FALSE;

		pathbuf[0] = '\0';
		if ( VFSFullPath(pathbuf, (TCHAR *)filename, cinfo->path) != NULL ){
			if( VFSGetRealPath(cinfo->info.hWnd, pathbuf, pathbuf) == FALSE ){
				return FALSE;
			}
		}
		if ( pathbuf[0] == '\0' ) return FALSE;
	}
										// CLSID 形式 -------------------------
	i = FindExtSeparator(filename);
	if ( (*(filename + i) == '.') && (*(filename + i + 1) == '{') &&
			(tstrlen(filename + i) == 39)){
		if ( newpath != NULL ){
			CatPath(NULL, newpath, filename);
			*newjumpname = '\0';
		}
		if ( hPopupMenu == NULL ) return TRUE;
		AppendMenuString(hPopupMenu, CRID_DIRTYPE_CLSID, MES_JNCL);
		AppendMenuString(hPopupMenu, CRID_DIRTYPE_CLSID_PAIR, MES_JPCL);
	}
	fsize = GetFileHeader(pathbuf, header, sizeof(header));
	if ( fsize == 0 ){
		if ( hPopupMenu != NULL ){
			VFSCheckDir(pathbuf, header, fsize | VFSCHKDIR_GETDIRMENU, (void **)hPopupMenu);
			AppendMenuString(hPopupMenu, CRID_DIRTYPE_PAIR, MES_JPDI);
		}
		return FALSE;
	}
//------------------------------------------------ ショートカットファイルの判別
										// Link ?
	if ( (fsize > 100) &&
		 (*header == 0x4c) &&
		 (*(header + 1) == 0) &&
		 (*(header + 2) == 0) ){
		if ( newpath != NULL ){
			TCHAR lname[VFPS], lxname[VFPS];

			if ( SUCCEEDED(GetLink(cinfo->info.hWnd, pathbuf, lname)) && lname[0] ){
				DWORD attr;

				lxname[0] = '\0';
				ExpandEnvironmentStrings(lname, lxname, TSIZEOF(lxname));
				attr = GetFileAttributesL(lxname);
				if ( (attr & FILE_ATTRIBUTE_DIRECTORY) && (attr != BADATTR) ){
					tstrcpy(newpath, lxname);
					*newjumpname = '\0';
				}else{
					TCHAR *p;

					p = VFSFindLastEntry(lxname);
					tstrcpy(newjumpname, (*p == '\\') ? p + 1 : p);
					*p = '\0';
					tstrcpy(newpath, lxname);
				}
				return TRUE;
			}else{
				if ( hPopupMenu == NULL ){
					SetPopMsg(cinfo, POPMSG_MSG, MES_ESCT);
					return FALSE;
				}
			}
		}
		if ( hPopupMenu == NULL ) return TRUE;
		AppendMenuString(hPopupMenu, CRID_DIRTYPE_SHORTCUT, MES_JNSH);
		AppendMenuString(hPopupMenu, CRID_DIRTYPE_SHORTCUT_PAIR, MES_JPSH);
	}
//---------------------------------------------------------- VFS系の判別
	if ( VFSCheckDir(pathbuf, header, fsize | VFSCHKDIR_GETDIRMENU, (void **)hPopupMenu) ){
		if ( newpath != NULL ){
			tstrcpy(newpath, pathbuf);
			*newjumpname = '\0';
		}
		if ( hPopupMenu == NULL ) return TRUE;
	}else{
		if ( hPopupMenu == NULL ) return FALSE;
	}
//------------------------------------------------------------ ストリーム
	{
		HANDLE hFile;
		WIN32_STREAM_ID stid;
		DWORD size, reads;
		LPVOID context = NULL;
		WCHAR wname[VFPS];

		hFile = CreateFileL(pathbuf, GENERIC_READ, FILE_SHARE_READ, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if ( hFile != INVALID_HANDLE_VALUE ){
			for ( ; ; ){
				size = (LPBYTE)&stid.cStreamName - (LPBYTE)&stid;
				if ( FALSE == BackupRead(hFile, (LPBYTE)&stid,
						size, &reads, FALSE, FALSE, &context) ){
					break;
				}
				if ( reads < size ) break;
				if ( FALSE == BackupRead(hFile, (LPBYTE)&wname,
						stid.dwStreamNameSize, &reads, FALSE, FALSE, &context) ){
					break;
				}
				if ( reads < stid.dwStreamNameSize ) break;

				if ( stid.dwStreamNameSize ){
					AppendMenuString(hPopupMenu, CRID_DIRTYPE_STREAM, MES_JNST);
					AppendMenuString(hPopupMenu, CRID_DIRTYPE_STREAM_PAIR, MES_JPST);
					break;
//				}else{	// default name ... 登録不要
//
				}
				if ( FALSE == BackupSeek(hFile, *(DWORD *)(&stid.Size),
						*((DWORD *)(&stid.Size) + 1), &reads, &reads, &context) ){
					break;
				}
			}
			if ( context != NULL ){
				BackupRead(hFile, (LPBYTE)&stid, 0, &reads, TRUE, FALSE, &context);
			}
			CloseHandle(hFile);
		}
	}
	AppendMenuString(hPopupMenu, CRID_DIRTYPE_PAIR, MES_JPDI);
	return TRUE;
}

//-----------------------------------------------------------------------------
// ディレクトリかどうかを判別し、ファイルならフルパス名を生成する
ERRORCODE DirChk(const TCHAR *name, TCHAR *dest)
{
	TCHAR buf[VFPS], *wp;

	if ( GetFileAttributesL(name) == BADATTR ){
		ERRORCODE ec;

		ec = GetLastError();
		if ( (ec == ERROR_FILE_NOT_FOUND) ||
			 (ec == ERROR_PATH_NOT_FOUND) ||
			 (ec == ERROR_BAD_NETPATH) ||
			 (ec == ERROR_DIRECTORY) ){		// path not found
			tstrcpy(buf, name);
			wp = VFSFindLastEntry(buf);
			if ( *wp != '\0' ){
				*wp = '\0';
				if ( ec == ERROR_PATH_NOT_FOUND ) VFSChangeDirectory(NULL, buf);
				ec = DirChk(buf, dest);
			}
		}
		return ec;
	}else{
		VFSFullPath(dest, (TCHAR *)name, dest);
		return NO_ERROR;
	}
}

//-----------------------------------------------------------------------------
// ディレクトリ移動処理
BOOL CellLook(PPC_APPINFO *cinfo, int IsFileRead)
{
	TCHAR temp[VFPS], *p;
	BOOL arcmode = FALSE;
	ENTRYCELL *cell;

	cell = &CEL(cinfo->e.cellN);
	if ( cell->attr & ECA_THIS ){	// "."
		PPC_RootDir(cinfo);
		return TRUE;
	}
	if ( cell->attr & ECA_PARENT ){	// ".."
		PPC_UpDir(cinfo);
		return TRUE;
	}
	// 書庫内書庫の判定＆展開
	if ( IsArcInArc(cinfo) || (IsFileRead == -2) ){
		arcmode = OnArcPathMode(cinfo);
	}
																	// <dir>
	if ( cell->f.dwFileAttributes &
			(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTEX_FOLDER) ){
		if ( IsTrue(cinfo->ChdirLock) ){
			if ( VFSFullPath(temp, CellFileName(cell), cinfo->path) == NULL ){
				SetPopMsg(cinfo, POPMSG_GETLASTERROR, NULL);
				return TRUE;
			}
			PPCuiWithPathForLock(cinfo, temp);
			return TRUE;
		}

		if ( cinfo->UseArcPathMask != ARCPATHMASK_OFF ){
			SavePPcDir(cinfo, TRUE);
			DxSetMotion(cinfo->DxDraw, DXMOTION_DownDir);

			tstrcpy(cinfo->ArcPathMask, CellFileName(cell));
			cinfo->ArcPathMaskDepth = GetEntryDepth(cinfo->ArcPathMask, NULL) + 1;
			cinfo->e.cellN = 0;
			MaskEntryMain(cinfo, &cinfo->mask, NilStr);

			if ( X_acr[1] ){ // カーソル位置を再現
				const TCHAR *vp;

				VFSFullPath(temp, cinfo->ArcPathMask, cinfo->OrgPath[0] ? cinfo->OrgPath : cinfo->path);
				UsePPx();
				vp = SearchHistory(PPXH_PPCPATH, temp);
				if ( vp != NULL ){
					const int *histp;

					histp = (const int *)GetHistoryData(vp);
					if ( (histp[1] >= 0) && (histp[1] < cinfo->e.cellIMax) ){
						cinfo->cellWMin = histp[1];
						InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
						if ( (histp[0] >= 0) && (histp[1] < cinfo->e.cellIMax) ){
							MoveCellCsr(cinfo, histp[0] - cinfo->e.cellN, NULL);
						}
					}
				}
				FreePPx();
			}
			SetCaption(cinfo);
			return TRUE;
		}
		SetPPcDirPos(cinfo);

#ifndef UNICODE
		if ( (strchr(CellFileName(cell), '?') != NULL) &&
			cell->f.cAlternateFileName[0] ){
			if ( VFSFullPath(cinfo->path,
					cell->f.cAlternateFileName, cinfo->path) == NULL ){
				SetPopMsg(cinfo, POPMSG_GETLASTERROR, NULL);
				return TRUE;
			}
		}else
#endif
		if ( VFSFullPath(cinfo->path, CellFileName(cell), cinfo->path) == NULL ){
			SetPopMsg(cinfo, POPMSG_GETLASTERROR, NULL);
			return TRUE;
		}
										// PIDL UNC → FS UNC
		if ( (cinfo->path[0] == '\\') &&
			 (cinfo->path[1] == '\\') &&
			 (cinfo->e.Dtype.mode == VFSDT_SHN) ){
			TCHAR buf[VFPS];

			buf[0] = '\0';
			VFSGetRealPath(cinfo->info.hWnd, buf, cinfo->path);
			if( (buf[0] == '\\') && !tstrstr(buf, T("::")) ){
				tstrcpy(cinfo->path, buf);
			}
		}
		DxSetMotion(cinfo->DxDraw, DXMOTION_DownDir);

		read_entry(cinfo, RENTRY_READ | RENTRY_ENTERSUB);
		return TRUE;
	}
	tstrcpy(temp, cinfo->path);
	p = CellFileName(cell);
	if ( IsTrue(cinfo->UnpackFix) ) p = FindLastEntryPoint(p);
	if ( IsFileRead && IsFileDir(cinfo, p, temp, cinfo->Jfname, NULL)){
		if ( IsTrue(cinfo->ChdirLock) ){
			PPCuiWithPathForLock(cinfo, temp);
			return TRUE;
		}
		DxSetMotion(cinfo->DxDraw, DXMOTION_DownDir);

		SavePPcDir(cinfo, FALSE);
		if ( arcmode && (cinfo->OrgPath[0] == '\0') ){ // 書庫内書庫から出たときの場所を記憶する
			VFSFullPath(cinfo->OrgPath, CellFileName(cell),
					cinfo->UnpackFixOldPath);
			OffArcPathMode(cinfo);
		}
		tstrcpy(cinfo->path, temp);
		if ( *cinfo->Jfname ){
			read_entry(cinfo, RENTRY_JUMPNAME);
		}else{
			read_entry(cinfo, RENTRY_READ);
		}
	}else{
		if ( IsTrue(arcmode) ) OffArcPathMode(cinfo);
		return FALSE;
	}
	return TRUE;
}

//-----------------------------------------------------------------------------
// セル移動した時に行う処理
BOOL NewCellN(PPC_APPINFO *cinfo, int OcellN, BOOL fulldraw)
{
	TCHAR viewname[VFPS];

	if ( VFSFullPath(viewname, (TCHAR *)GetCellFileName(cinfo, &CEL(cinfo->e.cellN), viewname), cinfo->path) == NULL ){
		cinfo->OldIconsPath[0] = '\0';
		if ( fulldraw == FALSE ){
			if ( !cinfo->FullDraw ){
				UpdateWindow_Part(cinfo->info.hWnd);	// 入れたほうが早い？
			}
			setflag(cinfo->DrawTargetFlags, DRAWT_1ENTRY);
			RefleshCell(cinfo, OcellN);
			if ( !cinfo->FullDraw ){
				UpdateWindow_Part(cinfo->info.hWnd);	// 入れたほうが早い？
			}
		}
		return TRUE;	// 表示できない
	}
															// 変化無し
	if ( (cinfo->e.cellN == OcellN) && !tstrcmp(viewname, cinfo->OldIconsPath) ){
		return FALSE;
	}
									// ドライブ一覧→タイトルバー修正
	if ( cinfo->path[0] == ':' ) SetCaption(cinfo);

	HideEntryTip(cinfo);
	if ( X_stip[TIP_LONG_TIME] != 0 ){
		setflag(cinfo->Tip.states, STIP_REQ_DELAY);
	}
									// カーソル位置のセルを描画
	tstrcpy(cinfo->OldIconsPath, viewname);
	if ( fulldraw == FALSE ){
		if ( !cinfo->FullDraw ){
			UpdateWindow_Part(cinfo->info.hWnd);	// 入れたほうが早い？
		}
		setflag(cinfo->DrawTargetFlags, DRAWT_1ENTRY);
		RefleshCell(cinfo, OcellN);
		if ( !cinfo->FullDraw ){
			UpdateWindow_Part(cinfo->info.hWnd);	// 入れたほうが早い？
		}
	}
									// 情報行アイコン取得
	#ifdef USEDIRECTX
		DxDrawFreeBMPCache(&cinfo->InfoIcon_Cache);
		cinfo->InfoIcon_DirtyCache = FALSE;
	#endif
	if ( cinfo->dset.infoicon >= DSETI_EXTONLY ){
		cinfo->hInfoIcon = LoadIcon(hInst, MAKEINTRESOURCE(Ic_LOADING));
		setflag(cinfo->SubTCmdFlags, SUBT_GETINFOICON);
		SetEvent(cinfo->SubT_cmd);
	}else{					 // その他
		cinfo->hInfoIcon = NULL;
	}
	if ( IsTrue(cinfo->UseSelectEvent) ){
		SendMessage(cinfo->info.hWnd, WM_PPXCOMMAND, K_E_SELECT, 0);
	}

	if ( cinfo->SlowMode == FALSE ){		// 連動関係の処理
		if ( cinfo->hSyncInfoWnd ){	// ^\[I] 処理
			HWND hEdit;

			hEdit = (HWND)SendMessage(cinfo->hSyncInfoWnd, WM_PPXCOMMAND, KE_getHWND, 0);
			if ( hEdit && IsWindow(hEdit) ){
				ThSTRUCT text;

				MakeFileInformation(cinfo, &text, &CEL(cinfo->e.cellN));
				SetWindowText(hEdit, (TCHAR *)text.bottom);
				ThFree(&text);
			}else{
				cinfo->hSyncInfoWnd = NULL;
			}
		}
		if ( cinfo->SyncViewFlag ){	// \[Y] 処理
			if (!(CEL(cinfo->e.cellN).attr&(ECA_THIS | ECA_PARENT | ECA_DIR)) &&
					(CEL(cinfo->e.cellN).state >= ECS_NORMAL) ){
				DWORD flags = PPV_NOFOREGROUND | PPV_NOENSURE | PPV_SYNCVIEW;

				if ( CEL(cinfo->e.cellN).f.nFileSizeHigh ||
					 (CEL(cinfo->e.cellN).f.nFileSizeLow > X_svsz) ){
					// でかいので時間が掛かる媒体は処理しない
					if (	(cinfo->e.Dtype.mode != VFSDT_PATH) &&
							(cinfo->e.Dtype.mode != VFSDT_SHN) &&
							(cinfo->e.Dtype.mode != VFSDT_LFILE) &&
							(cinfo->e.Dtype.mode != VFSDT_FATIMG) &&
							(cinfo->e.Dtype.mode != VFSDT_CDIMG) &&
							(cinfo->e.Dtype.mode != VFSDT_CDDISK) ){
						return TRUE;
					}
					setflag(flags, PPV_HEADVIEW);
				}
				// 書庫内のディレクトリと思われるエントリは表示しない
				if ( ((cinfo->e.Dtype.mode != VFSDT_UN) &&
					  (cinfo->e.Dtype.mode != VFSDT_SUSIE)) ||
					 ((CEL(cinfo->e.cellN).f.nFileSizeHigh == 0) &&
					  (CEL(cinfo->e.cellN).f.nFileSizeLow != 0)) ){
					PPxView(cinfo->info.hWnd, viewname, flags | (cinfo->SyncViewFlag & ~1) );
				}
			}
		}
		if ( hPropWnd ){
			SYNCPROPINFO si;

			si.ff = CEL(cinfo->e.cellN).f;
			tstrcpy(si.filename, viewname);
			SyncProperties(cinfo->info.hWnd, &si);
		}
	}
	DNotifyWinEvent(EVENT_OBJECT_FOCUS, cinfo->info.hWnd, OBJID_CLIENT, cinfo->e.cellN + 1);
	return TRUE;
}
									// 現在ドライブの次のドライブに移動
void JumpNextDrive(PPC_APPINFO *cinfo, int offset)
{
	DWORD drv;
	int nowdrive, i;
	TCHAR buf[VFPS];

	drv = GetLogicalDrives();
	nowdrive = upper(cinfo->path[0]) - 'A';	// '\\' は適当に処理
	for ( i = 0 ; i < 26 ; i++ ){
		nowdrive += offset;
		if ( nowdrive < 0 ) nowdrive = 'Z' - 'A';
		if ( nowdrive > ('Z' - 'A') ) nowdrive = 0;
		if ( drv & (LSBIT << nowdrive) ) break;
	}
	wsprintf(buf, T("%c:"), nowdrive + 'A');
	SetPPcDirPos(cinfo);
	VFSFullPath(cinfo->path, buf, cinfo->path);
	read_entry(cinfo, RENTRY_READ);
}
										// 別の移動設定でカーソル移動する
void USEFASTCALL UseMoveKey(PPC_APPINFO *cinfo, int offset, CURSORMOVER *cm)
{
	int type1, type2;

	type1 = cm->outw_type;
	type2 = cm->outr_type;
	if (	(type1 == OUTTYPE_LRKEY) ||
			(type2 == OUTTYPE_LRKEY) ||
			(type1 == OUTTYPE_PAGEKEY) ||
			(type2 == OUTTYPE_PAGEKEY) ){
		SetPopMsg(cinfo, POPMSG_MSG, T("nested error"));
		return;
	}
	if ( offset > 0 ){
		MoveCellCursor(cinfo, cm);
	}else{
		MoveCellCursorR(cinfo, cm);
	}
}
										// カーソルがはみ出したときの処理
void USEFASTCALL DoOutOfRange(PPC_APPINFO *cinfo, int type, int offset)
{
	switch (type){
		case OUTTYPE_PAIR:			// 反対窓
			PPcChangeWindow(cinfo, PPCHGWIN_PAIR);
			break;
		case OUTTYPE_FPPC:			// 前
			PPcChangeWindow(cinfo, offset > 0 ? PPCHGWIN_BACK : PPCHGWIN_NEXT);
			break;
		case OUTTYPE_NPPC:			// 次
			PPcChangeWindow(cinfo, offset > 0 ? PPCHGWIN_NEXT : PPCHGWIN_BACK);
			break;
		case OUTTYPE_PARENT:		// 親に移動
			PPC_UpDir(cinfo);
			break;
		case OUTTYPE_DRIVE:		// \[L]
			PPC_DriveJump(cinfo, FALSE);
			break;
		case OUTTYPE_FDRIVE:		// ドライブ移動
			JumpNextDrive(cinfo, offset > 0 ? -1 : 1);
			break;
		case OUTTYPE_NDRIVE:		// ドライブ移動
			JumpNextDrive(cinfo, offset > 0 ? 1 : -1);
			break;
		case OUTTYPE_LRKEY:			// ←→
			UseMoveKey(cinfo, offset, &XC_mvLR);
			break;
		case OUTTYPE_PAGEKEY:		// PgUp/dw
			UseMoveKey(cinfo, offset, &XC_mvPG);
			break;
		case OUTTYPE_RANGEEVENT12: // RANGEEVENT1～8
		case OUTTYPE_RANGEEVENT34:
		case OUTTYPE_RANGEEVENT56:
		case OUTTYPE_RANGEEVENT78:
			PPcCommand(cinfo, (WORD)( K_E_RANGE1 +
				(type - OUTTYPE_RANGEEVENT12) * 2 + ((offset > 0) ? 1 : 0)) );
			break;
		default:
			SetPopMsg(cinfo, POPMSG_MSG, T("XC_mv param error"));
	}
}

// 窓間移動優先処理
BOOL USEFASTCALL CheckPairJump(PPC_APPINFO *cinfo, int offset)
{
	int site;

	if ( cinfo->combo ){
		if ( NULL == (HWND)SendMessage(cinfo->hComboWnd, WM_PPXCOMMAND,
				KCW_getpairwnd, (LPARAM)cinfo->info.hWnd) ){
			return FALSE;
		}
		site = PPcGetSite(cinfo);
		if ( site == PPCSITE_SINGLE ) return FALSE;
		if ( (offset >= 0) ? (site == PPCSITE_RIGHT) : (site == PPCSITE_LEFT) ){
			return FALSE;
		}
		PostMessage(cinfo->hComboWnd, WM_PPXCOMMAND,
				TMAKELPARAM(KCW_nextppc, 0), (LPARAM)cinfo->info.hWnd);
		return TRUE;
	}

	if ( GetJoinWnd(cinfo) == NULL ) return FALSE;
	site = cinfo->RegID[2] & PAIRBIT;
	if ( (cinfo->swin & SWIN_JOIN) && (cinfo->swin & SWIN_SWAPMODE) ){
		site ^= PAIRBIT;
	}
	if ( offset < 0 ) site ^= PAIRBIT;
	if ( site == PPCSITE_SINGLE ) return FALSE;
	PPcChangeWindow(cinfo, (offset > 0) ? PPCHGWIN_NEXT : PPCHGWIN_BACK);
	return TRUE;
}

// カーソル位置を範囲内に補正する
void FixCellRange(PPC_APPINFO *cinfo, int mode, int *NcellWMin, int cellN)
{
	int PageEntries;			// １ページ分のエントリ数
	int MaxWMin;				// NcellWMinの最大値

	mode = mode ? 0 : XC_fwin;
	PageEntries = cinfo->cel.Area.cx * cinfo->cel.Area.cy;
	if ( PageEntries > cinfo->e.cellIMax ) PageEntries = cinfo->e.cellIMax;

	if ( mode == 0 ){			// 隙間無し
		MaxWMin = cinfo->e.cellIMax - PageEntries;
	}else if ( XC_mvUD.outw_type == OUTTYPE_LINESCROLL ){
		MaxWMin = cinfo->e.cellIMax - 1;
		if ( cinfo->cel.Area.cy > (XC_smar * 3) ) MaxWMin -= XC_smar;
	}else if ( mode == 1 ){		// 桁単位
		*NcellWMin = *NcellWMin - (*NcellWMin % cinfo->cel.Area.cy);
		MaxWMin = (cinfo->e.cellIMax - PageEntries + cinfo->cel.Area.cy - 1) /
				cinfo->cel.Area.cy * cinfo->cel.Area.cy;
	}else{						// ページ単位
		*NcellWMin = *NcellWMin - (*NcellWMin % PageEntries);
		if ( cellN >= 0 ){
			*NcellWMin += ((cellN - *NcellWMin) / PageEntries) * PageEntries;
		}
		MaxWMin = cinfo->e.cellIMax - (cinfo->e.cellIMax % PageEntries);
	}

	if ( *NcellWMin > MaxWMin ) *NcellWMin = MaxWMin;
	if ( *NcellWMin < 0 ) *NcellWMin = 0;
}

//-----------------------------------------------------------------------------
// 位置が変わったら真になる
BOOL MoveCellCsr(PPC_APPINFO *cinfo, int offset, const CURSORMOVER *cm)
{
	int PageEntries;			// １ページ分のエントリ数
	int NcellN, NcellWMin;		// 新しいカーソル位置と表示開始位置
	int limit, flag = 0;
	int scroll, OcellN;
	BOOL CellFix = TRUE;

	DEBUGLOGC("MoveCellCsr start (Focus:%x)", GetFocus());

	// ● cellN の範囲調整(1.29+n バグがどこかにある？)
	if ( cinfo->e.cellIMax <= 0 ){
		cinfo->e.cellIMax = 1;
		PPxCommonExtCommand(K_SENDREPORT, (WPARAM)T("under cellDMax"));
	}
	if ( cinfo->e.cellN >= cinfo->e.cellIMax ){
		cinfo->e.cellN = cinfo->e.cellIMax - 1;
	}
	if ( cinfo->e.cellN < 0 ){
		cinfo->e.cellN = 0;
		PPxCommonExtCommand(K_SENDREPORT, (WPARAM)T("under cellN"));
	}
										// カスタマイズ関係の情報を用意 -------
	if ( cm == NULL ){
		cm = XC_page ? &DefCM_scroll : &DefCM_page;
	}
	PageEntries = cinfo->cel.Area.cx * cinfo->cel.Area.cy;
	if ( PageEntries > cinfo->e.cellIMax ) PageEntries = cinfo->e.cellIMax;
	if ( cm->outw_type == OUTTYPE_LINESCROLL ){
		limit = 1;
		if ( cinfo->cel.Area.cy > (XC_smar * 3) ) limit += XC_smar;
		scroll = 1;
	}else{
		limit = 0;
		scroll = 0;
	}
										// 新しい移動先（希望）を算出 ---------
	switch( cm->unit & CMOVEUNIT_TYPEMASK ){
		case CMOVEUNIT_TYPESCROLL: // カーソル移動＋スクロール移動
			NcellN = cinfo->e.cellN + offset;
			NcellWMin = cinfo->cellWMin + offset;
			break;

		case CMOVEUNIT_TYPELOCKSCROLL: // スクロール移動のみ
			NcellN = cinfo->e.cellN;
			NcellWMin = cinfo->cellWMin + offset;
											// 表示開始位置の範囲外補正
			FixCellRange(cinfo, scroll, &NcellWMin, -1);
			if ( !XC_nsbf ){
				CellFix = FALSE;
			}else{
											// カーソルの範囲外補正
				if ( NcellN < (NcellWMin + limit) ) NcellN = NcellWMin + limit;
				if ( NcellN >= (NcellWMin + PageEntries - limit) ){
					NcellN = NcellWMin + PageEntries - limit - 1;
				}
				if ( NcellN >= cinfo->e.cellIMax ) NcellN = cinfo->e.cellIMax - 1;
			}
			break;

		case CMOVEUNIT_MARKUPDOWN:
			scroll = XC_page;
			NcellN = cinfo->e.cellN;
			NcellWMin = cinfo->cellWMin;
			if ( offset > 0 ){
				while ( offset ){
					NcellN = DownSearchMarkCell(cinfo, NcellN);
					if ( NcellN < 0 ){
						NcellN = cinfo->e.cellN;
						break;
					}
					offset--;
				}
			}else{
				while ( offset ){
					NcellN = UpSearchMarkCell(cinfo, NcellN);
					if ( NcellN < 0 ){
						NcellN = cinfo->e.cellN;
						break;
					}
					offset++;
				}
			}
			break;

		case CMOVEUNIT_MARKPREVNEXT:
			scroll = XC_page;
			NcellN = cinfo->e.cellN;
			NcellWMin = cinfo->cellWMin;
			if ( !IsCEL_Marked(NcellN) ){
				if ( cinfo->e.markC == 0 ) break;
				NcellN = offset > 0 ? cinfo->e.markLast : cinfo->e.markTop;
				NcellN = GetCellIndexFromCellData(cinfo, NcellN);
				if ( NcellN < 0 ){
					NcellN = cinfo->e.cellN;
				}
				break;
			}
			if ( offset > 0 ){
				while ( offset ){
					NcellN = GetNextMarkCell(cinfo, NcellN);
					if ( NcellN < 0 ){
						NcellN = cinfo->e.cellN;
						break;
					}
					offset--;
				}
			}else{
				while ( offset ){
					NcellN = CEL(NcellN).mark_bk;
					if ( NcellN == ENDMARK_ID ){
						NcellN = cinfo->e.cellN;
						break;
					}
					NcellN = GetCellIndexFromCellData(cinfo, NcellN);
					if ( NcellN < 0 ){
						NcellN = cinfo->e.cellN;
						break;
					}
					offset++;
				}
			}
			break;

		case CMOVEUNIT_TYPEDEFAULT: // 既定値
			scroll = XC_page;
			// default へ
		default:	// カーソル移動のみ(CMOVEUNIT_TYPEPAGE)/既定値
			NcellN = cinfo->e.cellN + offset;
			NcellWMin = cinfo->cellWMin;
			break;
	}
										// カーソルが両端を超えたときの処理 ---
	while ( NcellN < 0 ){					// 最小値--------------------------
		if ( NcellWMin > 0 ){
			NcellN = 0;
			break;
		}

		flag = 1;
		if ( cm->outr_hook & OUTHOOK_EDGE ){		// 先端移動オプション
			if ( cinfo->e.cellN != 0 ){
				NcellN = 0;
				break;
			}
		}
		if ( cm->outr_hook & OUTHOOK_WINDOWJUMP ){	//窓間移動優先
			if ( CheckPairJump(cinfo, offset) ){
				NcellN = cinfo->e.cellN;
				NcellWMin = cinfo->cellWMin;
				break;
			}
		}
		if ((cm->outr_hook & OUTHOOK_HOOK) && cinfo->KeyRepeats){ // 引っ掛けオプション
			NcellN = cinfo->e.cellN;
			break;
		}
		switch (cm->outr_type){
			case OUTTYPE_STOP:			// 停止
				NcellN = cinfo->e.cellN;
				break;
			case OUTTYPE_WL:			// 画面反対(線対称)
				NcellN = (cinfo->cel.Area.cx - 1) * cinfo->cel.Area.cy +
						 (cinfo->e.cellN - cinfo->cellWMin) % cinfo->cel.Area.cy;
				if (NcellN > (cinfo->e.cellIMax - 1)) NcellN = cinfo->e.cellIMax - 1;
				break;
			case OUTTYPE_WP:			// 画面反対(点対称)
				NcellN = cinfo->cellWMin + PageEntries - 1;
				if (NcellN > (cinfo->e.cellIMax - 1)) NcellN = cinfo->e.cellIMax - 1;
				break;
			case OUTTYPE_LINESCROLL:	// 行スクロール
			case OUTTYPE_COLUMNSCROLL:	// 桁スクロール
			case OUTTYPE_PAGE:			// ページ切替
			case OUTTYPE_HALFPAGE:		// 半ページ切替
				NcellN = cinfo->e.cellIMax - 1;
				break;
			default:
				DoOutOfRange(cinfo, cm->outr_type, offset);
				DEBUGLOGC("MoveCellCsr nomove end %d", offset);
				return FALSE;
		}
		break;
	}
	while ( NcellN >= cinfo->e.cellIMax ){	// 最大値------------------------------
		if ( (NcellWMin + PageEntries) < cinfo->e.cellIMax ){
			NcellN = cinfo->e.cellIMax - 1;
			break;
		}

		flag = 1;

		if ( cm->outr_hook & OUTHOOK_EDGE ){ // 先端移動オプション
			if ( cinfo->e.cellN != (cinfo->e.cellIMax - 1) ){
				NcellN = cinfo->e.cellIMax - 1;
				break;
			}
		}

		if ( cm->outr_hook & OUTHOOK_WINDOWJUMP ){				// 窓間移動優先
			if ( CheckPairJump(cinfo, offset) ){
				NcellN = cinfo->e.cellN;
				NcellWMin = cinfo->cellWMin;
				break;
			}
		}
		if ((cm->outr_hook & OUTHOOK_HOOK) && cinfo->KeyRepeats){ // 引っ掛けオプション
			NcellN = cinfo->e.cellN;
			break;
		}
		switch ( cm->outr_type ){
			case OUTTYPE_STOP:			// 停止
				NcellN = cinfo->e.cellN;
				break;
			case OUTTYPE_WL:			// 画面反対(線対称)
				NcellN = (cinfo->e.cellN - cinfo->cellWMin) % cinfo->cel.Area.cy;
				break;
			case OUTTYPE_WP:			// 画面反対(点対称)
				NcellN = cinfo->cellWMin;
				break;
			case OUTTYPE_LINESCROLL:	// 行スクロール
			case OUTTYPE_COLUMNSCROLL:	// 桁スクロール
			case OUTTYPE_PAGE:			// ページ切替
			case OUTTYPE_HALFPAGE:		// 半ページ切替
				NcellN = 0;
				break;
			default:
				DoOutOfRange(cinfo, cm->outr_type, offset);
				DEBUGLOGC("MoveCellCsr nomove end %d", offset);
				return FALSE;
		}
		break;
	}
	if ( IsTrue(CellFix) ){
		DEBUGLOGC("MoveCellCsr CellFix %d", offset);
										// 表示領域の調節 ---
		FixCellRange(cinfo, scroll, &NcellWMin, -1);
										// 窓より左 ---------------------------
		while ( NcellN < (NcellWMin + limit ) ){
			int delta;

			if ( flag ){
				NcellWMin = (NcellN / cinfo->cel.Area.cy) * cinfo->cel.Area.cy;
				break;
			}

			if ( cm->outw_hook & OUTHOOK_EDGE ){
				if ( cm->outw_type == OUTTYPE_LINESCROLL ){
					if ( (NcellN < NcellWMin) && (cinfo->e.cellN != NcellWMin) ){
						NcellN = NcellWMin;
						break;
					}
				}else{
					if ( cinfo->e.cellN != NcellWMin ){
						NcellN = NcellWMin;
						break;
					}
				}
			}

			if ( cm->outw_hook & OUTHOOK_WINDOWJUMP ){		//窓間移動優先
				if ( CheckPairJump(cinfo, offset) ){
					NcellN = cinfo->e.cellN;
					NcellWMin = cinfo->cellWMin;
					break;
				}
			}
			if ((cm->outw_hook & OUTHOOK_HOOK) && cinfo->KeyRepeats){	// 引っ掛け処理
				NcellN = cinfo->e.cellN;
				break;
			}
			delta = (NcellWMin + limit) - NcellN - 1;
			switch ( cm->outw_type ){
				case OUTTYPE_STOP:			// 停止
					NcellN = cinfo->e.cellN;
					break;
				case OUTTYPE_WL:			// 画面反対
				case OUTTYPE_WP:			// 画面反対
					NcellN = NcellWMin + PageEntries - 1;
					if (NcellN > (cinfo->e.cellIMax - 1)){
						NcellN = cinfo->e.cellIMax - 1;
					}
					break;
				case OUTTYPE_LINESCROLL:	// 行スクロール
					NcellWMin -= delta;
					scroll = 1;
					break;
				case OUTTYPE_COLUMNSCROLL:	// 桁スクロール
					NcellWMin -= (delta / cinfo->cel.Area.cy + 1) *
							cinfo->cel.Area.cy;
					break;
				case OUTTYPE_PAGE:			// ページ切替
					NcellWMin -= (delta / PageEntries + 1) * PageEntries;
					break;
				case OUTTYPE_HALFPAGE:		// 半ページ切替
					NcellWMin -= (delta / (PageEntries/2) + 1) *(PageEntries/2);
					scroll = 1;
					break;
				default:
					DoOutOfRange(cinfo, cm->outw_type, offset);
					DEBUGLOGC("MoveCellCsr nomove end %d", offset);
					return FALSE;
			}
			break;
		}
										// 窓より右 ---------------------------
		while ( NcellN >= (NcellWMin + PageEntries - limit)){
			int delta;

			if ( flag ){
				NcellWMin = (NcellN / cinfo->cel.Area.cy) * cinfo->cel.Area.cy -
						((cinfo->cel.Area.cx < 1) ?
							0 : (cinfo->cel.Area.cx - 1) * cinfo->cel.Area.cy);
				break;
			}
			if ( cm->outw_hook & OUTHOOK_EDGE ){
				if ( cinfo->e.cellN < (NcellWMin + PageEntries - limit - 1) ){
					NcellN = NcellWMin + PageEntries - limit - 1;
					if ( NcellN < NcellWMin ) NcellWMin = NcellN;
					break;
				}
			}
			if ( cm->outw_hook & OUTHOOK_WINDOWJUMP ){		//窓間移動優先
				if ( CheckPairJump(cinfo, offset) ){
					NcellN = cinfo->e.cellN;
					NcellWMin = cinfo->cellWMin;
					break;
				}
			}
			if ((cm->outw_hook & OUTHOOK_HOOK) && cinfo->KeyRepeats){	// 引っ掛け処理
				NcellN = cinfo->e.cellN;
				break;
			}
			delta = NcellN - (NcellWMin + PageEntries - limit);
			switch (cm->outw_type){
				case OUTTYPE_STOP:			// 停止
					NcellN = cinfo->e.cellN;
					break;
				case OUTTYPE_WL:			// 画面反対
				case OUTTYPE_WP:			// 画面反対
					NcellN = NcellWMin;
					break;
				case OUTTYPE_LINESCROLL:	// 行スクロール
					NcellWMin += delta;
					scroll = 1;
					break;
				case OUTTYPE_COLUMNSCROLL:	// 桁スクロール
					NcellWMin += (delta / cinfo->cel.Area.cy + 1) *
							cinfo->cel.Area.cy;
					break;
				case OUTTYPE_PAGE:			// ページ切替
					NcellWMin += (delta / PageEntries + 1) * PageEntries;
					break;
				case OUTTYPE_HALFPAGE:		// 半ページ切替
					NcellWMin += (delta / (PageEntries/2) + 1) *(PageEntries/2);
					scroll = 1;
					break;
				default:
					DoOutOfRange(cinfo, cm->outw_type, offset);
					DEBUGLOGC("MoveCellCsr nomove end %d", offset);
					return FALSE;
			}
			if ( NcellN < NcellWMin ) NcellWMin = NcellN;
			break;
		}
										// 表示領域の調節 ---
		FixCellRange(cinfo, scroll, &NcellWMin, NcellN);
	}
	OcellN = cinfo->e.cellN;
	if ( SelfDD_hWnd == NULL ) cinfo->e.cellN = NcellN;
										// Explorer風選択処理
	if ( cm->outw_hook & OUTHOOK_MARK ){
		int markstart = -1, markend C4701CHECK;
		int unmarkstart = -1, unmarkend C4701CHECK;

		if ( cinfo->e.cellNref == -1 ) cinfo->e.cellNref = OcellN;
		if ( cinfo->e.cellNref > OcellN ){	// 基準点より前が前回位置
			if ( cinfo->e.cellN < OcellN ){	// 前回位置より現在位置が前→増加
				markstart = cinfo->e.cellN;
				markend = OcellN;
			}else{							// 後→消去
				unmarkstart = OcellN;
				unmarkend = cinfo->e.cellN - 1;
				if ( cinfo->e.cellNref <= unmarkend ){ // 基準点より後が現在位置
					markstart = cinfo->e.cellNref;
					markend = cinfo->e.cellN;
					unmarkend = markstart - 1;
				}
			}
		}else{
			if ( cinfo->e.cellN > OcellN ){	// 前回位置より現在位置が後→増加
				markstart = OcellN;
				markend = cinfo->e.cellN;
			}else{							// 後→消去
				unmarkstart = cinfo->e.cellN + 1;
				unmarkend = OcellN;
				if ( cinfo->e.cellNref >= unmarkstart ){ // 基準点より前が現在位置
					markstart = cinfo->e.cellN;
					markend = cinfo->e.cellNref;
					unmarkstart = markend + 1;
				}
			}
		}
		cinfo->MarkMask = MARKMASK_DIRFILE;
		if ( unmarkstart != -1 ){
			setflag(cinfo->DrawTargetFlags, DRAWT_ENTRY);
			while ( unmarkstart <= unmarkend ){ // C4701ok
				if ( IsCEL_Marked(unmarkstart) ){
					CellMark(cinfo, unmarkstart, MARK_REMOVE);
					RefleshCell(cinfo, unmarkstart);
					RefleshInfoBox(cinfo, DE_ATTR_MARK);
				}
				unmarkstart++;
			}
		}
		if ( markstart != -1 ){
			setflag(cinfo->DrawTargetFlags, DRAWT_ENTRY);
			while ( markstart <= markend ){ // C4701ok
				if ( !IsCEL_Marked(markstart) ){
					CellMark(cinfo, markstart, MARK_CHECK);
					RefleshCell(cinfo, markstart);
					RefleshInfoBox(cinfo, DE_ATTR_MARK);
				}
				markstart++;
			}
		}
	}else{	// 選択基準位置を更新
		if ( offset ){
			cinfo->e.cellNref = cinfo->e.cellN;
		}
	}

	if ( cinfo->cellWMin != NcellWMin ){				// スクロール表示
		POINT DragSelectPos;
		RECT DragSelectArea;

		DEBUGLOGC("MoveCellCsr scroll %d", offset);
		if ( (cinfo->MouseStat.PushButton > MOUSEBUTTON_CANCEL) &&
			 (cinfo->MousePush == MOUSE_MARK) ){
			RECT DragSelectArea;

			GetCursorPos(&DragSelectPos);
			ScreenToClient(cinfo->info.hWnd, &DragSelectPos);

			CalcDragTarget(cinfo, &DragSelectPos, &DragSelectArea);
			DrawDragFrame(cinfo->info.hWnd, &DragSelectArea);
			MarkDragArea(cinfo, &DragSelectArea, MARK_REVERSE);
		}
		#ifndef USEDIRECTX
		if ( (cinfo->bg.X_WallpaperType != 0) ||
			(XC_page && (C_eInfo[ECS_EVEN] != C_AUTO)) ){
		#endif
										// 壁紙表示時は全画面更新
			cinfo->e.cellPoint = -1;
			NewCellN(cinfo, OcellN, TRUE);
			cinfo->cellWMin = NcellWMin;
			cinfo->DrawTargetFlags = DRAWT_ALL;
			InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
		#ifndef USEDIRECTX
		}else{
			int xoffset, yoffset;

			xoffset = (cinfo->cellWMin - NcellWMin) / cinfo->cel.Area.cy;
			yoffset = (cinfo->cellWMin - NcellWMin) % cinfo->cel.Area.cy;
										// 表示の再利用不可なので全画面更新
			if ( (xoffset <= -cinfo->cel.Area.cx) ||
				 (xoffset >= cinfo->cel.Area.cx) ){
				cinfo->e.cellPoint = -1;
				NewCellN(cinfo, OcellN, TRUE);
				cinfo->cellWMin = NcellWMin;
				cinfo->DrawTargetFlags = DRAWT_ALL;
				InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
			}else{
										// 再利用可能な表示があるのでスクロール
										// ※ xoffset, yoffset が共に !0 なら
										//    全画面更新が好ましいが、現状では
										//    おきにくいため、対策せず
				RECT box, newbox;
				HDC hDC;
				int OcellWMin;

				if ( cinfo->e.cellPoint >= 0 ){
					int cellPoint = cinfo->e.cellPoint;

					cinfo->e.cellPoint = -1;
					if ( (cellPoint >= NcellWMin) &&
						 (cellPoint < (NcellWMin + PageEntries)) ){
						RefleshCell(cinfo, cellPoint);
						if ( !cinfo->FullDraw ) UpdateWindow_Part(cinfo->info.hWnd);
					}
				}
				NewCellN(cinfo, OcellN, FALSE);

				box.left	= cinfo->BoxEntries.left;
				box.right	= box.left + cinfo->cel.VArea.cx * cinfo->cel.Size.cx;
				box.top		= cinfo->BoxEntries.top;
				box.bottom	= box.top + cinfo->cel.Area.cy * cinfo->cel.Size.cy;

				hDC = GetDC(cinfo->info.hWnd);
				ScrollDC(hDC, xoffset * cinfo->cel.Size.cx,
							 yoffset * cinfo->cel.Size.cy,
							&box, &box, NULL, &newbox);
				ReleaseDC(cinfo->info.hWnd, hDC);

				OcellWMin = cinfo->cellWMin;
				cinfo->cellWMin = NcellWMin;
										// 旧カーソルを消去
				RefleshCell(cinfo, OcellN - OcellWMin + NcellWMin +
										xoffset * cinfo->cel.Area.cy + yoffset);
				if ( !cinfo->FullDraw ) UpdateWindow_Part(cinfo->info.hWnd);
										// 新規部分を表示
				setflag(cinfo->DrawTargetFlags, DRAWT_1ENTRY);
				UnionRect(&box, &newbox, &cinfo->DrawTargetCell);
				cinfo->DrawTargetCell = box;

				InvalidateRect(cinfo->info.hWnd, &newbox, FALSE);
				if ( !cinfo->FullDraw ) UpdateWindow_Part(cinfo->info.hWnd);

				RefleshCell(cinfo, cinfo->e.cellN);
//				if ( !cinfo->FullDraw ) UpdateWindow_Part(cinfo->info.hWnd);

				RefleshInfoBox(cinfo, DE_ATTR_ENTRY | DE_ATTR_PAGE);
			}
			if ( cinfo->combo && (X_combos[0] & CMBS_COMMONINFO) ){
				PostMessage(cinfo->hComboWnd, WM_PPXCOMMAND, KCW_drawinfo, 0);
			}
		}
		#endif // USEDIRECTX
		if ( (cinfo->MouseStat.PushButton > MOUSEBUTTON_CANCEL) &&
			 (cinfo->MousePush == MOUSE_MARK) ){
			CalcDragTarget(cinfo, &DragSelectPos, &DragSelectArea);
			MarkDragArea(cinfo, &DragSelectArea, MARK_REVERSE);
			cinfo->DrawTargetFlags = DRAWT_ALL;
			InvalidateRect(cinfo->info.hWnd, NULL, FALSE);
			UpdateWindow_Part(cinfo->info.hWnd);
			DrawDragFrame(cinfo->info.hWnd, &DragSelectArea);
		}
		DEBUGLOGC("MoveCellCsr SetScrollBar %d", offset);
		SetScrollBar(cinfo, scroll);
		if ( cinfo->EntryIcons.hImage != NULL ){
			setflag(cinfo->SubTCmdFlags, SUBT_GETCELLICON);
			SetEvent(cinfo->SubT_cmd);
		}
		if ( XC_szcm == 2 ){ // ディレクトリサイズキャッシュの読み出し開始
			setflag(cinfo->SubTCmdFlags, SUBT_GETDIRSIZECACHE);
			SetEvent(cinfo->SubT_cmd);
		}
		DEBUGLOGC("MoveCellCsr end %d", offset);
		return TRUE;
	}else{ // 画面内位置変更
		DEBUGLOGC("MoveCellCsr newpos %d", offset);
		if ( !NewCellN(cinfo, OcellN, FALSE) ){ // 古いカーソルを消去/アイコン更新
			DEBUGLOGC("MoveCellCsr nomove end %d", offset);
							// 移動したが更新済み : カーソルは移動しなかった
			return (cinfo->e.cellN != OcellN) ? TRUE : FALSE;
		}
												// 新しいカーソルを表示
		SetScrollBar(cinfo, scroll);
		DEBUGLOGC("MoveCellCsr RefleshCell %d", offset);
		setflag(cinfo->DrawTargetFlags, DRAWT_1ENTRY);
		RefleshCell(cinfo, cinfo->e.cellN);
		if ( !cinfo->FullDraw ) UpdateWindow_Part(cinfo->info.hWnd);
												// 情報窓の更新
		RefleshInfoBox(cinfo, DE_ATTR_ENTRY);
		DEBUGLOGC("MoveCellCsr UpdateWindow %d", offset);
		UpdateWindow_Part(cinfo->info.hWnd);
		DEBUGLOGC("MoveCellCsr end %d", offset);
		return TRUE;
	}
}

/*-----------------------------------------------------------------------------
	特定の cell のマーク処理を行う

	flag = -1:反転 1:付ける 0:はずす -10:INDEX経由せずにはずす
-----------------------------------------------------------------------------*/
void CellMark(PPC_APPINFO *cinfo, ENTRYINDEX cellNo, int markmode)
{
	ENTRYINDEX fwdNo;
	ENTRYDATAOFFSET cellTNo;
	ENTRYCELL *cell;

	cellTNo = CELt(cellNo);
	cell = &CELdata(cellTNo);
	fwdNo = cell->mark_fw;

	if ( markmode == MARK_REVERSE ) markmode = ( fwdNo == NO_MARK_ID );

	if ( (markmode != MARK_REMOVE) && (cell->state >= ECS_NORMAL) &&
		 !(cell->attr & (ECA_THIS | ECA_PARENT | ECA_ETC)) &&
		 !(cell->f.dwFileAttributes & (MARKMASK_DIRFILE ^ cinfo->MarkMask)) ){
																// マークする
		if ( fwdNo == NO_MARK_ID ){
			cinfo->e.markC++;
			if ( cinfo->e.markTop == ENDMARK_ID ){		// 初めてのマーク
				cell->mark_bk = ENDMARK_ID;
				cinfo->e.markTop = cellTNo;
			}else{					// 既にある
				cell->mark_bk = cinfo->e.markLast;
				CELdata(cinfo->e.markLast).mark_fw = cellTNo;
			}
			cinfo->e.markLast = cellTNo;
			cell->mark_fw = ENDMARK_ID;
			AddDD(cinfo->e.MarkSize.l, cinfo->e.MarkSize.h,
					cell->f.nFileSizeLow, cell->f.nFileSizeHigh);
		}
	}else{
		ResetMark(cinfo, cell);
	}
}

void USEFASTCALL ResetMark(PPC_APPINFO *cinfo, ENTRYCELL *cell)
{
	ENTRYDATAOFFSET backNo, nextNo;

	nextNo = cell->mark_fw;
	if ( nextNo == NO_MARK_ID ) return;
	cinfo->e.markC--;
	backNo = cell->mark_bk;
	if ( backNo != ENDMARK_ID ){			// 一つ前を接続
		CELdata(backNo).mark_fw = nextNo;
	}else{
		cinfo->e.markTop = nextNo;	// 末端
	}
	if ( nextNo != ENDMARK_ID ){			// 一つ次を接続
		CELdata(nextNo).mark_bk = backNo;
	}else{			// 先頭
		cinfo->e.markLast = backNo;
	}
	cell->mark_fw = NO_MARK_ID;
	SubDD(cinfo->e.MarkSize.l, cinfo->e.MarkSize.h,
			cell->f.nFileSizeLow, cell->f.nFileSizeHigh);
}

//-----------------------------------------------------------------------------
BOOL MoveCellCursorR(PPC_APPINFO *cinfo, CURSORMOVER *cm)
{
	CURSORMOVER tmpcm;

	tmpcm = *cm;
	tmpcm.offset = -tmpcm.offset;
	return MoveCellCursor(cinfo, &tmpcm);
}

BOOL MoveCellCursor(PPC_APPINFO *cinfo, CURSORMOVER *cm)
{
	switch( cm->unit / CMOVEUNIT_RATEDIV ){
		case CMOVEUNIT_RATECOLUMN:
			return MoveCellCsr(cinfo, cm->offset * cinfo->cel.Area.cy, cm);
		case CMOVEUNIT_RATEPAGE:
			return MoveCellCsr(cinfo,
				cm->offset * cinfo->cel.Area.cx * cinfo->cel.Area.cy, cm);
		case CMOVEUNIT_RATEDECIPAGE:
			return MoveCellCsr(cinfo,
				cm->offset * cinfo->cel.Area.cx * cinfo->cel.Area.cy / 10, cm);
//		case CMOVEUNIT_RATEUPDOWN: , CMOVEUNIT_MARKUPDOWN等
		default:
			return MoveCellCsr(cinfo, cm->offset, cm);
	}
}

void GetCellRealFullName(PPC_APPINFO *cinfo, ENTRYCELL *cell, TCHAR *dest)
{
	TCHAR namebuf[VFPS], *cellfilename;

	cellfilename = (TCHAR *)GetCellFileName(cinfo, cell, namebuf);
	if ( cinfo->e.Dtype.mode != VFSDT_SHN ){
		VFSFullPath(dest, cellfilename, cinfo->path);
		return;
	}

	VFSFixPath(dest, cellfilename, cinfo->path, VFSFIX_FULLPATH | VFSFIX_REALPATH);
	if ( dest[0] != '?' ) return;
	VFSFullPath(dest, cellfilename, cinfo->RealPath);
}
