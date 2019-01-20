/*-----------------------------------------------------------------------------
	Paper Plane cUI											File size 関連
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPCOMBO.H"
#pragma hdrstop

#define AddFDD(dstL,dstH,srcL,srcH,andL,addL)\
	{DWORD I;I = ((srcL) + (addL)) & (andL);if (I < (srcL)) (dstH)++;\
	AddDD((dstL),(dstH),I,(srcH));}

typedef struct {
	DWORD sizeAL,sizeAH;	// 加算サイズ
	DWORD sizeNL,sizeNH;	// この窓のクラスタ換算サイズ
	DWORD addN;			// この窓の (byte per cluster - 1)
	DWORD andN;			// この窓の ~(byte per cluster - 1)
	DWORD sizePL,sizePH;	// 反対窓のクラスタ換算サイズ
	DWORD addP;			// 反対窓の (byte per cluster - 1)
	DWORD andP;			// 反対窓の ~(byte per cluster - 1)
	DWORD files;		// ファイル数
	DWORD dirs;			// ディレクトリ数
	DWORD links;		// シンボリックリンク等
	DWORD skips;		// アクセスできなかったディレクトリ
	volatile BOOL *BreakFlag;	// cinfo->BreakFlag を示す
} COUNTSTRUCT;

typedef struct {
	PPC_APPINFO *cinfo;
	TCHAR *files;		// カウント対象を保存する文字列群
	COUNTSTRUCT cs;
} COUNTPARAM;

const TCHAR CountSizeMainName[] = T("PPc count");

// 特定ディレクトリの容量計算 -------------------------------------------------
void DispMarkSizeDir(TCHAR *dir,COUNTSTRUCT *cs)
{
	TCHAR *t;
	HANDLE hFF;
	WIN32_FIND_DATA ff;

	(cs->dirs)++;
	t = dir + tstrlen(dir);
	CatPath(NULL,dir,T("*"));
	hFF = VFSFindFirst(dir,&ff);
	*t = '\0';
	if ( hFF == INVALID_HANDLE_VALUE ){
		cs->skips++;
	}else{
		do{
			if ( IsRelativeDir(ff.cFileName) ) continue;
			if ( IsTrue(*cs->BreakFlag) ) break;
			if ( ff.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ){
				cs->links++;
			}
			if ( ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
				CatPath(NULL,dir,ff.cFileName);
				DispMarkSizeDir(dir,cs);
				*t = 0;
			}else{
				(cs->files)++;
				AddDD(cs->sizeAL,cs->sizeAH,ff.nFileSizeLow,ff.nFileSizeHigh);
				AddFDD(cs->sizeNL,cs->sizeNH,
						ff.nFileSizeLow,ff.nFileSizeHigh,cs->andN,cs->addN);

				AddFDD(cs->sizePL,cs->sizePH,
						ff.nFileSizeLow,ff.nFileSizeHigh,cs->andP,cs->addP);
			}
		}while( IsTrue(VFSFindNext(hFF,&ff)) );
//		while( IsTrue(FindNextFile(hFF,&ff)));
		VFSFindClose(hFF);
//		FindClose(hFF);
	}
}

// マークされたCellの容量計算 -------------------------------------------------
DWORD WINAPI CountSizeMain(COUNTPARAM *cp)
{
	THREADSTRUCT threadstruct = {CountSizeMainName,XTHREAD_EXITENABLE | XTHREAD_TERMENABLE,NULL,0,0};
	PPC_APPINFO *cinfo;
	COUNTSTRUCT cs;
	struct dirinfo di;
	TCHAR dir[VFPS],*files,*path;
	ThSTRUCT th;
	DWORD AllL,AllH;
	COPYDATASTRUCT cds;
	DWORD tick = 0,nowtick;

	CoInitializeEx(NULL,COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
	PPxRegisterThread(&threadstruct);

	cinfo = cp->cinfo;
	path = cp->files;					// パラメータを回収
	cs = cp->cs;
	HeapFree( hProcessHeap,0,(COUNTPARAM *)cp);

	th.bottom = (char *)path;
	files = path + tstrlen(path) + 1;
	cinfo->BreakFlag = FALSE;
	AllL = cs.sizeAL;
	AllH = cs.sizeAH;
	tstrcpy(di.path,path);

	cds.dwData = KC_StDS;
	cds.cbData = sizeof(di);
	cds.lpData = &di;

// 計算メインループ ---------------------------
	while ( *files ){
		cs.sizeAL = 0;
		cs.sizeAH = 0;
		VFSFullPath(dir,files,path);
		DispMarkSizeDir(dir,&cs);
		if ( IsTrue(cinfo->BreakFlag) ) break;
		AddDD(AllL,AllH,cs.sizeAL,cs.sizeAH);
		tstrcpy(di.path,path);
		tstrcpy(di.name,files);
		di.low = cs.sizeAL;
		di.high = cs.sizeAH;
		SendMessage(cinfo->info.hWnd,WM_COPYDATA,
				0,(LPARAM)(PCOPYDATASTRUCT)&cds);
		files += tstrlen(files) + 1;
		nowtick = GetTickCount();
		if ( (nowtick - tick) > 1000 ){
			tick = nowtick;
			InvalidateRect(cinfo->info.hWnd,NULL,FALSE);
		}
	}

	if ( IsTrue(cinfo->BreakFlag) ){
		SetPopMsg(cinfo,POPMSG_MSG,MES_BRAK);
	}else{
		TCHAR buf1[32],buf2[32],buf3[32],buf4[32],buf5[32];

		FormatNumber(buf1,XFN_SEPARATOR,26,AllL,AllH);
		FormatNumber(buf2,XFN_SEPARATOR,26,cs.files,0);
		FormatNumber(buf3,XFN_SEPARATOR,26,cs.dirs,0);
		FormatNumber(buf4,XFN_SEPARATOR,26,cs.sizeNL,cs.sizeNH);
		if ( cs.andP ){
			FormatNumber(buf5,XFN_SEPARATOR,26,cs.sizePL,cs.sizePH);
			wsprintf(dir,T("%s bytes/ %s files/ %s directorys[%s/%s] "),
													buf1,buf2,buf3,buf4,buf5);
		}else{
			wsprintf(dir,T("%s bytes/ %s files/ %s directorys[%s] "),
													buf1,buf2,buf3,buf4);
		}
		if ( cs.skips != 0 ){
			TCHAR *ptr = dir + tstrlen(dir);
			wsprintf(ptr,T("/ %d skip dir. "),cs.skips);
		}
		if ( cs.links != 0 ){
			TCHAR *ptr = dir + tstrlen(dir);
			wsprintf(ptr,T("/ %d s.links "),cs.links);
		}
		SetPopMsg(cinfo,POPMSG_MSG,dir);
	}
	ThFree(&th);
	InvalidateRect(cinfo->info.hWnd,NULL,FALSE);
	cinfo->sizereentry = 0;

	PPxUnRegisterThread();
	CoUninitialize();
	return TRUE;
}

void CountMarkSize(PPC_APPINFO *cinfo)
{
	COUNTPARAM cp,*pcp;
	ThSTRUCT th;
	HANDLE hT;
	DWORD tmp;
	DWORD spc,bps,fc,tc;
	TCHAR dir[VFPS],buf[VFPS];
	int work;
	ENTRYCELL *cell;

	if ( cinfo->sizereentry ){
		SetPopMsg(cinfo,POPMSG_MSG,MES_EACO);
		return;
	}
	cinfo->sizereentry = 1;

	if ( (Combo.Report.hWnd != NULL) && (cinfo->e.markC == 0) ){
		VFSFullPath(buf,CEL(cinfo->e.cellN).f.cFileName,cinfo->path);
		SetPopMsg(cinfo,POPMSG_MSG,buf);
	}else{
		SetPopMsg(cinfo,POPMSG_MSG,MES_SACO);
	}
	UpdateWindow_Part(cinfo->info.hWnd);
										// カウント準備 ===============
	memset(&cp.cs,0,sizeof(cp.cs));
	cp.cs.BreakFlag = &cinfo->BreakFlag;
	ThInit(&th);
	ThAddString(&th,cinfo->path);
								// クラスタ換算用定数を作成 -----------
	GetPairPath(cinfo,buf);	// 反対窓
	if ( *buf != '\0' ){
		GetDriveName(dir,buf);
		if (GetDiskFreeSpace(dir,&spc,&bps,&fc,&tc)){
			cp.cs.addP = (spc * bps) - 1;
			cp.cs.andP = ~cp.cs.addP;
		}
	}
	GetDriveName(dir,cinfo->path);		// 現在窓
	if (GetDiskFreeSpace(dir,&spc,&bps,&fc,&tc)){
		cp.cs.addN = (spc * bps) - 1;
		cp.cs.andN = ~cp.cs.addN;
	}
								// マークファイルのみカウント ---------
	InitEnumMarkCell(cinfo,&work);
	while ( (cell = EnumMarkCell(cinfo,&work)) != NULL ){
		if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ){
			cp.cs.links++;
		}

		if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
			ThAddString(&th,cell->f.cFileName);
		}else{
			cp.cs.files++;
			AddDD(cp.cs.sizeAL,cp.cs.sizeAH,
					cell->f.nFileSizeLow,cell->f.nFileSizeHigh);

			AddFDD(cp.cs.sizeNL,cp.cs.sizeNH,
					cell->f.nFileSizeLow,cell->f.nFileSizeHigh,
					cp.cs.andN,cp.cs.addN);

			AddFDD(cp.cs.sizePL,cp.cs.sizePH,
					cell->f.nFileSizeLow,cell->f.nFileSizeHigh,
					cp.cs.andP,cp.cs.addP);
		}
	}
	cp.files = (TCHAR *)th.bottom;
	cp.cinfo = cinfo;
	pcp = PPcHeapAlloc(sizeof(COUNTPARAM));
	*pcp = cp;

	hT = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)CountSizeMain,pcp,0,&tmp);
	if ( hT != NULL ){
		SetThreadPriority(hT,THREAD_PRIORITY_LOWEST);
		CloseHandle(hT);
	}
}

int DispMarkSize(PPC_APPINFO *cinfo)
{
	ENTRYCELL *cell;
	int work;
	int result = FALSE;

	if ( cinfo->e.Dtype.mode == VFSDT_HTTP ) return FALSE;

	InitEnumMarkCell(cinfo,&work);
	while ( (cell = EnumMarkCell(cinfo,&work)) != NULL ){
		if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
			CountMarkSize(cinfo);
			result = TRUE;
			break;
		}
	}
	return result;
}

void ClearMarkSize(PPC_APPINFO *cinfo)
{
	TCHAR dirpath[VFPS];
	int work;
	ENTRYCELL *cell;
	BYTE *hist;
	WORD datasize;
								// マークファイルのみカウント ---------
	InitEnumMarkCell(cinfo,&work);
	while ( (cell = EnumMarkCell(cinfo,&work)) != NULL ){
		if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
			if ( XC_szcm == 1 ){
				if ( cell->comment != EC_NOCOMMENT ){
					// ※※※ガーページコレクションなどはしない
					cell->comment = EC_NOCOMMENT;
					cinfo->ModifyComment = TRUE;
				}
			}
			if ( cell->attr & ECA_DIRC ){
				VFSFullPath(dirpath,CellFileName(cell),cinfo->path);
				UsePPx();
				hist = (BYTE *)SearchHistory(PPXH_PPCPATH,dirpath);
				if ( hist != NULL ){
					datasize = GetHistoryDataSize(hist);
					if ( datasize >= ( sizeof(DWORD) * 4 ) ){
						DWORD *WritePtr = (DWORD *)(BYTE *)GetHistoryData(hist);

						WritePtr[2] = 0;
						WritePtr[3] = 0xffffffff;
					}
				}
				FreePPx();

				if ( IsCellPtrMarked(cell) ){
					SubDD(cinfo->e.MarkSize.l,cinfo->e.MarkSize.h,
							cell->f.nFileSizeLow,cell->f.nFileSizeHigh);
				}
				cell->f.nFileSizeLow = cell->f.nFileSizeHigh = 0;
				resetflag(cell->attr,ECA_DIRC);
			}
		}
	}
	Repaint(cinfo);
}
