/*-----------------------------------------------------------------------------
	Paper Plane xUI		BMP操作 (c)TORO
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include <windowsx.h>
#include "PPX.H"
#include "VFS.H"
#pragma hdrstop

#ifndef COLOR_BTNFACE
#define COLOR_BTNFACE 15
#endif

/* DIBのヘッダからパレットを作成する */
HPALETTE DIBtoPalette(HTBMP *hTbmp, int mode, int maxY)
{
	struct {
		WORD palVersion;
		WORD palNumEntries;
		PALETTEENTRY palPalEntry[256];
	} lPal;
	PALETTEENTRY *ppal;
	RGBQUAD *rgb;
	int r, g, b;
	DWORD ClrUsed;
	BYTE *src, *dst;
	BITMAPINFOHEADER *dib;

	dib = hTbmp->DIB;
	ClrUsed = dib->biClrUsed ? dib->biClrUsed : (DWORD)(1 << dib->biBitCount);
	lPal.palNumEntries = (WORD)ClrUsed;
	lPal.palVersion = 0x300;
	ppal = lPal.palPalEntry;
	if ( ClrUsed > 256 ){	//フルカラー画像は全体に散らばったパレットを作成
		HDC hDC;
		int VideoBits;

		hDC = GetDC(NULL);
		VideoBits = GetDeviceCaps(hDC, BITSPIXEL);
		ReleaseDC(NULL, hDC);
		if ( VideoBits > 8 ) return NULL; // 画面が256超なのでそのまま表示可能

		rgb = hTbmp->nb.rgb2;
		lPal.palNumEntries = 6*6*6;
		for( b = 0 ; b <= 255 ; b += 51 ){
			for(g = 0 ; g <= 255 ; g += 51){
				for(r = 0 ; r <= 255 ; r += 51, ppal++){
					rgb->rgbRed = ppal->peRed     = (BYTE)r;
					rgb->rgbGreen = ppal->peGreen = (BYTE)g;
					rgb->rgbBlue = ppal->peBlue   = (BYTE)b;
					rgb->rgbReserved = ppal->peFlags = 0;
					rgb++;
				}
			}
		}

		if ( dib->biBitCount == 24 ){
			int x, y;

			src = dst = hTbmp->bits;
			dib->biSizeImage = hTbmp->size.cx * maxY;
			for ( y = 0; y < maxY ; y++ ){
				for ( x = 0; x < dib->biWidth ; x++ ){
					r = ((*(src++) +25)/ 51);	// red
					g = ((*(src++) +25)/ 51);	// green
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
			hTbmp->nb.dib2 = *dib;
			hTbmp->DIB = &hTbmp->nb.dib2;
		}
	}else{
		DWORD ci;

		rgb = (LPRGBQUAD)((BYTE *)hTbmp->DIB + hTbmp->PaletteOffset);
		if ( IsBadReadPtr(rgb, ClrUsed * sizeof(RGBQUAD)) ) return NULL;

		for ( ci = 0; ci < ClrUsed; ci++, rgb++, ppal++ ){
			if ( mode == BMPFIX_TOOLBAR ){
				if ( (rgb->rgbRed == 255) && (rgb->rgbGreen == 0) &&
					(rgb->rgbBlue == 255) ){
					DWORD clr;

					clr = GetSysColor(COLOR_BTNFACE);
					rgb->rgbRed = (BYTE)clr;
					rgb->rgbGreen = (BYTE)(clr >> 8);
					rgb->rgbBlue = (BYTE)(clr >> 16);
				}
			}
			ppal->peRed = rgb->rgbRed;
			ppal->peGreen = rgb->rgbGreen;
			ppal->peBlue = rgb->rgbBlue;
			ppal->peFlags = 0;
		}
	}
	return CreatePalette((LOGPALETTE *)&lPal);
}

#define ColorFix(src, bright) \
{\
	int c;\
	c = *src * bright >> 8;\
	if ( c >= 0x100 ) c = 0xff;\
	*src++ = (BYTE)c;\
}

PPXDLL BOOL PPXAPI InitBMP(HTBMP *hTbmp, const TCHAR *filename, DWORD size, int bright)
{
	int palette, maxY;
	DWORD offset;
	DWORD color;

	if ( size < 8 ) goto error;
	if (	(*(hTbmp->dibfile + 0) != 'B')	||
			(*(hTbmp->dibfile + 1) != 'M')	||
			(*(hTbmp->dibfile + 5) != 0)	||
			(*(hTbmp->dibfile + 6) != 0)	||
			(*(hTbmp->dibfile + 7) != 0)	){
		VFSOn(VFS_BMP);
		if ( !VFSGetDibDelay(filename, hTbmp->dibfile, size,
					(bright == BMPFIX_PREVIEW) ? (TCHAR *)(DWORD_PTR)1 : NULL,
					&hTbmp->info, &hTbmp->bm, NULL) ){ // C4306ok
			VFSOff();
			if ( hTbmp->hPalette != NULL ) return FALSE;
			goto error;
		}
		VFSOff();
		if ( (hTbmp->DIB = LocalLock(hTbmp->info)) == NULL ) goto error;
		if ( (hTbmp->bits = LocalLock(hTbmp->bm)) == NULL ) goto error;
		size = (DWORD)LocalSize(hTbmp->bm);
		HeapFree(GetProcessHeap(), 0, hTbmp->dibfile);
		hTbmp->dibfile = NULL;
	}else{
		if ( size < (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) ){
			goto error;
		}
		hTbmp->DIB = (BITMAPINFOHEADER *)
				(hTbmp->dibfile + sizeof(BITMAPFILEHEADER));
		hTbmp->bits = NULL;
	}
	hTbmp->hPalette = NULL;

	offset = hTbmp->DIB->biSize;
	color = hTbmp->DIB->biBitCount;
	palette = hTbmp->DIB->biClrUsed;
	if ( (offset < (sizeof(BITMAPINFOHEADER) + 12) ) && (hTbmp->DIB->biCompression == BI_BITFIELDS) ){
		offset += 12;	// 16/32bit のときはビット割り当てがある
	}
	hTbmp->PaletteOffset = offset;
#pragma warning(suppress: 6297) // color は 8以下で、DWORD を越えることがない
	offset += palette ? palette * sizeof(RGBQUAD) :
			((color <= 8) ? (DWORD)(1 << color) * sizeof(RGBQUAD) : 0);
	if ( size < (offset + sizeof(BITMAPFILEHEADER)) ) goto error;

	if ( hTbmp->bits == NULL ){
		size -= sizeof(BITMAPFILEHEADER) + offset;
		hTbmp->bits = (BYTE *)hTbmp->DIB + offset;
	}
	hTbmp->size.cx = hTbmp->DIB->biWidth;
	hTbmp->size.cy = hTbmp->DIB->biHeight;
	if ( (hTbmp->size.cy == 0) || (hTbmp->size.cx <= 0) || (color == 0) || (color > 32) ){ // 大きさ異常?
		FreeBMP(hTbmp);
		return FALSE;
	}
	if ( hTbmp->size.cy < 0 ) hTbmp->size.cy = -hTbmp->size.cy; // トップダウン
	if ( palette && (color <= 8) ) hTbmp->DIB->biClrUsed = 1 << color;

	maxY = size / DwordBitSize(hTbmp->size.cx * color);
	if ( maxY <= 0 ){ // 大きさ異常
		FreeBMP(hTbmp);
		return FALSE;
	}
	if ( maxY > hTbmp->size.cy ) maxY = hTbmp->size.cy;

	if ( (bright != 100) && (bright > -200) ){ // 明度調節
		BYTE *src;
		int x, y, xm;

		bright = bright * 256 / 100;
		if ( color == 32 ){
			src = hTbmp->bits;
			xm = hTbmp->size.cx;

			for ( y = maxY ; y ; y-- ){
				for ( x = xm ; x ; x-- ){
					ColorFix(src, bright);
					ColorFix(src, bright);
					ColorFix(src, bright);
					src++;
				}
			}
		}else if ( color == 24 ){
			src = hTbmp->bits;
			xm = hTbmp->size.cx;

			for ( y = maxY ; y ; y-- ){
				for ( x = xm ; x ; x-- ){
					ColorFix(src, bright);
					ColorFix(src, bright);
					ColorFix(src, bright);
				}
				src += (4 - ALIGNMENT_BITS(src)) & 3;
			}
		}else if ( color == 16 ){
			src = hTbmp->bits;
			xm = hTbmp->size.cx;

			for ( y = maxY ; y ; y-- ){
				for ( x = xm ; x ; x-- ){
					WORD csrc, cdest, c;
// ビット割当てが 565
					csrc = *(WORD *)src;
					c = (WORD)((int)(csrc & 0x1f) * bright >> 8);
					if ( c >= 0x20 ) c = 0x1f;
					cdest = c;
					csrc >>= 5;

					c = (WORD)((int)(csrc & 0x3f) * bright >> 8);
					if ( c >= 0x40 ) c = 0x3f;
					cdest |= (WORD)(c << 5);
					csrc >>= 6;

					c = (WORD)((int)csrc * bright >> 8);
					if ( c >= 0x20 ) c = 0x1f;
					cdest |= (WORD)(c << (5 + 6));
/*
// ビット割当てが 555
					c = (WORD)((int)(csrc & 0x1f) * bright >> 8);
					if ( c >= 0x20 ) c = 0x1f;
					cdest |= (WORD)(c << 5);
					csrc >>= 5;

					c = (WORD)((int)(csrc & 0x1f) * bright >> 8);
					if ( c >= 0x20 ) c = 0x1f;
					cdest |= (WORD)(c << (5 + 5));
*/
					*(WORD *)src = cdest;
					src += 2;
				}
				src += (4 - ALIGNMENT_BITS(src)) & 3;
			}
		}else if ( color <= 8 ){
			src = (BYTE *)hTbmp->DIB + hTbmp->PaletteOffset;
			xm = hTbmp->DIB->biClrUsed ?
				hTbmp->DIB->biClrUsed : (DWORD)(1 << color);
			for ( x = xm ; x ; x-- ){
				ColorFix(src, bright);
				ColorFix(src, bright);
				ColorFix(src, bright);
				src++;
			}
		}
	}
	hTbmp->hPalette = DIBtoPalette(hTbmp, bright, maxY);
	return TRUE;
error:
	if ( hTbmp->dibfile != NULL ) HeapFree(GetProcessHeap(), 0, hTbmp->dibfile);
	hTbmp->DIB = NULL;
	hTbmp->hPalette = NULL;
	return FALSE;
}

PPXDLL BOOL PPXAPI LoadBMP(HTBMP *hTbmp, const TCHAR *filename, int bright)
{
	DWORD size;
	hTbmp->dibfile = NULL;
	hTbmp->hPalette = NULL;
	hTbmp->info = NULL;
	hTbmp->bm = NULL;

	if ( NO_ERROR != LoadFileImage(filename, 0,
			(char **)&hTbmp->dibfile, &size, (bright == 100) ? LFI_ALWAYSLIMIT : LFI_ALWAYSLIMITLESS) ){
		return FALSE;
	}
	return InitBMP(hTbmp, filename, size, bright);
}

PPXDLL BOOL PPXAPI DrawBMP(HDC hDC, _In_ HTBMP *hTbmp, int x, int y)
{
	HPALETTE hOldPal C4701CHECK;

	if ( (hTbmp == NULL) || (hTbmp->DIB == NULL) ) return FALSE;
											// hPalの論理パレットに変更する。
	if ( hTbmp->hPalette != NULL ){
		hOldPal = SelectPalette(hDC, hTbmp->hPalette, FALSE);
								// 論理パレットを現在のDCにマッピングする。
		RealizePalette(hDC);
	}
											// DIBを画面に転送
	SetDIBitsToDevice(hDC, x, y, hTbmp->size.cx, hTbmp->size.cy,
			0, 0, 0, hTbmp->size.cy, hTbmp->bits,
			(BITMAPINFO *)hTbmp->DIB, DIB_RGB_COLORS);
											// 前のパレットに戻す。
	if ( hTbmp->hPalette != NULL ) SelectPalette(hDC, hOldPal, FALSE); // C4701ok
	return TRUE;
}

PPXDLL BOOL PPXAPI FreeBMP(_Inout_ HTBMP *hTbmp)
{
	if ( (hTbmp == NULL) || (hTbmp->DIB == NULL) ) return FALSE;
	if ( hTbmp->hPalette != NULL) DeleteObject(hTbmp->hPalette);
	if ( hTbmp->dibfile != NULL ) HeapFree(GetProcessHeap(), 0, hTbmp->dibfile);
	if ( hTbmp->info != NULL ){
		LocalUnlock(hTbmp->info);
		LocalFree(hTbmp->info);
	}
	if ( hTbmp->bm != NULL ){
		LocalUnlock(hTbmp->bm);
		LocalFree(hTbmp->bm);
	}
	hTbmp->DIB = NULL;
	return TRUE;
}
