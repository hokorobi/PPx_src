/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System		ファイル操作,その他
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_FOP.H"
#include "VFS_STRU.H"
#pragma hdrstop

#ifndef PROCESS_MODE_BACKGROUND_BEGIN // I/O 優先度
#define PROCESS_MODE_BACKGROUND_BEGIN	0x00100000
#define PROCESS_MODE_BACKGROUND_END		0x00200000
#define THREAD_MODE_BACKGROUND_BEGIN	0x00010000
#define THREAD_MODE_BACKGROUND_END		0x00020000
#endif

const TCHAR StrMES_ECUD[] = MES_ECUD;
const TCHAR StrMES_QDRC[] = MES_QDRC;

ERRORCODE BackupFile(FOPSTRUCT *FS,const TCHAR *src)
{
	TCHAR backupdest[VFPS],*p,*dstp;
	ERRORCODE error;
	size_t size;

										// delete$ を用意する
	VFSFixPath(backupdest,FS->DestDir,FS->opt.source,VFSFIX_VFPS | VFSFIX_REALPATH | VFSFIX_NOFIXEDGE);
	CatPath(NULL,backupdest,T("deleted$"));
	if ( FS->BackupDirCreated == FALSE ){	// deleted$ 作成を１回だけする
		if ( FALSE != CreateDirectoryL(backupdest,NULL) ){
			FopLog(FS,backupdest,NULL,LOG_MAKEDIR);
		}
		FS->BackupDirCreated = TRUE;
	}
										// delete$内ディレクトリの作成
	dstp = FindLastEntryPoint(backupdest);
	size = dstp - backupdest;
	if ( (size >= 3) && !memcmp(src,backupdest,size) ){
		CatPath(NULL,backupdest,src + size);
		dstp = VFSFindLastEntry(backupdest);
		if ( *dstp == '\\' ){
			*dstp = '\0';
			MakeDirectories(backupdest,NULL);
		}
	}

	// チャレンジ１
	p = FindLastEntryPoint(src);
	CatPath(NULL,backupdest,p);
	if ( IsTrue(MoveFileL(src,backupdest)) ){
		FopLog(FS,src,backupdest,LOG_BACKUP);
		return NO_ERROR;
	}
	error = GetLastError();
	if ( error != ERROR_ALREADY_EXISTS ) return error;

	// ファイル重複を回避してチャレンジ２
	if ( GetUniqueEntryName(backupdest) == FALSE ) return error;
	if ( IsTrue(MoveFileL(src,backupdest)) ){
		FopLog(FS,src,backupdest,LOG_BACKUP);
		return NO_ERROR;
	}
	return error;
}

ERRORCODE CopySecurity(FOPSTRUCT *FS,const TCHAR *src,const TCHAR *dst)
{
	DWORD size,error;
	PSECURITY_DESCRIPTOR psd;

	GetFileSecurity(src,FS->opt.security,NULL,0,&size);
	psd = HeapAlloc(DLLheap,0,size);
	if ( psd == NULL ) goto error;
	if ( GetFileSecurity(src,FS->opt.security,psd,size,&size) == FALSE ){
		goto error;
	}
	if ( SetFileSecurity(dst,FS->opt.security,psd) == FALSE ){
		src = dst;
		goto error;
	}
	HeapFree(DLLheap,0,psd);
	return NO_ERROR;
error:
	error = GetLastError();
	if ( psd != NULL ) HeapFree(DLLheap,0,psd);
	if ( FS->opt.fop.flags & VFSFOP_OPTFLAG_SKIPERROR ){
		FWriteErrorLogs(FS,src,T("Security"),error);
		return NO_ERROR;
	}
	return error;
}

void SetFopLowPriority(FOPSTRUCT *FS)
{
	DWORD flags;

	if ( (FS != NULL) && (FS->opt.fop.flags & VFSFOP_OPTFLAG_LOWPRI) ){
		flags = ( WinType >= WINTYPE_VISTA ) ?
				THREAD_MODE_BACKGROUND_BEGIN : THREAD_PRIORITY_LOWEST;
	}else{
		flags = ( WinType >= WINTYPE_VISTA ) ?
				THREAD_MODE_BACKGROUND_END : THREAD_PRIORITY_NORMAL;
	}
	SetThreadPriority(GetCurrentThread(),flags);
}

/*
*************************************** undo
*/

void GetUndoLogFileName(TCHAR *name)
{
	name[0] = '\0';
	GetCustData(T("X_save"),name,VFPS);
	if ( name[0] == '\0' ){
		MakeTempEntry(MAX_PATH,name,0);
	}else{
		VFSFixPath(NULL,name,DLLpath,VFSFIX_FULLPATH | VFSFIX_REALPATH);
	}
	CatPath(NULL,name,T("PPXUNDO.LOG"));
}

BOOL DoDeleteUndo(FOPSTRUCT *FS,const TCHAR *type,const TCHAR *target)
{
	if ( target == NULL ){
		FWriteLog(FS->hEWnd,T("Error no target\r\n"));
		return FALSE;
	}

	FWriteLog(FS->hEWnd,T("Delete("));
	FWriteLog(FS->hEWnd,type);
	FWriteLog(FS->hEWnd,T("):"));
	FWriteLog(FS->hEWnd,target);
	if ( FS->testmode == FALSE ){
		if ( DeleteFileL(target) == FALSE ){
			FWriteLog(FS->hEWnd,T(" - error"));
		}
	}
	return TRUE;
}

BOOL DoMoveUndo(FOPSTRUCT *FS,const TCHAR *type,const TCHAR *target,const TCHAR *org)
{
	TCHAR name[VFPS];

	if ( target == NULL ){
		FWriteLog(FS->hEWnd,T("Error no target\r\n"));
		return FALSE;
	}
	FWriteLog(FS->hEWnd,T("Move("));
	FWriteLog(FS->hEWnd,type);
	FWriteLog(FS->hEWnd,T("):"));
	FWriteLog(FS->hEWnd,target);
	while ( FS->testmode == FALSE ){
		if ( MoveFileWithProgressL(target,org,(LPPROGRESS_ROUTINE)CopyProgress,
				&FS->progs,MOVEFILE_COPY_ALLOWED) == FALSE){
			ERRORCODE result;

			result = GetLastError();
			if ( result == ERROR_NOT_SAME_DEVICE ){	// MoveできないのでCopyで
				BOOL cancel = FALSE;
				if ( IsTrue(CopyFileExL(target,org,NULL,NULL,&cancel,0)) ){
					DeleteFileL(target);
					break;
				}
				result = GetLastError();
			}
			if ( result == ERROR_PATH_NOT_FOUND ){
				VFSFullPath(name,T(".."),org);
				if ( MakeDirectories(name,NULL) == NO_ERROR ) continue;
			}
			FWriteLog(FS->hEWnd,T(" - error"));
			return FALSE;
		}
		break;
	}
	return TRUE;
}

BOOL UndoCommand(FOPSTRUCT *FS)
{
	TCHAR name[VFPS],buf[VFPS];
	ERRORCODE result;
	TCHAR *mem, *text, *maxptr;
	TCHAR *nowp;

	CreateFWriteLogWindow(FS);
	GetUndoLogFileName(name);
	result = LoadTextImage(name, &mem, &text, &maxptr);
	if ( result != NO_ERROR ){
		if ( result == ERROR_FILE_NOT_FOUND ){
			FWriteLog(FS->hEWnd,MessageText(StrMES_ECUD));
		}else{
			FWriteErrorLogs(FS,T("Undo"),NULL,result);
		}
		FS->NoAutoClose = TRUE;
		return FALSE;
	}

	{ // tab を10文字に
		DWORD tabsize = 10 << 2;
		SendMessage(FS->hEWnd,EM_SETTABSTOPS,1,(LPARAM)&tabsize);
		InvalidateRect(FS->hEWnd,NULL,FALSE);
	}

	// ログの検証
	{
		BOOL havebackup = FALSE;
		DWORD wrong = 0;
		DWORD untarget = 0;
		DWORD unknowntype = 0;
		DWORD target = 0;

		nowp = text;
		while (*nowp){
			TCHAR *next,*tab;
									// 行末を取得
			next = tstrchr(nowp,'\r');
			if ( next == NULL ){
				next = nowp + tstrlen(nowp);
			}else{
				*next++ = '\0';
			}
									// Tab取得
			tab = tstrchr(nowp,'\t');
			if ( tab != NULL ){
				*tab = '\0';
				if ( !tstrcmp(nowp,STRLOGDEST) || !tstrcmp(nowp,STRLOGMAKEDIR)){	// 処理先
					// 何もしない
				}else if ( !tstrcmp(nowp,StrLogBackup) ){	// バックアップ有り
					havebackup = TRUE;
				}else{
					if ( !tstrcmp(nowp,StrLogCopy) ||
						 !tstrcmp(nowp,StrLogMove) ){
						target++;
						FWriteLog(FS->hEWnd,T("o "));
					}else if ( !tstrcmp(nowp,StrLogOverwrite) ||
						 !tstrcmp(nowp,StrLogReplace) ||
						 !tstrcmp(nowp,STRLOGDELETE) ){
						if ( havebackup ){
							target++;
							FWriteLog(FS->hEWnd,T("o "));
						}else{
							wrong++;
							FWriteLog(FS->hEWnd,T("x "));
						}
					}else if ( !tstrcmp(nowp,StrLogAppend) ){
						wrong++;
						FWriteLog(FS->hEWnd,T("x "));
					}else if ( !tstrcmp(nowp,StrLogTest) ||
						!tstrcmp(nowp,STRLOGSKIP) ||
						!tstrcmp(nowp,StrLogExist) ){
						untarget++;
						FWriteLog(FS->hEWnd,T("- "));
					}else{
						FWriteLog(FS->hEWnd,T("? "));
						unknowntype++;
					}
					FWriteLog(FS->hEWnd,nowp);
					FWriteLog(FS->hEWnd,T("\t"));
					FWriteLog(FS->hEWnd,tab + 1);
					FWriteLog(FS->hEWnd,T("\r\n"));
					havebackup = FALSE;
				}
				*tab = '\t';
			}
			nowp = next;
			while ( (*nowp == '\r') || (*nowp == '\n') ){
				*nowp = '\0';
				nowp++;
			}
		}
		wsprintf(buf,
			T("Target:%d / Untarget:%d / Badtarget:%d / Unknown:%d\r\n"),
			target,untarget,wrong,unknowntype);
		FWriteLog(FS->hEWnd,buf);

		if ( target && !wrong && !unknowntype ){
			if ( IsTrue(FS->testmode) ||
				 (PMessageBox(FopGetMsgParentWnd(FS),MES_QSUD,STR_FOPWARN,
						MB_ICONEXCLAMATION | MB_YESNO) == IDYES) ){
				TCHAR *tab,*targetp = NULL;

				FWriteLog(FS->hEWnd,T("\r\n***\r\n\r\n"));
				// ログの最後から順番に実行する
				while( nowp >= text ){
					while( (nowp > text) && (*(nowp - 1) != '\0') ) nowp--;
					if ( *nowp != '\0' ){
										// Tab取得
						tab = tstrchr(nowp,'\t');
						if ( tab != NULL ){
							*tab++ = '\0';
							if ( !tstrcmp(nowp,STRLOGDEST) ){	// 処理先
								targetp = tab;
							}else{
								if ( !tstrcmp(nowp,StrLogCopy) ||
									!tstrcmp(nowp,StrLogOverwrite) ){
									DoDeleteUndo(FS,nowp,targetp);
								}else if ( !tstrcmp(nowp,StrLogMove) ||
									!tstrcmp(nowp,StrLogReplace) ){
									DoMoveUndo(FS,nowp,targetp,tab);
								}else if ( !tstrcmp(nowp,StrLogBackup) ){
									DoMoveUndo(FS,nowp,targetp,tab);
								}else if ( !tstrcmp(nowp,STRLOGMAKEDIR) ){
									RemoveDirectoryL(tab);
								}else{
									FWriteLog(FS->hEWnd,nowp);
								}
								FWriteLog(FS->hEWnd,T("\r\n"));
							}
						}
					}
					nowp--;
				}
			}
		}else{
			FWriteLog(FS->hEWnd,MessageText(StrMES_ECUD));
		}
	}
	HeapFree(ProcHeap,0,mem);
	return TRUE;
}

//*************************************************************** File compare

typedef struct {
	PPXAPPINFO info;	// PPx 共通情報
	const TCHAR *src;
	const TCHAR *dest;
} FCSTRUCT;

DWORD_PTR USECDECL FCInfo(FCSTRUCT *FCS,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case '1':
			tstrcpy(uptr->enums.buffer,DLLpath);
			break;
		case 'C':
			tstrcpy(uptr->enums.buffer,FCS->src);
			break;
		case '2':
			tstrcpy(uptr->enums.buffer,FCS->dest);
			break;

		default:
			if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
			return 0;
	}
	return 1;
}

void FopCompareFile(HWND hWnd,const TCHAR *src,const TCHAR *dest)
{
	FCSTRUCT FCS;
	TCHAR buf[CMDLINESIZE];

	FCS.info.Function = (PPXAPPINFOFUNCTION)FCInfo;
	FCS.info.Name = T("FC");
	FCS.info.RegID = NilStr;
	FCS.info.hWnd = hWnd;
	FCS.src = src;
	FCS.dest = dest;

	buf[0] = '\0';
	GetCustData(T("X_fc"),&buf,sizeof buf);
	if ( buf[0] == '\0' ) tstrcpy(buf,T("fc %FCD \"%2\""));
	PP_ExtractMacro(hWnd,&FCS.info,NULL,buf,NULL,0);
}

int FopOperationMsgBox(FOPSTRUCT *FS,const TCHAR *msg,const TCHAR *title,UINT flags)
{
	int result;

	DisplaySrcNameNow(FS); // 処理ファイル名が未表示なら表示
	SetTaskBarButtonProgress(FS->hDlg,TBPF_PAUSED,0);
	SetJobTask(FS->hDlg,JOBSTATE_PAUSE);
	result = PMessageBox(FopGetMsgParentWnd(FS),msg,title,flags);
	SetJobTask(FS->hDlg,JOBSTATE_UNPAUSE);
	SetTaskBarButtonProgress(FS->hDlg,TBPF_NORMAL,0);
	return result;
}

ERRORCODE FileOperationReparse(FOPSTRUCT *FS,const TCHAR *src,TCHAR *dst,DWORD attr)
{
	int action;
	TCHAR orgsrc[VFPS];
	TCHAR text[CMDLINESIZE];

	GetReparsePath(src,orgsrc);
	if ( FS->reparsemode != FOPR_UNKNOWN ){
		action = FS->reparsemode;
	}else{
		wsprintf(text,T("%s ( %s )"),src,orgsrc);
		action = FopOperationMsgBox(FS,StrMES_QDRC,text,
				MB_PPX_YESNOIGNORECANCEL |
				MB_PPX_ALLCHECKBOX | MB_DEFBUTTON4 | MB_ICONQUESTION);
		if ( action > 0 && (action & ID_PPX_CHECKED) ){
			resetflag(action,ID_PPX_CHECKED);
			FS->reparsemode = action;
		}
	}
	if ( action == IDYES ){
		ERRORCODE result;

		if ( orgsrc[0] == '\0' ) return ERROR_ACCESS_DENIED;
		if ( (attr & FILE_ATTRIBUTE_DIRECTORY) &&
			 (VFSGetDriveType(orgsrc,NULL,NULL) != NULL) ){
			result = CreateJunction(dst,orgsrc,NULL);
		}else{
			result = FopCreateSymlink(dst,orgsrc,0);
		}
		if ( (result == NO_ERROR) && (FS->opt.fop.mode == FOPMODE_MOVE) ){
			if ( attr & FILE_ATTRIBUTE_READONLY ){
				SetFileAttributesL(src,FILE_ATTRIBUTE_NORMAL);
			}
			if ( attr & FILE_ATTRIBUTE_DIRECTORY ){
				RemoveDirectoryL(src);
			}else{
				DeleteFileL(src);
			}
		}
		return result;
	}else{
		if ( action == IDIGNORE ) return NO_ERROR; // 無視…何も行わないように
		if ( action != IDNO ) return ERROR_CANCELLED;
		return ERROR_MORE_DATA; // 処理の続行を指示する
	}
}

typedef struct {
	PPXAPPINFO info;
	PPXAPPINFO *ParentInfo;
	const TCHAR *cfgname;

	const TCHAR *user;
	const TCHAR *pass;
	const TCHAR *path;

	const TCHAR *src;
	const TCHAR *dest;
} AUXOPINFOSTRUCT;

extern INT_PTR CALLBACK FtpPassDlgBox(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam);

ERRORCODE UserPass(AUXOPINFOSTRUCT *info,BOOL pass,TCHAR *result)
{
	int mode,force;
	TCHAR *hostptr,*hosttop;
	TCHAR host[VFPS];

	if ( pass ){
		if ( *info->pass != '\0' ){
			tstrcpy(result,info->pass);
			return NO_ERROR;
		}
	}else{
		if ( *info->user != '\0' ){
			tstrcpy(result,info->user);
			return NO_ERROR;
		}
	}
	tstrcpy(host,info->path);
	hosttop = host;
	hostptr = VFSGetDriveType(host,&mode,&force);
	if ( hostptr != NULL ){ // ホスト検出に成功したときは、最後をホストにする
		*hostptr = '\0';
		hostptr = tstrrchr(host,'/');
		if ( hostptr == NULL ) hostptr = tstrrchr(host,'\\');
		if ( hostptr != NULL ) hosttop = hostptr + 1;
	}else{ // ホスト検出に失敗したときは、始め(xx:/を除く)をホストにする
		hostptr = tstrchr(host,':');
		if ( (hostptr != NULL) && (*(hostptr + 1) == '/') ){
			hostptr++;
			while ( *hostptr == '/' ) hostptr++;
			hosttop = hostptr;
		}
		hostptr = tstrchr(hosttop,'/');
		if ( hostptr == NULL ) hostptr = tstrrchr(hosttop,'\\');
		if ( hostptr != NULL ) *hostptr = '\0';
	}

	if ( tstrcmp(AuthHostCache,hosttop) ){
		TCHAR buf[0x1000],strbuf[VFPS],*bufp,user[0x100],pass[0x100];
		TCHAR dlgbuf[0x1000],*p;

		user[0] = '\0';
		pass[0] = '\0';
		if ( NO_ERROR == GetCustTable(T("_IDpwd"),hosttop,buf,sizeof(buf)) ){
			bufp = buf;

			ReadEString(&bufp,strbuf,sizeof(strbuf));
			tstrcpy(user,strbuf);
			tstrcpy(pass,strbuf + tstrlen(strbuf) + 1);
		}

		tstrcpy(dlgbuf,hosttop);
		p = dlgbuf + tstrlen(hosttop) + 1;
		tstrcpy(p,user);
		p += tstrlen(p) + 1;
		tstrcpy(p,pass);

		if ( PPxDialogBoxParam(DLLhInst,MAKEINTRESOURCE(IDD_LOGIN),
					NULL,FtpPassDlgBox,(LPARAM)dlgbuf) <= 0 ){
			return ERROR_CANCELLED;
		}
		if ( tstrlen(host) < TSIZEOF(AuthHostCache) ){
			tstrcpy(AuthHostCache,hosttop);
		}
		tstrcpy(AuthUserCache,dlgbuf);
		tstrcpy(AuthPassCache,dlgbuf + tstrlen(dlgbuf) + 1);
	}

	if ( pass ){
		tstrcpy(result,AuthPassCache);
	}else{
		tstrcpy(result,AuthUserCache);
	}
	return NO_ERROR;
}


DWORD_PTR USECDECL AuxOpFFFunc(AUXOPINFOSTRUCT *info,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
//		case PPXCMDID_STARTENUM:	// 検索開始(マーク無しもあり)…無視
//		case PPXCMDID_STARTNOENUM:	// 検索開始(マーク無しはなし)…無視
//		case PPXCMDID_NEXTENUM:		// 次へ…無視
//		case PPXCMDID_ENDENUM:		// 列挙終了…無視
//		case PPXCMDID_CHECKENUM:	//次の列挙は？…無視

		case PPXCMDID_FUNCTION:
			if ( !tstrcmp(uptr->funcparam.param,T("USER")) ){
				return UserPass(info,FALSE,uptr->funcparam.dest) ^ 1;
			}else if ( !tstrcmp(uptr->funcparam.param,T("PASS")) ){ // %*pass
				return UserPass(info,TRUE,uptr->funcparam.dest) ^ 1;
			}else if ( !tstrcmp(uptr->funcparam.param,T("PATH")) ){
				tstrcpy(uptr->funcparam.dest,info->path); // %*path
				return 1;
			}else if ( !tstrcmp(uptr->funcparam.param,T("SRC")) ){
				tstrcpy(uptr->funcparam.dest,info->src);
				return 1;
			}else if ( !tstrcmp(uptr->funcparam.param,T("DEST")) ){
				tstrcpy(uptr->funcparam.dest,info->dest);
				return 1;
			}

			uptr->funcparam.dest[0] = '\0';
			if ( NO_ERROR != GetCustTable(info->cfgname,uptr->funcparam.param,uptr->funcparam.dest,CMDLINESIZE) ){
				return 0;
			}
			PP_ExtractMacro(info->info.hWnd,&info->info,NULL,uptr->funcparam.dest,uptr->funcparam.dest,XEO_EXTRACTEXEC);

			return 1;

		default:
			if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
			return 0;
	}
}

ERRORCODE FileOperationAux(/*FOPSTRUCT *FS,*/const TCHAR *id,const TCHAR *AuxPath,const TCHAR *src,const TCHAR *dst)
{
	TCHAR path[VFPS];

	tstrcpy(path,AuxPath + 4);
	return AuxOperation(NULL,id,path,src,dst,NULL);
}

ERRORCODE AuxOperation(PPXAPPINFO *ParentInfo,const TCHAR *id,TCHAR *AuxPath,const TCHAR *src,const TCHAR *dst,TCHAR *resulttext)
{
	AUXOPINFOSTRUCT info;
	TCHAR cmdstr[CMDLINESIZE],*ptr = AuxPath,sep = '\\';
	const TCHAR *cfgname;

	info.ParentInfo = ParentInfo;
	info.user = NULL;
	info.pass = NULL;
	info.src = src;
	info.dest = dst;

	if ( *ptr == '/' ){
		sep = '/';
		while ( *ptr == '/' ) ptr++;
	}
	cfgname = ptr;
	for (;;){
		if ( *ptr == '\0' ) break;
		if ( *ptr == sep ){
			*ptr++ = '\0';
			break;
		}
		if ( (*ptr == ':') && (info.user == NULL) ){
			info.pass = ptr++;
			continue;
		}
		if ( (*ptr == '@') && (info.user == NULL) ){
			info.user = cfgname;
			*ptr++ = '\0';
			cfgname = ptr;
			continue;
		}
		ptr++;
	}
	if ( info.user == NULL ){
		info.user = NilStr;
		info.pass = NilStr;
	}else{
		if ( info.pass != NULL ){
			*(TCHAR *)info.pass++ = '\0';
		}
	}

	info.info.Function = (PPXAPPINFOFUNCTION)AuxOpFFFunc;
	info.info.Name = T("AuxOp");
	info.info.RegID = T("###");
	info.info.hWnd = NULL;
	info.cfgname = cfgname;
	info.path = ptr;

	cmdstr[0] = '\0';
	GetCustTable(cfgname,id,cmdstr,sizeof(cmdstr));
	if ( cmdstr[0] == '\0' ){
		return ERROR_INVALID_DRIVE;
	}
	return PP_ExtractMacro(NULL,&info.info,NULL,cmdstr,resulttext,
			(resulttext == NULL) ?
				(XEO_NOACTIVE | XEO_SEQUENTIAL) :
				(XEO_NOACTIVE | XEO_SEQUENTIAL | XEO_EXTRACTEXEC));
}

DWORD_PTR USECDECL FilenameInfoFunc(FILENAMEINFOSTRUCT *linfo,DWORD cmdID,PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case PPXCMDID_STARTENUM:	// 検索開始(マーク無しもあり)
		case PPXCMDID_STARTNOENUM:	// 検索開始(マーク無しはなし)
			uptr->enums.enumID = 0;
			break;

		case PPXCMDID_NEXTENUM:		// 次へ
		case PPXCMDID_ENDENUM:		//列挙終了
			uptr->enums.enumID = -1;
			return 0;

		case '1': {
			int mode = VFSPT_UNKNOWN;

			tstrcpy(uptr->enums.buffer,linfo->filename);
			VFSGetDriveType(uptr->enums.buffer,&mode,NULL);
			if ( (mode == VFSPT_DRIVE) || (mode == VFSPT_UNC) ){
				TCHAR *p;

				p = VFSFindLastEntry(uptr->enums.buffer);
				if ( p != uptr->enums.buffer ) *p = '\0';
				if ( *uptr->enums.buffer ) break;
			}
		}

		case 'C':
			if ( uptr->enums.enumID == -1 ){
				*uptr->enums.buffer = '\0';
				break;
			}
			// 'R' へ
		case 'R': {
			const TCHAR *p;

			p = FindLastEntryPoint(linfo->filename);
			tstrcpy(uptr->enums.buffer,p);
			break;
		}
		case PPXCMDID_ENUMATTR:
			if ( uptr->enums.enumID == -1 ){
				*uptr->enums.buffer = '\0';
				break;
			}
			*(DWORD *)uptr->enums.buffer = FILE_ATTRIBUTE_NORMAL;
			break;

		default:
			if ( cmdID <= PPXCMDID_FILL ) *uptr->enums.buffer = '\0';
			return 0;
	}
	return 1;
}
