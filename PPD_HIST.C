/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library					History & Customize Area

	先頭に「HistoryID(4bytes)とファイルの大きさ(DWORD)」
	history 構造
	+0(2)	次のリストへのポインタ(0:最終)
	+2(2)	内容の種類 ( b3, b15 は未使用 )
				b00(PPXH_GENERAL)	汎用
							[+4]   (0)	使用せず
				b01(PPXH_NUMBER)	数字
							[+4]   (0)	使用せず
				b02(PPXH_DIR)		ディレクトリ
							[+4]   (0)	使用せず

				b04(PPXH_USER2)		ユーザ定義2
							[+4]   (0)	使用せず
				b05(PPXH_USER1)		ユーザ定義1
							[+4]   (0)	使用せず

				b06(PPXH_ROMASTR)	ローマ字検索用のキャッシュ
							[+4]   (z)	該当する正規表現
				b07(PPXH_NETPCNAME)	ネットワークPC列挙用のキャッシュ
							[+4]   (0)	使用せず
				b08(PPXH_PPVNAME)	PPv 表示ファイル名
							[+4]+0 (4)	表示形式
								表示形式
									+0	表示可能な形式 B0:HEX B1:TEXT B2:IMG...
									+1	表示に用いた形式
									+2, +3	+1 によって変化
										TEXT:	+2:表示に用いた文字コード
												+3:HISTOPT_TEXTOPT_
										IMG:	+2:no use
												+3:B0, B1=回転
							[+4]+4 (4)	表示X
							[+4]+8 (4)	表示Y
							[+4]+12(z)	追加オプション (PPV_STRU.H option ID)
									+0	オプションのサイズ
									+1	ID
									+2～ オプションの内容
				b09(PPXH_SEARCH)	検索文字列
							[+4]   (0)	使用せず
				b10(PPXH_COMMAND)	コマンドライン
							[+4]   (0)	使用せず
				b11(PPXH_PATH)		フルパスファイル名(ディレクトリ含む)
							[+4]   (0)	使用せず
				b12(PPXH_FILENAME)	ファイル名(ディレクトリ無し)
							[+4]   (0)	使用せず
				b13(PPXH_MASK)		ワイルドカード
							[+4]   (0)	使用せず
				b14(PPXH_PPCPATH)	PPc ディレクトリパス
							[+4]+0 (4)	画面内のカーソル位置
							[+4]+4 (4)	画面表示開始位置
							([+4]+8 (8)	総サイズ、オプション)

	+4(2)	文字列部分のバイト数／バイナリ部分へのオフセット
	+6～	ヒストリ文字列(stringZ)


	先頭に「CustID(4bytes)とファイルの大きさ(DWORD)」
	customize data 構造
	+0(4)	次のリストへのポインタ(0:最終)
	+4(n)	文字列部分(AsciiZ/UNICODEZ)
	+4+n	実際のデータ

	customize table構造
	+0(4)	次のリストへのポインタ(0:最終)
	+4(n)	文字列部分(AsciiZ/UNICODEZ)
	+4+n	テーブル
	  +0(2)	次のリストへのポインタ(0:最終)
	  +2(n)	文字列部分(AsciiZ/UNICODEZ)
	  +2+n	実際のデータ

-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include "PPXVER.H"
#include "PPX.H"
#include "VFS.H"
#include "TMEM.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#include "VFS_STRU.H"
#pragma hdrstop

#define TABLENEXTOFFSETSIZE 2
#define TABLEENDMARKSIZE 2
#define REPORTEXPAND 0

const TCHAR RegTxtExtName[] = T(".txt");
const TCHAR expandsuccess[] = T("Customize area expanded : %d");
const TCHAR expandfix[] = T("Customize area fixed : %d");
const TCHAR expandfailed[] = T("Customize file expand error!!\nPlease Close PPx soon.\nカスタマイズ領域の拡張に失敗しました。\nこれ以上保存できないので直ちにPPxを終了してください。");

#ifdef UNICODE
#define EXTENDCUSTOMIZEDELTA 0x2000
#else
#define EXTENDCUSTOMIZEDELTA 0x1000
#endif

#if REPORTEXPAND
 DWORD ExtendPopTick = 0; // 前回の拡張通知から時間が経っていないかを確認する用
#endif

BYTE **MessageTextIndexTable; // メッセージテーブル
int MessageTextTableCount;	// メッセージテーブルのメッセージの個数
DWORD MessageTextCRC; // 再カスタマイズ時にメッセージの再取得を省略するため用

#define UNENUMCACHE 0x7f007f00
ENUMCUSTCACHESTRUCT enumcustcache = { NULL, NULL, MAX32, UNENUMCACHE };

PPXDLL void PPXAPI GetPPxDBinfo(PPXDBINFOSTRUCT *dbinfo)
{
	UsePPx();
	if ( dbinfo->structsize >= (sizeof(DWORD) * 3) ){
		BYTE *ptr;
		WORD w;

		ptr = HisP;
		for ( ; ; ){
			w = *(WORD *)ptr;
			if ( w == 0 ) break;
			ptr += w;
		}
		dbinfo->histsize = X_Hsize;
		dbinfo->histfree = X_Hsize - (ptr - HisP) - HIST_HEADER_FOOTER_SIZE;
	}
	if ( dbinfo->structsize >= (sizeof(DWORD) * 5) ){
		BYTE *ptr;
		DWORD datasize;

		ptr = CustP;
		for ( ; ; ){
			datasize = *(DWORD *)ptr;
			if ( datasize == 0 ) break;
			ptr += datasize;
		}
		dbinfo->custsize = X_Csize;
		dbinfo->custfree = X_Csize - (ptr - CustP) - CUST_HEADER_FOOTER_SIZE;
	}
	if ( dbinfo->structsize >= (sizeof(DWORD) * 5 + sizeof(TCHAR *)) ){
		MakeUserfilename(dbinfo->custpath, CUSTNAME, PPxRegPath);
	}
	FreePPx();
}

PPXDLL void PPXAPI SetPPxDBsize(int mode, DWORD size)
{
	if ( PMessageBox(NULL, MES_QSIZ, T("?"), MB_QYES) != IDOK ){
		return;
	}
	if ( size < 0x1000 ) size = 0x1000;
	if ( size > 0x100000 ) size = 0x100000;
	UsePPx();
	if ( mode == PPXDB_HISTORY )   HistSizeFromHisP = size;
	if ( mode == PPXDB_CUSTOMIZE ) CustSizeFromCustP = size;
	FreePPx();
	PPxSendMessage(WM_CLOSE, 0, 0);
}

// ヒストリを初期化する -------------------------------------------------------
PPXDLL void PPXAPI InitHistory(void)
{
	UsePPx();
	strcpy( HistHeaderFromHisP->HeaderID, HistoryID );
	HistSizeFromHisP = X_Hsize;
	memset(HisP, 0, X_Hsize - HIST_HEADER_SIZE);
	FreePPx();
}
/*-----------------------------------------------------------------------------
	指定されたジャンルの占有量をヒストリサイズの約半分に制限する
-----------------------------------------------------------------------------*/
BOOL LimitHistory(WORD type, DWORD addsize)
{
	const char *ptr;
	WORD w;
	DWORD size = 0, maxsize;

	maxsize = X_Hsize / 2 - addsize;
	ptr = (const char *)HisP;
	for ( ; ; ){
		w = *(WORD *)ptr;
		if ( w == 0 ) return TRUE; // チェック完了
		if ( *(WORD *)(ptr + 2) & type ){
			if ( (size + w) > maxsize ){
				DeleteHistory(type, (const TCHAR *)(ptr + 6));
				// 再試行
				ptr = (const char *)HisP;
				size = 0;
				continue;
			}
			size += w;
		}
		ptr += w;
	}
}
/*-----------------------------------------------------------------------------
	指定の名前を完全一致検索し、そのポインタを出力ないなら NULL
	UsePPx-FreePPx間内で使用すること
-----------------------------------------------------------------------------*/
PPXDLL const TCHAR * PPXAPI SearchHistory(WORD type, const TCHAR *str)
{
	const char *ptr;
	WORD w, l;

	ptr = (const char *)HisP;
	l = (WORD)TSTRSIZE(str);
	if ( !(type & PPXH_PPCPATH) ){
		for ( ; ; ){
			w = *(WORD *)ptr;
			if ( w == 0 ) return NULL;
			if (	(*(WORD *)(ptr + 2) & type) &&
					(*(WORD *)(ptr + 4) == l) &&
					!memcmp( ptr + 6, str, l - sizeof(TCHAR) ) ){
				return (const TCHAR *)(ptr + 6);
			}
			ptr += w;
		}
	}else{
		for ( ; ; ){
			w = *(WORD *)ptr;
			if ( w == 0 ) return NULL;
			if ( *(WORD *)(ptr + 2) & type ){
				if ( (*(TCHAR *)(ptr + 6) == ':') &&
					 (*(TCHAR *)(ptr + 6 + sizeof(TCHAR)) == '<') ){
					const TCHAR *pw;

					pw = tstrchr((TCHAR *)(ptr + 6 + sizeof(TCHAR) * 2), '>');
					if ( (pw != NULL) && !tstrcmp(pw + 1, str) ){
						return (const TCHAR *)(ptr + 6);
					}
				}else if ( (*(WORD *)(ptr + 4) == l) &&
						!memcmp( ptr + 6, str, l - sizeof(TCHAR) ) ){
					return (const TCHAR *)(ptr + 6);
				}
			}
			ptr += w;
		}
	}
}
/*-----------------------------------------------------------------------------
	指定の名前を前方一致検索し、そのポインタを出力ないなら NULL
	UsePPx-FreePPx間内で使用すること
-----------------------------------------------------------------------------*/
PPXDLL const TCHAR * PPXAPI SearchPHistory(DWORD type, const TCHAR *str)
{
	const char *ptr;
	WORD w, strsize;
	TCHAR si[VFPS], di[VFPS];

	ptr = (const char *)HisP;
	tstrcpy(si, str);
	tstrupr(si);
	strsize = (WORD)TSTRLENGTH(si);
	for ( ; ; ){
		w = *(WORD *)ptr;
		if ( w == 0 ) return NULL;
		if (	(*(WORD *)(ptr + 2) & (WORD)type) &&
				(*(WORD *)(ptr + 4) > strsize) ){
			memcpy(di, ptr + 6, strsize);
			di[strsize / sizeof(TCHAR)] = '\0';
			tstrupr(di);
			if ( !memcmp( si, di, strsize ) ){
				int ppxi;
				ShareX *sx;
				TCHAR *resp;
				DWORD attr;

				if ( !(type & PPXH_NOOVERLAP) ) return (const TCHAR *)(ptr + 6);
				// 既存のPPcのパスと重複していたら、無視する
				sx = Sm->P;
				for ( ppxi = 0 ; ppxi < X_Mtask ; ppxi++, sx++ ){
					if ( (sx->ID[0] != 'C') || (sx->ID[1] != '_') ) continue;
					if ( tstrcmp(sx->path, (const TCHAR *)(ptr + 6)) == 0 ) break;
				}
				attr = GetFileAttributes( (const TCHAR *)(ptr + 6) );
				if ( (ppxi == X_Mtask) &&
					 (attr != BADATTR) ){ // 実在しなければスキップ

					// 検索結果のリストファイルならスキップ
					resp = FindLastEntryPoint((const TCHAR *)(ptr + 6));
					if ( (memcmp(resp, T("PPX"), TSTROFF(3)) != 0) ||
						 (attr & FILE_ATTRIBUTE_DIRECTORY) ){
						 return (const TCHAR *)(ptr + 6);
					}
				}
			}
		}
		ptr += w;
	}
}
/*-----------------------------------------------------------------------------
	ヒストリに保存
-----------------------------------------------------------------------------*/
PPXDLL void PPXAPI WriteHistory(WORD type, const TCHAR *str, WORD b_size, void *bin)
{
	unsigned char *ptr;	// 現在の内容の先頭
	unsigned char *hismax;	// 最大値
	WORD fsize;		// １ヒストリ全体の大きさ
	WORD ssize;		// 文字列部分の大きさ
	WORD w;			// (+0)次の内容へのオフセット

	if ( *str == '\0' ) return; // からだ
	ssize = (WORD)TSTRSIZE(str);
	fsize = (WORD)(ssize + b_size + 6);
	ptr = HisP;
	hismax = HisP + X_Hsize - HIST_HEADER_FOOTER_SIZE - fsize;
		// どうあがいても入らない?
	if ( X_Hsize < ((DWORD)fsize + HIST_HEADER_FOOTER_SIZE) ) return;
	UsePPx();
	for ( ; ; ){
		w = *(WORD *)ptr;
								// 末尾or容量が確保できるぎりぎりの線
		if ( (w == 0) || ( hismax <= (ptr + w)) ){
			*(WORD *)(ptr + fsize) = 0;
			break;
		}
		if (	(*(WORD *)(ptr + 2) == type) &&
				(*(WORD *)(ptr + 4) == ssize) &&
				(memcmp( ptr + 6, str , ssize - sizeof(TCHAR)) == 0) ){
			if ( *(WORD *)(ptr + 0) != fsize ){ // サイズが変化した場合
				// →削除して書き直す
				FreePPx();
				DeleteHistory(type, str);
				WriteHistory(type, str, b_size, bin);
				return;
			}
			break;
		}
		ptr += w;
	}
	memmove( HisP + fsize, HisP, ptr - HisP);
	*(WORD *)(HisP + 0) = fsize;		// 次のリストへのポインタ
	*(WORD *)(HisP + 2) = type;			// 種類
	*(WORD *)(HisP + 4) = ssize;		// バイナリオフセット
	memcpy(HisP + 6, str, ssize);
	memcpy(HisP + 6 + ssize, bin, b_size);
	FreePPx();
}
/*-----------------------------------------------------------------------------
	指定番目のヒストリを取得, UsePPx-FreePPx間内で使用すること
-----------------------------------------------------------------------------*/
PPXDLL const TCHAR * PPXAPI EnumHistory(WORD type, int No)
{
	const char *ptr;

	ptr = (const char *)HisP;
	for ( ; ; ){
		int w;

		w = *(WORD *)ptr;
		if ( w == 0 ) return NULL;
		if ( *(WORD *)(ptr + 2) & type ){
			if ( No ){
				No--;
			}else{
				return (const TCHAR *)(ptr + 6);
			}
		}
		ptr += w;
	}
}

int CountHistory(WORD type)
{
	int count = 0;
	int w;
	BYTE *ptr;

	ptr = HisP;
	for ( ; ; ){
		w = *(WORD *)ptr;
		if ( w == 0 ) return count;
		if ( *(WORD *)(ptr + 2) & type ) count++;
		ptr += w;
	}
}
/*-----------------------------------------------------------------------------
	ヒストリを削除
-----------------------------------------------------------------------------*/
PPXDLL BOOL PPXAPI DeleteHistory(WORD type, const TCHAR *str)
{
	char *ptr, *found = NULL;
	WORD w, l;

	ptr = (char *)HisP;
	if ( str == NULL ){
		l = 0;
	}else{
		l = (WORD)TSTRSIZE(str);
	}
	UsePPx();
	for ( ; ; ){
		w = *(WORD *)ptr;
		if ( w == 0 ) break;	// 該当無し
		if ( *(WORD *)(ptr + 2) & type ){
			if ( !l ||
				((*(WORD *)(ptr + 4) == l) &&
					!memcmp( ptr + 6, str , l - sizeof(TCHAR)) ) ){
				if ( found == NULL ) found = ptr;
			}
		}
		if( w < *(WORD *)(ptr + 4) ){
			xmessage(XM_FaERRld, T("History data broken, fix now."));
			*(WORD *)ptr = 0;
			break;
		}
		ptr += w;
	}
	if ( found ){
		WORD fhistlen;

		fhistlen = *(WORD *)found;
		memmove(found, found + fhistlen, ptr + sizeof(WORD) - (found + fhistlen));
	}
	FreePPx();
	return found ? TRUE : FALSE;
}

/*-----------------------------------------------------------------------------
	PPcommon/Customize 関連
-----------------------------------------------------------------------------*/
// ●1.15 エラーアドレス検出に使うので、DefCust は、位置を変更させないこと
void DefCust(int mode)
{
	TCHAR *mem;				// カスタマイズ解析/終了位置
	TCHAR *log;
	HRSRC hres;
	DWORD size;

	hres = FindResource(DLLhInst, MAKEINTRESOURCE(DEFCUSTDATA), RT_RCDATA);
	#ifdef UNICODE
	{
		char *rc;
		DWORD rsize;
		UINT cp;

		rsize = SizeofResource(DLLhInst, hres);
		rc = LockResource(LoadResource(DLLhInst, hres));
		cp = IsValidCodePage(CP__SJIS) ? CP__SJIS : CP_ACP;
		size = MultiByteToWideChar(cp, 0, rc, rsize, NULL, 0);
		mem = HeapAlloc(ProcHeap, 0, TSTROFF(size) + 16);
		size = MultiByteToWideChar(cp, 0, rc, rsize, mem, size);
		mem[size] = '\0';
	}
	#else
		size = SizeofResource(DLLhInst, hres);
		mem = HeapAlloc(ProcHeap, 0, size + 16);
		memcpy(mem, LockResource(LoadResource(DLLhInst, hres)), size);
		mem[size] = '\0';
	#endif
	if ( PPcustCStore(mem, mem + size, mode >= 0 ? mode : -mode, &log, NULL) == 0 ){
										// 解析・格納 -------------------------
		xmessage(XM_FaERRd, T("DEFCUSTDATA store fault"));
	}
										// 結果表示 ---------------------------
	if (log){
		if (mode < 0){
			hUpdateResultWnd = PPEui(NULL,
				T("Customize ") T(FileCfg_Version) T(" update result"), log);
		}
		HeapFree(ProcHeap, 0, log);
	}
	HeapFree(ProcHeap, 0, mem);
}

// カスタマイズ領域を初期化する -----------------------------------------------
PPXDLL void PPXAPI InitCust(void)
{
	TCHAR buf1[VFPS], buf2[VFPS];
	TCHAR *memptr, *text, *maxptr;			// カスタマイズ解析位置

// カスタマイズの初期化 -------------------------------------------------------
	UsePPx();
	strcpy( ((PPXDATAHEADER *)(CustP - CUST_HEADER_SIZE))->HeaderID , CustID);
	CustSizeFromCustP = X_Csize;
	memset(CustP, 0, X_Csize - CUST_HEADER_SIZE);
	FreePPx();

// 初期値の登録 ---------------------------------------------------------------
	DefCust(0);
	SetCustData(T("PPxCFG"), T(FileCfg_Version), sizeof(T(FileCfg_Version)));
	if ( MessageTextTable != NULL ){ // テーブルが作成済み？
		if ( MessageTextTable != NOMESSAGETEXT ){
			HeapFree(ProcHeap, 0, MessageTextTable);
		}
		MessageTextTable = NULL;
	}

// Editor の設定を行う --------------------------------------------------------
	if ( GetRegString(HKEY_CLASSES_ROOT,
			RegTxtExtName, NilStr, buf1, TSIZEOF(buf1)) ){
		tstrcat(buf1, T("\\shell\\open\\command"));
										// アプリケーションのシェル -----------
		if ( GetRegString(HKEY_CLASSES_ROOT, buf1, NilStr, buf1, TSIZEOF(buf1)) ){
			const TCHAR *ptr;

			ptr = buf1;
			GetLineParam(&ptr, buf2);
			if ( tstrchr(buf2, ' ') != NULL ){
				wsprintf(buf1, T("\"%s\""), buf2);
				ptr = buf1;
			}else{
				ptr = buf2;
			}
			SetCustStringTable(T("A_exec"), T("editor"), ptr, 0);
			ptr = FindLastEntryPoint(buf2);
			if ( !tstricmp(ptr, T("WZEDITOR.EXE")) ){
				SetCustTable(T("A_exec"), T("editorL"), T("/J"), TSTROFF(3));
			}
		}
	}
// 初期カスタマイズを読んで、設定 ---------------------------------------------
	wsprintf(buf1, T("%s\\PPXDEF.CFG"), DLLpath);
	if ( LoadTextImage(buf1, &memptr, &text, &maxptr) == NO_ERROR ){
		PPcustCStore(text, maxptr, PPXCUSTMODE_APPEND, NULL, NULL);
		HeapFree(ProcHeap, 0, memptr);
		PPxCommonExtCommand(K_menukeycust, 0);
	}else{
		int len;

		len = wsprintf(buf1, T("%c*linemessage %s%%:*setcust KC_main:-|firstevent="), EXTCMD_CMD, MessageText(MES_NOIC));
		SetCustTable(T("KC_main"), T("firstevent"), buf1, TSTROFF(len + 1));
	}
}

void USEFASTCALL MakeCustMemSharename(TCHAR *custsharename, int id)
{
	wsprintf(custsharename, CUSTMEMNAME T("%d"), id);
}

void USEFASTCALL CustTableError(const TCHAR *str)
{
	TCHAR buf[0x100];

	wsprintf(buf, T("%s\nDetect Customize data collapsed.\nカスタマイズ領域が破損しています"), str);
	CriticalMessageBox(buf);
}
/*-----------------------------------------------------------------------------
	カスタマイズ領域を拡張する。SetCustDataから呼ばれる
-----------------------------------------------------------------------------*/
BOOL ExtendCustomizeArea(size_t size)
{
	TCHAR custfilename[MAX_PATH];
	TCHAR custsharename[MAX_PATH];
	int result, id;
	DWORD newsize;
	BYTE *tmpcust;

	if ( SM_cust.fileH == INVALID_HANDLE_VALUE ) return FALSE; // すでに失敗している
	Sm->CustomizeWrite++;	// カスタマイズ操作通知

	// 新しいサイズ
	if ( size != 0 ){
		newsize = (X_Csize + (size + EXTENDCUSTOMIZEDELTA)) & ~(EXTENDCUSTOMIZEDELTA - 1);
	}else{
		newsize = CustSizeFromCustP;
	}
	// 仮カスタマイズ領域を作成
	tmpcust = HeapAlloc(DLLheap, 0, (newsize > X_Csize) ? newsize : X_Csize);
	if ( tmpcust == NULL ) return FALSE;
	CustSizeFromCustP = newsize;
	memcpy(tmpcust, (BYTE *)SM_cust.ptr, X_Csize);
	CustP = tmpcust + CUST_HEADER_SIZE;
	X_Csize = newsize;

	// カスタマイズ領域を廃棄し、新規作成
	FileMappingOff(&SM_cust);
	id = Sm->CustomizeNameID;
	if ( size ) id++;
	MakeUserfilename(custfilename, CUSTNAME, PPxRegPath);
	MakeCustMemSharename(custsharename, id);
	result = FileMappingOn(&SM_cust, custfilename, custsharename, newsize, FILE_ATTRIBUTE_NORMAL);
	if ( result < 0 ){ // カスタマイズ領域作成失敗…仮カスタマイズ領域を残す
		SM_cust.ptr = NULL;
		SM_cust.fileH = INVALID_HANDLE_VALUE;
		SM_cust.mapH = INVALID_HANDLE_VALUE;
		XMessage(NULL, PPxName, XM_NiWRNld, expandfailed);
		return FALSE;
	}else{	// 成功したので再設定
		Sm->CustomizeNameID = id;
		CustP = (BYTE *)SM_cust.ptr + CUST_HEADER_SIZE;
		HeapFree(DLLheap, 0, tmpcust); // 仮カスタマイズ領域を廃棄
		if ( size ){ //結果メッセージ。拡張元は、前回から一定時間越えた時に表示
			UINT type = XM_DbgLOG;
#if REPORTEXPAND
			if ( (GetTickCount() - ExtendPopTick) > 10000 ){
				ExtendPopTick = GetTickCount();
				type = XM_FaERRld;
			}
#endif
			XMessage(NULL, PPxName, type, expandsuccess, X_Csize);
		}else{
			XMessage(NULL, PPxName, XM_DbgLOG, expandfix, X_Csize);
		}
		return TRUE;
	}
}

/*-----------------------------------------------------------------------------
	１項のみのカスタマイズ内容を設定する
	成功なら NO_ERROR
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI SetCustData(const TCHAR *str, const void *bin, size_t b_size)
{
	BYTE *ptr;		// 現在の内容の先頭
	BYTE *maxptr;	// カスタマイズ領域の最後+1の位置
	BYTE *top;		// カスタマイズ内容の最後の位置
	size_t fsize;	// １項目全体の大きさ
	size_t ssize;	// 文字列部分の大きさ
	DWORD datasize;		// (+0)次の内容へのオフセット

	ssize = TSTRSIZE(str);
	fsize = ssize + b_size + sizeof(DWORD);

	UsePPx();
	if ( CustSizeFromCustP != X_Csize ) ExtendCustomizeArea(0);

	ptr = CustP;
	top = maxptr = ptr + X_Csize - CUST_HEADER_FOOTER_SIZE - fsize;
	for ( ; ; ){
		datasize = *(DWORD *)ptr;
									// 末尾の判断
		if ( datasize == 0 ){
			top = ptr;
			break;
		}
									// 同名の項目の判断
		if ( !tstricmp((TCHAR *)(ptr + 4), str) ){
								// 内容の大きさが同じ：再配置の必要なし
			if ( datasize != fsize ){	// 内容の大きさが違う：再配置処理
				DWORD tmpw;

				top = ptr;	// 取り敢えず最後の項目を探す
				tmpw = datasize;
				do {
					top += tmpw;
					tmpw = *(DWORD *)top;
				}while( tmpw != 0 );
									// カスタマイズ領域が不足した?
				if ( (top - datasize) > maxptr ){
					datasize = (top - datasize) - maxptr;
					goto nofree;
				}
							// すき間を詰める
				memmove(ptr , ptr + datasize , top - ptr - datasize);
				top -= datasize;
				ptr = top;
				Sm->CustomizeWrite++;
			}
			break;
		}
		ptr += datasize;
	}
	if ( ptr > maxptr ){	// カスタマイズ領域が不足?
		datasize = ptr - maxptr;
		goto nofree;
	}
									// 内容の登録
	*(DWORD *)(ptr + 0) = fsize;
	memcpy(ptr + 4, str, ssize);
	memcpy(ptr + 4 + ssize, bin, b_size);
									// 最後なら EndOfData を付加
	if ( ptr == top ) *(DWORD *)(ptr + fsize) = 0;
	FreePPx();
	return NO_ERROR;

nofree:
	if ( IsTrue(ExtendCustomizeArea(datasize)) ){	// 拡張に成功
		FreePPx();
		return SetCustData(str, bin, b_size);
	}
	FreePPx();
	xmessage(XM_FaERRld, MES_ECFL);
	return -1;
}
/*-----------------------------------------------------------------------------
	カスタマイズ内容の１項を削除する
	成功なら NO_ERROR	見つからないなら 1
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI DeleteCustData(const TCHAR *str)
{
	BYTE *ptr;	// 現在の内容の先頭
	BYTE *top;	// カスタマイズ内容の最後の位置
	DWORD datasize, s;	// (+0)次の内容へのオフセット

	UsePPx();
	if ( CustSizeFromCustP != X_Csize ) ExtendCustomizeArea(0);
	ptr = CustP;
	for ( ; ; ){
		datasize = *(DWORD *)ptr;
									// 末尾の判断
		if ( datasize == 0 ) break;
									// 同名の項目の判断
		if ( !tstricmp( (TCHAR *)(ptr + 4), str )){
			top = ptr;	// 取り敢えず最後の項目を探す
			s = datasize;
			do {
				top += s;
				s = *(DWORD *)top;
			}while( s );
						// すき間を詰める
			memmove( ptr , ptr + datasize , top - ptr - datasize + 4);
			datasize = (NO_ERROR ^ 1);
			break;
		}
		ptr += datasize;
	}
	FreePPx();
	Sm->CustomizeWrite++;
	return (int)datasize ^ 1;
}
/*-----------------------------------------------------------------------------
	１項のみのカスタマイズ内容を取得する
	成功なら NO_ERROR
	b_size = 0 なら保存に必要な大きさを返す
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI GetCustData(const TCHAR *str, void *bin, size_t b_size)
{
	size_t ssize;		// 文字列部分の大きさ
	BYTE *ptr;
	int datasize;

	ssize = TSTRSIZE(str);
	UsePPx();
	if ( CustSizeFromCustP != X_Csize ) ExtendCustomizeArea(0);
	ptr = CustP;
	for ( ; ; ){
		datasize = *(int *)ptr;
		if ( datasize == 0 ){		// 末端に到着
			datasize = -1;
			break;
		}
		if ( !tstricmp( (TCHAR *)(ptr + 4), str) ){
			datasize -= (int)(ssize + 4);
			if ( !b_size  ) break;		// b_size == 0 ... 必要な大きさを報告
			if ( STATICCAST(size_t, datasize) > b_size ) datasize = b_size;
			memcpy(bin, ptr + 4 + ssize, datasize);
			datasize = NO_ERROR;
			break;
		}
		ptr += datasize;
	}
	FreePPx();
	return datasize;
}
/*-----------------------------------------------------------------------------
	指定番目の１項のみのカスタマイズ内容を取得する
	失敗なら -1 成功なら、読み込んだ大きさ
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI EnumCustData(int offset, TCHAR *str, void *bin, size_t b_size)
{
	BYTE *ptr;
	DWORD datasize;
	size_t ssize;

	UsePPx();
	if ( CustSizeFromCustP != X_Csize ) ExtendCustomizeArea(0);
	ptr = CustP;
	for ( ; ; ){
		datasize = *(DWORD *)ptr;
		if ( datasize == 0 ){			// 末端に到着
			FreePPx();
			return (offset < 0) ? -1 - offset : -1;
		}
		if ( offset == 0 ) break;
		ptr += datasize;
		offset--;
	};
	tstrcpy(str, (TCHAR *)(ptr + 4));
	ssize = TSTRSIZE(str) + 4;
	datasize -= ssize;
	if ( datasize > b_size ) datasize = b_size;
	memcpy(bin, ptr + ssize, datasize);
	FreePPx();
	return datasize;
}

/*-----------------------------------------------------------------------------
	カスタマイズ内容へのポインタを取得する
	UsePPx-FreePPx間内で使用すること
-----------------------------------------------------------------------------*/
BYTE *GetCustDataPtr(const TCHAR *str)
{
	BYTE *ptr; // 現在の内容の先頭
	DWORD datasize; // (+0)次の内容へのオフセット

	if ( CustSizeFromCustP != X_Csize ) ExtendCustomizeArea(0);
	ptr = CustP;

	for ( ; ; ){
		datasize = *(DWORD *)ptr;
		if ( datasize == 0 ) return NULL;					// 末尾の判断
		if ( tstricmp((TCHAR *)(ptr + 4), str ) == 0 ){
			return ptr + TSTRSIZE(str) + 4;
		}
		ptr += datasize;
	}
}

BYTE *GetCustDataPtrCache(const TCHAR *str)
{
	BYTE *ptr; // 現在の内容の先頭
	DWORD datasize; // (+0)次の内容へのオフセット

	if ( CustSizeFromCustP != X_Csize ) ExtendCustomizeArea(0);
	// キャッシュが利用できるかチェック
	if ( (enumcustcache.Counter == Sm->CustomizeWrite) &&
		!tstricmp((TCHAR *)(enumcustcache.DataPtr + 4), str) ){
		return enumcustcache.DataPtr + TSTRSIZE(str) + 4;
	}
	ptr = CustP;

	for ( ; ; ){
		datasize = *(DWORD *)ptr;
		if ( datasize == 0 ) return NULL;					// 末尾の判断
		if ( tstricmp((TCHAR *)(ptr + 4), str ) == 0 ){
			enumcustcache.DataPtr = ptr;
			enumcustcache.Counter = Sm->CustomizeWrite;
			enumcustcache.SubOffset = UNENUMCACHE;
			return ptr + TSTRSIZE(str) + 4;
		}
		ptr += datasize;
	}
}

/*-----------------------------------------------------------------------------
	配列内のカスタマイズ内容を削除する
	成功なら NO_ERROR  配列がないなら 1 項目がないなら 2
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI DeleteCustTable(const TCHAR *str, const TCHAR *sub, int index)
{
	BYTE *btm;	// テーブルの先頭
	BYTE *ptr;	// 現在の内容の先頭
	BYTE *top;	// カスタマイズ内容の最後の位置
	DWORD itemsize, s;	// (+0)次の内容へのオフセット

	UsePPx();
	btm = GetCustDataPtr(str);
	if ( btm == NULL ){			// 存在しない ---------------------------------
		FreePPx();
		return 1;
	}
								// 追加・更新 ---------------------------------
	ptr = btm;
	for ( ; ; ){
		itemsize = *(WORD *)ptr;
									// 末尾の判断
		if ( itemsize == 0 ) break;	// なし処理へ
		if ( (sub != NULL) ?
				(!tstricmp((TCHAR *)(ptr + 2), sub)) :	// 同名の項目の判断
				(index == 0) ){
									// 内容の大きさが違う：再配置処理
			top = ptr;	// 取り敢えず最後の項目を探す
			s = itemsize;
			do {
				top += s;
				s = *(WORD *)top;
			}while( s );
						// すき間を詰める
			memmove( ptr , ptr + itemsize , top - ptr - itemsize + 2);
			itemsize = (NO_ERROR ^ 2);
			break;
		}
		ptr += itemsize;
		index--;
	}
	Sm->CustomizeWrite++;
	FreePPx();
	return (int)itemsize ^ 2;
}
/*-----------------------------------------------------------------------------
	配列内のカスタマイズ内容を保存する
	成功なら NO_ERROR(0)
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI SetCustTable(const TCHAR *str, const TCHAR *sub, const void *bin, size_t b_size)
{
	BYTE *btm, *newbtm;		// テーブルの先頭
	BYTE *ptr;		// 現在の内容の先頭
	DWORD fsize;	// １項目全体の大きさ
	DWORD ssize;	// 文字列部分の大きさ
	DWORD oldtablesize = 0, newtablesize;
	DWORD s, itemsize;
	int result;

	ssize = TSTRSIZE(sub);
	fsize = ssize + b_size + TABLENEXTOFFSETSIZE;
	if ( fsize > 0xfffe ) return -1;

	UsePPx();
	btm = GetCustDataPtr(str);
	if ( btm == NULL ){			// 新規作成 -----------------------------------
		btm = HeapAlloc(DLLheap, 0, fsize + TABLEENDMARKSIZE);
		if ( btm != NULL ){
			*(WORD *)(btm + 0) = (WORD)fsize;
			memcpy(btm + TABLENEXTOFFSETSIZE, sub, ssize);
			memcpy(btm + TABLENEXTOFFSETSIZE + ssize, bin, b_size);
			*(WORD *)(btm + fsize) = 0;
			result = SetCustData(str, btm, fsize + TABLEENDMARKSIZE);
			HeapFree(DLLheap, 0, btm);
		}else{
			result = -1;
		}
		FreePPx();
		return result;
	}
								// 追加・更新 ---------------------------------
	ptr = btm;
	for ( ; ; ){
		itemsize = *(WORD *)ptr;
									// 末尾の判断
		if ( itemsize == 0 ) break;	// 追加処理へ
									// 同名の項目の判断
		if ( !tstricmp( (TCHAR *)(ptr + TABLENEXTOFFSETSIZE), sub )){
			BYTE *top; // カスタマイズ内容の最後の位置

			if ( itemsize == fsize ){	// 内容の大きさが同じ：更新のみ
				memcpy(ptr + TABLENEXTOFFSETSIZE + ssize, bin, b_size);
				FreePPx();
				return 0;
			}
								// 内容の大きさが違う：再配置処理
			top = ptr;	// 取り敢えず最後の項目を探す
			s = itemsize;
			do {
				top += s;
				s = *(WORD *)top;
			}while( s );

			oldtablesize = top - ptr - itemsize;
			break;
		}
		ptr += itemsize;
	}
	s = ptr - btm;	// 前半の大きさ
	newtablesize = s + fsize + oldtablesize + TABLEENDMARKSIZE;
	newbtm = HeapAlloc(DLLheap, 0, newtablesize);
	if ( newbtm == NULL ){
		FreePPx();
		return -1;
	}
	memcpy(newbtm, btm, s);	// 元データの前半を回収
						// 今回のデータを登録
	*(WORD *)(newbtm + s) = (WORD)fsize;
	memcpy(newbtm + s + TABLENEXTOFFSETSIZE, sub, ssize);
	memcpy(newbtm + s + TABLENEXTOFFSETSIZE + ssize, bin, b_size);
	if ( itemsize ){			// 元データの後半を回収
		memcpy(newbtm + s + fsize, btm + s + itemsize, oldtablesize);
		s += oldtablesize;
	}
	*(WORD *)(newbtm + s + fsize) = 0;
	result = SetCustData(str, newbtm, s + fsize + TABLEENDMARKSIZE);
	HeapFree(DLLheap, 0, newbtm);
	Sm->CustomizeWrite++;
	FreePPx();
	return result;
}

#pragma argsused
PPXDLL int PPXAPI SetCustStringTable(const TCHAR *str, const TCHAR *sub, const TCHAR *string, int keep_length)
{
	BYTE *btm, *newbtm;		// テーブルの先頭
	BYTE *ptr;		// 現在の内容の先頭
	DWORD fsize;	// １項目全体の大きさ
	DWORD ssize;	// 文字列部分の大きさ
	DWORD oldtablesize = 0, newtablesize;
	DWORD s, itemsize;
	int result;
	size_t b_size;

	UnUsedParam(keep_length);

	ssize = TSTRSIZE(sub);
	b_size = TSTRSIZE(string);
	fsize = ssize + b_size + TABLENEXTOFFSETSIZE;
	if ( fsize > 0xfffe ) return -1;

	UsePPx();
	btm = GetCustDataPtr(str);
	if ( btm == NULL ){			// 新規作成 -----------------------------------
		FreePPx();
		return SetCustTable(str, sub, string, b_size);
	}
								// 追加・更新 ---------------------------------
	ptr = btm;
	for ( ; ; ){
		itemsize = *(WORD *)ptr;
									// 末尾の判断
		if ( itemsize == 0 ) break;	// 追加処理へ
									// 同名の項目の判断
		if ( !tstricmp( (TCHAR *)(ptr + TABLENEXTOFFSETSIZE), sub) ){
			BYTE *top; // カスタマイズ内容の最後の位置

			if ( itemsize >= fsize ){ // 内容の大きさが同じか小さい：更新のみ
				memcpy(ptr + TABLENEXTOFFSETSIZE + ssize, string, b_size);
				FreePPx();
				return 0;
			}
								// 内容の大きさが違う：再配置処理
			top = ptr;	// 取り敢えず最後の項目を探す
			s = itemsize;
			do {
				top += s;
				s = *(WORD *)top;
			}while( s );

			oldtablesize = top - ptr - itemsize;
			break;
		}
		ptr += itemsize;
	}
	s = ptr - btm;	// 前半の大きさ
	newtablesize = s + fsize + oldtablesize + TABLEENDMARKSIZE;
	newbtm = HeapAlloc(DLLheap, 0, newtablesize);
	if ( newbtm == NULL ){
		FreePPx();
		return -1;
	}
	memcpy(newbtm, btm, s);	// 元データの前半を回収
						// 今回のデータを登録
	*(WORD *)(newbtm + s) = (WORD)fsize;
	memcpy(newbtm + s + TABLENEXTOFFSETSIZE, sub, ssize);
	memcpy(newbtm + s + TABLENEXTOFFSETSIZE + ssize, string, b_size);
	if ( itemsize ){			// 元データの後半を回収
		memcpy(newbtm + s + fsize, btm + s + itemsize, oldtablesize);
		s += oldtablesize;
	}
	*(WORD *)(newbtm + s + fsize) = 0;
	result = SetCustData(str, newbtm, s + fsize + TABLEENDMARKSIZE);
	HeapFree(DLLheap, 0, newbtm);
	Sm->CustomizeWrite++;
	FreePPx();
	return result;
}
/*-----------------------------------------------------------------------------
	配列内のカスタマイズ内容を末尾に追加する
	成功なら NO_ERROR
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI InsertCustTable(const TCHAR *str, const TCHAR *sub, DWORD index, const void *bin, size_t b_size)
{
	BYTE *btm, *newbtm;	// テーブルの先頭
	BYTE *insp;	// 挿入位置
	BYTE *top;	// カスタマイズ内容の最後の位置
	DWORD fsize;	// １項目全体の大きさ
	DWORD ssize;	// 文字列部分の大きさ
	DWORD itemsize, s;	// (+0)次の内容へのオフセット
	int result;
	DWORD newtablesize;

	ssize = TSTRSIZE(sub);
	fsize = ssize + b_size + TABLENEXTOFFSETSIZE;
	if ( fsize > 0xfffe ) return -1;

	UsePPx();
	btm = GetCustDataPtr(str);
	if ( btm == NULL ){			// 新規作成 -----------------------------------
		btm = HeapAlloc(DLLheap, 0, fsize + TABLEENDMARKSIZE);
		if ( btm != NULL ){
			*(WORD *)(btm + 0) = (WORD)fsize;
			memcpy(btm + TABLENEXTOFFSETSIZE, sub, ssize);
			memcpy(btm + TABLENEXTOFFSETSIZE + ssize, bin, b_size);
			*(WORD *)(btm + fsize) = 0;
			result = SetCustData(str, btm, fsize + TABLEENDMARKSIZE);
			HeapFree(DLLheap, 0, btm);
		}else{
			result = -1;
		}
		FreePPx();
		return result;
	}
								// 追加・更新 ---------------------------------
	top = btm;
	for ( ; ; ){
		itemsize = *(WORD *)top;
									// 末尾の判断
		if ( itemsize == 0 ){
			insp = top;
			break;
		}
									// indexの判断
		if ( !index ){
			insp = top;
			do {			// 最後の項目を探す
				top += itemsize;
				itemsize = *(WORD *)top;
			}while( itemsize != 0 );
			break;
		}
		index--;
		top += itemsize;
	}
	newtablesize = (top - btm) + fsize + TABLEENDMARKSIZE;
	newbtm = HeapAlloc(DLLheap, 0, newtablesize);
	if ( newbtm == NULL ){
		FreePPx();
		return -1;
	}
	s = insp - btm;
	if ( s != 0 ) memcpy(newbtm, btm, s);
	btm = newbtm + s;
	*(WORD *)btm = (WORD)fsize;
	btm += sizeof(WORD);
	memcpy(btm, sub, ssize);
	btm += ssize;
	memcpy(btm, bin, b_size);
	btm += b_size;
	memcpy(btm, insp, (top - insp) + sizeof(WORD));
	result = SetCustData(str, newbtm, (btm - newbtm) + (top - insp) + TABLEENDMARKSIZE);
	HeapFree(DLLheap, 0, newbtm);
	Sm->CustomizeWrite++;
	FreePPx();
	return result;
}
/*-----------------------------------------------------------------------------
	配列内のカスタマイズ内容を取得する
	成功なら NO_ERROR
	b_size = 0 なら保存に必要な大きさを返す
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI GetCustTable(const TCHAR *str, const TCHAR *sub, void *bin, size_t b_size)
{
	BYTE *ptr, *custmax;	// 現在の内容の先頭
	size_t ssize;	// 文字列部分の大きさ
	WORD itemsize;		// (+0)次の内容へのオフセット

	UsePPx();
	ptr = GetCustDataPtr(str);
	if ( ptr == NULL ){
		FreePPx();
		return -1;	// 未登録
	}
	custmax = CustP + X_Csize - CUST_FOOTER_SIZE - CUST_TABLE_FOOTER_SIZE;
	for ( ; ; ){
		itemsize = *(WORD *)ptr;
		if ( itemsize == 0 ){
			FreePPx();
			return -1;
		}
		if ( tstricmp( (TCHAR *)(ptr + 2), sub) == 0 ) break;
		ptr += itemsize;
		if ( ptr > custmax ) goto collapsed;
	}
	ssize = TSTRSIZE(sub);
	itemsize -= (WORD)(ssize + 2);
	if ( b_size == 0 ){
		FreePPx();
		return (int)itemsize;
	}
	if ( (DWORD)itemsize > b_size ) itemsize = (WORD)b_size;
	memcpy(bin, ptr + 2 + ssize, itemsize);
	FreePPx();
	return NO_ERROR;

collapsed:
	if ( CustSizeFromCustP == X_Csize ){ // 破損領域を破棄する
		*(WORD *)(ptr - itemsize) = 0;
	}
	FreePPx();
	CustTableError(str);
	return -1;
}

/*-----------------------------------------------------------------------------
	指定番目の配列内のカスタマイズ内容を取得する
	失敗なら -1 成功なら、読み込んだ大きさ
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI EnumCustTable(int offset, const TCHAR *str, TCHAR *sub, void *bin, size_t b_size)
{
	BYTE *tableptr;
	DWORD itemsize;
	size_t ssize;
	int offsetleft;

	UsePPx();
	tableptr = GetCustDataPtrCache(str);
	if ( tableptr == NULL ){
		FreePPx();
		return -1;	// 未登録
	}

	// キャッシュが利用可能ならキャッシュを使って開始位置を求める
	offsetleft = offset;
	if ( enumcustcache.SubOffset <= offsetleft ){
		tableptr = enumcustcache.SubPtr;
		offsetleft -= enumcustcache.SubOffset;
	}

	for ( ; ; ){
		itemsize = *(WORD *)tableptr;
		if ( itemsize == 0 ){
			FreePPx();
			return (offsetleft < 0) ? -1 - offsetleft : -1;
		}
		if ( offsetleft == 0 ) break;
		tableptr += itemsize;
		offsetleft--;
	};
	if ( (tableptr + itemsize) > (CustP + X_Csize - CUST_FOOTER_SIZE - CUST_TABLE_FOOTER_SIZE) ){
		goto collapsed;
	}
	ssize = TSTRSIZE((TCHAR *)(tableptr + 2)) + 2;
	if ( ssize >= itemsize ) goto collapsed;
	itemsize -= ssize;
	if ( itemsize > b_size ) itemsize = b_size;
	memcpy(sub, (TCHAR *)(tableptr + 2), ssize - 2);
	memcpy(bin, tableptr + ssize, itemsize);
	enumcustcache.SubPtr = tableptr;
	enumcustcache.SubOffset = offset;
	FreePPx();
	return itemsize;

collapsed:
	if ( CustSizeFromCustP == X_Csize ){ // 破損領域を破棄する
		*(WORD *)tableptr = 0;
	}
	FreePPx();
	CustTableError(str);
	return -1;
}

void LoadMessageTextTable(void)
{
	TCHAR tablename[12];
	int size;
	LCID tlcid;
	int count;
	BYTE *makingtable;

	UsePPx();
	// 使用する MexXX の名前を用意
	tlcid = (LCID)GetCustDword(T("X_LANG"), 0);
	if ( !tlcid ) tlcid = LOWORD(GetUserDefaultLCID());
	wsprintf(tablename, T("Mes%04X"), tlcid);

	{ // 使用する MexXX の CRC32 を取得し、変化無ければ既存のを使用
		BYTE *tableptr = GetCustDataPtrCache(tablename);
		DWORD CRC;

		if ( tableptr == NULL ){
			size = 0;
		}else{
			size = *(DWORD *)enumcustcache.DataPtr;
		}
		if ( size <= 2 ){
			if ( size < 0 ){
				MessageTextTableCount = 0;
				SetCustData(tablename, &MessageTextTableCount, 2); // \0 \0 を書き込み、エントリをとりあえず作成する
			}
			goto error;
		}
		CRC = crc32(tableptr, size, 0);
		if ( MessageTextTable != NULL ){ // テーブルが作成済み？
			if ( MessageTextTable != NOMESSAGETEXT ){
				if ( CRC == MessageTextCRC ){ // 内容が同じ？
					FreePPx(); // 同じなので処理しない
					return;
				}
				HeapFree(ProcHeap, 0, MessageTextTable);
			}
		}
		MessageTextCRC = CRC;
	}

	count = SortCustTable(tablename, NULL); // 並び替え & 破損チェック & 個数チェック
	resetflag(count, SORTCUSTTABLE_SORT);
	if ( count == 0 ){
		goto error;
	}
	size = DwordAlignment(size);
	makingtable = HeapAlloc(ProcHeap, 0, size + count * sizeof(BYTE *));
	if ( makingtable != NULL ){
		BYTE *dataptr;
		BYTE **indexptr;
		WORD w;

		MessageTextTableCount = count;
		MessageTextIndexTable = (BYTE **)(BYTE *)(makingtable + size);

		GetCustData(tablename, makingtable, size);
		dataptr = makingtable;
		indexptr = MessageTextIndexTable;
		for ( ;; ){
			w = *(WORD *)dataptr;
			if ( w != 0 ){
				*indexptr++ = dataptr + sizeof(WORD);
				dataptr += w;
				continue;
			}else{ // 異常系。通常は起きない
				MessageTextTableCount = indexptr - MessageTextIndexTable;
				break;
			}
		}
		MessageTextTable = makingtable;
		FreePPx();
		return;
	}
error:
	MessageTextTable = NOMESSAGETEXT;
	FreePPx();
	return;
}

#define MESTEXTOFFSETONCUST (TSTROFF(MESTEXTIDLEN) + sizeof(TCHAR))

const TCHAR * USEFASTCALL SearchMessageText(const TCHAR *idstr)
{
	const TCHAR *idstrtmp = idstr;
	BYTE **index;
	int mini, maxi;

	if ( MessageTextTable != NOMESSAGETEXT ){
		index = MessageTextIndexTable;
		mini = 0;
		maxi = MessageTextTableCount;
		while ( mini < maxi ){ // バイナリサーチで検索
			int mid, result;

			mid = (mini + maxi) / 2;
			result = memcmp( *(index + mid), idstrtmp, TSTROFF(MESTEXTIDLEN));
			if ( result < 0 ){
				mini = mid + 1;
			}else if ( result > 0 ){
				maxi = mid;
			}else{
				return (TCHAR *)(BYTE *)(*(index + mid) + MESTEXTOFFSETONCUST);
			}
		}
	}
	return NULL;
}

PPXDLL const TCHAR * PPXAPI MessageText(const TCHAR *text)
{
	const TCHAR *deftext;
	const TCHAR *findtext;

	if ( text == NULL ) return NULL;

	deftext = text;
	if ( (deftext[0] == '\0') || (deftext[1] == '\0') ||
		 (deftext[2] == '\0') || (deftext[3] == '\0') ||
		 (deftext[4] != '|') ){
		return text;	// alias 指定がない
	}
									// テーブルがなければ作成 -----------------
	if ( MessageTextTable == NULL ) LoadMessageTextTable();
									// 検索 -----------------------------------
	findtext = SearchMessageText(text);
	if ( findtext != NULL ) return findtext;
	return text + MESTEXTIDLEN + 1;
}

void ReloadMessageText(void)
{
	if ( MessageTextTable == NULL ) return;
	LoadMessageTextTable();
}

int WINAPI SortCust_Name(const BYTE *cust1, const BYTE *cust2)
{
	return tstrcmp((const TCHAR *)(const BYTE *)(cust1 + TABLENEXTOFFSETSIZE),
			(const TCHAR *)(const BYTE *)(cust2 + TABLENEXTOFFSETSIZE));
}

PPXDLL int PPXAPI SortCustTable(const TCHAR *str, int (WINAPI *func)(const BYTE *cust1, const BYTE *cust2))
{
	int count = 1;
	int sorted = 1;
	int result;
	BYTE *tablefirst, *tablelast, *tablemax, *areamax;

	if ( func == NULL ) func = SortCust_Name;
	UsePPx();
	// sort 済みか確認 & 個数算出
	{
		DWORD itemsize;
		BYTE *np, *bp;

		tablefirst = GetCustDataPtrCache(str);
		if ( tablefirst == NULL ) goto nodata;	// 未登録
		tablemax = enumcustcache.DataPtr + *(DWORD *)enumcustcache.DataPtr;
		areamax = CustP + X_Csize - sizeof(DWORD);
		if ( tablemax > areamax ){ // テーブル破損(カスタマイズ領域からはみ出た)
			tablemax = areamax;
			*(DWORD *)enumcustcache.DataPtr = tablemax - enumcustcache.DataPtr;
			CustTableError(str);
		}
			// 1つ目(比較無し)
		bp = tablefirst;
		itemsize = *(WORD *)bp;
		if ( itemsize == 0 ) goto nodata; // 登録数０
		np = bp + itemsize;
		tablemax -= sizeof(WORD); // EoD のマーキング分を減らす
			// 2つ目以降(比較有り)
		for ( ; ; ){
			if ( np <= tablemax ){
				itemsize = *(WORD *)np;
				if ( itemsize == 0 ) break;
				if ( sorted && (func(bp, np) > 0) ) sorted = 0;
				bp = np;
				np += itemsize;
				count++;
				continue;
			}else{ // 領域破損
				np -= itemsize;
				*(WORD *)np = 0;
				break;
			}
		}
		tablelast = np;
	}
	result = count;
	if ( sorted == 0 ){ // ソートされていないときはソートを行う
		BYTE **indextable, **indextablep; // ソート用インデックス
		BYTE *tabledata, *tabledatap; // ソート後内容
		BYTE *tp;

		setflag(result, SORTCUSTTABLE_SORT);
		indextable = (BYTE **)HeapAlloc(DLLheap, 0, count * sizeof(BYTE *));
		if ( indextable == NULL ) goto nodata;
		// ↓テーブル末尾は記憶する必要がないので省略
		tabledata = (BYTE *)HeapAlloc(DLLheap, 0, (tablelast - tablefirst));
		if ( tabledata == NULL ){
			HeapFree(DLLheap, 0, indextable);
			goto nodata;
		}
		// インデックスを作成しながら挿入ソート
		indextablep = indextable;
			// 1つ目(比較無し)
		tp = tablefirst;
		*indextablep = tp;
		tp = tp + *(WORD *)tp;
			// 2つ目以降(比較有り)
		for ( ; ; ){
			WORD itemsize;

			itemsize = *(WORD *)tp;
			if ( itemsize == 0 ) break;

			if ( func(*indextablep, tp) > 0 ){
				BYTE **indextable_cp;

				for ( indextable_cp = indextablep ; indextable_cp > indextable ; ){
					if ( func(*(indextable_cp - 1), tp) <= 0 ) break;
					indextable_cp--;
				}
				memmove(indextable_cp + 1, indextable_cp, (indextablep - indextable_cp + 1) * sizeof(BYTE *));
				*indextable_cp = tp;
				indextablep++;
			}else{
				indextablep++;
				*indextablep = tp;
			}
			tp += itemsize;
		}
		// インデックスを元にデータを作成
		indextablep = indextable;
		tabledatap = tabledata;
		while ( count-- ){
			WORD itemsize;

			if ( *indextablep == NULL ) break;
			itemsize = *(WORD *)*indextablep;
			memcpy(tabledatap, *indextablep, itemsize);
			indextablep++;
			tabledatap += itemsize;
		}
		// インデックスを元に作成したデータを保存
		memcpy(tablefirst, tabledata, tabledatap - tabledata);
		HeapFree(DLLheap, 0, tabledata);
		HeapFree(DLLheap, 0, indextable);
	}
	FreePPx();
	return result;
nodata:
	FreePPx();
	return 0;
}

// ●1.15 エラーアドレス検出に使うので、GetCustDword は、PPD_HIST.C の末尾にすること
DWORD USEFASTCALL GetCustDword(const TCHAR *name, DWORD defaultvalue)
{
	DWORD value;

	if ( NO_ERROR != GetCustData(name, &value, sizeof(value)) ){
		return defaultvalue;
	}
	return value;
}
