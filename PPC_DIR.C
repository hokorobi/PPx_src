/*-----------------------------------------------------------------------------
	Paper Plane cUI										Directory読み込み
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <commctrl.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#include "PPCOMBO.H"
#include "FATTIME.H"
#pragma hdrstop

#define dispNtimes	20		// 時間チェックを行うエントリ間隔
#define SORTINFOSIZE 10000	// ソート中表示を始めるエントリ数
#define ENABLECNTTIME ((5*1000) / dispNwait) // ブレイクチェックを始める秒数

#define CsCEL(No)  ((ENTRYCELL *)cs->cinfo->e.CELLDATA.p)[((ENTRYINDEX *)cs->cinfo->e.INDEXDATA.p)[No]]
#define CsCELdata(No) ((ENTRYCELL *)cs->cinfo->e.CELLDATA.p)[No]
#define CsCELt(No) ((ENTRYINDEX *)cs->cinfo->e.INDEXDATA.p)[No]

const TCHAR SortingStr[] = MES_SSRT;

void SetDummyRelativeDir(PPC_APPINFO *cinfo,const TCHAR *name,BYTE attr,BYTE type);
void LoadSettingGeneral(PPC_APPINFO *cinfo,int *flags,TCHAR *mask);
void LoadSettingSecond(PPC_APPINFO *cinfo,const TCHAR *path,int *flags,TCHAR *mask);
void PreExecuteSort(PPC_APPINFO *cinfo,BYTE sortid);

ENTRYCELL *AddMaskPathDirectory(PPC_APPINFO *cinfo,ENTRYDATAOFFSET cellindex,size_t len,ENTRYDATAOFFSET arcMax,int depth,ENTRYDATAOFFSET *zeroIndex)
{
	ENTRYCELL *cell;
	ENTRYDATAOFFSET i,zi;
	const TCHAR *filename;

	zi = *zeroIndex;
	for ( ; zi < arcMax ; zi++ ){
		if ( (CELdata(zi).f.nFileSizeHigh | CELdata(zi).f.nFileSizeLow) != 0 ){
			continue;
		}
		if ( CELdata(zi).f.dwFileAttributes &
				(FILE_ATTRIBUTEX_FOLDER | FILE_ATTRIBUTE_DIRECTORY) ){
			if ( cinfo->UseArcPathMask != ARCPATHMASK_OFF ) break; //更新時はスキップ不可
			continue;
		}
		break;
	}

	*zeroIndex = zi;

	filename = CELdata(cellindex).f.cFileName;
	CELdata(cellindex).attr &= (BYTE)(~ECA_GRAY);

	// 登録済みか確認
	for ( i = zi ; i < cinfo->e.cellDataMax ; i++ ){
		if ( !memcmp(filename,CELdata(i).f.cFileName,TSTROFF(len)) &&
			 (CELdata(i).f.cFileName[len] == '\0') ){

			if ( i < arcMax ){
				if ( CELdata(i).f.dwFileAttributes & FILE_ATTRIBUTEX_FOLDER ){
					return NULL;
				}
				setflag(CELdata(i).f.dwFileAttributes,FILE_ATTRIBUTEX_FOLDER);
				continue;
			}

			return NULL; // 既にある
		}
	}

	// ディレクトリを追加
	if ( TM_check(&cinfo->e.CELLDATA,sizeof(ENTRYCELL) * (cinfo->e.cellDataMax + 2)) == FALSE){
		return NULL;
	}
	if ( TM_check(&cinfo->e.INDEXDATA,sizeof(ENTRYINDEX) * (cinfo->e.cellDataMax + 2)) == FALSE ){
		return NULL;
	}

	cell = &CELdata(cinfo->e.cellDataMax);
	SetDummyCell(cell,CELdata(cellindex).f.cFileName);
	cell->f.cFileName[len] = '\0';
	cell->f.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	cell->state = ECS_NORMAL;
	cell->attr = 0;
	CELt(cinfo->e.cellIMax) = cinfo->e.cellDataMax;
	SetCellInfo(cinfo,cell,NULL);

	cinfo->e.cellDataMax++;
	cinfo->e.cellIMax++;

	// 今までで一番浅い階層なら記憶する
	if ( (cinfo->MinArcPathMaskDepth < 0) ||
		 (cinfo->MinArcPathMaskDepth > depth ) ){
			cinfo->MinArcPathMaskDepth = depth;
			tstrcpy(cinfo->ArcPathMask,cell->f.cFileName);
		// 同じ階層が複数ある→一つ親へ戻る
	}else if ( cinfo->MinArcPathMaskDepth == depth ){
		cinfo->MinArcPathMaskDepth--;
		*FindBothLastEntry(cinfo->ArcPathMask) = '\0';
	}
	return cell;
}

void MaskPathMain(PPC_APPINFO *cinfo,int defaultdepth,const TCHAR *defaultpath)
{
	ENTRYDATAOFFSET arcMax,offset,zeroIndex = 0;
	int depthMax = 0;

	arcMax = cinfo->e.cellDataMax;
	cinfo->MinArcPathMaskDepth =
			( (XC_pmsk[(cinfo->e.Dtype.mode == VFSDT_LFILE) ? 1 : 0] > 1) ||
			  (cinfo->e.Dtype.mode == VFSDT_LFILE)
			) ? -1 : 0;
	cinfo->ArcPathMask[0] = '\0';

	for ( offset = 0 ; offset < arcMax ; offset++ ){
		size_t len;
		int depth;
		const TCHAR *lp,*filename;
		ENTRYCELL *cell;

		filename = CELdata(offset).f.cFileName;

		if ( (CELdata(offset).f.nFileSizeHigh | CELdata(offset).f.nFileSizeLow) == 0 ){ // ディレクトリの可能性
			TCHAR *tmpname,*p;

			tmpname = CELdata(offset).f.cFileName;

			p = FindPathSeparator(tmpname);
			if ( p != NULL ){
				TCHAR *np;
				ENTRYDATAOFFSET j;

				for (;;){
					np = FindPathSeparator(p);
					if ( np == NULL ) break;
					p = np + 1;
				}
				if ( *(p + 1) == '\0' ){
					zeroIndex = offset + 1;
					for ( j = arcMax ; j < cinfo->e.cellDataMax ; j++ ){
						if ( !tstrcmp(tmpname,CELdata(j).f.cFileName) ){
							setflag(CELdata(offset).f.dwFileAttributes,FILE_ATTRIBUTEX_FOLDER);
							break;
						}
					}
				}
			}
		}

		depth = GetEntryDepth(filename,&lp);
		if ( depth > depthMax ) depthMax = depth;
		if ( lp == NULL ){ // パス区切り無し：ルート相当
			if ( !(CELdata(offset).attr & (ECA_PARENT | ECA_THIS)) ){
				cinfo->MinArcPathMaskDepth = 0;
				cinfo->ArcPathMask[0] = '\0';
			}
			continue;
		}
		// 仮想ディレクトリの生成
		len = lp - filename;	// <path>\filename の長さ
		cell = AddMaskPathDirectory(cinfo,offset,len,arcMax,depth,&zeroIndex);
		if ( cell == NULL ) continue; // 既にある
		while ( --depth ){ // 上位階層が無ければ作成する
			filename = cell->f.cFileName;
			lp = FindBothLastEntry(filename);
			cell = AddMaskPathDirectory(cinfo,cinfo->e.cellDataMax - 1,
					lp - filename,arcMax,depth,&zeroIndex);
			if ( cell == NULL ) break;
		}
	}

	if ( (arcMax != cinfo->e.cellDataMax) || (cinfo->UseArcPathMask != ARCPATHMASK_OFF) ){ // ディレクトリが増えた→階層有り
		if ( cinfo->UseArcPathMask == ARCPATHMASK_OFF ){
			cinfo->UseArcPathMask = ARCPATHMASK_ARCHIVE;
		}

		if ( (depthMax >= defaultdepth) && (*defaultpath != '\0') ){
			tstrcpy(cinfo->ArcPathMask,defaultpath);
			cinfo->ArcPathMaskDepth = defaultdepth;
			MaskEntryMain(cinfo,&cinfo->mask,cinfo->ArcPathMask);
		}else{
			cinfo->ArcPathMaskDepth = cinfo->MinArcPathMaskDepth;
			if ( cinfo->ArcPathMaskDepth == 0 ) cinfo->ArcPathMask[0] = '\0';
			MaskEntryMain(cinfo,&cinfo->mask,NilStr);
		}
	}
	cinfo->MinArcPathMaskDepth = 0;
}

BOOL CheckWarningName(const TCHAR *filename)
{
	TCHAR buf[VFPS];

	if ( tstrchr(filename,'%') ){				// 環境変数
		if ( TSIZEOF(buf) <= ExpandEnvironmentStrings(filename,buf,TSIZEOF(buf)) ){
			return TRUE;
		}
		filename = buf;
	}
	if ( tstrchr(filename,':') ) return TRUE;	// ドライブ名又はストリーム
	if ( filename[0] == '\\' ) return TRUE; // 絶対パス指定

	while (*filename){
		int count;

		count = 0;
		while ( *filename == '.' ){
			count++;
			filename++;
		}
		while ( *filename == ' ' ) filename++; // エントリ末尾の空白を除去
		if ( (count >= 2) && ((*filename == '\0') || (*filename == '\\')) ){
			return TRUE;
		}
		filename = FindPathSeparator(filename);
		if ( filename == NULL ) break;
		filename++;
	}
	return FALSE;
}

BOOL ReadDirBreakCheck(PPC_APPINFO *cinfo)
{
	MSG msg;

	while ( PeekMessage(&msg,NULL,0,0,PM_REMOVE) ){
		if ( msg.message == WM_QUIT ) break;
		if ( (msg.message == WM_KEYDOWN) && ((int)msg.wParam == VK_PAUSE) ){
			return TRUE;
		}

		if ( (msg.message == WM_RBUTTONUP) ||
			 ((msg.message == WM_KEYDOWN) && ((int)msg.wParam == VK_ESCAPE)) ){
			if ( PMessageBox(cinfo->info.hWnd,NULL,NULL,MB_QYES) == IDOK ){
				return TRUE;
			}
		}
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

struct masks {
	FN_REGEXP	FileMask;
	DWORD		Attr,Dir;
	BYTE		*extcolor;
};

struct readprogress {
	int		dispN;		// 表示試行ファイル数
	int		enablecnt;	// ブレイク有効化カウンタ
	DWORD	OldTime;
};

DWORD MiniHash(TCHAR *name)
{
	DWORD data = 0;

	while(*name){
		data += *(BYTE *)name++;
		if ( *name == '\0' ) break;
		data += (*(BYTE *)name++) << 8;
		if ( *name == '\0' ) break;
		data += (*(BYTE *)name++) << 16;
		if ( *name == '\0' ) break;
		data += (*(BYTE *)name++) << 24;
	}
	return data;
}

// コメントをファイルから取得する ---------------------------------------------
void GetCommentText(ThSTRUCT *comments,const TCHAR *commentfile)
{
	TCHAR *src,*dst,*top,*textimage;

	if ( LoadTextImage(commentfile,&textimage,&src,NULL) != NO_ERROR ) return;

	while( *src ){
		TCHAR *p;
		BOOL bracket;
										// ファイル名部分を抽出
		top = dst = src;
		bracket = FALSE;
		if ( *src == '\"' ){
			bracket = TRUE;
			src++;
		}
		while ( *src ){
			if ( *src == '*' ){	// '*' は無視
				src++;
				continue;
			}

			if ( !bracket ){
				if ( (UTCHAR)*src <= (UTCHAR)' ' ){
					while ( (*src == ' ') || (*src == '\t') ) src++;
					// "XXXX    .XXX" 形式以外なら抽出終了
					if ( *src != '.' ) break;
				}
			}
			if ( *src == '\"' ){
				src++;
				break;
			}

			if ( *src == '\\' ){	// 先頭・末尾の'\'は省略(directory)
				if ( src == top ){
					src++;
					continue;
				}
				if ( *(src + 1) == '\"' ){
					src += 2;
					break;
				}
				if ( *(src + 1) == ' ' ){
					src++;
					break;
				}
			}
			*dst++ = *src++;
		}
										// 区切りをスキップ
		while ( (*src == ' ') || (*src == '\t') ) src++;
										// 行末検索
		p = tstrchr(src,'\r');
		if ( p == NULL ){
			p = tstrchr(src,'\n');
			if ( p == NULL ) p = src + tstrlen(src);
		}
		if ( (dst != top) && (p != src) ){	// コメント有り→行末まで探す
			*dst = 0;
			ThAddString(comments,top);
			top = dst = src;
			while ( src < p ) *dst++ = *src++;
			for ( ; ; ){
									// 改行スキップ
				if ( (*src == '\r') && (*(src + 1) == '\n' ) )src++;
				if ( *src ) src++;
									// 行頭が空白で無い場合は連結せず
				if ( !(*src == ' ') && !(*src == '\t') ) break;
									// 行頭空白のスキップ
				while ( (*src == ' ') || (*src == '\t') ) src++;
									// コメントコピー
				while ( (UTCHAR)*src >= (UTCHAR)' ' ) *dst++ = *src++;
			}
			*dst = 0;
			ThAddString(comments,top);
		}else{
			src = p;
		}
									// 改行スキップ
		if ( *src == '\r' ){
			src++;
			if ( *src == '\n' ) src++;
		}else if ( *src == '\n' ) src++;
	}
	if ( comments->bottom != NULL ){
		ThAddString(comments,NilStr);
	}
	HeapFree( hProcessHeap,0,textimage);
}

// ディレクトリの読み込みエラーの時の処理
#ifndef ERROR_SYMLINK_CLASS_DISABLED
#define ERROR_SYMLINK_CLASS_DISABLED 1463
#endif

HANDLE FixReadDirectoryError(PPC_APPINFO *cinfo,struct readprogress *rps,int *flag)
{
	ERRORCODE errcode;
	HANDLE hFF;
	TCHAR buf[VFPS];

	errcode = GetLastError();

	if ( (cinfo->path[0] == '\\') && ((errcode == ERROR_NETNAME_DELETED) || (errcode == ERROR_ACCESS_DENIED)) ){
		const TCHAR *dirname;

		dirname = FindLastEntryPoint(cinfo->path);
		buf[0] = '\0';
		if ( !tstrcmp(dirname,T("Documents and Settings")) ){
			VFSFullPath(buf,T("..\\Users"),cinfo->path);
		}else if ( !tstrcmp(dirname,T("All Users")) ){
			VFSFullPath(buf,T("...\\ProgramData"),cinfo->path);
		}else if ( !tstrcmp(dirname,T("Default User")) ){
			VFSFullPath(buf,T("...\\Default"),cinfo->path);
		}else if ( !tstrcmp(dirname,T("Application Data")) ){
			VFSFullPath(buf,T("..\\AppData\\Roaming"),cinfo->path);
		}else if ( !tstrcmp(dirname,T("Local Settings")) ){
			VFSFullPath(buf,T("..\\AppData\\Local"),cinfo->path);
		}else if ( !tstrcmp(dirname,T("Start Menu")) ){
			VFSFullPath(buf,T("..\\Roaming\\Microsoft\\Windows\\Start Menu"),cinfo->path);
		}
		if ( *buf != '\0' ){
			hFF = FindFirstAsync(cinfo->info.hWnd,
					cinfo->LoadCounter,buf,&CELdata(0).f,
					&cinfo->e.Dtype,flag);
			if ( hFF != INVALID_HANDLE_VALUE ){
				tstrcpy(cinfo->path,buf);
				tstrcpy(cinfo->RealPath,cinfo->path);
				SetPopMsg(cinfo,POPMSG_MSG,MES_JPLN);
				return hFF;
			}
		}
	}

	if ( (errcode == ERROR_ACCESS_DENIED) || (errcode == ERROR_SYMLINK_CLASS_DISABLED) ){
		DWORD atr;

		atr = GetFileAttributesL(cinfo->path);

		if ( (atr != BADATTR) && (atr & FILE_ATTRIBUTE_REPARSE_POINT) ){
			TCHAR ReparsePath[VFPS];

			GetReparsePath(cinfo->path,ReparsePath);
			if ( *ReparsePath != '\0' ){
				if ( (cinfo->path[0] == '\\') && (ReparsePath[1] == ':') ){
					VFSFullPath(ReparsePath,ReparsePath + 2,cinfo->path);
				}

				hFF = FindFirstAsync(cinfo->info.hWnd,
						cinfo->LoadCounter,ReparsePath,&CELdata(0).f,
						&cinfo->e.Dtype,flag);
				if ( hFF != INVALID_HANDLE_VALUE ){
					tstrcpy(cinfo->path, ReparsePath);
					tstrcpy(cinfo->RealPath, ReparsePath);
					SetPopMsg(cinfo, POPMSG_MSG, MES_JPLN);
					return hFF;
				}
			}
		}
	}
	/*	サブディレクトリ内でファイルが見つからない場合、上位ディレクトリに
		移動できるか試す。
	*/
	if ( (!(*flag & RENTRY_NOFIXDIR) && (cinfo->ChdirLock == FALSE)) &&
		 ( (errcode == ERROR_PATH_NOT_FOUND) ||
		   (errcode == ERROR_BAD_NETPATH) ||
		   (errcode == ERROR_DIRECTORY) ||
		   (errcode == ERROR_FILE_EXISTS) ||
		   ((*flag & RENTRY_ENTERSUB) && IsTouchMessage()) ||
		   ((errcode == ERROR_FILE_NOT_FOUND) && (*flag & RENTRY_DRIVEFIX))) ){
		int mode;
		TCHAR *root;
		TCHAR *p;

		tstrcpy(buf,cinfo->path);
		p = VFSGetDriveType(buf,&mode,NULL);

		if ( (p != NULL) && !(*flag & RENTRY_NOFIXDIR) ){
			if ( *p == '\\' ) p++;
			if ( *p != '\0' ){
				if ( mode == VFSPT_UNC ){
					TCHAR *sharesep;

					sharesep = FindPathSeparator(p);
					if ( sharesep != NULL ){
						sharesep = FindPathSeparator(sharesep + 1);
						if ( (sharesep != NULL) && *(sharesep + 1) ){
							p = sharesep;
						}
					}
				}
				root = p;
				while ( *root != '\0' ){
					TCHAR *lastsep;

					if ( GetTickCount() > (rps->OldTime + dispNwait) ){
						lastsep = root;
					}else{
						lastsep = FindLastEntryPoint(root);
					}
					tstrcpy(cinfo->Jfname,lastsep);
					if ( lastsep > root ) lastsep--;
					*lastsep = '\0';
					hFF = FindFirstAsync(cinfo->info.hWnd,
							cinfo->LoadCounter,buf,&CELdata(0).f,
							&cinfo->e.Dtype,flag);
					if ( hFF != INVALID_HANDLE_VALUE ){
						TCHAR *sep;

						if ( (mode == VFSPT_UNC) && (*(root + 1) == '\0') ){
							*root = '\0';
						}
						tstrcpy(cinfo->path,buf);
						tstrcpy(cinfo->RealPath,buf);
						if ( *flag & RENTRY_ENTERSUB ){
							SetPopMsg(cinfo,errcode,NULL);
						}else{
							SetPopMsg(cinfo,POPMSG_MSG,*root ? MES_FUPD : MES_FUPR );
						}
						sep = FindPathSeparator(cinfo->Jfname);
						if ( sep != NULL ) *sep = '\0';
						setflag(*flag,RENTRY_JUMPNAME);
						return hFF;
					}
				}
			}
		}
	}
	// 失敗
	// ERROR_NO_MORE_FILES も含む？
	if ( errcode == ERROR_FILE_NOT_FOUND ){
		setflag(*flag,RENTRYI_GETFREESIZE);
	}
	if ( PPErrorMsg(buf,errcode) == NO_ERROR ){
		errcode = ERROR_FILE_NOT_FOUND;
		tstrcpy(buf,MessageText(StrNoEntries));
	}
	SetDummyCell(&CELdata(0),buf);
	setflag(CELdata(0).attr,ECA_PARENT);
	CELdata(0).f.nFileSizeLow = errcode;
	if ( rdirmask & ECAX_FORCER ){
		cinfo->e.cellIMax = 0;
		cinfo->e.cellDataMax = 0;
		if ( !(rdirmask & ECA_THIS) ){
			SetDummyRelativeDir(cinfo, T("."), ECA_DIR | ECA_THIS,ECT_THISDIR);
		}
		if ( !(rdirmask & ECA_PARENT) ){
			SetDummyRelativeDir(cinfo, T(".."), ECA_DIR | ECA_PARENT,ECT_UPDIR);
		}
		cinfo->e.cellDataMax++;
		cinfo->e.cellIMax++;
	}
	// CELt(0) = 0; 前で処理済み
	//ACCEPTRELOADMODE_NEW;
	return INVALID_HANDLE_VALUE;
}

// 比較関数群 =================================================================
const TCHAR *GetColumnExtString(COMPARESTRUCT *cs, ENTRYCELL *cell)
{
	COLUMNDATASTRUCT *cdsptr;
	PPC_APPINFO *cinfo;
	DWORD itemindex;

	if ( cell->cellcolumn < 0 ) return NilStr; // 未取得

	cinfo = cs->cinfo;
	itemindex = cs->columnID;

	cdsptr = (COLUMNDATASTRUCT *)(BYTE *)(cinfo->ColumnData.bottom + cell->cellcolumn);

	for ( ; ; ){
		if ( cdsptr->itemindex == (WORD)itemindex ){
			if ( cdsptr->textoffset ){
				return (TCHAR *)(BYTE *)(cinfo->ColumnData.bottom + cdsptr->textoffset);
			}else{
				return NilStr; // まだ読み込んでいない
			}
		}
		if ( cdsptr->nextoffset == 0 ) return NilStr;
		cdsptr = (COLUMNDATASTRUCT *)(BYTE *)(cinfo->ColumnData.bottom + cdsptr->nextoffset);
	}
}
//------------------------------------------------------------------ カラム拡張
int USEFASTCALL CmpColumnExt(COMPARESTRUCT *cs)
{
	return tstricmp(GetColumnExtString(cs, cs->SrcCell),
			GetColumnExtString(cs, cs->DstCell));
}

int USEFASTCALL NumberCompareString(COMPARESTRUCT *cs,int srcdelta,int dstdelta)
{
	UTCHAR *src,*dst;
	UTCHAR *srcbase,*dstbase;
	UTCHAR *srclast,*dstlast;

	src = srcbase = (UTCHAR *)(cs->SrcCell->f.cFileName + srcdelta);
	srclast = (UTCHAR *)(cs->SrcCell->f.cFileName + cs->SrcCell->ext);
	dst = dstbase = (UTCHAR *)(cs->DstCell->f.cFileName + dstdelta);
	dstlast = (UTCHAR *)(cs->DstCell->f.cFileName + cs->DstCell->ext);

	for ( ; ; ){
		int result;

		DWORD c1,c2;
		DWORD n1,n2;

		// 非数字部分(片方だけ数字も含む)の比較
		while ( (src < srclast) && (dst < dstlast) &&
				!(Isdigit(*src) && Isdigit(*dst)) ){
			src++;
			dst++;
		}

		result = CompareString(LOCALE_USER_DEFAULT,cs->option & ~XNORM_MASK,
				(TCHAR *)srcbase,src - srcbase,
				(TCHAR *)dstbase,dst - dstbase) - 2;
		if ( result != 0 ) return result;

		// 末尾に到達しているかを確認
		if ( src >= srclast ){
			return (dst >= dstlast) ? 0 : -1; // 両方末尾 : src のみ末尾
		}else if ( dst >= dstlast ){
			return 1; // dst のみ末尾
		}

		// 数字の比較
		n1 = n2 = 0;
		c1 = *src;
		do {
			n1 = n1 * 10 + (c1 - (UTCHAR)'0');
			c1 = *(++src);
		}while( Isdigit(c1) );

		c2 = *dst;
		do {
			n2 = n2 * 10 + (c2 - (UTCHAR)'0');
			c2 = *(++dst);
		}while( Isdigit(c2) );
		if ( n1 != n2 ) return n1 - n2;

		srcbase = src;
		dstbase = dst;
	}
}

int USEFASTCALL LastEntryCompareString(COMPARESTRUCT *cs)
{
	TCHAR *src,*dst;

	src = FindLastEntryPoint(cs->SrcCell->f.cFileName);
	dst = FindLastEntryPoint(cs->DstCell->f.cFileName);

	if ( cs->option & XNORM_NUMBER ){
		return NumberCompareString(cs,
				src - cs->SrcCell->f.cFileName,
				dst - cs->DstCell->f.cFileName);
	}

	return CompareString(LOCALE_USER_DEFAULT,cs->option & ~XNORM_MASK,
			src,cs->SrcCell->ext - (src - cs->SrcCell->f.cFileName),
			dst,cs->DstCell->ext - (dst - cs->DstCell->f.cFileName)) - 2;
}

//-------------------------------------------------------------------- ":" 専用
int USEFASTCALL CmpRootNameExt(COMPARESTRUCT *cs)
{
	int i;

	i = ((cs->SrcCell->f.cFileName[0] == '\\') ? 1 : 0) -
		((cs->DstCell->f.cFileName[0] == '\\') ? 1 : 0);
	if ( i != 0 ) return i;
	i = -((cs->SrcCell->f.cFileName[1] == ':') ? 1 : 0)
		+((cs->DstCell->f.cFileName[1] == ':') ? 1 : 0);
	if ( i != 0 ) return i;
	if ( (cs->SrcCell->f.cFileName[1] == ':') &&
		 (cs->DstCell->f.cFileName[1] == ':')){
		TCHAR schar,dchar;

		schar = cs->SrcCell->f.cFileName[2];
		dchar = cs->DstCell->f.cFileName[2];
		if ( (schar == '>') ||
			 ((schar == '\\') && (cs->SrcCell->f.cFileName[3] == '>')) ){
			schar = '\0';
		}
		if ( (dchar == '>') ||
			 ((dchar == '\\') && (cs->DstCell->f.cFileName[3] == '>')) ){
			dchar = '\0';
		}
		i = (schar ? 1 : 0) - (dchar ? 1 : 0);
		if ( i ) return i;
		if ( schar != '\0' ){
			i = (cs->SrcCell->f.cFileName[3] ? 1 : 0) -
				(cs->DstCell->f.cFileName[3] ? 1 : 0);
			if ( i ) return i;
		}
	}
	if ( cs->option & XNORM_NUMBER ) return NumberCompareString(cs, 0, 0);

	return CompareString(LOCALE_USER_DEFAULT, cs->option & ~XNORM_MASK,
		cs->SrcCell->f.cFileName,-1,
		cs->DstCell->f.cFileName,-1) - 2;
}

//-------------------------------------------------------------------- 拡張子順
int USEFASTCALL CmpExt(COMPARESTRUCT *cs)
{
	return tstricmp(cs->SrcCell->f.cFileName + cs->SrcCell->ext,
					cs->DstCell->f.cFileName + cs->DstCell->ext);
}
//---------------------------------------------------------------------- 名前順
int USEFASTCALL CmpName(COMPARESTRUCT *cs)
{
	if ( cs->option & XNORM_MASK ){
		if ( cs->option & XNORM_LASTENTRY ) return LastEntryCompareString(cs);
		return NumberCompareString(cs,0,0);
	}
	return CompareString(LOCALE_USER_DEFAULT,cs->option,
				cs->SrcCell->f.cFileName,cs->SrcCell->ext,
				cs->DstCell->f.cFileName,cs->DstCell->ext) - 2;
}
//-------------------------------------------------------------------- 大きい順
int USEFASTCALL CmpSize(COMPARESTRUCT *cs)
{
	if ( cs->SrcCell->f.nFileSizeHigh == cs->DstCell->f.nFileSizeHigh ){
		if (cs->SrcCell->f.nFileSizeLow <
			cs->DstCell->f.nFileSizeLow) return -1;
		if(cs->SrcCell->f.nFileSizeLow >
			cs->DstCell->f.nFileSizeLow) return 1;
		return 0;
	}
	if (cs->SrcCell->f.nFileSizeHigh < cs->DstCell->f.nFileSizeHigh) return -1;
	return 1;
}
//------------------------------------------------------------------ 最新更新順
int USEFASTCALL CmpTime(COMPARESTRUCT *cs)
{
	return FuzzyCompareFileTime0(&cs->SrcCell->f.ftLastWriteTime,
							&cs->DstCell->f.ftLastWriteTime);
}
//------------------------------------------------------------------ 最新作成順
int USEFASTCALL CmpCTime(COMPARESTRUCT *cs)
{
	return FuzzyCompareFileTime0(&cs->SrcCell->f.ftCreationTime,
							&cs->DstCell->f.ftCreationTime);
}
//------------------------------------------------------------------ 最新参照順
int USEFASTCALL CmpATime(COMPARESTRUCT *cs)
{
	return FuzzyCompareFileTime0(&cs->SrcCell->f.ftLastAccessTime,
							&cs->DstCell->f.ftLastAccessTime);
}
//-------------------------------------------------------------------- マーク順
int USEFASTCALL CmpMark(COMPARESTRUCT *cs)
{
	int s,d;
	s = !IsCellPtrMarked(cs->SrcCell);
	d = !IsCellPtrMarked(cs->DstCell);
	return s - d;
}
//---------------------------------------------------------------------- 変更順
int USEFASTCALL CmpChanged(COMPARESTRUCT *cs)
{
	int s,d;

	s = cs->SrcCell->state;
	if ( cs->SrcCell->attr & ECA_GRAY ) s = ECS_GRAY;
	d = cs->DstCell->state;
	if ( cs->DstCell->attr & ECA_GRAY ) d = ECS_GRAY;
	return s - d;
}
//------------------------------------------------------------------ コメント順
int USEFASTCALL CmpComment(COMPARESTRUCT *cs)
{
	if ( cs->SrcCell->comment == cs->DstCell->comment ) return 0;
	if ( cs->SrcCell->comment == EC_NOCOMMENT ) return 1;
	if ( cs->DstCell->comment == EC_NOCOMMENT ) return -1;
	return CompareString(LOCALE_USER_DEFAULT,cs->option & ~XNORM_MASK,
		ThPointerT(&cs->cinfo->EntryComments,cs->SrcCell->comment),-1,
		ThPointerT(&cs->cinfo->EntryComments,cs->DstCell->comment),-1) - 2;
}
//------------------------------------------------------------------ 読み込み順
int USEFASTCALL CmpOriginal(COMPARESTRUCT *cs)
{
	return cs->SrcCell - cs->DstCell;
}
//------------------------------------------------------------------ 拡張子色順
int USEFASTCALL CmpExtcolor(COMPARESTRUCT *cs)
{
	return cs->SrcCell->extC - cs->DstCell->extC;
}
//---------------------------------------------------------- ディレクトリ属性順
int USEFASTCALL CmpDirectory(COMPARESTRUCT *cs)
{
	return (cs->DstCell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) -
			(cs->SrcCell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}
// 以下、上記各関数の反転版 ---------------------------------------------------
int USEFASTCALL CmpRName(COMPARESTRUCT *cs)
{
	return -CmpName(cs);
}
int USEFASTCALL CmpRExt(COMPARESTRUCT *cs)
{
	return -CmpExt(cs);
}
int USEFASTCALL CmpRSize(COMPARESTRUCT *cs)
{
	return -CmpSize(cs);
}
int USEFASTCALL CmpRTime(COMPARESTRUCT *cs)
{
	return -CmpTime(cs);
}
int USEFASTCALL CmpRCTime(COMPARESTRUCT *cs)
{
	return -CmpCTime(cs);
}
int USEFASTCALL CmpRATime(COMPARESTRUCT *cs)
{
	return -CmpATime(cs);
}
int USEFASTCALL CmpRMark(COMPARESTRUCT *cs)
{
	return -CmpMark(cs);
}
int USEFASTCALL CmpRChanged(COMPARESTRUCT *cs)
{
	return -CmpChanged(cs);
}
int USEFASTCALL CmpRComment(COMPARESTRUCT *cs)
{
	return -CmpComment(cs);
}
int USEFASTCALL CmpRExtcolor(COMPARESTRUCT *cs)
{
	return -CmpExtcolor(cs);
}
int USEFASTCALL CmpRDirectory(COMPARESTRUCT *cs)
{
	return -CmpDirectory(cs);
}
int USEFASTCALL CmpRColumnExt(COMPARESTRUCT *cs)
{
	return -CmpColumnExt(cs);
}
// 属性のみ -------------------------------------------------------------------
#pragma argsused
int USEFASTCALL CmpAttributes(COMPARESTRUCT *cs)
{
	UnUsedParam(cs);

	return 0;
}
// ----------------------------------------------------------------------------
CMPFUNCSTRUCT CmpFuncs[SORTFUNCTYPES] = {
	{CmpName},			//  0
	{CmpExt},			//  1
	{CmpSize},			//  2
	{CmpTime},			//  3
	{CmpCTime},			//  4
	{CmpATime},			//  5
	{CmpMark},			//  6
	{CmpChanged},		//  7
	{CmpRName},			//  8
	{CmpRExt},			//  9
	{CmpRSize},			// 10
	{CmpRTime},			// 11
	{CmpRCTime},		// 12
	{CmpRATime},		// 13
	{CmpRMark},			// 14
	{CmpRChanged},		// 15
	{CmpAttributes},	// 16
	{CmpComment},		// 17
	{CmpRComment},		// 18
	{CmpOriginal},		// 19
	{CmpExtcolor},		// 20
	{CmpRExtcolor},		// 21
	{CmpDirectory},		// 22
	{CmpRDirectory},	// 23

	{CmpColumnExt},		// 24～42,44 column拡張
	{CmpRColumnExt},	// 25～43,45 column拡張
};
// 複数段ソート ---------------------------------------------------------------
int USEFASTCALL CmpMulti(COMPARESTRUCT *cs)
{
	int result;

	result = cs->SortF1(cs);
	if ( result ) return result;
	result = cs->SortF2(cs);
	if ( result ) return result;
	if ( cs->SortF3 != NULL ) return cs->SortF3(cs);
	return 0;
}

/*-----------------------------------------------------------------------------
	ソート優先順
								FILE_ATTRIBUTE_SYSTEM(B2→7)
								FILE_ATTRIBUTE_ARCHIVE(B5)
								FILE_ATTRIBUTE_LABEL(B3)
								FILE_ATTRIBUTE_DIRECTORY(B4→2)
								FILE_ATTRIBUTE_HIDDEN(B1)
								FILE_ATTRIBUTE_READONLY(B0)
-----------------------------------------------------------------------------*/
int USEFASTCALL CmpFunc(int di,COMPARESTRUCT *cs)
{
	ENTRYCELL *celld;
	DWORD dt;
											// 属性の比較 ---------------------
	cs->DstCell = celld = &CsCEL(di);
	if ( celld->attr & (ECA_THIS | ECA_PARENT) ) return 1;
	dt = celld->attrsortkey - cs->SrcCell->attrsortkey;
	if ( dt ) return dt;
											// 比較関数による比較 -------------
	return cs->SortF(cs);
}

// 二分挿入アルコニズムによるソート＆登録処理 ---------------------------------
int SortCmp(ENTRYDATAOFFSET cellno, ENTRYINDEX cellmax, COMPARESTRUCT *cs)
{
	ENTRYCELL *cells;
	ENTRYINDEX dh, dl, c;

	if ( cellmax == 0 ) return 0; // 始めてなのでソート不要
											// 比較の準備 ---------------------
	cs->SrcCell = cells = &CsCELdata(cellno);
	if ( cells->attr & (ECA_PARENT | ECA_THIS) ){
		if ( cells->attr & ECA_THIS ){ // . は必ず先頭に
			dl = 0;
		}else{ // .. は . の有無で位置を決定
			dl = ( CsCEL(0).attr & ECA_THIS ) ? 1 : 0;
		}
		goto insert;
	}
											// 末尾との比較 -------------------
	dh = cellmax - 1;
	if ( CmpFunc(dh,cs) >= 0 ) return cellmax;
											// 二分検索 -----------------------
	dl = 0;
	while ( (dh - dl) > 1 ){
		c = (dh + dl) >> 1;
		if ( CmpFunc(c,cs) < 0 ){
			dh = c;
		}else{
			dl = c;
		}
	}
	if ( ((dh - dl) == 1) && ( CmpFunc(dl,cs) >= 0) ) dl++;
insert:										// 挿入 ---------------------------
	if ( dl < cellmax ){
		memmove(&CsCELt(dl + 1), &CsCELt(dl), sizeof(ENTRYINDEX) * (cellmax - dl));
	}
	CsCELt(dl) = cellno;
	return dl;
}

CMPFUNCSTRUCT * USEFASTCALL GetColumnSort(COMPARESTRUCT *cs,char sortmode)
{
	if ( sortmode < SORT_COLUMNTYPE ) return &CmpFuncs[sortmode];
	if ( sortmode < SORTTYPES ){ // 拡張コメント
		cs->columnID = DFC_COMMENTEX_MAX - (((sortmode - (SORTFUNCTYPES - 2)) >> 1));
	}else{
		cs->columnID = cs->option;
	}
	return &CmpFuncs[SORTFUNC_COLUMN_UP + ((sortmode - (SORTFUNCTYPES - 2)) & 1) ];
}

BOOL InitSort(COMPARESTRUCT *cs,XC_SORT *xs)
{
	char sortmode;

	if ( !(xs->mode.block & 0xffff00) ){ // 旧設定([1][2]共に 0)の対策
		xs->mode.block |= 0xffff00;
	}
	cs->MaskAtr = xs->atr;
	cs->option  = xs->option;

	sortmode = xs->mode.dat[0];
	if ( (sortmode < 0) || (sortmode >= SORTFULLTYPES) ){
		cs->SortF = NULL;
		return FALSE;
	}
	cs->SortF = GetColumnSort(cs,sortmode)->f;

	if ( (xs->mode.dat[1] < 0) || (xs->mode.dat[1] > SORTFULLTYPES) ) return TRUE;
	if ( xs->mode.dat[0] == xs->mode.dat[1] ) return TRUE;
	cs->SortF1 = cs->SortF;
	cs->SortF2 = GetColumnSort(cs,xs->mode.dat[1])->f;
	cs->SortF  = CmpMulti;
	if ( (xs->mode.dat[2] < 0) || (xs->mode.dat[2] > SORTFULLTYPES) ){
		cs->SortF3 = NULL;
	}else{
		cs->SortF3 = GetColumnSort(cs,xs->mode.dat[2])->f;
	}
	return TRUE;
}

void MergeCellSort(COMPARESTRUCT *cs,const ENTRYINDEX min,const ENTRYINDEX max)
{
	ENTRYINDEX center,range,orgi,worki;
	ENTRYINDEX *table,*destp;
	ENTRYDATAOFFSET *worktbl;
										// 細切れ処理
	center = (min + max) / 2;
	if ( min < (center - 1)) MergeCellSort(cs,min,center);
	if ( center < (max - 1)) MergeCellSort(cs,center,max);
										// マージ本体
	table = (ENTRYINDEX *)cs->cinfo->e.INDEXDATA.p;
	range = center - min;
	if ( range ){	// 前半をワークへ
		ENTRYINDEX i;
		ENTRYINDEX *dp,*sp;

		sp = table + min;
		dp = cs->worktbl;
		i = range;
		do {
			*dp++ = *sp++;
		}while( --i );
	}
	worktbl = cs->worktbl;	// マージ
	orgi = center;
	worki = 0;
	destp = table + min;
	while ( orgi < max ){
		cs->SrcCell = &CsCELdata(worktbl[worki]);
		if ( CmpFunc(orgi,cs) <= 0 ){
			*destp++ = worktbl[worki++];
			if ( worki >= range ) return;
		}else{
			*destp++ = table[orgi++];
		}
	}
	if ( worki < range ){	// あまりを戻す
		ENTRYINDEX i;
		ENTRYINDEX *sp;

		sp = worktbl + worki;
		i = range - worki;
		do {
			*destp++ = *sp++;
		}while( --i );
	}
}

// [S]ort 本体 ----------------------------------------------------------------
void CellSort(PPC_APPINFO *cinfo, XC_SORT *xs)
{
	COMPARESTRUCT cs;
	ENTRYINDEX min,i;

	cs.cinfo = cinfo;
	if ( FALSE == InitSort(&cs, xs) ) return;
	cinfo->sort_nowmode.block = xs->mode.block;

	if ( cinfo->e.cellIMax > SORTINFOSIZE ){
		SetPopMsg(cinfo,POPMSG_NOLOGMSG,SortingStr);
		UpdateWindow_Part(cinfo->info.hWnd);
	}

	if ( xs->mode.dat[0] >= SORT_COLUMNTYPE ){
		PreExecuteSort(cinfo,xs->mode.dat[0]);
	}

	min = 0;
	while ( CEL(min).attr & (ECA_PARENT | ECA_THIS) ){
		min++;
		if ( min >= cinfo->e.cellIMax ){
			return;
		}
	}
	for ( i = min ; i < cinfo->e.cellIMax ; i++ ){
		DWORD asortkey;

		asortkey = CEL(i).f.dwFileAttributes & cs.MaskAtr;
		if ( asortkey & FILE_ATTRIBUTE_SYSTEM ){
											// B7=1 & FILE_ATTRIBUTE_SYSTEM=0
			asortkey ^= B7 | FILE_ATTRIBUTE_SYSTEM;
		}
		if ( asortkey & FILE_ATTRIBUTE_DIRECTORY ){
			asortkey ^= FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY;
					// FILE_ATTRIBUTE_SYSTEM=1 & FILE_ATTRIBUTE_DIRECTORY=0;
		}
		CEL(i).attrsortkey = (BYTE)asortkey;
	}

	if ( cinfo->e.cellIMax > 4000 ){
		cs.worktbl = PPcHeapAlloc( ( cinfo->e.cellIMax / 2 + 1 ) * sizeof(ENTRYINDEX));
		MergeCellSort(&cs,min,cinfo->e.cellIMax);
		PPcHeapFree(cs.worktbl);

		if ( cinfo->e.cellIMax > SORTINFOSIZE ){
			StopPopMsg(cinfo,PMF_DISPLAYMASK);
		}
	}else{
		for ( i = min + 1 ; i < cinfo->e.cellIMax ; i++ ){
			SortCmp(CELt(i), i, &cs);
		}
	}
}

#if 0
typedef struct {
	PPXAPPINFO info;
	PPC_APPINFO *parent;
} ALLEXECUTESTRUCT;

DWORD_PTR USECDECL AllExecuteFunc(ALLEXECUTESTRUCT *aes,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case PPXCMDID_STARTENUM:	// 検索開始(マーク無しもあり)
		case PPXCMDID_STARTNOENUM:	// 検索開始(マーク無しはなし)
			uptr->enums.enumID = 0;
			break;

		case PPXCMDID_NEXTENUM:		// 次へ
			if ( uptr->enums.enumID < 0 ) return 0;
			if ( uptr->enums.enumID < aes->parent->e.cellDMax ){
				uptr->enums.enumID++;
				break;
			}
		// PPXCMDID_ENDENUM へ
		case PPXCMDID_ENDENUM:		//列挙終了
			uptr->enums.enumID = -1;
			return 0;

		// その他は親に任せる
		default:
			return aes->parent->info.Function(&aes->parent->info,cmdID,uptr);
	}
	return 1;
}
#endif

void ExtCommentExecute(PPC_APPINFO *cinfo,DWORD id)
{
	TCHAR key[64],param[CMDLINESIZE];

	wsprintf(key,T("COMMENTEVENT%d"),id);
	param[0] = '\0';

	GetCustTable(StrKC_main,key,&param,sizeof(param));
	if ( param[0] != '\0' ){
		ExecDualParam(cinfo,param);
#if 0
		if ( (UTCHAR)param[0] == EXTCMD_CMD ){
			ALLEXECUTESTRUCT info;

			info.info = cinfo->info;
			info.info.Function = (PPXAPPINFOFUNCTION)AllExecuteFunc;
			info.parent = cinfo;
			PP_ExtractMacro(cinfo->info.hWnd,&info.info,NULL,param + 1,NULL,0);
		}else{
			ExecDualParam(cinfo,param);
		}
#endif
	}
}

void PreExecuteSort(PPC_APPINFO *cinfo,BYTE sortid)
{
	sortid = (BYTE)((sortid - SORT_COLUMNTYPE + 2) / 2); // 1～の値にする
	if ( cinfo->UseCommentsFlag & (1 << sortid) ) return; // 読み込み済み
	setflag(cinfo->UseCommentsFlag,1 << sortid);
	if ( cinfo->edit.ref > 0 ){
		EndCellEdit(cinfo);
		ExtCommentExecute(cinfo,sortid);
		StartCellEdit(cinfo);
	}else{
		ExtCommentExecute(cinfo,sortid);
	}
}

void SetSlowMode(PPC_APPINFO *cinfo)
{
	cinfo->SlowMode = TRUE;
	if ( cinfo->dset.infoicon >= DSETI_EXTONLY ){
		cinfo->dset.infoicon = DSETI_BOX;
	}
	if ( cinfo->dset.cellicon >= DSETI_EXTONLY ){
		cinfo->dset.cellicon = DSETI_BOX;
	}
}

// Cell 情報を作成し、保存 ----------------------------------------------------
void SetCellInfo(PPC_APPINFO *cinfo,ENTRYCELL *cell,BYTE *extcolor)
{
	TCHAR *extsp;

	cell->comment = EC_NOCOMMENT;
	if ( cinfo->e.Dtype.mode == VFSDT_LFILE ){
		if ( cell->f.dwFileAttributes == FILE_ATTRIBUTEX_MESSAGE ){
			cell->state = ECS_MESSAGE;
			cell->type  = ECT_SYSMSG;
			cell->extC = C_AUTO;
			cell->ext = (WORD)tstrlen(cell->f.cFileName);
			cell->attr = ECA_ETC;
			return;
		}

		if ( cell->f.dwFileAttributes & FILE_ATTRIBUTEX_LF_COMMENT ){
			const TCHAR *p;

			cell->comment = cinfo->EntryComments.top;
			ThSize(&cinfo->EntryComments,CMDLINESIZE);
			p = (const TCHAR *)cell->f.dwReserved0; // dwReserved0/1 を使用
			GetCommandParameter(&p,(TCHAR *)ThLast(&cinfo->EntryComments),CMDLINESIZE);
			cinfo->EntryComments.top += TSTRSIZE((TCHAR *)ThLast(&cinfo->EntryComments));
		}

		if ( cell->f.dwFileAttributes & FILE_ATTRIBUTEX_LF_MARK ){
			if ( !IsCellPtrMarked(cell) ){
				ENTRYINDEX cellTNo = cell - &CELdata(0);

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
				AddDD(cinfo->e.MarkSize.l,cinfo->e.MarkSize.h,
						cell->f.nFileSizeLow,cell->f.nFileSizeHigh);
			}
		}
	}
											// コメント -----------------------
	if ( cinfo->EntryComments.bottom != NULL ){
		TCHAR *p;

		p = (TCHAR *)cinfo->EntryComments.bottom;
		while( *p ){
			TCHAR *cp;

			cp = p + tstrlen(p) + 1;
			if ( (cell->f.cAlternateFileName[0] &&
						!tstricmp(p,cell->f.cAlternateFileName)) ||
				 !tstricmp(p,cell->f.cFileName) ){
				cell->comment =
						TSTROFF32(cp - (TCHAR *)cinfo->EntryComments.bottom);
				break;
			}
			p = cp + tstrlen(cp) + 1;
		}
	}
											// 拡張子仮設定 --------------------
	cell->ext = (WORD)tstrlen(cell->f.cFileName);
	cell->extC = C_defextC;
												// 特殊な物の判別 -------------
	if ( cell->f.cFileName[0] == '.' ){		// 親／自ディレクトリ
		if ( !cell->f.cFileName[1] ){		// This dir
			cell->type = ECT_THISDIR;
			setflag(cell->attr,ECA_DIR | ECA_THIS);
			return;
		}
		if ( (cell->f.cFileName[1] == '.') && !cell->f.cFileName[2] ){
			cell->type = ECT_UPDIR;
			setflag(cell->attr,ECA_DIR | ECA_PARENT);
			return;
		}
											// 拡張子位置 ---------------------
		extsp = cell->f.cFileName + 1;
	}else{
		extsp = cell->f.cFileName;
	}
											// 属性の種類 ---------------------
	{
		DWORD atr;

		atr = cell->f.dwFileAttributes;
		if ( atr & FILE_ATTRIBUTE_SYSTEM ){
			cell->type = ECT_SYSTEM;
		}else if ( atr & FILE_ATTRIBUTE_HIDDEN ){
			cell->type = ECT_HIDDEN;
		}else if ( atr & FILE_ATTRIBUTE_READONLY ){
			cell->type = ECT_RONLY;
		}else if ( atr & FILE_ATTRIBUTE_DIRECTORY ){
			cell->type = ECT_SUBDIR;
		}else if ( atr & FILE_ATTRIBUTE_LABEL ){
			cell->type = ECT_LABEL;
		}else{
			cell->type = ECT_NORMAL;
		}
		if ( atr & FILE_ATTRIBUTE_DIRECTORY ){
			setflag(cell->attr,ECA_DIR);
			if ( cell->f.nFileSizeLow | cell->f.nFileSizeHigh ){
				setflag(cell->attr,ECA_DIRC);
			}
			if ( !XC_sdir ) return;
		}
	}
	{ // 拡張子を求める
		TCHAR type,*src;

		src = extsp;
		extsp = NULL;
		for ( ; ; ){
			type = *src;
			if ( type == '\\' ){
				extsp = NULL;
				if ( *(src + 1) == '.' ) src++; // 先頭「.」をスキップ
			}else if ( type == '.' ){
				extsp = src;
			}else if ( type == '\0' ){
				if ( extsp != NULL ) break;
				return;
			}
#ifndef UNICODE
			if ( Iskanji(type) ) src++;
#endif
			src++;
		}
	}

	if ( (DWORD)(cell->ext - (extsp - cell->f.cFileName)) > X_extl ) return;
	cell->ext = (WORD)(extsp - cell->f.cFileName);
	extsp++;
												// 色 -------------------------
	if ( extcolor != NULL ){
		BYTE *q;			// 現在の内容の先頭
		DWORD ssize;		// 文字列部分の大きさ
		DWORD w;			// (+0)次の内容へのオフセット
		TCHAR ext[MAX_PATH];
		FN_REGEXP fn;

		ssize = TSTRSIZE32(extsp);
		memcpy(ext,extsp,ssize);
		tstrupr(ext);
		q = extcolor;
		for ( ; ; ){
			w = *(WORD *)q;
			if ( w == 0 ) break;
			if ( *(TCHAR *)(BYTE *)(q + sizeof(WORD)) != '/' ){
				if ( !memcmp( q + sizeof(WORD),ext,ssize) ){
					cell->extC = *(DWORD *)(q + sizeof(WORD) + ssize);
					break;
				}
			}else{
				int fnresult;

				MakeFN_REGEXP(&fn,(TCHAR *)(BYTE *)(q + sizeof(WORD)) + 1);
				fnresult = FinddataRegularExpression(&cell->f,&fn);
				FreeFN_REGEXP(&fn);
				if ( fnresult ){
					cell->extC = *(DWORD *)(q + sizeof(WORD) + TSTROFF(tstrlen((TCHAR *)(BYTE *)(q + sizeof(WORD))) + 1) );
					break;
				}
			}
			q += w;
		}
	}
}
											// find 初期化 --------------------
void InitMasks(struct masks *m,XC_MASK *mask,TCHAR *maskstr)
{
	int size;

	if ( (maskstr[0] == '\1') && (maskstr[1] == '\0') ){
		maskstr = mask->file;
		m->Attr = LOWORD(mask->attr);
		m->Dir  = (mask->attr & MASKEXTATTR_DIR) ? 0 : FILE_ATTRIBUTE_DIRECTORY;
	}else{
		m->Attr = 0;
		m->Dir = FILE_ATTRIBUTE_DIRECTORY;
	}

	m->extcolor = NULL;
	if ( MakeFN_REGEXP(&m->FileMask,maskstr) & REGEXPF_PATHMASK ) m->Dir = 0;
	size = GetCustDataSize(T("C_ext"));
	if ( size > 0 ){
		m->extcolor = PPcHeapAlloc(size);
		if ( m->extcolor != NULL ) GetCustData(T("C_ext"),m->extcolor,size);
	}
}

BOOL ReadDirProgress(PPC_APPINFO *cinfo,struct readprogress *r)
{
	DWORD NewTime;

	NewTime = GetTickCount();
	if ( NewTime > (r->OldTime + dispNwait) ){
		if ( r->enablecnt ){
			r->enablecnt--;
		}else{
			if ( IsTrue(ReadDirBreakCheck(cinfo)) ){
				SetPopMsg(cinfo,POPMSG_MSG,MES_BRAK);
				SetLastError(ERROR_MORE_DATA);
				return FALSE;
			}
		}
		r->OldTime = NewTime;
		InitCli(cinfo);
		InvalidateRect(cinfo->info.hWnd,NULL,TRUE);
		UpdateWindow_Part(cinfo->info.hWnd);
	}
	r->dispN += dispNtimes;
	return TRUE;
}

void ClearCellIconImage(PPC_APPINFO *cinfo)
{
	ENTRYDATAOFFSET offset;

	if ( cinfo->EntryIcons.hImage != NULL ){
		EnterCellEdit(cinfo);
		if ( cinfo->EntryIcons.hImage != INVALID_HANDLE_VALUE ){
			DImageList_Destroy(cinfo->EntryIcons.hImage);
		}
		cinfo->EntryIcons.hImage = NULL;

		for ( offset = 0 ; offset < cinfo->e.cellDataMax ; offset++ ){
			CELdata(offset).icon = ICONLIST_NOINDEX;
		}
		ResetDxDrawAtlas(cinfo->DxDraw);
		LeaveCellEdit(cinfo);
	}
}

void InitDirectoryCheck(PPC_APPINFO *cinfo)
{
	if ( (cinfo->e.Dtype.ExtData == INVALID_HANDLE_VALUE) ||
		 (XC_rrt[0] == -1) ||
		 (cinfo->dset.flags & DSET_NODIRCHECK) ){
		setflag(cinfo->SubTCmdFlags,SUBT_STOPDIRCHECK);	// 監視しない
	}else{
		//SetShellChangeNotification(cinfo);
		setflag(cinfo->SubTCmdFlags,SUBT_INITDIRCHECK);	// 監視開始
	}
	SetEvent(cinfo->SubT_cmd);
}

void USEFASTCALL UpdateEntry_Deleted(PPC_APPINFO *cinfo,ENTRYCELL *cell,struct masks *m)
{
	ENTRYINDEX ci,cc;
	cell->state = ECS_ADDED;	// 削除されていたのが追加

	cc = cell - &CELdata(0);
	for ( ci = 0 ; ci < cinfo->e.cellIMax + cinfo->e.cellStack ; ci++ ){
		if ( CELt(ci) == cc ){
			ci = -1;
			break;
		}
	}
	if ( ci != -1 ){
		if ( (cell->attr & (ECA_PARENT | ECA_THIS)) ||
			 ( !(cell->f.dwFileAttributes & m->Attr) &&
				((cell->f.dwFileAttributes & m->Dir) ||
				FinddataRegularExpression(&cell->f,&m->FileMask)))){
			if ( (cell->f.dwFileAttributes &
					FILE_ATTRIBUTE_DIRECTORY) &&
					 !(cell->attr & (ECA_PARENT | ECA_THIS)) ){
				cinfo->e.Directories++;
			}
			if ( cinfo->e.cellStack ){	// スタックがあれば移動
				memmove(&CELt(cinfo->e.cellIMax + 1),&CELt(cinfo->e.cellIMax),
						sizeof(ENTRYINDEX) * (cinfo->e.cellStack) );
			}
			CELt(cinfo->e.cellIMax) = cell - &CELdata(0);
			cinfo->e.cellIMax++;
		}
	}
	cell->attr = 0;
}

#define CellsIsNotSameAttributes(cell1,cell2) \
	(cell1->f.dwFileAttributes != cell2->f.dwFileAttributes)
#define CellsIsNotSameFileSize(cell1,cell2) \
	(!(cell1->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&	\
	 ((cell1->f.nFileSizeLow  != cell2->f.nFileSizeLow ) ||		\
	  (cell1->f.nFileSizeHigh != cell2->f.nFileSizeHigh) ))
#define CellsIsNotSameFileTime(cell1,cell2) \
	((cell1->f.ftLastWriteTime.dwLowDateTime				\
			!= cell2->f.ftLastWriteTime.dwLowDateTime) ||	\
	 (cell1->f.ftLastWriteTime.dwHighDateTime				\
			!= cell2->f.ftLastWriteTime.dwHighDateTime))

#define CellIsDirectory(cell) \
	(cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)

									// ファイルの検索開始 -----(再読込み)
void UpdateEntry(PPC_APPINFO *cinfo,TCHAR *readpath,int *flag,TCHAR *mask)
{
	HANDLE hFF;
	struct masks namemask;
	struct readprogress rps;
	ENTRYDATAOFFSET searchNo = 0,searchStartNo = 0;
	ENTRYDATAOFFSET oldcellMax;		// 元のエントリ数（調査範囲を元の範囲に限定する）
	int flagdata;

	if ( (cinfo->dset.infoicon >= DSETI_OVLNOC) ||
		 (cinfo->dset.cellicon >= DSETI_OVLNOC) ){
		ClearCellIconImage(cinfo);
	}
	cinfo->CellModified = CELLMODIFY_MODIFIED;

	if ( ! (CELdata(0).attr & ECA_GRAY) ){
		ENTRYCELL *cell;
		int i;

		cell = &CELdata(0);
		for ( i = cinfo->e.cellDataMax ; i ; i--,cell++ ){
			setflag(cell->attr,ECA_GRAY);
		}
	}
	if ( cinfo->CellHashType != CELLHASH_NAME ){
		ENTRYCELL *cell;
		int i;

		cell = &CELdata(0);
		for ( i = cinfo->e.cellDataMax ; i ; i--,cell++ ){
			cell->cellhash = MiniHash(cell->f.cFileName);
		}
		cinfo->CellHashType = CELLHASH_NAME;
	}

	cinfo->e.Dtype.mode = VFSDT_UNKNOWN;
	cinfo->e.Dtype.Name[0] = '\0';

	hFF = FindFirstAsync(cinfo->info.hWnd,cinfo->LoadCounter,readpath,
			&CELdata(cinfo->e.cellDataMax).f,&cinfo->e.Dtype,flag);
	cinfo->StateInfo.state = StateID_NoState;
//	if ( cinfo->StateInfo.hStateWnd != NULL ) SendMessage(cinfo->StateInfo.hStateWnd,WM_CLOSE,0,0);
	if ( hFF == INVALID_HANDLE_VALUE ){	// 失敗
		ERRORCODE errorcode;

		errorcode = GetLastError();
		if ( errorcode == ERROR_FILE_NOT_FOUND ){
			setflag(*flag,RENTRYI_GETFREESIZE);
		}
		if ( errorcode != ERROR_BUSY ){
			if ( CEL(0).type == ECT_SYSMSG ){
				PPErrorMsg(CEL(0).f.cFileName,errorcode);
				CEL(0).f.nFileSizeLow = errorcode;
			}
			SetPopMsg(cinfo,POPMSG_MSG,NULL);
			return;
		}
		// ERROR_BUSY : 読み込み中のため、とりあえず戻る
		cinfo->AcceptReload = *flag;
		cinfo->FDirWrite = FDW_REQUESTED;
		setflag(cinfo->SubTCmdFlags,SUBT_REQUESTRELOAD); // 時間経ってから更新
		InitDirectoryCheck(cinfo);	// ディレクトリ書き込みの監視を起動
		return;
	}
	// 更新時は、キャッシュや非同期読み込み中の状態は発生しない
	cinfo->AloadCount = 0;	// 遅延はないので解除
	InitDirectoryCheck(cinfo);	// ディレクトリ書き込みの監視を起動

	rps.dispN = dispNtimes;
	rps.OldTime = GetTickCount();
	rps.enablecnt = ENABLECNTTIME;
									// システムメッセージがあれば削除
	if ( (cinfo->e.cellIMax == 1) && (CEL(0).type == ECT_SYSMSG) ){
		cinfo->e.cellIMax--;
		cinfo->e.cellDataMax--;
		CELdata(cinfo->e.cellDataMax).f = CELdata(cinfo->e.cellDataMax + 1).f;	// 情報の移動
	}
	oldcellMax = cinfo->e.cellDataMax;
	InitMasks(&namemask,&cinfo->mask,mask);
	flagdata = *flag;
	do{
		ENTRYCELL *cell,*chkcell,*chkcellmax;
		DWORD hash;
		int remains;

		chkcellmax = &CELdata(oldcellMax);
		chkcell = &CELdata(cinfo->e.cellDataMax);
		chkcell->attr = 0;
		chkcell->cellhash = hash = MiniHash(chkcell->f.cFileName);
		cell = &CELdata(searchStartNo);
										// 同一かを検索
		for ( remains = (oldcellMax - searchNo) ; remains ; remains-- ){
										// 大文字・小文字を区別して同一か判別
			if ( (cell->cellhash == hash) && !(tstrcmp(chkcell->f.cFileName,cell->f.cFileName)) ){
				ENTRYDATAOFFSET nowNo;

				nowNo = cell - &CELdata(0);
				if ( (nowNo + 1) < oldcellMax ){
					searchStartNo = nowNo + 1; // このcellの次から検索開始
					if ( nowNo == searchNo ) searchNo++;
				}else{ // 最終エントリで見つかった
					searchStartNo = searchNo; // searchNoから検索開始
					if ( nowNo == searchNo ) searchNo++; // ++searchNo == oldcellMax になる
				}

				if ( cell->state == ECS_DELETED ){
					UpdateEntry_Deleted(cinfo,cell,&namemask);
				}else if ( CellsIsNotSameAttributes(chkcell,cell) ||
							CellsIsNotSameFileSize(chkcell,cell) ||
							CellsIsNotSameFileTime(chkcell,cell) ){
					cell->state = ECS_CHANGED;	// 変更あり
					if ( CellIsDirectory(chkcell) && (CellIsDirectory(cell)) ){
						chkcell->f.nFileSizeLow = cell->f.nFileSizeLow;
						chkcell->f.nFileSizeHigh = cell->f.nFileSizeHigh;
						cell->attr = ECA_DIRG;
					}else{
						cell->attr = 0;
					}

					if ( (cinfo->dset.infoicon < DSETI_OVLNOC) &&
						 (cinfo->dset.cellicon < DSETI_OVLNOC) ){
						cell->icon = ICONLIST_NOINDEX;
					}
				}else{							// 変更なし
					cell->attr &= (BYTE)~ECA_GRAY;
					chkcell->f.nFileSizeLow = cell->f.nFileSizeLow;
					chkcell->f.nFileSizeHigh = cell->f.nFileSizeHigh;
					cell->f = chkcell->f;	// 情報の更新
					goto next;
				}
				if ( IsCellPtrMarked(cell) ){ //マーク有りの時は総計を修正
					SubDD(cinfo->e.MarkSize.l,cinfo->e.MarkSize.h,
							cell->f.nFileSizeLow,cell->f.nFileSizeHigh);
					AddDD(cinfo->e.MarkSize.l,cinfo->e.MarkSize.h,
							chkcell->f.nFileSizeLow,chkcell->f.nFileSizeHigh);
				}
				cell->f = chkcell->f;	// 情報の更新
				chkcell = cell;
				break;
			}
			cell++;
			if ( cell == chkcellmax ){
				cell = &CELdata(searchNo);
			}
		}
		SetCellInfo(cinfo,chkcell,namemask.extcolor);
		if ( remains == 0 ){					// なかった→追加
			chkcell->state = ECS_ADDED;
			chkcell->mark_fw = NO_MARK_ID;
			chkcell->mark_bk = NO_MARK_ID;
			chkcell->cellcolumn = CCI_NOLOAD;
			chkcell->icon = ICONLIST_NOINDEX;
#if FREEPOSMODE
			chkcell->pos.x = NOFREEPOS;
#endif
			if ( !(chkcell->attr & rdirmask) ){
				if ( (chkcell->attr & (ECA_PARENT | ECA_THIS)) ||
					 (!(chkcell->f.dwFileAttributes & namemask.Attr) &&
						((chkcell->f.dwFileAttributes & namemask.Dir) ||
						 FinddataRegularExpression(&chkcell->f,&namemask.FileMask)))
				){
					if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
						if ( cell->attr & (ECA_PARENT | ECA_THIS) ){
							cinfo->e.RelativeDirs++;
						}else{
							cinfo->e.Directories++;
						}
					}

					if ( cinfo->e.cellStack ){	// スタックがあれば移動
						memmove(&CELt(cinfo->e.cellIMax+1),&CELt(cinfo->e.cellIMax),
								sizeof(ENTRYINDEX) * (cinfo->e.cellStack) );
					}
					CELt(cinfo->e.cellIMax) = cinfo->e.cellDataMax;
					cinfo->e.cellIMax++;
				}
			}
			cinfo->e.cellDataMax++;
			// ※TM_checkでcellアドレスが変わる場合有
			if ( TM_check(&cinfo->e.CELLDATA,sizeof(ENTRYCELL) * (cinfo->e.cellDataMax + 1) ) == FALSE ){
				break;
			}
			if ( TM_check(&cinfo->e.INDEXDATA,sizeof(ENTRYINDEX) * (cinfo->e.cellDataMax + 1) ) == FALSE ){
				break;
			}
			if ( cinfo->e.cellDataMax >= rps.dispN ){
				if ( ReadDirProgress(cinfo,&rps) == FALSE ) break;
			}
		}
next: ;
	}while( IsTrue(FindNextAsync(hFF,&CELdata(cinfo->e.cellDataMax).f,flagdata)) );
	FreeFN_REGEXP(&namemask.FileMask);
	FindCloseAsync(hFF,flagdata);

	if ( namemask.extcolor != NULL ){
		HeapFree( hProcessHeap,0,namemask.extcolor);
	}

	if ( cinfo->UseArcPathMask != ARCPATHMASK_OFF ){
		tstrcpy(readpath,cinfo->ArcPathMask);
		MaskPathMain(cinfo,cinfo->ArcPathMaskDepth,readpath);
	}

	{								// 未操作のファイル=削除ファイルを処理
		ENTRYCELL *cell;
		int i;

		cell = &CELdata(searchNo);
		for ( i = (oldcellMax - searchNo) ; i ; i--,cell++ ){	// 同一かを検索
			if ( !(cell->attr & ECA_GRAY) ) continue;
			cell->attr &= (BYTE)(~ECA_GRAY);
			if ( cell->attr & (ECA_PARENT | ECA_THIS) ) continue; // ./.. は残す

			cell->state = ECS_DELETED;
			ResetMark(cinfo,cell);
		}
	}
}
							// ファイルの検索開始 -----------（新規）
void ReadEntryMain(PPC_APPINFO *cinfo,TCHAR *readpath,int *flags,TCHAR *mask)
{
	COMPARESTRUCT cs;
	HANDLE hFF;
	TCHAR buf[VFPS];
	int errorcode;
	struct masks namemask;
	struct readprogress rps;
	DWORD sorttime = 0;
	BYTE attr = 0,sortid = 0;
	int flagdata;

	DEBUGLOGC("ReadEntryMain start",0);
	cinfo->CellModified = CELLMODIFY_NONE;
	cinfo->UseCommentsFlag = 0;
							// ディレクトリ情報の初期化
	cinfo->e.Dtype.mode = VFSDT_UNKNOWN;
	cinfo->e.Dtype.Name[0] = '\0';
	cinfo->e.Dtype.ExtData = INVALID_HANDLE_VALUE;
	cinfo->e.Dtype.BasePath[0] = '\0';
	cinfo->e.MarkSize.l = 0;
	cinfo->e.MarkSize.h = 0;
	cinfo->e.cellIMax = 1;
	cinfo->e.cellDataMax = 1;
	cinfo->e.markTop = ENDMARK_ID;
	cinfo->e.markLast = ENDMARK_ID;
	cinfo->e.markC = 0;
	cinfo->e.cellStack = 0;
	cinfo->e.Directories = 0;
	cinfo->e.RelativeDirs = 0;
	cinfo->e.AllRelativeDirs = 0;
	cinfo->CellHashType = CELLHASH_NONE;
#if FREEPOSMODE
	cinfo->FreePosEntries = 0;
#endif
	TM_check(&cinfo->e.CELLDATA,sizeof(ENTRYCELL) * 2);
	TM_check(&cinfo->e.INDEXDATA,sizeof(ENTRYINDEX) * 2);
	SetDummyCell(&CELdata(0),MessageText(MES_LOAD));
	CELt(0) = 0;
	{
		TCHAR *sep = FindPathSeparator(cinfo->path);
		cinfo->PathSeparater = (sep == NULL) ? (TCHAR)'\\' : *sep;
	}

	cinfo->LoadCounter++;
	cinfo->AloadCount = 0;
	if ( cinfo->EntryIcons.hImage != NULL ){
		if ( cinfo->EntryIcons.hImage != INVALID_HANDLE_VALUE ){
			EnterCellEdit(cinfo);
			DImageList_Destroy(cinfo->EntryIcons.hImage);
			LeaveCellEdit(cinfo);
		}
		cinfo->EntryIcons.hImage = NULL;
		ResetDxDrawAtlas(cinfo->DxDraw);
	}

	DEBUGLOGC("ReadEntryMain 2",0);
	tstrcpy(cinfo->RealPath,cinfo->path);
	{
		TCHAR *sep;

		sep = tstrrchr(cinfo->RealPath,':');
		if ( (sep != NULL) &&
			 (sep > (cinfo->RealPath + 1)) &&
			 (*(sep - 1) == ':') ){
			*(sep - 1) = '\0';
		}
	}
	rps.dispN = dispNtimes;
	rps.OldTime = GetTickCount();
	rps.enablecnt = ENABLECNTTIME;

	DEBUGLOGC("ReadEntryMain FindFirstAsync",0);
	hFF = FindFirstAsync(cinfo->info.hWnd,cinfo->LoadCounter,readpath,
			&CELdata(0).f,&cinfo->e.Dtype,flags);
	DEBUGLOGC("ReadEntryMain FindFirstAsync end",0);
	if ( hFF == INVALID_HANDLE_VALUE ){	// 失敗
		cinfo->StateInfo.state = StateID_ReadError;
		hFF = FixReadDirectoryError(cinfo,&rps,flags);
		cinfo->StateInfo.state = StateID_NoState;
//		if ( cinfo->StateInfo.hStateWnd != NULL ) SendMessage(cinfo->StateInfo.hStateWnd,WM_CLOSE,0,0);
		if ( hFF == INVALID_HANDLE_VALUE ) return;
	}
	cinfo->StateInfo.state = StateID_NoState;
//	if ( cinfo->StateInfo.hStateWnd != NULL ) SendMessage(cinfo->StateInfo.hStateWnd,WM_CLOSE,0,0);
	setflag(*flags,RENTRYI_GETFREESIZE);
	if ( cinfo->e.Dtype.mode == VFSDT_SHN ){
		if ( VFSGetRealPath(cinfo->info.hWnd,cinfo->RealPath,cinfo->path)
				== FALSE ){
			tstrcpy(cinfo->RealPath,T("?"));
		}
		if ( tstrcmp(cinfo->e.Dtype.Name,VFSFFTYPENAME_storageSHN) == 0 ){
			if ( cinfo->dset.infoicon > DSETI_EXTONLY ){
				cinfo->dset.infoicon = DSETI_EXTONLY;
			}
			if ( cinfo->dset.cellicon > DSETI_EXTONLY ){
				cinfo->dset.cellicon = DSETI_EXTONLY;
			}
		}
	}

	if ( (cinfo->e.Dtype.mode == VFSDT_UN) ||
		 (cinfo->e.Dtype.mode == VFSDT_SUSIE) ||
		 (cinfo->e.Dtype.mode == VFSDT_CABFOLDER) ||
		 (cinfo->e.Dtype.mode == VFSDT_LZHFOLDER) ||
		 (cinfo->e.Dtype.mode == VFSDT_ZIPFOLDER) ){
		LoadSettingSecond(cinfo,StrArchiveMode,flags,mask);
	}
	{									// sort 初期化 --------------------
		XC_SORT *usexs;

		usexs = (cinfo->XC_sort.mode.dat[0] >= 0) ? &cinfo->XC_sort : &cinfo->dset.sort;
		cinfo->sort_nowmode.block = usexs->mode.block;
		cs.cinfo = cinfo;
		InitSort(&cs,usexs);
		if ( cinfo->path[0] == ':' ) cs.SortF = CmpRootNameExt;
		if ( usexs->mode.dat[0] >= SORT_COLUMNTYPE ){
			sorttime = MAX32;
			sortid = usexs->mode.dat[0];
		}
	}

	InitDirectoryCheck(cinfo);	// ディレクトリ書き込みの監視を起動 =======

	cinfo->RequestVolumeLabel = TRUE;

	flagdata = *flags;
	if ( flagdata & RENTRYI_NOREADCOMMENT ){
		cinfo->e.Dtype.mode = VFSDT_PATH;	// 臨時
	}else{
		BOOL readcomment = TRUE;

		if ( cinfo->RealPath[0] == '?' ) readcomment = FALSE;
		if ( cinfo->path[0] == '\\' ){
			TCHAR *p;

			p = FindPathSeparator(cinfo->path + 2);
			// \\name か \\name\\path
			if ( (p == NULL) || (NULL == FindPathSeparator(p + 1)) ){
				readcomment = FALSE;
			}
		}

		cinfo->ModifyComment = FALSE;
		ThFree(&cinfo->EntryComments);
		ThFree(&cinfo->ColumnData);
		CatPath(cinfo->CommentFile,cinfo->RealPath,T("00_INDEX.TXT"));
		if ( cinfo->e.Dtype.ExtData != INVALID_HANDLE_VALUE ){
			if ( readcomment ){
				GetCommentText(&cinfo->EntryComments,cinfo->CommentFile);
				if ( cinfo->EntryComments.bottom != NULL ){
					setflag(cinfo->UseCommentsFlag,B0);
				}
			}
		}
	}
	InitMasks(&namemask,&cinfo->mask,mask);

	cinfo->e.cellIMax = 0;
	cinfo->e.cellDataMax = 0;
		// 非同期読み込みのキャッシュ使用時は、グレー表示＆低速モード
	if ( cinfo->e.Dtype.ExtData == INVALID_HANDLE_VALUE ){
		attr = ECA_GRAY;
		cinfo->SlowMode = TRUE; // SetSlowMode() は使用しない。後で元に戻すから
		cinfo->AcceptReload = flagdata;
	}else{
		cinfo->LoadCounter++; // 読み込めた。誤って更新が来ても良いようにカウントを変更
	}

	if ( (rdirmask & ECAX_FORCER) &&
		 !IsRelativeDir(CELdata(0).f.cFileName) &&
		 (cinfo->e.Dtype.mode != VFSDT_DLIST) ){
		if ( !(rdirmask & ECA_THIS) ){
			SetDummyRelativeDir(cinfo,T("."),ECA_DIR | ECA_THIS,ECT_THISDIR);
		}
		if ( !(rdirmask & ECA_PARENT) ){
			SetDummyRelativeDir(cinfo,T(".."),ECA_DIR | ECA_PARENT,ECT_UPDIR);
		}
	}

	DEBUGLOGC("ReadEntryMain loop start",0);
	for ( ; ; ){
		ENTRYCELL *cell;

		cell = &CELdata(cinfo->e.cellDataMax);
		cell->mark_fw = NO_MARK_ID;
		cell->mark_bk = NO_MARK_ID;
		cell->cellcolumn = CCI_NOLOAD;
		cell->state = ECS_NORMAL;
		cell->attr = attr;
		cell->icon = ICONLIST_NOINDEX;
#if FREEPOSMODE
		cell->pos.x = NOFREEPOS;
#endif
		SetCellInfo(cinfo,cell,namemask.extcolor);
		if ( cell->f.cFileName[0] == '>' ){
			if ( IsTrue(FindOptionDataAsync(hFF,FINDOPTIONDATA_LONGNAME,buf,flagdata)) ){
				EntryExtData_SetString(cinfo,DFC_LONGNAME,cell,buf);
			}
		}
		if ( !(cell->attr & rdirmask) ){
			if ( (cell->attr & (ECA_PARENT | ECA_THIS)) ||		// ..
				 (!(cell->f.dwFileAttributes & namemask.Attr) &&
				  ((cell->f.dwFileAttributes & namemask.Dir) ||
					FinddataRegularExpression(&cell->f,&namemask.FileMask)) ) ){

				if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
					if ( cell->attr & (ECA_PARENT | ECA_THIS) ){
						cinfo->e.RelativeDirs++;
					}else{
						cinfo->e.Directories++;
					}
				}
				CELt(cinfo->e.cellIMax) = cinfo->e.cellDataMax;
				if ( cs.SortF != NULL ){
					DWORD asortkey;

					asortkey = cell->f.dwFileAttributes & cs.MaskAtr;
					if ( asortkey & FILE_ATTRIBUTE_SYSTEM ){
											// B7=1 & FILE_ATTRIBUTE_SYSTEM=0
						asortkey ^= B7 | FILE_ATTRIBUTE_SYSTEM;
					}
					if ( asortkey & FILE_ATTRIBUTE_DIRECTORY ){
						asortkey ^=
							FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY;
					// FILE_ATTRIBUTE_SYSTEM=1 & FILE_ATTRIBUTE_DIRECTORY=0;
					}
					cell->attrsortkey = (BYTE)asortkey;
					if ( sorttime != MAX32 ){
						// 移動量を加算
						sorttime += cinfo->e.cellIMax
								- SortCmp(cinfo->e.cellDataMax, cinfo->e.cellIMax, &cs);
					}
				}
				cinfo->e.cellIMax++;
			}
		}

		cinfo->e.cellDataMax++;
			// ※TM_checkでcellアドレスが変わる場合有
		if ( TM_check(&cinfo->e.CELLDATA,
				sizeof(ENTRYCELL) * (cinfo->e.cellDataMax + 2)) == FALSE ){
			errorcode = ERROR_OUTOFMEMORY;
			break;
		}
		if ( TM_check(&cinfo->e.INDEXDATA,
				sizeof(ENTRYINDEX) * (cinfo->e.cellDataMax + 2)) == FALSE ){
			errorcode = ERROR_OUTOFMEMORY;
			break;
		}

		if ( cinfo->e.cellDataMax >= rps.dispN ){
			// 挿入ソートに時間が掛かりすぎているなら(移動量が多すぎ)、
			// ソートを後回しにする
			// ※エントリ数が増えるとオーバーフローするが、ソート後回しに
			//   なる傾向なので対策しない
			if ( (cinfo->e.cellDataMax > 1000) &&
				 (((DWORD)cinfo->e.cellDataMax << 6) < sorttime ) ){
				sorttime = MAX32;
			}
			if ( ReadDirProgress(cinfo,&rps) == FALSE ){
				errorcode = ERROR_MORE_DATA;
				break;
			}
		}
		if ( IsTrue(FindNextAsync(hFF,&CELdata(cinfo->e.cellDataMax).f,flagdata)) ){
			continue;
		}
		errorcode = GetLastError();
						// (1)CD-ROM等で読み込み失敗し、更に読める場合がある
						// (2)cFileName に入りきらない長さのエントリがある
		if ( errorcode == ERROR_MORE_DATA ){
			int chance;

			chance = 10;
			for ( ; ; ){
				Sleep(100);
				if ( IsTrue(FindNextAsync(hFF,&CELdata(cinfo->e.cellDataMax).f,flagdata)) ){
					errorcode = NO_ERROR;
					break;
				}
				errorcode = GetLastError();
				if ( errorcode == ERROR_NO_MORE_FILES ){
					errorcode = ERROR_MORE_DATA;
					break;
				}
				if ( errorcode != ERROR_MORE_DATA ) break;
				chance--;
				if ( !chance ) break;
			}
		}
		if ( errorcode != NO_ERROR ) break;
	}

	DEBUGLOGC("ReadEntryMain loop end",0);
	FreeFN_REGEXP(&namemask.FileMask);
	FindCloseAsync(hFF,flagdata);

											// 後回しにしたソートを実行する
	if ( (cs.SortF != NULL) && (sorttime == MAX32) ){
		int min;

		if ( cinfo->e.cellIMax > SORTINFOSIZE ){
			SetPopMsg(cinfo,POPMSG_NOLOGMSG,SortingStr);
			UpdateWindow_Part(cinfo->info.hWnd);
		}
		if ( sortid >= SORT_COLUMNTYPE ){
			PreExecuteSort(cinfo,sortid);
		}

		// . と .. をスキップ
		for ( min = 0 ; CEL(min).attr & (ECA_PARENT | ECA_THIS) ; min++ );
		cs.worktbl = PPcHeapAlloc( ( cinfo->e.cellIMax / 2 + 1 ) * sizeof(ENTRYINDEX));
		MergeCellSort(&cs,min,cinfo->e.cellIMax);
		PPcHeapFree(cs.worktbl);
		if ( cinfo->e.cellIMax > SORTINFOSIZE ) StopPopMsg(cinfo,PMF_DISPLAYMASK);
	}

	if ( errorcode != ERROR_NO_MORE_FILES ){
		PPErrorMsg(buf,errorcode);
		SetDummyCell(&CELdata(cinfo->e.cellDataMax),buf);
		setflag(CELdata(cinfo->e.cellDataMax).attr,ECA_PARENT);
		CELdata(cinfo->e.cellDataMax).f.nFileSizeLow = errorcode;
		CELt(cinfo->e.cellIMax) = cinfo->e.cellDataMax;

		cinfo->e.cellDataMax++;
		cinfo->e.cellIMax++;
	}
	if ( cinfo->celF.fmt[0] == DE_WIDEV ){
		FixCellWideV(cinfo);
		if ( XC_awid ){
			int oldx;

			oldx = cinfo->cel.Area.cx;
			InitCli(cinfo);
			FixWindowSize(cinfo,oldx - cinfo->cel.Area.cx,0);
		}
		if ( cinfo->hHeaderWnd != NULL ) FixHeader(cinfo);
	}
	if ( (cinfo->e.Dtype.mode == VFSDT_LFILE) &&
		 (memcmp(cinfo->path,T("aux:"),TSIZEOF(4)) != 0) ){
		if ( cinfo->e.Dtype.BasePath[0] != '\0' ){
			if ( VFSGetRealPath(cinfo->info.hWnd,cinfo->RealPath,cinfo->e.Dtype.BasePath) == FALSE ){
				tstrcpy(cinfo->RealPath,cinfo->path);
			}
		}else{
			*VFSFindLastEntry(cinfo->RealPath) = '\0';
		}
	}
	if ( namemask.extcolor != NULL ){
		HeapFree( hProcessHeap,0,namemask.extcolor);
	}
	// 「.」「..」の算出
	for ( cinfo->e.AllRelativeDirs = 0 ;
			CELdata(cinfo->e.AllRelativeDirs).attr & (ECA_PARENT | ECA_THIS) ;
			cinfo->e.AllRelativeDirs++ );
	DEBUGLOGC("ReadEntryMain end",0);
}

int StartCellEdit(PPC_APPINFO *cinfo)
{
	int ref;

	EnterCellEdit(cinfo);
	if ( (ref = cinfo->edit.ref) == 0 ) cinfo->edit.ref++;
	LeaveCellEdit(cinfo);
	return ref;
}
/*-----------------------------------------------------------------------------
	エントリ一覧を取得する
-----------------------------------------------------------------------------*/
void read_entry(PPC_APPINFO *cinfo,int flags)
{
	TCHAR buf[VFPS],mask[VFPS];
	DWORD StartTime;
	int WillCellN = 0,WillWinO = 0;

	DEBUGLOGC("read_entry start",0);
	if ( StartCellEdit(cinfo) ) return;

	cinfo->StateInfo.state = StateID_Init;
	cinfo->StateInfo.tick = StartTime = GetTickCount();

	cinfo->e.cellNref = -1;
	cinfo->FDirWrite = FDW_NORMAL;	// 更新開始待ちを解除

	if ( !(flags & RENTRY_UPDATE) && IsTrue(cinfo->ModifyComment) ){
		WriteComment(cinfo,cinfo->CommentFile);
	}
	HideFileNameTip(cinfo);
	VFSGetDriveType(cinfo->path,&cinfo->e.Dtype.pathtype,NULL);
	LoadSettingGeneral(cinfo,&flags,mask);
	if ( cinfo->dset.flags & DSET_ASYNCREAD ){ // 非同期読み込み+保存を有効
		setflag(flags,RENTRYI_ASYNCREAD | RENTRYI_SAVECACHE);
		if ( cinfo->dset.flags & DSET_REFRESH_ACACHE ){
			setflag(flags,RENTRYI_REFRESHCACHE);
		}
		if ( cinfo->dset.flags & DSET_NOSAVE_ACACHE ){
			resetflag(flags,RENTRYI_SAVECACHE);
		}
	}
	if ( flags & RENTRY_NOASYNC ) resetflag(flags,RENTRYI_ASYNCREAD);
	if ( !(flags & RENTRYI_ASYNCREAD) ){
		// 非同期読み込みしないときはここでカレントディレクトリを変更
		if ( VFSArchiveSection(VFSAS_CHECK,NULL) == 0 ){
			VFSTryDirectory(cinfo->info.hWnd,cinfo->path,TRUE);
		}
	}
											// 希望 cell 位置を準備する
	if ( flags & RENTRY_SAVEOFF ){
		WillCellN = cinfo->e.cellN;
		WillWinO  = cinfo->cellWMin;
		setflag(flags,RENTRYI_NOSETHISTORY);
	}else{
									//	↓１つ前のifで判断済み
		if ( !(flags & (RENTRY_NOHIST /*| RENTRY_SAVEOFF*/)) && X_acr[1] ){
			const TCHAR *vp;

			UsePPx();
			vp = SearchHistory(PPXH_PPCPATH,cinfo->OrgPath[0] ? cinfo->OrgPath : cinfo->path);
			if ( vp != NULL ){
				const int *histp;

				histp = (const int *)GetHistoryData(vp);
				if ( histp[1] >= 0 ){
					WillCellN = histp[0];
					WillWinO  = histp[1];
					setflag(flags,RENTRY_SAVEOFF);
				}
			}
			FreePPx();
		}
	}
	cinfo->SlowMode = FALSE;
	cinfo->e.cellN = 0;
	cinfo->cellWMin = 0;
	cinfo->AcceptReload = RENTRY_READ;

	tstrcpy(buf,cinfo->path);
/*
	if ( cinfo->AuxiliaryOperation ){
		const TCHAR *auxcmd = ThGetString(&cinfo->StringVariable,T("AUXlist"),NULL,0);
		if ( auxcmd != NULL ){
			PP_ExtractMacro(cinfo->info.hWnd,&cinfo->info,NULL,auxcmd,buf,XEO_EXTRACTEXEC);
		}
	}
*/
	SetLinebrush(cinfo,LINE_NORMAL);
	// 始めからキャッシュを使用する場合
	if ( (flags & RENTRY_USECACHE) ||
		 (!(flags & RENTRY_CACHEREFRESH) &&
		 	(cinfo->dset.flags & DSET_CACHEONLY)) ){
		setflag(flags,RENTRYI_NOREADCOMMENT);
		cinfo->SlowMode = TRUE;
		if ( GetCache_Path(buf,cinfo->path,NULL) == FALSE ){
			// 既存のcacheが無い
			tstrcpy(buf,cinfo->path);
			setflag(flags,RENTRY_CACHEREFRESH);
		}else{
			setflag(flags,RENTRYI_CACHE);
		}
	}
	if ( flags & RENTRY_UPDATE ){
		DEBUGLOGC("read_entry update start",0);
		cinfo->StateInfo.state = StateID_UpdateStart;
		UpdateEntry(cinfo,buf,&flags,mask);
	}else{
		DEBUGLOGC("read_entry read start",0);

		cinfo->StateInfo.state = StateID_ReadStart;
		cinfo->DiskSizes.free.u.HighPart =
			cinfo->DiskSizes.total.u.HighPart =
			cinfo->DiskSizes.used.u.HighPart = MAX32;

		ReadEntryMain(cinfo,buf,&flags,mask);
		cinfo->UseArcPathMask = ARCPATHMASK_OFF;
		cinfo->UseSplitPathName = FALSE;

		if ( cinfo->e.Dtype.mode == VFSDT_LFILE ){
			if ( (flags & RENTRY_USEPATHMASK) ||
				 (!(flags & RENTRY_NOUSEPATHMASK) &&
				 	((XC_pmsk[1] > 0) ||
				 	 (CELdata(0).f.dwFileAttributes & FILE_ATTRIBUTEX_FOLDER))) ){
				MaskPathMain(cinfo,0,NilStr);
				if ( CELdata(0).f.dwFileAttributes == (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTEX_FOLDER) ){
					cinfo->UseArcPathMask = ARCPATHMASK_DIRECTORY;
				}
			}else if ( XC_pmsk[1] < 0 ){
				cinfo->UseSplitPathName = TRUE;
			}
		}else if ( (cinfo->e.Dtype.mode == VFSDT_UN) ||
			 (cinfo->e.Dtype.mode == VFSDT_SUSIE) ){
			if ( (flags & RENTRY_USEPATHMASK) ||
				 (!(flags & RENTRY_NOUSEPATHMASK) && (XC_pmsk[0] > 0)) ){
				MaskPathMain(cinfo,0,NilStr);
			}else if ( XC_pmsk[0] < 0 ){
				cinfo->UseSplitPathName = TRUE;
			}
		}
	}
	DEBUGLOGC("read_entry u/r end",0);
	if ( flags & RENTRYI_CACHE ) cinfo->e.Dtype.ExtData = INVALID_HANDLE_VALUE;

	if ( cinfo->e.cellIMax <= 0 ){
		TM_check(&cinfo->e.CELLDATA,sizeof(ENTRYCELL) * (cinfo->e.cellDataMax + 2));
		TM_check(&cinfo->e.INDEXDATA,sizeof(ENTRYINDEX) * (cinfo->e.cellDataMax + 2));
		SetDummyCell(&CELdata(cinfo->e.cellDataMax),MessageText(StrNoEntries));
		CELdata(cinfo->e.cellDataMax).f.nFileSizeLow = ERROR_FILE_NOT_FOUND;
		setflag(CELdata(cinfo->e.cellDataMax).attr,ECA_PARENT);
		CELt(0) = cinfo->e.cellDataMax;
		cinfo->e.cellIMax = 1;
	}else{
		DWORD EndTime;

		EndTime = GetTickCount() - StartTime;
		if ( EndTime & B31 ) EndTime = 0 - EndTime;
		if ( (EndTime / cinfo->e.cellIMax) >= X_Slow ){
			SetPopMsg(cinfo,POPMSG_MSG,MES_SLMD);
			SetSlowMode(cinfo);
		}
		if ( !tstrcmp(cinfo->e.Dtype.Name,T("Xlong ")) ){
			SetPopMsg(cinfo,POPMSG_MSG,MES_WLLP);
			SetSlowMode(cinfo);
		}
		if ( WillWinO >= cinfo->e.cellIMax ) WillWinO = cinfo->e.cellIMax - 1;

		if ( flags & RENTRY_JUMPNAME ){
			ENTRYINDEX jumpN,casei = -1;
			DWORD cmplen;

			// cinfo->Jfname と一致するファイル名を検索
			cmplen = tstrlen32(cinfo->Jfname);
			if ( !(flags & RENTRY_JUMPNAME_INC) ) cmplen++;

			for ( jumpN = 0 ; jumpN < cinfo->e.cellIMax ; jumpN++ ){
				if ( !tstrnicmp(CEL(jumpN).f.cFileName,cinfo->Jfname,cmplen) ){
					if ( !(flags & (RENTRY_NEWFILE | RENTRY_NEWDIR)) ) break;
					if ( !tstrcmp(CEL(jumpN).f.cFileName,cinfo->Jfname) ){
						break;
					}else{
						casei = jumpN;
						continue;
					}
				}
			}
			// 見つからなく、新規エントリの場合は仮エントリを作成する
			if ( flags & (RENTRY_NEWFILE | RENTRY_NEWDIR) ){
				ENTRYCELL *cell;

				if ( jumpN >= cinfo->e.cellIMax ) jumpN = casei;
				if ( jumpN < 0 ){
					jumpN = cinfo->e.cellIMax;
					if ( (FALSE != TM_check(&cinfo->e.CELLDATA,sizeof(ENTRYCELL) * (cinfo->e.cellDataMax + 2))) &&
						 (FALSE != TM_check(&cinfo->e.INDEXDATA,sizeof(ENTRYINDEX) * (cinfo->e.cellDataMax + 2)))  ){
						cell = &CELdata(cinfo->e.cellDataMax);

						SetDummyCell(cell,cinfo->Jfname);
						cell->f.dwFileAttributes = flags & RENTRY_NEWFILE ?
								0 : FILE_ATTRIBUTE_DIRECTORY;
						cell->state = ECS_ADDED;
						cell->attr = 0;
						CELt(jumpN) = cinfo->e.cellDataMax;
						SetCellInfo(cinfo,cell,NULL);
						if ( cinfo->CellHashType == CELLHASH_NAME ){
							cell->cellhash = MiniHash(cell->f.cFileName);
						}
						cinfo->e.cellDataMax++;
						cinfo->e.cellIMax++;
					}
				}
			}
			// 位置が決まったら移動
			if ( jumpN < cinfo->e.cellIMax ){
				cinfo->cellWMin = WillWinO;

				// 新規作成エントリの時は、旧カーソルの直下に移動
				if ( (flags & (RENTRY_NEWFILE | RENTRY_NEWDIR) ) &&	// 新規作成
					 (flags & RENTRY_SAVEOFF) &&	// WillCellN が有効か
					 (WillCellN >= 0) && (jumpN > WillCellN) &&	// WillCellN の範囲
					 !(CEL(WillCellN).attr & (ECA_PARENT | ECA_THIS | ECA_ETC)) &&
					 ((CEL(jumpN).state & ECS_HLMASK) == ECS_ADDED) ){	// 新規
					DWORD backup;

					backup = CELt(jumpN);
					//他のエントリを１下げ(+2以降)、旧カーソル直下(+1)を空ける
					memmove(&CELt(WillCellN + 2),&CELt(WillCellN + 1),
							sizeof(ENTRYINDEX) * (jumpN - WillCellN - 1));
					jumpN = WillCellN + 1;
					CELt(jumpN) = backup;
				}
				resetflag(flags,RENTRY_NOHIST | RENTRY_SAVEOFF);
				cinfo->e.cellN = jumpN;
			}
		}
	}
	DEBUGLOGC("read_entry free count",0);
											// 空き容量計算 -------------------
	if ( (cinfo->e.Dtype.ExtData != INVALID_HANDLE_VALUE) || (flags & RENTRYI_GETFREESIZE) ){
		if ( (flags & (RENTRYI_ASYNCREAD | RENTRY_USECACHE)) ||
			 (cinfo->RealPath[0] == '\\') ){
			setflag(cinfo->SubTCmdFlags,SUBT_GETFREESPACE);
			SetEvent(cinfo->SubT_cmd);
		}else{
			GetDirectoryFreeSpace(cinfo,FALSE);
		}
	}
									// セキュリティホールになりそうなパスの調査
	if ( X_wnam ){
		if ( (cinfo->e.Dtype.mode == VFSDT_UN) ||
			 (cinfo->e.Dtype.mode == VFSDT_SUSIE) ||
			 (cinfo->e.Dtype.mode == VFSDT_CABFOLDER) ||
			 (cinfo->e.Dtype.mode == VFSDT_LZHFOLDER) ||
			 (cinfo->e.Dtype.mode == VFSDT_ZIPFOLDER) ){
			BOOL warn = FALSE;
			ENTRYDATAOFFSET offset;

			for ( offset = 0 ; offset < cinfo->e.cellDataMax ; offset++ ){
				if ( CELdata(offset).attr & (ECA_THIS | ECA_PARENT) ) continue;
				if ( IsTrue(CheckWarningName(CELdata(offset).f.cFileName)) ){
					warn = TRUE;
					CELdata(offset).state = ECS_CHANGED;
				}
			}
			if ( IsTrue(warn) ) SetPopMsg(cinfo,POPMSG_MSG,MES_FEWE);
		}
	}
	// 更新した *cache のキャッシュを保存する
	if ( (flags & RENTRY_CACHEREFRESH) && (cinfo->dset.flags & DSET_CACHEONLY) ){
		GetCache_Path(buf,cinfo->path,NULL);
		if ( FALSE == WriteListFileForRaw(cinfo,buf) ){
			SetPopMsg(cinfo,POPMSG_MSG,CacheErrorTitle);
		}
	}
	DEBUGLOGC("read_entry fix view",0);

	// WM_SETREDRAW で SW_HIDE が解除されるので対策(タブ使用時に問題がでる)
//	if ( !(X_combos & CMBS_TABALWAYS) ) // 後でタブが増えるときは←では早すぎ
#define IsUseChildWnds(cinfo) (cinfo->combo || cinfo->docks.b.hWnd || cinfo->docks.t.hWnd || cinfo->hToolBarWnd || cinfo->hTreeWnd || cinfo->hHeaderWnd)
	if ( !IsUseChildWnds(cinfo) ){
		SendMessage(cinfo->info.hWnd,WM_SETREDRAW,FALSE,0);
	}
	if ( flags & (RENTRY_NOHIST | RENTRY_SAVEOFF) ){
		InitCli(cinfo);
		if ( WillCellN >= cinfo->e.cellIMax ) WillCellN = cinfo->e.cellIMax - 1;
		cinfo->e.cellN = WillCellN;
		cinfo->cellWMin = WillWinO;
	}
							// 「..」にカーソルが当たるように調整
	if ( ((cinfo->e.cellN + 1) < cinfo->e.cellIMax) &&
		 (CEL(cinfo->e.cellN).attr & ECA_THIS) ){
		cinfo->e.cellN++;
	}
	PPxSetPath(cinfo->RegNo,(cinfo->e.Dtype.mode != VFSDT_LFILE) ?
			cinfo->RealPath : cinfo->path);

	DEBUGLOGC("read_entry fix d-h list",0);
	while ( cinfo->path[0] ){	// ディレクトリリストの更新
		if ( cinfo->PathTrackingList.top ){
			DWORD top;

			top = BackPathTrackingList(cinfo,cinfo->PathTrackingList.top);
			if ( !tstrcmp((TCHAR *)(cinfo->PathTrackingList.bottom + top),cinfo->path) ){
				break;
			}
			if ( top > TSTROFF(MAXPATHTRACKINGLIST) ){
				size_t off;

				off = 0;
				while ( (top - off) > TSTROFF(MAXPATHTRACKINGLIST) ){
					off += TSTRSIZE((TCHAR *)(cinfo->PathTrackingList.bottom + off));
				}
				memmove(cinfo->PathTrackingList.bottom,
						cinfo->PathTrackingList.bottom + off,
						cinfo->PathTrackingList.size - off);
				cinfo->PathTrackingList.top -= off;
			}
		}
		ThAddString(&cinfo->PathTrackingList,cinfo->path);
		break;
	}
	if ( VFSArchiveSection(VFSAS_CHECK,NULL) == 0 ){
		SetCurrentDirectory(PPcPath);
	}
										// 更新後の画面に更新する -----
	InitCli(cinfo);
	EndCellEdit(cinfo);
	cinfo->OldIconsPath[0] = '\0';	// アイコン表示を必ずするようにする
	if ( (XC_nsbf != 0) ||
		 ((flags & (RENTRY_NEWFILE | RENTRY_NEWDIR | RENTRY_UPDATE | RENTRY_SAVEOFF)) != (RENTRY_UPDATE | RENTRY_SAVEOFF)) ||
		 (cinfo->e.cellN >= cinfo->e.cellIMax) ||
		 (cinfo->cellWMin >= cinfo->e.cellIMax) ){
		MoveCellCsr(cinfo,0,NULL);
	}
	SetCaption(cinfo);

	if ( !(flags & (RENTRY_UPDATE | RENTRYI_NOSETHISTORY)) &&
		 !( (cinfo->e.cellIMax <= 1) && (CEL(0).type == ECT_SYSMSG) ) ){
		SavePPcDir(cinfo,TRUE);
	}

	if ( !IsUseChildWnds(cinfo) ){
		SendMessage(cinfo->info.hWnd,WM_SETREDRAW,TRUE,0);
	}
	Repaint(cinfo);
/*
	// 壁紙表示時は、WS_CLIPCHILDREN のため描画されないのを描画させる
	if ( X_fles | cinfo->bg.X_WallpaperType ){
		if ( cinfo->hTreeWnd ) InvalidateRect(cinfo->hTreeWnd,NULL,FALSE);
	}
*/
	if ( cinfo->combo ){
		PostMessage(cinfo->hComboWnd,WM_PPXCOMMAND,
				KCW_setpath,(LPARAM)cinfo->info.hWnd);
	}
	if ( !(flags & RENTRY_UPDATE) ){
		if ( flags & RENTRYI_EXECLOADCMD ){
			PP_ExtractMacro(cinfo->info.hWnd,&cinfo->info,NULL,T("*execute ,%si\"LoadCommand\""),NULL,0);
		}
		if ( IsTrue(cinfo->UseLoadEvent) ){
			SendMessage(cinfo->info.hWnd,WM_PPXCOMMAND,K_E_LOAD,0);
		}
	}
//	EndCellEdit(cinfo); // ここだとアイコンが砂時計に

	if ( XC_szcm == 2 ){ // ディレクトリサイズキャッシュの読み出し開始
		setflag(cinfo->SubTCmdFlags,SUBT_GETDIRSIZECACHE);
		SetEvent(cinfo->SubT_cmd);
	}

#ifndef _WIN64
	{
		MEMORYSTATUS mstate;

		mstate.dwLength = sizeof(MEMORYSTATUS);
		GlobalMemoryStatus(&mstate);
		if ( mstate.dwAvailVirtual < (500 * MB) ){
			SetPopMsg(cinfo,POPMSG_MSG,MemWarnStr);
		}
	}
#endif
	DEBUGLOGC("read_entry end",0);
}
//------------------------------------- キャプション用文字列を生成
void SetCaption(PPC_APPINFO *cinfo)
{
	TCHAR *ptr,buf[VFPS * 2];

	tstrcpy(buf,cinfo->UserInfo);
	ptr = buf + tstrlen(buf);
	if ( (cinfo->path[0] == ':') && (cinfo->path[1] != ':') ){ // Drives
		VFSFullPath(ptr,CEL(cinfo->e.cellN).f.cFileName,NULL);
		PPxSetPath(cinfo->RegNo,ptr);
		tstrcpy(cinfo->RealPath,ptr);
	}else if ( (cinfo->e.Dtype.mode == VFSDT_LFILE) &&
			   (cinfo->e.Dtype.BasePath[0] != '\0') ){
		tstrcpy(ptr,cinfo->e.Dtype.BasePath);
		tstrcat(ptr,T(" - listfile"));
	}else{
		tstrcpy(ptr,cinfo->path);
		if ( !((cinfo->e.cellIMax <= 1) && (CEL(0).type == ECT_SYSMSG)) ){
			if ( cinfo->hTreeWnd != NULL ){
				SendMessage(cinfo->hTreeWnd,VTM_SETPATH,0,(LPARAM)cinfo->path);
			}
			if ( (Combo.hTreeWnd != NULL) && !firstinit &&
				 (hComboFocus == cinfo->info.hWnd) ){
				SendMessage(Combo.hTreeWnd,VTM_SETPATH,0,(LPARAM)cinfo->path);
			}
		}
	}

	if ( cinfo->docks.t.hAddrWnd != NULL ){
		SetWindowText(cinfo->docks.t.hAddrWnd,ptr);
		SendMessage(cinfo->docks.t.hAddrWnd,EM_SETSEL,EC_LAST,EC_LAST);
	}
	if ( cinfo->docks.b.hAddrWnd != NULL ){
		SetWindowText(cinfo->docks.b.hAddrWnd,ptr);
		SendMessage(cinfo->docks.b.hAddrWnd,EM_SETSEL,EC_LAST,EC_LAST);
	}

	if ( (cinfo->UseArcPathMask != ARCPATHMASK_OFF) && cinfo->ArcPathMask[0] ){
		const TCHAR *arcpath = cinfo->ArcPathMask;

		if ( (*arcpath == '\\') || (*arcpath == '/') ) arcpath++;
		AppendPath(ptr,arcpath,cinfo->PathSeparater);
	}
	if ( XC_dpmk ) CatPath(NULL,ptr,cinfo->mask.file);
	if ( cinfo->BoxStatus.top != cinfo->docks.t.client.bottom ){
		tstrlimcpy(cinfo->Caption,buf,TSIZEOF(cinfo->Caption));
	}
	SetWindowText(cinfo->info.hWnd,buf);
}

void GetDirectoryFreeSpace(PPC_APPINFO *cinfo,BOOL subthread)
{
	DWORD tmp,SpC,BpS,FC;
	ULARGE_INTEGER UserFree,Total,TotalFree;
	TCHAR buf[VFPS],RealPath[VFPS];

	if ( cinfo->RealPath[0] == '?' ) return;

	if ( IsTrue(subthread) ){
//		if ( cinfo->DiskSizes.total.u.HighPart != MAX32 ) return;
		EnterCellEdit(cinfo);
	}

	if ( (cinfo->e.Dtype.mode != VFSDT_PATH) &&
		 (cinfo->e.Dtype.mode != VFSDT_DLIST) &&
		 (cinfo->e.Dtype.mode != VFSDT_SHN) &&
		 (cinfo->e.Dtype.mode != VFSDT_UNKNOWN) ){
		ENTRYDATAOFFSET offset;

		Total.u.LowPart = Total.u.HighPart = 0;
		for ( offset = 0 ; offset < cinfo->e.cellDataMax ; offset++ ){
			AddDD(Total.u.LowPart,Total.u.HighPart,
				CELdata(offset).f.nFileSizeLow,CELdata(offset).f.nFileSizeHigh);
		}
		UserFree.u.HighPart = MAX32;
	}else{
		tstrcpy(RealPath,cinfo->RealPath);
		if ( IsTrue(subthread) ) LeaveCellEdit(cinfo);
		if ( tstrcmp(cinfo->e.Dtype.Name,VFSFFTYPENAME_Network) == 0 ) return;

		Total.u.HighPart = MAX32;
		GetDriveName(buf,RealPath);
#ifndef UNICODE
		if ( DGetDiskFreeSpaceEx )
#endif
		{
			DGetDiskFreeSpaceEx(GetNT_9xValue(RealPath,buf),
					&UserFree,&Total,&TotalFree);
		}
		if ( Total.u.HighPart == MAX32 ){
			if ( ((OSver.dwMajorVersion < 6) || (GetLastError() == ERROR_INVALID_FUNCTION)) &&
					GetDiskFreeSpace(buf,&SpC,&BpS,&FC,&tmp) ){
				Total.u.HighPart = UserFree.u.HighPart = 0;
				SpC *= BpS;
				UserFree.u.LowPart = SpC * FC;
				Total.u.LowPart = SpC * tmp;
			}else{
				UserFree.u.HighPart = MAX32;
			}
		}

		if ( IsTrue(subthread) ){
			EnterCellEdit(cinfo);
			if ( tstrcmp(RealPath,cinfo->RealPath) != 0 ){
				LeaveCellEdit(cinfo);
				return;
			}
		}
	}
	cinfo->DiskSizes.free = UserFree;
	cinfo->DiskSizes.total = Total;
	if ( UserFree.u.HighPart & B31 ){
		Total.u.HighPart = MAX32;
	}else{ // C4701 ok
		SubDD(Total.u.LowPart,Total.u.HighPart,UserFree.u.LowPart,UserFree.u.HighPart);
	}
	if ( Total.u.HighPart & B31 ) Total.u.HighPart = MAX32; // 空きが正常に得られなかった恐れがある
	cinfo->DiskSizes.used = Total;
	if ( IsTrue(subthread) ){
		LeaveCellEdit(cinfo);
		RefleshInfoBox(cinfo,DE_ATTR_DIR);
	}
}

void SetDummyRelativeDir(PPC_APPINFO *cinfo,const TCHAR *name,BYTE attr,BYTE type)
{
	ENTRYCELL *cell;

	cell = &CELdata(cinfo->e.cellDataMax);
	CELdata(cinfo->e.cellDataMax + 1) = *cell;
	SetDummyCell(cell,name);
	cell->f.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	cell->state = ECS_NORMAL;
	cell->attr = attr;
	cell->type = type;
	cinfo->e.cellDataMax++;
	cinfo->e.cellIMax++;
	CELt(cinfo->e.cellIMax) = cinfo->e.cellDataMax;
}

// 指定パスに関する設定を取得する
//マスク、低速、キャッシュも？
void LoadSettingMain(LOADSETTINGS *ls,const TCHAR *path)
{
	LOADSETTINGS tmpls;
	const TCHAR *p;

	tmpls.disp[0] = '\0';
	if ( NO_ERROR != GetCustTable(T("XC_dset"),path,&tmpls,sizeof(tmpls)) ){
		return;
	}

	ls->dset.flags = (ls->dset.flags & (WORD)~tmpls.dset.deflags) | tmpls.dset.flags;
	if ( tmpls.dset.infoicon != DSETI_DEFAULT ){
		ls->dset.infoicon = tmpls.dset.infoicon;
	}
	if ( tmpls.dset.cellicon != DSETI_DEFAULT ){
		ls->dset.cellicon = tmpls.dset.cellicon;
	}
	if ( tmpls.dset.sort.mode.dat[0] >= 0 ) ls->dset.sort = tmpls.dset.sort;

	p = tmpls.disp;
	while ( SkipSpace(&p) ){
		if ( !memcmp(p,LOADDISPSTR,SIZEOFTSTR(LOADDISPSTR)) ){
			p += TSIZEOFSTR(LOADDISPSTR);
			GetLineParam(&p,ls->disp);
			continue;
		}
		if ( !memcmp(p,LOADMASKSTR,SIZEOFTSTR(LOADMASKSTR)) ){
			p += TSIZEOFSTR(LOADMASKSTR);
			GetLineParam(&p,ls->mask);
			continue;
		}
		if ( !memcmp(p,LOADCMDSTR,SIZEOFTSTR(LOADCMDSTR)) ){
			p += TSIZEOFSTR(LOADCMDSTR);
			GetLineParam(&p,ls->cmd);
			continue;
		}
		XMessage(GetFocus(),NULL,XM_NiERRld,T("XC_dset:%s load error:%s"),path,p);
		return;
	}
}

void USEFASTCALL ReloadCellDispFormat(PPC_APPINFO *cinfo,const TCHAR *name)
{
	FreeCFMT(&cinfo->celF);
	LoadCFMT(&cinfo->celF,T("MC_celS"),name,&CFMT_celF);
	if ( cinfo->celF.fmt[0] == DE_WIDEV ) FixCellWideV(cinfo);
	if ( cinfo->hHeaderWnd != NULL ) FixHeader(cinfo);
	cinfo->FixcelF = TRUE;
}

// 現在パスの設定を取得する(書庫用)
void LoadSettingSecond(PPC_APPINFO *cinfo,const TCHAR *path,int *flags,TCHAR *mask)
{
	LOADSETTINGS ls;

	ls.dset = cinfo->dset;
	ls.disp[0] = '\0';
	ls.mask[0] = '\1';
	ls.mask[1] = '\0';
	ls.cmd[0] = '\0';
	LoadSettingMain(&ls,path);

	if ( ls.disp[0] ) ReloadCellDispFormat(cinfo,ls.disp);
	if ( ls.mask[0] != '\1' ) tstrcpy(mask,ls.mask);
	if ( ls.cmd[0] != '\0' ){
		setflag(*flags,RENTRYI_EXECLOADCMD);
		ThSetString(&cinfo->StringVariable,T("LoadCommand"),ls.cmd);
	}

	cinfo->dset = ls.dset;
	cinfo->iconR = (ls.dset.infoicon != DSETI_NOSPACE) ? cinfo->XC_ifix_size.cx : 0;
}

// 現在パスの設定を取得する(一般)
void LoadSettingGeneral(PPC_APPINFO *cinfo,int *flags,TCHAR *mask)
{
	TCHAR path[VFPS],*p;
	LOADSETTINGS ls;
	int drive;

	ls.dset.flags = 0;
	ls.dset.infoicon = DSETI_OVL;
	ls.dset.cellicon = DSETI_OVL;
	ls.dset.sort.mode.block = 0xffffff00;
	ls.dset.sort.atr = 0x1f;
	ls.dset.sort.option = NORM_IGNORECASE;
	ls.disp[0] = '\0';
	ls.mask[0] = '\1';
	ls.mask[1] = '\0';
	ls.cmd[0] = '\0';

	// 全指定
	LoadSettingMain(&ls,T("*"));

	// ドライブ限定
	tstrcpy(path,cinfo->path);
	p = VFSGetDriveType(path,&drive,NULL);
	if ( p != NULL ){
		TCHAR backup;

		if ( *p == '\\' ) p++;
		backup = *p;
		*p = '\0';
		if ( drive >= VFSPT_FILESCHEME ){ // VFSPT_FTP,VFSPT_SHELLSCHEME,VFSPT_HTTP
			TCHAR *rootp = tstrchr(path,'/');
			if ( rootp != NULL ){
				*rootp = '\0';
				LoadSettingMain(&ls,path);
				*rootp = '/';
			}
		}
		LoadSettingMain(&ls,path);
		*p = backup;
	}else{
		p = path;
	}
	// 下層パス
	if ( *p != '\0' ){
		for ( ; ; ){	// 途中
			TCHAR backup,*q;

			q = FindPathSeparator(p);
			if ( q == NULL ) break;
			p = q + 1;
			backup = *p;
			*p = '\0';
			LoadSettingMain(&ls,path);
			*p = backup;
		}
		// 最終層の更に下層指定
		p += tstrlen(p);
		*p = '\\';
		*(p + 1) = '\0';
		LoadSettingMain(&ls,path);
		// 最終層
		*p = '\0';
		LoadSettingMain(&ls,path);
	}else if ( path[0] == '\\' ){ // \\ のとき
		LoadSettingMain(&ls,T("\\"));
	}

	if ( !(*flags & RENTRY_UPDATE) ){ // 表示書式の反映(新規読み込み時のみ)
		if ( ls.disp[0] ){
			ReloadCellDispFormat(cinfo,ls.disp);
		}else{
			if ( IsTrue(cinfo->FixcelF) ){
				cinfo->FixcelF = FALSE;

				FreeCFMT(&cinfo->celF);
				LoadCFMT(&cinfo->celF,T("XC_celF"),cinfo->RegCID + 1,&CFMT_celF);
				if ( cinfo->celF.fmt[0] == DE_WIDEV ) FixCellWideV(cinfo);
				if ( cinfo->hHeaderWnd != NULL ) FixHeader(cinfo);
			}
		}
	}
	cinfo->dset = ls.dset;
	cinfo->iconR = (ls.dset.infoicon != DSETI_NOSPACE) ? cinfo->XC_ifix_size.cx : 0;
	tstrcpy(mask,ls.mask);
	if ( ls.cmd[0] != '\0' ){
		setflag(*flags,RENTRYI_EXECLOADCMD);
		ThSetString(&cinfo->StringVariable,T("LoadCommand"),ls.cmd);
	}
}
