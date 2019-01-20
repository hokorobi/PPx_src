/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System	FAT File System Image 処理
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

#define DEFAULTSECTOR	512	// 一般のセクタサイズ

#define DISKFAT12	0
#define DISKFAT16	1
#define DISKFAT32	2
#define DISKNTFS	3
#define DISKEXFAT	4

#pragma pack(push,1)
					// BPB情報(FAT)
typedef struct {
	BYTE	JumpID;		// (JMP命令 EB/E9)
	WORD	JumpAddr;		// (JMPのジャンプ先)
	char	Label[8];		// ディスクラベル(FAT
	WORD	SectorSize;		// １セクタのバイト数
	BYTE	ClusterSize;	// １クラスタのセクタ数
	WORD	ReservedSector;	// 予約セクタ数
							// +10h
	BYTE	FAT;			// FAT の数
	WORD	RootEntries;	// ルートエントリ数(FAT12,16)
	WORD	Sectors;		// セクタ数
	BYTE	MediaID;		// メディアディスクリプタ
	WORD	FATsize;		// 1FATのセクタ数
	WORD	sec;			// トラック当たりのセクタ数
	WORD	head;			// ヘッド数
	DWORD	ReservedSector32;	// 予約セクタ数(FAT32)
							// +20h
	DWORD	Sectors32;		// セクタ数(FAT32)
	DWORD	FATsize32;		// 1FATのセクタ数(FAT32)
	WORD	Flags;
	WORD	FileVersion;
	DWORD	RootDirectory;	// ルートディレクトリの開始クラスタ(FAT32)
							// +30h
	WORD	FSInfo;			// ファイルシステム情報のセクタ
	WORD	Backup;			// ブートセクタのバックアップセクタ
} BPBSTRUCT;
					// BPB情報(NTFS)
typedef struct {
	BYTE	JumpID;		// (JMP命令 EB/E9)
	WORD	JumpAddr;		// (JMPのジャンプ先)
	char	Label[8];		// ディスクラベル
	WORD	SectorSize;		// １セクタのバイト数
	BYTE	ClusterSize;	// １クラスタのセクタ数
	WORD	reserved1;		// (予約セクタ数)
							// +10h
	BYTE	reserved2;		// (FAT の数)
	WORD	reserved3;		// (ルートエントリ数(FAT12,16))
	WORD	reserved4;		// (セクタ数)
	BYTE	MediaID;		// メディアディスクリプタ
	WORD	reserved5;		// (1FATのセクタ数)
	WORD	sec;			// トラック当たりのセクタ数
	WORD	head;			// ヘッド数
	DWORD	ReservedSector;	// 予約セクタ数
							// +20h
	DWORD	reserved6;		// (セクタ数(FAT32))
	DWORD	reserved7;		// 1FATのセクタ数(FAT32) ※0x800080 固定
	DWORD	SectorsL,SectorsH; // 全セクタ数
							// +30h
	DDWORD	MFT;			// $MFT開始クラスタ
	DWORD	MFTML,MFTMH;	// $MFTmirr 開始クラスタ
							// +40h
	DWORD	CFRS;			// FRSあたりのクラスタ数
	DWORD	CINDEX;			// 1indexあたりのクラスタ数
	DWORD	sn;				// シリアル番号
	DWORD	sum;			// チェックサム
} NTFSBPBSTRUCT;
					// BPB情報(EXFAT)
typedef struct {
	BYTE	JumpID;		// (JMP命令 EB/E9)
	WORD	JumpAddr;		// (JMPのジャンプ先)
	char	Label[8];		// ディスクラベル "EXFAT   "
	BYTE	reserved[0x35];	// FATBPB(0 fill)
							// +40h
	DDWORD	U2;				// 不明
	DDWORD	Sectors;		// 全セクタ数
	DWORD	FATsector;		// FATセクタ
	DWORD	FATsectors;		// FATセクタ数
	DWORD	Cluster;		// クラスタ開始位置
	DWORD	Clusters;		// クラスタ数
							// +60h
	DWORD	RootDirectory;	// ルートディレクトリの開始クラスタ
	DWORD	sn;				// シリアル番号
	DWORD	U3;				// 不明
	BYTE	SectorBits;		// セクタのビット数
	BYTE	ClusterBits;	// セクタのビット数
	BYTE	U4,U5;
} EXFATBPBSTRUCT;

#define MFT_RECORD_USE	B0
#define MFT_RECORD_DIR	B1
#define MFT_RECORD_VIEWINDEX	B3
#define MFT_RECORD_SPACE	0xffff

// $mft の内部構造
typedef struct {
//00
	DWORD ID;			// "FILE"
	WORD offsetFixup;	// Offset to fixup 0x30 byte
	WORD sizeFixup;		// Size of fixup   3 byte
	DWORD seqL,seqH;	// log seq no
//10
	WORD seqNo;			// seq no (1)
	WORD hardlinks;		// hardlink count(1)
	WORD attroffset;	// MFSATTR へのオフセット 0x38
	WORD flags;			// flags: 1 MFT_RECORD_
	DWORD realsize;		// realsize
	DWORD allocsize;	// realsize
//20
	DWORD baseMFTL,baseMFTH;
	WORD nextAttrID,XPalign;
	DWORD mftNo;
//30
} MFSRECORD;

#define MFT_ATR_STANDARD_INFORMATION	0x10
#define MFT_ATR_ATTRIBUTE_LIST			0x20
#define MFT_ATR_FILE_NAME				0x30
#define MFT_ATR_OBJECT_ID				0x40
#define MFT_ATR_SECURITY_DESCRIPTOR		0x50
#define MFT_ATR_VOLUME_NAME				0x60
#define MFT_ATR_VOLUME_INFORMATION		0x70
#define MFT_ATR_DATA					0x80
#define MFT_INDEX_ROOT					0x90
#define MFT_INDEX_ALLOCATION			0xa0
#define MFT_BITMAPS						0xb0
#define MFT_REPARSE_POINT				0xc0
#define MFT_EA_INFORMATION				0xd0
#define MFT_EA							0xe0
#define MFT_PROPERTY_SET				0xf0
#define MFT_LOGGED_UTILITY_STREAM		0x100
#define MFT_FIRST_USER_DEFINED_ATTRIBUTE 0x1000

typedef struct {
	DWORD ID;			// Attribute の種類
	WORD size;			// Attribute の大きさ
	WORD status;
	WORD nameLength;	// Attribute Name の長さ
	WORD nameoffset;	// Attribute Name の位置
	DWORD id;
//10
	DWORD regLength;
	WORD regOffset;
	WORD indexflag;
} MFSATTR;

typedef struct { // MFT_ATR_STANDARD_INFORMATION
	MFSATTR mfsattr;
	FILETIME create,mod,mftmod,access; // 各種日付
	DWORD attr,maxnumberversion,version,classid,ownerid;
	DWORD QuotaL,QuotaH,USNL,USNH;
} MFSSINFO;

#define FILE_ATTRIBUTE_MFT_DIRECTORY 0x10000000
#define FILE_ATTRIBUTE_MFT_INDEXVIEW 0x20000000
#define MFSNAMETYPE_POSIX	0
#define MFSNAMETYPE_WIN		1
#define MFSNAMETYPE_DOS		2
#define MFSNAMETYPE_WINDOS	3

typedef struct { // MFT_ATR_FILE_NAME
	MFSATTR mfsattr;
	DWORD parentL,parentH;	// 親のMFS ID
	FILETIME create,mod,mftmod,access; // 各種日付
	DWORD allocL,allocH;
	DWORD realL,realH;	// ファイルサイズ
	DWORD attr,res1;
	BYTE len;			// ファイル名長
	BYTE nametype;		// MFSNAMETYPE_
	WCHAR filename[];
} MFSNAME;

// exFAT エントリ
#define EXTYPE_VOLUME 0x83
typedef struct { // Volume name
	BYTE type; // 83h
	BYTE len;
	WCHAR name[15];
} EXFATENTRY_VOLUME;

#define EXTYPE_ATTRIBUTE1 0x85
typedef struct { // Attributes 1
	BYTE type; // 85h
	BYTE len;
	WORD sum;
	DWORD attributes;
	WORD CreateTime[2],LastWriteTime[2],LastAccessTime[2];
	BYTE U[11];
} EXFATENTRY_ATTRIBUTE1;

#define EXTYPE_ATTRIBUTE2 0xC0
typedef struct { // Attributes 2
	BYTE type; // c0h
	BYTE map; // FAT がいるときは 01 ?
	BYTE U1;
	BYTE len;
	DWORD sum;
	DDWORD FileSize;
	DWORD U2,Cluster;
	DDWORD extendFileSize;
} EXFATENTRY_ATTRIBUTE2;

#define EXTYPE_NAME 0xC1
typedef struct { // Name
	BYTE type; // c1h
	BYTE U;
	WCHAR name[15];
} EXFATENTRY_NAME;

typedef union {
	BYTE type;
	// 03 ... ?
	// 81 ... Use Bitmap table
	// 82 ... Char table
	EXFATENTRY_VOLUME v;      // 83
	EXFATENTRY_ATTRIBUTE1 a1; // 85
	EXFATENTRY_ATTRIBUTE2 a2; // C0
	EXFATENTRY_NAME n;        // C1
} EXFATENTRY;
#pragma pack(pop)

typedef struct { // MFT_ATR_DATA
	DWORD ID;			// Attribute の種類
	DWORD size;			// Attribute の大きさ
	BYTE status;		// 1ならクラスタ情報 0なら直接格納
	BYTE nameLength;	// Attribute Name の長さ
	WORD nameoffset;	// Attribute Name の位置
	WORD atr,id;

	union {
		struct { // 直接格納
		//10
			DWORD size;
			WORD offset;
			BYTE flag,unk;
		//18
			BYTE data[];
		} Rdata;
		struct { // クラスタ情報
		//10
			DWORD StartVCNL,StartVCNH;
			DWORD LastVCNL,LastVCNH;
		//20
			WORD offset; //
			BYTE compression,res1,res2[4];
			DWORD allocsizeL,allocsizeH;
		//30
			DWORD realL,realH;
			DWORD compresssizeL,compresssizeH;
		//40
			BYTE datas[]; // +0(1)	LowHBYTE:サイズのバイト数
				//			HighHBYTE:クラスタのバイト数
				// +1(LowHBYTE) サイズ
				// +(1+LowHBYTE)(HighHBYTE)クラスタ
		} Ndata;
	} Udata;
} MFTDATA;

//-------------------------------------
BOOL FindEntryNTFSImage(FATSTRUCT *fats,TCHAR *now,TCHAR *next,WIN32_FIND_DATA *ff);
BOOL FindEntryEXFATImage(FATSTRUCT *fats,TCHAR *now,TCHAR *next,WIN32_FIND_DATA *ff);

#define lfnblock	(5+6+2)
#define lfnmaxsize	(lfnblock * 13 + 1)

int EntryToFindData(WIN32_FIND_DATA *ff,FATENTRY *entry,WCHAR *lfnname)
{
	char *src,*dst;
	int i;
#ifdef UNICODE
	char filenameA[16];
#endif
	src = entry->name;
	if ( *src == '\0' ) return 0;			// 最後のエントリ
	if ( entry->atr == 0xf ){		// LFN entry
		int block;
		WCHAR *wdest;

		block = *src & 0x1f;	// ブロックの位置を求める
		if ( block > lfnblock ) return 0xf; // 限界を超えている
		wdest = lfnname + (block - 1) * lfnblock;
		if ( *src & (B6 | B7) ){ // 末尾指定有り
			*(wdest + lfnblock) = '\0';
		}
		memcpy(wdest     ,((LFNENTRY *)entry)->name1,sizeof(WCHAR) * 5);
		memcpy(wdest +  5,((LFNENTRY *)entry)->name2,sizeof(WCHAR) * 6);
		memcpy(wdest + 11,((LFNENTRY *)entry)->name3,sizeof(WCHAR) * 2);
		return 0xf;		// LFN のため、無視させる
	}
//------------------------------------- 一般のエントリ
	ff->cAlternateFileName[0] = '\0';

#ifdef UNICODE
	dst = filenameA;
#else
	dst = ff->cFileName;
#endif
	i = entry->atr & FILE_ATTRIBUTE_LABEL ? 13 : 8;
										// ファイル名を生成 -------------------
	if ( (BYTE)*src == 0xe5 ){				// 削除ファイル
//		return 0xe5;
		*dst++ = '=';
		src++;
		i--;
	}else if ( *src == 0x05 ){				// E5 に変換
		*dst++ = (char)'\xe5';
		src++;
		i--;
	}
	for (  ; i > 0 ; i-- ){
		if ((BYTE)*src <= ' ') break;
		*dst++ = *src++;
	}
										// 拡張子を生成 -----------------------
	if ( !(entry->atr & FILE_ATTRIBUTE_LABEL) ){
		src = entry->ext;
		if ( *src > ' ' ){
			*dst++ = '.';
			for ( i = 0 ; i < 3 ; i++ ){
				if (*src <= ' ') break;
				*dst++ = *src++;
			}
		}
	}
	*dst = '\0';
#ifdef UNICODE
	AnsiToUnicode(filenameA,ff->cFileName,MAX_PATH);
#endif
	if ( *lfnname ){
		*(lfnname + lfnmaxsize - 1) = 0;
		tstrcpy( ff->cAlternateFileName,ff->cFileName );
		strcpyWToT(ff->cFileName,lfnname,MAX_PATH);
		*lfnname = 0;
	}
	return -1;
}

//------------------------------------- ディスクイメージの参照を終了する
void CloseFATImage(FATSTRUCT *fats)
{
	if ( fats->FAT != NULL ){
		HeapFree(DLLheap,0,fats->FAT);
		fats->FAT = NULL;
	}
	if ( fats->Buffer != NULL ){
		HeapFree(DLLheap,0,fats->Buffer);
		fats->Buffer = NULL;
	}
	if ( fats->hFile != NULL ){
		CloseHandle(fats->hFile);
		fats->hFile = NULL;
	}
}
//------------------------------------- ディスクイメージの参照を開始する
BOOL OpenFATImage(FATSTRUCT *fats,const TCHAR *fname,int offset)
{
	DWORD size,rsize,bufsize;
	BPBSTRUCT *bpb;

	fats->hFile	= NULL;
	fats->FAT	= NULL;
	fats->Buffer	= NULL;
	fats->Offset	= offset;

	fats->hFile = CreateFileL(fname,GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,NULL);
	if ( fats->hFile == INVALID_HANDLE_VALUE ) return FALSE;
										// ブートセクタを読み込む -------------
	if ( MAX32 == SetFilePointer(fats->hFile,fats->Offset,NULL,FILE_BEGIN) ){
		goto error;
	}
	fats->Buffer = HeapAlloc(DLLheap,0,DEFAULTSECTOR);
	if ( fats->Buffer == NULL ) goto error;
	if ( FALSE == ReadFile(fats->hFile,fats->Buffer,DEFAULTSECTOR,&rsize,NULL) ){
		goto error;
	}
	if ( rsize != DEFAULTSECTOR ) goto error;
	bpb = (BPBSTRUCT *)fats->Buffer;
										// BPB の正当性チェック
	if ( bpb->SectorSize == 0 ){ // exFAT ------------------------------------
		EXFATBPBSTRUCT *exbpb;

		if ( memcmp(bpb->Label,"EXFAT   ",8) ) goto error;
		fats->fattype = DISKEXFAT;
		exbpb = (EXFATBPBSTRUCT *)bpb;
		fats->BpS   = 1 << exbpb->SectorBits;
		bufsize = fats->c.BpC = fats->BpS << exbpb->ClusterBits;
		fats->FATOffset = exbpb->FATsector * fats->BpS;
		fats->DATAOffset = exbpb->Cluster * fats->BpS - fats->c.BpC * 2;
		fats->ROOT = exbpb->RootDirectory;
		fats->NowFAT = MAX32;
		size = fats->c.BpC;
	}else{
		if ( bpb->SectorSize & 0xff ) goto error;
		if ( ((bpb->FATsize32 != 0x800080) && !bpb->FAT) || (bpb->FAT > 2) ) goto error;
										// BPB から情報取得
		fats->roots = bpb->RootEntries * sizeof(FATENTRY);
		fats->BpS   = bpb->SectorSize;
		bufsize = fats->c.BpC   = bpb->ClusterSize * fats->BpS;
										// FATの種類を調べる ------------------
		if ( fats->roots == 0 ){	// FAT32/NTFS
			size = fats->c.BpC;
			if ( bpb->FATsize32 == 0x800080 ){ // NTFS
				fats->fattype = DISKNTFS;
				fats->DATAOffset = 0;
				fats->d.n.mft.cluster= ((NTFSBPBSTRUCT *)bpb)->MFT;
				fats->d.n.mft.left	= 0x4000 / fats->c.BpC; //最小MFTのクラスタ数
				fats->d.n.mft.datas	= NULL;
				if ( bufsize < 0x400 ) bufsize = 0x400;
			}else{				// FAT32
				fats->fattype = DISKFAT32;

				fats->FATOffset  = offset + bpb->ReservedSector * fats->BpS;
				fats->ROOT		= bpb->RootDirectory;
				fats->DATAOffset = fats->FATOffset +
						bpb->FATsize32 * bpb->FAT * fats->BpS - fats->c.BpC * 2;
				fats->NowFAT = 0;
			}
		}else{					// FAT12/16
			size = bpb->FATsize * fats->BpS;	// FATの大きさ(byte)
			if ( size < (0x1000 * 3 / 2) ){
				fats->fattype = DISKFAT12;	// FAT12
			}else{
				fats->fattype = DISKFAT16;
			}
			fats->FATOffset  = offset + bpb->ReservedSector * fats->BpS;
			fats->ROOT		= fats->FATOffset + bpb->FATsize * bpb->FAT * fats->BpS;
			fats->DATAOffset = fats->ROOT + fats->roots - fats->c.BpC * 2;
		}
	}
										// バッファを確保し直す ---------
	if ( bufsize > DEFAULTSECTOR ){
		char *ptr;

		ptr = HeapReAlloc(DLLheap,0,fats->Buffer,bufsize);
		if ( ptr == NULL ) goto error;
		fats->Buffer = (BYTE *)ptr;
	}
	if ( fats->fattype == DISKNTFS ) return TRUE;
										// FAT読み込み ------------------------
	if ( MAX32 == SetFilePointer(fats->hFile,fats->FATOffset,NULL,FILE_BEGIN) ){
		goto error;
	}
	fats->FAT = HeapAlloc(DLLheap,0,size);
	if ( fats->FAT == NULL ) goto error;
	if ( FALSE == ReadFile(fats->hFile,fats->FAT,size,&rsize,NULL) ){
		goto error;
	}
	if ( rsize != size ) goto error;
	return TRUE;
error:
	CloseFATImage(fats);
	return FALSE;
}

BOOL FatChain(FATSTRUCT *fats,DWORD *fat)
{
	DWORD next;

	next = *fat;

	switch (fats->fattype){
		case DISKFAT12:
			if ( next >= 0xff8 ) return FALSE;
			next = *(WORD *)(fats->FAT + next + (next >> 1));
			if ( *fat & 1 ) next >>= 4;
			*fat = next & 0xfff;
			return TRUE;

		case DISKFAT16:
			if ( next >= 0xfff8 ) return FALSE;
			*fat = *(WORD *)(fats->FAT + (next * 2));
			return TRUE;

		case DISKFAT32: {
			DWORD fatoffset,page,tmp;

			if ( next >= 0xffffff8 ) return FALSE;
			fatoffset = *fat * sizeof(DWORD);
			page = fatoffset / fats->c.BpC;
			fatoffset = fatoffset % fats->c.BpC;
			if ( page != fats->NowFAT ){
				if ( MAX32 == SetFilePointer(fats->hFile,
						fats->FATOffset + fats->BpS * page,NULL,FILE_BEGIN) ){
					return FALSE;
				}
				if ( FALSE == ReadFile(fats->hFile,fats->FAT,fats->c.BpC,&tmp,NULL) ){
					return FALSE;
				}
				fats->NowFAT = page;
			}
			*fat = *(DWORD *)(fats->FAT + fatoffset);
			return TRUE;
		}
		case DISKNTFS:
			return TRUE;

		case DISKEXFAT: {
//			DWORD fatoffset,page,tmp;

			if ( fats->d.e_useFAT > 1 ){ // FAT 不要
				*fat = next+1;
				return TRUE;
			}
//			MessageA("now making...");
			*fat = next+1;
			return TRUE;
/*
			fatoffset = *fat * sizeof(DWORD);
			page = fatoffset / fats->c.BpC;
			fatoffset = fatoffset % fats->c.BpC;
			if ( page != fats->NowFAT ){
				if ( MAX32 == SetFilePointer(fats->hFile,
						fats->FATOffset + fats->BpS * page,NULL,FILE_BEGIN) ){
					return FALSE;
				}
				if ( FALSE == ReadFile(fats->hFile,fats->FAT,fats->c.BpC,&tmp,NULL) ){
					return FALSE;
				}
				XMessage(NULL,NULL,XM_DUMP,fats->FAT,256);
				fats->NowFAT = page;
			}
			*fat = *(DWORD *)(fats->FAT + fatoffset);
			return TRUE;
*/
		}
	}
	return FALSE;
}

BOOL SetFATNextCluster(FATSTRUCT *fats)
{
	DWORD index = fats->entryindex;

	if ( FatChain(fats,&index) == FALSE ) return FALSE;
	if ( index == 0 ){	// 未使用クラスタ→簡易undelete処理を行う
		for ( ; ; ){
			fats->entryindex++;
			index = fats->entryindex;
			// はみ出したので終了
			if ( FatChain(fats,&index) == FALSE ) return FALSE;
			// 読もうとしているクラスタも未使用なので正解
			if ( index == 0 ) break;
			// ※存在するクラスタ数よりも大きくなった場合、次のFATの+0,+1も
			//   読み出し、fffHがあるためループが終了するはず。
		}
		return TRUE;
	}
	fats->entryindex = index;
	return TRUE;
}

BOOL SetFATCluster(FATSTRUCT *fats,DWORD clusterL,DWORD clusterH)
{
	DWORD seekL,seekH;

	DDmul(clusterL,fats->c.BpC,&seekL,&seekH);
	AddDD(seekL,seekH,fats->DATAOffset,clusterH * fats->c.BpC);
	if ( fats->DATAOffset > 0xfff00000 ) seekH--; // 64Kcluster のとき、fats->DATAOffset が負数になるので、AddDD の結果を補正する
	if ( (MAX32 == SetFilePointer(fats->hFile,seekL,(LONG *)&seekH,FILE_BEGIN))
		&& (GetLastError() != NO_ERROR) ){
		return FALSE;
	}
	return TRUE;
}

DWORD ReadDirdata(FATSTRUCT *fats)
{
	DWORD rsize;

	if ( fats->nowroot == 0 ) return 0;	// これ以上ない…undeleteのときは対策
	if ( fats->nowroot > 0 ){	// FAT12/16のルートディレクトリ
		if ( MAX32 == SetFilePointer(fats->hFile,
				fats->ROOT + fats->nowpointer * fats->BpS,NULL,FILE_BEGIN) ){
			return 0;
		}
		if ( FALSE == ReadFile(fats->hFile,fats->Buffer,fats->BpS,&rsize,NULL)){
			return 0;
		}
		if ( rsize != fats->BpS ) return 0;
		fats->nowroot--;
		fats->nowpointer++;
	}else{						// FAT 追跡
		if ( fats->nowroot == -1 ){
			fats->nowroot = -2;
		}else{
			if ( FatChain(fats,&fats->nowpointer) == FALSE ) return 0;
		}

		if ( FALSE == SetFATCluster(fats,fats->nowpointer,0) ) return 0;
		if ( FALSE == ReadFile(fats->hFile,fats->Buffer,fats->c.BpC,&rsize,NULL)){
			return 0;
		}
		if ( rsize != fats->c.BpC ) return 0;
	}
	return rsize;
}

BOOL FindEntryFATImage(FATSTRUCT *fats,TCHAR *fname,WIN32_FIND_DATA *ff)
{
	WCHAR lfnname[lfnmaxsize];
	FATENTRY *entry;
	TCHAR *now C4701CHECK,*next;

	if ( fname != NULL ){	// Findfirst mode
		if ( fats->fattype == DISKNTFS ){
			fats->d.n.readmft = fats->d.n.mft;
			fats->d.n.entry.left = MAX32;
			fats->nowroot = -1;
			fats->ROOT = 0; // 0:$MFT 1:$fragmented
		}else if ( (fats->fattype == DISKFAT32) || (fats->fattype == DISKEXFAT)){
			fats->nowroot = -1;
			fats->nowpointer = fats->ROOT;
		}else{ // DISKFAT12/DISKFAT16
			fats->nowroot = fats->roots / fats->BpS;
			fats->nowpointer = 0;
		}

		fats->nowleft = 0;
		now = fname;
		// 検索パスの切り出し
		next = FindPathSeparator(fname);
		if ( next != NULL ){
			*next = '\0';
		}else{
			if (*fname) next = fname + tstrlen(fname) - 1;
		}
	}else{	// Findnextmode
		if ( (fats->nowleft == 0) && (fats->nowroot == 0) ) return FALSE;
		next = NULL;
	}
	lfnname[0] = '\0';

	if ( fats->fattype == DISKNTFS ) return FindEntryNTFSImage(fats,now,fname,ff);
	if ( fats->fattype == DISKEXFAT ) return FindEntryEXFATImage(fats,now,next,ff);

	for ( entry = fats->nowentry ; ; entry++ ){
		if ( fats->nowleft == 0 ){
			fats->nowleft = ReadDirdata(fats);
			if ( fats->nowleft == 0 ) break;
			fats->nowleft /= sizeof(FATENTRY);
			entry = (FATENTRY *)fats->Buffer;
		}
		if ( entry->name[0] == '\0' ) break;
		fats->nowleft--;
		if ( EntryToFindData(ff,entry,lfnname) > 0 ) continue;
		if ( next != NULL ){
			if ( !tstricmp(now,ff->cFileName) ||
				 !tstricmp(now,ff->cAlternateFileName) ){
				fats->nowroot = -1;
				fats->nowpointer = entry->clusterL;
				if ( fats->fattype == DISKFAT32 ){
					fats->nowpointer |= entry->clusterH << 16;
				}
				fats->nowleft = 0;
				now = next + 1;
				next = FindPathSeparator(now);
				if ( next != NULL ){
					*next = '\0';
				}else{
					if (*now) next = now + tstrlen(now) - 1;
				}
			}
			continue;
		}
		DosDateTimeToFileTime(entry->Cdate,entry->Ctime,&ff->ftCreationTime);
		DosDateTimeToFileTime(entry->Adate,0,&ff->ftLastAccessTime);
		DosDateTimeToFileTime(entry->Wdate,entry->Wtime,&ff->ftLastWriteTime);
		ff->dwFileAttributes	= (DWORD)entry->atr;
		ff->nFileSizeHigh		= 0;
		ff->nFileSizeLow		= entry->size;
		fats->entryindex = entry->clusterL;
		if ( fats->fattype == DISKFAT32 ){
			fats->entryindex |= entry->clusterH << 16;
		}
		fats->nowentry = entry + 1;
		return TRUE;
	}
	fats->nowleft = 0;
	fats->nowroot = 0;
	return FALSE;
}

void SetClusterRInfo(CLUSTERRINFO *crinfo)
{
	int size;
	BYTE datasize;
	DWORD rel;

	datasize = *crinfo->datas++;
	// クラスタの数
	size = datasize & 0xf;
	crinfo->left = *(DWORD *)(crinfo->datas) & (MAX32 >> ((4 - size) * 8));
	crinfo->datas += size;
	// クラスタの場所
	size = datasize >> 4;
	if ( size > 4 ){
		rel = *(DWORD *)(crinfo->datas + 4) & (MAX32 >> ((8 - size) * 8));
		if ( rel & (1 << (((size - 4) * 8) - 1)) ){ // 負の値
			rel = (1 << ((size - 4) * 8)) - rel;
			SubDD(crinfo->clusterBase.l,crinfo->clusterBase.h,0 - *(DWORD *)(crinfo->datas),rel);
		}else{
			AddDD(crinfo->clusterBase.l,crinfo->clusterBase.h,*(DWORD *)(crinfo->datas),rel);
		}
	}else{
		rel = *(DWORD *)(crinfo->datas) & (MAX32 >> ((4 - size) * 8));
		if ( rel & (1 << ((size * 8) - 1)) ){ // 負の値
			rel = (1 << (size * 8)) - rel;
			SubDD(crinfo->clusterBase.l,crinfo->clusterBase.h,rel,0);
		}else{
			AddDD(crinfo->clusterBase.l,crinfo->clusterBase.h,rel,0);
		}
	}
	crinfo->cluster = crinfo->clusterBase;
	crinfo->datas += size;
}

DWORD ReadNTFSClusterLarge(FATSTRUCT *fats,BYTE *dest,CLUSTERRINFO *crinfo)
{
	DWORD size;

	if ( crinfo->left == 0 ){
		if ( crinfo->datas == NULL ) return 0;
		if ( *crinfo->datas == 0 ){
			crinfo->datas = NULL;
			return 0;
		}
		SetClusterRInfo(crinfo);
//		Messagef("crinfo->left %d",crinfo->left);
//		Messagef("cls %x",crinfo->cluster.l);
//		Messagef("next %x",*crinfo->datas);
	}
	crinfo->left--;
	if ( FALSE == SetFATCluster(fats,crinfo->cluster.l,crinfo->cluster.h) ){
		return 0;
	}

	if ( FALSE == ReadFile(fats->hFile,dest,fats->c.BpC,&size,NULL) ){
		return 0;
	}
	if ( size != fats->c.BpC ) return 0;
	crinfo->cluster.l++;
	return size;
}

DWORD ReadNTFSCluster(FATSTRUCT *fats,BYTE *dest,CLUSTERRINFO *crinfo)
{
	MFTDATA *mftd = (MFTDATA *)(fats->d.n.mftd);

	if ( mftd->status == 0 ){ // 直接格納
		DWORD size;

		size = mftd->Udata.Rdata.size;
		mftd->Udata.Rdata.size = 0;
		memcpy(dest,mftd->Udata.Rdata.data,size);
		return size;
	}
	if ( crinfo->left == MAX32 ){ // まだクラスタが決定していない
		crinfo->clusterBase.l = 0;
		crinfo->clusterBase.h = 0;
		crinfo->datas = mftd->Udata.Ndata.datas;
		SetClusterRInfo(crinfo);
	}
	return ReadNTFSClusterLarge(fats,dest,crinfo);
}

BOOL FindEntryNTFSImage(FATSTRUCT *fats,TCHAR *now,TCHAR *fname,WIN32_FIND_DATA *ff)
{
	MFSRECORD *mfsr;
	DWORD offset;

	if ( fname != NULL ){
		if ( tstrcmp(now,T("$fragmented")) == 0 ) fats->ROOT = 1; // 断片一覧
	}

	for ( ; ; ){
		if ( fats->nowleft == 0 ){
			BYTE *dest;

			dest = fats->Buffer;
			while ( fats->nowleft < 0x400 ){
				DWORD size;

				size = ReadNTFSClusterLarge(fats,dest,&fats->d.n.readmft);
				if ( size == 0 ) return FALSE;
				fats->nowleft += size;
				dest += size;
			}
			fats->entryindex = 0;
		}

		mfsr = (MFSRECORD *)(fats->Buffer + fats->entryindex);
		if ( mfsr->ID != 0x454c4946 ){
			fats->entryindex += 0x400;
			fats->nowleft -= 0x400;
			if ( fats->nowleft > 0x10000 ) fats->nowleft = 0;
			continue;
		}
		if ( mfsr->allocsize == 0 ) return FALSE;
		fats->entryindex += mfsr->allocsize;
		fats->nowleft -= mfsr->allocsize;
		if ( (fats->nowleft > 0x10000) || (mfsr->realsize > mfsr->allocsize) ){
			return FALSE;
		}
		offset = mfsr->attroffset;

		ff->cFileName[0] = '\0';
		ff->cAlternateFileName[0] = '\0';
		fats->d.n.mftd = NULL;

		while ( offset < mfsr->realsize ){
			MFSATTR *mfsa;

			mfsa = (MFSATTR *)(BYTE *)((BYTE *)mfsr + offset);
			if ( mfsa->ID == MFT_ATR_DATA ){
				MFTDATA *mftd;

				mftd = (MFTDATA *)mfsa;
				if ( mfsr->mftNo == 0 ){
					DWORD readc;

					// ●1.29 readcによる補正は多分不要だけど、未確認
					readc = fats->d.n.readmft.cluster.l -
							fats->d.n.mft.cluster.l;
					fats->d.n.readmft.datas = mftd->Udata.Ndata.datas;
					fats->d.n.readmft.clusterBase.l = 0;
					fats->d.n.readmft.clusterBase.h = 0;
					SetClusterRInfo(&fats->d.n.readmft);
					fats->d.n.readmft.cluster.l += readc;
					fats->d.n.readmft.left -= readc;
				}
				fats->d.n.mftd = mftd;

				if ( mftd->status == 1 ){ // クラスタ情報
					ff->nFileSizeHigh	= mftd->Udata.Ndata.realH;
					ff->nFileSizeLow	= mftd->Udata.Ndata.realL;
					// 断片の有無チェック
					if ( fats->ROOT == 1 ){
						BYTE *datas = mftd->Udata.Ndata.datas,datasdata;

						datasdata = *datas;
						if ( datasdata == 0 ){
							ff->cFileName[0] = '\0';
							break;
						}
						datas += (datasdata & 0xf) + (datasdata >> 4) + 1;
						if ( *datas == 0 ){
							ff->cFileName[0] = '\0';
							break;
						}
					}
				}else{
					ff->nFileSizeHigh	= 0;
					ff->nFileSizeLow	= mftd->Udata.Rdata.size;
					if ( fats->ROOT == 1 ){
						ff->cFileName[0] = '\0';
						break;
					}
				}
			}else if ( mfsa->ID == MFT_ATR_STANDARD_INFORMATION ){
				MFSSINFO *mfsi;

				mfsi = (MFSSINFO *)mfsa;
				ff->ftCreationTime = mfsi->create;
				ff->ftLastAccessTime = mfsi->access;
				ff->ftLastWriteTime = mfsi->mod;

				//↓FILE_ATTRIBUTE_MFT_DIRECTORY とかを消去する必要がある為
				ff->dwFileAttributes = mfsi->attr & 0x7ffff;
				if ( mfsr->flags & MFT_RECORD_DIR ){
					setflag(ff->dwFileAttributes,FILE_ATTRIBUTE_DIRECTORY);
				}
			}else if ( mfsa->ID == MFT_ATR_FILE_NAME ){
				MFSNAME *mfsn;
				TCHAR *dstname;

				mfsn = (MFSNAME *)mfsa;
				if ( mfsn->nametype != MFSNAMETYPE_DOS ){
					ff->nFileSizeHigh	= mfsn->realH;
					ff->nFileSizeLow	= mfsn->realL;
					dstname = ff->cFileName;

					// 削除ファイルに印を付ける
					if ( !(mfsr->flags & MFT_RECORD_USE) ) *dstname++ = '*';
				}else{
					dstname = ff->cAlternateFileName;
				}

				if ( mfsn->len == 0 ){
					tstrcpy(dstname,T("<no name>"));
				}else{
					WCHAR *bp,bk;

					bp = mfsn->filename + mfsn->len;
					bk = *bp;
					*bp = '\0';
					strcpyWToT(dstname,mfsn->filename,MAX_PATH);
					*bp = bk;
				}
			}else if ( mfsa->ID == MAX32 ){
				break;
			}
			if ( mfsa->size == 0) break;
			offset += mfsa->size;
		}
		if ( ff->cFileName[0] != '\0' ) return TRUE;
	}
}

#pragma argsused
DWORD ReadFATCluster(FATSTRUCT *fats,BYTE *dest,DWORD destsize)
{
	DWORD size;
	UnUsedParam(destsize);

	if ( fats->fattype == DISKNTFS ) return ReadNTFSCluster(fats,dest,&fats->d.n.entry);
	if ( FALSE == SetFATCluster(fats,fats->entryindex,0) ) return 0;
	if ( FALSE == ReadFile(fats->hFile,dest,fats->c.BpC,&size,NULL) ) return 0;

	return size;
}

BOOL FindEntryEXFATImage(FATSTRUCT *fats,TCHAR *now,TCHAR *next,WIN32_FIND_DATA *ff)
{
	EXFATENTRY *entry;
	DWORD nameoffset = 0;
	DWORD useFAT = 0;

	fats->d.e_useFAT = 1; // FAT を使用する
	memset(ff,0,sizeof(WIN32_FIND_DATA));
	for( entry = (EXFATENTRY *)fats->nowentry ; ; entry++ ){
		if ( fats->nowleft == 0 ){
											// FAT 追跡
			if ( fats->nowroot < 0 ){
				fats->nowroot = 0;
			}else{
				if ( FatChain(fats,&fats->nowpointer) == FALSE ) return 0;
			}

			if ( FALSE == SetFATCluster(fats,fats->nowpointer,0) ) return 0;
			if ( FALSE == ReadFile(fats->hFile,fats->Buffer,fats->c.BpC,&fats->nowleft,NULL)){
				return 0;
			}
			if ( fats->nowleft != fats->c.BpC ) return FALSE;
			fats->nowleft /= sizeof(EXFATENTRY);
			entry = (EXFATENTRY *)fats->Buffer;
		}
		if ( entry->type == 0 ) goto ok;
		fats->nowleft--;
		switch (entry->type){
			case EXTYPE_ATTRIBUTE1: {
				FILETIME tmptime;
				if ( nameoffset ){
					goto ok;
				}
				ff->dwFileAttributes	= (DWORD)entry->a1.attributes;
				DosDateTimeToFileTime(entry->a1.CreateTime[1],entry->a1.CreateTime[0],&tmptime);
				LocalFileTimeToFileTime(&tmptime,&ff->ftCreationTime);
				DosDateTimeToFileTime(entry->a1.LastWriteTime[1],entry->a1.LastWriteTime[0],&tmptime);
				LocalFileTimeToFileTime(&tmptime,&ff->ftLastWriteTime);
				DosDateTimeToFileTime(entry->a1.LastAccessTime[1],entry->a1.LastAccessTime[0],&tmptime);
				LocalFileTimeToFileTime(&tmptime,&ff->ftLastAccessTime);
				break;
			}
			case EXTYPE_ATTRIBUTE2:
				if ( !(ff->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
					ff->nFileSizeLow	= entry->a2.FileSize.l;
					ff->nFileSizeHigh	= entry->a2.FileSize.h;
				}
				useFAT = entry->a2.map;
				fats->entryindex = entry->a2.Cluster;
				break;

			case EXTYPE_NAME: {
				WCHAR *src;
				#ifdef UNICODE
					WCHAR *dst;
				#endif
				int len = 15;

				src = entry->n.name;
				#ifdef UNICODE
					dst = ff->cFileName + nameoffset;
					while ( len ){
						if ( *src == '\0' ) break;
						*dst++ = *src++;
						nameoffset++;
						len--;
					}
					*dst = '\0';
				#else
					while ( len ){
						if ( *src == '\0' ) break;
						src++;
						len--;
					}
					nameoffset += WideCharToMultiByte(CP_ACP,0,entry->n.name,15 - len,ff->cFileName + nameoffset,32,NULL,NULL);
					ff->cFileName[nameoffset] = '\0';
				#endif
				break;
			}
		}
		continue;
ok:
		if ( nameoffset == 0 ) break;
		if ( next != NULL ){
			if ( tstricmp(now,ff->cFileName) == 0 ){
				fats->nowroot = -1;
				fats->nowpointer = fats->entryindex;
				fats->nowleft = 0;
				now = next + 1;
				next = FindPathSeparator(now);
				if ( next != NULL ){
					*next = '\0';
				}else{
					if (*now) next = now + tstrlen(now) - 1;
				}
			}
			fats->d.e_useFAT = useFAT;
			nameoffset = 0;
			useFAT = 3;
			memset(ff,0,sizeof(WIN32_FIND_DATA));
			continue;
		}
		fats->nowentry = (FATENTRY *)entry;
		return TRUE;
	}
	fats->nowleft = 0;
	fats->nowroot = 0;
	return FALSE;
}

DWORD GetPC9801FirstDriveSector(const BYTE *header)
{
	int sectorsize,headsize,cylindersize;
	const BYTE *drivetable;

	if ( memcmp(header + 0x1002,"\x90\x90IPL1",6) ) return 0;

	sectorsize = *(DWORD *)(header + 0x0010);
	drivetable = header + sectorsize + 0x1000;
	headsize = sectorsize * *(DWORD *)(header + 0x0014); //Sector count
	cylindersize = headsize * *(DWORD *)(header + 0x0018); // head count
//	headsize =  tracksize * *(DWORD *)(header + 0x001c); // track count

	return	*(drivetable + 0x08) * sectorsize +
			*(drivetable + 0x09) * headsize +
			*(const WORD *)(drivetable + 0x0a) * cylindersize +
			0x1000;
}
