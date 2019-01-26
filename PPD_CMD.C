/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library						マクロ実行処理
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#include "CALC.H"
#pragma hdrstop
#define DEFCMDNAME
#include "PPXCMDS.C"

#define ISCMDNULREQUEIERSIZE TSTROFF(2)

#ifndef SC_MONITORPOWER
#define SC_MONITORPOWER 0xF170
#endif

BOOL EditExtractText(EXECSTRUCT *Z);
BOOL EnumEntries(EXECSTRUCT *Z,const TCHAR *extract);

const TCHAR HistType[] = T("gnhdcfsmpveuUxX");	// ヒストリ種類を表わす文字 ※ R,O は別の用途で使用中
#define HistType_GENERAL 0 // g 相当
#define HistType_PATHFIX 5 // f 相当

const TCHAR ExecuteDefaultTitle[] = MES_TEXE;
const TCHAR Title_ValueName[] = T(StringVariable_Command_Title);
const TCHAR ResponseName_ValueName[] = T(StringVariable_Command_Response);
const TCHAR EditCache_ValueName[] = T(StringVariable_Command_EditCache);

#ifndef GW_ENABLEDPOPUP
#define GW_ENABLEDPOPUP 6
#endif

typedef struct {
	HWND hFoundWnd;
	const TCHAR *caption;
} SEARCHWINDOW;

PPXDLL WORD PPXAPI GetHistoryType(const TCHAR **param)
{
	const TCHAR *ptr;
	WORD historytype = 0;

	for ( ;; ){
		TCHAR code;

		code = **param;
		if (((UTCHAR)code < 'a') ||((ptr = tstrchr(HistType,code)) == NULL)){
			return historytype;
		}
		historytype |= HistWriteTypeflag[ptr - HistType];
		(*param)++;
	}
}

/*-----------------------------------------------------------------------------
	パラメータ入手関数の定義
-----------------------------------------------------------------------------*/
PPXDLL void PPXAPI PPxRegGetIInfo(PPXAPPINFO *ptr)
{
	PPxDefInfo = ptr;
}
#pragma argsused
DWORD_PTR USECDECL PPxDefInfoDummyFunc(PPXAPPINFO *ppxa,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	UnUsedParam(ppxa);
	if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
	return 0;
}
//-----------------------------------------------------------------------------
BOOL GetEditMode(const TCHAR **param,TINPUT_EDIT_OPTIONS *options)
{
	const TCHAR *p;
	WORD histt;
	WORD rhistflags = 0;

	options->flags = 0;
	switch ( **param ){ // HistType との衝突に注意
		case 'R':
			options->flags = TINPUT_EDIT_OPTIONS_use_refline;
			(*param)++;
			break;

		case 'O':
			options->flags = TINPUT_EDIT_OPTIONS_use_optionbutton;
			(*param)++;
			break;
	}

	p = tstrchr(HistType,**param);
	if ( (p == NULL) || (*p == '\0') ){
		XMessage(NULL,NULL,XM_GrERRld,T("option error"));
		return FALSE;
	}

	// ※ %eg の場合は、0 … 指定無しになる
	options->hist_writetype = (BYTE)( histt = (WORD)(p - HistType) );
	(*param)++;
	if ( **param == ',' ){
		for ( ;; ){
			(*param)++;
			p = tstrchr(HistType,**param);
			if ( (p == NULL) || (*p == '\0') ) break;
			setflag(rhistflags,HistWriteTypeflag[p - HistType]);
		}
	}else{
		rhistflags = HistReadTypeflag[histt];
	}
	options->hist_readflags = rhistflags;
	return TRUE;
}

void SetEditMode(EXECSTRUCT *Z)
{
	resetflag(Z->status,ST_EDITHIST);
	if ( IsTrue(GetEditMode(&Z->src,&Z->edit.options)) ){
		setflag(Z->status,ST_EDITHIST);
	}
}

void LineEscape(EXECSTRUCT *Z) // %b 行末エスケープ・文字挿入
{
	TCHAR c;

	c = *Z->src;
	switch ( c ){
		case '\r':
		case '\n':
			for (;;){
				*Z->dst++ = c;
				c = *(++Z->src);
				if ( (c != '\r') && (c != '\n') ) break;
			}
			break;

		case 'n':
			Z->src++;
			*Z->dst++ = '\r';
			*Z->dst++ = '\n';
			break;

		case 't':
			Z->src++;
			*Z->dst++ = '\t';
			break;

		case 'x':
			Z->src++;
			c = (TCHAR)GetHexNumber(&Z->src);
			if ( c != 0 ) *Z->dst++ = c;
			break;

		default:
			c = (TCHAR)GetNumber(&Z->src);
			if ( c != 0 ) *Z->dst++ = c;
			break;
	}
}

void USEFASTCALL SetTitleMacro(EXECSTRUCT *Z)
{
	TCHAR *maxp,*titlep;

	titlep = ThAllocString(&Z->StringVariable,Title_ValueName,VFPS);
	maxp = titlep + VFPS - 1;
	for (;;){
		UTCHAR c;

		c = *Z->src;
		if ( c < ' ' ) break;
		Z->src++;
		if ( c == '\"' ) break;
		if ( titlep < maxp ) *titlep++ = c;
	}
	*titlep = '\0';
}

TCHAR * USEFASTCALL GetZCurDir(EXECSTRUCT *Z)
{
	if ( Z->curdir[0] == '\0' ){
		if ( !PPxEnumInfoFunc(Z->Info,'1',Z->curdir,&Z->IInfo) ){
			GetCurrentDirectory(VFPS,Z->curdir);
		}
	}
	return Z->curdir;
}

DWORD GetFmacroOption(const TCHAR **string)
{
	DWORD flags = 0;
	const TCHAR *p;
	TCHAR c;

	p = *string;
	for ( ;; p++ ){
		c = *p;
		if ( (c < 'B') || (c > 'X') ) break;
		switch ( c ){
			case 'B': setflag(flags,FMOPT_BLANKET);		continue;
			case 'C': setflag(flags,FMOPT_FILENAME);	continue;
			case 'D': setflag(flags,FMOPT_DIR);			continue;
			case 'H': setflag(flags,FMOPT_DRIVE);		continue;
//			case 'I': setflag(flags,FMOPT_IDL);			continue;
			case 'M': setflag(flags,FMOPT_MARK);		continue;
			case 'N': setflag(flags,FMOPT_NOBLANKET);	continue;
			case 'P': setflag(flags,FMOPT_LASTSEPARATOR); continue;
			case 'R': setflag(flags,FMOPT_REALPATH);	continue;
			case 'S': setflag(flags,FMOPT_USESFN);		continue;
			case 'T': setflag(flags,FMOPT_FILEEXT);		continue;
			case 'U': setflag(flags,FMOPT_UNIQUE);		continue;
			case 'V': setflag(flags,FMOPT_ENABLEVFS);	continue;
			case 'X': setflag(flags,FMOPT_FILENAME | FMOPT_FILENOEXT); continue;
		}
		break;
	}
	*string = p;
	return flags;
}

void GetFmacroString(DWORD flag,TCHAR *src,TCHAR *dest)
{
	DWORD attr;

	if ( flag & FMOPT_USESFN ) GetShortPathName(src,src,VFPS);
	if ( flag & (FMOPT_FILEEXT | FMOPT_FILENOEXT) ){
		attr = GetFileAttributes(src); // dir により拡張子処理をするかどうか
	}
	if ( (flag & (FMOPT_FILENAME | FMOPT_DIR)) !=
			(FMOPT_FILENAME | FMOPT_DIR) ){ // フルパスでないときの処理

		if ( flag & (FMOPT_DRIVE | FMOPT_FILEEXT) ){

			if ( flag & FMOPT_FILEEXT ){	// 拡張子
				TCHAR *p;

				p = VFSFindLastEntry(src);
				if ( (attr == BADATTR) || !(attr & FILE_ATTRIBUTE_DIRECTORY) ||
						GetCustDword(T("XC_sdir"),0) ){
					if ( *p != '\0' ) p++;
					p = tstrrchr(p,'.');
					if ( p == NULL ){
						*src = '\0';
					}else{
						tstrcpy(src,p + 1);
					}
				}else{
					*src = '\0';
				}
			}else{							// ドライブ名
				TCHAR *p,*q,*r;
				int mode;

				p = VFSGetDriveType(src,&mode,NULL);
				if ( p != NULL ){
					if ( mode == VFSPT_DRIVE ){
						*p = '\0';
					}else if ( mode == VFSPT_UNC ){
						q = FindPathSeparator(p);
						if ( q != NULL ){
							r = FindPathSeparator(q + 1);
							if ( r != NULL ) *r = '\0';
						}
					}
				}
			}
		}else{ // ファイル名 or ディレクトリ
			TCHAR *xp,xpc;

			xp = VFSFindLastEntry(src);
			if ( xp != src ){
				if ( flag & FMOPT_DIR ){ // ディレクトリ
					*xp = '\0';
				}else{ // ファイル名
					xpc = *xp;
					tstrcpy(src,(xpc == '\\') || (xpc == '/') ? xp + 1 : xp);
				}
			}
		}
	}
	if ( flag & FMOPT_FILENOEXT ){
		if ( (attr == BADATTR) || !(attr & FILE_ATTRIBUTE_DIRECTORY) ||
				GetCustDword(T("XC_sdir"),0) ){
			TCHAR *lastentry;

			lastentry = VFSFindLastEntry(src);
			*(lastentry + FindExtSeparator(lastentry)) = '\0';
		}
	}
	if ( flag & FMOPT_LASTSEPARATOR ) CatPath(NULL,src,NilStr);

	if ( !(flag & FMOPT_NOBLANKET) &&
		 ((flag & FMOPT_BLANKET) || tstrchr(src,' ') || tstrchr(src,',')) ){
		wsprintf(dest,T("\"%s\""),src);
	}else{
		tstrcpy(dest,src);
	}
}

void GetPopupPoint(EXECSTRUCT *Z,POINT *pos)
{
	if ( Z->posptr == NULL ){
		if ( PPxInfoFunc(Z->Info,PPXCMDID_POPUPPOS,pos) ) return;
		if ( (Z->hWnd != NULL) && IsWindowVisible(Z->hWnd) ){
			pos->x = 0;
			pos->y = 0;
			ClientToScreen(Z->hWnd,pos);
		}else{
			GetCursorPos(pos);
		}
	}else{
		*pos = *Z->posptr;
	}
}

BOOL infoExtCheck(EXECSTRUCT *Z)
{
	TCHAR buf[64];

	if ( (PPxEnumInfoFunc(Z->Info,PPXCMDID_ENUMATTR,buf,&Z->IInfo) == 0) ||
		!(*(DWORD *)buf & FILE_ATTRIBUTE_DIRECTORY) ||
		GetCustDword(T("XC_sdir"),0) ){
		return TRUE; // 拡張子あり扱い
	}
	return FALSE;
}

void DefaultCmd(EXECSTRUCT *Z,DWORD cmdID,TCHAR *dest)
{
	TCHAR buf[VFPS];

	switch( cmdID ){
		case '1':
			GetCurrentDirectory(VFPS,dest);
			break;

		case '0': {	// 自分自身へのパス
			TCHAR *p;

			GetModuleFileName(NULL,dest,MAX_PATH);
			p = FindLastEntryPoint(dest);
			*p = '\0';
			break;
		}

//		case 'C':
		case 'R':
			PPxEnumInfoFunc(Z->Info,'C',dest,&Z->IInfo);
			break;

		case 'X':
			// 'Y' へ
		case 'Y': {
			TCHAR *p;

			PPxEnumInfoFunc(Z->Info,'C',dest,&Z->IInfo);
			if ( infoExtCheck(Z) ){
				p = VFSFindLastEntry(dest);
				if ( *p != '\0' ) p++;
				p = tstrrchr(p,'.');
				if ( p != NULL ) *p = '\0';
			}
			break;
		}

		case 'T':
			// 'T' へ
		case 't': {
			TCHAR *p;

			if ( !infoExtCheck(Z) ){
				dest[0] = '\0';
				break;
			}

			PPxEnumInfoFunc(Z->Info,'C',buf,&Z->IInfo);
			p = VFSFindLastEntry(buf);
			if ( *p != '\0' ) p++;
			p = tstrrchr(p,'.');
			if ( p == NULL ){
				dest[0] = '\0';
			}else{
				tstrcpy(dest,p + 1);
			}
			break;
		}
	}
	return;
}

// %F
void Get_F_MacroData(PPXAPPINFO *info,PPXCMD_F *fbuf,PPXCMDENUMSTRUCT *work)
{
	DWORD flags;
	const TCHAR *p;
	TCHAR buf[VFPS];

	p = fbuf->source;
	if ( PPxEnumInfoFunc(info,'F',(TCHAR *)fbuf,work) ) return;

	flags = GetFmacroOption(&p);
	fbuf->source = p;
	if ( !PPxEnumInfoFunc(info,'1',fbuf->dest,work) ){
		GetCurrentDirectory(VFPS,fbuf->dest);
	}
	PPxEnumInfoFunc(info,'C',buf,work);
	if ( buf[0] == '\0' ){
		if ( flags & (FMOPT_FILENAME | FMOPT_USESFN | FMOPT_FILEEXT) ){
			fbuf->dest[0] = '\0';
			return;
		}
		buf[0] = '?';
		buf[1] = '\0';
	}
	VFSFullPath(NULL,buf,fbuf->dest);
	GetFmacroString(flags,buf,fbuf->dest);
}

// %M[&][@][[?][:]name][,[!]shortcut]
// & : %s Menu_Index に内容を保存
// : : %M:M_pjump 等、マクロ文字展開無し
// @ : pjump用追加項目
// ? : 動的生成指定
// ! : メニュー表示無しで選択
BOOL USEFASTCALL MenuCmd(EXECSTRUCT *Z)
{
	TCHAR cid[MAX_PATH],*p;
	TCHAR param[CMDLINESIZE];
	TCHAR def[VFPS],buf[MAX_PATH],c;
	int flags = 0;

	if ( Z->flag & XEO_DISPONLY ){
		*Z->dst++ = '%';
		*Z->dst++ = *(Z->src - 1);
		return FALSE;
	}
	if ( *Z->src == '$' ){
		Z->src++;
		flags = MENUFLAG_NOEXTACT;
	}
	if ( *Z->src == '&' ){
		Z->src++;
		setflag(flags,MENUFLAG_SETINDEX);
	}else if ( flags == 0 ){
		Z->src--;
	}
	p = cid;
	while( (UTCHAR)(c = *Z->src) >= (UTCHAR)'0' ){ // " や ' や , を含めない
		*p++ = c;
		Z->src++;
	}
	*p = '\0';
	def[0] = '\0';
	if ( *Z->src == ',' ){
		Z->src++;
		if ( *Z->src == '!' ){
			Z->src++;
			setflag(flags,MENUFLAG_SELECT);
		}
		GetLineParam(&Z->src,def);
	}
	if ( cid[1] != 'E' ){				// メニュー ...........................
		if ( MenuCommand(Z,cid,def,flags) == FALSE ) return TRUE;
		if ( Z->result != NO_ERROR ) return TRUE;
	}else{								// 拡張子判別 .........................
		if ( def[0] == '\0' ){
			PPxEnumInfoFunc(Z->Info,'C',buf,&Z->IInfo);
			if ( buf[0] == '\0' ){
				Z->result = ERROR_NO_MORE_ITEMS;
				return TRUE;
			}
			CatPath(def,GetZCurDir(Z),buf);
		}

		if ( (*(cid + 2) != ':') ?
				(PP_GetExtCommand(def,cid + 1,param,NULL) == PPEXTRESULT_FILE)
			  : (NO_ERROR == GetCustTable(cid + 3,def,param,TSTROFF(CMDLINESIZE)))
		){
			const TCHAR *newsrc;

			newsrc = param;
			if ( (UTCHAR)*newsrc == EXTCMD_CMD ){
				newsrc++;
			}else{
				if ( (UTCHAR)*newsrc == EXTCMD_KEY ) newsrc++;
				while( *((WORD *)newsrc) ){
					ERRORCODE result;

					result = PPxInfoFunc(Z->Info,PPXCMDID_PPXCOMMAD,(void *)newsrc);
					if ( result > 1 ){ // NO_ERROR,ERROR_INVALID_FUNCTION 以外はエラー
						if ( (result == ERROR_CANCELLED) || !(Z->flag & XEO_IGNOREERR) ){
							break;
						}
					}
					newsrc += sizeof(WORD) / sizeof(TCHAR);
				}
			}
			BackupSrc(Z,newsrc);
		}else{
			Z->result = ERROR_NO_MORE_ITEMS;
		}
		return TRUE;
	}
	return FALSE;
}

void ExtractPPxCall(HWND hTargetWnd,EXECSTRUCT *Z,const TCHAR *macroparam)
{
	DWORD pid;
	HANDLE hMap,hSendMap,hProcess;
	TCHAR *param;

	hMap = CreateFileMapping(INVALID_HANDLE_VALUE,
				NULL,PAGE_READWRITE,0,TSTROFF(CMDLINESIZE),NULL);
	if ( hMap == NULL ){
		PPErrorBox(NULL,NULL,PPERROR_GETLASTERROR);
		return;
	}
	param = MapViewOfFile(hMap,FILE_MAP_ALL_ACCESS,0,0,TSTROFF(CMDLINESIZE));
	if ( param == NULL ){
		CloseHandle(hMap);
		PPErrorBox(NULL,NULL,PPERROR_GETLASTERROR);
		return;
	}
	GetWindowThreadProcessId(hTargetWnd,&pid);
	hProcess = OpenProcess(PROCESS_DUP_HANDLE,FALSE,pid);
	DuplicateHandle(GetCurrentProcess(),hMap,
				hProcess,&hSendMap,0,FALSE,DUPLICATE_SAME_ACCESS);
	tstrcpy(param,macroparam);
	SendMessage(hTargetWnd,WM_PPXCOMMAND,K_EXTRACT,(LPARAM)hSendMap);
	CloseHandle(hSendMap);

	tstrcpy(Z->dst,param);
	Z->dst += tstrlen(Z->dst);
	UnmapViewOfFile(param);
	CloseHandle(hMap);
	return;
}

void GetValue(EXECSTRUCT *Z,DWORD cmdID,TCHAR *dest)
{
	if ( PPxEnumInfoFunc(Z->Info,cmdID,dest,&Z->IInfo) ) return;
	DefaultCmd(Z,cmdID,dest);
}

void ZGetName(EXECSTRUCT *Z,TCHAR *dest,TCHAR cmd)
{
	TCHAR buf[VFPS + sizeof(TCHAR) * 2];

	if ( cmd == '\0' ) cmd = *(Z->src - 1);
	switch ( cmd ){
		case 'F': { // %F
			((PPXCMD_F *)buf)->source = Z->src;
			((PPXCMD_F *)buf)->dest[0] = '\0';

			Get_F_MacroData(Z->Info,(PPXCMD_F *)buf,&Z->IInfo);

			if ( Z->src < ((PPXCMD_F *)buf)->source ){
				while( Z->src < ((PPXCMD_F *)buf)->source ){
					if ( (*Z->src == 'C') || (*Z->src == 'X') ){
						setflag(Z->flag,XEO_EXECMARK);
					}
					Z->src++;
				}
			}else{
				while( Isalpha(*Z->src) ){
					if ( (*Z->src == 'C') || (*Z->src == 'X') ){
						setflag(Z->flag,XEO_EXECMARK);
					}
					Z->src++;
				}
			}
			tstrcpy(dest,((PPXCMD_F *)buf)->dest);
			break;
		}
		case 'C': // %C
		case 'X': // %X
			setflag(Z->status,ST_PATHFIX);
		// %T へ
		case 'T': // %T
								// カレントのファイルの属性を入手
			GetValue(Z,cmd,dest);
			if ( PPxEnumInfoFunc(Z->Info,PPXCMDID_ENUMATTR,buf,&Z->IInfo) &&
					(*(DWORD *)buf & FILE_ATTRIBUTE_DIRECTORY) ){
				if ( (Z->flag & XEO_DIRWILD) && ( cmd == 'C' ) ){
					CatPath(NULL,dest,T("*.*"));
				}
				setflag(Z->status,ST_SDIRREF | ST_CHKSDIRREF);
			}else{
				setflag(Z->status,ST_CHKSDIRREF);
			}
			setflag(Z->flag,XEO_EXECMARK);
			break;

		default: // その他は未定義扱い
			*dest = '\0';
	}
}

void USEFASTCALL convertslash(EXECSTRUCT *Z,TCHAR *path)
{
	if ( !(Z->flag & (XEO_PATHSLASH | XEO_PATHESCAPE)) ) return;

	while ( *path ){
		if ( Ismulti(*path) ){
			path++;
			if ( *path == '\0' ) break;
		}else{
			switch ( *path ){
				case '\\':
					*path = '/';
					break;

				case '[':
				case ']':
					if ( !(Z->flag & XEO_PATHESCAPE) ) break;
					memmove(path + 1,path,TSTRSIZE(path));
					*path++ = '\\';
					break;
			}
		}
		path++;
	}
}

BOOL CreateResponseFile(EXECSTRUCT *Z)
{
	TCHAR atr[16],*p,buf[VFPS + 4]; // +4 は、「""」 + α分
	const TCHAR *oldsrc = NULL; // 書式指定
	HANDLE hFile;
	DWORD size;
	BOOL addall = TRUE;
	BOOL writebom = FALSE;
	#ifdef UNICODE
	char bufA[VFPS * 3 + 4];
	#endif
	UINT codepage = CP_ACP;

	if ( Z->flag & XEO_DISPONLY ){		// 表示のみ
		tstrcpy(Z->dst,T("ResFile"));
		Z->dst += 7;
		return TRUE;
	}

	if ( *Z->src == '*' ){
		Z->src++;
		addall = FALSE;
	}
	if ( *Z->src == '8' ){
		Z->src++;
		codepage = CP_UTF8;
	}
	if ( *Z->src == 'U' ){
		Z->src++;
		codepage = CP_PPX_UCF2;
	}
	if ( *Z->src == 'B' ){
		Z->src++;
		writebom = TRUE;
	}

	if ( ThGetString(&Z->StringVariable,ResponseName_ValueName,Z->dst,VFPS) != NULL ){ // 実行(作成済み)
		Z->dst += tstrlen(Z->dst);
		while ( Isalpha(*Z->src) ) Z->src++; // 書式設定をスキップ
		return TRUE;
	}

	#ifdef UNICODE
	if ( (Z->command == 'u') && (codepage == CP_ACP) ){ // %u の場合は、文字コード補正が必要
		UN_DLL *uD;
		int i;

		p = Z->DstBuf;
		GetCommandParameter((const TCHAR **)&p,buf,TSIZEOF(buf));

		uD = undll_list;
		for ( i = 0 ; i < undll_items ; i++,uD++ ){
			if ( tstricmp(uD->DllName,buf) ) continue;

			if ( uD->hadd == NULL ){
				if ( LoadUnDLL(uD) == FALSE ) break;
			}

			if ( uD->UnarcW != NULL ){
				if ( uD->flags & UNDLLFLAG_RESPONSE_UTF8 ){
					codepage = CP_UTF8;
				}else{
					codepage = CP_PPX_UCF2;
				}
			}else if ( uD->SetUnicodeMode(TRUE) ){
				uD->SetUnicodeMode(FALSE);
				codepage = CP_UTF8;
			}
			if ( codepage != CP_ACP ){
				p = tstrchr(Z->DstBuf,',');
				if ( p != NULL ){
					p++;
					memmove(p + 2,p,TSTRSIZE(p));
					Z->dst += 2;
					p[0] = '!';
					p[1] = (TCHAR)((codepage == CP_PPX_UCF2) ? '2' : '8');
				}
			}
			break;
		}
	}
	#endif

	if ( Isalpha(*Z->src) ) oldsrc = Z->src + 1;

										// 実行(新規作成)
	MakeTempEntry(MAX_PATH,Z->dst,FILE_ATTRIBUTE_NORMAL);
	hFile = CreateFile(Z->dst,GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,CREATE_ALWAYS,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
	if ( hFile == INVALID_HANDLE_VALUE ){
		Z->result = GetLastError();
		return FALSE;
	}
	PPxInfoFunc(Z->Info,PPXCMDID_GETTMPFILENAME,Z->dst);
	ThSetString(&Z->StringVariable,ResponseName_ValueName,Z->dst);
	if ( tstrchr(Z->dst,' ') == NULL ){
		Z->dst += tstrlen(Z->dst);
	}else{
		Z->dst += wsprintf(Z->dst,T("\"%s\""),ThGetString(&Z->StringVariable,ResponseName_ValueName,NULL,0));
	}
	buf[0] = '\"';

	setflag(Z->status,ST_CHKSDIRREF);

	if ( writebom ){
		if ( codepage == CP_PPX_UCF2 ){
			WriteFile(hFile,UCF2HEADER,UCF2HEADERSIZE,&size,NULL);
		}else{
			WriteFile(hFile,UTF8HEADER,UTF8HEADERSIZE,&size,NULL);
		}
	}

	for ( ; ; ){
		if ( oldsrc != NULL ){ // 書式指定有り
			Z->src = oldsrc;
			ZGetName(Z,buf,'\0');
			// 蛇足コードだった
//			if ( !buf[0] || ((buf[0] == '\"') && (buf[1] == '\"')) ){ // 空欄
//				break;
//			}
			convertslash(Z,buf);
			p = buf;
		}else{ // 書式指定無し…%C
			GetValue(Z,'C',buf + 1);
			if ( buf[1] == '\0' ) break;
			convertslash(Z,buf + 1);
							// 空白あり なら「"」で括る
			if ( tstrchr(buf + 1,' ') ){
				p = buf;
				tstrcat(p,T("\""));
			}else{
				p = buf + 1;
			}
		}
		if ( PPxEnumInfoFunc(Z->Info,PPXCMDID_ENUMATTR,atr,&Z->IInfo) &&
								(*(DWORD *)atr & FILE_ATTRIBUTE_DIRECTORY) ){
			if ( addall ){
				if ( *p == '\"' ){
					*(p + tstrlen(p) - 1) = '\0';
					CatPath(NULL,p,T("*\""));
				}else{
					CatPath(NULL,p,WildCard_All);
				}
			}
			setflag(Z->status,ST_SDIRREF);
		}
		#ifdef UNICODE
			if ( codepage != CP_PPX_UCF2 ){
				if ( 0 == WideCharToMultiByteU8(codepage,0,p,-1,bufA,sizeof(bufA),NULL,NULL) ){
					strcpy(bufA,"<long>");
				}
				if ( WriteFile(hFile,bufA,strlen32(bufA),&size,NULL) == FALSE ) break;
				if ( WriteFile(hFile,"\r\n",2,&size,NULL) == FALSE ) break;
			}else{
				if ( WriteFile(hFile,p,TSTRLENGTH32(p),&size,NULL) == FALSE ) break;
				if ( WriteFile(hFile,L"\r\n",4,&size,NULL) == FALSE ) break;
			}
		#else
			if ( codepage == CP_UTF8 ){
				WCHAR bufW[VFPS];

				AnsiToUnicode(p,bufW,TSIZEOF(bufW));
				if ( 0 == WideCharToMultiByteU8(codepage,0,bufW,-1,buf + 1,sizeof(buf) - 2,NULL,NULL) ){
					strcpy(buf + 1,"<long>");
				}
				p = buf + 1;
			}
			if ( WriteFile(hFile,p,strlen(p),&size,NULL) == FALSE ) break;
			if ( WriteFile(hFile,"\r\n",2,&size,NULL) == FALSE ) break;
		#endif
		if ( PPxEnumInfoFunc(Z->Info,PPXCMDID_NEXTENUM,buf + 1,&Z->IInfo)== 0){
			break;
		}
	}
	CloseHandle(hFile);
	return TRUE;
}


const TCHAR *ZGetTitleName(EXECSTRUCT *Z)
{
	const TCHAR *title;

	title = ThGetString(&Z->StringVariable,Title_ValueName,NULL,0);
	if ( title == NULL ) title = MessageText(ExecuteDefaultTitle);
	return title;
}


BOOL ZTinput(EXECSTRUCT *Z,TINPUT *tinput)
{
	int inputtype;
	WORD rhisttype;

	if ( !(tinput->flag & B31) ){
		if ( Z->status & ST_EDITHIST ){ // ※ %eg の場合は 0…指定無しになる
			inputtype = Z->edit.options.hist_writetype;
			rhisttype = Z->edit.options.hist_readflags;

			if ( Z->edit.options.flags != 0 ){
				if ( Z->edit.options.flags & TINPUT_EDIT_OPTIONS_use_optionbutton ){
					setflag(tinput->flag,TIEX_USEOPTBTN);
				}else{
					setflag(tinput->flag,TIEX_USEREFLINE);
				}
			}
		}else if ( Z->status & ST_PATHFIX ){
			inputtype = HistType_PATHFIX;
			rhisttype = PPXH_NAME_R;
		}else{
			inputtype = HistType_GENERAL;
			rhisttype = PPXH_GENERAL | PPXH_COMMAND | PPXH_PATH;
		}
		tinput->hRtype = rhisttype;
		tinput->hWtype = HistWriteTypeflag[inputtype];
		setflag(tinput->flag,TinputTypeflags[inputtype]);
	}
	tinput->hOwnerWnd = Z->hWnd;
	tinput->info = Z->Info;

	if ( tInputEx(tinput) <= 0 ){
		Z->result = ERROR_CANCELLED;
		return FALSE;
	}
	return TRUE;
}

#if 0
HWND GetWindowAloneOnCombo(const TCHAR *ID)
{
	HWND hTargetWnd = Sm->ppc.hComboWnd[ID[2] - 'A'];
	HWND hWnd;

	if ( (hTargetWnd != NULL) && (hTargetWnd != BADHWND) ){
	DWORD pid;
	HANDLE hMap,hSendMap,hProcess;
	TCHAR *param;
	const TCHAR *np;

	hMap = CreateFileMapping(INVALID_HANDLE_VALUE,
				NULL,PAGE_READWRITE,0,TSTROFF(CMDLINESIZE),NULL);
	if ( hMap == NULL ) return BADHWND;
	param = MapViewOfFile(hMap,FILE_MAP_ALL_ACCESS,0,0,TSTROFF(CMDLINESIZE));
	if ( param == NULL ){
		CloseHandle(hMap);
		return BADHWND;
	}
	GetWindowThreadProcessId(hTargetWnd,&pid);
	hProcess = OpenProcess(PROCESS_DUP_HANDLE,FALSE,pid);
	DuplicateHandle(GetCurrentProcess(),hMap,
				hProcess,&hSendMap,0,FALSE,DUPLICATE_SAME_ACCESS);

	wsprintf ( param, T("%%N%s"), ID );
	SendMessage(hTargetWnd,WM_PPXCOMMAND,K_EXTRACT,(LPARAM)hSendMap);
	CloseHandle(hSendMap);

	np = param;
	hWnd = (HWND)GetDigitNumber(&np);

	UnmapViewOfFile(param);
	CloseHandle(hMap);

		if ( hWnd == NULL ) hWnd = BADHWND;
		return hWnd;
	}

	return BADHWND; // 該当なし
}
#endif

HWND GetPPcPairWindow(PPXAPPINFO *ppxa)
{
	HWND hWnd;
	int i;

	hWnd = (HWND)PPxInfoFunc(ppxa,PPXCMDID_PAIRWINDOW,&i);
	if ( hWnd != NULL ) return hWnd;

	// 見つからない…アクティブ PPc を探す
	UsePPx();
	i = Sm->ppc.LastFocusID;
	if ( CheckPPcID(i) == FALSE ){
		for ( i = 0 ; i < X_Mtask ; i++ ){
			if ( IsTrue(CheckPPcID(i)) ) break;
		}
	}
	if ( i < X_Mtask ) hWnd = Sm->P[i].hWnd;
	FreePPx(); // 一旦、専有を解除する
	if ( i >= X_Mtask ) return NULL;
	return (HWND)SendMessage(hWnd,WM_PPXCOMMAND,KC_GETSITEHWND,(LPARAM)KC_GETSITEHWND_PAIR);
}

// BADHWND ... 該当ウィンドウがない
// NULL ... ID指定がされていない
PPXDLL HWND PPXAPI GetPPxhWndFromID(PPXAPPINFO *ppxa, const TCHAR **src, TCHAR *path)
{
	int i; // 0～X_Mtask 未満なら有効な値
	TCHAR *bufp, buf[REGEXTIDSIZE], code;
	int combosite = -2, offset;
	HWND hComboWnd;

	code = SkipSpace(src);
	if ( code == '~' ){ // 反対窓
		HWND hPairWnd;

		(*src)++;
		hPairWnd = GetPPcPairWindow(ppxa);
		if ( hPairWnd == NULL ) return BADHWND;
		return hPairWnd;
	}

	if ( !Isalpha(code) ){
		if ( code == '-' ){ // PPv の呼び出し元 PPc
			HWND hWnd;

			(*src)++;
			hWnd = (HWND)PPxInfoFunc(ppxa, PPXCMDID_GETREQHWND, NULL);
			if ( hWnd != NULL ){
				while ( Isalpha(**src) || (**src == '_') ) (*src)++;
				return hWnd;
			}
			if ( !Isalpha(**src) ) return BADHWND; // 指定なし
		}else{
			return NULL; // 指定なし
		}
	}
	bufp = buf;
	for ( offset = 0 ; offset < (REGEXTIDSIZE - 1) ; offset++ ){ //ID部分を抽出
		if ( !Isalpha(**src) && (**src != '_') ) break;
		*bufp++ = upper(*((*src)++));
	}
	*bufp = '\0';
	if ( offset >= REGIDSIZE ){ // C_Zxyy / CZxyy 形式？
		HWND hWnd;

		if ( buf[1] == '_' ){
			buf[1] = buf[0];
			bufp = buf + 1;
		}else{
			bufp = buf;
		}
		hWnd = Sm->ppc.hComboWnd[TinyCharUpper(buf[2]) - 'A'];
		if ( hWnd == hProcessComboWnd ){
			hWnd = (HWND)PPxInfoFunc(ppxa, PPXCMDID_COMBOGETHWNDEX, bufp);
		}else{
			LPARAM lParam;

			if ( (hWnd == NULL) || (hWnd == BADHWND) ) return BADHWND;
			lParam = (DWORD)buf[3] | ((DWORD)buf[4] << 8) | ((DWORD)buf[5] << 16) | ((DWORD)buf[5] << 24);
			hWnd = (HWND)SendMessage(hWnd, WM_PPXCOMMAND, KCW_GetIDWnd, lParam);
		}
		if ( hWnd == NULL ) return BADHWND;
		return hWnd;
	}

	if ( **src == '#' ){ // 一体化指定
		TCHAR c;

		if ( TinyCharUpper(buf[0]) != 'C' ) return BADHWND;

		if ( (TinyCharUpper(buf[1]) == 'B') && Isalpha(buf[2]) ){ // 任意のcombo
			hComboWnd = Sm->ppc.hComboWnd[TinyCharUpper(buf[2]) - 'A'];
		}else{ // default
			hComboWnd = hProcessComboWnd;
			if ( hComboWnd == NULL ) hComboWnd = Sm->ppc.hComboWnd[0];
		}
		if ( (hComboWnd == NULL) || (hComboWnd == BADHWND) ) return BADHWND;

		(*src)++;
		c = upper(**src);
		if ( c == 'C' ){ // 現在窓
			(*src)++;
			combosite = KC_GETSITEHWND_CURRENT;
		}else if ( c == 'L' ){ // 左側
			(*src)++;
			combosite = KC_GETSITEHWND_LEFT;
		}else if ( c == 'R' ){ // 右側
			(*src)++;
			combosite = KC_GETSITEHWND_RIGHT;
		}else if ( c == '~' ){ // 反対窓
			(*src)++;
			combosite = KC_GETSITEHWND_PAIR;
		}else if ( Isdigit(c) ){ // 左からのペイン位置
			combosite = KC_GETSITEHWND_LEFTENUM + GetDigitNumber(src);
		}else{
			combosite = KC_GETSITEHWND_BASEWND;
		}
	}

	if ( Isalpha(**src) ){ // 余分な文字
		while ( Isalpha(**src) ) (*src)++;
		return BADHWND;
	}

	if ( combosite != -2 ){ // 一体化窓関連取得
		HWND hWnd;

		hWnd = (HWND)SendMessage(hComboWnd, WM_PPXCOMMAND, KC_GETSITEHWND, (LPARAM)combosite);
		if ( hWnd == NULL ) return BADHWND;
		if ( path == NULL ) return hWnd;

		UsePPx();
		if ( hWnd == NULL ){
			i = -1;
		}else{
			for ( i = 0 ; i < X_Mtask ; i++ ){
				if ( Sm->P[i].hWnd == hWnd ) break;
			}
		}
	}else{ // 通常窓取得
		if ( buf[0] == '\0' ) return NULL;
		UsePPx();
		if ( (buf[0] == 'C') && (buf[1] == '\0') ){ // 1文字指定
			HWND hFocusWnd = Sm->ppc.hLastFocusWnd;

			if ( (hFocusWnd != NULL) && IsWindow(hFocusWnd) ){
				FreePPx();
				return hFocusWnd;
			}

			i = Sm->ppc.LastFocusID;
			if ( CheckPPcID(i) == FALSE ){
				for ( i = 0 ; i < X_Mtask ; i++ ){
					if ( IsTrue(CheckPPcID(i)) ) break;
				}
			}
		}else{
			i = SearchPPx(buf);
			if ( i < 0 ){
				buf[3] = '\0';
				buf[2] = buf[1];
				buf[1] = '_';
				i = SearchPPx(buf);
			}
		}
	}

	if ( (i >= 0) && (i < X_Mtask) ){
		HWND hWnd = Sm->P[i].hWnd;

		if ( path != NULL ) tstrcpy(path, Sm->P[i].path);
		FreePPx();
		return hWnd;
	}
	FreePPx();
	return BADHWND;
}

PPXDLL void PPXAPI ForceSetForegroundWindow(HWND hWnd)
{
	DWORD_PTR sendresult;
	DWORD inactiveTID,activeTID;

	inactiveTID	= GetCurrentThreadId();
	activeTID	= GetWindowThreadProcessId(GetForegroundWindow(),NULL);
	AttachThreadInput(inactiveTID,activeTID,TRUE);
	if ( WinType >= WINTYPE_2000 ){ // ダイアログがあれば、そちらを対象に
		if ( IsWindowEnabled(hWnd) == FALSE ){ // 子ウィンドウがないと、別のウィンドウを示すので、対策
			hWnd = GetWindow(hWnd,GW_ENABLEDPOPUP);
		}
	}
	if ( IsIconic(hWnd) || !IsWindowVisible(hWnd) ){
		SendMessageTimeout(hWnd,WM_SYSCOMMAND,SC_RESTORE,0xffff0000,
				SMTO_ABORTIFHUNG,300,&sendresult);
	}
	SetForegroundWindow(hWnd);
	AttachThreadInput(inactiveTID,activeTID,FALSE);
}

void USEFASTCALL ZGetPPvCursorPos(EXECSTRUCT *Z)
{
	DWORD cmdid = *(Z->src - 1);

	switch ( *Z->src ){
		case 'H': // %lH %LH
			cmdid += 0x100;
			// 'V'へ
		case 'V': // %lV %LV
			Z->src++;
			break;
		//default:
	}
	PPxEnumInfoFunc(Z->Info,cmdid,Z->dst,&Z->IInfo);
	Z->dst += tstrlen(Z->dst);
}

void USEFASTCALL ZGetWndCaptionMacro(EXECSTRUCT *Z)
{
	HWND nhWnd;

	nhWnd = GetCaptionWindow(Z->hWnd);
	*Z->dst = '\0';
	SendMessage(nhWnd, WM_GETTEXT, VFPS, (LPARAM)Z->dst);
	Z->dst += tstrlen(Z->dst);
}

// 特殊環境変数
void ZStringVariable(EXECSTRUCT *Z, const TCHAR **src, int mode)
{
	TCHAR name[CMDLINESIZE],*str;
	ThSTRUCT *UseTH = &ProcessStringValue;

	for (;;){
		switch ( *(*src)++ ){
			case 'g': // 展開処理
				mode = StringVariable_extract;
				continue;

			case 'p': // プロセス内
				break;

			case 'i': {// ID内
				ThSTRUCT *THt = (ThSTRUCT *)PPxInfoFunc(Z->Info, PPXCMDID_GETWNDVARIABLESTRUCT, NULL);
				if ( THt != NULL ) UseTH = THt;
				break;
			}

//			case 'e': // セル内
//				break;

			case 'o': // コマンドライン内
				UseTH = &Z->StringVariable;
				break;

			default: { // デフォルト
				ThSTRUCT *THt = (ThSTRUCT *)PPxInfoFunc(Z->Info, PPXCMDID_GETTEMPVARIABLESTRUCT, NULL);
				if ( THt != NULL ){
					UseTH = THt;
				}else{
					THt = (ThSTRUCT *)PPxInfoFunc(Z->Info, PPXCMDID_GETWNDVARIABLESTRUCT, NULL);
					if ( THt != NULL ) UseTH = THt;
				}
				(*src)--;
				break;
			}

		}
		break;
	}
	if ( *(*src) == ',' ) (*src)++;
	if ( SkipSpace(src) == '\0' ){
		HMENU hPopupMenu;
		int index = 0;

		hPopupMenu = CreatePopupMenu();
		for (;;){
			if ( FALSE == ThEnumString(UseTH, index++, Z->dst, name, TSIZEOF(name)) ){
				break;
			}
			wsprintf(Z->dst + tstrlen(Z->dst), T("=%s"), name);
			AppendMenuString(hPopupMenu, index + 1, Z->dst);
		}
		*Z->dst = '\0';
		index = TTrackPopupMenu(Z, hPopupMenu, NULL);
		if ( index ){
			GetMenuString(hPopupMenu, index + 1, Z->dst, CMDLINESIZE, MF_BYCOMMAND);
			Z->dst += tstrlen(Z->dst);
		}
		DestroyMenu(hPopupMenu);
		return;
	}
	if ( mode == StringVariable_command ){
		tstrcpy(name,*src);
	}else{
		if ( **src != '\"' ){
			GetCommandParameter(src, name, TSIZEOF(name));
		}else{
			TCHAR *dest = name;

			(*src)++;
			while ( (UTCHAR)**src >= ' ' ){
				if ( **src == '\"' ){
					(*src)++;
					break;
				}
				*dest++ = *(*src)++;
			}
			*dest = '\0';
		}
	}
	str = tstrchr(name,'=');
	if ( str == NULL ){
		*Z->dst = '\0';
		ThGetString(UseTH, name, Z->dst, CMDLINESIZE - 1);
		if ( mode == StringVariable_extract ){
			Z->result = PP_ExtractMacro(Z->hWnd, Z->Info, NULL, Z->dst, Z->dst, 0);
		}
		Z->dst += tstrlen(Z->dst);
	}else{
		*str++ = '\0';
		ThSetString(UseTH, name, str);
	}
}

BOOL CALLBACK SearchWindowProc(HWND hWnd,LPARAM lParam)
{
	TCHAR caption[CMDLINESIZE];

	if ( GetWindowText(hWnd,caption,TSIZEOF(caption) - 1) == 0 ) return TRUE;
	if ( tstrstr(caption,((SEARCHWINDOW *)lParam)->caption) != NULL ){
		((SEARCHWINDOW *)lParam)->hFoundWnd = hWnd;
		return FALSE;
	}
	return TRUE;
}

PPXDLL HWND PPXAPI GetWindowHandleByText(PPXAPPINFO *ppxa,const TCHAR *param)
{
	HWND hTargetWnd;
	SEARCHWINDOW sw;
	const TCHAR *ptr;
									// PPx 指定あり？
	ptr = param;
	hTargetWnd = GetPPxhWndFromID(ppxa,&ptr,NULL);
	if ( (hTargetWnd != NULL) && (hTargetWnd != BADHWND) ){
		return hTargetWnd;
	}
	if ( *param == '#' ){
		ptr = param + 1;
		hTargetWnd = (HWND)GetDigitNumber(&ptr);
		if ( (hTargetWnd != NULL) && (*ptr == '\0') ) return hTargetWnd;
	}
									// ウィンドウサーチ
	sw.hFoundWnd = NULL;
	sw.caption = param;
	EnumWindows(SearchWindowProc,(LPARAM)&sw);
	return sw.hFoundWnd;
}

// オプションを取得 -----------------------------------------------------------
void ZReadOption(EXECSTRUCT *Z)
{
	TCHAR c;
	int i;

	while ((c = *Z->src) != '\0'){
		Z->src++;
		if (c == ',') break;
		for (i = 0 ; XEO_OptionString[i] ; i++ ){
			if ( c == XEO_OptionString[i] ){
				c = *Z->src;
				switch(c){
					case '-':
						Z->src++;
						resetflag(Z->flag,1 << i);
						break;
					case '+':
						Z->src++;
					default:
						setflag(Z->flag,1 << i);
						break;
				}
				break;
			}
		}
		if ( XEO_OptionString[i] == '\0' ){
			Z->src--;
			break;
		}
	}
}

void BackupSrc(EXECSTRUCT *Z,const TCHAR *newsrc)
{
	OLDSRCSTRUCT *next;

	SkipSpace(&newsrc);
	if ( *newsrc == '\0' ) return;

	next = HeapAlloc(DLLheap,0,sizeof(OLDSRCSTRUCT) + TSTRSIZE(newsrc));
	if ( next == NULL ) return;
	next->src = Z->src;
	next->backptr = Z->oldsrc;
	Z->oldsrc = next;
	Z->src = (TCHAR *)&next[1];
	tstrcpy((TCHAR *)&next[1],newsrc);
}

void Freeoldsrc(EXECSTRUCT *Z)
{
	while ( Z->oldsrc != NULL ){
		OLDSRCSTRUCT *back;

		back = Z->oldsrc;
		Z->oldsrc = back->backptr;
		HeapFree(DLLheap,0,back);
	}
}

void LoadHistory(EXECSTRUCT *Z,WORD historytype)
{
	const TCHAR *ptr;
	WORD tmphistorytype;

	tmphistorytype = GetHistoryType(&Z->src);
	if ( tmphistorytype != 0 ) historytype = tmphistorytype;

	UsePPx();
	ptr = EnumHistory(historytype,GetDigitNumber(&Z->src));
	if ( ptr != NULL ){
		tstrcpy(Z->dst,ptr);
		Z->dst += tstrlen(Z->dst);
	}
	FreePPx();
	return;
}

void ZGetMessageText(EXECSTRUCT *Z)
{
	const TCHAR *srcp;
	TCHAR *dstp,*maxdst;

	if ( *Z->src == '\"' ) Z->src++;

	srcp = MessageText(Z->src);
	dstp = Z->dst;
	maxdst = Z->DstBuf + CMDLINESIZE - 1;

	if ( (srcp == Z->src) || (srcp == (Z->src + MESTEXTIDLEN + 1)) ){
		for (;;){
			TCHAR c;

			c = *srcp;
			if ( c == '\0' ) break;
			srcp++;
			if ( c == '\"' ) break;
			if ( dstp >= maxdst ) break;
			*dstp++ = c;
		}
		Z->src = srcp;
	}else{
		for (;;){
			TCHAR c;

			c = *srcp;
			if ( c == '\0' ) break;
			srcp++;
			if ( dstp >= maxdst ) break;
			*dstp++ = c;
		}
		for (;;){
			TCHAR c;

			c = *Z->src;
			if ( c == '\0' ) break;
			Z->src++;
			if ( c == '\"' ) break;
		}
	}

	*dstp = '\0';
	Z->dst = dstp;
}

void ZExpandAliasWithExtract(EXECSTRUCT *Z)
{
	TCHAR *p;
	DWORD size;

	p = Z->dst;
	if ( *Z->src == '\'' ) Z->src++;
	while ( ((UTCHAR)*Z->src >= ' ') && (*Z->src != '\'') ) *p++ = *Z->src++;
	if ( *Z->src == '\'' ) Z->src++;
	*p = '\0';

	size = CMDLINESIZE - (Z->dst - Z->DstBuf);
	GetCustTable(T("A_exec"),Z->dst,Z->dst,TSTROFF(size));
	Z->result = PP_ExtractMacro(Z->hWnd,Z->Info,NULL,Z->dst,Z->dst,0);
	Z->dst += tstrlen(Z->dst);
}

void ZExpandAlias(EXECSTRUCT *Z)
{
	TCHAR buf[CMDLINESIZE],*p;
	DWORD size;

	p = buf;
	while ( ((UTCHAR)*Z->src >= ' ') && (*Z->src != '\'') ) *p++ = *Z->src++;
	if ( *Z->src == '\'' ) Z->src++;
	*p = '\0';

	size = CMDLINESIZE - (Z->dst - Z->DstBuf);
	if ( NO_ERROR != GetCustTable(T("A_exec"),buf,Z->dst,TSTROFF(size)) ){
		GetEnvironmentVariable(buf,Z->dst,size);
	}
	Z->dst += tstrlen(Z->dst);
}

void GetPPxHWND(EXECSTRUCT *Z)
{
	HWND hWnd;

	if ( *Z->src == '.' ){
		Z->src++;
		hWnd = GetParent(Z->hWnd);
	}else{
		hWnd = GetPPxhWndFromID(Z->Info,&Z->src,NULL);
		if ( hWnd == BADHWND ) return; // 該当無し
		if ( hWnd == NULL ) hWnd = Z->hWnd; // ID指定無し
	}
	if ( hWnd != NULL ) Z->dst += wsprintf(Z->dst,T("%u"),hWnd);
	return;
}

void GetPPxID(EXECSTRUCT *Z)
{
	int len;

	if ( *Z->src != '#' ){ // 各窓のID
		// 一体化窓の CZabc 形式
		if ( (Z->Info->RegID[0] == 'C') && (Z->Info->RegID[2] == 'Z') ){
			*Z->dst = '\0';
			PPxInfoFunc(Z->Info,PPXCMDID_GETSUBID,Z->dst);
			if ( Z->dst[0] != '\0' ){
				Z->dst += tstrlen(Z->dst);
				return;
			}
		}
		// C_A → CA
		len = wsprintf(Z->dst,T("%s"),Z->Info->RegID);
		if ( Z->dst[1] == '_' ){
			Z->dst[1] = Z->dst[2];
			len--;
		}
	}else{ // 一体化窓
		Z->src++;
		if ( PPxInfoFunc(Z->Info,PPXCMDID_COMBOIDNAME,Z->dst) == 0 ) return;
		len = tstrlen32(Z->dst);
	}

	Z->dst += len;
	*Z->dst = '\0';
}

void GetPPxPath(EXECSTRUCT *Z)
{
	HWND hWnd;

	*Z->dst = '\0';
	hWnd = GetPPxhWndFromID(Z->Info,&Z->src,Z->dst); // Z->dst に path が保存

	if ( hWnd == NULL ){ // デフォルト指定
		tstrcpy(Z->dst,GetZCurDir(Z));
	}else if ( hWnd == BADHWND ){ // 該当無し
		return;
	}else if ( *Z->dst == '\0' ){
		ExtractPPxCall(hWnd,Z,T("%1"));
	}
	Z->dst += tstrlen(Z->dst);
	return;
}

int FindSearch(const TCHAR *structs[],int structmax,const TCHAR *str)
{
	int min,max;

	min = 0;
	max = structmax;
	while ( min < max ){
		int mid,result;

		mid = (min + max) / 2;
		result = tstrcmp(structs[mid],str);
		if ( result < 0 ){
			min = mid + 1;
		}else if ( result > 0 ){
			max = mid;
		}else{
			return mid;
		}
	}
	return -1;
}

void GetCursorFileName(EXECSTRUCT *Z) // %R,%Y,%t
{
	DWORD attr;

	GetValue(Z,*(Z->src - 1),Z->dst);
								// カーソルエントリのファイルの属性を入手
	if ( PPxEnumInfoFunc(Z->Info,PPXCMDID_CSRATTR,(TCHAR *)&attr,&Z->IInfo) &&
			(attr & FILE_ATTRIBUTE_DIRECTORY) ){
		setflag(Z->status,ST_SDIRREF | ST_CHKSDIRREF);
	}else{
		setflag(Z->status,ST_CHKSDIRREF);
	}
	Z->dst += tstrlen(Z->dst);
}

DWORD GetModuleNameHash(const TCHAR *src,TCHAR *dest)
{
	DWORD hash = 0;

	while( *src != '\0' ){
		TCHAR c;

		c = upper(*src++);
		*dest++ = c;
		hash = (DWORD)(hash << 6) | (DWORD)(hash >> (32 - 6)) | (DWORD)c;
	}
	*dest = '\0';
	return hash | CID_USERNAME;
}

BOOL GetCommandString(EXECSTRUCT *Z)
{
	TCHAR cmdname[CmdFuncMaxLen];
	const TCHAR *src;
									// コマンドを切り出す
	{
		int i;
		TCHAR *dest,firstchar;

		dest = cmdname;
		firstchar = SkipSpace(&Z->src);
		src = Z->src;
		if ( firstchar == '*' ){
			*dest++ = firstchar;
			src++;
		}
		for ( i = 0 ; i < (int)(TSIZEOF(cmdname) - 1) ; i++ ){
			if ( !Isalnum(*src) ) break;
			*dest++ = *src++;
		}
		if ( dest == cmdname ) return TRUE; // 文字列無し
		*dest = '\0';
	}
									// *が付加→コマンド
	if ( cmdname[0] == '*' ){
		DWORD namehash;
		int cmdid;

		SkipSpace(&src);
		Z->src = src;
		namehash = GetModuleNameHash(cmdname + 1,cmdname + 1);

		cmdid = FindSearch(CmdName,CID_COUNTS,cmdname + 1);
		if ( cmdid >= 0 ){
			Z->command = cmdid + CID_MINID;
		}else{
			Z->command = namehash;
			tstrcpy(Z->dst,cmdname + 1);
			// tstrlen(cmdname + 1) + 1 の最適化結果↓
			Z->dst += tstrlen(cmdname);
			*Z->dst = '\0';
		}
		return FALSE;
	}
									// 記号類が付加→コマンドでない
	if ( (*src != '.') && (*src != '\\') && (*src != ':') ){
		if ( NO_ERROR == GetCustTable(T("A_exec"),cmdname,Z->dst,
				TSTROFF(CMDLINESIZE) - TSTROFF(Z->dst - Z->DstBuf)) ){ // エイリアス有り
			Z->src = src;
			BackupSrc(Z,Z->dst);
			return FALSE;
		}
	}
	return TRUE;
}

// Z の初期化 -----------------------------------------------------------------
void ZInit(EXECSTRUCT *Z)
{
	Z->command = CID_FILE_EXEC;
	Z->dst = Z->edit.EdBottom = Z->DstBuf;
	Z->edit.CsrBottom = 0;
	Z->edit.CsrTop = -1;
}

/*-----------------------------------------------------------------------------
	hWnd		親ウィンドウ
	pos			ウィンドウ表示する時に用いるスクリーン座標。NULLならhWndの(0,0)
	param		パラメータ(ASCIIZ)
	extract		展開先。必要なければ NULL でよい。
	flag		各種設定
	戻り値		NO_ERROR:正常終了
				259(ERROR_NO_MORE_ITEMS):空欄終了
				ERROR_CANCELLED :キャンセル
-----------------------------------------------------------------------------*/
PPXDLL ERRORCODE PPXAPI PP_ExtractMacro(HWND hWnd, PPXAPPINFO *ParentInfo, POINT *pos, const TCHAR *param, TCHAR *extract, int flags)
{
	EXECSTRUCT Z;
	BOOL loadnext = FALSE;
	TCHAR *funcstart = NULL;

#ifdef WINEGCC
	setflag(flag,XEO_NOUSEPPB);
#endif
										// 最初の初期化 -----------------------
	ThInit(&Z.ExtendDst);
	ThInit(&Z.ExpandCache);
	ThInit(&Z.StringVariable);
	Z.hWnd = hWnd;
	Z.posptr = pos;

	if ( ParentInfo != NULL ){
		Z.Info = ParentInfo;
	}else{
		Z.Info = PPxDefInfo;
	}

	PPxEnumInfoFunc(Z.Info, PPXCMDID_STARTENUM, Z.DstBuf, &Z.IInfo);
	Z.oldsrc = NULL;
	Z.status = 0;
	Z.result = NO_ERROR;
	Z.curdir[0] = '\0';
	Z.useppb = -1;
	Z.edit.cache.hash = MAX32;
	while ( Z.result == NO_ERROR ){
										// 繰り返し行う初期化 -----------------
		Z.flag = flags;
		Z.status = Z.status & (ST_EXECLOOP);
		Z.src = param;
		Z.ExtendDst.top = 0;
		Z.quotation = FALSE;
		ZInit(&Z);
		Freeoldsrc(&Z);
										// パラメータの解析開始 ---------------
		while ( Z.result == NO_ERROR ){
			if ( (Z.DstBuf + CMDLINESIZE - 1) <= Z.dst ){ // 長さ制限
				if ( (Z.command == CID_FILE_EXEC) &&
					 (Z.edit.ed == NULL) &&
					 !(Z.flag & (XEO_DISPONLY | XEO_NOEDIT)) &&
					 (extract == NULL) ){
					// 通常実行 && 部分編集無し && 展開無し
					// コマンドラインは 32k,cmd は 8k までなので、チェックする
					if ( (Z.ExtendDst.top + (Z.dst - Z.DstBuf)) > 8000 ){
						Z.result = RPC_S_STRING_TOO_LONG;
						PPErrorBox(Z.hWnd,NULL,RPC_S_STRING_TOO_LONG);
						break;
					}
				}else if ( (Z.command != CID_CLIPTEXT) &&
						   (Z.command != CID_SET) ){
					Z.result = RPC_S_STRING_TOO_LONG;
					PPErrorBox(Z.hWnd,NULL,RPC_S_STRING_TOO_LONG);
					break;
				}
				*Z.dst = '\0';
				ThCatString(&Z.ExtendDst,Z.DstBuf);
				Z.dst = Z.DstBuf;
				*Z.dst = '\0';
			}

			if ( *Z.src == '\0' ){ // src の nested を戻す
				OLDSRCSTRUCT *back;

				if ( Z.oldsrc == NULL ) break;
				back = Z.oldsrc;
				Z.src = back->src;
				Z.oldsrc = back->backptr;
				HeapFree(DLLheap,0,back);
				continue;
			}
														// *コマンド / コマンド
			if ( (Z.dst == Z.DstBuf) && (Z.command == CID_FILE_EXEC) &&
					!(Z.flag & XEO_DISPONLY) &&
					((extract == NULL) || (Z.flag & XEO_EXTRACTEXEC)) ){
				if ( GetCommandString(&Z) == FALSE ) continue;
			}

			if ( (*Z.src == '\"') && (funcstart != NULL) ){ // " チェック
				*Z.dst++ = *Z.src++;
				if ( Z.quotation ){
					if ( *Z.src != '\"' ){
						Z.quotation = FALSE;
					}else{
						*Z.dst++ = *Z.src++;
						continue;
					}
				}else{
					Z.quotation = TRUE;
					continue;
				}
			}

			// 関数末端
			if ( (*Z.src == ')') && (funcstart != NULL) && (Z.quotation == FALSE) ){
				Z.src++;
				*Z.dst = '\0';
				Z.dst = funcstart;
				FunctionModule(&Z);
				funcstart = NULL;
				continue;
			}

			if ( ((UTCHAR)(*Z.src) < ' ') && ((*Z.src == '\n') || (*Z.src == '\r')) && !(Z.flag & XEO_INRETURN) ){ // 改行
				do {
					Z.src++;
				} while ( (*Z.src == '\n') || (*Z.src == '\r') );
				if ( Z.flag & XEO_DISPONLY ){		// 表示のみ
					*Z.dst++ = ':';
				}else{					// 実行
					*Z.dst = '\0';
					ZExec(&Z);
					ZInit(&Z);
				}
				continue;
			}

			if ( *Z.src != '%' ){				// '%' 以外はそのまま複写
				*Z.dst++ = *Z.src++;
				continue;
			}

			Z.edit.ed = NULL;
			resetflag(Z.status,ST_PATHFIX);
			if ( *(++Z.src) == '!' ){		// %!	部分編集 ------------------
				Z.edit.ed = Z.dst;
				Z.src++;
			}else if ( *(Z.src) == '$' ){	// %$	キャッシュ付き部分編集 ---
				setflag(Z.status,ST_USECACHE);
				Z.edit.ed = Z.dst;
				Z.src++;
				Z.edit.cache.srcptr = Z.src;
			}
			*Z.dst = '\0';
			switch( *Z.src++ ){ //---------------------------------- マクロ解析
	case '\0':					//----- NULL	文末 --------------------------
		Z.src--;
		break;

	case '\"':					//----- %"		タイトル変更 ------------------
		SetTitleMacro(&Z);
		break;

	case '#':					//----- %#		対象ファイル名 ----------------
		loadnext = EnumEntries(&Z,extract); // ※extractは、展開の有無判断用
		break;

	case '\'':					//----- %'		A_exec / 環境変数参照 ---------
		ZExpandAlias(&Z);
		break;

	case '*': {					//----- %*		関数モジュール ----------------
		TCHAR *dst;
		const TCHAR *olds;

		for ( dst = Z.dst ; Isalnum(*Z.src) ; ) *dst++ = *Z.src++;
		*dst++ = '\0';
		*dst = '\0';

		olds = Z.src;
		if ( SkipSpace(&Z.src) != '(' ){ // 括弧無し
			Z.src = olds;
			FunctionModule(&Z);
			if ( Z.result != NO_ERROR ) continue;
		}else if ( funcstart != NULL ){ // %* 内 %*
			NestedFunction(&Z);
			if ( Z.result != NO_ERROR ) continue;
		}else{ // 括弧有り
			Z.src++;
			funcstart = Z.dst;
			Z.dst = dst;
			Z.quotation = FALSE;
			continue;
		}
		break;
	}

	case '0':					//----- %0		PPx ディレクトリ --------------
	case '1':						//	%1	カーソル位置のディレクトリ
	case '2':						//	%2	カーソルの反対位置ディレクトリ
	case '3':						//	%3	*pack 内で使用
	case '4':						//	%4	*pack 内で使用
		setflag(Z.status,ST_PATHFIX);
		GetValue(&Z,*(Z.src - 1),Z.dst);
		Z.dst += tstrlen(Z.dst);
		break;

	case '&':					//----- %&	終了コードで判断 ----------------
		setflag(Z.flag,XEO_SEQUENTIAL);
		if ( Z.flag & XEO_DISPONLY ){		// 表示のみ
			*Z.dst++ = ':';
			break;
		}else{					// 実行
			ZExec(&Z);
			if ( Z.result != NO_ERROR ) break;
			if ( Z.ExitCode != 0 ){
				Z.result = ERROR_CANCELLED;
				break;
			}
			ZInit(&Z);
			continue;
		}

	case ':':					//----- %:		コマンド区切り ----------------
		if ( Z.flag & XEO_DISPONLY ){		// 表示のみ
			*Z.dst++ = ':';
			break;
		}else{					// 実行
			ZExec(&Z);
			ZInit(&Z);
			continue;
		}

	case 'C':					// %C		対象ファイル名 ------------
	case 'X':					// %X	対象ファイル拡張子なし --------
	case 'T':					// %T	対象ファイル拡張子のみ --------
		ZGetName(&Z,Z.dst,'\0');
		Z.dst += tstrlen(Z.dst);
		break;

	case 'D':					//----- %D		PPxのパス取得 ------------
		GetPPxPath(&Z);
		break;

	case 'E':					//----- %E		パラメータ入力 ----------------
		Z.edit.ed = Z.dst;
		break;

	case 'F':					//----- %F		 ----------------
		ZGetName(&Z,Z.dst,'\0');
		if ( Z.quotation != FALSE ) tstrreplace(Z.dst,T("\""),T("\"\""));
		Z.dst += tstrlen(Z.dst);
		break;

	case 'G':					//----- %G		 ----------------
		ZGetMessageText(&Z);
		break;

	case 'H':					//----- %H		ヒストリー呼び出し ------------
		LoadHistory(&Z,PPXH_GENERAL | PPXH_COMMAND);
		break;

	case 'I':						//	%I	情報メッセージボックス
	case 'J':						//	%J	エントリ移動
	case 'K':						//	%K	内蔵コマンド
	case 'Q':						//	%Q	確認メッセージボックス
	case 'Z':						//	%Z	ShellExecute で実行
	case 'j':						//	%j	パスジャンプ
	case 'k':						//	%k	内蔵コマンド
	case 'v':						//	%v	PPV で表示
	case 'z':						//	%z	ShellContextMenu で実行
		if ( Z.flag & XEO_DISPONLY ){	// 表示
			*Z.dst++ = '%';
			*Z.dst++ = *(Z.src - 1);
		}else{
			ZExec(&Z);
			ZInit(&Z);
			Z.command = *(Z.src - 1);
			SkipSpace(&Z.src);
		}
		break;

	case 'L':					//	%Ln	PPv の論理行数 ----------------
	case 'l':					//	%ln	PPv の表示行数 ----------------
		ZGetPPvCursorPos(&Z);
		break;

	case 'M':					//----- %M		補助メニュー／拡張子判別 ------
		if ( MenuCmd(&Z) ) continue;
		break;

	case 'N':					//----- %N		PPxのHWND取得 ------------
		GetPPxHWND(&Z);
		break;

	case 'O':					//----- %O		オプション再設定 --------------
		ZReadOption(&Z);
		break;

	case 'P':					//----- %P		パス・ファイル名入力 ----------
		setflag(Z.status,ST_PATHFIX);
		Z.edit.ed = Z.dst;
		break;

	case 'R':					//----- %R		カーソル位置ファイル名 --------
	case 'Y':						//	%Y	カーソル位置の拡張子なし ------
		setflag(Z.status,ST_PATHFIX);
		// %t へ
	case 't':						//	%t	カーソル位置の拡張子のみ ------
		GetCursorFileName(&Z);
		break;

	case 'S':					//------ %S		ディレクトリがある場合の文字列-
		if ( !(Z.status & ST_CHKSDIRREF) ){ // 属性判定をしていないならここで。
			if ( PPxEnumInfoFunc(Z.Info,PPXCMDID_ENUMATTR,Z.dst,&Z.IInfo) &&
					(*(DWORD *)Z.dst & FILE_ATTRIBUTE_DIRECTORY) ){
				setflag(Z.status,ST_SDIRREF | ST_CHKSDIRREF);
			}else{
				setflag(Z.status,ST_CHKSDIRREF);
			}
		}

		if ( SkipSpace(&Z.src) == '\"' ) Z.src++;
		while ( *Z.src && (*Z.src != '\"') ){
			if ( Z.status & ST_SDIRREF ){ // サブディレクトリがあるときだけ複写
				*Z.dst++ = *Z.src++;
			}else{
				Z.src++;
			}
		}
		if ( *Z.src == '\"' ) Z.src++;
		break;

	case 'W':					//----- %W		Window Caption取得 ------------
		ZGetWndCaptionMacro(&Z);
		break;

	case '\\': {				//----- %\		\ の付加 ----------------------
		TCHAR *buftop;

		setflag(Z.status,ST_PATHFIX);
		buftop = Z.DstBuf;
#ifndef UNICODE
		if ( (Z.command >= CID_USERNAME) || (funcstart != NULL) ){
			buftop = Z.dst;
			while ( buftop > Z.DstBuf ){
				if ( *(buftop - 1) == '\0' ) break;
				buftop--;
			}
		}
#endif
		if ( Z.dst > buftop ){
			UTCHAR c;

			c = (UTCHAR)*(Z.dst - 1);
#ifdef UNICODE
			if ( (c  > ' ') && (c != '\"') && (c != '\\') ) *Z.dst++ = '\\';
#else
			if ( (c  > ' ') && (c != '\"') ){
				CatPath(NULL,buftop,NilStr);
				Z.dst += tstrlen(Z.dst);
			}
#endif
		}
		break;
	}

	case '@':					//----- %@		レスポンスファイル 対応 %C ----
		*Z.dst++ = '@';
		// %a へ
	case 'a':					//----- %a		レスポンスファイル 対応 %C ----
		setflag(Z.status,ST_PATHFIX);
		if ( CreateResponseFile(&Z) == FALSE ) continue;
		break;

	case 'b':					//----- %b		行末エスケープ・文字挿入 ------
		LineEscape(&Z);
		break;

	case 'e':					//----- %e		一行編集の設定変更 ------------
		SetEditMode(&Z);
		break;

	case 'g':					//----- %g		A_exec展開
		ZExpandAliasWithExtract(&Z);
		break;

	case 'h':					//----- %h		ヒストリー呼び出し ------------
		LoadHistory(&Z,PPXH_PPCPATH);
		break;

	case 'm':					//----- %m		追加情報(何もしない) ----------
		if ( *Z.src == '\"' ){
			Z.src++;
			for (;;){
				UTCHAR c;

				c = (UTCHAR)*Z.src;
				if ( c < ' ' ) break;
				Z.src++;
				if ( c == '\"' ) break;
			}
		}else{
			while ( (UTCHAR)(*Z.src) > ' ' ) Z.src++;
		}
		continue;

	case 'n':					//----- %n		WND取得 ------------
		GetPPxID(&Z);
		break;

	case 's':					//----- %s		特殊環境変数
		ZStringVariable(&Z, &Z.src, StringVariable_function);
		break;

	case 'u':					//----- %u	UnXXX を実行 ------------------
		if ( Z.flag & XEO_DISPONLY ){	// 表示
			*Z.dst++ = '%';
			*Z.dst++ = 'u';
		}else{
			ZExec(&Z);
			ZInit(&Z);
			Z.command = 'u';
			setflag(Z.status, ST_MULTIPARAM);
		}
		break;

	case '{':					//----- %{		部分編集指定開始 --------------
		Z.edit.ed = NULL; // %$ での編集を無効にする
		Z.edit.EdBottom = Z.dst;
		Z.edit.CsrBottom = 0;
		Z.edit.CsrTop = -1;
		break;

	case '|':					//----- %|		カーソル位置指定 --------------
		if ( Z.edit.CsrTop == -1 ){	// 1st:指定位置にカーソル
			Z.edit.CsrBottom = Z.edit.CsrTop = Z.dst - Z.edit.EdBottom;
		}else{					// 2nd:指定範囲を選択
			Z.edit.CsrBottom = Z.edit.CsrTop;
			Z.edit.CsrTop = Z.dst - Z.edit.EdBottom;
		}
		break;

	case '}':					//----- %}		部分編集指定終了 --------------
		Z.edit.ed = Z.edit.EdBottom;
		break;

	case '~': {					//----- %~		反対窓内容取得 ----------------
		TCHAR *p;
		HWND hPairWnd;

		p = Z.dst;
		*p++ = '%';
		while ( Isalnum(*Z.src) || (*Z.src == '#') ) *p++ = *Z.src++;
		*p = '\0';
		hPairWnd = GetPPcPairWindow(Z.Info);
		if ( hPairWnd != NULL ) ExtractPPxCall(hPairWnd, &Z, Z.dst);
		break;
	}

	default:					//----- 未定義	その文字自身 ------------------
		*Z.dst++ = *(Z.src - 1);
}
//-----------------------------------------------------------------------------
			if ((Z.edit.ed != NULL) && !(Z.flag &(XEO_DISPONLY | XEO_NOEDIT))){
				*Z.dst = '\0';
				if( EditExtractText(&Z) == FALSE ) break; // 部分編集
			}
		}
		*Z.dst = '\0';
		if ( Z.result != NO_ERROR ) break; // エラー有り

		if ( extract != NULL ){	// 展開処理
			Z.DstBuf[CMDLINESIZE - 1] = '\0';
			tstrcpy(extract,Z.DstBuf);
			if ( (Z.DstBuf + CMDLINESIZE - 1) <= Z.dst ){ // 長さ制限
				Z.result = RPC_S_STRING_TOO_LONG;
			}
		}else{					// 実行処理
			ZExec(&Z);

			// 次のマークを処理するか判断
			if ( (Z.flag & XEO_EXECMARK) && !(Z.flag & XEO_NOEXECMARK) ){
				setflag(Z.status,ST_EXECLOOP);
				if ( loadnext == FALSE ){
					if ( PPxEnumInfoFunc(Z.Info,PPXCMDID_NEXTENUM,Z.DstBuf,&Z.IInfo) != 0 ){
						continue;
					}
				}else{ // %# は既に PPXCMDID_NEXTENUM を実行済み
					loadnext = FALSE;
					PPxEnumInfoFunc(Z.Info,'C',Z.DstBuf,&Z.IInfo);
					if ( Z.DstBuf[0] ) continue;
				}
			}
		}
		break;
	}
	PPxEnumInfoFunc(Z.Info,PPXCMDID_ENDENUM,Z.DstBuf,&Z.IInfo);

	if ( Z.flag & (XEO_DELTEMP | XEO_DOWNCSR) ){
		if ( (extract == NULL) && (Z.flag & XEO_DELTEMP) ){
			const TCHAR *ResName;

			ResName = ThGetString(&Z.StringVariable,ResponseName_ValueName,NULL,0);
			if ( ResName != NULL ) DeleteFileL(ResName);
		}
		if ( (Z.flag & XEO_DOWNCSR) && (Z.result == NO_ERROR) ){
			PostMessage(Z.hWnd,WM_PPXCOMMAND,K_raw | K_dw,0);
		}
	}

	ThFree(&Z.StringVariable);
	ThFree(&Z.ExpandCache); // %M のキャッシュを解放
	ThFree(&Z.ExtendDst);
	Freeoldsrc(&Z);
	if ( Z.useppb != -1 ) FreePPb(Z.hWnd, Z.useppb);
	return Z.result;
}
/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
BOOL IsCmdNul(TCHAR *cmdbuf)
{
	if ( cmdbuf[0] == '\0' ) return TRUE;
	if ( cmdbuf[1] == '\0' ){
		if ( ((UTCHAR)cmdbuf[0] == EXTCMD_CMD) ||
			 ((UTCHAR)cmdbuf[0] == EXTCMD_KEY) ){
			return TRUE;
		}
	}
	return FALSE;
}

PPXDLL int PPXAPI PP_GetExtCommand(const TCHAR *src,const TCHAR *ID,TCHAR *cmdbuf,TCHAR *TypeName)
{
	VFSFILETYPE vft;
	int count = 0;
	TCHAR kword[MAX_PATH],name[VFPS];
	BYTE *filebuf;
	const TCHAR *namep,*extp;
	DWORD size = 0;
	HANDLE hFile;
	FN_REGEXP fn;
	int result = PPEXTRESULT_FILE;
	ERRORCODE errresult;

	cmdbuf[0] = '\0';
	filebuf = HeapAlloc(DLLheap,0,VFS_check_size);
	if ( filebuf == NULL ) return PPEXTRESULT_NONE;

										// ファイルの内容で判別 ---------------
	vft.flags = VFSFT_TYPE | VFSFT_STRICT;
	vft.type[0] = '\0';

	hFile = CreateFileL(src,GENERIC_READ,
			FILE_SHARE_WRITE | FILE_SHARE_READ,NULL,
			OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if ( hFile != INVALID_HANDLE_VALUE ){
		size = ReadFileHeader(hFile,filebuf,VFS_check_size);
		CloseHandle(hFile);
	}
	if ( size && (TypeName != NULL) ){ // ファイルならディレクトリかどうかの調査も行う
		if ( tstrstr(src,T(".{")) != NULL ){
			result = PPEXTRESULT_CLSID;
		}else if ( (size > 100) && (*filebuf == 0x4c) &&
				(*(filebuf + 1) == 0) && (*(filebuf + 2) == 0) ){
			result = PPEXTRESULT_LINK;
		}else{
			tstrcpy(name,src);

			if ( VFSCheckDir(name,filebuf,size,NULL) ){
				result = PPEXTRESULT_VFSDIR;
			}
		}
	}

	errresult = VFSGetFileType(src,(char *)filebuf,size,&vft);
	HeapFree(DLLheap,0,filebuf);

	namep = FindLastEntryPoint(src);
	extp = namep + FindExtSeparator(namep);

	if ( errresult == NO_ERROR ){
		if ( TypeName != NULL ) tstrcpy(TypeName,vft.type);

		if ( GetCustDword(T("X_exts"),0) == 0 ){
			if ( *extp == '.' ){				// 拡張子あり
				wsprintf(name,T("%s%s"),vft.type,extp);
				if ( NO_ERROR == GetCustTable(ID,name,cmdbuf,TSTROFF(CMDLINESIZE)) ){
					if ( !IsCmdNul(cmdbuf) ) return result; // 空欄なら以降の名前判別へ
				}
			}

			if ( NO_ERROR == GetCustTable(ID,vft.type,cmdbuf,TSTROFF(CMDLINESIZE)) ){
				if ( !IsCmdNul(cmdbuf) ) return result; // 空欄なら以降の名前判別へ
			}
			vft.type[0] = '\0'; // 内容判別できなかったので名前判別へ
		}
	}
										// ファイル名・拡張子の抽出 -----------
	if ( *extp == '.' ){				// 拡張子あり
		extp++;
		if ( TypeName != NULL ) tstrcpy(TypeName,extp);
	}else{								// 拡張子なし
		tstrcpy(name,namep);
		tstrcat(name,T("."));
		namep = name;
		if ( TypeName != NULL ) tstrcpy(TypeName,name);
	}
										// ファイル名で判別 -------------------
	for ( ; EnumCustTable(count,ID,kword,cmdbuf,ISCMDNULREQUEIERSIZE) >= 0 ; count++ ){ // 限定
		if ( kword[0] == ':' ){ // ファイル内容種別だった
							// 種別で判定済み or 種別無しなので次へ
			if ( vft.type[0] == '\0' ) continue;
			if ( !IsCmdNul(cmdbuf) && !tstricmp(kword,vft.type) ) goto enumhit;
		}else if ( kword[0] == '/' ){ // ワイルドカード指定
			int fnresult;

			MakeFN_REGEXP(&fn,kword + 1);
			fnresult = FilenameRegularExpression(namep,&fn);
			FreeFN_REGEXP(&fn);
			if ( fnresult ) goto enumhit;
		}
		if ( tstrchr(kword,'.') ){			// 名前で判別
			if ( !tstricmp(kword,namep) ) goto enumhit;
		}else{								// 拡張子で判別
			if ( !tstricmp(kword,extp) ) goto enumhit;
		}
	}
	if ( (*extp == '\0') &&
		 (NO_ERROR == GetCustTable(ID,T("."),cmdbuf,TSTROFF(CMDLINESIZE))) ){
		return result;
	}
	if ( NO_ERROR == GetCustTable(ID,WildCard_All,cmdbuf,TSTROFF(CMDLINESIZE)) ){
		return result;
	}
	if ( result != PPEXTRESULT_FILE ) return -result;
	return PPEXTRESULT_NONE;

enumhit:
	EnumCustTable(count,ID,kword,cmdbuf,TSTROFF(CMDLINESIZE));
	return result;
}

// %# 本体
BOOL EnumEntries(EXECSTRUCT *Z,const TCHAR *extract)
{
	BOOL useopt = FALSE;
	const TCHAR *oldsrc;
	const TCHAR *ptr;
	TCHAR separator = ' ',*enummax,buf[VFPS];
	int maxentries = 0xfffffff;

	oldsrc = Z->src;
	if ( Isdigit(*oldsrc) ) maxentries = GetDigitNumber(&oldsrc);

	if ( ((UTCHAR)*oldsrc > ' ') && !Isalnum(*oldsrc) ){
		separator = *oldsrc++;
	}
	if ( (UTCHAR)*oldsrc > ' ' ){
		useopt = TRUE;
		oldsrc++;
	}
	enummax = Z->DstBuf + CMDLINESIZE - (1 + 3);
	// 後段の文字列が残るように末尾を決定
	ptr = Z->src;
	while ( ((UTCHAR)(*ptr) >= ' ') || ((*ptr != '\n') && (*ptr == '\r')) ){ // 改行以外
		if ( *ptr == '%' ){
			if ( *(ptr + 1) == '%' ){
				ptr++;
			}else if ( *(ptr + 1) == ':' ){
				break; // コマンド区切り
			}else {
				enummax -= 100; // 仮の確保幅
				if ( enummax < Z->dst ){
					enummax = Z->dst;
					break;
				}
			}
		}
		ptr++;
		enummax--;
	}
	setflag(Z->status,ST_CHKSDIRREF);
	for ( ; ; ){
		int len;

		Z->src = oldsrc;
		if ( IsTrue(useopt) ){
			if ( (*(oldsrc - 1) == 'F') &&
					PPxEnumInfoFunc(Z->Info,PPXCMDID_ENUMATTR,buf,&Z->IInfo) &&
					(*(DWORD *)buf & FILE_ATTRIBUTE_DIRECTORY) ){
				setflag(Z->status,ST_SDIRREF);
			}

			ZGetName(Z,buf,'\0'); // ST_SDIRREF 判定は、%F のときにはない
			if ( Z->quotation != FALSE ) tstrreplace(buf,T("\""),T("\"\""));
		}else{
			ZGetName(Z,buf,'C'); // ST_SDIRREF 判定内蔵
		}

		len = tstrlen32(buf);
		if ( enummax <= (Z->dst + len) ){
			if ( (Z->command == CID_FILE_EXEC) && (Z->edit.ed == NULL) && !(Z->flag & (XEO_DISPONLY | XEO_NOEDIT)) && (extract == NULL) ){ // 通常実行 && 部分編集無し && 展開無し
				// コマンドラインは 32k,cmd は 8k までなので、チェックする
				if ( Z->ExtendDst.top > (8000 - CMDLINESIZE * 2) ) break;
			}else if ( Z->command != CID_CLIPTEXT ){
				break;
			}
			*Z->dst = '\0';
			ThCatString(&Z->ExtendDst,Z->DstBuf);
			Z->dst = Z->DstBuf;
		}
		if ( (buf[0] != '\"') && (tstrchr(buf,separator)) != NULL ){
			Z->dst += wsprintf(Z->dst,T("\"%s\""),buf);
		}else{
			tstrcpy(Z->dst,buf);
			Z->dst += len;
		}
		if ( PPxEnumInfoFunc(Z->Info,PPXCMDID_NEXTENUM,buf,&Z->IInfo) == 0 ){
			break;
		}
		*Z->dst++ = separator;
		*Z->dst = '\0';

		if ( --maxentries == 0 ) break;
	}
	return TRUE;
}

DWORD GetCacheHash(EXECSTRUCT *Z)
{
	const TCHAR *plast,*p;
	DWORD hash = 0;

	p = Z->edit.cache.srcptr;
	plast = Z->src;
	if ( (plast - p) > CMDLINESIZE ){
		hash = *p;
	}else{
		for ( ; p < plast ; p++ ) hash += *p;
	}
	return hash;
}

// 部分編集を行う
BOOL EditExtractText(EXECSTRUCT *Z)
{
	TINPUT tinput;
	DWORD hash C4701CHECK;

	if ( (Z->status & ST_USECACHE) &&
		 (Z->edit.cache.hash == (hash = GetCacheHash(Z))) ){
		// キャッシュが使用できる
		ThGetString(&Z->StringVariable,EditCache_ValueName,Z->edit.ed,CMDLINESIZE);
	}else{
		tinput.title	= ZGetTitleName(Z);
		tinput.buff		= Z->edit.ed;
		tinput.size		= CMDLINESIZE - (Z->dst - Z->DstBuf) - 1;
		tinput.flag		= TIEX_USESELECT | TIEX_USEINFO;
		tinput.firstC	= Z->edit.CsrBottom;
		tinput.lastC	= Z->edit.CsrTop;
		if ( ZTinput(Z,&tinput) == FALSE ) return FALSE;

		if ( Z->status & ST_PATHFIX ){
			VFSFixPath(NULL,Z->edit.ed,GetZCurDir(Z),VFSFIX_VFPS);
		}
		if ( Z->status & ST_USECACHE ){
			Z->edit.cache.hash = hash; // C4701ok
			ThSetString(&Z->StringVariable,EditCache_ValueName,Z->edit.ed);
		}
	}
	Z->dst = Z->edit.ed + tstrlen(Z->edit.ed);
	Z->edit.EdBottom = Z->dst;
	Z->edit.CsrBottom = 0;
	Z->edit.CsrTop = -1;
	resetflag(Z->status,ST_USECACHE);
	return TRUE;
}
