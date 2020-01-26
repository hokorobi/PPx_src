//-----------------------------------------------------------------------------
//	共有メモリ関係
//-----------------------------------------------------------------------------
#include "WINAPI.H"
#include "PPX.H"
#include "TMEM.H"
#include "PPD_DEF.H"

//------------------------------------- 共有メモリを解放する
void FileMappingOff(FM_H *fm)
{
	if ( fm->ptr != NULL ) UnmapViewOfFile(fm->ptr);
	if ( fm->mapH != NULL ) CloseHandle(fm->mapH);
	if ( (fm->fileH != NULL) && (fm->fileH != INVALID_HANDLE_VALUE) ){
		CloseHandle(fm->fileH);
	}
}
/*------------------------------------- 共有メモリを確保する
	fm			共有メモリ管理構造体
	fname		ファイルマッピングに使用するフルパスのファイル名
	mapname		ファイルマッピング名
	size		確保サイズ
	atr			FILE_ATTRIBUTE_NORMAL								通常
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE	一時
*/
int FileMappingOn(FM_H *fm, const TCHAR *fname, const TCHAR *mapname, DWORD size, DWORD atr)
{
	int status = 0;
	ERRORCODE err;
	TCHAR localmapname[MAX_PATH];

	fm->fileH = NULL;
	// fm->mapH = NULL;
	fm->ptr = NULL;

	tstrcpy(localmapname, UserName);
	tstrcat(localmapname, mapname);
	//************************** NT-- OpenFileMapping -> CreateFile
	#ifndef UNICODE
	//************************** 95-- CreateFile -> OpenFileMapping
	if ( OSver.dwPlatformId != VER_PLATFORM_WIN32_NT ){
		if ( atr & FILE_FLAG_DELETE_ON_CLOSE ){
			fm->fileH = INVALID_HANDLE_VALUE;
		}else{
												// ファイルを作成する
		// 95 ではあらかじめ作成しておく
		// → 95 はここに置かないと最後まで管理してくれない（tmp時）
			fm->fileH = CreateFile(fname, GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
					OPEN_ALWAYS, atr, NULL);
			err = GetLastError();
			if ( err == NO_ERROR ){	// ファイル新規作成
				status = FMAP_CREATEFILE;
			}else if ( err != ERROR_ALREADY_EXISTS ){ // 作成失敗
				status = FMAP_FILEOPENERROR;
			}
		}
	}
	#endif
										// 共有メモリを作成する
	fm->mapH = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, localmapname);

	if ( fm->mapH == NULL ){		// Open に失敗→新規作成する必要があるかも
		#ifndef UNICODE
		if ( OSver.dwPlatformId == VER_PLATFORM_WIN32_NT )
		#endif
		{
			if ( atr & FILE_FLAG_DELETE_ON_CLOSE ){
				fm->fileH = INVALID_HANDLE_VALUE;
			}else{
													// ファイルを作成する
				// NT ではここで作成する
				// CreateFile ではエラーがでても無視する
				fm->fileH = CreateFile(fname, GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
						OPEN_ALWAYS, atr, NULL);
				err = GetLastError();
				if ( err == NO_ERROR ){	// ファイル新規作成
					status = FMAP_CREATEFILE;
				}else if ( err != ERROR_ALREADY_EXISTS ){ // 作成失敗
					status = FMAP_FILEOPENERROR;
				}
			}
		}
		// ※ 前のOpenFileMappingの後に、別スレッドでCreateFileMappingが
		// 完了していてもここでハンドルを得ることができる。
		fm->mapH = CreateFileMapping(fm->fileH, NULL, PAGE_READWRITE,
				0, size, localmapname);
		if ( fm->mapH == NULL ){
			FileMappingOff(fm);
			return -1;
		}
		status++;	// 新規作成 (FMAP_CREATEMAP)
	}
										// 共有メモリを配置する
	fm->ptr = MapViewOfFile(fm->mapH, FILE_MAP_ALL_ACCESS, 0, 0, size);
	if ( fm->ptr == NULL ){
		FileMappingOff(fm);
		return -1;
	}
	return status;
}
