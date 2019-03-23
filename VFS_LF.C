/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System				～ List File 処理 ～
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "VFS_FF.H"
#pragma hdrstop

const TCHAR TypeName_ListFile[] = T("List");

/*-----------------------------------------------------------------------------
	一行を取得する
-----------------------------------------------------------------------------*/
TCHAR *GetListLines(FF_LFILE *list)
{
	TCHAR *ptr,*bottom;
	const TCHAR *maxptr = list->maxptr;

	bottom = ptr = list->readptr;
	while ( ptr < maxptr ){
		bottom = ptr = SkipSpaceAndFix(ptr);
		if ( *ptr == '\0' ){		// 空行?
			ptr++;
			continue;
		}
		if ( *ptr == ';'){
			ptr++;
			while( *ptr ){
				if ( (*ptr == '\r') || (*ptr == '\n') ){
					ptr++;
					break;
				}
				ptr++;
			}
			continue;
		}
		while( *ptr ){		// コメントの除去
			if ( (*ptr == '\r') || (*ptr == '\n') ){
				*ptr++ = 0;
				if ( (*ptr == '\r') || (*ptr == '\n') ) ptr++;
				break;
			}
			ptr++;
		}
		if ( *bottom != '\0' ) break;
	}
	list->readptr = ptr;
	return bottom;
}
// 行末空白を除去２ -----------------------------------------------------------
void ReadFileTime(TCHAR **ptr,FILETIME *ft)
{
	ft->dwHighDateTime = (DWORD)GetNumber((const TCHAR **)ptr);
	if ( SkipSpace((const TCHAR **)ptr) == '.' ) (*ptr)++;
	ft->dwLowDateTime = (DWORD)GetNumber((const TCHAR **)ptr);
}

void GetP(TCHAR **ptr,TCHAR *dest,int size)
{
	TCHAR *maxptr;

	maxptr = dest + size - 1;
	(*ptr)++;
	while ( (**ptr != '\"') && ((UTCHAR)**ptr) ){
		if ( dest < maxptr ){
			*dest++ = *(*ptr)++;
		}else{
			(*ptr)++;
		}
	}
	(*ptr)++;
	*dest = '\0';
}

BOOL GetListLine(FF_LFILE *list,WIN32_FIND_DATA *findfile)
{
	TCHAR *line;

	if ( list->readptr >= list->maxptr ) goto EOL;
	line = GetListLines(list);

	memset(findfile,0,(BYTE *)findfile->cFileName - (BYTE *)findfile + sizeof(TCHAR));
	// findfile->cFileName[0] = '\0'; // ↑で初期化済み
	findfile->cAlternateFileName[0] = '\0';
	list->longname = NULL;
	list->comment = NULL;

	if ( SkipSpace((const TCHAR **)&line) != '\"' ){ // " なし…ファイル名のみ
		DWORD atr;
		TCHAR *dest,*destmax;
		const TCHAR *lineptr;

		if ( *line == '\0' ) goto EOL;
		atr = GetFileAttributesL(line);
		if ( atr != BADATTR ){
			findfile->dwFileAttributes = atr | FILE_ATTRIBUTEX_NODETAIL;
		}

		dest = findfile->cFileName;
		destmax = dest + MAX_PATH - 1;
		lineptr = line;
		while ( *lineptr != '\0' ){
			*dest++ = *lineptr++;
			if ( dest >= destmax ){
				list->longname = line;
				findfile->cFileName[0] = '>';
				tstrlimcpy(findfile->cFileName + 1,line,MAX_PATH);
				break;
			}
		}
		*dest = '\0';
	}else{	// " あり…各種属性有り
		GetP(&line,findfile->cFileName,MAX_PATH);
		if ( SkipSpace((const TCHAR **)&line) == ',' ){
			line++;
			while ( *line ){
				TCHAR c;

				c = SkipSpace((const TCHAR **)&line);
				if ( c == '\"' ){	// 単独 " … SFN
					GetP(&line,findfile->cAlternateFileName,13);
				}else if ( *(line + 1) == ':' ){
					line += 2;
					switch (c){	// A:x	属性
						case 'A':
							findfile->dwFileAttributes = GetNumber((const TCHAR **)&line) &
								~(FILE_ATTRIBUTEX_LF_MARK | FILE_ATTRIBUTEX_LF_COMMENT);
							break;
						case 'C':	// C:x.x 作成時刻
							ReadFileTime(&line,&findfile->ftCreationTime);
							break;
						case 'L':	// L:x.x 最終アクセス時刻
							ReadFileTime(&line,&findfile->ftLastAccessTime);
							break;
						case 'W':	// W:x.x 更新時刻
							ReadFileTime(&line,&findfile->ftLastWriteTime);
							break;
						case 'S':	// S:x.x サイズ
							findfile->nFileSizeHigh = (DWORD)GetNumber((const TCHAR **)&line);
							if ( SkipSpace((const TCHAR **)&line) == '.' ) line++;
							findfile->nFileSizeLow = (DWORD)GetNumber((const TCHAR **)&line);
							break;
						case 'R':	// R:n dwReserved0
							findfile->dwReserved0 = (DWORD)GetNumber((const TCHAR **)&line);
							break;
						case 'M':	// M:0/1 マーク
							if ( *line == '1' ){
								setflag(findfile->dwFileAttributes,FILE_ATTRIBUTEX_LF_MARK);
							}
							while ( Isdigit(*line) ) line++;
							break;
						case 'T': {	// T:… コメント
							UTCHAR c1,c2;

							c1 = *(line + 0);
							c2 = *(line + 1);
							if ( ((c1 == '\"') && (c2 != '\"') && (c2 >= ' '))
								|| ((c1 != ',') && (c1 >= ' ')) ){
								*(const TCHAR **)(&findfile->dwReserved0) = line;
								setflag(findfile->dwFileAttributes,FILE_ATTRIBUTEX_LF_COMMENT);
							}
						}
							// default へ
						default:	// 未定義…解析終了
							line = T("");
					}
				}else{
					break;
				}
				if ( SkipSpace((const TCHAR **)&line) != ',' ) break;
				line++;
			}
		}else{
			DWORD attr;

			attr = GetFileAttributesL(findfile->cFileName);
			if ( attr != BADATTR ) findfile->dwFileAttributes = attr;
		}
	}
	return TRUE;
EOL:
	SetLastError(ERROR_NO_MORE_FILES);
	return FALSE;
}

ERRORCODE InitFindFirstListFile(VFSFINDFIRST *VFF,const TCHAR *Fname,WIN32_FIND_DATA *findfile)
{
	ERRORCODE result;

	result = LoadTextImage(Fname, &VFF->v.LFILE.mem, &VFF->v.LFILE.readptr, &VFF->v.LFILE.maxptr);
	if ( result == NO_ERROR ){
		VFF->v.LFILE.type = VFSDT_LFILE_TYPE_PARENT;
		VFF->v.LFILE.base = NilStr;
		VFF->v.LFILE.search = NilStr;
		VFF->mode = VFSDT_LFILE;
		VFF->TypeName = TypeName_ListFile;
		SetDummydir(findfile,T("."));
		findfile->dwReserved0 = 0;
		findfile->dwReserved1 = 0;

		while ( VFF->v.LFILE.readptr < (VFF->v.LFILE.maxptr - 2) ){
			const TCHAR *line;

			if ( *VFF->v.LFILE.readptr != ';' ) break;
			VFF->v.LFILE.readptr++;
			line = GetListLines(&VFF->v.LFILE);
			if ( memcmp(line,T("Base="),5 * sizeof(TCHAR)) == 0 ){
				VFF->v.LFILE.base = line + 5;
			}
			if ( memcmp(line,T("Search="),7 * sizeof(TCHAR)) == 0 ){
				VFF->v.LFILE.search = line + 7;
			}
			if ( memcmp(line,T("Error="),6 * sizeof(TCHAR)) == 0 ){
				line += 6;
				result = (ERRORCODE)GetNumber((const TCHAR **)&line);
			}
			if ( memcmp(line,T("Option=directory"),16 * sizeof(TCHAR)) == 0 ){
				setflag(findfile->dwFileAttributes,FILE_ATTRIBUTEX_FOLDER);
			}
			if ( memcmp(line,T("Option=archive"),14 * sizeof(TCHAR)) == 0 ){
				setflag(findfile->dwFileAttributes,FILE_ATTRIBUTEX_FOLDER | FILE_ATTRIBUTE_ARCHIVE);
			}
		}
	}
	return result;
}
