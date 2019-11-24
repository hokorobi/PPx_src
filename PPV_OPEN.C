/*-----------------------------------------------------------------------------
	Paper Plane vUI												Open
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <winioctl.h>
#include "PPX.H"
#include "VFS.H"
#include "PPX_64.H"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop

#define COUNTREADTIME 0
#define FILEBUFMARGIN 1024

const DWORD Palette16[16] = { // Raw Image 用デフォルトパレット
	0x000000, 0x800000, 0x008000, 0x808000,
	0x000080, 0x800080, 0x008080, 0x808080,
	0xc0c0c0, 0xff0000, 0x00ff00, 0xffff00,
	0x0000ff, 0xff00ff, 0x00ffff, 0xffffff
};

#pragma pack(push, 1)
// EMF/RawImage/OS/2用 仮DIB(そのうち動的確保にするつもり)
struct {
	BITMAPINFOHEADER bmiHeader;
	DWORD bmiColors[16];
} temp_DIB;
#pragma pack(pop)
BITMAPINFOHEADER *temp_DIB_ptr = NULL;

// バックグラウンド行数計算 ----------------------
#define READINTERVAL_FILEREAD  ((TIMER_READLINE * 5) / 16)
#define READINTERVAL_LINECOUNT ((TIMER_READLINE * 10) / 16)

int CountReadLines = READLINE_TIMER;
int ReadDisp = 0;
DWORD ReadStep;
int ReadEnter = 0;

const TCHAR *runasstate = NULL;
// ----------------------
int X_llsizTable[] = { 0, IDNO, IDYES, IDCANCEL };

void LoadEvent(void);
void CheckType(void);

BOOL ReadFileBreakCheck(void);
void CALLBACK AnimateProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

#if COUNTREADTIME
 DWORD readstartTick;
#endif

typedef struct {
	int page;	// 表示するページ
	DWORD timer;
	size_t headersize;
	size_t offset;
	size_t size;
	size_t optionoffset;
	POINT dispoffset;
	int trans;
} GETPAGESTRUCT;

// GIF ------------------------------------------------------------------------
#pragma pack(push, 1)
typedef struct {
	BYTE ID[6];			//+0 GID87a/GIF89a
	WORD width, height;	//+6, 8
	BYTE flag;		//+10 B7:Palette B6-4:Dot size B3:sort B2-0:Palette size
	BYTE Tindex;	//+11 透明色 index
	BYTE aspect;	//+12 比率
	BYTE palette[];	//+13 パレット
} GIFHEADER;

typedef struct {
	BYTE ID;			//+0 GIFHEADER_IMAGEBLOCKID
	WORD left, top;		//+1, 3
	WORD width, height;	//+5, 7
	BYTE flag;		//+9 B7:Palette B6:Interlace B5:sort B4-3:unuse B2-0:Palette size
	BYTE palette[];	//+10 パレット
//	BYTE LZHsize;
} GIFIMAGEHEADER;

typedef struct {
	BYTE size;			//+0   (4)
	BYTE flags;			//+1 GIFCONTROLHEADER_xxx
	WORD time;			//+2, 3
	BYTE trans;			//+4 透過色パレット
} GIFCONTROLHEADER;
#pragma pack(pop)

#define GIFHEADER_FLAG_USEPALETTE B7
#define GIFHEADERGETPALETTE(flag) ((2 << (flag & 7)) * 3)
#define GIFHEADER_IMAGEBLOCKID 0x2c
#define GIFHEADER_EXTENSIONID 0x21
#define GIFHEADER_EXTENSION_CONTROL 0xf9
#define GIFHEADER_ENDID 0x3b
#define GIFCONTROLHEADER_TRANSPARENT B0
#define GIFCONTROLHEADER_USERINPUT B1
#define GIFCONTROLHEADER_DRAW_MASK (B2 | B3 | B4)
#define GIFCONTROLHEADER_DRAW_OVER B2
#define GIFCONTROLHEADER_DRAW_BG B3
#define GIFCONTROLHEADER_DRAW_BACK (B2 | B3)

// PNG ------------------------------------------------------------------------
#pragma pack(push, 1)
typedef struct {
	BYTE header[8]; // 0x89 PNG 0x0d 0x0a 0x1a 0x0a
	BYTE IHDR[8]; // 0 0 0 0x0d IHDR
	DWORD width;
	DWORD height;
	BYTE bits;
	BYTE colorflags;
	BYTE alg;
	BYTE filter;
	BYTE interla;
	DWORD CRC;
} PNGheader;

typedef struct {
	DWORD length;
	DWORD id;
} PNGchunk;

typedef struct {
	PNGchunk chunk;
	DWORD frames;
	DWORD loops;
} PNG_AnimationControl_chunk;

typedef struct {
	PNGchunk pc;
	DWORD sequence_number;
	DWORD width;
	DWORD height;
	DWORD x_offset;
	DWORD y_offset;
	WORD delay_num;
	WORD delay_den;
	BYTE dispose_op;
	BYTE blend_op;
} PNGFrameControlStruct;

typedef struct {
	PNGchunk pc;
	BYTE r1, red, g1, green, b1, blue;
} PNGBackGroundStruct;

#define SizeOfPNGcrc 4
#define SizeOfPNGframes 4
#define SizeOfPNGid 4
#define SizeOfPNGchunks (sizeof(PNGchunk) + SizeOfPNGcrc)
#define PNG_IDAT 0x54414449
#define PNG_tRNS 0x534e5274
#define PNG_bKGD 0x44474b62
#define PNG_acTL 0x4c546361
#define PNG_fdAT 0x54416466
#define PNG_fcTL 0x4c546366
#define APNG_DISPOSE_OP_NONE 0
#define APNG_DISPOSE_OP_BACKGROUND 1
#define APNG_DISPOSE_OP_PREVIOUS 2
#define APNG_BLEND_OP_SOURCE 0
#define APNG_BLEND_OP_OVER 1
#pragma pack(pop)

PreFrameInfoStruct PreFrameInfo = { PreFramePage_NoCheck , {0, 0} , {0, 0} , NULL, 0, 0 };

void InitRawImageRectSize(void)
{
	vo_.bitmap.rawsize.cx = (VOi->defwidth <= 0) ? WndSize.cx : VOi->defwidth;
	if ( vo_.bitmap.rawsize.cx <= 0 ) vo_.bitmap.rawsize.cx = 1;
	vo_.bitmap.rawsize.cy = 1;
	if ( vo_.bitmap.ShowInfo->biBitCount ){
		DWORD linesize = DwordBitSize(vo_.bitmap.rawsize.cx * vo_.bitmap.ShowInfo->biBitCount);
		if ( vo_.file.UseSize < linesize ){ // 1行満たない
			vo_.bitmap.rawsize.cx = (vo_.bitmap.rawsize.cx * 8) / (vo_.bitmap.ShowInfo->biBitCount);
		}else{
			vo_.bitmap.rawsize.cy = vo_.file.UseSize / linesize;
		}
		vo_.bitmap.ShowInfo->biWidth = vo_.bitmap.rawsize.cx;
		vo_.bitmap.ShowInfo->biHeight = vo_.bitmap.rawsize.cy;
		vo_.bitmap.showsize = vo_.bitmap.rawsize;
		vo_.bitmap.info->biSizeImage = DwordBitSize(vo_.bitmap.rawsize.cx * vo_.bitmap.ShowInfo->biBitCount) * vo_.bitmap.rawsize.cy;
	}
}

// TIFF ----------------------------------------------------------------------
#pragma pack(push, 1)
typedef struct {
	WORD tag;
	WORD type;
	DWORD count, offset;
} IFD;
#pragma pack(pop)

// ------------------------------------------------------------------------

DWORD USEFASTCALL GetBigEndDWORD(void *ptr)
{
	DWORD value;

	value = *(DWORD *)ptr;
	return	(value << 24) |
			((value & 0xff00) << 8) |
			((value >> 8) & 0xff00) |
			(value >> 24);
}

DWORD USEFASTCALL GetLittleEndDWORD(void *ptr)
{
	return *(DWORD *)ptr;
}

typedef DWORD (USEFASTCALL * GetDWORDfunc)(void *ptr);

WORD USEFASTCALL GetBigEndWORD(void *ptr)
{
	WORD value;

	value = *(WORD *)ptr;
	return	(WORD)((WORD)(value >> 8) | (WORD)(value << 8));
}

WORD USEFASTCALL GetLittleEndWORD(void *ptr)
{
	return *(WORD *)ptr;
}

typedef WORD (USEFASTCALL * GetWORDfunc)(void *ptr);

void USEFASTCALL SetBigEndDWORD(BYTE *ptr, DWORD value)
{
	*(DWORD *)ptr =
		(value << 24) |
		((value & 0xff00) << 8) |
		((value & 0xff0000) >> 8) |
		(value >> 24);
}

void InitPNG(GETPAGESTRUCT *gpage)
{
	size_t offset = sizeof(PNGheader);
	int page = 0, frames = 0;
	BOOL findfcTL = FALSE;
	DWORD timer = 1000;

#ifdef _WIN64
	if ( vo_.file.UseSize > ((DWORD)3 * GB) ) return;
#endif
	if ( gpage != NULL ){
		gpage->headersize = offset;
		gpage->timer = 1000;
		gpage->dispoffset.x = 0;
		gpage->dispoffset.y = 0;
		gpage->trans = -1;
		gpage->offset = 0;
		gpage->optionoffset = 0;
		page = gpage->page;
	}

	while( vo_.file.UseSize > offset ){
		PNGchunk *pc;

		pc = (PNGchunk *)(vo_.file.image + offset);
		if ( pc->id == PNG_acTL ){ // acTL フレーム数
			if ( gpage == NULL ){
				frames = GetBigEndDWORD(&((PNG_AnimationControl_chunk *)pc)->frames);
			}
		}else if ( pc->id == PNG_IDAT ){ // ビットマップ
			if ( gpage != NULL ){
				if ( gpage->offset == 0 ){
					gpage->offset = offset;

					if ( findfcTL == FALSE ){
						if ( page == 0 ) return;
						page--;
						findfcTL = TRUE;
					}
				}
			}else{
				if ( (findfcTL == FALSE) && frames ){
					frames++;
					findfcTL = TRUE;
				}
			}
		}else if ( pc->id == PNG_tRNS ){ // tRNS 透過情報
			if ( ((PNGheader *)vo_.file.image)->colorflags == 3 ){
				int colorlen, colorindex;
				BYTE *alpha, alphamax;

				// 一番透過度が低いパレットを透明色にする
				alpha = (BYTE *)(pc + 1);
				alphamax = 0x40;
				colorlen = GetBigEndDWORD(&pc->length);
				for ( colorindex = 0 ; colorindex < colorlen ; colorindex++, alpha++){
					if ( *alpha < alphamax ){
						alphamax = *alpha;
						vo_.bitmap.transcolor = colorindex;
					}
				}
			}
		}else if ( pc->id == PNG_bKGD ){ // bKGD 背景色情報
			if ( gpage != NULL ){
				gpage->trans = -2;
				if ( GetBigEndDWORD(&pc->length) == 6 ){ // full color
					gpage->trans = ((PNGBackGroundStruct *)pc)->blue + (((PNGBackGroundStruct *)pc)->green << 8) + (((PNGBackGroundStruct *)pc)->red << 16);
				}
			}
		}else if ( pc->id == PNG_fcTL ){ // fcTL フレーム情報
			findfcTL = TRUE;

			if ( page == 0 ){
				PNGFrameControlStruct *fcs;

				fcs = (PNGFrameControlStruct *)pc;

				timer = GetBigEndWORD(&fcs->delay_den);
				if ( timer == 0 ) timer = 100;
				timer = (GetBigEndWORD(&fcs->delay_num) * 1000) / timer;
				if ( timer == 0 ) timer = 10;

				if ( gpage != NULL ){
					if ( gpage->offset == 0 ) gpage->offset = offset;
					gpage->optionoffset = offset;
					gpage->timer = timer;
					gpage->dispoffset.x = GetBigEndDWORD(&fcs->x_offset);
					gpage->dispoffset.y = GetBigEndDWORD(&fcs->y_offset);
					return;
				}else{
					if ( (fcs->dispose_op == APNG_DISPOSE_OP_BACKGROUND) &&
						 (PreFrameInfo.page == PreFramePage_NoCheck) ){
						PreFrameInfo.page = PreFramePage_BG;
						PreFrameInfo.offset.x = PreFrameInfo.offset.y = 0;
						PreFrameInfo.size.cx = vo_.bitmap.info->biWidth;
						PreFrameInfo.size.cy = vo_.bitmap.info->biHeight;
						if ( PreFrameInfo.size.cy < 0 ) PreFrameInfo.size.cy = -PreFrameInfo.size.cy;
					}
					break;
				}
			}
			page--;
		}
		offset += GetBigEndDWORD(&pc->length) + SizeOfPNGchunks;
	}

	if ( frames ){
		if ( vo_.bitmap.page.max == 0 ){
			if ( vo_.bitmap.page.animate ){
				SetTimer(vinfo.info.hWnd, TIMERID_ANIMATE,
						timer ? timer : 10, AnimateProc);
			}else{
				SetPopMsg(POPMSG_NOLOGMSG, MES_IANI);
			}
		}
		if ( gpage == NULL ) vo_.bitmap.page.max = frames;
	}
}

void InitGIF(GETPAGESTRUCT *gpage)
{
	size_t offset = sizeof(GIFHEADER);
	int imagecount = 0, firsttimer = 0;
	GIFHEADER *gh;

#ifdef _WIN64
	if ( vo_.file.UseSize > ((DWORD)3 * GB) ) return;
#endif
	gh = (GIFHEADER *)vo_.file.image;
	if ( gh->flag & GIFHEADER_FLAG_USEPALETTE ){
		offset += GIFHEADERGETPALETTE(gh->flag);
	}
	PreFrameInfo.fillcolor = gh->Tindex;
	if ( gpage != NULL ){
		gpage->headersize = offset;
		gpage->timer = 1000;
		gpage->dispoffset.x = 0;
		gpage->dispoffset.y = 0;
		gpage->trans = -1;
	}
	while( vo_.file.UseSize > offset ){
		switch ( *(vo_.file.image + offset) ){
			case GIFHEADER_IMAGEBLOCKID: {	// Image
				GIFIMAGEHEADER *gih;

				if ( vo_.file.UseSize <= (offset + sizeof(GIFIMAGEHEADER) + 1) ){
					break;
				}
				if ( gpage != NULL ) gpage->offset = offset;

				gih = (GIFIMAGEHEADER *)(vo_.file.image + offset);
				if ( gih->flag & GIFHEADER_FLAG_USEPALETTE ){
					offset += GIFHEADERGETPALETTE(gih->flag);
				}
				offset += sizeof(GIFIMAGEHEADER) + 1;
				while( vo_.file.UseSize >= (offset + 1) ){ // data block
					BYTE size;

					size = *(vo_.file.image + offset++);
					if ( size == 0 ) break;
					offset += size;
				}
				if ( gpage != NULL ){
					if ( imagecount == gpage->page ){
						gpage->dispoffset.x = gih->left;
						gpage->dispoffset.y = gih->top;
						gpage->size = offset - gpage->offset;
						offset = vo_.file.UseSize; // 走査終了
					}
				}else if ( (imagecount == 0) && (PreFrameInfo.page == PreFramePage_NoCheck) ){
					PreFrameInfo.offset.x = gih->left;
					PreFrameInfo.offset.y = gih->top;
				}
				imagecount++;
				break;
			}

			case GIFHEADER_EXTENSIONID: {	// Extension
				BYTE id;

				if ( vo_.file.UseSize <= (offset + 3) ) break;
				id = *(vo_.file.image + offset + 1);
				offset += 2;

				if ( (id == GIFHEADER_EXTENSION_CONTROL) &&
						(vo_.file.UseSize >= (offset + 5)) ){
					GIFCONTROLHEADER *gt;

					gt = (GIFCONTROLHEADER *)(vo_.file.image + offset);
					if ( gpage != NULL ){
						if ( imagecount == gpage->page ){
							gpage->timer = gt->time * 10;
							if ( gpage->timer == 0 ) gpage->timer = 40;

							if ( gt->flags & GIFCONTROLHEADER_TRANSPARENT ){
								PreFrameInfo.fillcolor = (BYTE)(gpage->trans = gt->trans);
							}
							if ( firsttimer == 0 ) firsttimer = gpage->timer;

							switch ( gt->flags & GIFCONTROLHEADER_DRAW_MASK ){
								case GIFCONTROLHEADER_DRAW_BG:
									PreFrameInfo.page = PreFramePage_BG;
									break;
								case GIFCONTROLHEADER_DRAW_BACK:
//									PreFrameInfo.page = PreFramePage_Restore;
									break;
							}
						}
					}else if ( (imagecount == 0) && (PreFrameInfo.page == PreFramePage_NoCheck) ){
						if ( gt->flags & GIFCONTROLHEADER_TRANSPARENT ){
							PreFrameInfo.fillcolor = (BYTE)(vo_.bitmap.transcolor = gt->trans);
						}
						if ( ((gt->flags & GIFCONTROLHEADER_DRAW_MASK) ==
							  GIFCONTROLHEADER_DRAW_BG) ){
							PreFrameInfo.page = PreFramePage_BG;
							PreFrameInfo.size.cx = vo_.bitmap.info->biWidth;
							PreFrameInfo.size.cy = vo_.bitmap.info->biHeight;
							if ( PreFrameInfo.size.cy < 0 ) PreFrameInfo.size.cy = -PreFrameInfo.size.cy;
						}
						// ※ GIFCONTROLHEADER_DRAW_BACK は Page0では無効
					}
				}

				while( vo_.file.UseSize >= (offset + 1) ){ // data block
					BYTE size;

					size = *(vo_.file.image + offset++);
					if ( !size ) break;
					offset += size;
				}
				break;
			}

			case GIFHEADER_ENDID:	// end
			default:	// 不明
				offset = vo_.file.UseSize;
				break;
		}
	}
	if ( imagecount > 1 ){
		if ( vo_.bitmap.page.max == 0 ){
			if ( vo_.bitmap.page.animate ){
				SetTimer(vinfo.info.hWnd, TIMERID_ANIMATE,
						firsttimer ? firsttimer : 1000, AnimateProc);
			}else{
				SetPopMsg(POPMSG_NOLOGMSG, MES_IANI);
			}
		}
		if ( gpage == NULL ) vo_.bitmap.page.max = imagecount;
	}
}

GetDWORDfunc InitTIFF(GETPAGESTRUCT *gpage)
{
	size_t offset = 4;
	int IFDcount = 0;
	GetWORDfunc GetWord;
	GetDWORDfunc GetDword;

#ifdef _WIN64
	if ( vo_.file.UseSize > ((DWORD)3 * GB) ) return NULL;
#endif
	if ( *vo_.file.image == 'M' ){
		GetDword = GetBigEndDWORD;
		GetWord = GetBigEndWORD;
	}else{
		GetDword = GetLittleEndDWORD;
		GetWord = GetLittleEndWORD;
	}

	if ( gpage != NULL ){
		gpage->headersize = 4;
		gpage->timer = 1000;
		gpage->dispoffset.x = 0;
		gpage->dispoffset.y = 0;
		gpage->trans = -1;
	}
	if ( gpage != NULL ) gpage->offset = 8;	// 仮のオフセットを用意
	while( vo_.file.UseSize > (offset + sizeof(DWORD)) ){
		int IFDdata;

		offset = GetDword(vo_.file.image + offset); // IFD の場所を取得
		if ( !offset || (vo_.file.UseSize <= (offset + sizeof(WORD))) ) break;
		if ( (gpage != NULL) && (IFDcount == gpage->page) ){
			gpage->offset = offset;
			break;
		}
		IFDdata = GetWord(vo_.file.image + offset);
		if ( !IFDdata ) break;
		offset += sizeof(WORD) + IFDdata * sizeof(IFD);
		IFDcount++;
	}
	if ( IFDcount > 1 ){
		if ( vo_.bitmap.page.max == 0 ) SetPopMsg(POPMSG_NOLOGMSG, MES_IPAG);
		if ( gpage == NULL ) vo_.bitmap.page.max = IFDcount;
	}
	return GetDword;
}

HILIGHTKEYWORD *LoadDefaultHighlight(ThSTRUCT *mem, const TCHAR *filename, const TCHAR *ext)
{
	TCHAR tmp[0x400];
	const TCHAR *tmpp;
	HILIGHTKEYWORD *hks = NULL;
	int count = 0;
	int check = 0;

	while( EnumCustTable(count, T("CV_hkey"), tmp, NULL, 0) >= 0 ){
		FN_REGEXP fn;

		if ( *tmp == '/' ){
			if ( filename != NULL ){
				MakeFN_REGEXP(&fn, tmp + 1);
				check = FilenameRegularExpression(filename, &fn);
				FreeFN_REGEXP(&fn);
			}
		}else{
			check = (tstricmp(ext, tmp) == 0);
		}
		if ( check ) break;
		count++;
	}
	if ( check == 0 ) return NULL; // 該当無し

	tmp[0] = '\0';
	EnumCustTable(count, T("CV_hkey"), tmp, tmp, sizeof(tmp));
	if ( tmp[0] == '\0' ) return NULL; // 中身無し

	tmpp = tmp;
	for ( ;; ){
		TCHAR *nextp;
		int sizeA, sizeW, size, color, extend;
#ifdef UNICODE
		#define strA bufA
		#define strW tmpp
		char bufA[100];
#else
		#define strA tmpp
		#define strW bufW
		WCHAR bufW[100];
#endif
		nextp = tstrchr(tmpp, '\n');
		if ( nextp != NULL ) *nextp++ = '\0';
		if ( *tmpp == '\0' ) break;

		extend = 0;
		if ( SkipSpace(&tmpp) == '<' ){
			tmpp++;
			extend = HILIGHTKEYWORD_T;
		}
		if ( *tmpp == '>' ){
			tmpp++;
			extend |= HILIGHTKEYWORD_B;
		}
		if ( *tmpp == '~' ){
			tmpp++;
			extend |= HILIGHTKEYWORD_R;
		}
		if ( SkipSpace(&tmpp) == ',' ) tmpp++;
									// 色を取得
		color = GetColor(&tmpp, TRUE);
		if ( SkipSpace(&tmpp) == ',' ) tmpp++;
									// キーワードを取得 & 文字コードを２つ用意
#ifdef UNICODE
		UnicodeToAnsi(tmpp, bufA, 100);
		sizeA = strlen32(bufA) + 1;
		sizeW = (strlenW32(tmpp) + 1) * sizeof(WCHAR);
#else
		AnsiToUnicode(tmpp, bufW, 100);
		sizeA = strlen(tmpp) + 1;
		sizeW = (strlenW(bufW) + 1) * sizeof(WCHAR);
#endif
									// 保存する
		size = sizeof(HILIGHTKEYWORD) + sizeW +((sizeA + 1) & 0xfffe);
		ThSize(mem, size);
		hks = (HILIGHTKEYWORD *)ThLast(mem);
		hks->next = (HILIGHTKEYWORD *)(LONG_PTR)size;
		hks->ascii = (const char *)(size_t)(sizeW);
		hks->wide = 0;
		hks->color = color;
		hks->extend = extend;
		memcpy((char *)(hks + 1) + sizeW, strA, sizeA);
		memcpy((char *)(hks + 1), strW, sizeW);
		mem->top += size;

		if ( nextp == NULL ) break;
		tmpp = nextp;
	}
	return hks;
#undef strA
#undef strW
}

void LoadHighlight(VFSFILETYPE *vft)
{
	const TCHAR *fname, *ext, *textp;
	ThSTRUCT mem;
	HILIGHTKEYWORD *hks;
	TCHAR buf[CMDLINESIZE];
	int colorcount = 0;

	if ( X_hkey != NULL ) HeapFree( PPvHeap, 0, X_hkey );
	X_hkey = NULL;
	ThInit(&mem);

	// デフォルトのハイライト設定を取得
	fname = VFSFindLastEntry(vo_.file.name);
	ext = fname + FindExtSeparator(fname);
	if ( *ext == '.' ) ext++;

	if ( (hks = LoadDefaultHighlight(&mem, fname, ext)) == NULL ){
		if ( (vft->type[0] == '\0') || ((hks = LoadDefaultHighlight(&mem, NULL, vft->type)) == NULL) ){
			hks = LoadDefaultHighlight(&mem, NULL, T("*"));
		}
	}

	// 一時ハイライト設定を取得
	textp = ThGetString(NULL, T("Highlight"), NULL, 0);
	if ( textp != NULL ) while ( GetLineParam(&textp, buf) >= ' ' ){
		int sizeA, sizeW, size, color;
#ifdef UNICODE
		#define strA bufA
		#define strW buf
		char bufA[100];
#else
		#define strA buf
		#define strW bufW
		WCHAR bufW[100];
#endif
									// キーワードを取得 & 文字コードを２つ用意
#ifdef UNICODE
		UnicodeToAnsi(buf, bufA, 100);
		sizeA = strlen32(bufA) + 1;
		sizeW = (strlenW32(buf) + 1) * sizeof(WCHAR);
#else
		AnsiToUnicode(buf, bufW, 100);
		sizeA = strlen(buf) + 1;
		sizeW = (strlenW(bufW) + 1) * sizeof(WCHAR);
#endif
		color = CV_hili[colorcount + 1];
		colorcount = (colorcount + 1) & 7;
									// 保存する
		size = sizeof(HILIGHTKEYWORD) + sizeW +((sizeA + 1) & 0xfffe);
		ThSize(&mem, size);
		hks = (HILIGHTKEYWORD *)ThLast(&mem);
		hks->next = (HILIGHTKEYWORD *)(LONG_PTR)size;
		hks->ascii = (const char *)(size_t)(sizeW);
		hks->wide = 0;
		hks->color = color;
		hks->extend = HILIGHTKEYWORD_R;
		memcpy((char *)(hks + 1) + sizeW, strA, sizeA);
		memcpy((char *)(hks + 1), strW, sizeW);
		mem.top += size;
	}
	if ( hks != NULL ) hks->next = NULL;

	X_hkey = (HILIGHTKEYWORD *)mem.bottom;
	if ( X_hkey != NULL ){
									// ポインタを正規化する
		char *bottom;
		HILIGHTKEYWORD *hks;

		bottom = mem.bottom;
		X_hkey = hks = (HILIGHTKEYWORD *)bottom;
		for ( ;; ){
			hks->ascii = (const char *)(char *)((char *)hks + sizeof(HILIGHTKEYWORD) + (size_t)hks->ascii);
			hks->wide = (const WCHAR *)(char *)((char *)hks + sizeof(HILIGHTKEYWORD));
			if ( hks->next == NULL ) break;
			hks->next = (HILIGHTKEYWORD *)(char *)((char *)hks + (size_t)hks->next);
			hks = hks->next;
		}
	}
}

void SetHighlight(PPV_APPINFO *vinfo)
{
	VFSFILETYPE vft;

	if ( PP_ExtractMacro(vinfo->info.hWnd, &vinfo->info, NULL, T("%\"") MES_THIL T("\"*string i,Highlight=%{%s\"Highlight\"%}"), NULL, 0) != NO_ERROR ) return;

	vft.flags = VFSFT_TYPE;
	if ( VFSGetFileType(vo_.file.name, (char *)vo_.file.image, vo_.file.UseSize, &vft) != NO_ERROR ){
		vft.type[0] = '\0';
	}
	LoadHighlight(&vft);
	InvalidateRect(vinfo->info.hWnd, NULL, TRUE);
}

// テキストインデックスの補正
//void FixTextIndex(VT_TABLE *ti, DWORD_PTR offset)
void FixTextIndex(DWORD_PTR offset)
{
	VT_TABLE *ti;
	int line, maxline;

	ti = VOi->ti;
	if ( ti == NULL ) return;

	maxline = VOi->cline;
	for ( line = 0 ; line < maxline ; line++ ){
		ti->ptr += offset;
		ti++;
	}
}

void ImageRealloc(DWORD reqsize)
{
	HGLOBAL newH, oldH;
	BYTE *newImage, *oldImage;
	DWORD asize;

	oldImage = vo_.file.image;
	oldH = vo_.file.mapH;
	GlobalUnlock(oldH);

	if ( reqsize >= 0x80000000 ){
		asize = 0x80000000;
	}else{
		asize = vo_.file.ImageSize * 2;
		while ( asize < reqsize ) asize *= 2;
	}
	newH = GlobalReAlloc(oldH, asize + FILEBUFMARGIN, GMEM_MOVEABLE);
	if ( newH == NULL ){ // 確保失敗
		newH = oldH;
	}else{ // 拡張
		vo_.file.ImageSize = asize;
	}
	newImage = GlobalLock(newH);
	if ( newImage != oldImage ){ // アドレスがずれるので、補正をする
		vo_.file.image = newImage;
		VOi->img = newImage;
		vo_.file.mapH = newH;
		VOi->line = VOi->cline = 0;

		if ( vo_.DModeType == DISPT_DOCUMENT ){
			// mtinfo.MemSize
			MakeIndexTable(MIT_FIRST, MIT_PARAM_DOCUMENT);
		}else{
			mtinfo.MemSize = vo_.file.UseSize;
			MakeIndexTable(MIT_FIRST, MIT_PARAM_TEXT);
		}
	}
}

BOOL ReadData(DWORD starttime)
{
	DWORD endtime = 0;
	DWORD load = 0, loadsize;
	BYTE *rp;
	BOOL readend = FALSE;

	if ( ReadingStream != READ_STDIN ){	// ファイル
		if ( (vo_.file.ImageSize - vo_.file.UseSize) < ReadStep ){
			ReadStep = vo_.file.ImageSize - vo_.file.UseSize;
		}
		if ( vo_.file.name[0] == '#' ){
			if ( ReadStep > 0x10000 ) ReadStep = 0x10000;
		}
	}else{	// READ_STDIN 標準入力
		if ( vo_.file.UseSize == 0 ){
			ReadStep = 100;
		}else{
			if ( PeekNamedPipe(hReadStream, NULL, 0, NULL, &ReadStep, NULL) == FALSE ){
				ReadStep = 0; // 読み込み終了
			}else{
				if ( ReadStep == 0 ) return FALSE;
			}
			// メモリ再確保
			if ( (ReadStep + vo_.file.UseSize) > vo_.file.ImageSize ){
				ImageRealloc(ReadStep + vo_.file.UseSize);

				// メモリ不足のため読めるだけ読んでから終了
				if ( (ReadStep + vo_.file.UseSize) > vo_.file.ImageSize ){
					ReadStep = vo_.file.ImageSize - vo_.file.UseSize;
					readend = TRUE;
				}
			}
		}
	}
	loadsize = ReadStep;
	rp = vo_.file.image + vo_.file.UseSize;
	while( loadsize ){
		if ( ReadFile(hReadStream, rp,
				(loadsize > 0x10000 ? (loadsize / 16) : loadsize),
				&load, NULL) == FALSE ){
			load = 0;
			rp = NULL;
		}else{
			rp += load;
			vo_.file.UseSize += load;
			if ( ReadFileBreakCheck() != FALSE ){
				load = 0;
				SetPopMsg(POPMSG_NOLOGMSG, MES_BRAK);
			}
		}
		endtime = GetTickCount();
		if ( load == 0 ) break;
		loadsize -= load;
		if ( (endtime - starttime) >= READINTERVAL_FILEREAD ) break;
	}
	if ( (readend != FALSE) || (load == 0) ){ // ファイルのread完了
		if ( vo_.file.ImageSize > vo_.file.UseSize ){
			memset(vo_.file.image + vo_.file.UseSize, 0xaa, vo_.file.ImageSize - vo_.file.UseSize);
		}
		CloseHandle(hReadStream);
		hReadStream = NULL;
		ReadingStream = READ_NONE;
		readend = TRUE;
	}
	if ( rp != NULL ){
		VOi->reading = TRUE;
		memset(rp, 0, FILEBUFMARGIN);
		if ( ReadingStream != READ_STDIN ){	// ファイル
			endtime = (endtime >= starttime) ?
					(endtime - starttime) : (starttime - endtime);
			if ( endtime < READINTERVAL_FILEREAD ){
				if ( ReadStep < 10 * MB ){
					if ( endtime < (READINTERVAL_FILEREAD / 3) ){
						ReadStep *= 2;
					}else{
						ReadStep += (ReadStep >= 0x20000) ? 0x20000 : 0x1000;
					}
				}
			}else{ // 標準入力 READ_STDIN
				if ( endtime > (READINTERVAL_FILEREAD * 2) ){
					ReadStep /= 2;
				}else{
					ReadStep -= (ReadStep > 0x20000) ? 0x20000 : 0x1000;
				}
				if ( ReadStep < 0x4000 ) ReadStep = 0x4000;
			}
		}
	}
	return readend;
}

// 別スレッドに改造するときは、同時に MakeIndexTable を実行しないように
// する必要がある
#pragma argsused
void CALLBACK BackReaderProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	DWORD starttime, endtime;
	MSG msgdata;
	UnUsedParam(uMsg);UnUsedParam(idEvent);UnUsedParam(dwTime);

	if ( ReadEnter ) return;
	ReadEnter++;
	starttime = GetTickCount();
	if ( ReadingStream != READ_NONE ){ // ファイルの読み込み
		if ( ReadData(starttime) == FALSE ){	// 読み込み中
			mtinfo.MemSize = vo_.file.UseSize;
		}else{						// 読み込み完了
			DWORD olddt;

			olddt = vo_.DModeType;
			CheckType();
			if ( vo_.DModeType != olddt ){
				VOi->img = NULL;
				CheckType();
			}
			SetScrollBar();
			if ( XV_tmod && (vo_.DModeBit & VO_dmode_SELECTABLE) ){
				InitCursorMode(hWnd, FALSE);
			}
			InvalidateRect(hWnd, NULL, FALSE);
		}
		if ( vo_.DModeBit == DOCMODE_HEX ){
			VOi->line = ((vo_.file.UseSize + 15) >> 4);
		}
	}

	if ( VOi->reading != FALSE ){ // 行数計算
		ReadDisp++;
		if ( vo_.DModeBit != DOCMODE_TEXT ){
			VOi->reading = FALSE;
		}else{
			int readunit, readcount;

			readunit = (CountReadLines <= (READLINE_TIMERMIN * 8)) ?
					CountReadLines : (CountReadLines / 16);
			readcount = 0;
			for ( ;; ){
				MakeIndexTable(MIT_NEXT, readunit);
				if ( VOi->reading == FALSE ) break;
				if ( PeekMessage(&msgdata, NULL, 0, 0, PM_NOREMOVE) ){
					if ( msgdata.message != WM_TIMER ){
						goto fin;
					}
				}
				readcount += readunit;
				endtime = GetTickCount();
				endtime = (endtime >= starttime) ?
						(endtime - starttime) : (starttime - endtime);
				if ( endtime >= (READINTERVAL_FILEREAD + READINTERVAL_LINECOUNT) ){
					CountReadLines = readcount;
					break;
				}
			}

			if ( ReadingStream == READ_STDIN ){
				MoveCsr(0, VOi->line, FALSE);
				ReadDisp = READLINE_DISPS;
			}

			// 直前の改行まで巻き戻し
			{
				int count = 5;
				while ( (VOi->cline > 0) &&
						!(VOi->ti[VOi->cline].attrs & VTTF_TOP) ){
					VOi->cline--;
					count--;
					if ( count == 0 ) break;
				}
			}
		}
	}
	// ファイル読み込みが終了したとき、指定行へジャンプする
	if ( (VOi->reading == FALSE) && (ReadingStream == READ_NONE) ){
		KillTimer(vinfo.info.hWnd, TIMERID_READLINE);
		BackReader = FALSE;
		mtinfo.PresetPos = 0;
		MoveCsr(0, mtinfo.PresetY - VOi->offY, FALSE);
		if ( VOi->offY < mtinfo.PresetY ){
		// 移動し切れていない場合はやり直し
		// ※ MoveCsr 内の SetScrollBar で VO_maxY が再設定されることがある為
			MoveCsr(0, mtinfo.PresetY - VOi->offY, FALSE);
		}
		mtinfo.OpenFlags = 0;
	}
	// ファイル読み込み、指定行表示も完了
	if ( (ReadDisp >= READLINE_DISPS) || (BackReader == FALSE) ){
		ReadDisp = 0;
		InvalidateRect(hWnd, NULL, FALSE);	// 更新指定
		SetScrollBar();
		LoadEvent();
		#if COUNTREADTIME
		{
			TCHAR buf[20];
			wsprintf(buf, T("Time:%d"), GetTickCount() - readstartTick);
			SetPopMsg(POPMSG_NOLOGMSG, buf);
		}
		#endif
	}
fin:
	ReadEnter--;
}

void SetAllTextCode(int codetype)
{
	VO_I[DISPT_HEX].textC = VO_I[DISPT_TEXT].textC = VO_I[DISPT_DOCUMENT].textC = codetype;
}

//---------------------------------------------------------------- 固有の初期化
int VD_emf(void)
{
	SetAllTextCode(VTYPE_SYSTEMCP);
	if ( vo_.eMetafile.handle != NULL ) return 0;
	vo_.eMetafile.handle = SetEnhMetaFileBits(vo_.file.UseSize, vo_.file.image);
	if ( vo_.eMetafile.handle != NULL ){
		ENHMETAHEADER metah;

		vo_.SupportTypeFlags = VO_type_ALLDOCUMENT;
		VOi->img = vo_.file.image;
		vo_.bitmap.ShowInfo = &temp_DIB.bmiHeader;
		vo_.bitmap.ShowInfo->biWidth = 100;
		vo_.bitmap.ShowInfo->biHeight = 100;
		if ( GetEnhMetaFileHeader(vo_.eMetafile.handle, sizeof(metah), &metah) ){
			vo_.bitmap.ShowInfo->biWidth = metah.rclBounds.right - metah.rclBounds.left;
			vo_.bitmap.ShowInfo->biHeight = metah.rclBounds.bottom - metah.rclBounds.top;

			vo_.bitmap.ShowInfo->biXPelsPerMeter = metah.rclBounds.left;
			vo_.bitmap.ShowInfo->biYPelsPerMeter = metah.rclBounds.top;

			VOi->width = metah.rclBounds.right / fontX + 1;
			VOi->line = metah.rclBounds.bottom / LineY + 1;

		}
		vo_.bitmap.rawsize.cx = vo_.bitmap.ShowInfo->biWidth;
		vo_.bitmap.rawsize.cy = vo_.bitmap.ShowInfo->biHeight;
		FixBitmapShowSize(&vo_);

		vo_.DocmodeType = DOCMODE_EMETA;
		if ( GetEnhMetaFilePaletteEntries(vo_.eMetafile.handle, 0, NULL) ){
			struct {
				WORD palVersion;
				WORD palNumEntries;
				PALETTEENTRY palPalEntry[256];
			} lPal;

			lPal.palVersion = 0x300;
			lPal.palNumEntries = (WORD)GetEnhMetaFilePaletteEntries(
					vo_.eMetafile.handle, 256, (PALETTEENTRY *)&lPal.palPalEntry);
			vo_.bitmap.hPal = CreatePalette((CONST LOGPALETTE *)&lPal);
		}
	}else{
		vo_.DModeType = DISPT_TEXT;
	}
	return 0;
}
//-------------------------------------
int VD_c(void)
{
	if ( VOi->tab == 8 ) VOi->tab = 4;
	return -1;
}
//-------------------------------------
int VD_systemcp(void)
{
	SetAllTextCode(VTYPE_SYSTEMCP);
	return -1;
}
//-------------------------------------
int VD_jis(void)
{
	VOi->width = 76;
	SetAllTextCode(VTYPE_JIS);
	return -1;
}
//-------------------------------------
int VD_unicode(void)
{
	SetAllTextCode(VTYPE_UNICODE);
	return -1;
}
//-------------------------------------
int VD_unicodeB(void)
{
	SetAllTextCode(VTYPE_UNICODEB);
	return -1;
}
//-------------------------------------
int VD_html(void)
{
	VO_Ttag = 1;
	return -1;
}
//-------------------------------------
int VD_xml(void)
{
	VO_Ttag = 2;
	return -1;
}
//-------------------------------------
int VD_officezip(void)
{
	SetAllTextCode(VTYPE_UTF8);
	VO_Ttag = 1;
	X_tlen = 1100;
	return -1;
}
//-------------------------------------
int VD_u_html(void)
{
	SetAllTextCode(VTYPE_UNICODE);
	VO_Ttag = 2;
	return -1;
}
//-------------------------------------
int VD_quoted(void)
{
	VO_Tquoted = 1;
	return -1;
}
//-------------------------------------
int VD_rtf(void)
{
	SetAllTextCode(VTYPE_RTF);
	return -1;
}
//-------------------------------------
int VD_word7(void)
{
	DWORD off;

	off = 0x200 + *(DWORD *)(vo_.file.image + 0x218);
	if ( off >= vo_.file.UseSize ) off = 0;

	vo_.SupportTypeFlags = VO_type_ALLDOCUMENT;
	SetAllTextCode(VTYPE_SJISB);
	VOi->img = vo_.file.image + off;

	if ( (off + 8) < vo_.file.UseSize ){
		if( !memcmp(VOi->img, "\x79\x81\x91\x8f\xde\x97\xbc\x96", 8) ){
			VOi->width = 72;
		}
	}

	mtinfo.MemSize = vo_.file.UseSize - off;
	MakeIndexTable(MIT_FIRST, MIT_PARAM_DOCUMENT);
	return 0;
}
//-------------------------------------
int VD_word8(void)
{
	int i, type = VTYPE_SYSTEMCP;
	DWORD off;
	BYTE *p;

	off = 0x200 + *(DWORD *)(vo_.file.image + 0x218);
	if ( off >= vo_.file.UseSize ) off = 0;
	if ( !*(DWORD *)(vo_.file.image + off) ) off += 0x200;

	p = vo_.file.image + off;
	for ( i = 0 ; i < 32 ; i++ ){
		if ( !*p || (*p & 0x80) ){
			type = VTYPE_UNICODE;
			break;
		}
		p++;
	}

	vo_.SupportTypeFlags = VO_type_ALLDOCUMENT;
	SetAllTextCode(type);
	VOi->img = vo_.file.image + off;

	mtinfo.MemSize = vo_.file.UseSize - off;
	MakeIndexTable(MIT_FIRST, MIT_PARAM_DOCUMENT);
	return 0;
}
//-------------------------------------
int VD_oasys(void)
{
	BYTE *p;

	vo_.SupportTypeFlags = VO_type_ALLDOCUMENT;
	if ( VO_textS[VTYPE_SYSTEMCP] == textcp_sjis ){
		SetAllTextCode(VTYPE_SYSTEMCP);
	}else{
		VO_CodePage = CP__SJIS;
		VO_CodePageValid = IsValidCodePage(VO_CodePage);
		SetAllTextCode(VTYPE_OTHER);
	}
	VOi->MakeText = VD_oasys_mdt;

	if ( vo_.file.UseSize > 0x7d0 ){
		p = (BYTE *)vo_.file.image + 0x7d0;
	}else{
		p = (BYTE *)vo_.file.image;
	}
	for( ;; ){
		if ( *p++ == 0xf0 ){
			if ( *p++ == 0x04 ) break;
		}
	}
	VOi->img = p + 2;

	mtinfo.MemSize = vo_.file.UseSize - (VOi->img - vo_.file.image);
	MakeIndexTable(MIT_FIRST, MIT_PARAM_DOCUMENT);
	return 0;
}
//-------------------------------------
int VD_unitext(void)
{
	vo_.SupportTypeFlags = VO_type_ALLDOCUMENT;
	SetAllTextCode(VTYPE_UNICODE);
	VOi->img = vo_.file.image + 2;

	mtinfo.MemSize = vo_.file.UseSize - 2;
	MakeIndexTable(MIT_FIRST, MIT_PARAM_DOCUMENT);
	return 0;
}
//-------------------------------------
int VD_unitextB(void)
{
	vo_.SupportTypeFlags = VO_type_ALLDOCUMENT;
	SetAllTextCode(VTYPE_UNICODEB);
	VOi->img = vo_.file.image + 2;

	mtinfo.MemSize = vo_.file.UseSize - 2;
	MakeIndexTable(MIT_FIRST, MIT_PARAM_DOCUMENT);
	return 0;
}
//-------------------------------------
int VD_utf8(void)
{
	vo_.SupportTypeFlags = VO_type_ALLDOCUMENT;
	SetAllTextCode(VTYPE_UTF8);
	VOi->img = vo_.file.image + 3;

	mtinfo.MemSize = vo_.file.UseSize - 3;
	MakeIndexTable(MIT_FIRST, MIT_PARAM_DOCUMENT);
	return 0;
}
//-------------------------------------
int VD_text(void)
{
	return -1;
}

#define CODEFUNCMAX 19
typedef struct {
	int (*func)(void);
} VD_CODEFUNCINIT;

const VD_CODEFUNCINIT Vd_codefuncinit[CODEFUNCMAX] = {
	{VD_systemcp},	{VD_jis},		{VD_unicode},	{VD_html},
	{VD_rtf},		{VD_word7},		{VD_oasys},		{VD_unitext},
	{VD_unicodeB},	{VD_unitextB},	{VD_utf8},		{VD_c},
	{VD_emf},		{VD_word8},		{VD_quoted},	{VD_xml},
	{VD_u_html},	{VD_officezip},	{VD_text}
};

#define IOCTL_CDROM_GET_DRIVE_GEOMETRY 0x0002404C

// ファイルイメージを取得する--------------------------------------------------
#define VOPENFILE_
HANDLE vOpenFile(const TCHAR *filename, const TCHAR **wp, const TCHAR **dllp)
{
	TCHAR buf[VFPS], *fp, *separator = NULL;
	HANDLE hFile;
	int openmode;
	BOOL rawsize = FALSE;
	TCHAR *vp, drive;
	int depth = 0;

	tstrcpy(buf, filename);
	vp = VFSGetDriveType(buf, &openmode, NULL);
	if ( vp == NULL ){		// 種類が分からない→相対指定の可能性→絶対化
		VFSFullPath(NULL, buf, NULL);
		vp = VFSGetDriveType(buf, &openmode, NULL);
		if ( vp == NULL ) goto patherror; // それでも種類が分からない→エラー
	}
										// 種類別の処理 -----------------------
	switch (openmode){
		case VFSPT_RAWDISK:		// "#A:" を "\\.\A:" に変換
			vp -= 2;
			drive = *vp;
			wsprintf(buf, T("\\\\.\\%c:"), drive);
			rawsize = TRUE;
			break;

		case VFSPT_DRIVELIST:
			goto patherror;

		default:
			if ( (openmode == VFSPT_SHELLSCHEME) || (openmode <= VFSPT_SHN_DESK) ){
				if ( FALSE == VFSGetRealPath(vinfo.info.hWnd, buf, filename) ){
					tstrcpy(buf, filename);
				}
			}
			break;
	}

	for ( ;; ){
		DWORD attr;

		fp = VFSFindLastEntry(buf);
		if ( rawsize != FALSE ){
			attr = 0;
		}else{
			attr = GetFileAttributesL(buf);
		}
		if ( attr != BADATTR ){
			if ( attr & FILE_ATTRIBUTE_DIRECTORY ){
				if ( depth == 0 ){
					if ( *fp == '\\' ) fp++;
					wsprintf(vo_.file.typeinfo, T("%s is directory."), fp);
					return NULL;
				}else{
					SetLastError(ERROR_FILE_NOT_FOUND);
				}
			}else{
				hFile = CreateFileL(buf, GENERIC_READ,
						FILE_SHARE_WRITE | FILE_SHARE_READ, NULL,
						OPEN_EXISTING,
						FILE_FLAG_SEQUENTIAL_SCAN,
// GENERIC_READ のときは、FILE_FLAG_POSIX_SEMANTICS が効かない？
//						FileCase ?
//							(FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_POSIX_SEMANTICS) : FILE_FLAG_SEQUENTIAL_SCAN,
						NULL);
				if ( hFile != INVALID_HANDLE_VALUE ){
					if ( (separator != NULL) &&
						 (separator != INVALID_HANDLE_VALUE) &&
						 ((fp = FindPathSeparator(separator)) != NULL) ){
						*fp = '\0';
					}else{
						fp = buf + tstrlen(buf);
					}
					break;
				}
			}
			goto accesserror;
		}else{
			if ( separator == NULL ){
				separator = tstrrchr(buf, ':'); // "::" を検索
				if ( (separator != NULL) && (separator >= (buf + 2)) &&
					 (*(separator - 1) == ':') ){
					*(separator - 1) = '\0';
					*dllp = filename + (separator - buf) - 1;
					continue;
				}
			}
			separator = INVALID_HANDLE_VALUE;
			if ( (openmode == VFSPT_SHELLSCHEME) || (openmode <= VFSPT_SHN_DESK) ){
				ERRORCODE result;
				TCHAR *textimage = NULL;
				DWORD memsize;

				result = VFSLoadFileImage(buf , 0, (char **)&textimage, &memsize, NULL);
				if ( result == NO_ERROR ){
					if ( (vo_.file.mapH = GlobalAlloc(GMEM_MOVEABLE,
							memsize + FILEBUFMARGIN)) != NULL ){
						if ( (vo_.file.image = GlobalLock(vo_.file.mapH)) == NULL ){
							GlobalFree(vo_.file.mapH);
							VO_error(PPERROR_GETLASTERROR);
						}else{
							memcpy(vo_.file.image, textimage, memsize);
							memset(vo_.file.image + memsize, 0, FILEBUFMARGIN);
							tstrcpy(vo_.file.source, filename);
							FileDivideMode = FDM_FORCENODIV;

							vo_.file.ImageSize = memsize;
							FileRealSize.LowPart = vo_.file.UseSize = memsize;
							FileRealSize.HighPart = 0;
							return INVALID_HANDLE_VALUE;
						}
					}
				}
			}
		}
		if ( *fp == '\0' ) goto accesserror;
		*fp = '\0';
		depth++;
	}

	if ( rawsize ){	// #x: でファイルサイズが求められないので、別の方法で決定
		DISK_GEOMETRY diskinfo;
		DWORD tmp;

		*wp = filename + (vp - buf) + 2;

		// 始めにCD-ROMをチェックし、無ければDISKのチェック
		if ( (FALSE != DeviceIoControl(hFile, IOCTL_CDROM_GET_DRIVE_GEOMETRY,
						NULL, 0, &diskinfo, sizeof(DISK_GEOMETRY), &tmp, NULL)) ||
			 (FALSE != DeviceIoControl(hFile, IOCTL_DISK_GET_DRIVE_GEOMETRY,
						NULL, 0, &diskinfo, sizeof(DISK_GEOMETRY), &tmp, NULL)) ){
			DWORD tracksize, tmpL, tmpH;

			tracksize = diskinfo.TracksPerCylinder *
				diskinfo.SectorsPerTrack * diskinfo.BytesPerSector;

			DDmul(tracksize, diskinfo.Cylinders.u.LowPart,
					&FileRealSize.LowPart, &FileRealSize.HighPart);
			DDmul(tracksize, diskinfo.Cylinders.u.HighPart, &tmpL, &tmpH);
			FileRealSize.HighPart += tmpL;
		}else{
			FileRealSize.LowPart = 200 * MB;
			FileRealSize.HighPart = 0;
		}
	}else{
		ERRORCODE result;

		*wp = filename + (fp - buf);
		FileRealSize.LowPart = GetFileSize(hFile, &FileRealSize.HighPart);
		if ( (FileRealSize.LowPart == MAX32) && ((result = GetLastError()) != NO_ERROR) ){
			CloseHandle(hFile);
			VO_error(result);
			return NULL;
		}
		vo_.file.IsFile = 1;
	}
	vo_.file.UseSize = FileRealSize.LowPart;
	return hFile;
patherror:
	VO_error(ERROR_BAD_PATHNAME);
	return NULL;
accesserror:
	VO_error(PPERROR_GETLASTERROR);
	return NULL;
}

BOOL ReadFileBreakCheck(void)
{
	MSG msgdata;

	if ( PeekMessage(&msgdata, NULL, 0, 0, PM_NOREMOVE) == FALSE ) return FALSE;
	if ( (msgdata.message == WM_QUIT) || (msgdata.message == WM_CLOSE) ){
		return TRUE;
	}
	if ( (msgdata.message == WM_RBUTTONUP) ||
		 ((msgdata.message == WM_KEYDOWN) &&
		  (((int)msgdata.wParam == VK_ESCAPE)||((int)msgdata.wParam == VK_PAUSE))) ){
		if ( PMessageBox(vinfo.info.hWnd, MES_QABO, T("Break check"),
					MB_APPLMODAL | MB_OKCANCEL | MB_DEFBUTTON1 |
					MB_ICONQUESTION) == IDOK){
			return TRUE;
		}
	}
	return FALSE;
}

DWORD ReadAll(void)
{
	DWORD load, step = 1 * MB;
	BYTE *rp;

	if ( FileDivideMode >= FDM_DIV ) return 1; // 分割時は、全て読めない
	if ( ReadingStream == READ_NONE ) return vo_.file.UseSize;
	rp = vo_.file.image + vo_.file.UseSize;

	SetFilePointer(hReadStream, vo_.file.UseSize, NULL, FILE_BEGIN);
	for ( ;; ){
		if ( (vo_.file.ImageSize - vo_.file.UseSize) < step ){
			step = vo_.file.ImageSize - vo_.file.UseSize;
		}
		if ( ReadFile(hReadStream, rp, step, &load, NULL) == FALSE ){
			if ( ReadingStream == READ_STDIN ) break;
			PPErrorBox(NULL, NULL, PPERROR_GETLASTERROR);
			return 0;
		}
		if ( load == 0 ) break;
		rp += load;
		vo_.file.UseSize += load;
		if ( ReadFileBreakCheck() != FALSE ){
			SetPopMsg(POPMSG_NOLOGMSG, MES_BRAK);
			break;
		}
	}
	CloseHandle(hReadStream);
	if ( vo_.file.ImageSize > vo_.file.UseSize ){
		memset(rp, 0xaa, vo_.file.ImageSize - vo_.file.UseSize);
	}
	memset(rp, 0, FILEBUFMARGIN);
	hReadStream = NULL;
	ReadingStream = READ_NONE;
	return vo_.file.UseSize;
}

BOOL OpenViewHttp(const TCHAR *filename)
{
	ThSTRUCT th;
	char *bottom, *ptr;
	DWORD datasize;

	if ( GetImageByHttp(filename, &th) == FALSE ) return FALSE;
	bottom = th.bottom;
	datasize = th.top - 1;

	ptr = strstr(bottom, "\r\n\r\n");
	if ( (ptr != NULL) && (*(ptr + 4) != '\0') ){
		datasize -= ptr - bottom + 4;
		bottom = ptr + 4;
	}
	if ( (vo_.file.mapH = GlobalAlloc(GMEM_MOVEABLE, datasize + FILEBUFMARGIN)) == NULL ){
		VO_error(PPERROR_GETLASTERROR);
		return FALSE;
	}
	if ( (vo_.file.image = GlobalLock(vo_.file.mapH)) == NULL ){
		GlobalFree(vo_.file.mapH);
		VO_error(PPERROR_GETLASTERROR);
		return FALSE;
	}
	memcpy(vo_.file.image, bottom, datasize);
	*bottom = '\0';
	ThCatStringA(&vo_.memo, th.bottom);
	ThFree(&th);
	memset(vo_.file.image + datasize, 0, FILEBUFMARGIN);

	vo_.file.ImageSize = datasize;
	FileRealSize.LowPart = vo_.file.UseSize = datasize;
	FileRealSize.HighPart = 0;
	tstrcpy(vo_.file.source, filename);
	return TRUE;
}

// ファイル読み込みメイン -----------------------------------------------------
BOOL OpenViewFile(const TCHAR *filename, int flags)
{
	HANDLE hFile;
	DWORD size;
	const TCHAR *wp, *dllp = NULL;
	DWORD starttime, tick;

	{
		TCHAR tmppath[VFPS], *p, *popt;

		p = VFSFindLastEntry(filename);
		popt = tstrstr(p, T("::"));
		if ( popt != NULL ){
			tstrcpy(tmppath, filename);
			tmppath[popt - filename] = '\0';
			filename = tmppath;
		}
	}

	if ( !memcmp(filename, httpstr, SIZEOFTSTR(httpstr)) ||
		 !memcmp(filename, httpsstr, SIZEOFTSTR(httpsstr)) ){
		return OpenViewHttp(filename);
	}
										// ファイルを開く
	hFile = vOpenFile(filename, &wp, &dllp);
	if ( hFile == NULL ) return FALSE;
	if ( hFile == INVALID_HANDLE_VALUE ) return TRUE; // shell: のときは内部で
	GetCustData(T("X_wsiz"), &X_wsiz, sizeof X_wsiz);
	if ( !(flags & PPV__reload) ){ // 新規なら分割モードを初期化
		if ( (FileRealSize.HighPart != 0) || (FileRealSize.LowPart >= X_wsiz) ){
			const TCHAR *vp;

			UsePPx();
			vp = SearchHistory(PPXH_PPVNAME, filename);
			if ( vp != NULL ){
				int datasize = GetHistoryDataSize(vp) - sizeof(int) * 3;

				if ( datasize >= 10 ){
					const BYTE *bp;

					bp = (const BYTE *)GetHistoryData(vp) + sizeof(int) * 3;
					if ( (BYTE)*(bp + 1) == HISTOPTID_FileDIV ){
						FileDividePointer = *(DDWORDST *)(bp + 2);
					}else if ( ((BYTE)*(bp + 1) == HISTOPTID_OLDTAIL) &&
							   (datasize >= HISTOPTSIZE_OLDTAIL_L) ){
						FileDividePointer = *(DDWORDST *)(bp + 6);
					}
					if ( (FileDividePointer.HighPart > FileRealSize.HighPart) ||
						((FileDividePointer.HighPart == FileRealSize.HighPart) &&
						 (FileDividePointer.LowPart >= FileRealSize.LowPart) ) ){
						FileDividePointer.LowPart = FileDividePointer.HighPart = 0;
					}
				}
			}
			FreePPx();
		}
	}

	if ( (*wp != '\0') &&
		 !((*wp == '\\') && (*(wp + 1) == '\0')) ){
		TCHAR arcfile[VFPS];
		DWORD result;
		HWND hDWnd;
		DWORD X_llsiz = 2;
															// ファイルだった
		tstrcpy(arcfile, filename);
		arcfile[wp - filename] = '\0';
		GetCustData(T("X_llsiz"), &X_llsiz, sizeof(X_llsiz));
		hDWnd = vinfo.info.hWnd;
		if ( (X_llsiz == 1) || (FileDivideMode == FDM_FORCENODIV) ){
			X_llsiz = 1;
			hDWnd = (HWND)LFI_ALWAYSLIMITLESS;
		}else if ( X_llsiz == 2 ){
			hDWnd = (HWND)LFI_ALWAYSLIMIT;
		}
		result = VFSGetArchivefileImage(hDWnd, hFile, arcfile, wp,
				&FileRealSize.LowPart, &FileRealSize.HighPart,
				&vo_.file.mapH, &vo_.file.image);
//		CloseHandle(hFile); // VFSGetArchivefileImage 内で閉じている
		if ( result != NO_ERROR ){
			VO_error(result);
			return FALSE;
		}else{
			if ( (X_llsiz == 2) && (FileRealSize.LowPart == X_wsiz) ){
				FileDivideMode = FDM_DIV;
			}
			vo_.file.UseSize = FileRealSize.LowPart;
			return TRUE;
		}
	}
//------------------------------------- ファイルの大きさを制限する
	vo_.file.UseSize = FileRealSize.LowPart;
	if ( FileRealSize.HighPart > 0 ){
		if ( FileDivideMode < FDM_NODIVMAX ) FileDivideMode = FDM_DIV;
		vo_.file.UseSize = MAX32;	// 丸め込み1
		if ( FileDividePointer.LowPart | FileDividePointer.HighPart ){
			FileDivideMode = FDM_DIV2ND;
		}
	}

	if ( flags & PPV_HEADVIEW ){
		DWORD X_svsz = 2097152;

		GetCustData(T("X_svsz"), &X_svsz, sizeof(X_svsz));
		if ( vo_.file.UseSize > X_svsz ){
			vo_.file.UseSize = X_svsz;
			FileDivideMode = FDM_DIV;
		}
	}

	if ( vo_.file.UseSize > X_wsiz ){	// 丸め込み2
		int result;

		if ( FileDivideMode == FDM_NODIV ){
			FileDivideMode = FDM_DIV;
		}
		if ( FileDividePointer.LowPart | FileDividePointer.HighPart ){
			FileDivideMode = FDM_DIV2ND;
		}

		if ( (FileDivideMode >= FDM_DIV2ND) ||
			 FileRealSize.HighPart ||
			 (FileRealSize.LowPart >= ((DWORD)1 * GB)) ){
			result = IDYES;
		}else if ( FileDivideMode == FDM_FORCENODIV ){
			result = IDNO;
		}else{
			HWND hOldFocusWnd;
			DWORD X_llsiz = 2;

			GetCustData(T("X_llsiz"), &X_llsiz, sizeof(X_llsiz));
			if ( X_llsiz <= 3 ){
				result = X_llsizTable[X_llsiz];
			}else{
				result = 0;
			}
			if ( result == 0 ){
				hOldFocusWnd = GetForegroundWindow();
				ForceSetForegroundWindow(vinfo.info.hWnd);
				result = PMessageBox(vinfo.info.hWnd, MES_QOSL, T("Warning"),
						MB_ICONEXCLAMATION | MB_YESNOCANCEL);
				if ( hOldFocusWnd != vinfo.info.hWnd ){
					ForceSetForegroundWindow(hOldFocusWnd);
				}
			}
		}
		if ( result == IDYES ){
			DDWORDST temp;

			vo_.file.UseSize = X_wsiz; // 分割

			temp = FileRealSize;
			SubDD(temp.LowPart, temp.HighPart, FileDividePointer.LowPart, FileDividePointer.HighPart);
			if ( (temp.HighPart == 0) && (temp.LowPart < X_wsiz) ){
				vo_.file.UseSize = temp.LowPart;
			}
		}else if ( result != IDNO ){ // 中止
			VO_error(PPERROR_GETLASTERROR);
			CloseHandle(hFile);
			return FALSE;
		}else{
			FileDivideMode = FDM_NODIV;
			FileDividePointer.LowPart = FileDividePointer.HighPart = 0;
		}
	}

	if ( FileDivideMode >= FDM_DIV ){
		SetFilePointer(hFile, FileDividePointer.LowPart, (LONG *)&FileDividePointer.HighPart, FILE_BEGIN);
	}
	while ( (vo_.file.mapH = GlobalAlloc(GMEM_MOVEABLE, vo_.file.UseSize + FILEBUFMARGIN)) == NULL){
		if ( vo_.file.UseSize > 200 * MB ){
			vo_.file.UseSize = 200 * MB;
			continue;
		}
		if ( vo_.file.UseSize > 10 * MB ){
			vo_.file.UseSize = 10 * MB;
			continue;
		}
		VO_error(PPERROR_GETLASTERROR);
		CloseHandle(hFile);
		return FALSE;
	}
	if ( (vo_.file.image = GlobalLock(vo_.file.mapH)) == NULL ){
		VO_error(PPERROR_GETLASTERROR);
		CloseHandle(hFile);
		return FALSE;
	}
	// ヘッダ取得
	memset( vo_.file.image + vo_.file.UseSize, 0, FILEBUFMARGIN);
	vo_.file.ImageSize = vo_.file.UseSize;
	size = min(vo_.file.UseSize, 0x1000);

	starttime = GetTickCount();
	if ( ReadFile(hFile, vo_.file.image, size, &vo_.file.UseSize, NULL) == FALSE ){
		VO_error(PPERROR_GETLASTERROR);
		CloseHandle(hFile);
		return FALSE;
	}
	if ( vo_.file.ImageSize > vo_.file.UseSize ){	// 最初は多めにファイルを読み込む
		tick = GetTickCount();
		ReadStep = 0x20000;
		ReadingStream = READ_FILE;
		hReadStream = hFile;
		ReadEnter++;
		if ( (tick - starttime) < READINTERVAL_FILEREAD ) ReadData(tick);
		ReadEnter--;
	}else{
		CloseHandle(hFile);
	}
	if ( vo_.file.image != NULL ){
		memset(vo_.file.image + vo_.file.UseSize, 0, FILEBUFMARGIN);
		return TRUE;
	}else{
		return FALSE;
	}
}
// ヒストリからカーソル位置などを取得 -----------------------------------------
void PPVGetHist(void)
{
	const TCHAR *vp;

	VOsel.now.y.line = -1; // 未初期化指定
	if ( IsTrue(vo_.file.memdata) ) return;

	UsePPx();
	vp = SearchHistory(PPXH_PPVNAME, vo_.file.name);
	if ( vp != NULL ){
		int datasize = GetHistoryDataSize(vp) - sizeof(int) * 3;
		if ( datasize >= 0 ){
			const BYTE *bp;

			bp = (const BYTE *)GetHistoryData(vp);
			if ( *bp == (BYTE)vo_.SupportTypeFlags ){ // 表示形式が一致していると思われるなら利用
				vo_.DModeType = *(bp + 1);
				VOi = &VO_I[vo_.DModeType];
				if ( vo_.DModeType == DISPT_IMAGE ){
					vo_.bitmap.rotate = *(bp + 3);
				}else{
					BYTE bpflags;

					VOi->textC = *(bp + 2);
					bpflags = *(bp + 3);
					if ( bpflags & HISTOPT_TEXTOPT_QUOTED ){
						VO_Tquoted = 1;
					}
					if ( bpflags & HISTOPT_TEXTOPT_ESC ){
						VO_Tesc = 0;
					}
					if ( bpflags & HISTOPT_TEXTOPT_TAGMASK ){
						VO_Ttag = (bpflags >> HISTOPT_TEXTOPT_TAGSHIFT) - 1;
					}
				}
				if ( !(mtinfo.OpenFlags & PPV__NoGetPosFromHist) ){
					VOi->offX = *(int *)(bp + 4);
					VOi->offY = *(int *)(bp + 8);
				}
				bp += sizeof(int) * 3;
				while ( datasize > 0 ){
					BYTE len;

					if ( (BYTE)*(bp + 1) <= (HISTOPTID_BookmarkMin + MaxBookmark) ){
						int bkindex = (BYTE)*(bp + 1) - HISTOPTID_BookmarkMin;
						if ( *bp == HISTOPTSIZE_Bookmark ){
							Bookmark[bkindex].pos = *(POINT *)(bp + 2);
						}else if ( *bp == HISTOPTSIZE_Bookmark_L ){
							Bookmark[bkindex] = *(BookmarkInfo *)(bp + 2);
						}
						setflag(ShowTextLineFlags, SHOWTEXTLINE_BOOKMARK);
					}else if ( (BYTE)*(bp + 1) == HISTOPTID_OLDTAIL ){
						TailModeFlags = 1;
						OldTailLine = *(int *)(bp + 2);
						setflag(ShowTextLineFlags, SHOWTEXTLINE_OLDTEXT);
					}else if ( (BYTE)*(bp + 1) == HISTOPTID_OTHERCP ){
						VO_CodePage = (int)*(WORD *)(bp + 2);
						VO_CodePageValid = IsValidCodePage(VO_CodePage);
					}else if ( (BYTE)*(bp + 1) == HISTOPTID_WIDTH ){
						VOi->width = *(int *)(bp + 2);
					}else if ( (BYTE)*(bp + 1) == HISTOPTID_CARET ){
						VOsel.now.x.offset = *(int *)(bp + 2);
						VOsel.now.y.line = *(int *)(bp + 6);
					}
					len = (BYTE)*bp;
					if ( len == 0 ) break;
					datasize -= len;
					bp += len;
				}
			}
		}
	}
	FreePPx();
	if ( !(mtinfo.OpenFlags & PPV__NoGetPosFromHist) ){
		mtinfo.PresetY = VOi->offY;
	}
}

void SetOpts(VIEWOPTIONS *viewopts)
{
	if ( (viewopts->dtype >= 0) &&
		 ((vo_.SupportTypeFlags >> (viewopts->dtype - 1)) & LSBIT) ){
		vo_.DModeType = viewopts->dtype;
		VOi = &VO_I[vo_.DModeType];

		if ( vo_.DModeType == DISPT_RAWIMAGE ){
			VOi->width = VOi->defwidth = -1;
			VOi->textC = 32; // 32bit (R10G10B10) / R5G6B5
		}
	}
	if ( viewopts->T_code >= 0 ){
		VOi->textC = viewopts->T_code;
		if ( VOi->textC >= VTYPE_MAX ){
			VO_CodePage = VOi->textC;
			VO_CodePageValid = IsValidCodePage(VO_CodePage);
			VOi->textC = VTYPE_OTHER;
		}
	}
	if ( viewopts->T_siso >= 0 ) VO_Tmode = viewopts->T_siso;
	if ( viewopts->T_esc  >= 0 ) VO_Tesc = viewopts->T_esc;
	if ( viewopts->T_tag  >= 0 ) VO_Ttag = viewopts->T_tag;
	if ( viewopts->T_show_css  >= 0 ) VO_Tshow_css = viewopts->T_show_css;
	if ( viewopts->T_show_script  >= 0 ) VO_Tshow_script = viewopts->T_show_script;
	if ( viewopts->T_tab  >= 1 ) VOi->tab = viewopts->T_tab;
	if ( viewopts->T_width >= 0 ){
		VOi->defwidth = viewopts->T_width - VIEWOPTIONS_WIDTHOFFSET;
		VOi->width = FixedWidthRange(VOi->defwidth);
	}
	if ( (viewopts->I_animate >= 0) && (vo_.bitmap.page.animate != viewopts->I_animate) ){
		vo_.bitmap.page.animate = viewopts->I_animate;
		if ( vo_.bitmap.page.animate == FALSE ){
			KillTimer(vinfo.info.hWnd, TIMERID_ANIMATE);
		}
	}
	if ( viewopts->tailflags >= 0 ){
		TailModeFlags = (DWORD)viewopts->tailflags;
	}

	if ( viewopts->linespace >= 0 ){
		LineY = fontY + viewopts->linespace;
		if ( LineY < 1 ) LineY = 1;
	}
}

// 表示準備 -------------------------------------------------------------------
void InitViewObject(VIEWOPTIONS *viewopts, TCHAR *type)
{
	VOi = &VO_I[vo_.DModeType];
	VOi->reading = TRUE;
	VO_Tmode = VO_Tmodedef;
	VO_CodePageChanged = FALSE;
	ScrollWidth = ScrollWidth_MIN;

	vo_.DModeBit = DOCMODE_NONE; // バグ取り用

	if ( type != NULL ){ // 初めての読み込みなので設定取得
		TCHAR optbuf[CMDLINESIZE];
		VIEWOPTIONS viewo;

		if ( NO_ERROR == GetCustTable(T("XV_opts"), type, &optbuf, sizeof(optbuf)) ){
			CheckParam(&viewo, optbuf, NULL);
			SetOpts(&viewo);
		}else{
			TCHAR *ext;

			ext = VFSFindLastEntry(vo_.file.name);
			ext += FindExtSeparator(ext);
			if ( *ext != '\0' ) ext++;
			if ( NO_ERROR == GetCustTable(T("XV_opts"), ext, &optbuf, sizeof(optbuf)) ){
				CheckParam(&viewo, optbuf, NULL);
				SetOpts(&viewo);
			}
		}
	}
	if ( viewopts != NULL ) SetOpts(viewopts);
	switch( vo_.DModeType ){
		case DISPT_HEX: //-----------------------------------------------------
			vo_.DModeBit = DOCMODE_HEX;

			if ( VOi->img == NULL ){
				VOi->width = HEXWIDTH;
				VOi->line = ((vo_.file.UseSize + 15) >> 4);
				VOi->tab = 1;
				VOi->img = vo_.file.image;
			}
			if ( (VOi->textC < 0) || (VOi->textC >= VTYPE_MAX) ){
				VOi->textC = VTYPE_SYSTEMCP;
			}
			break;

		case DISPT_TEXT: //----------------------------------------------------
			vo_.DModeBit = DOCMODE_TEXT;

			if ( VOi->img == NULL ){
															// 文字コード 判別
				if ( (VOi->textC < 0) || (VOi->textC >= VTYPE_MAX) ){
					VOi->textC = GetTextCodeType(vo_.file.image, vo_.file.UseSize);
					if ( VOi->textC >= VTYPE_MAX ){
						if ( VOi->textC == CP__UTF16L ){
							VOi->textC = VTYPE_UNICODE;
						}else if ( VOi->textC == CP__UTF16B ){
							VOi->textC = VTYPE_UNICODEB;
						}else if ( VOi->textC == CP_UTF8 ){
							VOi->textC = VTYPE_UTF8;
						}else{
							VO_CodePage = VOi->textC;
							VO_CodePageValid = IsValidCodePage(VO_CodePage);
							VOi->textC = VTYPE_OTHER;
						}
					}
				}
				VOi->img = vo_.file.image;
				mtinfo.MemSize = vo_.file.UseSize;
				MakeIndexTable(MIT_FIRST, MIT_PARAM_TEXT);
			}
			break;

		case DISPT_IMAGE: //------------------------------------------------
			vo_.DModeBit = DOCMODE_BMP;

			if ( !memcmp(vo_.file.image, "GIF8", 4) ) InitGIF(NULL);
			if ( !memcmp(vo_.file.image, "\x89PNG", 4) ) InitPNG(NULL);
			if ( !memcmp(vo_.file.image, "II*", 4) ) InitTIFF(NULL);
			if ( !memcmp(vo_.file.image, "MM\0*", 4) ) InitTIFF(NULL);
			if ( vo_.bitmap.ShowInfo == NULL ){
				vo_.bitmap.ShowInfo = vo_.bitmap.info;
				CreateDIBtoPalette(&vo_);
			}
			if ( vo_.bitmap.info->biBitCount == 32 ) vo_.bitmap.transcolor = 0;
			break;

		case DISPT_DOCUMENT: //------------------------------------------------
			if ( vo_.DocmodeType == DOCMODE_NONE ){
				vo_.DocmodeType = DOCMODE_TEXT;
			}
			if ( VOi->img == NULL ){
				VOi->img = mtinfo.img;
				vo_.DModeBit = DOCMODE_TEXT;
				// mtinfo.MemSize
				MakeIndexTable(MIT_FIRST, MIT_PARAM_DOCUMENT);
			}else{
				vo_.DModeBit = vo_.DocmodeType;
			}
			break;

		case DISPT_RAWIMAGE: //------------------------------------------------
			vo_.DModeBit = DOCMODE_RAWIMAGE;

			// サイズ情報は SetScrollBar() でおこなう
			temp_DIB.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			temp_DIB.bmiHeader.biWidth = 1; // 仮
			temp_DIB.bmiHeader.biHeight = 1; // 仮
			temp_DIB.bmiHeader.biPlanes = 1;
			temp_DIB.bmiHeader.biBitCount = (WORD)(VOi->textC & 0x3f);
			temp_DIB.bmiHeader.biSizeImage = 0; // FixOpenBmp で修復
			temp_DIB.bmiHeader.biXPelsPerMeter = 0;
			temp_DIB.bmiHeader.biYPelsPerMeter = 0;

			if ( vo_.bitmap.hPal != NULL ){
				DeleteObject(vo_.bitmap.hPal);
				vo_.bitmap.hPal = NULL;
			}
			vo_.bitmap.ShowInfo = vo_.bitmap.info = &temp_DIB.bmiHeader;
			vo_.bitmap.bits.ptr = vo_.file.image;
									// パレット / ビットフィールドを初期化
			if ( VOi->textC >= 16 ){ // 16/24/32
				if ( VOi->textC != 24 ){
					temp_DIB.bmiHeader.biCompression = BI_BITFIELDS;
				}else{
					temp_DIB.bmiHeader.biCompression = BI_RGB;
				}
				temp_DIB.bmiHeader.biClrUsed = 0;
				temp_DIB.bmiHeader.biClrImportant = 0;
				temp_DIB.bmiColors[3] = 0;

				if ( temp_DIB.bmiHeader.biBitCount == 16 ){ // 16bit
					if ( VOi->textC & B6 ){ // R5G6B5
						temp_DIB.bmiColors[0] = 0xf800;
						temp_DIB.bmiColors[1] = 0x07e0;
						temp_DIB.bmiColors[2] = 0x001f;
						if ( VOi->textC & B7 ){ // 並びが RGB
							temp_DIB.bmiColors[2] = 0xf800;
							temp_DIB.bmiColors[0] = 0x001f;
						}
					}else{ // R5G5B5
						temp_DIB.bmiColors[0] = 0x7c00;
						temp_DIB.bmiColors[1] = 0x03e0;
						temp_DIB.bmiColors[2] = 0x001f;
						if ( VOi->textC & B7 ){ // 並びが RGB
							temp_DIB.bmiColors[0] = 0x001f;
							temp_DIB.bmiColors[2] = 0x7c00;
						}
					}
				}else if ( VOi->textC & B6 ){ // 32bit (R10G10B10)
					temp_DIB.bmiColors[0] = 0x3ff00000;
					temp_DIB.bmiColors[1] = 0x000ffc00;
					temp_DIB.bmiColors[2] = 0x000003ff;
					if ( VOi->textC & B7 ){ // 並びが RGB
						temp_DIB.bmiColors[0] = 0x000003ff;
						temp_DIB.bmiColors[2] = 0x3ff00000;
					}
				}else{ // 24/32bit
					temp_DIB.bmiColors[0] = 0xff0000;
					temp_DIB.bmiColors[1] = 0x00ff00;
					temp_DIB.bmiColors[2] = 0x0000ff;
					if ( VOi->textC & B7 ){ // 並びが RGB
						temp_DIB.bmiColors[0] = 0x0000ff;
						temp_DIB.bmiColors[2] = 0xff0000;
					}
					if ( temp_DIB.bmiHeader.biBitCount == 32 ){
						vo_.bitmap.transcolor = 0;
					}
				}
			}else{ // 1/4/8
				temp_DIB.bmiHeader.biCompression = BI_RGB;

				switch ( temp_DIB.bmiHeader.biBitCount ){
					case 1:
						temp_DIB.bmiColors[0] = 0x000000;
						temp_DIB.bmiColors[1] = 0xffffff;
						break;
//					case 4:
//					case 8:
					default:
						memcpy(temp_DIB.bmiColors, Palette16, sizeof(Palette16));
						break;
				}
				// LOGPALETTE を作るのが面倒なので、temp_DIB をつかいまわす
				temp_DIB.bmiHeader.biClrImportant =
					  /* palVersion */ 0x300
					+ /* palNumEntries */ (temp_DIB.bmiHeader.biClrUsed << 16);
				vo_.bitmap.hPal = CreatePalette((LOGPALETTE *)&temp_DIB.bmiHeader.biClrImportant);

				temp_DIB.bmiHeader.biClrImportant =
						temp_DIB.bmiHeader.biClrUsed = 1 << temp_DIB.bmiHeader.biBitCount;
			}
			InitRawImageRectSize();
			break;
	}
	if ( viewopt_opentime.dtype <= 0 ){
		viewopt_opentime.dtype = vo_.DModeType;
		viewopt_opentime.T_code = VOi->textC;
		viewopt_opentime.T_siso = VO_Tmode;
		viewopt_opentime.T_esc  = VO_Tesc;
		viewopt_opentime.T_tag  = VO_Ttag;
		viewopt_opentime.T_tab  = -1;
		viewopt_opentime.T_width  = VOi->width;
		viewopt_opentime.I_animate  = vo_.bitmap.page.animate;
		viewopt_opentime.I_CheckeredPattern  = -1;
		viewopt_opentime.linespace  = -1;
	}
	if ( vo_.DModeBit == DOCMODE_TEXT ){
		GetAsyncKeyState(VK_PAUSE); // 読み捨て(最下位bit対策)
		while ( VOi->reading != FALSE ){
			if ( (VOi->offY + READLINE_SUPP) < VOi->line ) break;
			MakeIndexTable(MIT_NEXT, READLINE_SUPP);
			if ( GetAsyncKeyState(VK_PAUSE) & KEYSTATE_FULLPUSH ) break;
		}
	}
	if ( (type == NULL) || (XV.img.imgD[0] >= IMGD_AUTOWINDOWSIZE) ){
		SetScrollBar();
	}

	if ( (ReadingStream != READ_NONE) ||
		 ( (vo_.DModeBit & VO_dmode_ENABLEBACKREAD) && (VOi->reading != FALSE) ) ){
		SetTimer(vinfo.info.hWnd, TIMERID_READLINE, TIMER_READLINE, BackReaderProc);
		BackReader = TRUE;
	}else{
		mtinfo.OpenFlags = 0;
	}
}

void SetTitle(const TCHAR *title, const TCHAR *ext)
{
	TCHAR buf[VFPS * 2];

	if ( runasstate == NULL ) runasstate = CheckRunAs();
	if ( runasstate != NULL ){
		wsprintf(buf, T("PPV[%c](%s)%s"), RegID[2], runasstate, title);
	}else{
		wsprintf(buf, T("PPV[%c]%s"), RegID[2], title);
	}
	if ( ext != NULL ){
		TCHAR extbuf[MAX_PATH];

		wsprintf(extbuf, T(".%s"), ext);
		tstrcat(buf, extbuf);
		tstrcat(vo_.file.name, extbuf);
	}
	SetWindowText(vinfo.info.hWnd, buf);
}
BOOL USEFASTCALL FixOpenBmp(BITMAPINFOHEADER *bih, VIEWOPTIONS *viewopt, DWORD bits_size)
{
	vo_.bitmap.rawsize.cx = bih->biWidth;
	vo_.bitmap.rawsize.cy = bih->biHeight;
	FixBitmapShowSize(&vo_);

	if ( (bih->biClrUsed == 0) && (bih->biBitCount <= 8) ){
		bih->biClrUsed = 1 << bih->biBitCount;
	}

	if ( (bih->biSize == 0x6c /*BMPV4*/) || (bih->biSize == 0x7c /*BITMAPV5HEADER*/) ){
		vo_.bitmap.UseICC = ICM_OFF;
		SetPopMsg(POPMSG_NOLOGMSG, T("include color space"));
	}
	if ( bih->biSizeImage == 0 ){ // サイズ情報無し
		bih->biSizeImage = DwordBitSize(vo_.bitmap.rawsize.cx * bih->biBitCount) * vo_.bitmap.rawsize.cy;
	}

	if ( ((bih->biCompression == BI_RGB) || (bih->biCompression == BI_BITFIELDS)) &&
			(bih->biSizeImage > bits_size) ){ // ヘッダ情報のサイズがビットマップより大きい
		bih->biSizeImage = bits_size;
		bih->biHeight = bih->biSizeImage / DwordBitSize(vo_.bitmap.rawsize.cx * bih->biBitCount);
		if ( bih->biHeight == 0 ) return FALSE; // イメージが１ライン分も無い
	}
	if ( bih->biBitCount == 64 ){								// レンジ調整
		WORD *ptr, *ptrmax;
		BYTE *destptr;

		bih->biBitCount = 32;
		ptr = (WORD *)vo_.bitmap.bits.ptr;
		ptrmax = (WORD *)(BYTE *)(vo_.bitmap.bits.ptr + bits_size - 1);
		destptr = vo_.bitmap.bits.ptr;
		for ( ;; ){ // 8bit に納める
			WORD pix;
			pix = (WORD)(*ptr++ >> 4);
			if ( pix >= 0x100 ) pix = 0xff;
			*destptr++ = (BYTE)pix;
			if ( ptr >= ptrmax ) break;
		}
	}
														// アスペクト比調整
	XV.img.AspectW = bih->biXPelsPerMeter / 39;
	XV.img.AspectH = bih->biYPelsPerMeter / 39;
	XV.img.AspectRate = !bih->biYPelsPerMeter ? 0 : ((bih->biXPelsPerMeter * ASPACTX) / bih->biYPelsPerMeter);
	if ( (XV.img.AspectRate > (ASPACTX * 3)) ||
		 (XV.img.AspectRate < (ASPACTX / 3)) ||
		 ((XV.img.AspectRate > (ASPACTX - (ASPACTX / 10))) &&
		  (XV.img.AspectRate < (ASPACTX + (ASPACTX / 10)))) ){
		XV.img.AspectRate = 0;
	}else{
		TCHAR buf[64];

		wsprintf(buf, T("AspectRate %d:%d"), XV.img.AspectW, XV.img.AspectH);
		SetPopMsg(POPMSG_NOLOGMSG, buf);
	}
	PPVGetHist();
	InitViewObject(viewopt, NULL);

	if ( (viewopt != NULL) ? (viewopt->I_CheckeredPattern == 1) : (viewopt_def.I_CheckeredPattern == 1) ){
		if ( vo_.bitmap.transcolor >= 0 ) ModifyAlpha(vinfo.info.hWnd);
	}
	return TRUE;
}

BOOL OpenCursorIconObject(const TCHAR *filename, VIEWOPTIONS *viewopt)
{
	int offset;
	BITMAPINFOHEADER *bih, *tempbih;
	const TCHAR *imagetypename;

	if ( convert || (ReadAll() <= 0) ){
		VO_error(PPERROR_GETLASTERROR);
		return FALSE;
	}
	if ( vo_.file.UseSize < (0x16 /* ICONDIR + ICONDIRENTRY */ + sizeof(BITMAPINFOHEADER)) ) return FALSE;
										// ICONDIR.idEntries[0].dwImageOffset;
	tempbih = (BITMAPINFOHEADER *)((BYTE *)vo_.file.image + *(DWORD *)(vo_.file.image + 0x12));
	if ( (DWORD_PTR)(((BYTE *)tempbih - (BYTE *)vo_.file.image) + 0x10) > vo_.file.UseSize ) return FALSE;
	if ( tempbih->biSize != sizeof(BITMAPINFOHEADER) ) return FALSE;
	if ( *(vo_.file.image + 02) == '\1' ){
		imagetypename = T("ico");
		tstrcpy(vo_.file.typeinfo, T("icon"));
	}else{
		imagetypename = T("cur");
		tstrcpy(vo_.file.typeinfo, T("cursor"));
	}

	vo_.bitmap.info = tempbih;

	if ( IsTrue(vo_.file.memdata) ) SetTitle(filename, imagetypename);

	vo_.SupportTypeFlags = VO_type_ALLIMAGE;
	vo_.DModeType = DISPT_IMAGE;

	offset = CalcBmpHeaderSize(vo_.bitmap.info);
	vo_.bitmap.ShowInfo = (BITMAPINFOHEADER *)HeapAlloc(PPvHeap, 0, offset);
	if ( vo_.bitmap.ShowInfo == NULL ) return FALSE;
	bih = vo_.bitmap.ShowInfo;
	vo_.bitmap.AllocShowInfo = TRUE;

	memcpy(bih, vo_.bitmap.info, offset);

	bih->biHeight /= 2;

	vo_.bitmap.bits.ptr = (BYTE *)vo_.bitmap.info + offset;
	return FixOpenBmp(bih, viewopt, vo_.file.UseSize - offset );
}

BOOL OpenBmpObject(const TCHAR *filename, VIEWOPTIONS *viewopt)
{
	BITMAPINFOHEADER *bih;
	DWORD bmpoffset, offset;

	if ( convert || (ReadAll() <= 0) ){
		VO_error(PPERROR_GETLASTERROR);
		return FALSE;
	}

	if ( IsTrue(vo_.file.memdata) ) SetTitle(filename, T("bmp"));

	vo_.bitmap.info = (BITMAPINFOHEADER *)(vo_.file.image + 0xe);
	bih = vo_.bitmap.info;

	switch ( vo_.bitmap.info->biSize ){
		case sizeof(BITMAPCOREHEADER):	// OS/2 形式 0x0c bcBitCount まで
			if ( temp_DIB_ptr == NULL ){
				temp_DIB_ptr = LocalAlloc(LMEM_FIXED, sizeof(BITMAPINFOHEADER) + (0x100 * sizeof(RGBQUAD)));
				if ( temp_DIB_ptr == NULL ) return FALSE;
			}
			vo_.bitmap.ShowInfo = bih = temp_DIB_ptr;

			bih->biSize = sizeof(BITMAPINFOHEADER);
			bih->biWidth = ((BITMAPCOREHEADER *)vo_.bitmap.info)->bcWidth;
			bih->biHeight = ((BITMAPCOREHEADER *)vo_.bitmap.info)->bcHeight;
			if ( bih->biHeight & B15 ) bih->biHeight = -(0x10000 - bih->biHeight);
			bih->biPlanes = ((BITMAPCOREHEADER *)vo_.bitmap.info)->bcPlanes;
			bih->biBitCount = ((BITMAPCOREHEADER *)vo_.bitmap.info)->bcBitCount;
			bih->biSizeImage = 0; // FixOpenBmp で修復
			bih->biCompression = BI_RGB;
			bih->biXPelsPerMeter = 3780;
			bih->biYPelsPerMeter = 3780;
			bih->biClrImportant = bih->biClrUsed = 0;
			tstrcpy(vo_.file.typeinfo, T("DIB(OS/2)"));
			if ( bih->biBitCount <= 8 ){
				DWORD c;
				DWORD *WinPal;
				RGBTRIPLE *OS2Pal;

				OS2Pal = (RGBTRIPLE *)((BYTE *)vo_.bitmap.info + vo_.bitmap.info->biSize);
				WinPal = (DWORD *)((BYTE *)bih + bih->biSize);
				bih->biClrImportant = bih->biClrUsed = 1 << bih->biBitCount;
				for ( c = 0 ; c < bih->biClrUsed ; c++ ){
					*WinPal++ = OS2Pal->rgbtBlue + (OS2Pal->rgbtGreen << 8) + (OS2Pal->rgbtRed << 16);
					OS2Pal++;
				}
			}
			break;

		case sizeof(BITMAPINFOHEADER): // 0x28
			tstrcpy(vo_.file.typeinfo, T("DIB"));
			break;

		case 0x40:	// OS/2 v2 形式(OS22XBITMAPHEADER) 0x40
			// 0～0x28はBITMAPINFOHEADERと同じ、0x28～0x40
			tstrcpy(vo_.file.typeinfo, T("DIB(OS/2v2)"));
			break;

		case 0x6c:
			tstrcpy(vo_.file.typeinfo, T("DIBv4"));
			break;

		case 0x7c:
			tstrcpy(vo_.file.typeinfo, T("DIBv5"));
			break;

		default:
			tstrcpy(vo_.file.typeinfo, T("DIB(breaked?)"));
			if ( bih->biSize < sizeof(BITMAPINFOHEADER) ) return FALSE;
	}
	if ( (bih->biWidth == 0) || (bih->biWidth & B31) || (bih->biHeight == 0) ){
		return FALSE;
	}
	if ( bih->biPlanes > 2 ) bih->biPlanes = 1; // 異常データ対策
	if ( bih->biClrUsed > 255 ) bih->biClrUsed = 0; // 異常データ対策

	vo_.SupportTypeFlags = VO_type_ALLIMAGE;
	vo_.DModeType = DISPT_IMAGE;

	offset = CalcBmpHeaderSize(vo_.bitmap.info);
	bmpoffset = ((BITMAPFILEHEADER *)vo_.file.image)->bfOffBits;
	if ( bmpoffset > offset ){
		vo_.bitmap.bits.ptr = vo_.file.image + bmpoffset;
	}else{
		vo_.bitmap.bits.ptr = (BYTE *)vo_.bitmap.info + offset;
	}
	return FixOpenBmp(bih, viewopt,
			vo_.file.UseSize - (vo_.bitmap.bits.ptr - vo_.file.image) );
}

HGLOBAL OpenViewStdin(void)
{
	DWORD starttick, nowtick;
	HGLOBAL memblock;

	vo_.file.memdata = TRUE;
	vo_.file.ImageSize = 0x8000;
	memblock = GlobalAlloc(GMEM_MOVEABLE, vo_.file.ImageSize + FILEBUFMARGIN);
	if ( memblock == NULL ){
		VO_error(PPERROR_GETLASTERROR);
		return NULL;
	}
	if ( (vo_.file.image = GlobalLock(memblock)) == NULL ){
		GlobalFree(memblock);
		VO_error(PPERROR_GETLASTERROR);
		return NULL;
	}
	vo_.file.mapH = memblock;
	vo_.file.UseSize = 0;
	ReadingStream = READ_STDIN;
	hReadStream = GetStdHandle(STD_INPUT_HANDLE);
	ReadEnter++;

	starttick = nowtick = GetTickCount();
	for ( ;; ){
		if ( IsTrue(ReadData(nowtick)) ) break;
		nowtick = GetTickCount();
		if ( (nowtick - starttick) > 200 ) break;
		Sleep(0);
	}
	ReadEnter--;
	return memblock;
}

// ファイルを開く -------------------------------------------------------------
BOOL OpenViewObject(const TCHAR *filename, HGLOBAL memblock, VIEWOPTIONS *viewopt, int flags)
{
	if ( OpenEntryNow != 0 ) return FALSE;
	OpenEntryNow = 1;
	mtinfo.OpenFlags = flags;
#if COUNTREADTIME
	readstartTick = GetTickCount();
#endif
	vo_.file.memdata = FALSE;
	CloseViewObject();

	PreFrameInfo.page = PreFramePage_NoCheck;

	if ( viewopt != NULL ){
		VO_optdata = *viewopt;
		VO_opt = &VO_optdata;
	}else{
		VO_opt = NULL;
	}
	SetTitle(filename, NULL);
	tstrcpy(vo_.file.typeinfo, T("Loading file"));
	vo_.file.IsFile = 0;

	if ( ! (flags & PPV__reload) ){
		FileDividePointer.LowPart = FileDividePointer.HighPart = 0;
		FileDivideMode = FDM_NODIV;
	}
										// fileimage を読み込む ---------------
	if ( memblock == NULL ){ // ファイル読み込み
		if ( flags & PPV__stdin ){	// 標準入力
			memblock = OpenViewStdin();
			if ( memblock == NULL ) goto error;
		}else{
			if ( OpenViewFile(filename, flags) == FALSE ) goto error;
		}
	}else{	// GlobalAlloc
		vo_.file.memdata = TRUE;
		vo_.file.UseSize = GlobalSize(memblock);
		if ( vo_.file.UseSize == 0 ){
			VO_error(PPERROR_GETLASTERROR);
			GlobalFree(memblock);
			goto error;
		}
		vo_.file.mapH = memblock;
		memblock = GlobalReAlloc(memblock, vo_.file.UseSize + FILEBUFMARGIN, 0);
		if ( memblock != NULL ) vo_.file.mapH = memblock;
		if ( (vo_.file.image = GlobalLock(vo_.file.mapH)) == NULL ){
			VO_error(PPERROR_GETLASTERROR);
			goto error;
		}
	}

	tstrcpy(vo_.file.name, filename);
	if ( X_tray & X_tray_PPv ) ShowWindow(vinfo.info.hWnd, SW_RESTORE);
	HideCaret(vinfo.info.hWnd);
	InvalidateRect(vinfo.info.hWnd, &BoxStatus, TRUE);	// 更新指定
	UpdateWindow_Part(vinfo.info.hWnd);

	//--------------------------------------
	if ( !memcmp(vo_.file.image, T("HTTP/1."), 7) ){
		BYTE *p, *maxptr;

		p = vo_.file.image + 7;
		maxptr = vo_.file.image + vo_.file.UseSize;
		for ( ; p < maxptr ; p++ ){
			if ( !memcmp( p, "\r\n\r\n", 4) ){
				if ( IDOK != PMessageBox(vinfo.info.hWnd, T("HTTP decode?"), T("open"),
						MB_OKCANCEL | MB_SETFOREGROUND | MB_DEFBUTTON1 |
							MB_ICONQUESTION) ){
					 break;
				}
				ThAppend(&vo_.memo, vo_.file.image, p + 4 - vo_.file.image);
				ThAddString(&vo_.memo, NilStr);
				vo_.file.image = p + 4;
				break;
			}
			if ( (*p == '\r') || (*p == '\n') ||
				  ((*p >= ' ') && (*p <= 'z')) ) continue;
			break;
		}
	}
										// 各種初期化 -------------------------
	{
		int count = 0;
		TCHAR keyword[VFPS], param[VFPS], *extptr;
		FN_REGEXP fn;

		extptr = VFSFindLastEntry(vo_.file.name);
		while( EnumCustTable(count++,
				T("XV_tab"), keyword, param, sizeof(param)) >= 0 ){
			TCHAR *p;
			int tab;

			MakeFN_REGEXP(&fn, param);
			if ( FilenameRegularExpression(extptr, &fn) ){
				FreeFN_REGEXP(&fn);
				p = keyword;
				tab = GetIntNumber((const TCHAR **)&p);
				VO_I[DISPT_HEX].tab = VO_I[DISPT_TEXT].tab = VO_I[DISPT_DOCUMENT].tab = tab;
				break;
			}
			FreeFN_REGEXP(&fn);
		}
		count = 0;

		while( EnumCustTable(count++,
				T("XV_cols"), keyword, param, sizeof(param)) >= 0 ){
			TCHAR *p;
			int cols;

			MakeFN_REGEXP(&fn, param);
			if ( FilenameRegularExpression(extptr, &fn) ){
				FreeFN_REGEXP(&fn);
				p = keyword;
				cols = GetIntNumber((const TCHAR **)&p);
				VO_I[DISPT_TEXT].defwidth = VO_I[DISPT_DOCUMENT].defwidth = cols;

				VO_I[DISPT_TEXT].width = VO_I[DISPT_DOCUMENT].width = FixedWidthRange(cols);
				break;
			}
			FreeFN_REGEXP(&fn);
		}
	}
										// 判別 -------------------------------
	if ( !(flags & PPV_HEADVIEW) ){
										// susie plug-in 判別
		int rr = VFSGetDibDelay(vo_.file.name,
				vo_.file.image, vo_.file.UseSize,
				vo_.file.typeinfo,
				&vo_.bitmap.info_hlocal, &vo_.bitmap.bits.mapH, ReadAll);
		if ( rr < 0 ){ // 分割しているので解除
			FileDivideMode = FDM_FORCENODIV;
			OpenEntryNow = 0;
			return OpenViewObject(filename, memblock, viewopt, flags | PPV__reload);
		}

		if ( rr ){
			if ( convert ){
				VO_error(PPERROR_GETLASTERROR);
				goto error;
			}
			vo_.bitmap.bits.ptr = LocalLock(vo_.bitmap.bits.mapH);
			vo_.bitmap.info = LocalLock(vo_.bitmap.info_hlocal);
			if ( vo_.bitmap.info != NULL ){
				if ( (vo_.bitmap.info->biWidth != 0) && (vo_.bitmap.info->biHeight != 0) ){
					vo_.SupportTypeFlags = VO_type_ALLIMAGE;
					vo_.DModeType = DISPT_IMAGE;
					GetMemo();
					if ( FixOpenBmp(vo_.bitmap.info, viewopt,
							LocalSize(vo_.bitmap.bits.mapH)) ){
						goto success;
					}
				}
			}
		}
										// CUR/ICO 判別
		if (	(*(vo_.file.image + 00) == '\0')	&&
				(*(vo_.file.image + 01) == '\0')	&&
				((*(vo_.file.image + 02) == '\1') || (*(vo_.file.image + 02) == '\2'))	&&
				(*(vo_.file.image + 03) == '\0') ){
			if ( OpenCursorIconObject(filename, viewopt) != FALSE ){
				goto success;
			}
		}else if ( (vo_.file.UseSize >= 0x16) && (*(DWORD *)(vo_.file.image + 10) < vo_.file.UseSize) &&
				(*(vo_.file.image +  0) == 'B')	&& // BMP 判別
				(*(vo_.file.image +  1) == 'M')	&&
				(*(vo_.file.image +  6) == 0)	&&
				(*(vo_.file.image +  7) == 0)	&&
				(*(vo_.file.image + 16) == 0)	){
			if ( FileDivideMode == FDM_DIV ){ // 分割しているので解除
				FileDivideMode = FDM_FORCENODIV;
				OpenEntryNow = 0;
				return OpenViewObject(filename, memblock, viewopt, flags | PPV__reload);
			}

			if ( OpenBmpObject(filename, viewopt) != FALSE ) goto success;
		}
	}

	VO_I[DISPT_HEX].textC = VO_I[DISPT_TEXT].textC = -1;
	CheckType();
	if ( FileDivideMode == FDM_DIV ) SetPopMsg(POPMSG_NOLOGMSG, MES_IDIV);

success:
	LoadEvent();
	if ( (vo_.DModeBit & DOCMODE_BMP) && vo_.bitmap.rotate ){ // 回転有り
		switch (vo_.bitmap.rotate){
			case 1:
				Rotate(vinfo.info.hWnd, 10);
				break;
			case 2:
				Rotate(vinfo.info.hWnd, 10);
				Rotate(vinfo.info.hWnd, 10);
				break;
			case 3:
				Rotate(vinfo.info.hWnd, -10);
				break;
		}
	}
	OpenEntryNow = 0;
	return TRUE;

error:
	OpenEntryNow = 0;
	return FALSE;
}

void LoadEvent(void)
{
	const TCHAR *extname;

	if ( BackReader ) return; //読込/行数計算中

	if ( FirstCommand != NULL ){
		PostMessage(vinfo.info.hWnd, WM_PPXCOMMAND, K_FIRSTCMD, 0);
	}

	if ( vo_.DModeBit & VO_dmode_IMAGE ){
		extname = T("KV_img");
	}else{
		extname = (VOsel.cursor != FALSE) ? T("KV_crt") : T("KV_page");
	}
	if ( IsExistCustTable(extname, T("LOADEVENT")) ||
		 IsExistCustTable(T("KV_main"), T("LOADEVENT")) ){
		PostMessage(vinfo.info.hWnd, WM_PPXCOMMAND, K_E_LOAD, 0);
	}
}

void GetMemo(void)
{
	VFSFILETYPE vft;
										// ファイル種別の判定とそれによる初期化
	if ( (X_swmt == 0) || (vo_.memo.bottom != NULL) ) return;
	vft.flags = VFSFT_INFO;

	if ( VFSGetFileType(vo_.file.name, (char *)vo_.file.image, vo_.file.UseSize, &vft) != NO_ERROR ){
		return;
	}

	if ( vft.info != NULL ){
		ThCatString(&vo_.memo, vft.info);
		HeapFree(PPvHeap, 0, vft.info);
	}
}

void CheckType(void)
{
	VFSFILETYPE vft;
										// ファイル種別の判定とそれによる初期化
	vft.flags = VFSFT_TYPE | VFSFT_TYPETEXT | VFSFT_EXT;
	if ( X_swmt && (vo_.memo.bottom == NULL) ) setflag(vft.flags, VFSFT_INFO);
	if ( VFSGetFileType(vo_.file.name, (char *)vo_.file.image, vo_.file.UseSize, &vft) == NO_ERROR ){
		int codefunc;

		wsprintf(vo_.file.typeinfo, T("%s(%s)"), vft.typetext, vft.type);
		if ( IsTrue(vo_.file.memdata) ) SetTitle(vo_.file.name, vft.ext);

		vo_.DModeType = LOWORD(vft.dtype); // DISPT_xxx
		VOi = &VO_I[vo_.DModeType];
/*
		if ( VOi->defwidth == WIDTH_AUTOFIX ){
			int newwidth = FixedWidthRange(VOi->defwidth);
			if ( newwidth != VOi->width ){
	VOi->img = NULL;
				VOi->width = newwidth;
				mtinfo.PresetY = VOi->offY; // 読み込み完了後の表示位置を指定(●1.2x暫定)
				FixChangeMode(vinfo.info.hWnd);


			}
		}
*/
		codefunc = HIWORD(vft.dtype); // DT_xxx
		if ( codefunc < CODEFUNCMAX ){
			if ( Vd_codefuncinit[codefunc].func() ){
				vo_.SupportTypeFlags = VO_type_ALLTEXT;
			}
		}
#if 0
		if ( tstrcmp(vft.type, T(":DOCX")) == 0 ){
			TCHAR arcfile[VFPS], fname[VFPS];
			DWORD result, sizeL, sizeH;
			HWND hDWnd;
			HANDLE hFile;
			HANDLE mapH;
			BYTE *image;

			tstrcpy(arcfile, vo_.file.name);
			tstrcpy(fname, T("\\word\\document.xml"));
			hDWnd = vinfo.info.hWnd;
			hFile = CreateFileL(arcfile, GENERIC_READ,
						FILE_SHARE_WRITE | FILE_SHARE_READ, NULL,
						OPEN_EXISTING,
						FILE_FLAG_SEQUENTIAL_SCAN,
						NULL);
			result = VFSGetArchivefileImage(hDWnd, hFile, arcfile, fname,
				&sizeL, &sizeH, &mapH, &image);
			if ( result == NO_ERROR ){
				vo_.SupportTypeFlags = VO_type_ALLDOCUMENT;
				SetAllTextCode(VTYPE_UTF8);
				vo_.text.document.ptr = mtinfo.img = VOi->img = image;
				vo_.text.document.mapH = mapH;
				mtinfo.MemSize = sizeL;
				VO_Ttag = 2;
				vft.dtype = DISPT_DOCUMENT;
				MakeIndexTable(MIT_FIRST, MIT_PARAM_DOCUMENT);
/*
				vo_.file.UseSize = sizeL;
				vo_.file.mapH = mapH;
				vo_.file.image = image;
*/
			}
		}
#endif
		PPVGetHist();
		InitViewObject(VO_opt, vft.type);

		if ( X_swmt && (vft.info != NULL) ){
			ThCatString(&vo_.memo, vft.info);
			HeapFree(PPvHeap, 0, vft.info);
		}
	}else{		// 不明テキスト
		vo_.SupportTypeFlags = VO_type_ALLTEXT;
		if ( vo_.DModeType != DISPT_HEX ){
			vo_.DModeType = DISPT_TEXT;
			VOi = &VO_I[DISPT_TEXT];
		}
		tstrcpy(vo_.file.typeinfo, T("Unknown"));
		PPVGetHist();
		InitViewObject(VO_opt, T("*"));
		vft.type[0] = '\0';
	}
	if ( vo_.DModeBit & VO_dmode_TEXTLIKE ) LoadHighlight(&vft);
}

void FollowOpenView(PPV_APPINFO *vinfo)
{

	OpenEntryNow = 1;
	if ( X_tray & X_tray_PPv ){
		SetWindowLongPtr(vinfo->info.hWnd, GWL_STYLE, GetWindowLongPtr(vinfo->info.hWnd, GWL_STYLE) & ~WS_MINIMIZE);
		ShowWindow(vinfo->info.hWnd, SW_SHOW);
	}
	if ( IsIconic(vinfo->info.hWnd) || !IsWindowVisible(vinfo->info.hWnd) ){
		PostMessage(vinfo->info.hWnd, WM_SYSCOMMAND, SC_RESTORE, 0xffff0000);
	}

	if ( TailModeFlags ){
		if ( OldTailLine >= 0 ){
			MoveCsr(0, (OldTailLine - 2) - VOi->offY, FALSE);
		}else{
			MoveCsr(0, VO_maxY, FALSE);
		}
	}

	InvalidateRect(vinfo->info.hWnd, NULL, TRUE);
	SetScrollBar();
	DxSetMotion(DxDraw, DXMOTION_NewWindow);
	if ( XV.img.imgD[0] <= IMGD_FIXWINDOWSIZE ){ // IMGD_FIXWINDOWSIZE/IMGD_AUTOFIXWINDOWSIZE
		// 最小化とかしていたら窓表示が戻るまで待つ
		if ( IsIconic(vinfo->info.hWnd) || !IsWindowVisible(vinfo->info.hWnd) ){
			MSG msgdata;
			DWORD tick = GetTickCount();

			while ( PeekMessage(&msgdata, vinfo->info.hWnd, 0, 0, PM_REMOVE) ){
				if ( msgdata.message == WM_QUIT ) break;
				TranslateMessage(&msgdata);
				DispatchMessage(&msgdata);

				if ( (GetTickCount() - tick) > 500 ) break;
			}
		}

		if ( vo_.DModeBit & VO_dmode_IMAGE ){
			FixWindowSize(vinfo->info.hWnd, 0, 0); // 画像に合わせて調節
		}else{ // テキスト用の大きさに調節
			SetWindowPos(vinfo->info.hWnd, NULL, 0, 0,
					WinPos.pos.right - WinPos.pos.left,
					WinPos.pos.bottom - WinPos.pos.top,
					SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER);
		}
	}

	if ( XV_tmod && (vo_.DModeBit & VO_dmode_SELECTABLE) ){
		InitCursorMode(vinfo->info.hWnd, FALSE);
	}else{
		HideCaret(vinfo->info.hWnd);
	}
	OpenEntryNow = 0;
}

void OpenAndFollowViewObject(PPV_APPINFO *vinfo, const TCHAR *filename, HGLOBAL mem, VIEWOPTIONS *viewopt, int flags)
{
	OpenViewObject(filename, mem, viewopt, flags);
	FollowOpenView(vinfo);
}

#if NODLL
extern DWORD crc32(const BYTE *bin , DWORD size, DWORD r);
#else
const LONG crc32_half[16] = {
	0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
	0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
	0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
	0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
};

DWORD crc32(const BYTE *bin, DWORD size, DWORD r)
{
	DWORD crc = ~r;
	const BYTE *ptr = bin;
	BYTE chr;

	if ( size == MAX32 ) size = TSTRLENGTH32((TCHAR *)bin);
	while ( size-- ){
		chr = *ptr++;
		crc = crc32_half[(crc ^  chr      ) & 0x0F] ^ (crc >> 4);
		crc = crc32_half[(crc ^ (chr >> 4)) & 0x0F] ^ (crc >> 4);
	}
	return ~crc;
}
#endif


#pragma argsused
void CALLBACK AnimateProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	UnUsedParam(hWnd);UnUsedParam(uMsg);UnUsedParam(idEvent);UnUsedParam(dwTime);
	KillTimer(hWnd, idEvent);
	if ( (vo_.bitmap.page.current + 1) >= vo_.bitmap.page.max ){
		vo_.bitmap.page.current = 0;
		ChangePage(0);
	}else{
		ChangePage(1);
	}
}

int USEFASTCALL dabs(int a)
{
	return a >= 0 ? a : -a;
}

int getdelta(DWORD d1, DWORD d2)
{
	return (dabs((int)(d1 & 0xff0000) - (int)(d2 & 0xff0000)) >> 16) +
		(dabs((int)(d1 & 0xff00) - (int)(d2 & 0xff00)) >> 8) +
		dabs((int)(d1 & 0xff) - (int)(d2 & 0xff));
}
enum {
	TMODE_NONE = 0,
	TMODE_GIF_2X, // 2値 bit shift 有り
	TMODE_GIF_4,
	TMODE_GIF_8_2C, // 8bit / 2bit上書き
	TMODE_GIF_4to8, // 8bit に 4bitを合成
	TMODE_PNG_8,
	TMODE_PNG_24,
	TMODE_PNG_32,

	TMODE_GIFMODE = 16,
	TMODE_PNGMODE
};
#define BitSizeMacro(bits) (((bits) + 7) / 8)
#define BitPosMacro(bits) ((bits) / 8)

void ChangePage(int delta)
{
	GETPAGESTRUCT gpage;
	BYTE *tempimg;
	size_t tempsize;
	HLOCAL hHeader, hImage;
	BITMAPINFO *NewBMPinfo;
	BOOL tmode = TMODE_NONE;
	TCHAR *filename;
	BOOL nosupport = FALSE;
	int newpage;

	KillTimer(vinfo.info.hWnd, TIMERID_ANIMATE);

	if ( (vo_.DModeBit != DOCMODE_BMP) || (vo_.bitmap.page.max == 0) ) return;

	newpage = vo_.bitmap.page.current + delta;
	if ( (newpage < 0) || (newpage >= vo_.bitmap.page.max) ) return;
	gpage.page = newpage;
	filename = vo_.file.name;

	if ( (PreFrameInfo.page <= PreFramePage_BG) &&
		 ((delta == 1) || ((delta == 0) && (newpage == 0))) ){
		if ( PreFrameInfo.page == PreFramePage_BG ){
			PreFrameInfo.page = gpage.page;
		}
	}

	if ( PreFrameInfo.page == gpage.page ){ // 前のフレームで指示された処理を行う
		if ( PreFrameInfo.bitmapsize != 0 ){ // １つまえに戻す
			memcpy(vo_.bitmap.bits.ptr, PreFrameInfo.bitmap, PreFrameInfo.bitmapsize);
		}else{ // 塗りつぶし	●読み込み直後のフレームは、InitPNG(NULL) のためPreFrameInfo の設定がされていないので、消されないバグとなっている
			int h, dstwidth, copywidth;
			BYTE *dst;

			dstwidth = DwordBitSize(vo_.bitmap.rawsize.cx * vo_.bitmap.info->biBitCount);
			copywidth = BitSizeMacro(PreFrameInfo.size.cx * vo_.bitmap.info->biBitCount);
			if ( PreFrameInfo.offset.y + PreFrameInfo.size.cy > vo_.bitmap.rawsize.cy ){
				PreFrameInfo.size.cy = vo_.bitmap.rawsize.cy - PreFrameInfo.offset.y;
			}
			if ( vo_.bitmap.info->biBitCount == 4 ){
				PreFrameInfo.fillcolor = (BYTE)((PreFrameInfo.fillcolor & 0xf) | (PreFrameInfo.fillcolor << 4));
			}
			dst = vo_.bitmap.bits.ptr +
				  dstwidth * (vo_.bitmap.rawsize.cy - PreFrameInfo.offset.y - 1) +
				  BitSizeMacro(PreFrameInfo.offset.x * vo_.bitmap.info->biBitCount);
			for ( h = 1 ; h <= PreFrameInfo.size.cy ; h++ ){
				memset(dst, PreFrameInfo.fillcolor, copywidth);
				dst -= dstwidth;
			}
		}
	}

	if ( !memcmp(vo_.file.image, "GIF8", 4) ){
		GIFIMAGEHEADER *gi;

		InitGIF(&gpage);
		tempsize = gpage.headersize + gpage.size + 1;
		tempimg = HeapAlloc(PPvHeap, 0, tempsize + FILEBUFMARGIN);
		if ( tempimg == NULL ) return;

		memcpy(tempimg, vo_.file.image, gpage.headersize);
		gi = (GIFIMAGEHEADER *)(tempimg + gpage.headersize);
		memcpy(gi, vo_.file.image + gpage.offset, gpage.size);
		memset(tempimg + tempsize - 1, GIFHEADER_ENDID, 1024);
		// 座標調整
		gi->left = 0;
		gi->top = 0;
		((GIFHEADER *)tempimg)->width  = gi->width;
		((GIFHEADER *)tempimg)->height = gi->height;
		tmode = TMODE_GIFMODE;
	}else if ( !memcmp(vo_.file.image, "II*", 4) || !memcmp(vo_.file.image, "MM\0*", 4) ){
		GetDWORDfunc GetDWORD = InitTIFF(&gpage);
		if ( GetDWORD == NULL ) return;

		tempsize = vo_.file.UseSize;
		tempimg = HeapAlloc(PPvHeap, 0, tempsize + FILEBUFMARGIN);
		if ( tempimg == NULL ) return;

		memcpy(tempimg, vo_.file.image, vo_.file.UseSize);
		*(DWORD *)(tempimg + 4) = GetDWORD(&gpage.offset);

	}else if ( !memcmp(vo_.file.image, "\x89PNG", 4) ){
		InitPNG(&gpage);

		filename = T(":\\:.png");
		tempsize = vo_.file.UseSize;
		tempimg = HeapAlloc(PPvHeap, 0, tempsize + FILEBUFMARGIN);
		if ( tempimg == NULL ) return;

		if ( gpage.optionoffset == 0 ){
			memcpy(tempimg, vo_.file.image, tempsize);
		}else{
			DWORD srcoffset, destoffset;
			PNGchunk *pc;
			PNGFrameControlStruct *fcs;

			// 該当フレームの直前までコピー
			srcoffset = destoffset = gpage.offset;
			memcpy(tempimg, vo_.file.image, srcoffset);
			while ( srcoffset < gpage.optionoffset ){
				DWORD csize;

				pc = (PNGchunk *)(vo_.file.image + srcoffset);
				csize = GetBigEndDWORD(&pc->length);
				if ( csize == 0 ) break;
				// IDAT と fdAT はコピーしない
				if ( (pc->id != PNG_IDAT) && (pc->id != PNG_fdAT) ){
					memcpy(tempimg + destoffset, vo_.file.image + srcoffset,
						csize + SizeOfPNGchunks);
					destoffset += csize + SizeOfPNGchunks;
				}
				srcoffset += csize + SizeOfPNGchunks;
			}

			// fcTL からフレーム情報を取得
			srcoffset = gpage.optionoffset;
			fcs = (PNGFrameControlStruct *)(vo_.file.image + srcoffset);
			*(DWORD *)(tempimg + 0x10) = fcs->width;
			*(DWORD *)(tempimg + 0x14) = fcs->height;
			srcoffset += GetBigEndDWORD(&fcs->pc.length) + SizeOfPNGchunks;

			// フレーム表示後の処理
			if ( (fcs->dispose_op != APNG_DISPOSE_OP_NONE) &&
				 ((delta == 1) || ((delta == 0) && (newpage == 0))) ){
				if ( fcs->dispose_op == APNG_DISPOSE_OP_BACKGROUND ){
					PreFrameInfo.page = gpage.page + 1;
				}else if ( fcs->dispose_op == APNG_DISPOSE_OP_PREVIOUS ){
					if ( PreFrameInfo.bitmapsize == 0 ){
						DWORD bitmapsize;

						bitmapsize = DwordBitSize(vo_.bitmap.rawsize.cx * vo_.bitmap.ShowInfo->biBitCount) * vo_.bitmap.rawsize.cy;

						PreFrameInfo.bitmap = HeapAlloc(PPvHeap, 0, bitmapsize);
						if ( PreFrameInfo.bitmap != NULL ){
							PreFrameInfo.bitmapsize = bitmapsize;
						}
					}
					if ( PreFrameInfo.bitmapsize != 0 ){
						memcpy(PreFrameInfo.bitmap, vo_.bitmap.bits.ptr, PreFrameInfo.bitmapsize);
						PreFrameInfo.page = gpage.page + 1;
					}
				}else{
					nosupport = TRUE; // 対応していない処理
				}
			}

			// 合成方法
			if ( fcs->blend_op == APNG_BLEND_OP_OVER ){
				tmode = TMODE_PNGMODE; // 合成が必要
				if ( gpage.trans == -1 ) gpage.trans = 0;
			}else if ( fcs->blend_op >= 2 ){
				nosupport = TRUE; // 対応していない処理
			}

			// フレーム内容をコピー
			while ( srcoffset < vo_.file.UseSize ){
				DWORD csize;

				pc = (PNGchunk *)(vo_.file.image + srcoffset);
				csize = GetBigEndDWORD(&pc->length);
				if ( csize == 0 ) break;
				if ( pc->id == PNG_fcTL ) break; // fcTL...次のフレームに到達

				if ( pc->id == PNG_fdAT ){
					PNGchunk *destpc;

					destpc = (PNGchunk *)(tempimg + destoffset);
					destpc->id = PNG_IDAT; // IDAT に変更
					SetBigEndDWORD((BYTE *)&destpc->length, csize - SizeOfPNGframes);
					memcpy(tempimg + destoffset + sizeof(PNGchunk),
						vo_.file.image + srcoffset + sizeof(PNGchunk) + SizeOfPNGframes,
						csize - SizeOfPNGframes);
					SetBigEndDWORD((BYTE *)destpc + sizeof(PNGchunk) + csize - SizeOfPNGframes,
						crc32((const BYTE *)&destpc->id, csize + SizeOfPNGid - SizeOfPNGframes, 0) );
					destoffset += csize + SizeOfPNGchunks - SizeOfPNGframes;
				}else{
					memcpy(tempimg + destoffset, vo_.file.image + srcoffset,
						csize + SizeOfPNGchunks);
					destoffset += csize + SizeOfPNGchunks;
				}
				srcoffset += csize + SizeOfPNGchunks;
			}
			// 残りをコピー
			memcpy(tempimg + destoffset, vo_.file.image + srcoffset,
					vo_.file.UseSize - srcoffset);
			tempsize = destoffset + (vo_.file.UseSize - srcoffset);
		}
	}else{
		return;
	}

	if ( VFSGetDibDelay(filename,
			tempimg, tempsize, vo_.file.typeinfo, &hHeader, &hImage, NULL) ){
		SIZE newsize;

		NewBMPinfo = (BITMAPINFO *)LocalLock(hHeader);
		newsize.cx = NewBMPinfo->bmiHeader.biWidth;
		newsize.cy = NewBMPinfo->bmiHeader.biHeight;

		if ( newsize.cy < 0 ) newsize.cy = -newsize.cy; // トップダウン
		PreFrameInfo.size = newsize;
		PreFrameInfo.offset = gpage.dispoffset;

		// 色深度が違う場合は加工できない
		if ( (vo_.bitmap.info->biBitCount != NewBMPinfo->bmiHeader.biBitCount) &&
			 !((tmode == TMODE_GIFMODE) && (vo_.bitmap.info->biBitCount == 8)) ){
			// 透明指定有りか大きさが異なっている
			if ( (gpage.trans >= 0) ||
			  (vo_.bitmap.rawsize.cx != newsize.cx) || (vo_.bitmap.rawsize.cy != newsize.cy) ){
				nosupport = TRUE;
			}
		}

		if ( gpage.page &&
			((vo_.bitmap.info->biBitCount == NewBMPinfo->bmiHeader.biBitCount)||
			((tmode == TMODE_GIFMODE) && (vo_.bitmap.info->biBitCount == 8))) &&
			// 大きさが収まっている
			(vo_.bitmap.rawsize.cy >= (gpage.dispoffset.y + newsize.cy)) &&
			(vo_.bitmap.rawsize.cx >= (gpage.dispoffset.x + newsize.cx)) &&
			// 透明指定有りか大きさが異なっている
			( (gpage.trans >= 0) ||
			  (vo_.bitmap.rawsize.cx != newsize.cx) || (vo_.bitmap.rawsize.cy != newsize.cy) ) ){
			// 合成処理を行う
			BYTE *newimg, *src, *dst;
			int h, i, trans, srcwidth, dstwidth, dstoffset, copywidth;
			BOOL paletteFix = FALSE;
			BYTE fixpals[256];
			DWORD *newpal;

			if ( vo_.bitmap.info->biBitCount <= 8 ){
				DWORD *oldpal;

				oldpal = (DWORD *)((BYTE *)vo_.bitmap.info + vo_.bitmap.info->biSize);
				newpal = (DWORD *)((BYTE *)NewBMPinfo + NewBMPinfo->bmiHeader.biSize);

				if ( 0 != memcmp(oldpal, newpal,
					(size_t)(1 << NewBMPinfo->bmiHeader.biBitCount) * sizeof(DWORD) ) ){
					paletteFix = TRUE;
					if ( tmode == TMODE_GIFMODE ){ // 新しいパレットインデックスを旧パレットインデックスに変換する表を作成
						int ni, oi, newcols, oldcols;

						oldcols = 1 << vo_.bitmap.info->biBitCount;
						newcols = 1 << NewBMPinfo->bmiHeader.biBitCount;
						for ( ni = 0 ; ni < newcols ; ni++ ){
							DWORD oldindex, newpald;
							int paldelta;

							paldelta = 0xfffff;
							oldindex = oldcols - 1;
							newpald = newpal[ni];
							for ( oi = 0 ; oi < oldcols ; oi++ ){
								int chkdelta;

								chkdelta = getdelta(newpald, oldpal[oi]);
								if ( chkdelta < paldelta ){
									paldelta = chkdelta;
									oldindex = oi;
									if ( paldelta == 0 ) break;
								}
							}
							fixpals[ni] = (BYTE)oldindex;
						}
					}
				}
			}

			newimg = (BYTE *)LocalLock(hImage);

			if ( tmode != TMODE_NONE ){
				if ( tmode == TMODE_GIFMODE ){
					if ( (vo_.bitmap.info->biBitCount == 1) &&
						 (gpage.dispoffset.x & 7) ){
						tmode = TMODE_GIF_2X;
					}else if ( vo_.bitmap.info->biBitCount == 4 ){
						tmode = TMODE_GIF_4;
					}else if ( (vo_.bitmap.info->biBitCount == 8) &&
						(NewBMPinfo->bmiHeader.biBitCount == 4) ){
						tmode = TMODE_GIF_4to8;
					}else{
						tmode = TMODE_GIF_8_2C;
					}
					nosupport = FALSE;
				}else if ( tmode == TMODE_PNGMODE ){
					if ( vo_.bitmap.info->biBitCount == 8 ){
						tmode = TMODE_PNG_8;
					}else if ( vo_.bitmap.info->biBitCount == 32 ){
						tmode = TMODE_PNG_32;
					}else if ( vo_.bitmap.info->biBitCount == 24 ){
						tmode = TMODE_PNG_24;
					}else{
						tmode = TMODE_NONE;
					}
				}
			}

			// 8bitカラー以外は透過色をひとまずバイト単位のみで扱う●
			trans = gpage.trans;
			if ( (trans >= 0) && (vo_.bitmap.info->biBitCount < 8) ){
				if ( vo_.bitmap.info->biBitCount == 1 ){
					trans = -1;
				}else{
					trans = trans | (trans << 4);
				}
				if ( gpage.dispoffset.x & ((8 / NewBMPinfo->bmiHeader.biBitCount) - 1) ){
					nosupport = TRUE;
				}
			}

			srcwidth = DwordBitSize(newsize.cx * NewBMPinfo->bmiHeader.biBitCount);
			dstwidth = DwordBitSize(vo_.bitmap.rawsize.cx * vo_.bitmap.info->biBitCount);
			dstoffset = BitPosMacro(gpage.dispoffset.x * vo_.bitmap.info->biBitCount);
			copywidth = BitSizeMacro(newsize.cx * NewBMPinfo->bmiHeader.biBitCount);

			for ( h = 1 ; h <= newsize.cy ; h++ ){
				src = newimg + (newsize.cy - h) * srcwidth;
				dst = vo_.bitmap.bits.ptr
					+ (vo_.bitmap.rawsize.cy - gpage.dispoffset.y - h) * dstwidth
					+ dstoffset;

				switch ( tmode ){
					case TMODE_GIF_2X: {
						int bitx;

						bitx = gpage.dispoffset.x & 7;
						for ( i = copywidth ; i ; i--, src++, dst++ ){
							WORD srcdots, dstdots;

							srcdots = (WORD)((WORD)*src << (8 - bitx));
							dstdots = (WORD)(
								((((WORD)*dst << 8) | (WORD)*(dst + 1)) &
								 ~(0xff00 >> bitx)) | srcdots);
							*dst = (BYTE)(WORD)(dstdots >> 8);
							*(dst + 1) = (BYTE)dstdots;
						}
						continue;
					}
					case TMODE_GIF_4: {
						int skip_i;

						skip_i = (newsize.cx & 1) ? 1 : 0;
						if ( gpage.dispoffset.x & 1 ){ // 4bit 列が不一致
							for ( i = copywidth ; i ; i--, src++, dst++ ){
								BYTE srcdots;

								srcdots = *src;
								if ( ((srcdots >> 4) != gpage.trans) &&
									 (i > skip_i) ){
									*dst = (BYTE)((*dst & 0xf0) |
											(paletteFix ?
											 fixpals[srcdots >> 4] : (srcdots >> 4)) );
								}
								if ( (srcdots & 0xf) != gpage.trans ){
									*(dst + 1) = (BYTE)((*(dst + 1) & 0xf) |
											(paletteFix ?
											 (fixpals[srcdots & 0xf] << 4) : (srcdots << 4)) );
								}
							}
						}else{	// 4bit 列が一致
							for ( i = copywidth ; i ; i--, src++, dst++ ){
								BYTE srcdots, destdots, dot;

								srcdots = *src;
								destdots = *dst;

								dot = (BYTE)(srcdots & 0xf);
								if ( (dot != gpage.trans) && (i > skip_i) ){
									destdots = (BYTE)((destdots & 0xf0) |
											(paletteFix ? fixpals[dot] : dot) );
								}
								dot = (BYTE)(srcdots >> 4);
								if ( dot != gpage.trans ){
									destdots = (BYTE)((destdots & 0xf) |
											(paletteFix ? (fixpals[dot] << 4) : (dot << 4)) );
								}
								*dst = destdots;
							}
						}
						continue;
					}
					case TMODE_GIF_8_2C:
						for ( i = copywidth ; i ; i--, src++, dst++ ){
							if ( *src != trans ){
								if ( IsTrue(paletteFix) ){
									*dst = fixpals[*src];
								}else{
									*dst = *src;
								}
							}
						}
						continue;

					case TMODE_GIF_4to8: {
						int skip_i;

						skip_i = (newsize.cx & 1) ? 1 : 0;
						if ( gpage.dispoffset.x & 1 ){ // 4bit 列が不一致
							for ( i = copywidth ; i ; i--, src++, dst+=2 ){
								BYTE srcdots, dot;

								srcdots = *src;
								dot = (BYTE)(srcdots >> 4);
								if ( (dot != gpage.trans) && (i > skip_i) ){
									*dst = paletteFix ? fixpals[dot] : dot;
								}
								dot = (BYTE)(srcdots & 0xf);
								if ( dot != gpage.trans ){
									*(dst + 1) = paletteFix ? fixpals[dot] : dot;
								}
							}
						}else{	// 4bit 列が一致
							for ( i = copywidth ; i ; i--, src++, dst+=2 ){
								BYTE srcdots, dot;

								srcdots = *src;
								dot = (BYTE)(srcdots & 0xf);
								if ( (dot != gpage.trans) && (i > skip_i) ){
									*dst = paletteFix ? fixpals[dot] : dot;
								}
								dot = (BYTE)(srcdots & 0x4);
								if ( dot != gpage.trans ){
									*(dst + 1) = paletteFix ? fixpals[dot] : dot;
								}
							}
						}
						continue;
					}
					case TMODE_PNG_8:
						for ( i = copywidth ; i ; i--, src++, dst++ ){
							if ( newpal[*src] >= 0x60000000 ) *dst = *src;
						}
						continue;

					case TMODE_PNG_24:
						for ( i = copywidth ; i ; i -= 3, src+=3, dst+=3 ){
							DWORD color;

							color = *(DWORD *)src;
							if ( (color & 0xffffff) != (DWORD)trans ){
								*(WORD *)dst = (WORD)color;
								*(BYTE *)(dst + 2) = (BYTE)(color >> 16);
							}
						}
						continue;

					case TMODE_PNG_32:
						for ( i = copywidth ; i ; i -= 4, src+=4, dst+=4 ){
							if ( *(DWORD *)src >= 0x60000000 ){
								*(DWORD *)dst = *(DWORD*)src;
							}
						}
						continue;

					default:
						memcpy(dst, src, copywidth);
				}
			}

			LocalUnlock(hHeader);
			LocalFree(hHeader);

			LocalUnlock(hImage);
			LocalFree(hImage);
		}else{
			// 合成処理を行わず、差し替え
			if ( vo_.bitmap.info_hlocal != NULL ){
				LocalUnlock(vo_.bitmap.info_hlocal);
				LocalFree(vo_.bitmap.info_hlocal);
			}
			if ( vo_.bitmap.bits.mapH != NULL ){
				LocalUnlock(vo_.bitmap.bits.mapH);
				LocalFree(vo_.bitmap.bits.mapH);
			}
			vo_.bitmap.info_hlocal = hHeader;
			vo_.bitmap.bits.mapH = hImage;

			vo_.bitmap.info = &NewBMPinfo->bmiHeader;
			vo_.bitmap.bits.ptr = LocalLock(hImage);
		}
		if ( !vo_.bitmap.info->biClrUsed && (vo_.bitmap.info->biBitCount <= 8) ){
			vo_.bitmap.info->biClrUsed = 1 << vo_.bitmap.info->biBitCount;
		}

		vo_.bitmap.ShowInfo = vo_.bitmap.info;
		DIRECTXDEFINE(DxDrawFreeBMPCache(&vo_.bitmap.DxCache));
		if ( vo_.bitmap.hPal != NULL ) DeleteObject(vo_.bitmap.hPal);
		CreateDIBtoPalette(&vo_);

		vo_.SupportTypeFlags = VO_type_ALLIMAGE;
		vo_.DModeType = DISPT_IMAGE;
		FixChangeMode(vinfo.info.hWnd);

		vo_.bitmap.page.current = gpage.page;
		if ( vo_.bitmap.page.animate ){
			if ( gpage.timer == 0 ) gpage.timer = 10;
			SetTimer(vinfo.info.hWnd, TIMERID_ANIMATE, gpage.timer, AnimateProc);
		}
		if ( nosupport ){
			SetPopMsg(POPMSG_NOLOGMSG, T("this frame not full support"));
		}
	}else{
		SetPopMsg(POPMSG_NOLOGMSG, T("frame read error"));
	}
	HeapFree(PPvHeap, 0, tempimg);
}

void ReMakeIndexes(HWND hWnd)
{
	ScrollWidth = ScrollWidth_MIN;
	if ( vo_.DModeBit & DOCMODE_TEXT ){
		if ( VOsel.select != FALSE ) ResetSelect(TRUE); // 選択を解除
		MakeIndexTable(MIT_REMAKE, VOi->offY);
		SetScrollBar();
		if ( mtinfo.PresetPos == 0 ){
			MoveCsr(0, mtinfo.PresetY - VOi->offY, FALSE);
		}
	}else{
		mtinfo.PresetY = VOi->offY; // 読み込み完了後の表示位置を指定(●1.2x暫定)
		FixChangeMode(hWnd);
	}
}
