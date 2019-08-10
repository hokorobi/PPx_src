/*-----------------------------------------------------------------------------
	Paper Plane vUI		Bitmap Image
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop

#pragma pack(push, 4)
typedef struct {
	DWORD bV5Size;
	LONG  bV5Width;
	LONG  bV5Height;
	WORD  bV5Planes;
	WORD  bV5BitCount;
	DWORD bV5Compression;
	DWORD bV5SizeImage;
	LONG  bV5XPelsPerMeter;
	LONG  bV5YPelsPerMeter;
	DWORD bV5ClrUsed;
	DWORD bV5ClrImportant;
	DWORD bV5RedMask;
	DWORD bV5GreenMask;
	DWORD bV5BlueMask;
	DWORD bV5AlphaMask;
	DWORD bV5CSType;
	DWORD bV5Endpoints_[3 * 3];
	DWORD bV5GammaRed;
	DWORD bV5GammaGreen;
	DWORD bV5GammaBlue;
	DWORD bV5Intent;
	DWORD bV5ProfileData;
	DWORD bV5ProfileSize;
	DWORD bV5Reserved;
} BITMAPV5HEADER_;
#pragma pack(pop)

/* DIBのヘッダからパレットを作成する */
void CreateDIBtoPalette(PPvViewObject *vo)
{
	struct {
		WORD palVersion;
		WORD palNumEntries;
		PALETTEENTRY palPalEntry[256];
	} lPal;
	PALETTEENTRY *ppal;
	RGBQUAD *rgb;
	int ClrUsed;
	BYTE *src, *dst;
	BITMAPINFOHEADER *dib = vo->bitmap.ShowInfo;

	vo->bitmap.hPal = NULL;
	vo->bitmap.rawsize.cx = dib->biWidth;
	vo->bitmap.rawsize.cy = dib->biHeight;
	FixBitmapShowSize(vo);

	ClrUsed = dib->biClrUsed ? (int)dib->biClrUsed : 1 << dib->biBitCount;
	lPal.palNumEntries = (WORD)ClrUsed;
	lPal.palVersion = 0x300;
	ppal = lPal.palPalEntry;
	if ( ClrUsed > 256 ){ //フルカラー画像は全体に散らばったパレットを作成
		int r, g, b;

		if ( VideoBits > 8 ){
			return; // パレット作成不要
		}else{
			struct {
				BITMAPINFOHEADER dib2;
				RGBQUAD rgb2[256];
			} nb;
			rgb = nb.rgb2;
			lPal.palNumEntries = 6*6*6;
			for ( b = 0 ; b <= 255 ; b += 51 ){
				for ( g = 0 ; g <= 255 ; g += 51 ){
					for ( r = 0 ; r <= 255 ; r += 51, ppal++){
						rgb->rgbRed = ppal->peRed   = (BYTE)r;
						rgb->rgbGreen = ppal->peGreen = (BYTE)g;
						rgb->rgbBlue = ppal->peBlue  = (BYTE)b;
						rgb->rgbReserved = ppal->peFlags = 0;
						rgb++;
					}
				}
			}

			if ( dib->biBitCount == 24 ){
				int x, y;

				src = dst = vo->bitmap.bits.ptr;
				dib->biSizeImage = vo->bitmap.rawsize.cx * vo->bitmap.rawsize.cy;
				for ( y = 0; y < vo->bitmap.rawsize.cy ; y++ ){
					for ( x = 0; x < dib->biWidth ; x++ ){
						r = ((*(src++) + 25) / 51);	// red
						g = ((*(src++) + 25) / 51);	// green
													// blue
						*dst++ = (BYTE)(r * 36 + g * 6 + ((*(src++) +25)/ 51));
					}
					src += (4 - ALIGNMENT_BITS(src)) & 3;
					dst += (4 - ALIGNMENT_BITS(dst)) & 3;
				}
				dib->biBitCount = 8;
				dib->biCompression = BI_RGB;
				dib->biClrUsed = 6*6*6;
				dib->biClrImportant = 0;
				nb.dib2 = *dib;

				HeapFree(PPvHeap, 0, vo->bitmap.ShowInfo);
				vo->bitmap.AllocShowInfo = FALSE;
				vo->bitmap.ShowInfo = &nb.dib2;
			}
		}
	}else{
		BYTE alpha = 0;
		int i;

		rgb = (LPRGBQUAD)((LPSTR)dib + sizeof(BITMAPINFOHEADER));
		if ( IsBadReadPtr(rgb, ClrUsed * sizeof(RGBQUAD)) ) return;
		for ( i = 0 ; i < ClrUsed ; i++, rgb++, ppal++ ){
			ppal->peRed   = rgb->rgbRed;
			ppal->peGreen = rgb->rgbGreen;
			ppal->peBlue  = rgb->rgbBlue;
			ppal->peFlags = 0;
			alpha |= rgb->rgbReserved;
		}
		if ( alpha > 0x80 ){ // パレットにα値が使用されていそう→透明パレットを決定
			rgb = (LPRGBQUAD)((LPSTR)dib + sizeof(BITMAPINFOHEADER));
			alpha = 0x60;
			for ( i = 0 ; i < ClrUsed ; i++, rgb++ ){
				if ( alpha > rgb->rgbReserved ){
					alpha = rgb->rgbReserved;
					vo->bitmap.transcolor = i;
				}
			}
		}
	}
	vo->bitmap.hPal = CreatePalette((LOGPALETTE *)&lPal);
}

void ClipBitmap(HWND hWnd)
{
	DWORD palettesize = 0, profilesize = 0;
	DWORD bitmapsize = 0, tmpsize, headersize;
	HGLOBAL hImage;
	char *img;
	int color, palette C4701CHECK;
	UINT ClipType = CF_DIB;

	HGLOBAL hImageNP;
	char *imgNP;

	headersize = vo_.bitmap.info->biSize;
	if ( headersize < sizeof(BITMAPINFOHEADER) ){
												// OS/2 形式
		color = ((BITMAPCOREHEADER *)vo_.bitmap.info)->bcBitCount;
		if ( color <= 8 ){
			palette = 1 << color;
			palettesize = palette * sizeof(RGBTRIPLE);
		}
	}else{								// WINDOWS 形式
		color = vo_.bitmap.info->biBitCount;
		palette = vo_.bitmap.info->biClrUsed;
		palettesize += palette ? palette * sizeof(RGBQUAD) :
				((color <= 8) ? (DWORD)(1 << color) * sizeof(RGBQUAD) : 0);
		bitmapsize = vo_.bitmap.info->biSizeImage;

		if ( (vo_.bitmap.info->biCompression == BI_BITFIELDS) &&
			 (headersize < (sizeof(BITMAPINFOHEADER) + 12)) ){
			headersize += 12;	// 16/32bit のときはビット割り当てがある
		}

		if ( headersize == sizeof(BITMAPV5HEADER_) ){
			ClipType = CF_DIBV5;
			if ( (((BITMAPV5HEADER_ *)vo_.bitmap.info)->bV5ProfileSize > 0) &&
				 (((BITMAPV5HEADER_ *)vo_.bitmap.info)->bV5ProfileData == sizeof(BITMAPV5HEADER_))
			){
				profilesize = ((BITMAPV5HEADER_ *)vo_.bitmap.info)->bV5ProfileSize;
			}
		}
	}
	if ( color == 1 ){		/* color = 1;(省略)*/
	}else if ( color <= 4  ){	color = 4;
	}else if ( color <= 8  ){	color = 8;
	}else if ( color <= 16 ){	color = 16;
	}else if ( color <= 24 ){	color = 24;
	}else {						color = 32;
	}
	vo_.bitmap.rawsize.cx = vo_.bitmap.ShowInfo->biWidth;
	vo_.bitmap.rawsize.cy = vo_.bitmap.ShowInfo->biHeight;
	FixBitmapShowSize(&vo_);

	tmpsize = DwordBitSize(vo_.bitmap.rawsize.cx * color) * vo_.bitmap.rawsize.cy;
	if ( (bitmapsize == 0) || (tmpsize > bitmapsize) ) bitmapsize = tmpsize;

	hImage = GlobalAlloc(GMEM_MOVEABLE, headersize + palettesize + bitmapsize + profilesize);
	if ( hImage != NULL ){
		img = GlobalLock(hImage);
		if ( img == NULL ){
			GlobalFree(hImage);
			return;
		}
		memcpy(img, vo_.bitmap.info, headersize);
		if ( headersize >= sizeof(BITMAPINFOHEADER) ){ // biSizeImage を修正
			((BITMAPINFOHEADER *)img)->biSizeImage = bitmapsize;
		}
		if ( palettesize && (vo_.bitmap.hPal != NULL) ){
			PALETTEENTRY *pe;
			int i;

			pe = (PALETTEENTRY *)(img + vo_.bitmap.info->biSize);
			GetPaletteEntries(vo_.bitmap.hPal, 0, palette, pe); // C4701ok
						// PALETTEENTRY → RGBQUAD に修復
			for ( i = 0 ; i < palette ; i++, pe++ ){
				BYTE tmp;

				tmp = pe->peRed;
				pe->peRed = pe->peBlue;
				pe->peBlue = tmp;
			}
		}
		// bitmap 転送
		memcpy(img + headersize + palettesize, vo_.bitmap.bits.ptr, bitmapsize);

		if ( profilesize != 0 ){ // ICC プロファイルあり
			 ((BITMAPV5HEADER_ *)img)->bV5ProfileData = headersize + palettesize + bitmapsize;
			// 末尾にプロファイルを用意
			memcpy(img + headersize + palettesize + bitmapsize, (char *)vo_.bitmap.info + headersize, profilesize);

			hImageNP = GlobalAlloc(GMEM_MOVEABLE, headersize + palettesize + bitmapsize);
			// プロファイル無しBMPを用意
			if ( hImageNP != NULL ){
				imgNP = GlobalLock(hImageNP);
				memcpy(imgNP, img, headersize + palettesize + bitmapsize);
				((BITMAPV5HEADER_ *)imgNP)->bV5ProfileData = 0;
				GlobalUnlock(hImageNP);
			}
		}
		GlobalUnlock(hImage);
	}
	if ( hImage == NULL ){
		SetPopMsg(POPMSG_NOLOGMSG, T("Clip error"));
	}else{
		OpenClipboardV(hWnd);
		EmptyClipboard();
		SetClipboardData(ClipType, hImage);
		if ( (profilesize != 0) && (hImageNP != NULL) ){ // profile無しを保存
			SetClipboardData(CF_DIB, hImageNP);
		}
		CloseClipboard();
		SetPopMsg(POPMSG_NOLOGMSG, MES_CPTX);
	}
	return;
}

void Rotate(HWND hWnd, int d)
{
	BITMAPINFOHEADER *NewBmpInfo;
	BYTE *NewDib, *src, *dest;
	HLOCAL hBmpInfo, hDib;
	DWORD width, x, y, srcwidth, bmpy;

	if ( vo_.DModeBit != DOCMODE_BMP ) return;
	if ( (vo_.bitmap.info->biBitCount > 32) ||
		 ((vo_.bitmap.info->biCompression != BI_RGB) &&
			(vo_.bitmap.info->biCompression != BI_BITFIELDS)) ){
		SetPopMsg(POPMSG_NOLOGMSG, T("Unsupport format"));
		return;
	}
	srcwidth = DwordBitSize(vo_.bitmap.rawsize.cx * vo_.bitmap.info->biBitCount);
	width = DwordBitSize(vo_.bitmap.rawsize.cy * vo_.bitmap.info->biBitCount);
	hDib = LocalAlloc(LMEM_FIXED, width * vo_.bitmap.rawsize.cx);
	if ( hDib == NULL ){
		SetPopMsg(POPMSG_MSG, T("Memory alloc error."));
		return;
	}
	NewDib = LocalLock(hDib);

	if ( vo_.bitmap.info_hlocal != NULL ){
		x = LocalSize(vo_.bitmap.info_hlocal); // mapHがNULLだと x64でデッドロック
	}else{
		x = 0;
	}
	if ( x == 0 ){
		x = sizeof(BITMAPINFOHEADER) + vo_.bitmap.info->biClrUsed * sizeof(RGBQUAD) + 12;
	}
	hBmpInfo = LocalAlloc(LMEM_FIXED, x);
	if ( hBmpInfo == NULL ) return;
	NewBmpInfo = LocalLock(hBmpInfo);
	memcpy(NewBmpInfo, vo_.bitmap.info, x);
	NewBmpInfo->biWidth = vo_.bitmap.info->biHeight;
	NewBmpInfo->biHeight = vo_.bitmap.info->biWidth;
	NewBmpInfo->biSizeImage = width * vo_.bitmap.rawsize.cx;
	bmpy = NewBmpInfo->biHeight;
	if ( NewBmpInfo->biWidth < 0 ){ // トップダウン
		NewBmpInfo->biWidth = -NewBmpInfo->biWidth;
		NewBmpInfo->biHeight = -NewBmpInfo->biHeight;
	}

	if ( d < 0 ){	// 左回り
		if ( vo_.bitmap.info->biBitCount == 32 ){
			for ( y = 0 ; y < (DWORD)bmpy ; y++ ){
				src = vo_.bitmap.bits.ptr + 4 * y + srcwidth * (NewBmpInfo->biWidth - 1);
				dest = NewDib + width * y;
				for ( x = NewBmpInfo->biWidth ; x ; x-- ){
					*(DWORD *)dest = *(DWORD *)src;
					dest += 4;
					src -= srcwidth;
				}
			}
		}else if ( vo_.bitmap.info->biBitCount == 24 ){
			for ( y = 0 ; y < (DWORD)bmpy ; y++ ){
				src = vo_.bitmap.bits.ptr + 3 * y + srcwidth * (NewBmpInfo->biWidth - 1);
				dest = NewDib + width * y;
				for ( x = NewBmpInfo->biWidth ; x ; x-- ){
					*dest++ = *src;
					*dest++ = *(src + 1);
					*dest++ = *(src + 2);
					src -= srcwidth;
				}
			}
		}else if ( vo_.bitmap.info->biBitCount == 16 ){
			for ( y = 0 ; y < (DWORD)bmpy ; y++ ){
				src = vo_.bitmap.bits.ptr + 2 * y + srcwidth * (NewBmpInfo->biWidth - 1);
				dest = NewDib + width * y;
				for ( x = NewBmpInfo->biWidth ; x ; x-- ){
					*(WORD *)dest = *(WORD *)src;
					dest += 2;
					src -= srcwidth;
				}
			}
		}else if ( vo_.bitmap.info->biBitCount == 8 ){
			for ( y = 0 ; y < (DWORD)bmpy ; y++ ){
				src = vo_.bitmap.bits.ptr + y + srcwidth * (NewBmpInfo->biWidth - 1);
				dest = NewDib + width * y;
				for ( x = NewBmpInfo->biWidth ; x ; x-- ){
					*dest++ = *src;
					src -= srcwidth;
				}
			}
		}else if ( vo_.bitmap.info->biBitCount == 4 ){
			BYTE bits, srcB;
			int bitI;

			bits = 0;
			for ( y = 0 ; y < (DWORD)bmpy ; y++ ){
				src = vo_.bitmap.bits.ptr + y / 2 + srcwidth * (NewBmpInfo->biWidth - 1);
				srcB = (BYTE)(y & 1);
				dest = NewDib + width * y;
				bitI = 0;
				for ( x = NewBmpInfo->biWidth ; x ; x-- ){
					bits = (BYTE)(bits << 4);
					bits |= !srcB ? (BYTE)(*src >> 4) : (BYTE)(*src & 0xf);
					if ( (bitI = ~bitI) == 0 ) *dest++ = bits;
					src -= srcwidth;
				}
				if ( bitI ) *dest = (BYTE)(bits << 4);
			}
		}else{ // vo_.bitmap.info->biBitCount == 1
			BYTE bits, srcB;
			int bitI;

			bits = 0;
			for ( y = 0 ; y < (DWORD)bmpy ; y++ ){
				src = vo_.bitmap.bits.ptr + y / 8 + srcwidth * (NewBmpInfo->biWidth - 1);
				srcB = (BYTE)(B7 >> (y & 7));
				dest = NewDib + width * y;
				bitI = 0;
				for ( x = (DWORD)NewBmpInfo->biWidth ; x ; x-- ){
					bits = (BYTE)(bits << 1);
					if ( *src & srcB ) bits |= LSBIT;
					if ( ((--bitI) & 7) == 0 ) *dest++ = bits;
					src -= srcwidth;
				}
				if ( bitI & 7 ) *dest = (BYTE)(bits << (bitI & 7));
			}
		}
	}else{			// 右回り
		if ( vo_.bitmap.info->biBitCount == 32 ){
			for ( y = 0 ; y < (DWORD)bmpy ; y++ ){
				src = vo_.bitmap.bits.ptr + 4 * (bmpy - y - 1);
				dest = NewDib + width * y;
				for ( x = (DWORD)NewBmpInfo->biWidth ; x ; x-- ){
					*(DWORD *)dest = *(DWORD *)src;
					dest += 4;
					src += srcwidth;
				}
			}
		}else if ( vo_.bitmap.info->biBitCount == 24 ){
			for ( y = 0 ; y < (DWORD)bmpy ; y++ ){
				src = vo_.bitmap.bits.ptr + 3 * (bmpy - y - 1);
				dest = NewDib + width * y;
				for ( x = NewBmpInfo->biWidth ; x ; x-- ){
					*dest++ = *src;
					*dest++ = *(src + 1);
					*dest++ = *(src + 2);
					src += srcwidth;
				}
			}
		}else if ( vo_.bitmap.info->biBitCount == 16 ){
			for ( y = 0 ; y < (DWORD)bmpy ; y++ ){
				src = vo_.bitmap.bits.ptr + 2 * (bmpy - y - 1);
				dest = NewDib + width * y;
				for ( x = NewBmpInfo->biWidth ; x ; x-- ){
					*(WORD *)dest = *(WORD *)src;
					dest += 2;
					src += srcwidth;
				}
			}
		}else if ( vo_.bitmap.info->biBitCount == 8 ){
			for ( y = 0 ; y < (DWORD)bmpy ; y++ ){
				src = vo_.bitmap.bits.ptr + (bmpy - y - 1);
				dest = NewDib + width * y;
				for ( x = NewBmpInfo->biWidth ; x ; x-- ){
					*dest++ = *src;
					src += srcwidth;
				}
			}
		}else if ( vo_.bitmap.info->biBitCount == 4 ){
			BYTE bits, srcB;
			int bitI;

			bits = 0;
			for ( y = 0 ; y < (DWORD)bmpy ; y++ ){
				src = vo_.bitmap.bits.ptr + (bmpy - y - 1) / 2;
				srcB = (BYTE)((bmpy - y - 1) & 1);
				dest = NewDib + width * y;
				bitI = 0;
				for ( x = NewBmpInfo->biWidth ; x ; x-- ){
					bits = (BYTE)(bits << 4);
					bits |= !srcB ? (BYTE)(*src >> 4) : (BYTE)(*src & 0xf);
					if ( (bitI = ~bitI) == 0 ) *dest++ = bits;
					src += srcwidth;
				}
				if ( bitI ) *dest = (BYTE)(bits << 4);
			}
		}else{ // vo_.bitmap.info->biBitCount == 1
			BYTE bits, srcB;
			int bitI;

			bits = 0;
			for ( y = 0 ; y < (DWORD)bmpy ; y++ ){
				src = vo_.bitmap.bits.ptr + (bmpy - y - 1) / 8;
				srcB = (BYTE)(B7 >> ((bmpy - y - 1) & 7));
				dest = NewDib + width * y;
				bitI = 0;
				for ( x = NewBmpInfo->biWidth ; x ; x-- ){
					bits = (BYTE)(bits << 1);
					if ( *src & srcB ) bits |= LSBIT;
					if ( ((--bitI) & 7) == 0 ) *dest++ = bits;
					src += srcwidth;
				}
				if ( bitI & 7 ) *dest = (BYTE)(bits << (bitI & 7));
			}
		}
	}
	DIRECTXDEFINE(DxDrawFreeBMPCache(&vo_.bitmap.DxCache));
	LocalUnlock(vo_.bitmap.info_hlocal);
	LocalUnlock(vo_.bitmap.bits.mapH);
	LocalFree(vo_.bitmap.info_hlocal);
	LocalFree(vo_.bitmap.bits.mapH);
	vo_.bitmap.info_hlocal = hBmpInfo;
	vo_.bitmap.bits.mapH = hDib;
	vo_.bitmap.ShowInfo = vo_.bitmap.info = NewBmpInfo;
	vo_.bitmap.bits.ptr = NewDib;

	if ( XV.img.AspectRate != 0 ){
		XV.img.AspectRate = (ASPACTX * ASPACTX) / XV.img.AspectRate;
	}

	vo_.bitmap.rawsize.cx = vo_.bitmap.info->biWidth;
	vo_.bitmap.rawsize.cy = vo_.bitmap.info->biHeight;
	FixBitmapShowSize(&vo_);

	if ( (d == -1) || (d == 1) ){
		vo_.bitmap.rotate += d;
		InvalidateRect(hWnd, NULL, TRUE);
		SetScrollBar();
	}
}

#pragma pack(push, 1)
typedef struct {
	BYTE B, G, R, A;
} MaskBits;
#pragma pack(pop)

#define backsize 6
#define backdefcolor 0xc0
#define backdefcolor2 0xe0

#define backdefcolor8 2

void ModifyAlpha16(void)
{
	BYTE *bit, *maxbit;
	int backleft, backcountX, backcountY, width;
	BYTE backLcolor, backcolor, transcolor, transcolorH;

	bit = vo_.bitmap.bits.ptr;
	width = DwordAlignment(vo_.bitmap.rawsize.cx / 2);
	maxbit = bit + width * vo_.bitmap.rawsize.cy;

	backleft = backsize / 2;
	backcountX = 0;
	backcountY = 0;
	backLcolor = backcolor = backdefcolor8;
	transcolor = (BYTE)vo_.bitmap.transcolor;
	transcolorH = (BYTE)(transcolor << 4);

	for ( ; bit < maxbit ; ){
		BYTE c = *bit;

		if ( (c & 0xf) == transcolor ) c = (BYTE)((c & 0xf0) | backcolor);
		if ( (c & 0xf0) == transcolorH ) c = (BYTE)((c & 0xf) | (backcolor << 4));
		*bit++ = c;

		if ( --backleft == 0 ){
			backcolor ^= 1;
			backleft = backsize / 2;
			backcountX += backsize / 2;
			if ( (backcountX + (backsize / 2)) >= width ){
				if ( backcountX < width ){
					backleft = width - backcountX;
				}else{
					backcountX = 0;
					backcountY++;
					if ( backcountY >= backsize ){
						backLcolor ^= 1;
						backcountY = 0;
					}
					backcolor = backLcolor;
				}
			}
		}
	}
}

void ModifyAlpha256(void)
{
	BYTE *bit, *maxbit;
	int backleft, backcountX, backcountY, width;
	BYTE backLcolor, backcolor, transcolor;

	bit = vo_.bitmap.bits.ptr;
	width = DwordAlignment(vo_.bitmap.rawsize.cx);
	maxbit = bit + width * vo_.bitmap.rawsize.cy;

	backleft = backsize;
	backcountX = 0;
	backcountY = 0;
	backLcolor = backcolor = backdefcolor8;
	transcolor = (BYTE)vo_.bitmap.transcolor;

	for ( ; bit < maxbit ; bit++ ){
		if ( *bit == transcolor ) *bit = backcolor;

		if ( --backleft == 0 ){
			backcolor ^= 1;
			backleft = backsize;
			backcountX += backsize;
			if ( (backcountX + backsize) >= width ){
				if ( backcountX < width ){
					backleft = width - backcountX;
				}else{
					backcountX = 0;
					backcountY++;
					if ( backcountY >= backsize ){
						backLcolor ^= 1;
						backcountY = 0;
					}
					backcolor = backLcolor;
				}
			}
		}
	}
}

void ModifyAlpha(HWND hWnd)
{
	MaskBits *bit, *maxbit;
	int backleft, backcountX, backcountY;
	WORD backLcolor, backcolor;

	if ( vo_.bitmap.info->biBitCount < 32 ){
		if ( vo_.bitmap.info->biBitCount == 8 ){
			ModifyAlpha256();
		}else if ( vo_.bitmap.info->biBitCount == 4 ){
			ModifyAlpha16();
		}
	}else{
		bit = (MaskBits *)vo_.bitmap.bits.ptr;
		// 32bit なのでビット境界調整は不要
		maxbit = bit + vo_.bitmap.rawsize.cx * vo_.bitmap.rawsize.cy;

		backleft = backsize;
		backcountX = 0;
		backcountY = 0;
		backLcolor = backcolor = backdefcolor;

		for ( ; bit < maxbit ; bit++ ){
			WORD A, backAcolor;

			A = (WORD)bit->A;
			if ( A != 0xff ){
				backAcolor = (WORD)(backcolor * (0x100 - A));
				bit->B = (BYTE)(WORD)( (((WORD)bit->B * A) + backAcolor) >> 8);
				bit->G = (BYTE)(WORD)( (((WORD)bit->G * A) + backAcolor) >> 8);
				bit->R = (BYTE)(WORD)( (((WORD)bit->R * A) + backAcolor) >> 8);
				bit->A = 0xff;
			}
			if ( --backleft == 0 ){
				backcolor ^= (backdefcolor ^ backdefcolor2);
				backleft = backsize;
				backcountX += backsize;
				if ( (backcountX + backsize) >= vo_.bitmap.rawsize.cx ){
					if ( backcountX < vo_.bitmap.rawsize.cx ){
						backleft = vo_.bitmap.rawsize.cx - backcountX;
					}else{
						backcountX = 0;
						backcountY++;
						if ( backcountY >= backsize ){
							backLcolor ^= (backdefcolor ^ backdefcolor2);
							backcountY = 0;
						}
						backcolor = backLcolor;
					}
				}
			}
		}
	}
	InvalidateRect(hWnd, NULL, TRUE);
}
