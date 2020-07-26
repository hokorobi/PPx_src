/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System				ファイル種別判別

※VFSGetFileType内ユーザ関数のエラーコード
	NO_ERROR			VFSFILETYPE が未設定なのでSetFileTypeを行う必要ある
						※result->info設定のみも↑
	ERROR_MORE_DATA		VFSFILETYPEを設定済みなのでそのままNO_ERROR終了可能
	ERROR_NO_DATA_DETECTED	誤判定なので判別を続行する
	ERROR_FILEMARK_DETECTED	(vfd_COM専用、VFSFT_STRICT使用時に)ある程度絞れたが、判定が
						完全に確定できていない

-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#define ONVFSDLL		// VFS.H の DLL export 指定
#define VFS_FCHK
#include <shlobj.h>
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#pragma hdrstop

#pragma pack(push, 1)
typedef struct tagUSERTYPEBLOCK {
	struct tagUSERTYPEBLOCK *next;
	DWORD first, last;
	DWORD size;
	char data[];
} USERTYPEBLOCK;

typedef struct tagUSERFILETYPE {
	struct tagUSERFILETYPE *next;
	TCHAR *name;
//	USERTYPEBLOCK block[]; // VCでは []配列を含む構造体の[]配列はできない
} USERFILETYPE;
#pragma pack(pop)

#define HEADERTYPE_ABS B0 // 固定ヘッダ(実質使用せず)
#define HEADERTYPE_REL B1 // 浮動ヘッダ
#define HEADERTYPE_REL_LAST B2 // 浮動ヘッダ、チェック用

#define AH(off, size, head, func, name, memo, ext, code)\
	{HEADERTYPE_ABS, off, size, head, func, T(name), T(memo), T(ext), code}
#define RH(maxl, size, head, func, name, memo, ext, code)\
	{HEADERTYPE_REL, maxl, size, head, func, T(name), T(memo), T(ext), code}
#define RH_L(maxl, size, head, func, name, memo, ext, code)\
	{HEADERTYPE_REL | HEADERTYPE_REL_LAST, maxl, size, head, func, T(name), T(memo), T(ext), code}
#define JustH(off, size, head, func, name, memo, ext, code)\
	{ 0, off, size, head, func, T(name), T(memo), T(ext), code}
#define NH(off, size, head, func, name, memo, ext, code)\
	{ 0, off, size, head, func, T(name), T(memo), T(ext), code}


#define DT_scp		(DISPT_TEXT) // デフォルト VD_systemcp
#define DT_jis		(0x10000 | DISPT_TEXT) // VD_jis
#define DT_utf16L	(0x20000 | DISPT_TEXT) // VD_unicode
#define DT_html		(0x30000 | DISPT_TEXT) // VD_html
#define DT_rtf		(0x40000 | DISPT_TEXT) // VD_rtf
#define DT_word7	(0x50000 | DISPT_DOCUMENT) // VD_word7
#define DT_oasys	(0x60000 | DISPT_DOCUMENT) // VD_oasys
#define DT_utextL	(0x70000 | DISPT_DOCUMENT) // VD_unitext
#define DT_utf16B	(0x80000 | DISPT_TEXT) // VD_unicodeB
#define DT_utextB	(0x90000 | DISPT_DOCUMENT) // VD_unitextB
#define DT_utf8		(0xa0000 | DISPT_DOCUMENT) // VD_utf8
#define DT_c		(0xb0000 | DISPT_TEXT) // VD_c
#define DT_emf		(0xc0000 | DISPT_DOCUMENT) // VD_emf
#define DT_word8	(0xd0000 | DISPT_DOCUMENT) // VD_word8
#define DT_mail		(0xe0000 | DISPT_TEXT) // VD_mail
#define DT_xml		(0xf0000 | DISPT_TEXT) // VD_xml
#define DT_uhtml	(0x100000 | DISPT_TEXT) // VD_u_html
#define DT_officex	(0x110000 | DISPT_TEXT) // VD_officezip
#define DT_text		(0x120000 | DISPT_TEXT) // VD_text
#define DT_wordx	(0x130000 | DISPT_DOCUMENT) // VD_wordx

/*================================================================ ユーザ定義
 USERFILETYPE
  USERTYPEBLOCK
  USERTYPEBLOCK
		:
  USERFILETYPEのname本体
 USERFILETYPE
		:
*/

USERFILETYPE tmptype = {NULL, NULL};
char *usertypes = INVALID_HANDLE_VALUE;

void MakeUserFileType(void)
{
	ThSTRUCT th;
	int index = 0;
	TCHAR kword[MAX_PATH], buf[CMDLINESIZE], *p, *q;
	DWORD typeoffset;
	USERTYPEBLOCK tempblock;
#ifdef UNICODE
	char bufA[MAX_PATH];
#endif
	ThInit(&th);
	buf[0] = '\0';
	while ( EnumCustTable(index++, T("X_uftyp"), kword, buf, sizeof(buf)) >= 0 ){
		typeoffset = th.top;
		ThAppend(&th, &tmptype, sizeof(tmptype));
		p = buf;
		while ( *p != '\0' ){
			// 検索範囲を求める
			tempblock.next = NULL;
			if ( *p == '-' ){ // -n
				p++;
				tempblock.first = 0;
				tempblock.last = (DWORD)GetNumber((const TCHAR**)&p);
			}else{
				tempblock.first = (DWORD)GetNumber((const TCHAR**)&p);
				if ( *p == '-' ){ // n-m
					p++;
					tempblock.last = (DWORD)GetNumber((const TCHAR**)&p);
				}else{
					tempblock.last = tempblock.first;
				}
			}
			if ( *p == ',' ) p++;
			// 検索文字列を取得する
			q = tstrchr(p, ',');
			if ( q != NULL ){
				*q++ = '\0';
			}else{
				q = p + tstrlen(p);
			}
#ifdef UNICODE
			UnicodeToAnsi(p, bufA, MAX_PATH);
			#define DataPtr bufA
#else
			#define DataPtr p
#endif
			{	// 非文字の処理
				char *src, *dst;
				src = dst = DataPtr;
				while ( *src ){
					char c;

					if ( IskanjiA(*src) ){
						*src++ = *dst++;
						*src++ = *dst++;
						continue;
					}

					if ( *src != '\\' ){
						*src++ = *dst++;
						continue;
					}
					src++;
					if ( *src == 'x' ) src++;
					c = (char)GetHexCharA(&src);
					*dst++ = (char)((c * 16) + GetHexCharA(&src));
				}
				tempblock.size = (SIZE32_T)(dst - DataPtr);
				tempblock.next = (USERTYPEBLOCK *)(th.top + sizeof(USERTYPEBLOCK) + tempblock.size);
			}
			ThAppend(&th, &tempblock, sizeof(tempblock));
			ThAppend(&th, DataPtr, tempblock.size);
			p = q;
		}
		((USERFILETYPE *)ThPointer(&th, typeoffset))->name = (TCHAR *)(DWORD_PTR)th.top;
		ThAddString(&th, kword);
		((USERFILETYPE *)ThPointer(&th, typeoffset))->next = (USERFILETYPE *)(DWORD_PTR)th.top;
	}
	if ( !th.top ){
		usertypes = NULL;
		ThFree(&th);
		return;
	}
	usertypes = th.bottom;
	{		// offset を pointer に変換
		size_t offset = 0, next;

		for ( ; ; ){
			USERFILETYPE *tmpft;
			USERTYPEBLOCK *tmpbl;

			tmpft = (USERFILETYPE *)ThPointer(&th, offset);
			tmpft->name = ThPointerT(&th, (size_t)tmpft->name);

			// USERTYPEBLOCK の変換
			tmpbl = (USERTYPEBLOCK *)(tmpft + 1); // USERFILETYPE直後BLOCKを指
			for ( ; ; ){
				tmpbl->next = (USERTYPEBLOCK *)ThPointer(&th, (size_t)tmpbl->next);
				if ( tmpbl->next == (USERTYPEBLOCK *)tmpft->name ){
					tmpbl->next = NULL;
					break;
				}
				tmpbl = tmpbl->next;
			}

			// USERFILETYPE の変換
			next = (size_t)tmpft->next;
			tmpft->next = (USERFILETYPE *)ThPointer(&th, next);
			if ( next >= th.top ){
				tmpft->next = 0;
				break;
			}else{
				offset = next;
				continue;
			}
		}
	}
}

void SetUserFileType(VFSFILETYPE *dest, const USERFILETYPE *src)
{
	dest->dtype = 2;
	tstrcpy(dest->type, src->name);
	if ( dest->flags & VFSFT_TYPETEXT ) tstrcpy(dest->typetext, T("User define"));
	if ( dest->flags & VFSFT_EXT      ) tstrcpy(dest->ext, NilStr);
}

BOOL CheckUserFileType(const char *image, DWORD size, VFSFILETYPE *result)
{
	USERFILETYPE *ut;

	if ( usertypes == INVALID_HANDLE_VALUE ) MakeUserFileType();
	for ( ut = (USERFILETYPE *)usertypes ; ut != NULL ; ut = ut->next ){
		USERTYPEBLOCK *ub;
		const char *ptr;

		ub = (USERTYPEBLOCK *)(ut + 1); // USERFILETYPE直後のUSERTYPEBLOCKを指
		ptr = image;
		for ( ; ; ){
			if ( ub->first == ub->last ){ // 固定位置ヘッダ
				ptr = image + ub->first;
				if ( memcmp(ptr, ub->data, ub->size) ) break;
				ptr += ub->size;
			}else{ //浮動位置ヘッダ
				DWORD leftsize;
				char *p = NULL;

				if ( size <= ub->last ) break; // サイズ不足
				leftsize = size - ub->size;
				if ( ub->first ) ptr = image + ub->first;
				if ( ub->last ){
					if ( leftsize > (ub->last - ub->first) ){
						leftsize = ub->last - ub->first;
					}
				}else{
					if ( leftsize > 200 * KB ) leftsize = 200 * KB;
				}
				while ( (leftsize > 0) &&
						(p = memchr(ptr, *ub->data, leftsize)) != NULL ){
					if ( !memcmp(p + 1, ub->data + 1, ub->size - 1) ){
						ptr = p + ub->size;
						break;
					}
					leftsize -= (SIZE32_T)(p - ptr + 1);
					if ( leftsize <= 0 ){
						p = NULL;
						break;
					}
					ptr = p + 1;
				}
				if ( p == NULL ) break; // 該当しなかった
			}
			// この ub は該当する
			ub = ub->next;
			if ( ub == NULL ){	// 最後まで該当→一致した
				if ( result != NULL ) SetUserFileType(result, ut);
				return TRUE;
			}
		}
	}
	return FALSE;
}

//================================================================ 詳細チェック
void SetFileType(VFSFILETYPE *dest, const VFD_HEADERS *src)
{
	dest->dtype = src->dtype;
	tstrcpy(dest->type, src->type);
	if ( dest->flags & VFSFT_TYPETEXT ) tstrcpy(dest->typetext, src->typetext);
	if ( dest->flags & VFSFT_EXT      ) tstrcpy(dest->ext, src->ext);
}

// HTML -----------------------------------------------------------------------
const VFD_HEADERS vfd_filelinkheader =
	NH(0, 0, NULL, NULL, ":FILELINK", "File Link", "lnk", DT_scp);

#pragma argsused
ERRORCODE vfd_link(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	TCHAR orgname[VFPS];
	UnUsedParam(image);UnUsedParam(size);UnUsedParam(header);

	if ( SUCCEEDED(GetLink(NULL, fname, orgname)) ){
		if ( (GetFileAttributesL(orgname) & FILE_ATTRIBUTE_DIRECTORY) == 0 ){
			SetFileType(result, &vfd_filelinkheader);
			return ERROR_MORE_DATA;
		}
	}
	return NO_ERROR;
}

// HTML -----------------------------------------------------------------------
const VFD_HEADERS vfd_XHTMLheader =
	NH(0, 0, NULL, NULL, ":HTML", "XHTML", "html", DT_html);
const VFD_HEADERS vfd_webmheader =
	NH(0, 0, NULL, NULL, ":WEBM", "webm", "webm movie", DT_scp);
const VFD_HEADERS vfd_matroskaheader =
	NH(0, 0, NULL, NULL, ":MKV", "mkv", "matroska movie", DT_scp);
/*
.mkv ‐ 映像と音声を含めたビデオファイル
.mka ‐ 音声のみを含めた音楽ファイル
.mks ‐ 字幕を含
*/
#pragma argsused
ERRORCODE vfd_HTML2(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	char c;
	UnUsedParam(fname); UnUsedParam(image); UnUsedParam(size); UnUsedParam(result);

	c = *(header + 14);
	if ( (c == '\r') || (c == ';') ) return NO_ERROR;
	return ERROR_NO_DATA_DETECTED;
}

#pragma argsused
ERRORCODE vfd_HTML(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	const char *p, *maxptr;
	UnUsedParam(fname);

	// ヘッダより前のチェック
	p = image;
	if ( p < header ){
		if ( memcmp(p, UTF8HEADER, 3) == 0 ) p += 3;

		for ( ; p < header ; p++ ){
			if ( (*p != ' ')  && (*p != '\t') &&
				 (*p != '\n') && (*p != '\r') ){
				return ERROR_NO_DATA_DETECTED;
			}
		}
	}
	// ヘッダ後の '>' チェック
	maxptr = image + size;
	for ( ; p < maxptr ; p++ ){
		if ( (*p == '\r') || (*p == '\n') || (*p == '\t') ) continue;
		if ( *(BYTE *)p < 0x20 ) break;
		if ( *p != '>' ) continue;

		if ( *(header + 1) != '?' ) return NO_ERROR; // html
		// xml なら xhtml のチェック
		p = memchr(p, '<', maxptr - p);
		if ( p == NULL ) return NO_ERROR;
		if ( memcmp(p + 1, "!DOCTYPE html", 13) != 0 ) return NO_ERROR;
		SetFileType(result, &vfd_XHTMLheader);
		return ERROR_MORE_DATA;
	}
	return ERROR_NO_DATA_DETECTED;
}

#pragma argsused
ERRORCODE vfd_EBML(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname); UnUsedParam(header);

	if ( size > 0x40 ){
		if ( !memcmp(image + 0x1a, "\x81\x08\x42\x82\x84\x77\x65\x62", 8) ){
			SetFileType(result, &vfd_webmheader);
			return ERROR_MORE_DATA;
		}
		if ( !memcmp(image + 0x1a, "\x81\x08\x42\x82\x88\x6d\x61\x74", 8) ){
			SetFileType(result, &vfd_matroskaheader);
			return ERROR_MORE_DATA;
		}
		return NO_ERROR;
	}
/*
	for ( ; (p + 1) < maxptr ; p++ ){
		int size;

		c = *p;
		if ( c & 0x10 ){
			size = c & 0x7f;
		}else if ( ((c & 0xc0) == 0x40) && ((p + 2) < maxptr) ){
			size = ((c & 0x3f) << 8) + *(p + 1);
			p += 1;
		}else if ( ((c & 0xe0) == 0x20) && ((p + 3) < maxptr) ){
			size = ((c & 0x1f) << 8) + (*(p + 1) << 8) + *(p + 2);
			p += 2;
		}else if ( ((c & 0xf0) == 0x10) && ((p + 4) < maxptr) ){
			size = ((c & 0x0f) << 8) + (*(p + 1) << 8) + (*(p + 2) << 8) + *(p + 3);
			p += 2;
		}else if ( ((c & 0xf8) == 0x08) && ((p + 5) < maxptr) ){
			size = ((c & 0x0f) << 8) + (*(p + 1) << 8) + (*(p + 2) << 8) + (*(p + 3) << 8) + *(p + 4);
			p += 3;
		}
		SetFileType(result, &vfd_XHTMLheader);
		return ERROR_MORE_DATA;
	}
*/
	return ERROR_NO_DATA_DETECTED;
}
// TEXT -----------------------------------------------------------------------
#pragma argsused
ERRORCODE vfd_text(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	const char *maxptr;
	UnUsedParam(fname);UnUsedParam(header);UnUsedParam(result);

	maxptr = image + size;
	for ( ; image < maxptr ; image++ ){
		if ( ((BYTE)*image < (BYTE)' ') && (*image != '\t') &&
			 (*image != '\r') && (*image != '\n') ){
			 return ERROR_NO_DATA_DETECTED;
		}
	}
	return NO_ERROR;
}

// PNG ------------------------------------------------------------------------
const VFD_HEADERS vfd_APNG =
	NH(0, 0, NULL, NULL, ":APNG", "APNG", "png", DT_scp);
const VFD_HEADERS vfd_FPNG =
	NH(0, 0, NULL, NULL, ":FPNG", "firework PNG", "png", DT_scp);
#pragma argsused
ERRORCODE vfd_PNG(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	const char *ptr, *maxptr;
	UnUsedParam(fname);UnUsedParam(header);

	ptr = image + 0x21;
	maxptr = image + size;
	for ( ; ptr < maxptr ; ){
		DWORD nextoffset;

		nextoffset = *(DWORD *)ptr;
		if ( nextoffset == 0 ) return NO_ERROR;
		switch ( *(DWORD *)(ptr + 4) ){
			case 0x4c546361: // acTL ?
				SetFileType(result, &vfd_APNG);
				return ERROR_MORE_DATA;

			case 0x46426b6d: // mkBG ?
				SetFileType(result, &vfd_FPNG);
				return ERROR_MORE_DATA;

			case 0x54414449: // IDAT ?
				return NO_ERROR;
		}
		ptr += (DWORD)((nextoffset >> 24) + ((nextoffset >> 8) & 0xff00) + ((nextoffset << 8) & 0xff0000)) + 12;
	}
	return ERROR_NO_DATA_DETECTED;
}

// BMP ------------------------------------------------------------------------
#pragma argsused
ERRORCODE vfd_BMP(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(size);UnUsedParam(result);

	if ( (*(header + 16) == 0) &&			// イメージのオフセットが64k未
		(*(WORD *)(header + 06) == 0) ){	// reserved が使われていない？

// ●ここに Version を入れたい(^^;

		return NO_ERROR;
	}else{
		return ERROR_NO_DATA_DETECTED;
	}
}
// FAT Disk Image -------------------------------------------------------------
const VFD_HEADERS vfd_NTFSheader =
	NH(0, 0, NULL, NULL, ":NTFS", "NTFS Disk Image", "BIN", DT_scp);
const VFD_HEADERS vfd_exFATheader =
	NH(0, 0, NULL, NULL, ":EXFAT", "exFAT Disk Image", "BIN", DT_scp);

#pragma argsused
ERRORCODE vfd_MBRIMG(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname);UnUsedParam(size);UnUsedParam(header);UnUsedParam(result);

	if ( ((*(image + 0x1be) | *(image + 0x1ce) | *(image + 0x1de) | *(image + 0x1ee)) & 0x7f) == 0 ){
		return NO_ERROR;
	}
	return ERROR_NO_DATA_DETECTED;
}

#pragma argsused
ERRORCODE vfd_FATIMG(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	int fatresult;
	UnUsedParam(fname);UnUsedParam(header);

	fatresult = CheckFATImage((const BYTE *)image, (const BYTE *)image + size);
	switch ( fatresult ){
		case CHECKFAT_NONE:
			return ERROR_NO_DATA_DETECTED;

		case CHECKFAT_EXFAT:
			SetFileType(result, &vfd_exFATheader);
			return ERROR_MORE_DATA;

		case CHECKFAT_NTFS:
			SetFileType(result, &vfd_NTFSheader);
			return ERROR_MORE_DATA;
	}
	return NO_ERROR;
}

#pragma argsused
ERRORCODE vfd_yaffs(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(result);
	if ( (size >= (0x800 + 64)) &&
		 (*(DWORD *)(header + 0x804) == 1) &&
		 (*(DWORD *)(header + 0x80c) == 0xffff) ){
		return NO_ERROR;
	}
	return ERROR_NO_DATA_DETECTED;
}

#pragma argsused
ERRORCODE vfd_squashfs(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(result);
	if ( (size >= 0x800) &&
		 (*(WORD *)(header + 0xc) == 0) && // ブロックサイズ(BE)が 8k 未満？
		 (*(header + 0xf) == 0) &&
		 (*(header + 0xe) != 0) &&
		 (*(header + 0xe) <= 0x20) ){
		return NO_ERROR;
	}
	return ERROR_NO_DATA_DETECTED;
}

// LHA ------------------------------------------------------------------------
#pragma argsused
ERRORCODE vfd_LHA(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	BYTE sum = 0, *p, chksum;
	UnUsedParam(fname);UnUsedParam(result);

	if ( !IsalnumA(*(header + 3)) ) return ERROR_NO_DATA_DETECTED;	// -lh"?"
	if ( *(BYTE *)(header + 0x12) >= 3 ) return ERROR_NO_DATA_DETECTED; // hdr
											// 範囲チェック
	if ( (image + 2) > header ) return ERROR_NO_DATA_DETECTED;
	p = (BYTE *)header + *(BYTE *)(header - 2);
	if ( (image + size) <= (char *)p ) return ERROR_NO_DATA_DETECTED;
											// sum check
	chksum = (BYTE)*(header - 1);
	if ( chksum == 0 ) return NO_ERROR;	// sum が登録されていない！！
	while ( (BYTE *)header < p )	sum += *(BYTE *)header++;
	return (sum == chksum) ? NO_ERROR : ERROR_NO_DATA_DETECTED;
}
// ARJ ------------------------------------------------------------------------
#pragma argsused
ERRORCODE vfd_ARJ(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(size);UnUsedParam(result);
	// 固定値
	if ( *(BYTE *)(header + 10) != 2 ) return ERROR_NO_DATA_DETECTED;

	// OS種別の制限
	if ( *(BYTE *)(header + 7) > 10 ) return ERROR_NO_DATA_DETECTED;

	// ヘッダサイズの制限
	if ( (*(BYTE *)(header + 2) + (*(BYTE *)(header + 3) << 8)) <= 2600 ){
		return NO_ERROR;
	}
	return ERROR_NO_DATA_DETECTED;
}
// CAB ------------------------------------------------------------------------
#pragma argsused
ERRORCODE vfd_CAB(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(size);UnUsedParam(result);
	// 書庫の実体へのオフセット上位16bit ... 1M 越えはエラー
	if ( *(WORD *)(header + 18) >= 0x10 ) return ERROR_NO_DATA_DETECTED;
	return NO_ERROR;
}
// PKZIP ----------------------------------------------------------------------
const char Odfheader[] = "mimetypeapplication/vnd.oasis.opendocument";
const VFD_HEADERS vfd_Odfheader[] = {
	AH(0, 12, "presentation",	NULL, ":ODP", "OpenDocument Impress V2", "odp", DT_scp),
	AH(0, 11, "spreadsheet",	NULL, ":ODS", "OpenDocument Calc V2", "ods", DT_scp),
	AH(0, 13, "text-template", NULL, ":OTT", "OpenDocument Writer Template V2", "ott", DT_scp),
	AH(0, 4, "base",			NULL, ":ODT", "OpenDocument Base V2", "odb", DT_scp),
	AH(0, 7, "formula",		NULL, ":ODF", "OpenDocument Fomula V2", "odf", DT_scp),
	AH(0, 8, "graphics",		NULL, ":ODG", "OpenDocument Graph V2", "odg", DT_scp),
	AH(0, 4, "text",			NULL, ":ODT", "OpenDocument Writer V2", "odt", DT_scp),
	NH(0, 0, NULL,			NULL, ":ODT", "OpenDocument V2", "odt", DT_scp)
};
const char Sofheader[] = "mimetypeapplication/vnd.sun.xml";
const VFD_HEADERS vfd_Sofheader[] = {
	AH(0, 0, "writer",		NULL, ":SXW", "OpenDocument Writer V1", "sxw", DT_scp),
	NH(0, 0, NULL,			NULL, ":SXW", "OpenDocument V1", "sxw", DT_scp)
};
const char XPSheader1[] = "Metadata/Job_PT.xml";
const char XPSheader2[] = "[Content_Types].xml";
const VFD_HEADERS vfd_XPSheader =
	NH(0, 0, NULL, NULL, ":XPS", "XPS Document", "xps", DT_scp);

ERRORCODE SearchPkExtHeader(const char *header, const VFD_HEADERS *vdh, VFSFILETYPE *result)
{
	for ( ; vdh->flags ; vdh++ ){
		if ( !memcmp(header, vdh->header, vdh->hsize) ){
			break;
		}
	}
	SetFileType(result, vdh);
	return ERROR_MORE_DATA;
}

const VFD_HEADERS vfd_XPSheaders[] = {
RH(0xc00, 10, "xl/workboo", NULL, ":XLSX",	"Excel2007", "xlsx", DT_scp),
RH(0xc00, 16, "xl/_rels/workboo", NULL, ":XLSX",	"Excel2007", "xlsx", DT_scp),
RH(0xc00, 12, "xl/drawings/", NULL, ":XLSX",	"Excel2007", "xlsx", DT_scp),
RH(0xc00, 10, "word/docum", NULL, ":DOCX",	"Word2007", "docx", DT_wordx),
RH(0xc00, 16, "word/_rels/docum", NULL, ":DOCX",	"Word2007", "docx", DT_scp),
RH(0x1000, 11, "ppt/slides/", NULL, ":PPTX",	"PowerPoint2007", "pptx", DT_scp),
RH(0x1000, 13, "ppt/drawings/", NULL, ":PPTX",	"PowerPoint2007", "pptx", DT_scp),
NH(0, 0, NULL, NULL, ":XPS", "XPS Document", "xps", DT_scp),
};

#pragma argsused
ERRORCODE vfd_PK(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	DWORD neededversion;
	UnUsedParam(fname);UnUsedParam(image);

	neededversion = *(header + 4);
	if ( (neededversion < 10/* V1.0*/) || (neededversion >= 100/* V10.0*/) ){
		return ERROR_NO_DATA_DETECTED;
	}

	if ( size > 0x80 ){
		if ( !memcmp(header + 0x1e, Odfheader, sizeof(Odfheader) - 1) ){
			return SearchPkExtHeader(header + 0x1e + sizeof(Odfheader),
					vfd_Odfheader, result);
		}
		if ( !memcmp(header + 0x1e, Sofheader, sizeof(Sofheader) - 1) ){
			return SearchPkExtHeader(header + 0x1e + sizeof(Sofheader),
					vfd_Sofheader, result);
		}
		if ( !memcmp(header + 0x1e, XPSheader1, sizeof(XPSheader1) - 1) ){
			SetFileType(result, &vfd_XPSheader);
			return ERROR_MORE_DATA;
		}

		if ( !memcmp(header + 0x1e, XPSheader2, sizeof(XPSheader2) - 1) ){
			const VFD_HEADERS *vdh;

			for ( vdh = vfd_XPSheaders ; vdh->flags ; vdh++ ){
				const char *ptr, *p;
				DWORD leftsize;
				#define xps_skipsize 0x200

				if ( size < (xps_skipsize + vdh->hsize) ) continue;
				ptr = header + xps_skipsize;
				leftsize = size - xps_skipsize;
				if ( leftsize > vdh->off ) leftsize = vdh->off;
				while ( (leftsize > 0) &&
						(p = memchr(ptr, *vdh->header, leftsize)) != NULL ){
					if ( !memcmp(p + 1, vdh->header + 1, vdh->hsize - 1) ){
						goto found;
					}
					leftsize -= (SIZE32_T)(p - ptr + 1);
					if ( leftsize <= 0 ) break;
					ptr = p + 1;
				}
			}
found:
			SetFileType(result, vdh);
			return ERROR_MORE_DATA;
		}
	}
	return NO_ERROR;
}
// MSXBIN ---------------------------------------------------------------------
#pragma argsused
ERRORCODE vfd_MSXBIN(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	WORD s, e;
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(size);UnUsedParam(result);

	s = *(WORD *)(header + 1);
	e = *(WORD *)(header + 3);
	if ( s < e ) return NO_ERROR;
	return ERROR_NO_DATA_DETECTED;
}
// FM-TOWNS MQ ----------------------------------------------------------------
#pragma argsused
ERRORCODE vfd_MQ(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(result);

	if ( (size > 0x40) && (*(WORD *)(header + 12) == MAX16) ) return NO_ERROR;
	return ERROR_NO_DATA_DETECTED;
}
// FM-TOWNS P3 ----------------------------------------------------------------
#pragma argsused
ERRORCODE vfd_P3(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(result);

	if ( (size > 0x40) && (*(DWORD *)(header + 0x5a) == MAX32) ) return NO_ERROR;
	return ERROR_NO_DATA_DETECTED;
}
// MACBIN ---------------------------------------------------------------------
#pragma argsused
ERRORCODE vfd_MACBIN(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	int len;
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(size);UnUsedParam(result);

	len = header[1];
	if ( (len < 1) || (len > 63 ) ) return ERROR_NO_DATA_DETECTED;

	header += 2;
	while (len){
		if ( (*(BYTE *)header) < 0x20 ) return ERROR_NO_DATA_DETECTED;
		header++;
		len--;
	}
	return NO_ERROR;
}

typedef struct {
	BYTE tag[2];
	BYTE format[2];
	BYTE size[4];
	BYTE data[4];
} TIFFDIR;

DWORD GetTiffDWORD(BYTE *p, BOOL BE)
{
	if ( BE == FALSE ) return *(DWORD *)p;
	return (*p * 0x1000000) + (*(p+1) * 0x10000) + (*(p+2) * 0x100) + *(p+3);
}
DWORD GetTiffWORD(BYTE *p, BOOL BE)
{
	if ( BE == FALSE ) return *(WORD *)p;
	return (*p * 0x100) + *(p+1);
}

char *GetTiffStr(BYTE *base, TIFFDIR *dir, BOOL BE)
{
	if ( GetTiffDWORD(dir->size, BE) <= 4 ) return (char *)dir->data;
	return (char *)(base + GetTiffDWORD(dir->data, BE));
}

#ifdef UNICODE
void ThCatStringTA(ThSTRUCT *th, const char *str)
{
	WCHAR buf[0x1000];

	buf[0] = '\0';
	AnsiToUnicode(str, buf, 0x1000);
	ThCatString(th, buf);
}
#else
#define ThCatStringTA(th, str) ThCatString(th, str)
#endif

#ifdef UNICODE
#define ThCatStringTW(th, str) ThCatString(th, str)
#else
void ThCatStringTW(ThSTRUCT *th, WCHAR *wstr)
{
	char buf[0x1000];

	buf[0] = '\0';
	UnicodeToAnsi(wstr, buf, 0x1000);
	ThCatString(th, buf);
}
#endif

void GetTiffRate(BYTE *base, TIFFDIR *dir, BOOL BE, TCHAR *buf)
{
	DWORD offset, v1, v2;

	offset = GetTiffDWORD(dir->data, BE);
	if ( offset < 0x10000 ){
		v1 = *(DWORD *)(base + offset);
		v2 = *(DWORD *)(base + offset + 4);
		if ( (v1 < 0x10000) && (v2 < 0x10000) && (v2 != 0) ){
			if ( v2 == 1 ){
				wsprintf(buf, T(" %u\r\n"), v1);
			}else{
				wsprintf(buf, T(" %u/%u\r\n"), v1, v2);
			}
			return;
		}
	}
	tstrcpy(buf, T("?/?\r\n"));
}

void ThCatTiffData(ThSTRUCT *th, const TCHAR *header, BYTE *base, TIFFDIR *dir, BOOL BE)
{
	TCHAR buf[32];

	ThCatString(th, header);
	switch( GetTiffWORD(dir->format, BE) ){
		case 1:
			wsprintf(buf, T(" %u\r\n"), dir->data[0]);
			break;

		case 2:
			ThCatString(th, T(" "));
			ThCatStringTA(th, GetTiffStr(base, dir, BE));
			ThCatString(th, T("\r\n"));
			return;

		case 3:
			wsprintf(buf, T(" %u\r\n"), GetTiffWORD(dir->data, BE));
			break;

		case 4:
			wsprintf(buf, T(" %u\r\n"), GetTiffDWORD(dir->data, BE));
			break;

		case 5:
			GetTiffRate(base, dir, BE, buf);
			break;

		case 6:
			wsprintf(buf, T(" %d\r\n"), dir->data[0]);
			break;

		case 8:
			wsprintf(buf, T(" %d\r\n"), GetTiffWORD(dir->data, BE));
			break;

		case 9:
			wsprintf(buf, T(" %d\r\n"), GetTiffDWORD(dir->data, BE));
			break;

		default:
			ThCatString(th, T(" [?]\r\n"));
			return;
	}
	ThCatString(th, buf);
}


void GetExifInfo(BYTE *base, BYTE *basemax, ThSTRUCT *th)
{
	BOOL BE = FALSE;
	TIFFDIR *dir;
	int count;
	BYTE *p;
	const TCHAR *name;
	DWORD tag;
	TCHAR namebuf[16];

	ThCatString(th, T("*Exif\r\n"));
	if ( *base == 'M' ) BE = TRUE;
	p = base + GetTiffDWORD(base + 4, BE);
	count = GetTiffWORD(p, BE);
	dir = (TIFFDIR *)(p + 2);
	while ( count && ((BYTE *)dir < basemax) ){
		tag = GetTiffWORD(dir->tag, BE);
		switch( tag ){
			case 0x100:
				name = T("**Width:");
				break;
			case 0x101:
				name = T("**Length:");
				break;
			case 0x102:
				name = T("**BitsPerSample:");
				break;
			case 0x106:
				name = T("**PhotometricInterpretation:");
				break;
			case 0x10e:
				name = T("**Comment:");
				break;
			case 0x10f:
				name = T("**Maker:");
				break;
			case 0x110:
				name = T("**Model:");
				break;
			case 0x112:
				name = T("**Rotate:");
				break;
			case 0x115:
				name = T("**SamplesPerPixel:");
				break;
			case 0x11a:
				name = T("**XResolution:");
				break;
			case 0x11b:
				name = T("**YResolution:");
				break;
			case 0x128:
				name = T("**ResolutionUnit:");
				break;
			case 0x131:
				name = T("**Software:");
				break;
			case 0x132:
				name = T("**Date:");
				break;
			case 0x13b:
				name = T("**Artist:");
				break;
			case 0x013e:
				name = T("**WhitePoint:");
				break;
			case 0x013f:
				name = T("**PrimaryChromaticities:");
				break;
			case 0x0211:
				name = T("**YCbCrCoefficients:");
				break;
			case 0x0213:
				name = T("**YCbCrPositioning:");
				break;
			case 0x8298:
				name = T("**Copyright:");
				break;

			case 0x8825:
				name = T("**GPStag:");
				break;

			case 0x8769: // Exif IFD Pointer
				count--;
				dir++;
				continue;

			case 0x9c9b:
				name = T("**WinTitle:");
				break;

			case 0x9c9c:
				name = T("**WinComment:");
				break;

			case 0x9c9d:
				name = T("**WinAuthor:");
				break;

			case 0xc4a5:
				name = T("**PrintImageMatching:");
				break;

			default:
				wsprintf(namebuf, T("**%04d:"), tag);
				name = namebuf;
		}
		ThCatTiffData(th, name, base, dir, BE);
		count--;
		dir++;
	}
}

#define BEWORDPTR(ptr) (WORD)((*(ptr) * 0x100) + *(ptr + 1))
#define BEDWORDPTR(ptr) (DWORD)((*(ptr) * 0x1000000) + (*(ptr+1) * 0x10000) + (*(ptr+2) * 0x100) + *(ptr+3))
void AddTTFString(ThSTRUCT *th, const TCHAR *title, BYTE *basestr, BYTE *header)
{
	BYTE *str;
	DWORD size;
	int type; // 0:multibyte 1:unicode
	TCHAR buf[0x200];

	ThCatString(th, title);
	str = basestr + BEWORDPTR(header + 10);
	size = BEWORDPTR(header + 8);
	switch (BEWORDPTR(header)){
		case 0: // UNICODE
			type = 1;
			break;
		case 1: // Macintosh
			type = 0;
			break;
		case 3: // Microsoft
			type = BEWORDPTR(header + 2) == 10 ? 1 : 0;
			break;
		default:
			return;
	}
	if (type){
		WCHAR wtemp[0x200], *wp;
		int i;

		for (wp = wtemp, i = size / 2; i ; i-- ){
			*wp++ = (WCHAR)(*str * 0x100 + *(str + 1));
			str += 2;
		}
		#ifdef UNICODE
			*wp = '\0';
			ThCatString(th, wtemp);
		#else
			buf[WideCharToMultiByte(
					CP_ACP, 0, wtemp, size / 2, buf, 0x200, NULL, NULL)] = '\0';
			ThCatString(th, buf);
		#endif
	}else{
		#ifdef UNICODE
			buf[MultiByteToWideChar(
					CP_ACP, MB_PRECOMPOSED, (char *)str, size, buf, 0x200)] = '\0';
			ThCatString(th, buf);
		#else
			ThAppend(th, str, size);
		#endif
	}
	ThCatString(th, T("\r\n"));
}

#pragma argsused
ERRORCODE vfd_TTF(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	ThSTRUCT th;
	int headers;
	BYTE *p, *imagemax, *np, *strbase, *base;
	UnUsedParam(fname);

	if ( result == NULL ) return NO_ERROR;
	if ( !(result->flags & VFSFT_INFO) ) return NO_ERROR;
	ThInit(&th);
	if ( *header == 't' ){ // TTC
		base = (BYTE *)(header + BEDWORDPTR(header + 12));
	}else{	// TTF
		base = (BYTE *)header;
	}
	headers = BEWORDPTR(base + 4);
	p = (BYTE *)(base + 0xc);

	for ( ; headers ; headers--, p += 0x10){
		if ( memcmp(p, "name", 4) ) continue;
		np = (BYTE *)(image + BEDWORDPTR(p + 8));
		imagemax = (BYTE *)(np + BEDWORDPTR(p + 12));
		if ( (BYTE *)(image + size) < imagemax ) break; // データ不足
		headers = BEWORDPTR(np + 2);
		strbase = np + BEWORDPTR(np + 4);
		np += 6;
		for ( ; headers ; headers--, np += 12 ){
			switch(BEWORDPTR(np + 6)){
				case 1:
					AddTTFString(&th, T("*Font Family:"), strbase, np);
					break;
				case 2:
					AddTTFString(&th, T("*Font Subfamily:"), strbase, np);
					break;
				case 4:
					AddTTFString(&th, T("*Full Font name:"), strbase, np);
					break;
				case 5:
					AddTTFString(&th, T("*Version:"), strbase, np);
					break;
				default:
	//				AddTTFString(&th, T("*?:"), strbase, np);
					break;
			}
		}
		break;
	}
	result->info = (TCHAR *)th.bottom;
	return NO_ERROR;
}
//  ---------------------------------------------------------------------
#pragma argsused
ERRORCODE vfd_JPEG(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	ThSTRUCT th;
	TCHAR buf[VFPS];
	BYTE *p, *imagemax;
	UnUsedParam(fname);

	if ( result == NULL ) return NO_ERROR;
	if ( !(result->flags & VFSFT_INFO) ) return NO_ERROR;

	ThInit(&th);
	p = (BYTE *)(header + 2);
	imagemax = (BYTE *)(image + size);
	while ( (p + 8) < imagemax ){
		if ( *p != 0xff ) break; // 識別子ではない
		switch ( *(p + 1) ){
			case 0xC0:	// SOF0 baseline
			case 0xC1:	// SOF1 seq
			case 0xC2:	// SOF2 prog
			case 0xC3:	// SOF3 lossless
				wsprintf(buf, T("*SOF%d\r\n"), *(p + 1) - 0xc0);
				ThCatString(&th, buf);
				wsprintf(buf, T("*Size:%dx%d\r\n*colors %d\r\n"), BEWORDPTR(p+7), BEWORDPTR(p+5), *(p + 9));
				ThCatString(&th, buf);
				break;

//			case 0xC4:	// DHT ハフマンテーブル
//			case 0xD0:	// FFD0～FFD7 RSTi Restart Markers
//			case 0xD8:	// SOI Start Of Image
			case 0xD9:	// EOI End Of Image
			case 0xDA:	// SOS Start Of Scan
				p = imagemax;
				break;

//			case 0xDB:	// DQT 量子化テーブル
//			case 0xDC:	// DNL Define Number of Lines
//			case 0xDD:	// DRI Define Restart Interval

//			case 0xE0:	// APP0 JFIF 情報
			case 0xE1:	// APP1 Exif 情報
				if ( !memcmp(p+4, "Exif", 5) ) GetExifInfo(p + 10, imagemax, &th);
				break;

			case 0xE2:	// APP2 カラープロファイル
				ThCatString(&th, T("*colorprofile\r\n"));
				break;

//			case 0xEC:	// ? Ducky

			case 0xEE: {	// APP14 Adobe
				const TCHAR *Tr;
				switch ( *(p + 15) ){
					case 1:
						Tr = T("*color adobe YCbCr\r\n");
						break;
					case 2:
						Tr = T("*color adobe YCCK(CMYK)\r\n");
						break;
					default:
						Tr = T("*color adobe CMYK/RGB\r\n");
						break;
				}
				ThCatString(&th, Tr);
				break;
			}

			case 0xFE:	// COM Comment
				ThCatString(&th, T("*comment\r\n"));
				break;

			default:
				if ( (*(p + 1) < 0xc0) || (*(p + 1) > 0xed) ){
					p = imagemax;
					break;	// 異常コード
				}
//				wsprintf(buf, "*Unknown: %d\r\n", *(p+1));
//				ThCatString(&th, buf);
				break;
		}
		p += 2 + BEWORDPTR(p + 2);
	}
	result->info = (TCHAR *)th.bottom;
	return NO_ERROR;
}
// .ico / .cur
#pragma argsused
ERRORCODE vfd_Icon(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname);UnUsedParam(size);UnUsedParam(header);UnUsedParam(result);

	if ( *(DWORD *)(image + 0x12) != (DWORD)(*(WORD *)(image + 4) * 0x10 + 6) ){
		return ERROR_NO_DATA_DETECTED;
	}
	return NO_ERROR;
}

#pragma argsused
ERRORCODE vfd_GIF(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	ThSTRUCT th;
	TCHAR buf[VFPS];
	BYTE *p;
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(size);

	if ( result == NULL ) return NO_ERROR;
	if ( !(result->flags & VFSFT_INFO) ) return NO_ERROR;

	ThInit(&th);
	p = (BYTE *)(header + 6);

	wsprintf(buf, T("*Size:%dx%d"), *(WORD *)p, *(WORD *)(p+2));
	ThCatString(&th, buf);
	result->info = (TCHAR *)th.bottom;
	return NO_ERROR;
}

#pragma argsused
ERRORCODE vfd_EMF(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname);UnUsedParam(size);UnUsedParam(header);UnUsedParam(result);

	if ( *(DWORD *)image != 1 ) return ERROR_NO_DATA_DETECTED;
	return NO_ERROR;
}

// ジャストシステム関係 -------------------------------------------------------
const VFD_HEADERS JustHeaders[] = {
	JustH(0, 4, "JXW.JEX",	NULL, ":JXW", "一太郎", "JSW", DT_scp),
	JustH(0, 4, "HANA.JEX",	NULL, ":HANA", "花子",  "JBH", DT_scp),
	JustH(0, 4, "SNS.JEX",	NULL, ":SNS", "三四郎", "SNS", DT_scp),
	NH   (0, 0, NULL,		NULL, ":JUST", "Justsystem app.", "", DT_scp)};

#pragma argsused
ERRORCODE vfd_JUST(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	const VFD_HEADERS *vdh;
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(size);

	for ( vdh = JustHeaders ; vdh->flags ; vdh++ ){
		if ( stricmp(header + 0x80, vdh->header) == 0 ) break;
	}
	SetFileType(result, vdh);
	return ERROR_MORE_DATA;
}
// 複合ドキュメント -----------------------------------------------------------
typedef struct {
	TCHAR *type;
	TCHAR *comment;
	TCHAR *ext;
	int dtype;
	BYTE clsid[16];
} DOCS;
#define DOCH(type, comment, ext, dtype) {T(type), T(comment), T(ext), dtype

#define WORD8NO 3
const DOCS DocsHeaders[] = {
	DOCH(":DOC2",		"MS-Word2.0", "DOC", DT_scp),
	  {0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":DOC5",		"MS-Word5", "DOC", DT_scp),
	  {0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":DOC95",		"MS-Word6/7(95)", "DOC", DT_word7),
	  {0x00, 0x09, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":DOC97",		"MS-Word97/98", "DOC", DT_word8),
	  {0x06, 0x09, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":XLS",		"MS-Excel", "XLS", DT_scp),
	  {0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":XLS95",		"MS-Excel95", "XLS", DT_scp),
	  {0x10, 0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":XLS97",		"MS-Excel97", "XLS", DT_scp),
	  {0x20, 0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":BND",		"MS-Office binder", "BND", DT_scp),
	  {0x00, 0x04, 0x85, 0x59, 0x64, 0x66, 0x1b, 0x10,
	   0xb2, 0x1c, 0x00, 0xaa, 0x00, 0x4b, 0xa9, 0x0b}},
	DOCH(":PNT95",		"MS-PowerPoint95", "PNT", DT_scp),
	  {0x70, 0xae, 0x7b, 0xea, 0x3b, 0xfb, 0xcd, 0x11,
	   0xa9, 0x03, 0x00, 0xaa, 0x00, 0x51, 0x0e, 0xa3}},
	DOCH(":PNT97",		"MS-PowerPoint97", "PNT", DT_scp),
	  {0x10, 0x8d, 0x81, 0x64, 0x9b, 0x4f, 0xcf, 0x11,
	   0x86, 0xea, 0x00, 0xaa, 0x00, 0xb9, 0x29, 0xe8}},
	DOCH(":PNTWIZ",		"MS-PowerPoint97 Wizard", "WIZ", DT_scp),
	  {0xf0, 0x46, 0x72, 0x81, 0x0a, 0x72, 0xcf, 0x11,
	   0x87, 0x18, 0x00, 0xaa, 0x00, 0x60, 0x26, 0x3b}},

	DOCH(":PUB",		"MS-Publisher", "PUB", DT_scp),
	  {0x01, 0x12, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":WORKS",		"MS-Works", "WPS", DT_scp),
	  {0xb2, 0x5a, 0xa4, 0x0e, 0x0a, 0x9e, 0xd1, 0x11,
	   0xa4, 0x07, 0x00, 0xc0, 0x4f, 0xb9, 0x32, 0xba}},
	DOCH(":MSGRA",		"MS-Graph", "GRA", DT_scp),
	  {0x03, 0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":VISIO",		"Visio", "VSD", DT_scp),
	  {0x11, 0x1a, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":VISIO",		"Visio", "VSD", DT_scp),
	  {0x12, 0x1a, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":VISIO",		"Visio", "VSD", DT_scp),
	  {0x13, 0x1a, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":MST",		"Windows Installer Transform", "MST", DT_scp),
	  {0x82, 0x10, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":MSI",		"Windows Installer Package", "MSI", DT_scp),
	  {0x84, 0x10, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":MSP",		"Windows Installer Patch", "MSP", DT_scp),
	  {0x86, 0x10, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}},
	DOCH(":SDW",		"StarWriter", "SDW", DT_scp),
	  {0xd1, 0xf9, 0x0c, 0xc2, 0xae, 0x85, 0xd1, 0x11,
	   0xaa, 0xb4, 0x00, 0x60, 0x97, 0xda, 0x56, 0x1a}},
	DOCH(":JXW7",		"一太郎7", "JFW", DT_scp),
	  {0x00, 0x70, 0x59, 0x78, 0x84, 0x1d, 0xcf, 0x11,
	   0x97, 0x13, 0x00, 0x20, 0xaf, 0xd8, 0x06, 0xf4}},
	DOCH(":JXW8",		"一太郎8/9", "JTD", DT_utf16L),
	  {0x01, 0x70, 0x59, 0x78, 0x84, 0x1d, 0xcf, 0x11,
	   0x97, 0x13, 0x00, 0x20, 0xaf, 0xd8, 0x06, 0xf4}},
	DOCH(":JHD",		"花子9", "JHD", DT_scp),
	  {0x60, 0x27, 0xce, 0x06, 0xa4, 0x93, 0xd1, 0x11,
	   0x90, 0x79, 0x00, 0x80, 0x5f, 0x1d, 0x74, 0x56}},
	{NULL,			NULL, NULL, 0,
	  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
};

// ファイルプロパティAPIが無いときは、機能停止させる
#ifdef PRSPEC_PROPID
IID T_FMTID_SummaryInformation = {0xf29f85e0L, 0x4ff9, 0x1068,{0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9}};

const TCHAR *propidlist[] =
	{T("?"),	T("?"),		T("Title\t\t"),		T("Subject\t\t"),
	T("Author\t\t"), T("Keywords\t"), T("Comments\t"), T("Template\t"),
	T("LastAuthor\t"), T("Revision Number\t"), T("Edit Time\t"), T("Last printed\t"),
	T("Created\t\t"), T("Last Saved\t"), T("Page Count\t"), T("Word Count\t"),
	T("Char Count\t"), T("Thumpnail"), T("AppName\t\t"), T("Doc Security\t")
};

void DumpProcvariant(ThSTRUCT *th, PROPVARIANT *procv)
{
	TCHAR buf[64];
	FILETIME lft;
	SYSTEMTIME lst;

	switch (procv->vt){
		case VT_LPSTR:
			if ( strlen(procv->pszVal) > 144 ){
				char bufa[144 + 16];

				memcpy(bufa, procv->pszVal, 144);
				strcpy(bufa + 144, "...");
				ThCatStringTA(th, bufa);
			}else{
				ThCatStringTA(th, procv->pszVal);
			}
			break;
		case VT_BSTR:
			ThCatStringTW(th, procv->bstrVal);
			break;
		case VT_I4:
			wsprintf(buf, T("%d"), procv->lVal);
			ThCatString(th, buf);
			break;
		case VT_FILETIME:
			FileTimeToLocalFileTime(&procv->filetime, &lft);
			FileTimeToSystemTime(&lft, &lst);
			wsprintf(buf, T("%04d-%02d-%02d %02d:%02d:%02d"),
				lst.wYear, lst.wMonth, lst.wDay,
				lst.wHour, lst.wMinute, lst.wSecond);
			ThCatString(th, buf);
			break;
	}
}
#ifndef STGOPTIONS_VERSION
#if _WIN32_WINNT == 0x500
#define STGOPTIONS_VERSION 1
#elif _WIN32_WINNT > 0x500
#define STGOPTIONS_VERSION 2
#else
#define STGOPTIONS_VERSION 0
#endif

#ifndef WINEGCC
typedef struct tagSTGOPTIONS
{
	USHORT usVersion;
	USHORT reserved;
	ULONG ulSectorSize;
#if STGOPTIONS_VERSION >= 2
	const WCHAR *pwcsTemplateFile;
#endif
} STGOPTIONS;
#endif
#endif

#pragma argsused
void vfd_COMinfo(const TCHAR *fname, const char *image, const char *imagemax, VFSFILETYPE *result)
{
	ThSTRUCT th;
	int cnt = 4;
	HANDLE hOle32;
	DefineWinAPI(HRESULT, StgOpenStorageEx, (const WCHAR *pwcsName, DWORD grfMode, DWORD stgfmt, DWORD grfAttrs, STGOPTIONS *pStgOptions, void *reserved, REFIID riid, void **ppObjectOpen));
	DefineWinAPI(HRESULT, PropVariantClear, (PROPVARIANT *pvar));
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(result);

	ThInit(&th);
	if ( image != NULL ){
		while ( (image + 0x80) <= imagemax ){
			if ( *(const WORD *)image == '\0' ) break;
			ThCatString(&th, WildCard_All);
			ThCatStringTW(&th, (WCHAR *)image);
			ThCatString(&th, T("\r\n"));
			image += 0x80;
			cnt--;
			if ( !cnt ) break;
		}
	}
	hOle32 = GetModuleHandle(T("OLE32.DLL"));

	GETDLLPROC(hOle32, PropVariantClear);
	if ( DPropVariantClear == NULL ) return;	// Win95 では存在しない
	GETDLLPROC(hOle32, StgOpenStorageEx);

	{
		IStorage *pStorage = NULL;
		IPropertySetStorage *pProperty;
		IPropertyStorage *pPStorage = NULL;
		IEnumSTATPROPSTG *pEnumProp;
		STATPROPSTG pstg;
		ULONG fetched;
		PROPSPEC procs;
		PROPVARIANT procv;

#ifdef UNICODE
	#define FNAMEW fname
#else
		WCHAR FNAMEW[VFPS];
		AnsiToUnicode(fname, FNAMEW, VFPS);
#endif
		// IPropertySetStorage を取得
		if ( DStgOpenStorageEx != NULL ){
			if ( FAILED(DStgOpenStorageEx(FNAMEW,
					STGM_READ | STGM_SHARE_EXCLUSIVE, 4 /*STGFMT_ANY*/, 0, NULL,
					NULL, &XIID_IPropertySetStorage, (void **)&pProperty)) ){
				pProperty = NULL;
			}
		}else{
			pProperty = NULL;
			if ( SUCCEEDED(StgOpenStorage(FNAMEW, NULL,
					STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &pStorage)) ){
				pStorage->lpVtbl->QueryInterface(pStorage,
						&XIID_IPropertySetStorage, (void **)&pProperty);
			}
		}
		if ( pProperty != NULL ){
			if ( SUCCEEDED(pProperty->lpVtbl->Open(pProperty, &T_FMTID_SummaryInformation, STGM_READ | STGM_SHARE_EXCLUSIVE, &pPStorage)) ){
				if ( SUCCEEDED(pPStorage->lpVtbl->Enum(pPStorage, &pEnumProp)) ){
					while ( pEnumProp->lpVtbl->Next(pEnumProp, 1, &pstg, &fetched) == S_OK ){
						if ( fetched == 0 ) break;
						memset(&procs, 0, sizeof(procs));
						procs.ulKind = PRSPEC_PROPID;
						procs.propid = pstg.propid;
						if ( SUCCEEDED(pPStorage->lpVtbl->ReadMultiple(pPStorage, 1, &procs, &procv)) ){
							if ( procs.ulKind == PRSPEC_LPWSTR ){
								ThCatStringTW(&th, pstg.lpwstrName);
							}else{
								if ( pstg.propid < 0x14 ){
									ThCatString(&th, propidlist[procs.propid]);
								}else{
									ThCatString(&th, T("Unknown ID"));
								}
							}
							ThCatString(&th, T(" : "));
							DumpProcvariant(&th, &procv);
							ThCatString(&th, T("\r\n"));
							DPropVariantClear(&procv);
						}
					}
					pEnumProp->lpVtbl->Release(pEnumProp);
				}
				pPStorage->lpVtbl->Release(pPStorage);
			}
			pProperty->lpVtbl->Release(pProperty);
		}
		if ( pStorage != NULL ) pStorage->lpVtbl->Release(pStorage);
#if 0
		if ( SUCCEEDED(StgOpenStorage(FNAMEW, NULL,
					STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &pStorage)) ){
			IEnumSTATSTG *pEnumElement;

			if ( SUCCEEDED(pStorage->lpVtbl->EnumElements(pStorage, 0, NULL, 0, &pEnumElement)) ){
				TCHAR buf[0x200];

				for ( ;; ){
					STATSTG ss;
					ULONG count;

					if ( FAILED(pEnumElement->lpVtbl->Next(pEnumElement, 1, &ss, &count)) || (count == 0 ) ){
						break;
					}
					ThCatString(&th, WildCard_All);
					ThCatStringTW(&th, ss.pwcsName);
					CoTaskMemFree(ss.pwcsName);
					wsprintf(buf, T("\r\n  Type:%d\r\n  Size:%d\r\n  Time:\r\n"), ss.type, ss.cbSize);
					ThCatString(&th, buf);
				}
				pEnumElement->lpVtbl->Release(pEnumElement);
			}
			pStorage->lpVtbl->Release(pStorage);
		}
#endif
	}
	result->info = (TCHAR *)th.bottom;
}
#endif


void SetComType(VFSFILETYPE *result, const DOCS *comp)
{
	result->dtype = comp->dtype;
	tstrcpy(result->type, comp->type);
	if ( result->flags & VFSFT_TYPETEXT ){
		tstrcpy(result->typetext, comp->comment);
	}
	if ( result->flags & VFSFT_EXT){
		tstrcpy(result->ext, comp->ext);
	}
}

ERRORCODE vfd_COM(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	const DOCS *comp;
	const char *rootentry;
	DWORD offset;

									// "Root Entry"を得る
	offset = (*(DWORD *)(header + 0x30) + 1) * 0x200;
	if ( (offset + 200) >= size ){
		rootentry = NULL;
	}else{
		rootentry = header + offset;
		if ( (*(DWORD *)rootentry != 0x6f0052) ||
			 (*(DWORD *)(rootentry + 4) != 0x74006f) ){
			return ERROR_NO_DATA_DETECTED;
		}
	}
	#ifdef PRSPEC_PROPID
	if ( result->flags & VFSFT_INFO ){
		vfd_COMinfo(fname, rootentry, image + size, result);
	}
	#endif
	if ( rootentry != NULL ){
		rootentry += 0x50;
									// CLD ID を検索
		for ( comp = DocsHeaders ; comp->type ; comp++ ){
			if ( !memcmp(rootentry, comp->clsid, 16) ){
				SetComType(result, comp);
				return ERROR_MORE_DATA;
			}
		};
	}else{
		if ( size > 0x400 ){
			if ( *(DWORD *)(image + 0x200) == 0x00c1a5ec ){
				SetComType(result, &DocsHeaders[WORD8NO]);
				return ERROR_MORE_DATA;
			}
		}
	}
	if ( result->flags & VFSFT_STRICT ) return ERROR_FILEMARK_DETECTED;
	return NO_ERROR;
}
// 実行ファイル ---------------------------------------------------------------
struct EXEEXTTYPESTURCT {
	const TCHAR *exttype, *uses, *ext;
} ExeExtTypes[] = {
	{T(":CPL"), T("Control Panel"), T("cpl")},
	{T(":DLL"), T("Dynamic Link Library"), T("dll")},
	{T(":DRV"), T("Device driver"), T("DRV")},
	{T(":IME"), T("Input Method Editor"), T("ime")},
	{T(":OCX"), T("OLE Custom Controls"), T("ocx")},
	{T(":SCR"), T("Screen saver"), T("scr")},
	{T(":SYS"), T("Device driver"), T("sys")},
	{T(":VXD"), T("Virtual device driver"), T("VXD")},
	{T(":EXE"), T("progarm"), T("exe")},
	{NULL, NULL, NULL}
};

const TCHAR X86STR[] = T("x86");
const TCHAR X64STR[] = T("x64");
const TCHAR IA64STR[] = T("IA64");

#define MH(id, name) {id, T(name)}
struct MPUTABLESTRUCT {
	WORD id;
	const TCHAR *name;
} MpuTable[] = {
{IMAGE_FILE_MACHINE_I386, X86STR},
#ifdef IMAGE_FILE_MACHINE_R3000
MH(IMAGE_FILE_MACHINE_R3000, "R3000"),
MH(IMAGE_FILE_MACHINE_R4000, "R4000"),
MH(IMAGE_FILE_MACHINE_R10000, "R10000"),
MH(0x169, "MIPS WCE"),
MH(IMAGE_FILE_MACHINE_ALPHA, "Alpha"),
#endif
MH(0x1a2, "SH3"),
MH(0x1a3, "SH3DSP"),
MH(0x1a4, "SH3E"),
MH(0x1a6, "SH4"),
MH(0x1a8, "SH5"),
MH(0x1c0, "ARM"),
MH(0x1c2, "ARM T/T2LE"),
MH(0x1c4, "ARM T2LE"),
MH(0x1d3, "AM33"),
MH(IMAGE_FILE_MACHINE_POWERPC, "PowerPC"),
MH(0x1f1, "PowerPC FP"),
{0x200, IA64STR},
MH(0x266, "MIPS16"),
MH(0x284, "ALPHA64"),
MH(0x366, "MIPS FPU"),
MH(0x466, "MIPF FPU16"),
MH(0x520, "Infineon"),
MH(0xcef, "CEF"),
MH(0xebc, "EFI"),
{0x8664, X64STR},
MH(0x9041, "M32R"),
MH(0xaa64, "ARM64 LE"),
MH(0xc0ee, "CEE"),
MH(0, "Unknown MPU")
};

const TCHAR EXE32USTR[] = T(":EXE32U");
const TCHAR EXE32UCSTR[] = T(":EXE32UC");

struct SYSTEMTABLESTRUCT {
	WORD id;
	const TCHAR *name;
	const TCHAR *exttype;
} SystemTable[] = {
{IMAGE_SUBSYSTEM_NATIVE,	T("Native"), EXE32USTR},
{IMAGE_SUBSYSTEM_WINDOWS_GUI, T("GUI"), EXE32USTR},
{IMAGE_SUBSYSTEM_WINDOWS_CUI, T("CUI"), EXE32UCSTR},
//{4, "回復コンソール用？"}
{IMAGE_SUBSYSTEM_OS2_CUI,	T("OS/2"), T(":EXE32OS2")},
//{6, "？"}
{IMAGE_SUBSYSTEM_POSIX_CUI,	T("POSIX"), T(":EXEPOSIX")},
{8,		T("Win9xDriver"), T(":VXD")},
{9,		T("WinCE"), T(":EXECE")},
{10,	T("EFI"), T(":EXEEFI")},
{11,	T("EFI Boot Service"), T(":EXEEFI")},
{12,	T("EFI runtime"), T(":EXEEFI")},
{13,	T("EFI ROM"), T(":EXEEFI")},
{14,	T("XBOX"), T(":EXEXBOX")},
{0, T("Unknown"), T(":EXE")}
};

const TCHAR *exeguitable[] = {
	T(":EXE32"), T(":EXEIA64"), T(":EXEX64"), EXE32USTR
};
const TCHAR *execontable[] = {
	T(":EXE32C"), T(":EXEIA64C"), T(":EXEX64C"), EXE32UCSTR
};
const TCHAR *CheckExeMPU(const TCHAR *idtable[], const TCHAR *id)
{
	if ( id == X86STR ) return idtable[0];
	if ( id == IA64STR ) return idtable[1];
	if ( id == X64STR ) return idtable[2];
	return idtable[3];
}
#ifndef IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR 14
#endif

#define FirstImageSectionHeader(xhdr) ((IMAGE_SECTION_HEADER *)((BYTE *)xhdr + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + xhdr->FileHeader.SizeOfOptionalHeader))

struct VERSIONTABLENAMES {
	const TCHAR *name;
	const TCHAR *ID;
} VerTnames[] ={
	{T("FileDescription"), T("Description")}, // ファイルの説明
	{T("FileVersion"), T("Version\t")}, // ファイルバージョン
	{T("CompanyName"), T("CompanyName")}, // 会社名
	{T("LegalTrademarks"), T("LegalTrademarks")}, // トレンドマーク
	{T("Comments"), T("Comment\t")},
	{T("InternalName"), T("InternalName")}, // ファイル名
	{T("LegalCopyright"), T("LegalCopyright")}, // コピーライト
	{T("OriginalFilename"), T("OriginalName")}, // オリジナルファイル名
	{T("ProductName"), T("ProductName")}, // プロダクト名
	{T("ProductVersion"), T("ProductVersion")}, // プロダクトバージョン
	{T("PrivateBuild"), T("PrivateBuild")}, // プライベートビルドバージョン
	{T("SpecialBuild"), T("SpecialBuild")}, // スペシャルビルドバージョン
	{NULL, NULL}
};

DefineWinAPI(BOOL, GetFileVersionInfo, (LPCTSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData));
DefineWinAPI(DWORD, GetFileVersionInfoSize, (LPCTSTR lptstrFilename, LPDWORD lpdwHandle));
DefineWinAPI(BOOL, VerQueryValue, (LPCVOID pBlock, LPCTSTR lpSubBlock, LPVOID *lplpBuffer, PUINT puLen));

LOADWINAPISTRUCT VERSIONDLL[] = {
	LOADWINAPI1T(GetFileVersionInfo),
	LOADWINAPI1T(GetFileVersionInfoSize),
	LOADWINAPI1T(VerQueryValue),
	{NULL, NULL}
};

void ThWS(char *res, ThSTRUCT *th, TCHAR *base, struct VERSIONTABLENAMES *Vt)
{
	TCHAR namebuf[80];
	UINT size;
	TCHAR buf[0x8000], *p;

	wsprintf(namebuf, base, Vt->name);
	if ( IsTrue(DVerQueryValue(res, namebuf, (LPVOID *)&p, &size)) ){
		if( (size > 0) && (sizeof(buf) > size) ){
			wsprintf(buf, T("%s\t:%s")T(NL), Vt->ID, p);
			ThCatString(th, buf);
		}
	}
}

#ifndef IMAGE_NT_OPTIONAL_HDR64_MAGIC
#define IMAGE_NT_HEADERS32 IMAGE_NT_HEADERS
#endif

void GetExeVerInfo(const TCHAR *fname, VFSFILETYPE *result, IMAGE_NT_HEADERS32 *xhdr)
{
	TCHAR ibuf[MAX_PATH], base[80];
	char *verres;
	ThSTRUCT th;
	DWORD size;
	DWORD *verlang;
	DWORD i;
	HANDLE hVersionDLL;

	hVersionDLL = LoadWinAPI("VERSION.DLL", NULL, VERSIONDLL, LOADWINAPI_LOAD);
	if ( hVersionDLL == NULL ) return;

	ThInit(&th);

	size = DGetFileVersionInfoSize((TCHAR *)fname, NULL);
	if ( size != 0 ){
		verres = HeapAlloc(DLLheap, 0, size);
		if ( verres != NULL ){
			DGetFileVersionInfo((TCHAR *)fname, 0, size, verres);

			if ( IsTrue(DVerQueryValue(verres, T("\\VarFileInfo\\Translation"), (void *)&verlang, (UINT *)&size)) ){
				// 言語コードに対応した情報を出力する
				for ( i = size / sizeof(DWORD) ; i ; i--, verlang++ ){
					struct VERSIONTABLENAMES *vt;

					wsprintf(base, T("\\StringFileInfo\\%04x%04x\\%%s"),
							LOWORD(*verlang), HIWORD(*verlang));
					for ( vt = VerTnames ; vt->name ; vt++ ){
						ThWS(verres, &th, base, vt);
					}
				}
			}
			HeapFree(DLLheap, 0, verres);
		}
	}
	wsprintf(ibuf, T("Image base\t:%x"), xhdr->OptionalHeader.ImageBase);
	ThCatString(&th, ibuf);
	result->info = (TCHAR *)th.bottom;
	FreeLibrary(hVersionDLL);
}

ERRORCODE vfd_EXE(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	TCHAR buf[MAX_PATH];
//	TCHAR option[MAX_PATH];

	const TCHAR *OStype = T("DOS");	// OSの種類
	const TCHAR *OStarget = NilStr;	// 対象MPU
	const TCHAR *OStarget2 = T("16bit");	// 種類
	const TCHAR *OSuse = T("progarm");	// 実行ファイルの用途
	const TCHAR *OSexttype = T(":EXEDOS");		// 種類
	const TCHAR *OSext = T("EXE");		// 拡張子

	result->dtype = 2;
//	option[0] = '\0';
								// 拡張子で種別を決定 -------------------------
	{
		struct EXEEXTTYPESTURCT *eet = ExeExtTypes;
		const TCHAR *p;

		p = VFSFindLastEntry(fname);
		p += FindExtSeparator(p);
		if (*p == '.') p++;
		tstrcpy(buf, p);
		tstrupr(buf);
		for ( ; eet->ext ; eet++ ){
			if ( !tstrcmp(eet->ext, buf) ){
				OSext = eet->ext;
				OSexttype = eet->exttype;
				OSuse = eet->uses;
				break;
			}
		}
	}
								// 対象OSで種別を決定 -------------------------
	if ( (size > 0x400) && !memcmp(image + 0x288, "_winzip_", 8) ){
		if ( result->flags & VFSFT_TYPE ) tstrcpy(result->type, T(":PKZIP"));
		if ( result->flags & VFSFT_TYPETEXT ){
			tstrcpy(result->typetext, T("PKZIP archive"));
		}
		if ( result->flags & VFSFT_EXT ) tstrcpy(result->ext, T("zip"));
		return ERROR_MORE_DATA;
	}
	{
		DWORD offset;

		offset = ((IMAGE_DOS_HEADER *)header)->e_lfanew; // 拡張ヘッダへのoffset
		if ( offset < (size - 0x180) ){
			IMAGE_NT_HEADERS32 *xhdr;

			xhdr = (IMAGE_NT_HEADERS32 *)(header + offset);
			if ( xhdr->Signature == IMAGE_NT_SIGNATURE ){ //PE header(Win32/64)
				struct MPUTABLESTRUCT *mts;
				struct SYSTEMTABLESTRUCT *sts;
				WORD mid;

				wsprintf(buf, T("Windows %d.%d"),
					xhdr->OptionalHeader.MajorSubsystemVersion,
					xhdr->OptionalHeader.MinorSubsystemVersion);
				OStype = buf;

				// 対象MPUを決定
				mid = xhdr->FileHeader.Machine;	// target MPU
				for ( mts = MpuTable ; mts->id ; mts++ ){
					if ( mid == mts->id ) break;
				}
				OStarget = mts->name;

				// 対象を決定
				mid = xhdr->OptionalHeader.Subsystem;
				for ( sts = SystemTable ; sts->id ; sts++ ){
					if ( mid == sts->id ) break;
				}
				OStarget2 = sts->name;

				if ( result->flags & VFSFT_INFO ){
					GetExeVerInfo(fname, result, xhdr);
				}
				if ( xhdr->FileHeader.Characteristics & IMAGE_FILE_DLL ){
					OSuse = T("Dynamic Link Library");
					OStarget2 = NilStr;
					OSexttype = T(":DLL");
					OSext = T("DLL");
				}else{
					DWORD vadr;
					// CLR/.NET コードの判別
					if ( xhdr->OptionalHeader.Magic == 0x20b ){	// 64bit
						// 16 で位置補正をしている
						vadr = ((IMAGE_NT_HEADERS32 *)(image + offset + 16))->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress;
					}else{	// 0x10b
						vadr = xhdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress;
					}
					if ( vadr != 0 ){
						// IMAGE_COR20_HEADER の位置を求める
						vadr = FirstImageSectionHeader(xhdr)->PointerToRawData + (vadr - xhdr->OptionalHeader.BaseOfCode);
						if ( vadr < size - 0x40 ){
							wsprintf(buf, T("CLR %d.%d"),
								*(WORD *)(image + vadr + 4),
								*(WORD *)(image + vadr + 6));
						}else{
							OStype = T("CLR");
						}
						// IMAGE_DIRECTORY_ENTRY_SECURITY 証明書
						OSexttype = T(":EXECLR");
					}else{
						OSexttype = sts->exttype;
						if ( OSexttype == EXE32USTR ){
							OSexttype = CheckExeMPU(exeguitable, OStarget);
						}else if ( OSexttype == EXE32UCSTR ){
							OSexttype = CheckExeMPU(execontable, OStarget);
						}
					}
				}
				if ( result->flags & VFSFT_TYPETEXT ){
					IMAGE_SECTION_HEADER *LastSection;
					DWORD secsize;
					HANDLE hFF;
					WIN32_FIND_DATA ff;
//					TCHAR *dst;

					LastSection = IMAGE_FIRST_SECTION(xhdr) + xhdr->FileHeader.NumberOfSections - 1;
					secsize = LastSection->PointerToRawData + LastSection->SizeOfRawData;
					hFF = FindFirstFileL(fname, &ff);
					if ( hFF != INVALID_HANDLE_VALUE ){
						FindClose(hFF);
						if ( ((ff.nFileSizeLow > secsize) || ff.nFileSizeHigh) &&
							// EXEが1000h単位に揃えられているとき
							 (ff.nFileSizeLow != ((secsize + 0xfff) & 0xfffff000)) &&
							// EXEが200h単位に揃えられているとき
							 (ff.nFileSizeLow != ((secsize + 0x1ff) & 0xfffffe00))
						){
							TCHAR *ap;

							ap = buf + tstrlen(buf) + 1;
							wsprintf(ap, T("%s with Archive 0x%x"), OSuse, secsize);
							OSuse = ap;
						}
					}
					/*
					dst = option;
					*dst++ = ' ';
#define xIMAGE_FILE_LARGE_ADDRESS_AWARE       0x0020
					if ( xhdr->FileHeader.Characteristics & xIMAGE_FILE_LARGE_ADDRESS_AWARE ){
						dst += wsprintf(dst, T(">2G "));
					}
					*/
				}
			}else if ( ((IMAGE_OS2_HEADER *)xhdr)->ne_magic ==
								IMAGE_OS2_SIGNATURE ){	// NE header(Win/OS)
				if ( ((IMAGE_OS2_HEADER *)xhdr)->ne_exetyp == 2 ){	// Win16
					OStype = T("Windows");
					OStarget = T("16bit");
					OStarget2 = T("GUI");
					OSexttype = T(":EXE16");
				}else if (((IMAGE_OS2_HEADER *)xhdr)->ne_exetyp == 1){//OS/2, 16
					OStype = T("OS/2");
					OStarget = T("16bit");
					OStarget2 = NilStr;
					OSexttype = T(":EXE16OS2");
				}else{	// 0:Unknown, 3:DOS4
					OStarget = T("Unknown NE header");
				}
			}else if ( ((IMAGE_OS2_HEADER *)xhdr)->ne_magic == 0x584c ){
															// LX header(OS2)
				OStype = T("OS/2");
				OStarget = T("32bit");
				OStarget2 = NilStr;
				OSexttype = T(":EXE32OS2");
			}else if ( ((IMAGE_VXD_HEADER *)xhdr)->e32_magic ==
					IMAGE_VXD_SIGNATURE ){ // LE header(Win/OS)
				OStype = T("Windows");
				OSuse = T("Driver");
				OSexttype = T(":VXD");
			}
		}
	}
	if ( result->flags & VFSFT_TYPE ) tstrcpy(result->type, OSexttype);
	if ( result->flags & VFSFT_TYPETEXT ){
		wsprintf(result->typetext, T("%s %s %s %s"),
				OStype, OStarget, OStarget2, OSuse);
	}
	if ( result->flags & VFSFT_EXT ) tstrcpy(result->ext, OSext);
	return ERROR_MORE_DATA;
}

typedef WORD Elf32_Half;
typedef WORD Elf32_Off;
typedef DWORD Elf32_Word;
typedef DWORD Elf32_Addr;
typedef struct
{
	struct {
		char EI_MAG[4];
		BYTE EI_CLASS;
		BYTE EI_DATA;
		BYTE EI_VERSION;
		BYTE EI_PAD;
		char padding[8];
	} e_ident;
	Elf32_Half e_type;
	Elf32_Half e_machine;
	Elf32_Word e_version;
	Elf32_Addr e_entry;
	Elf32_Off  e_phoff;
	Elf32_Off  e_shoff;
	Elf32_Word e_flags;
	Elf32_Half e_ehsize;
	Elf32_Half e_phentsize;
	Elf32_Half e_phnum;
	Elf32_Half e_shentsize;
	Elf32_Half e_shnum;
	Elf32_Half e_shstrndx;
} Elf32_Ehdr;

const TCHAR *Elf32_ei_class[] =
{
	T("?bit"),
	T("32bit"),
	T("64bit"),
};

const TCHAR *Elf32_e_type[] =
{
	T("?"),
	T("rel"),
	T("exec"),
	T("dyn"),
	T("core")
};

const TCHAR *Elf32_e_machine[] =
{
	T("?"),
	T("WE 32100"),
	T("SPARC"),
	T("Intel x86"),
	T("68000"),
	T("88000"),
	T("?"),
	T("Intel 860"),
	T("MIPS RS3000"),
};

int GetElfValue(DWORD value, BYTE EI_DATA, DWORD valuemax)
{
	if ( EI_DATA == 2 /* ELFDATA2MSB */ ){
		value = ((value & 0xff00) >> 8) | ((value & 0xff) << 8);
	}
	if ( value > valuemax ) value = 0;
	return value;
}

#pragma argsused
ERRORCODE vfd_ELF(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname);UnUsedParam(header);

	if ( size <= sizeof(Elf32_Ehdr) ) return ERROR_NO_DATA_DETECTED;
	if ( result->flags & VFSFT_TYPETEXT ){
		const TCHAR *OStype, *OStarget, *OStarget2;
		Elf32_Ehdr *ehdr = (Elf32_Ehdr *)image;

		OStarget = Elf32_e_machine[GetElfValue(ehdr->e_machine, ehdr->e_ident.EI_DATA, 8)];
		OStarget2 = Elf32_ei_class[GetElfValue(ehdr->e_ident.EI_CLASS, 0, 2)];
		OStype = Elf32_e_type[GetElfValue(ehdr->e_type, ehdr->e_ident.EI_DATA, 8)];
		result->dtype = 2;
		tstrcpy(result->type, T(":ELF"));
		wsprintf(result->typetext, T("Unix/Linux %s %s %s"),
				OStarget, OStarget2, OStype);
		if ( result->flags & VFSFT_EXT ) result->ext[0] = '\0';
		return ERROR_MORE_DATA;
	}
	return NO_ERROR;
}

// RIFF -----------------------------------------------------------------------
const VFD_HEADERS RIFFheaders[] = {
	AH(0, 4, "ACON", NULL, ":ANI",	"Animated cursors", "ani", DT_scp),
	AH(0, 4, "AVI ", NULL, ":AVI",	"Video for windows Movie", "avi", DT_scp),
	AH(0, 4, "RMID", NULL, ":SMF",	"RIFF Standard MIDI File", "RMI", DT_scp),
	AH(0, 4, "WAVE", NULL, ":WAV",	"Wave file", "wav", DT_scp),
	AH(0, 4, "WEBP", NULL, ":WEBP",	"WebP image", "webp", DT_scp),
	NH(0, 0, NULL, NULL,	":RIFF", "RIFF", "riff", DT_scp)};

#pragma argsused
ERRORCODE vfd_RIFF(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	const VFD_HEADERS *vdh;
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(size);

	for ( vdh = RIFFheaders ; vdh->flags ; vdh++ ){
		if (!memcmp(header + 8, vdh->header, 4)) break;
	}
	SetFileType(result, vdh);
	return ERROR_MORE_DATA;
}

#pragma argsused
ERRORCODE vfd_ID3(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	UnUsedParam(fname);UnUsedParam(image);UnUsedParam(size);UnUsedParam(result);

	if ( (*(header + 3) >= 2) && (*(header + 3) <= 6) && (*(header + 4) == 0)){
		return NO_ERROR;
	}
	return ERROR_NO_DATA_DETECTED;
}

const VFD_HEADERS QTheaders[] = {
	AH(0, 4, "avif", NULL,	":AVIF", "AV1 Image File Format", "avif", DT_scp),
	AH(0, 4, "3g2a", NULL,	":3GP",	"MPEG4 3GPP2 movie", "3gp", DT_scp),
	AH(0, 4, "3g2b", NULL,	":3GP",	"MPEG4 3GPP2 movie", "3gp", DT_scp),
	AH(0, 4, "3g2c", NULL,	":3GP",	"MPEG4 3GPP2 movie", "3gp", DT_scp),
	AH(0, 4, "3gp4", NULL,	":3GP",	"MPEG4 3GPP movie", "3gp", DT_scp),
	AH(0, 4, "3gp5", NULL,	":3GP",	"MPEG4 3GPP movie", "3gp", DT_scp),
	AH(0, 4, "3gp6", NULL,	":3GP",	"MPEG4 3GPP movie", "3gp", DT_scp),
	AH(0, 4, "3gp7", NULL,	":3GP",	"MPEG4 3GPP movie", "3gp", DT_scp),
	AH(0, 4, "heic", NULL,	":HEIC", "HEIC image", "heic", DT_scp),
	AH(0, 4, "isom", NULL,	":MP4",	"MPEG4 movie", "mp4", DT_scp),
	AH(0, 4, "kddi", NULL,	":3GP",	"MPEG4 3GPP movie(KDDI)", "3gp", DT_scp),
	AH(0, 4, "mif1", NULL,	":HEIF", "HEIF image", "heif", DT_scp),
	AH(0, 4, "mmp4", NULL,	":3GP",	"MPEG4 3GPP movie(NTT docomo)", "3gp", DT_scp),
	AH(0, 4, "mp41", NULL,	":MP4",	"MP4 v1 movie", "mp4", DT_scp),
	AH(0, 4, "mp42", NULL,	":MP4",	"MP4 v2 movie", "mp4", DT_scp),
	NH(0, 0, NULL, NULL,	":QT", "QuickTime/MPEG4 movie/audio", "qt", DT_scp)
};

#pragma argsused
ERRORCODE vfd_qt(const TCHAR *fname, const char *image, DWORD size, const char *header, VFSFILETYPE *result)
{
	const VFD_HEADERS *vdh;
	UnUsedParam(fname);UnUsedParam(size);UnUsedParam(result);

	if ( *(WORD *)image != 0 ) return ERROR_NO_DATA_DETECTED;
	for ( vdh = QTheaders ; vdh->flags ; vdh++ ){
		if ( memcmp(header + 4, vdh->header, 4) == 0 ) break;
	}
	SetFileType(result, vdh);
	return ERROR_MORE_DATA;
}

// 一般 -----------------------------------------------------------------------
const VFD_HEADERS RootHeaders[] = {
										// Image ------------------------------
AH(0, 2, "BM", vfd_BMP,		":BMP",		"BITMAP image", "bmp", DT_scp),
AH(0, 4, "GIF8", vfd_GIF,	":GIF",		"GIF image", "gif", DT_scp),
AH(0, 12, "\x89PNG\x0d\x0a\x1a\x0a\0\0\0\x0d", vfd_PNG,	":PNG",		"PNG image", "png", DT_scp),
AH(0, 11, "\x8aMNG\x0d\x0a\x1a\x0a\0\0", NULL,	":MNG",		"MNG animation", "mng", DT_scp),
AH(0, 11, "\x8bJNG\x0d\x0a\x1a\x0a\0\0", NULL,	":JNG",		"JNG image", "jng", DT_scp),
AH(0, 4, "II*", NULL,		":TIFF",	"TIFF image", "tif", DT_scp),
AH(0, 4, "II+", NULL,		":BTIFF",	"Big TIFF image", "tif", DT_scp),
AH(0, 4, "MM\0*", NULL,		":TIFF",	"TIFF image", "tif", DT_scp),
AH(0, 4, "MM\0+", NULL,		":BTIFF",	"Big TIFF image", "tif", DT_scp),
AH(0, 4, "II\xbc\1", NULL,	":WDP",		"JPEG XR image", "wdp", DT_scp),
AH(0, 2, "\xff\xd8", vfd_JPEG, ":JPEG",	"JPEG image", "jpg", DT_scp),
AH(4, 8, "jP  \r\n\x87\xa", NULL, ":JP2",	"JPEG 2000 image", "jp2", DT_scp),
AH(0, 4, "\xff\x4f\xff\x51", NULL, ":J2K",	"JPEG 2000 stream image", "j2k", DT_scp),
AH(0, 6, "8BPS\0\1", NULL,	":PSD",		"PhotoShop", "psd", DT_scp),
AH(0, 6, "8BPS\0\2", NULL,	":PSB",		"PhotoShop Big Document", "PSB", DT_scp),
AH(8, 4, "AMFF", NULL,		":AMFF",	"AmigaMetaFile", "AMFF", DT_scp),
AH(0, 11, "%!PS-Adobe-", NULL, ":PS",	"PostScriptFile", "ps", DT_scp),
AH(1, 7, "\0\2\0\0\0\0", NULL, ":TGA",	"TGA image", "tga", DT_scp), //iconとの誤判定
AH(0, 4, "\0\0\1", vfd_Icon, ":ICON",	"Icon image", "ico", DT_scp),
AH(0, 4, "\0\0\2", vfd_Icon, ":CUR",	"Cursor image", "cur", DT_scp),
//AH(0, 4, "PICT", NULL,			":PICT",	"Macintosh Pictures", "PCT", DT_scp),
AH(0x20a, 4, "\0\x11\x02\xff", NULL,	":PICT",	"Macintosh Pictures", "PIC", DT_scp),
AH(0, 3, "\xa\0\1", NULL,	":PCX",		"PC Paintbrush 2.5 image", "pcx", DT_scp),
AH(0, 3, "\xa\2\1", NULL,	":PCX",		"PC Paintbrush 2.8 image", "pcx", DT_scp),
AH(0, 3, "\xa\3\1", NULL,	":PCX",		"PC Paintbrush 2.8 image", "pcx", DT_scp),
AH(0, 3, "\xa\4\1", NULL,	":PCX",		"PC Paintbrush Win image", "pcx", DT_scp),
AH(0, 3, "\xa\5\1", NULL,	":PCX",		"PC Paintbrush 3.0 image", "pcx", DT_scp),
AH(0, 6, "MAKI02", NULL,	":MAG",		"MAG image", "MAG", DT_scp),
AH(0, 6, "Pixia", NULL,		":PIXIA",	"Pixia image", "pxa", DT_scp),
AH(0, 6, "VCLMTF", NULL,	":SVM",		"StarView MetaFile", "SVM", DT_scp),
AH(0, 5, "xof 0", NULL,		":X",		"DirectX file format", "x", DT_scp),
AH(0, 4, "\x1a\x45\xdf\xa3", vfd_EBML, ":EBML", "EBML document", "EBML", DT_scp),
										// Document ---------------------------
AH(0x39, 12, "<w:document ", NULL, ":DOCX",	"Word2007", "xml", DT_officex),

RH(0x800, 5, "<?xml", vfd_HTML,	":XML",		"XML", "xml", DT_xml),
RH(0x800, 5, "<HTML", vfd_HTML,	":HTML",	"HTML", "html", DT_html),
RH(0x800, 5, "<html", vfd_HTML,	":HTML",	"HTML", "html", DT_html),
RH(0x800, 14, "ype: text/html", vfd_HTML2, ":HTML", "HTML", "html", DT_html),
RH(0x800, 1, "<", vfd_HTML,	":HTML",	"HTML like", "htm", DT_html),
RH(0x2, 10, "<\0?\0x\0m\0l", NULL,	":XML",		"XML", "xml", DT_uhtml),
RH(0x2, 10, "<\0h\0t\0m\0l", NULL,	":HTML",	"HTML", "html", DT_uhtml),
RH(0x2, 2, "<", NULL,	":HTML",	"HTML like", "htm", DT_uhtml),
//RH(0x1000, 20, "g: quoted-printable\r", NULL, ":QUOTED", "E-Mail message", "eml", DT_mail),

AH(0, 5, "{\\rtf", NULL,	":RTF",		"Rich Text", "rtf", DT_rtf),
AH(0, 4, "%PDF", NULL,		":PDF",		"Portable Doc. File", "pdf", DT_scp),
AH(0, 7, "010010\0", NULL,	":JIS",		"JIS text", "JIS", DT_jis),
AH(0, 7, "010020\0", NULL,	":JIS",		"JIS text", "JIS", DT_jis),
AH(0, 7, "020020\0", NULL,	":JIS",		"JIS text", "JIS", DT_jis),
AH(0, 7, "030140\0", NULL,	":JIS",		"JIS text", "JIS", DT_jis),
AH(0, 4, "DHL1", NULL,		":JIS",		"JIS text", "JIS", DT_jis),
AH(0, 4, "DHL2", NULL,		":JIS",		"JIS text", "JIS", DT_jis),
AH(0, 3, "2\xbe", NULL,		":WRITE",	"MS-Write", "wri", DT_scp),
AH(0, 3, "1\xbe", NULL,		":WRITE",	"MS-Write", "wri", DT_scp),
AH(0, 4, "\x1d}\0\0", NULL,	":WS",		"WordStar", "WS", DT_scp),
AH(0, 4, "\xffWPC", NULL,	":WP",		"WordPerfect", "WPD", DT_scp),
AH(0, 2, "\xd0\xcf", vfd_COM, ":DOCS",	"複合ドキュメント", "", DT_scp),
AH(0, 2, "\xf1\x10", NULL,	":OA2",		"OASYS2(A)", "OA2", DT_oasys),
AH(0, 9, "VjCD0100\4", NULL, ":CDX",	"ChemDraw", "cdx", DT_scp),
AH(0, 8, "AC1004\0", NULL,	":ACAD",	"AutoCAD EX-II", "", DT_scp),
AH(0, 8, "AC1012\0", NULL,	":ACAD",	"AutoCAD R12", "", DT_scp),
AH(0, 8, "AC1013\0", NULL,	":ACAD",	"AutoCAD R13", "", DT_scp),
AH(0, 8, "AC1014\0", NULL,	":ACAD",	"AutoCAD R14", "", DT_scp),
AH(0, 8, "AC1015\0", NULL,	":ACAD",	"AutoCAD R15/2000", "", DT_scp),
AH(0, 8, "AC1018\0", NULL,	":ACAD",	"AutoCAD R18/2004", "", DT_scp),
AH(0, 8, "AC1021\0", NULL,	":ACAD",	"AutoCAD R21/2007", "", DT_scp),
AH(0, 8, "AC1024\0", NULL,	":ACAD",	"AutoCAD R24/2010", "", DT_scp),
AH(0, 8, "AC1027\0", NULL,	":ACAD",	"AutoCAD R27/2013", "", DT_scp),
AH(0, 4, "AC10", NULL,		":ACAD",	"AutoCAD", "", DT_scp),
AH(0, 6, "(DWF V", NULL,	":DWF",		"Drawing Web Format", "dwf", DT_scp),
AH(0, 8, "WordPro", NULL,	":LWP",		"LotusWordPro", "LWP", DT_scp),
AH(0, 6, "\x9\0\4\0\2", NULL, ":XLS",		"MS-Excel2", "xls", DT_scp),
AH(0, 4, "EP*\0", NULL,		":MDI",		"Microsoft Document Imaging image", "mdi", DT_scp),
AH(0, 8, "\x60\xe\x82\x1\x7\x80\x3", NULL,	":XDW",	"DocuWorks Document", "xdw", DT_scp),

AH(0, 11, "BEGIN:VCARD", NULL, ":VCF",		"vCard", "vcf", DT_text),
AH(0, 15, "BEGIN:VCALENDAR", NULL, ":VCS",	"vCalendar", "vcs", DT_text),
AH(0, 4, "\'\\\" ", NULL,	":MAN",		"Manual text", "", DT_scp),
AH(0, 4, ".TH ", NULL,		":MAN",		"Manual text", "", DT_scp),
										// Media ------------------------------
AH(0, 4, "MThd", NULL,		":SMF",		"Standerd Midi Format", "mid", DT_scp),
AH(0, 4, "RIFF", vfd_RIFF,	":RIFF",	"RIFF", "RIFF", DT_scp),
AH(0, 4, "RCM-", NULL,		":RCM",		"RECOMPOSER", "RCP", DT_scp),
AH(0, 4, "\0\0\1\xba", NULL, ":MPG",		"MPEG movie", "mpg", DT_scp),
AH(0, 4, "\0\0\1\xb3", NULL, ":MPG",		"MPEG movie", "mpg", DT_scp),
AH(0, 2, "\xff\xfb", NULL,	":MP3",		"MPEG1 Layer 3", "mp3", DT_scp),
AH(0, 2, "\xff\xf3", NULL,	":MP3",		"MPEG2 Layer 3", "mp3", DT_scp),
AH(0, 3, "ID3", vfd_ID3,	":MP3",		"MPEG1/2 Layer 3 with ID3v2", "mp3", DT_scp),
AH(4, 4, "ftyp", vfd_qt,	":QT",		"qt", "qt", DT_scp),
AH(0, 2, "0&", NULL,		":ASF",		"WindowsMediaTec.", "asf", DT_scp),
AH(0, 4, ".RMF", NULL,		":RM",		"RealAudio movie", "rm", DT_scp),
AH(4, 6, "moov\0", NULL,	":QT",		"QuickTime movie", "qt", DT_scp),
AH(0, 8, "\0\0\0\x08wide", NULL, ":MOV",	"QuickTime movie", "mov", DT_scp),
AH(0, 5, "fLaC", NULL,		":FLAC",	"FLAC audio", "flac", DT_scp),
AH(0, 4, "OggS", NULL,		":OGGV",	"Ogg Vorbis audio", "ogg", DT_scp),
AH(0, 4, "dns.", NULL,		":AU",		"Audio file(Sun, Next)", "au", DT_scp),
AH(0, 6, "\xd7\xcd\xc6\x9a\0", NULL, ":WMF", "Placeable Windows Meta File", "wmf", DT_scp),
AH(0, 4, "\1\0\x09", NULL,	":WMF",		"Windows MetaFile", "wmf", DT_scp),
AH(0x28, 4, " EMF", vfd_EMF, ":EMF",		"Enhanced MetaFile", "emf", DT_emf),
AH(0, 3, "FWS", NULL,		":SWF",		"Flash Contents", "swf", DT_scp),
AH(0, 3, "CWS", NULL,		":SWF",		"Flash Contents(Compressed)", "swf", DT_scp),
AH(0, 3, "FLV", NULL,		":FLV",		"Flash Video", "flv", DT_scp),

AH(0, 15, "binary stl file", NULL, ":STLB", "STL vector image", "stl", DT_scp),
AH(0, 11, "solid ascii", NULL, ":STLA",	"STL vector image", "stl", DT_scp),
AH(0, 9, "ISO-10303", NULL, ":STEP",	"STEP CAD image", "stp", DT_scp),
AH(0, 6, "#VRML ", NULL,	":VRML",	"VRML vector image", "wrl", DT_scp),
AH(0, 4, "glTF", NULL,		":GLTF",	"GL Transmission Format", "glb", DT_scp),
//	Encapsulated PostScript File  %!PS-Adobe-?.? EPSF
										// Etc --------------------------------
AH(0, 5, "\0\0\x1a\0\0", NULL, ":123",		"Lotus1-2-3", "WK3", DT_scp),
AH(0, 5, "\0\0\x1a\0\2", NULL, ":123",		"Lotus1-2-3", "WK4", DT_scp),
AH(0, 5, "\0\0\x1a\0\3", NULL, ":123",		"Lotus1-2-3", "123", DT_scp),
AH(3, 10, "\0FREELANCE", NULL, ":PRZ",		"LotusFreelance", "PRZ", DT_scp),
AH(0, 4, "!BDN", NULL,		":XCHG",	"MS-Exchange mailbox", "PST", DT_scp),
AH(0, 2, "?_", NULL,		":HELP",	"WinHELP", "HLP", DT_scp),
AH(0, 6, ":Base ", NULL,	":HELPC",	"WinHELP Contents", "CNT", DT_scp),
AH(0, 4, "ITSF", NULL,		":HHELP",	"HtmlHELP", "chm", DT_scp),
AH(0, 4, "DOC", vfd_JUST,	":JUST",	"Justsystem app.", "", DT_scp),
AH(0, 4, "PMCC", NULL,		":GRP",		"PROGMAN GROUP", "GRP", DT_scp),
AH(4, 3, "\x82\x6am", NULL,	":MBC",		"KTX macro binary", "MBC", DT_scp),
AH(0, 2, "\x00x", NULL,		":PIF",		"PIF/95", "PIF", DT_scp),
AH(0, 2, "\x00\x4b", NULL,	":PIF",		"PIF/NT", "pif", DT_scp),
AH(0, 4, "\x4c\0\0", vfd_link, ":LINK",	"Link", "lnk", DT_scp),
AH(0, 10, "!<symlink>", NULL, ":CLINK",	"cygwin symbolic Link", "", DT_scp),
AH(0x12, 13, "\x52\0\x65\0\x67\0\x69\0\x73\0\x74\0\x72", NULL,	":REG",	"Registry file", "reg", DT_utextL),
AH(0x3c, 8, "TEXtREAd", NULL, ":PDOC",	"Palm document", "PRC", DT_scp),
AH(4, 16, "Standard Jet DB", NULL, ":MDB",	"MS-Access DB", "mdb", DT_scp),
AH(0, 4, "MSFT", NULL,		":MSFT",	"Type Library", "tlb", DT_scp),
AH(0, 4, "\0\1\0\0", vfd_TTF, ":TTF",		"TrueType Font", "ttf", DT_scp),
AH(0, 5, "ttcf\0", vfd_TTF,	":TTC",		"TrueType Font Collection", "ttc", DT_scp),
AH(0, 16, "JASC BROWS FILE", NULL, ":JBF",	"Paint Shop Pro Thumb nail cache", "JBF", DT_scp),
AH(0, 4, "ID;P", NULL,		":SLK",		"SYLK form", "SLK", DT_scp),

AH(0, 8, "\3\0\0\0\1\0\0", vfd_yaffs, ":YAFFS",	"yaffs Disk Image", "img", DT_scp),
AH(0, 4, "hsqs", vfd_squashfs, ":SQUASHFS",	"squashfs Disk Image", "img", DT_scp),
AH(0x8000, CDHEADERSIZE, StrISO9660, NULL, ":CD", "CD-ROM Disk Image", "iso", DT_scp),
AH(0x9310, CDHEADERSIZE, StrISO9660, NULL, ":CD", "CD-ROM Disk Image", "bin", DT_scp),
AH(0x9318, CDHEADERSIZE, StrISO9660, NULL, ":CD", "CD-ROM XA Disk Image", "bin", DT_scp),
AH(0x8000, CDHEADERSIZE, StrUDF, NULL, ":CD",	"DVD Disk Image", "iso", DT_scp),
AH(0x9310, CDHEADERSIZE, StrUDF, NULL, ":CD",	"DVD Disk Image", "bin", DT_scp),

//AH(0, 4, "\xc3\xab\xcd\xab", NULL, ":ACS",	"Microsoft Agent Character File", "ACS", DT_scp),
										// PPx --------------------------------
AH(0, 4, "TC1", NULL,		":TC1",		"PPx Customize file", "DAT", DT_scp),
AH(0, 4, "TC2", NULL,		":TC2",		"PPx Customize file", "DAT", DT_scp),
AH(0, 4, "TH1", NULL,		":TH1",		"PPx History file", "DAT", DT_scp),
AH(0, 4, "TH2", NULL,		":TH2",		"PPx History file", "DAT", DT_scp),
AH(0, 6, "PPxCFG", NULL,	":XCFG",	"PPx configuration file", "CFG", DT_text),
AH(0, 9, UTF8HEADER "PPxCFG", NULL, ":XCFG", "PPx configuration file", "CFG", DT_utf8),
AH(0, 14, "\xff\xfeP\0P\0x\0C\0F\0G", NULL, ":XCFG", "PPx configuration file", "CFG", DT_utextL),
AH(0, sizeof(LHEADER) - 3 + 3, UTF8HEADER LHEADER, NULL, ":XLF", "PPx list file", "txt", DT_utf8),
AH(0, sizeof(LHEADER) - 3, LHEADER, NULL, ":XLF", "PPx list file", "txt", DT_scp),
AH(2, 18, ";\0L\0i\0s\0t\0F\0i\0l\0e", NULL, ":XLF", "PPx list file", "txt", DT_utextL),
AH(0, 9, "'!*script", NULL,	":XVBS",	"PPx script(vbs)", "vbs", DT_scp),
AH(0, 9, "#!*script", NULL,	":XPLS",	"PPx script(pls)", "pls", DT_scp),
AH(0, 10, "//!*script", NULL,	":XJS",		"PPx script(js)", "js", DT_scp),
AH(0, 13, UTF8HEADER "//!*script", NULL,	":XJS",		"PPx script(js)", "js", DT_utf8),
										// Text -------------------------------
AH(0, 2, UCF2HEADER, NULL,	":UTEXT",	"UNICODE text", "txt", DT_utextL),
AH(0, 4, "\xd\x0\xa\0x0", NULL, ":UTEXT", "UNICODE text", "txt", DT_utf16L),
AH(0, 2, "\xfe\xff", NULL,	":BUTXT",	"Big endian UNICODE", "txt", DT_utextB),
AH(0, 4, "\x0\xd\x0\0xa", NULL, ":BUTXT",	"Big endian UNICODE", "txt", DT_utf16B),
AH(0, 3, UTF8HEADER, NULL,	":UTF8",	"UTF-8 text", "txt", DT_utf8),
RH(0x200, 10, "\n#include ", vfd_text, ":C",	"C/C like source", "c", DT_c),
RH(0x200, 10, "\n#include\t", vfd_text, ":C",	"C/C like source", "c", DT_c),
AH(0, 14, "X-Deliver-To: ", NULL, ":EML",	"E-Mail message", "eml", DT_mail),
AH(0, 14, "Delivered-To: ", NULL, ":EML",	"E-Mail message", "eml", DT_mail),
AH(0, 10, "Received: ", NULL, ":EML",		"E-Mail message", "eml", DT_mail),
AH(0, 13, "Return-Path: ", NULL, ":EML",	"E-Mail message", "eml", DT_mail),
										// Encode -----------------------------
AH(0, 7, "\x1a\0\0\0\x1a\0", NULL, ":SSF",	"MULTI2 encode", "SSF", DT_scp),
										// Archive ----------------------------
AH(0x101, 5, "ustar", NULL,	":TAR",		"Tar archive", "tar", DT_scp),
AH(0, 8, "!<arch>\xa", NULL, ":AR",		"AR archive", "ar", DT_scp),
AH(0, 2, "\xc7\x71", NULL,	":CPIO",	"CPIO archive", "ar", DT_scp),
AH(0, 4, "SZDD", NULL,		":SZDD",	"MS-COMPRESS 5 archive", "", DT_scp),
AH(0, 4, "KWAJ", NULL,		":KWAJ",	"MS-COMPRESS 6 archive", "", DT_scp),
AH(0, 3, "ZOO", NULL,		":ZOO",		"ZOO archive", "ZOO", DT_scp),
AH(0, 3, "\x1f\x9d\x90", NULL, ":Z",	"compress archive", "z", DT_scp),
AH(0, 3, "BZh", NULL,		":BZIP2",	"bzip2 archive", "bz2", DT_scp),
AH(0, 2, "\x1f\x8b", NULL,	":GZIP",	"gzip archive", "gz", DT_scp),
AH(0, 4, "\xed\xab\xee\xdb", NULL, ":RPM",	"RPM package", "rpm", DT_scp),
AH(0, 3, "2.0", NULL,		":DEB",		"Debian package", "deb", DT_scp),
AH(0, 6, "MSWIM", NULL,		":WIM",		"Windows Imaging Format archive", "wim", DT_scp),
AH(0, 4, "\x78\x9f\x3e\x22", NULL, ":TNEF", "TNEF/winmail.dat archive", "dat", DT_scp),
// RH(0,～ はここから RH_L までに限定すること
RH(0, 4, "MSCF", vfd_CAB,	":CAB",		"MS-CABINET archive", "cab", DT_scp),
RH(0, 4, "PK\3\4", vfd_PK,	":PKZIP",	"PKZIP archive", "zip", DT_scp),
RH(0, 4, "Rar!", NULL,		":RAR",		"RAR archive", "rar", DT_scp),
RH(0, 3, "-lh", vfd_LHA,	":LHA",		"LHA archive", "LZH", DT_scp),
RH(0, 4, "GCA0", NULL,		":GCA",		"GCA archive", "GCA", DT_scp),
RH(0, 4, "GCAX", NULL,		":GCA",		"GCA archive", "GCA", DT_scp),
RH(0, 4, "7z\xbc\xaf", NULL, ":7Z",		"7-Zip archive", "7z", DT_scp),
RH(0, 6, "\xfd" "7zXZ", NULL, ":XZ",	"xz archive", "xz", DT_scp),
RH(0, 5, "\x5d\0\0\x4", NULL, ":LZMA",	"lzma archive", "lzma", DT_scp),
RH(0, 5, "\x5d\0\0\x80", NULL, ":LZMA",	"lzma archive", "lzma", DT_scp),
RH(0, 2, "\x60\xea", vfd_ARJ, ":ARJ",	"ARJ archive", "ARJ", DT_scp),
RH_L(0x4000, 3, "-lz", vfd_LHA, ":LHA",	"LArc archive", "LZS", DT_scp),
RH(0x4000, 3, "-pm", NULL,	":PMA",		"PMA archive", "PMA", DT_scp),
RH(0x4000, 7, "**ACE**", NULL, ":ACE",	"ACE archive", "ACE", DT_scp),
RH(0x4000, 8, "StuffIt ", NULL, ":SIT",	"StuffIt archive", "SIT", DT_scp),
AH(8, 12, "NullsoftInst", NULL, ":NSIS", "Nullsoft Scriptable Install System", "EXE", DT_scp),
AH(0, 4, "zlb\x1a", NULL,	":ZLIB",	"zlib archive", "zlib", DT_scp),
										// Etc2 -------------------------------
AH(0, 2, "MZ", vfd_EXE,		":EXE",	"", "EXE", DT_scp),
AH(0, 4, "\x7f\x45LF", vfd_ELF, ":ELF",	"UNIX progarm", "", DT_scp),
AH(0, 1, "\xfe", vfd_MSXBIN, ":MSXI",	"MSX BSAVE file", "BIN", DT_scp),
AH(0, 4, "HU\0", NULL,		":X68K",	"X68000 progarm", "x", DT_scp),
AH(0, 2, "MQ", vfd_MQ,		":TOWNS",	"FM-TOWNS progarm", "EXG", DT_scp),
AH(0, 2, "P3", vfd_P3,		":TOWNS",	"FM-TOWNS progarm", "EXP", DT_scp),
AH(0, 4, "\xCA\xFE\xBA\xBE", NULL, ":MACU",	"Mac OS X Universal binary", "", DT_scp),
AH(0, 4, "\xff\x00\x00\x00", NULL, ":E500B", "PC-E500 BASIC BINARY", "BAS", DT_scp),
AH(0, 4, "\xff\x00\x01\x00", NULL, ":E500D", "PC-E500 DATA.BAS", "BAS", DT_scp),
AH(0, 4, "\xff\x00\x02\x00", NULL, ":E500F", "PC-E500 FUNCKEY", "DAT", DT_scp),
AH(0, 4, "\xff\x00\x04\x01", NULL, ":E500E", "PC-E500 AER", "DAT", DT_scp),
AH(0, 4, "\xff\x00\x06\x01", NULL, ":E500I", "PC-E500 BSAVE file", "BIN", DT_scp),
AH(0, 4, "\xff\x00\x07\x01", NULL, ":E500R", "PC-E500 RAMFILE", "BIN", DT_scp),
AH(0, 4, "\xff\x00\x08\x00", NULL, ":E500A", "PC-E500 BASIC TEXT", "BAS", DT_scp),
AH(0x42f, 11, "\0Apple_HFS", NULL, ":DMG",	"Apple disk image", "bin", DT_scp),
AH(0x4af, 11, "\0Apple_HFS", NULL, ":DMG",	"Apple disk image", "bin", DT_scp),
AH(0x1fe, 2, "\x55\xaa", vfd_MBRIMG, ":MBR",	"MBR disk image", "bin", DT_scp),
RH(0x8000, 1, "\xeb", vfd_FATIMG, ":FAT",	"FAT Disk Image", "bin", DT_scp),
RH(0x8000, 1, "\xe9", vfd_FATIMG, ":FAT",	"FAT Disk Image", "bin", DT_scp),
AH(0, 1, "\0", vfd_MACBIN, ":MACBIN", "MacBinary", "BIN", DT_scp),
AH(0, 6, "\0\5\x16\7\0\2", NULL, ":MACRES",	"Mac Resource Forks", "", DT_scp),
RH(0x8000, 1, "\x60", vfd_FATIMG, ":XFD",	"X68000 Disk Image", "BIN", DT_scp),
AH(0, 17, "MEDIA DESCRIPTOR\1", NULL, ":MDS", "CD-ROM media descriptor", "mds", DT_scp),
AH(0x1002, 6, "\x90\x90IPL1", NULL, ":98HD", "PC-9801 HD Image", "BIN", DT_scp),
AH(0, 4, "COWD", NULL, ":VMDK", "VMware Disk Image(-3.x)", "vmdk", DT_scp),
AH(0, 4, "KDMV", NULL, ":VMDK4", "VMware Disk Image(4.x)", "vmdk", DT_scp),
AH(0, 4, "\xb4\x6e\x68\x44", NULL, ":TIB",	"True Image Disk Image", "tib", DT_scp),
AH(0, 10, "conectix\0", NULL,	":VHD",	"VHD Disk Image", "vhd", DT_scp),
AH(0, 8, "vhdxfile", NULL,		":VHDX",	"VHDx Disk Image", "vhdx", DT_scp),
AH(0, 9, "PGPdMAIN|", NULL,	":PGD",		"PGP Disk Image", "pgd", DT_scp),
AH(0, 4, "MSQM", NULL,		":SQM",		"MS Service Quality Monitoring log", "sqm", DT_scp),
{ 0, 0, 0, NULL, NULL, NULL, NULL, NULL, 0}
};

const VFD_HEADERS DirHeader =
	NH(0, 0, NULL, NULL, ":DIR", "Directory", "", DT_scp);


const VFD_HEADERS *ReverseList[] = {
	RootHeaders, JustHeaders, RIFFheaders, NULL
};

const VFD_HEADERS AllHeader =
	NH(0, 0, NULL, NULL, "*", "Default", "", DT_scp);

ERRORCODE ReverseGetFileType(const TCHAR *FileName, VFSFILETYPE *result)
{
	const VFD_HEADERS **mainlist, *list;
	int mode;

	if ( FileName[0] == '\0' ) return ERROR_NO_DATA_DETECTED;
	mode = (FileName[0] == ':');
	for ( mainlist = ReverseList ; *mainlist != NULL ; mainlist++ ){
		for ( list = *mainlist ; list->flags ; list++ ){
			if ( !tstricmp(mode ? list->type : list->ext, FileName) ){
				SetFileType(result, list);
				return NO_ERROR;
			}
		}
	}
	if ( mode && !tstricmp(DirHeader.type, FileName) ){
		SetFileType(result, &DirHeader);
		return NO_ERROR;
	}
	if ( !tstricmp(AllHeader.type, FileName) ){
		SetFileType(result, &AllHeader);
		return NO_ERROR;
	}
	return ERROR_NO_DATA_DETECTED;
}

VFSDLL ERRORCODE PPXAPI VFSGetFileType(const TCHAR *FileName, const char *image, DWORD size, VFSFILETYPE *result)
{
	const VFD_HEADERS *vdh, *vdh_rel = NULL;
	char buf[VFS_check_size], *p_rel = NULL;
	DWORD atr;

	if ( result != NULL ){
		result->info = NULL;
		result->comment = NULL;
	}
	if ( size == MAX32 ) return ReverseGetFileType(FileName, result);
										// ディレクトリかを判別 ---------------
	if ( FileName[0] == ':' ){
		vdh = &DirHeader;
		goto found;
	}
	atr = GetFileAttributesL(FileName);
	if ( atr == BADATTR ){
		if ( size != 0 ){
			atr = 0;
		}else{
			return GetLastError();
		}
	}
	if ( atr & FILE_ATTRIBUTE_DIRECTORY ){
		vdh = &DirHeader;
		goto found;
	}
										// メモリイメージを取得 ---------------
	if ( size == 0 ){
		HANDLE hFile;

		hFile = CreateFileL(FileName, GENERIC_READ,
				FILE_SHARE_WRITE | FILE_SHARE_READ, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if ( hFile == INVALID_HANDLE_VALUE ) return GetLastError();
		size = ReadFileHeader(hFile, (BYTE *)buf, VFS_check_size);
		CloseHandle(hFile);
		image = buf;
	}
	if ( usertypes != NULL ){
		if ( IsTrue(CheckUserFileType(image, size, result)) ) return NO_ERROR;
	}
										// 判別メイン -------------------------
	for ( vdh = RootHeaders ; vdh->flags ; vdh++ ){
		if ( !(vdh->flags & HEADERTYPE_REL) ){	// 固定位置ヘッダ
			if ( size <= (vdh->off + vdh->hsize) ) continue;
			if ( 0 == memcmp(image + vdh->off, vdh->header, vdh->hsize) ){
				ERRORCODE err;

				if ( vdh->mChk == NULL ) goto found;
				err = vdh->mChk(FileName, image, size, image + vdh->off, result);
				if ( err == ERROR_MORE_DATA ) return NO_ERROR;
				if ( err == NO_ERROR ) goto found;
				if ( err == ERROR_FILEMARK_DETECTED ) return err;
				continue;
			}
		}else{									// 浮動位置ヘッダ -------------
			const char *ptr;
			char *p;
			DWORD leftsize;

			if ( size <= vdh->hsize ) continue;
			leftsize = size - vdh->hsize;
			ptr = image;
			if ( vdh->off ){ // 検索範囲の制限有り
				if ( vdh->flags & HEADERTYPE_REL_LAST ){
					if ( p_rel != NULL ){
						vdh = vdh_rel;
						goto found;
					}
				}
				if ( leftsize > vdh->off ) leftsize = vdh->off;
			}else{
				if ( leftsize > 200*1024 ) leftsize = 200*1024;
			}
			while ( (leftsize > 0) &&
					(p = memchr(ptr, *vdh->header, leftsize)) != NULL ){
				if ( !memcmp(p + 1, vdh->header + 1, vdh->hsize - 1) ){
					ERRORCODE err = NO_ERROR;

					if ( (vdh->mChk == NULL) ||
						 (err = vdh->mChk(FileName, image, size, p, result)) == NO_ERROR ){
						if ( vdh->off == 0 ){
							if ( (p_rel == NULL) || (p < p_rel) ){
								vdh_rel = vdh;
								p_rel = p;
							}
						}else{
							goto found;
						}
					}
					if ( err == ERROR_MORE_DATA ) return NO_ERROR;
				}
				leftsize -= (SIZE32_T)(p - ptr + 1);
				if ( leftsize <= 0 ) break;
				ptr = p + 1;
			}
		}
	}
	return ERROR_NO_DATA_DETECTED;

found:
	if ( result != NULL ) SetFileType(result, vdh);
	return NO_ERROR;
}

VFSDLL DWORD PPXAPI ReadFileHeader(HANDLE hFile, BYTE *header, DWORD headersize)
{
	DWORD size, moreread, fsize; // Readfile用、追加読込サイズ、読込済みサイズ

	fsize = 0;
	if ( ReadFile(hFile, header, min(headersize, VFS_TINYREADSIZE), &fsize, NULL) == FALSE ){
		fsize = 0;
	}
	moreread = headersize - fsize; // headersize は必ず fsize 以上
	// EXE ヘッダなら、末尾に書庫があるか調べる
	if ( (fsize >= VFS_TINYREADSIZE) && (*header == 'M') && (*(header+1) == 'Z') ){
		DWORD offset;
		IMAGE_NT_HEADERS32 *xhdr;
		IMAGE_SECTION_HEADER *LastSection;
		BY_HANDLE_FILE_INFORMATION finfo;

		offset = ((IMAGE_DOS_HEADER *)header)->e_lfanew; // 拡張ヘッダoffset
		if ( offset < (fsize - 0x300) ){ // 書庫ファイルなら大抵ヘッダが小さめ
			xhdr = (IMAGE_NT_HEADERS32 *)(header + offset);
			LastSection = IMAGE_FIRST_SECTION(xhdr) + xhdr->FileHeader.NumberOfSections - 1;
			if ( fsize > ((BYTE *)LastSection - header + sizeof(IMAGE_SECTION_HEADER)) ){
			size = LastSection->PointerToRawData + LastSection->SizeOfRawData;
			GetFileInformationByHandle(hFile, &finfo);
			// EXEヘッダが示すサイズが末尾+署名分のサイズより大きい
			if ( (finfo.nFileSizeLow > (size + xhdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size) ) &&
					// EXEが1000h単位に揃えられていない?
					(finfo.nFileSizeLow != ((size + 0xfff) & 0xfffff000)) ){
				// EXEフォーマット末尾より後ろの一部分も取得
				if ( MAX32 != SetFilePointer(hFile, size, NULL, FILE_BEGIN) ){
					ReadFile(hFile, header + VFS_TINYREADSIZE, VFS_ARCREADSIZE, &fsize, NULL);
					// EXEフォーマット途中を取得
					SetFilePointer(hFile, VFS_TINYREADSIZE, NULL, FILE_BEGIN);
					fsize += VFS_TINYREADSIZE;
					if ( headersize > fsize ) moreread = headersize - fsize;
				}
			}
			}
		}
	}else if ( VFS_TINYREADSIZE == fsize ){
		if ( !((*header == 0xc5) && (*(header+1) == 0xc5)) &&
			 !((*header == 0x33) && (*(header+1) == 0xed) && (*(header+2) == 0x90)) &&
			 !((*header == '\0') && ((*(header+1) == '\0') || (*(header+1) == 0xff))) &&
			 !((*header == 'E') && (*(header+1) == 'R')) ){ // ISO 以外?
			if ( headersize > VFS_GENERALREADSIZE ){
				moreread = VFS_GENERALREADSIZE - fsize;
			}
		}
	}
	if ( moreread ){
		size = 0;
		ReadFile(hFile, header + fsize, moreread, &size, NULL);
		fsize += size;
	}
	if ( fsize < headersize ) memset(header + fsize, 0, headersize - fsize);
	return fsize;
}

// Windows が保持している拡張子名から種類を示す文字列を得る -------------------
VFSDLL BOOL PPXAPI VFSCheckFileByExt(const TCHAR *FileName, TCHAR *result)
{
	const TCHAR *ptr;
	TCHAR buf[VFPS];
	int err = FALSE;

	if ( result != NULL ) *result = '\0';
	ptr = VFSFindLastEntry(FileName);
	ptr += FindExtSeparator(ptr);
	if ( *ptr == '\0' ) return FALSE;
										// 拡張子からキーを求める -------------
	if ( GetRegString(HKEY_CLASSES_ROOT, ptr, NilStr, buf, TSIZEOF(buf)) ){
										// アプリケーションのシェル -----------
		if ( !GetRegString(HKEY_CLASSES_ROOT, buf, NilStr,
				result ? result : buf, TSIZEOF(buf)) ){
			if ( result != NULL ) tstrcpy(result, buf); // max VFPS
		}
		err = TRUE;
	}
	return err;
}
