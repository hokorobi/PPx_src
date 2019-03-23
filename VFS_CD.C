/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System	CD-ROM File System Image 処理
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "PPX.H"
#include "PPX_64.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "VFS_FF.H"
#pragma hdrstop

#define DEFAULTCDSECTOR	2048	// 一般のセクタサイズ

BOOL SetCDSector(CDS *cds,DWORD sector);
BOOL FindEntryUDFImage(CDS *cds,const TCHAR *fname,WIN32_FIND_DATA *ff);
DWORD ReadPathdata(CDS *cds);
BOOL FixSector(CDS *cds);
void CheckAVDP(CDS *cds);

//------------------------------------- ディスクイメージの参照を終了する
void CloseCDImage(CDS *cds)
{
	if ( cds->Buffer.raw != NULL ) HeapFree(DLLheap,0,cds->Buffer.raw);
	if ( cds->hFile != NULL ) CloseHandle( cds->hFile );
}
//------------------------------------- ディスクイメージの参照を開始する
BOOL OpenCDImage(CDS *cds,const TCHAR *fname,int offset)
{
	DWORD rsize;
	PVDSTRUCT *pvd;

	cds->hFile = NULL;
	cds->Buffer.raw = NULL;

	cds->hFile = CreateFileL(fname,GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,NULL);
	if ( cds->hFile == INVALID_HANDLE_VALUE ) return FALSE;
	cds->Buffer.raw = HeapAlloc(DLLheap,0,DEFAULTCDSECTOR * 2);
	if ( cds->Buffer.raw == NULL ) goto error;
	pvd = cds->Buffer.pvd;
	cds->mode = CD_ISO9660;
	cds->PathTable = 0;
	cds->Offset = 0;
	cds->dataoffset = 0;
	cds->ReadBpS = cds->c.BpC = DEFAULTCDSECTOR; //pvd->SectorSize[0];
	if ( offset & (DEFAULTCDSECTOR - 1) ){	// データ部以外もある
		cds->ReadBpS = 2352;	// データ部分以外も
		cds->Offset = offset & 0xff;
	}
	if ( offset == CDHEADEROFFSET4 ){ // この形式に限定
		cds->Offset = 0x4b000;
	}
	for ( ; ; ){ // ISO9660 の確認
										// セクタを読み込む -------------
		if ( MAX32 == SetFilePointer(cds->hFile,offset,NULL,FILE_BEGIN) ){
			goto error;
		}
		if ( FALSE == ReadFile(cds->hFile,
				cds->Buffer.raw,DEFAULTCDSECTOR,&rsize,NULL) ){
			goto error;
		}
		if ( rsize != DEFAULTCDSECTOR ) goto error;
										// PVD/SVD の正当性チェック
		if ( memcmp(pvd->ID,"CD001",sizeof pvd->ID) ) break;
		if ( pvd->type == 1 ){						// PVD から情報取得
			cds->PathTable = pvd->PathTable1LE;
		}else if ( pvd->type == 2 ){				// SVD が Joliet か？
			if ( !memcmp(pvd->ESC,"%/",2) ){
				cds->mode = CD_JULIET;
				cds->PathTable = pvd->PathTable1LE;
				break;
			}
		}else if ( pvd->type == 0xff ) break;
		offset += cds->ReadBpS;
	}
										// バッファを確保し直す ---------
	if ( cds->c.BpC > DEFAULTCDSECTOR ){
		char *ptr;

		ptr = HeapReAlloc(DLLheap,0,cds->Buffer.raw,cds->c.BpC * 2);
		if ( ptr == NULL ) goto error;
		cds->Buffer.raw = (BYTE *)ptr;
	}
	// UDF チェック -----------------------------------------------------------
										// セクタを読み込む -------------
	// AVDP があるか？(LBN=256or512,last sector,last sector - 256の何処か)
	if ( MAX32 != SetFilePointer(cds->hFile,cds->ReadBpS * 256 + cds->Offset,NULL,FILE_BEGIN) ){ //LBN=256
		CheckAVDP(cds);
	}
	if ( cds->PathTable == 0 ) goto error;
	return TRUE;
error:
	CloseCDImage(cds);
	return FALSE;
}

							// エンディアンを反転してコピー
void USEFASTCALL BEUnicodeCopy(WCHAR *dst,char *src,DWORD length)
{
	for ( ; length ; length--){
		*dst++ = (WCHAR)((BYTE)*(src + 1) | ((WCHAR)((BYTE)*src) << 8));
		src += 2;
	}
	*dst = '\0';
}
#ifndef UNICODE
void BEUnicodeToAnsi(char *src,char *dst,DWORD length)
{
	WCHAR cnvbuf[128];

	length /= 2;
	BEUnicodeCopy(cnvbuf,src,length);
	WideCharToMultiByte(CP_ACP,0,cnvbuf,-1,dst,128,"?",NULL);
}
#endif

BOOL FindEntryCDImage(CDS *cds,const TCHAR *fname,WIN32_FIND_DATA *ff)
{
	DIRECTORYRECORD *dir;

	if ( cds->mode == CD_UDF ) return FindEntryUDFImage(cds,fname,ff);
	if ( fname != NULL ){		// First
		TCHAR filename[VFPS];
		TCHAR *now,*next;
		DWORD extend = 0;
		int no = 1;
		int parent = 1;

		tstrcpy(filename,fname);
		cds->nowdir = cds->PathTable;
		if ( ReadPathdata(cds) == FALSE ){
			cds->nowentry = NULL;
			return FALSE;
		}
		now = filename;
		next = FindPathSeparator(filename);
		if ( next != NULL ){
			*next = '\0';
		}else{
			if ( *filename != '\0' ) next = filename + tstrlen(filename) - 1;
		}

		if ( *now == '\0' ){	// Root は１番目なので決めうちで
			extend = ((PATHRECORD *)cds->nowentry)->extend;
		}else for ( ; ; ){
			PATHRECORD *pdir;
			TCHAR name[256];

			pdir = (PATHRECORD *)cds->nowentry;
			if ( pdir->len == 0 ) break;	// 最後だった
			if ( pdir->parent > parent ) break;	// もう検索しても見つからない
			if ( (pdir->parent == parent) && (no > 1) ){
				if ( cds->mode == CD_ISO9660 ){
				#ifdef UNICODE
					int len;
					len = MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,
							pdir->name,pdir->len,name,TSIZEOF(name));
					name[len] = '\0';
				#else
					memcpy(name,pdir->name,pdir->len);
					name[pdir->len] = '\0';
				#endif
				}else{ // Juliet
				#ifdef UNICODE
					BEUnicodeCopy(name,pdir->name,pdir->len / sizeof(WCHAR));
				#else
					BEUnicodeToAnsi(pdir->name,name,pdir->len);
				#endif
				}
				if ( !tstricmp(name,now) ){
					parent = no;
					now = next + 1;
					if ( *now == '\0' ){
						extend = pdir->extend;
						break;
					}

					next = FindPathSeparator(now);
					if ( next != NULL ){
						*next = '\0';
					}else{
						if (*now) next = now + tstrlen(now) - 1;
					}
				}
			}
			cds->nowentry += sizeof(PATHRECORD) - 1 +
							 ((pdir->len + pdir->extlen + 1) & (MAX32 - 1));
			no++;
			if ( FixSector(cds) == FALSE ) return FALSE;
		}
		if ( extend == 0 ){
			cds->nowentry = NULL;
			return FALSE;
		}
		cds->nowdir = extend;
		if ( ReadPathdata(cds) == FALSE ){
			cds->nowentry = NULL;
			return FALSE;
		}
	}else{							// Next
		if ( cds->nowentry == NULL ) return FALSE;
	}
	for ( ; ; ){
		TCHAR *p;
		DWORD atr,UTCL,UTCH;
		SYSTEMTIME lTime;

		dir = (DIRECTORYRECORD *)cds->nowentry;
											// このセクタの最後→次のセクタへ
		if ( !dir->next || ((cds->Buffer.raw + cds->c.BpC) < cds->nowentry ) ){
			cds->nowdir++;
			cds->nowleft -= cds->c.BpC;
			if ( (cds->nowleft <= 0) || (ReadPathdata(cds) == FALSE) ){
				cds->nowentry = NULL;
				return FALSE;
			}
			continue;
		}
		if ( dir->len == 1 ){	// 「.」「..」
			if ( dir->name[0] == 0 ){	// このエントリの大きさも同時に取得
				cds->nowleft = dir->size[0];
				SetDummydir(ff,T("."));
				break;
			}
			if ( dir->name[0] == 1 ){
				SetDummydir(ff,T(".."));
				break;
			}
		}

		if ( cds->mode == CD_ISO9660 ){
			#ifdef UNICODE
			{
				int len;
				len = MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,
							dir->name,dir->len,ff->cFileName,MAX_PATH);
				ff->cFileName[len] = '\0';
			}
			#else
				memcpy(ff->cFileName,dir->name,dir->len);
				ff->cFileName[dir->len] = '\0';
			#endif
		}else{ // Juliet
			#ifdef UNICODE
				BEUnicodeCopy(ff->cFileName,dir->name,dir->len / sizeof(WCHAR));
			#else
				BEUnicodeToAnsi(dir->name,ff->cFileName,dir->len);
			#endif
		}
		p = tstrchr(ff->cFileName,';');
		if ( p != NULL ) *p = '\0';
		ff->cAlternateFileName[0] = '\0';

		atr = 0;
		if ( dir->flags & CDATR_HIDDEN ){
			setflag(atr,FILE_ATTRIBUTE_HIDDEN);
		}
		if ( dir->flags & CDATR_DIRECTORY ){
			setflag(atr,FILE_ATTRIBUTE_DIRECTORY);
			ff->nFileSizeLow = 0;
		}else{
			ff->nFileSizeLow = dir->size[0];
		}
		if ( dir->flags & CDATR_PROTECTED ){
			setflag(atr,FILE_ATTRIBUTE_READONLY);
		}

		cds->entryindex = dir->extend[0];
		ff->dwFileAttributes = atr;
		ff->nFileSizeHigh = 0;
		ff->ftCreationTime.dwLowDateTime = 0;
		ff->ftCreationTime.dwHighDateTime = 0;
		ff->ftLastAccessTime.dwLowDateTime = 0;
		ff->ftLastAccessTime.dwHighDateTime = 0;
		lTime.wYear = (WORD)(dir->date.year + 1900);
		lTime.wMonth = dir->date.month;
		lTime.wDay = dir->date.day;
		lTime.wHour = dir->date.hour;
		lTime.wMinute = dir->date.minute;
		lTime.wSecond = dir->date.second;
		lTime.wMilliseconds = 0;
		SystemTimeToFileTime(&lTime,&ff->ftLastWriteTime);

		DDmul(dir->date.UTC * 15 * 60,1000 * 1000 * 10,&UTCL,&UTCH);
		if ( dir->date.UTC < 0 ){ // 負の補正
			UTCH += 0xff676980;
		}
		SubDD(ff->ftLastWriteTime.dwLowDateTime,
				ff->ftLastWriteTime.dwHighDateTime,UTCL,UTCH);
		break;
	}
	cds->ReadFileNext = FALSE;
	cds->nowentry += dir->next;
	if ( dir->next < 0x20 ) cds->nowentry = NULL;
	return TRUE;
}

DWORD ReadPathdata(CDS *cds)
{
	DWORD rsize;

	if ( FALSE == SetCDSector(cds,cds->nowdir) ) return FALSE;
	if ( FALSE == ReadFile(cds->hFile,cds->Buffer.raw,cds->c.BpC * 2,&rsize,NULL) ){
		return FALSE;
	}
	if ( rsize < cds->c.BpC ) return FALSE;
	cds->nowentry = cds->Buffer.raw;
//	XMessage(NULL,NULL,XM_DUMPLOG,(char *)cds->Buffer.raw,0x100);
	return TRUE;
}

BOOL FixSector(CDS *cds)
{
	if ( cds->nowentry > (cds->Buffer.raw + cds->c.BpC) ){
		DWORD off;

		off = ToSIZE32_T(cds->nowentry - (cds->Buffer.raw + cds->c.BpC));
		cds->nowdir++;
		if ( ReadPathdata(cds) == FALSE ){
			cds->nowentry = NULL;
			return FALSE;
		}
		cds->nowentry += off;
	}
	return TRUE;
}

DWORD GetUDFSectorFromFE(UDF_FE *fe)
{
	DWORD L_EA;
	UDFSHORTAD *sad;

	L_EA = fe->L_EA;
	if ( L_EA >= (DEFAULTCDSECTOR - sizeof(UDFSHORTAD) - 0xb0) ) return 0;
	sad = (UDFSHORTAD *)&(fe->EA[L_EA]);
	return sad->position;
}

DWORD GetUDFSectorFromEFE(UDF_EFE *efe)
{
	DWORD L_EA;
	UDFSHORTAD *sad;

	L_EA = efe->L_EA;
	if ( L_EA >= (DEFAULTCDSECTOR - sizeof(UDFSHORTAD) - 0xd8) ) return 0;
	sad = (UDFSHORTAD *)&(efe->EA[L_EA]);
	return sad->position;
}

BOOL SetCDSector(CDS *cds,DWORD sector)
{
	DWORD seekL,seekH;

	DDmul(sector,cds->ReadBpS,&seekL,&seekH);
	AddDD(seekL,seekH,cds->Offset,0);
	if ( (MAX32 == SetFilePointer(cds->hFile,seekL,(LONG *)&seekH,FILE_BEGIN)) &&
		 (GetLastError() != NO_ERROR) ){
		return FALSE;
	}
	return TRUE;
}

void FixUDFTime(FILETIME *ftime,UDFTIME *utime)
{
	int tz;
	SYSTEMTIME lTime;
	DWORD UTCL,UTCH;

	lTime.wYear   = utime->year;
	lTime.wMonth  = utime->month;
	lTime.wDay    = utime->day;
	lTime.wHour   = utime->hour;
	lTime.wMinute = utime->minute;
	lTime.wSecond = utime->second;
	lTime.wMilliseconds = utime->msecond;
	SystemTimeToFileTime(&lTime,ftime);
	tz = (int)(utime->tz & 0xfff);
	if ( tz == 0x801 ) tz = 0;
	DDmul(tz,60 * 1000 * 1000 * 10,&UTCL,&UTCH); // 分単位に変更
	if ( tz >= 0x800 ) SubDD(UTCL,UTCH,0x34600000,0x23c); // 負の場合の補正
	SubDD(ftime->dwLowDateTime,ftime->dwHighDateTime,UTCL,UTCH);
}

#ifdef UNICODE
BOOL GetUDFFileName(WCHAR *dest,UDFFID *FID)
{
	BYTE *name;
	int size;

	size = FID->L_FI - 1;
	if ( size <= 0 ) return FALSE;
	name = (BYTE *)&FID->IU[FID->L_IU];
	if ( name[0] == 8 ){
		name++;
		while (size-- > 0){
			*dest++ = (WCHAR)*name++;
		}
	}else if ( name[0] == 0x10 ){
		name++;
		for ( ; size > 0 ; size -= 2){
			*dest++ = (WCHAR)((BYTE)*(name + 1) | ((WCHAR)((BYTE)*name) << 8));
			name += 2;
		}
	}else{
		return FALSE;
	}
	*dest = '\0';
	return TRUE;
}
#else
BOOL GetUDFFileName(char *dest,UDFFID *FID)
{
	BYTE *name;
	int size;

	size = FID->L_FI - 1;
	if ( size <= 0 ) return FALSE;
	name = (BYTE *)&FID->IU[FID->L_IU];
	if ( name[0] == 8 ){
		name++;
		while (size-- > 0){
			*dest++ = *name++;
		}
		*dest = '\0';
	}else if ( name[0] == 0x10 ){
		WORD *wdest,buf[258];

		name++;
		wdest = buf;
		for ( ; size > 0 ; size -= 2){
			*wdest++ = (WCHAR)((BYTE)*(name + 1) | ((WCHAR)((BYTE)*name) << 8));
			name += 2;
		}
		*wdest = '\0';
		WideCharToMultiByte(CP_ACP,0,(WORD *)buf,-1,dest,256,"?",NULL);
	}else{
		return FALSE;
	}
	return TRUE;
}
#endif

BOOL FindEntryUDFImage(CDS *cds,const TCHAR *fname,WIN32_FIND_DATA *ff)
{
	if ( fname != NULL ){		// First
		TCHAR filename[VFPS];
		TCHAR *now,*next;

		tstrcpy(filename,fname);
		// RootDir を取得
		cds->nowdir = cds->PathTable;
		if ( ReadPathdata(cds) == FALSE ){
			cds->nowentry = NULL;
			return FALSE;
		}

		now = filename;
		next = FindPathSeparator(filename);
		if ( next != NULL ){
			*next = '\0';
		}else{
			if ( *filename != '\0' ) next = filename + tstrlen(filename) - 1;
		}
		if ( *now != '\0' ) for ( ; ; ){
			TCHAR name[258];
			UDFFID *FID;

			FID = (UDFFID *)cds->nowentry;
			if ( FID->tag.id != UDF_FID_ID ){
				cds->nowentry = NULL;
				return FALSE;
			}

			// 次の場所
			cds->nowentry = (BYTE *)FID + FID->tag.len + sizeof(UDFTAG);
			if ( IsTrue(GetUDFFileName(name,FID)) ){
				// 名前と属性が一致
				if ( !tstricmp(name,now) && (FID->FC & UDFATR_DIRECTORY) ){

					// 該当エントリのFEを取得
					cds->nowdir = FID->ICB.location.LBN;
					ReadPathdata(cds);

					if ( cds->Buffer.udftag->id == UDF_FE_ID ){
						UDF_FE *fe;

						fe = cds->Buffer.fe;
						if ( (fe->ICB.Flags & 3) == 3 ){
							cds->nowdir = 0;
							cds->nowentry = (BYTE *)&(fe->EA[fe->L_EA]);
						}else{
							cds->nowdir = GetUDFSectorFromFE(fe);
							if ( (cds->nowdir == 0) || (ReadPathdata(cds) == FALSE) ){
								cds->nowentry = NULL;
								return FALSE;
							}
						}
					}else if ( cds->Buffer.udftag->id == UDF_EFE_ID ){
						UDF_EFE *efe;

						efe = cds->Buffer.efe;
						if ( (efe->ICB.Flags & 3) == 3 ){
							cds->nowdir = 0;
							cds->nowentry = (BYTE *)&(efe->EA[efe->L_EA]);
						}else{
							cds->nowdir = GetUDFSectorFromEFE(efe);
							if ( (cds->nowdir == 0) || (ReadPathdata(cds) == FALSE) ){
								cds->nowentry = NULL;
								return FALSE;
							}
						}
					}else{
						cds->nowentry = NULL;
						return FALSE;
					}

					// 末尾エントリなら完了
					now = next + 1;
					if ( *now == '\0' ) break;

					next = FindPathSeparator(now);
					if ( next != NULL ){
						*next = '\0';
					}else{
						if ( *now != '\0' ) next = now + tstrlen(now) - 1;
					}
				}
			}
			if ( FixSector(cds) == FALSE ) return FALSE;
		}
		cds->entryindex = 0;
		SetDummydir(ff,T("."));
		return TRUE;
	}else{							// Next
		if ( cds->nowentry == NULL ) return FALSE;

		if ( cds->entryindex == 0 ){
			cds->entryindex = 1;
			SetDummydir(ff,T(".."));
			return TRUE;
		}
	}
	for ( ; ; ){
		UDFFID *FID;

		FID = (UDFFID *)cds->nowentry;
		if ( FID->tag.id != UDF_FID_ID ){
			cds->nowentry = NULL;
			return FALSE;
		}
		// 次の場所
		cds->nowentry = (BYTE *)FID + FID->tag.len + sizeof(UDFTAG);

		if ( IsTrue(GetUDFFileName(ff->cFileName,FID)) ){
			union {
				char buffer[DEFAULTCDSECTOR];
				UDF_FE fe;
				UDF_EFE efe;
			} febuf;
			DWORD sizeL,sizeH;
			DWORD atr;

			ff->cAlternateFileName[0] = '\0';

			SetCDSector(cds,FID->ICB.location.LBN);
			ReadFile(cds->hFile,febuf.buffer,sizeof(febuf),&sizeL,NULL);

			atr = 0;
			sizeL = sizeH = 0;

			if ( (febuf.fe.ICB.Flags & 3) == 3 ){ //UDF_FE 内にファイルの実体がある
				cds->entryindex = FID->ICB.location.LBN;
				cds->dataoffset = 1;
				if ( FID->FC & UDFATR_DIRECTORY ){
					setflag(atr,FILE_ATTRIBUTE_DIRECTORY);
				}else{
					if ( febuf.fe.tag.id == UDF_FE_ID ){
						sizeL = febuf.fe.L_AD;
					}else if ( febuf.fe.tag.id == UDF_EFE_ID ){
						sizeL = febuf.efe.L_AD;
					}
				}
			}else{ // L_EA に保存場所が書いてある
				if ( febuf.fe.tag.id == UDF_FE_ID ){
					cds->entryindex = ((UDFSHORTAD *)&(febuf.fe.EA[febuf.fe.L_EA]))->position - cds->OffsetDelta;
				}else if ( febuf.fe.tag.id == UDF_EFE_ID ){
					cds->entryindex = ((UDFSHORTAD *)&(febuf.efe.EA[febuf.efe.L_EA]))->position - cds->OffsetDelta;
				}

				cds->dataoffset = 0;
				if ( FID->FC & UDFATR_DIRECTORY ){
					setflag(atr,FILE_ATTRIBUTE_DIRECTORY);
				}else{
					DWORD len;
					UDFSHORTAD *usadr;

					if ( febuf.fe.tag.id == UDF_FE_ID ){
						len = febuf.fe.L_AD;
						usadr = (UDFSHORTAD *)&(febuf.fe.EA[febuf.fe.L_EA]);
					}else if ( febuf.fe.tag.id == UDF_EFE_ID ){
						len = febuf.efe.L_AD;
						usadr = (UDFSHORTAD *)&(febuf.efe.EA[febuf.efe.L_EA]);
					} else{
						return FALSE;
					}
					while ( len >= sizeof(UDFSHORTAD) ){
						AddDD(sizeL, sizeH, usadr->length, 0);
						usadr++;
						len -= sizeof(UDFSHORTAD);
					}
				}
			}

			ff->nFileSizeLow = sizeL;
			ff->nFileSizeHigh = sizeH;

			if ( FID->FC & UDFATR_HIDDEN ) setflag(atr,FILE_ATTRIBUTE_HIDDEN);
			ff->dwFileAttributes = atr;

			ff->ftCreationTime.dwLowDateTime = 0;
			ff->ftCreationTime.dwHighDateTime = 0;

			if ( febuf.fe.tag.id == UDF_FE_ID ){
				FixUDFTime(&ff->ftLastWriteTime,&febuf.fe.MDT);
				FixUDFTime(&ff->ftLastAccessTime,&febuf.fe.ADT);
			}else if ( febuf.fe.tag.id == UDF_EFE_ID ){
				FixUDFTime(&ff->ftLastWriteTime,&febuf.efe.MDT);
				FixUDFTime(&ff->ftLastAccessTime,&febuf.efe.ADT);
			}
			if ( FixSector(cds) == FALSE ) return FALSE;
			break;
		}
	}
	cds->ReadFileNext = FALSE;
	return TRUE;
}

DWORD ReadCDCluster(CDS *cds,BYTE *dest,DWORD destsize)
{
	DWORD size;
	UDF_FE *fe;

	if ( cds->ReadFileNext == FALSE ){
		if ( FALSE == SetCDSector(cds,cds->entryindex) ) return 0;
		cds->ReadFileNext = !(cds->ReadBpS & 0xff);
	}
	if ( cds->dataoffset == 0 ){
		if ( cds->ReadBpS & 0xff ){
			if ( FALSE == ReadFile(cds->hFile,dest,cds->c.BpC,&size,NULL) ) return 0;
		}else{
			if ( FALSE == ReadFile(cds->hFile,dest,destsize,&size,NULL) ) return 0;
		}
		return size;
	}
	if ( FALSE == ReadFile(cds->hFile,dest,cds->c.BpC,&size,NULL) ) return 0;
						// FE内埋め込み(UDF)
	fe = (UDF_FE *)dest;
	if ( fe->tag.id == UDF_FE_ID ){
		size = fe->L_AD;
		memmove(dest,&fe->EA[fe->L_EA],size);
	}else if ( fe->tag.id == UDF_EFE_ID ){
		UDF_EFE *efe = (UDF_EFE *)dest;

		size = efe->L_AD;
		memmove(dest,&efe->EA[efe->L_EA],size);
	}
	return size;
}

BOOL SetCDNextCluster(CDS *cds)
{
	cds->entryindex++;
	return TRUE;
}

BOOL ReadUDFSector(CDS *cds,DWORD sector)
{
	DWORD seekL,seekH,rsize;

	DDmul(sector,cds->ReadBpS,&seekL,&seekH);
	AddDD(seekL,seekH,cds->Offset,0);
	if ((MAX32 == SetFilePointer(cds->hFile,seekL,(LONG *)&seekH,FILE_BEGIN)) &&
		 (GetLastError() != NO_ERROR) ){
		return FALSE;
	}
#if 1
	return ReadFile(cds->hFile,cds->Buffer.raw,DEFAULTCDSECTOR,&rsize,NULL) &&
			(rsize == DEFAULTCDSECTOR);
#else
{
	BOOL result;
	result = ReadFile(cds->hFile,cds->Buffer.raw,DEFAULTCDSECTOR,&rsize,NULL);
	XMessage(NULL,NULL,XM_DUMPLOG,(char *)cds->Buffer.raw,0x100);
	return result;
}
#endif
}

void CheckAVDP(CDS *cds)
{
	DWORD rsize;
	int tryc = 10;
	DWORD FD = MAX32,PAL = MAX32,RootDirFE;
	DDWORD sector;

	// AVDP 取得
	if ( FALSE == ReadFile(cds->hFile,cds->Buffer.raw,DEFAULTCDSECTOR,&rsize,NULL) ){
		return;
	}
	if ( memcmp(cds->Buffer.raw,"\2",2) != 0 ) return;
	DDmul( cds->Buffer.avdp->VDS.loc,cds->ReadBpS,&sector.l,&sector.h);
	if ( cds->Offset > 0 ) AddDD(sector.l,sector.h,cds->Offset,0);
	while( tryc >= 0 ){
		if ( (MAX32 != SetFilePointer(cds->hFile,sector.l,
				(PLONG)&sector.h,FILE_BEGIN)) &&
			 (FALSE != ReadFile(cds->hFile,cds->Buffer.raw,
				DEFAULTCDSECTOR,&rsize,NULL)) ){
			if ( cds->Buffer.udftag->id == UDF_PD_ID ){
				PAL = cds->Buffer.pd->PAL;
			}else if ( cds->Buffer.udftag->id == UDF_LVD_ID ){
				FD = cds->Buffer.lvd->LVCU.location.LBN;
				if ( PAL != MAX32 ) break;
			}else if ( cds->Buffer.udftag->id == UDF_TD_ID ){ // 終端
				break;
			}
		}
		tryc--;
		AddDD(sector.l,sector.h,cds->ReadBpS,0);
	}
	if ( (PAL != MAX32) && (FD != MAX32) ){ //
		if ( ReadUDFSector(cds,PAL + FD) ){ // FSD 取得
			cds->OffsetDelta = 0;
			if ( cds->Buffer.udftag->id != UDF_FSD_ID ){
				if ( !ReadUDFSector(cds,0x120 + FD) ) return; // FSD 取得
				if ( cds->Buffer.udftag->id != UDF_FSD_ID ) return;
				cds->OffsetDelta = 0x120 - PAL;
				PAL = 0x120;
			}

			RootDirFE = cds->Buffer.fsd->ROOTICB.location.LBN + PAL;

			if ( ReadUDFSector(cds,RootDirFE) ){ // Root Directory FE 取得
				if ( cds->Buffer.udftag->id == UDF_FE_ID ){
					cds->PathTable = GetUDFSectorFromFE(cds->Buffer.fe);
				}
				if ( cds->Buffer.udftag->id == UDF_EFE_ID ){
					cds->PathTable = GetUDFSectorFromEFE(cds->Buffer.efe);
				}

				cds->mode = CD_UDF;
				cds->Offset = PAL * cds->ReadBpS + (cds->Offset & 0xff);
				// 内容が合っているか確認する
				if ( (SetCDSector(cds,cds->PathTable) == FALSE) ||
					 (ReadFile(cds->hFile,cds->Buffer.raw,sizeof(WORD),&rsize,NULL) == FALSE) ||
					 (rsize < sizeof(WORD)) ||
					 (cds->Buffer.fsd->tag.id != UDF_FID_ID) ){

					ReadUDFSector(cds,RootDirFE + 1);
					cds->PathTable = GetUDFSectorFromFE(cds->Buffer.fe);
				}
			}
		}
	}
}
