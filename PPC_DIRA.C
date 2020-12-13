/*-----------------------------------------------------------------------------
	Paper Plane cUI					Directory読み込み - 非同期読み込み
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

/*
	dirty == TRUE なら自動廃棄される
	result == NO_ERROR ならデータが有効
*/
#define SAVE_REPORT RENTRYI_CACHE
#define SAVE_DUMP RENTRYI_REFRESHCACHE

#define FFASTATE_COMPLETE B0 // 読み込み完了
#define FFASTATE_CACHE B1 // １度使用済み。キャッシュ用
#define FFASTATE_ENABLEFREE B2 // FreeFfa で廃棄してもよい(読み込み完了したら立つ)
#define USEMEMCACHETIME 40 // メモリキャッシュに保存するときの判断に使うms

const TCHAR FindFirstAsyncThreadName[] = T("Async dir read");

typedef struct tagFindFirstAsync{
	struct tagFindFirstAsync *next;	// チェーン用
	ERRORCODE result;		// 結果
	volatile int ref;		// 参照数
	volatile int save;		// 0:キャッシュを保存しない
			// RENTRYI_CACHE SAVE_REPORT	通知flag(読み込み完了後に通知)
			// RENTRYI_REFRESHCACHE SAVE_DUMP 保存flag(読み込み完了後に保存)
	volatile BOOL dirty;	// 既に古い内容になった
	DWORD state;			// FFASTATE_
	DWORD count;			// 読み込み済みのエントリ数
	HANDLE hFind;			// VFSFindFirst のハンドル(ListFileのときに使用)
	TM_struct files;		// 読み込んだ内容
	TCHAR path[VFPS];		// 探すディレクトリ

	HWND hWnd;				// 通知先 NULL == 対象がない
	LPARAM lParam;			// 通知先メッセージ(cinfo->LoadCounter相当)
	DWORD LastTick;			// 読み込み完了時の時間(自動廃棄用)
	DWORD ReadTick;			// 読み込みに掛かった時間(廃棄判断用)

	VFSDIRTYPEINFO Dtype;
} FINDFIRSTASYNC;

typedef struct {
  FINDFIRSTASYNC *ffa;
  WIN32_FIND_DATA *read;
  DWORD count;
} FINDFIRSTASYNCHEAP;

FINDFIRSTASYNC *FindFirstAsyncList = NULL;

const TCHAR GetCache_PathStr[] = T("%s\\%08X");
const char  GetCache_CacheStrA[] = LHEADER ";Base=";
#define GetCache_CacheStrASize (sizeof(GetCache_CacheStrA) - 1)

const WCHAR GetCache_CacheStrW[] = UNICODESTR(LHEADER) L";Base=";
#define GetCache_CacheStrWSize (sizeof(GetCache_CacheStrW) - sizeof(WCHAR))

// *cache task 本体
void DirTaskCommand(PPC_APPINFO *cinfo)
{
	HMENU hMenu;
	TCHAR buf[VFPS * 2];
	FINDFIRSTASYNC *sffa;

	int count = 0;

	hMenu = CreatePopupMenu();

	EnterCriticalSection(&FindFirstAsyncSection);
	sffa = FindFirstAsyncList;
	while ( sffa != NULL ){
		wsprintf(buf, T("%d:%d %dentries(%d) %dms ref:%d%s%s%s %s"),
			sffa->LastTick, sffa->result, // ID: result
			sffa->count, sffa->files.s, // entries (size)
			sffa->ReadTick,
			sffa->ref,
			(sffa->state & FFASTATE_COMPLETE) ? NilStr : T("(read)"),
			(sffa->state & FFASTATE_CACHE) ? T("(cache)") : NilStr,
//			(sffa->hWnd != NULL) ? T("(used)") : NilStr,
			IsTrue(sffa->dirty) ? T("(dirty)") : NilStr,
			sffa->path);

		AppendMenuString(hMenu, ++count, buf);

		sffa = sffa->next;
	}
	LeaveCriticalSection(&FindFirstAsyncSection);

	if ( count == 0 ) AppendMenuString(hMenu, 0, T("No task"));

	PPcTrackPopupMenu(cinfo, hMenu);
	DestroyMenu(hMenu);
}

BOOL GetCache_Path(TCHAR *filename, const TCHAR *dpath, int *type)
{
	TCHAR basedir[VFPS], *tail, dirpath[VFPS];
	HANDLE hFile;
	int subid = 0;

	tstrcpy(dirpath, dpath);
	tail = tstrchr(dirpath, '*');
	if ( tail != NULL ) *(tail - 1) = '\0';

	basedir[0] = '\0';
	GetCustData(T("X_cache"), basedir, VFPS);
	if ( basedir[0] == '\0' ){
		MakeTempEntry(VFPS, basedir, 0);
		tstrcat(basedir, T("cache"));
		MakeDirectories(basedir, NULL);
	}else{
		VFSFixPath(NULL, basedir, PPcPath, VFSFIX_FULLPATH | VFSFIX_REALPATH);
	}
	tail = filename + wsprintf(filename, GetCache_PathStr, basedir, crc32((const BYTE *)dirpath, MAX32, 0));
	tstrcpy(tail, T(".txt"));
	for ( ; ; ){				// base が一致するファイルを求める
		DWORD size;
		BYTE temp[VFPS * sizeof(WCHAR) + sizeof(GetCache_CacheStrW)];
		TCHAR text[VFPS + 100], *p;

		hFile = CreateFileL(filename, GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
				OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if ( hFile == INVALID_HANDLE_VALUE ) return FALSE;
		if ( FALSE == ReadFile(hFile, temp, sizeof(temp)-2, &size, NULL) ) size = 0;
		temp[size] = 0;
		temp[size + 1] = 0;
		CloseHandle(hFile);

		if ( !memcmp(temp, GetCache_CacheStrA, GetCache_CacheStrASize) ){ // C6385 OK(0 書き込みで誤動作防止済み)
#ifdef UNICODE
			AnsiToUnicode((char *)(temp + GetCache_CacheStrASize), text, VFPS);
			p = text;
#else
			p = (char *)(temp + GetCache_CacheStrASize);
#endif
		}else if ( !memcmp(temp + 2, GetCache_CacheStrW, GetCache_CacheStrWSize) ){
#ifdef UNICODE
			p = (WCHAR *)(temp + 2 + GetCache_CacheStrWSize);
#else
			UnicodeToAnsi((WCHAR *)(temp + 2 + GetCache_CacheStrWSize), text, VFPS);
			p = text;
#endif
		}else{
			p = NULL;
		}
		if ( p != NULL ){
			TCHAR *z;

			z = tstrchr(p, '|');
			if ( z != NULL ){
				*z++= '\0';
				if ( !tstrcmp(p, dirpath) ){
					if ( type != NULL ) *type = GetIntNumber((const TCHAR **)&z);
					return TRUE;
				}
			}
		}
		// このファイルでなかったので次にする
		wsprintf(tail, T("%d.txt"), subid++);
	}
}

// キャッシュ生成要求(非同期処理した) && 特殊なディレクトリ以外なら
// キャッシュを保存する。
void DumpCache(FINDFIRSTASYNC *ffa)
{
	HANDLE hFile;
	TCHAR name[VFPS];
	int trycounter = 10;

	if ( !(ffa->save & SAVE_DUMP) ) return;	// 保存する必要がない
	// ドライブリスト、リストファイルは保存の対象外
	if ( (ffa->Dtype.mode == VFSDT_DLIST) ||
		 (ffa->Dtype.mode == VFSDT_LFILE) ){
		return;
	}
	GetCache_Path(name, ffa->path, NULL);
	for ( ; ; ){ // ERROR_SHARING_VIOLATION のときは少し待ってみる
		ERRORCODE error;

		hFile = CreateFileL(name, GENERIC_WRITE, 0, NULL,
				CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if ( hFile != INVALID_HANDLE_VALUE ) break;
		error = GetLastError();
		if ( error != ERROR_SHARING_VIOLATION ){
			PPErrorBox(NULL, CacheErrorTitle, error);
			break;
		}
		if ( trycounter-- == 0 ) break;
		Sleep(50);
	}
	if ( hFile != INVALID_HANDLE_VALUE ){
		DWORD tmp;
		WIN32_FIND_DATA *ff;
		int count;
		TCHAR bb[VFPS];
#ifdef UNICODE
		WriteFile(hFile, UCF2HEADER, UCF2HEADERSIZE, &tmp, NULL);
		WriteFile(hFile, GetCache_CacheStrW, GetCache_CacheStrWSize, &tmp, NULL);
#else
		WriteFile(hFile, GetCache_CacheStrA, GetCache_CacheStrASize, &tmp, NULL);
#endif
		WriteFile(hFile, ffa->path, TSTRLENGTH32(ffa->path), &tmp, NULL);
		tmp = wsprintf(bb, T("|%d\r\n"), ffa->Dtype.mode);

		WriteFile(hFile, bb, TSTROFF32(tmp), &tmp, NULL);

		ff = (WIN32_FIND_DATA *)ffa->files.p;
		count = ffa->count;
		while ( count ){
			if ( !IsRelativeDir(ff->cFileName) ){
				WriteFF(hFile, ff, ff->cFileName);
			}
			ff++;
			count--;
		}
		CloseHandle(hFile);
	}
}

void FreeFfa(FINDFIRSTASYNC *ffa)
{
	EnterCriticalSection(&FindFirstAsyncSection);

	if ( (ffa->state & FFASTATE_ENABLEFREE) && (ffa->ref <= 0) ){
		if ( FindFirstAsyncList != NULL ){ // リンクの調整
			if ( FindFirstAsyncList == ffa ){
				FindFirstAsyncList = ffa->next;
			}else{
				FINDFIRSTASYNC *ffalink;

				ffalink = FindFirstAsyncList;
				for (;;){
					if ( ffalink->next == NULL ) break; // リスト内に入っていないのでそのまま削除
					if ( ffalink->next == ffa ){
						ffalink->next = ffa->next; // 切り離し
						break;
					}
					ffalink = ffalink->next;
				}
			}
		}
		if ( ffa->hFind != NULL ) VFSFindClose(ffa->hFind);
		TM_kill(&ffa->files);
		PPcHeapFree(ffa);

	}else{ // スレッドがまだ生きているので廃棄指示をする
		ffa->hWnd = NULL; // 通知停止
		ffa->path[0] = '\0'; // 検索対象外
		ffa->dirty = TRUE; // 廃棄指示
	}

	LeaveCriticalSection(&FindFirstAsyncSection);
}

void USEFASTCALL GetDtypeInfo(HANDLE hFind, VFSDIRTYPEINFO *Dtype)
{
	VFSGetFFInfo(hFind, &Dtype->mode, Dtype->Name, &Dtype->ExtData);
	if ( Dtype->mode == VFSDT_LFILE ){
		TCHAR *p;

		VFSGetFFInfo(hFind, (int *)1, Dtype->BasePath, NULL);
		if ( NULL != (p = tstrchr(Dtype->BasePath, '|')) ) *p = '\0';
	}
}

DWORD WINAPI FindFirstAsyncThread(FINDFIRSTASYNC *ffa)
{
	THREADSTRUCT threadstruct = {FindFirstAsyncThreadName, XTHREAD_EXITENABLE | XTHREAD_TERMENABLE, NULL, 0, 0};
	HANDLE hFind = NULL;
	ULONG_PTR structsize;
	TCHAR dirpath[VFPS];

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	PPxRegisterThread(&threadstruct);
	if ( TM_check(&ffa->files, sizeof(ENTRYCELL) * 3) == FALSE ){
		ffa->result = ERROR_NOT_ENOUGH_MEMORY;
		hFind = INVALID_HANDLE_VALUE;
	}else if ( VFSArchiveSection(VFSAS_CHECK, NULL) == 0 ){
		ffa->result = VFSTryDirectory(ffa->hWnd, ffa->path, TRUE);
		if ( (ffa->result != NO_ERROR) &&
			 (ffa->result != ERROR_INVALID_PARAMETER) &&
			 (ffa->result != ERROR_DIRECTORY) &&
			 (ffa->result != ERROR_FILE_NOT_FOUND) &&
			 (ffa->result != ERROR_PATH_NOT_FOUND) &&
			 (ffa->result != ERROR_FILE_EXISTS) ){
			hFind = INVALID_HANDLE_VALUE;
		}
	}

	if ( hFind == NULL ){									// 通常読込
		CatPath(dirpath, ffa->path, T("*"));
		hFind = VFSFindFirst(dirpath, (WIN32_FIND_DATA *)ffa->files.p);

		if ( hFind == INVALID_HANDLE_VALUE ){	// 失敗
			ffa->result = GetLastError();
		}else{
			DWORD tick = ffa->LastTick; // CreateThread 時のtickを取得

			GetDtypeInfo(hFind, &ffa->Dtype);
			ffa->count++;
			if ( ffa->Dtype.mode == VFSDT_LFILE ){
				resetflag(ffa->save, SAVE_DUMP);
				ffa->hFind = hFind;
			}else{
				structsize = sizeof(WIN32_FIND_DATA);
				for ( ; ; ){
					DWORD newtick;

					if ( FALSE == VFSFindNext(hFind, (WIN32_FIND_DATA *)
							((BYTE *)ffa->files.p + structsize)) ){
						ERRORCODE error;

						error = GetLastError();
						if ( error == ERROR_MORE_DATA ){
							int chance;

							chance = 10;
							for ( ; ; ){
								Sleep(100);
								if ( FALSE != VFSFindNext(hFind, (WIN32_FIND_DATA *)
										((BYTE *)ffa->files.p + structsize)) ){
									error = NO_ERROR;
									break;
								}
								error = GetLastError();

								if ( error != ERROR_MORE_DATA ) break;
								chance--;
								if ( !chance ) break;
							}
							if ( error != NO_ERROR ) break;
						}else{
							break;
						}
					}
					if ( IsTrue(ffa->dirty) ){ // dirtyデータなので保存・通知しない
						ffa->save = 0;
						break;
					}
					structsize += sizeof(WIN32_FIND_DATA);
					if ( TM_check(&ffa->files, structsize +
							sizeof(WIN32_FIND_DATA)) == FALSE ){
						break;
					}
					ffa->count++;
					newtick = GetTickCount();
					if ( (newtick - tick) >= 100 ){		// 経過報告
						tick = newtick;
						if ( (ffa->save & SAVE_REPORT) && (ffa->hWnd != NULL) ){
							PostMessage(ffa->hWnd, WM_PPXCOMMAND, TMAKELPARAM
									(KC_RELOAD, min(ffa->count, 0xffff)),
									ffa->lParam);
						}
					}
				}
				VFSFindClose(hFind);
			}
			threadstruct.flag = 0;	// 強制終了を禁止
			ffa->result = NO_ERROR;
			DumpCache(ffa);
		}
	}
	if ( VFSArchiveSection(VFSAS_CHECK, NULL) == 0 ){
		SetCurrentDirectory(PPcPath);
	}
	if ( ffa->dirty == FALSE ){
		DWORD ntick = GetTickCount();

		if ( ffa->result == NO_ERROR ) ffa->ReadTick = ntick - ffa->LastTick;
		ffa->LastTick = ntick;
		if ( (ffa->save & SAVE_REPORT) && (ffa->hWnd != NULL) ){
			PostMessage(ffa->hWnd, WM_PPXCOMMAND, KC_RELOAD, ffa->lParam);
		}
		setflag(ffa->state, FFASTATE_COMPLETE | FFASTATE_ENABLEFREE);
	}else{
		setflag(ffa->state, FFASTATE_ENABLEFREE);
		FreeFfa(ffa); // dirty なので廃棄する
	}
	PPxUnRegisterThread();
	CoUninitialize();
	return 0;
}

void FindCloseAsync(HANDLE hFind, int flags)
{
	FINDFIRSTASYNC *ffa;

	if ( !(flags & RENTRYI_ASYNCREAD) ){
		VFSFindClose(hFind);
		return;
	}

	ffa = ((FINDFIRSTASYNCHEAP *)hFind)->ffa;
	if ( ffa->ReadTick < USEMEMCACHETIME ){
		EnterCriticalSection(&FindFirstAsyncSection);
		if ( ffa->ref > 0 ) ffa->ref--;
		LeaveCriticalSection(&FindFirstAsyncSection);
		FreeFfa(ffa);
	}else{
		BOOL listed = FALSE;

		EnterCriticalSection(&FindFirstAsyncSection);
		if ( FindFirstAsyncList != NULL ){ // リストに登録されているかを確認
			if ( FindFirstAsyncList == ffa ){
				listed = TRUE;
			}else{
				FINDFIRSTASYNC *ffalink;

				ffalink = FindFirstAsyncList;
				while ( ffalink->next != NULL ){
					if ( ffalink->next == ffa ){
						listed = TRUE;
						break;
					}
					ffalink = ffalink->next;
				}
			}
		}
		if ( ffa->ref > 0 ) ffa->ref--;
		LeaveCriticalSection(&FindFirstAsyncSection);
		if ( listed == FALSE ) FreeFfa(ffa); // リスト外なら解放
	}

	PPcHeapFree((FINDFIRSTASYNCHEAP *)hFind);
}

BOOL FindNextAsync(HANDLE hFind, WIN32_FIND_DATA *ff, int flags)
{
	FINDFIRSTASYNCHEAP *ffah;

	if ( !(flags & RENTRYI_ASYNCREAD) ) return VFSFindNext(hFind, ff);

	ffah = (FINDFIRSTASYNCHEAP *)hFind;
	if ( ffah->count == 0 ){
		SetLastError(ERROR_NO_MORE_FILES);
		return FALSE;
	}
	*ff = *ffah->read++;
	ffah->count--;
	return TRUE;
}

BOOL FindOptionDataAsync(HANDLE hFind, DWORD optionID, void *data, int flags)
{
	if ( flags & RENTRYI_ASYNCREAD ) return FALSE;
	return VFSFindOptionData(hFind, optionID, data);
}

HANDLE FindFirstAsyncReadStart(FINDFIRSTASYNC *ffa, WIN32_FIND_DATA *ff, VFSDIRTYPEINFO *Dtype, int *flags)
{
	FINDFIRSTASYNCHEAP *ffah;

	if ( (ffa->result == NO_ERROR) && (ffa->count == 0) ){	// エントリがないとき
		ffa->result = ERROR_NO_MORE_FILES;
	}
	if ( ffa->result != NO_ERROR ){
		ERRORCODE result = ffa->result;

		FreeFfa(ffa);
		SetLastError(result);
		return INVALID_HANDLE_VALUE;
	}
	setflag(ffa->state, FFASTATE_CACHE); // キャッシュ利用を有効

	if ( Dtype != NULL ) *Dtype = ffa->Dtype;

	if ( ffa->hFind != NULL ){ // FindFirst を直接使用するため、ffa を解放
		HANDLE hFind = ffa->hFind;

		resetflag(*flags, RENTRYI_ASYNCREAD);
		ffa->hFind = NULL;
		*ff = *((WIN32_FIND_DATA *)ffa->files.p);
		FreeFfa(ffa);
		return hFind;
	}
	ffa->ref++;

	ffah = PPcHeapAlloc( sizeof(FINDFIRSTASYNCHEAP) );
	ffah->ffa = ffa;
	ffah->read = ffa->files.p;
	ffah->count = ffa->count;

	FindNextAsync((HANDLE)ffah, ff, RENTRYI_ASYNCREAD);
	return (HANDLE)ffah;
}

#define TIMEOUTCHECKSW 0

#if TIMEOUTCHECKSW
	void TimeOutCheck(const TCHAR *path, const TCHAR *str, DWORD tick)
	{
		DWORD rtick = GetTickCount() - tick;
		if ( rtick < 10 ) return;
		XMessage(NULL, NULL, XM_DbgLOG, T("FindFirstAsync %s(Tick:%d)%s"), str, rtick, path);
	}
#else
	#define TimeOutCheck(path, str, tick)
#endif

/*	lParam	cinfo->LoadCounter
*/
HANDLE FindFirstAsync(HWND hWnd, LPARAM lParam, const TCHAR *path, WIN32_FIND_DATA *ff, VFSDIRTYPEINFO *Dtype, int *flags)
{
	FINDFIRSTASYNC *useffa = NULL, *sffa, *MemCacheFfa = NULL;
	DWORD tick;
	TCHAR name[VFPS];
	int sizecount = 0;

	if ( !(*flags & RENTRYI_ASYNCREAD) ){ // 非同期処理を行わないので通常読込を行う
		HANDLE hFind;

#if TIMEOUTCHECKSW
		tick = GetTickCount();
#endif
		CatPath(name, (TCHAR *)path, T("*"));
		hFind = VFSFindFirst(name, ff);

		TimeOutCheck(path, T("SyncRead-FF"), tick);

		if ( (hFind != INVALID_HANDLE_VALUE) && (Dtype != NULL) ){
			GetDtypeInfo(hFind, Dtype);
		}
		return hFind;
	}
		// 処理中リストに該当パスがあるか調べる
	EnterCriticalSection(&FindFirstAsyncSection);
	sffa = FindFirstAsyncList;
	tick = GetTickCount();
	while ( sffa != NULL ){
		if ( sffa->dirty != FALSE ){ // dirty のとき、廃棄可能なら廃棄
			FINDFIRSTASYNC *nextffa;

			nextffa = sffa->next;
			FreeFfa(sffa);
			sffa = nextffa;
			continue;
		}else if ( !tstrcmp(path, sffa->path) ){ // 同じディレクトリ
			if ( *flags & RENTRY_MODIFYUP ){	// 更新読込なら、廃棄する
				FINDFIRSTASYNC *nextffa;

				nextffa = sffa->next;
				FreeFfa(sffa);
				sffa = nextffa;
				continue;
												 // 通常読込中
			}else if ( sffa->state & FFASTATE_COMPLETE ){ // 読込完了->利用
				if ( sffa->state & FFASTATE_CACHE ){ // メモリキャッシュ用を発見
					// 同一パスの古いキャッシュを見つけたので廃棄
					if ( MemCacheFfa != NULL ){
						MemCacheFfa->ref--;
						FreeFfa(MemCacheFfa);
					}
					// キャッシュ利用可能として記憶
					sffa->lParam = lParam;
					MemCacheFfa = sffa;
					MemCacheFfa->ref++;
				}else{
					HANDLE hFF;
					// 同一パスの古いキャッシュを見つけたので廃棄
					if ( MemCacheFfa != NULL ){
						MemCacheFfa->ref--;
						FreeFfa(MemCacheFfa);
//						MemCacheFfa = NULL;
					}
					hFF = FindFirstAsyncReadStart(sffa, ff, Dtype, flags);
					LeaveCriticalSection(&FindFirstAsyncSection);
					return hFF;
				}
			}else{ // 読み込み中
				// ●読み込み中なので、MemCache か重複読み込みにする
				// ●できれば、複数に提供できるようにしたい
				if ( sffa->hWnd == hWnd ){
					useffa = sffa; // 自分の読込中なのでキャッシュ読み込みへ
				}
			}
		}

		// 読み込み中の内容が不要になったのでフリーにする
		if ( (sffa->hWnd == hWnd) && (sffa->lParam != lParam) ){
			setflag(sffa->state, FFASTATE_CACHE);
			sffa->hWnd = NULL; // 通知 off
		}
		sizecount += sffa->files.s;

		sffa = sffa->next;
	}
#if 1
	if ( sizecount > X_ardir[1] ){ // キャッシュが増えたので、入らない物を廃棄
		int usesize;

		sffa = FindFirstAsyncList;
		usesize = X_ardir[1] + (X_ardir[1] / 8);
		while ( sffa != NULL ){
			// フリーで破棄可能な物を廃棄
			if ( (sffa != useffa) && (sffa != MemCacheFfa) &&
				 (sffa->ref <= 0) && (sffa->state & FFASTATE_ENABLEFREE) ){
				FINDFIRSTASYNC *targetffa;

				sizecount -= sffa->files.s;
				targetffa = sffa;
				sffa = sffa->next;
				FreeFfa(targetffa); // dirty チェックをさせる
				if ( sizecount < usesize ) break;
			}else{
				sffa = sffa->next;
			}
		}
#ifndef RELEASE
		XMessage(NULL, NULL, XM_DbgLOG, T("Async FreeCache:%d"), sizecount);
#endif
	}
#endif
	LeaveCriticalSection(&FindFirstAsyncSection);

		// 処理中リストに該当がないため、読み込みスレッドを新規作成
	if ( useffa == NULL ){ // useffa != NULL ... 読み込み中スレッド有り
		HANDLE hComplete;
		DWORD tid;

		useffa = PPcHeapAlloc( sizeof(FINDFIRSTASYNC) );
		useffa->next = NULL;
		useffa->result = ERROR_BUSY;
		useffa->ref = 0;
		useffa->save = *flags & RENTRYI_REFRESHCACHE;
		useffa->dirty = FALSE;
		useffa->state = 0;
		useffa->count = 0;

		useffa->hWnd = hWnd;
		useffa->lParam = lParam;
		useffa->LastTick = tick;
		useffa->ReadTick = 0;

		useffa->hFind = NULL;
		useffa->files.p = NULL;
		useffa->files.s = 0;
		useffa->files.h = NULL;
		tstrcpy(useffa->path, path);

		if ( X_ardir[0] < 0 ) setflag(useffa->save, RENTRYI_REFRESHCACHE);

		hComplete = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE)FindFirstAsyncThread, useffa, 0, &tid);
		if ( hComplete == NULL ) return INVALID_HANDLE_VALUE;
#if 1
		TimeOutCheck(path, T("ASync-CreateThread"), tick);
		if ( X_ardir[0] >= 0 ){
			DWORD result = WaitForSingleObject(hComplete,
					((*flags & RENTRY_UPDATE) || (X_ardir[0] == 0)) ?
							30 : X_ardir[0] * 100);

			TimeOutCheck(path, T("ASync-Wait"), tick);

			if ( result == WAIT_OBJECT_0 ){	// 読み込み成功した
				CloseHandle(hComplete);
				// 同一パスのキャッシュがあれば、廃棄する
				if ( MemCacheFfa != NULL ){
					EnterCriticalSection(&FindFirstAsyncSection);
					MemCacheFfa->ref--;
					FreeFfa(MemCacheFfa);
					LeaveCriticalSection(&FindFirstAsyncSection);
				}
				return FindFirstAsyncReadStart(useffa, ff, Dtype, flags);
			}

			// 時間が経ったため、負担にならないようスレッド優先度を下げる
			SetThreadPriority(hComplete, THREAD_PRIORITY_BELOW_NORMAL);
		}
#endif
		CloseHandle(hComplete);

		if ( *flags & RENTRYI_SAVECACHE ){
			useffa->save = SAVE_REPORT | SAVE_DUMP; // 保存・通知を有効にする
		}else{
			useffa->save = SAVE_REPORT; // 通知のみ
		}
										// FindFirstAsyncList に登録
		EnterCriticalSection(&FindFirstAsyncSection);
		if ( FindFirstAsyncList == NULL ){
			FindFirstAsyncList = useffa;
		}else{
			FINDFIRSTASYNC *ffalink;

			ffalink = FindFirstAsyncList;
			while ( ffalink->next ) ffalink = ffalink->next;
			ffalink->next = useffa;
		}
		LeaveCriticalSection(&FindFirstAsyncSection);
	}

	if ( !(*flags & RENTRY_UPDATE) ){ // 時間が経ったためキャッシュを読み込む
									 // ※更新モード時は、キャッシュを使わない
		if ( MemCacheFfa != NULL ){
			HANDLE hFF;

			EnterCriticalSection(&FindFirstAsyncSection);
			hFF = FindFirstAsyncReadStart(MemCacheFfa, ff, Dtype, flags);
			MemCacheFfa->ref--; // 本関数内の参照を解除
			LeaveCriticalSection(&FindFirstAsyncSection);
			return hFF;
		}

		#if TIMEOUTCHECKSW
			tick = GetTickCount();
		#endif
		if ( IsTrue(GetCache_Path(name, path, &Dtype->mode)) ){ // ファイルキャッシュ有り
			HANDLE hFind;

			resetflag(*flags, RENTRYI_ASYNCREAD);
			tstrcat(name, T("\\*"));
			hFind = VFSFindFirst(name, ff);
			Dtype->ExtData = INVALID_HANDLE_VALUE;
			tstrcpy(Dtype->Name, T("??? "));

			TimeOutCheck(path, T("Cache read"), tick);
			return hFind;
		}
		// メモリキャッシュもファイルキャッシュもないので busy

	}else{ // 更新時、読み込み完了していないので busy
		if ( MemCacheFfa != NULL ){ // メモリキャッシュがあれば占有解除
			EnterCriticalSection(&FindFirstAsyncSection);
			MemCacheFfa->ref--; // 本関数内の参照を解除
			LeaveCriticalSection(&FindFirstAsyncSection);
		}
	}

	// 該当するキャッシュがなかった/キャッシュ未使用
	SetLastError(ERROR_BUSY);
	return INVALID_HANDLE_VALUE;	// 現在読み込み中
}
