/*-----------------------------------------------------------------------------
	Paper Plane xUI			お手軽BMP表示
-----------------------------------------------------------------------------*/
typedef struct {
	BYTE *dibfile; // BMPファイルイメージ
	BYTE *bits; // ビットマップのイメージ
	HANDLE info,bm;
	HPALETTE hPalette;
	SIZE size; // 画像の大きさ(DIBの値はトップダウンもあるので使わないこと)
	BITMAPINFOHEADER *DIB;
	DWORD PaletteOffset;

	struct {		// 減色パレット
		BITMAPINFOHEADER dib2;
		RGBQUAD rgb2[256];
	} nb;
} HTBMP;

extern PPXDLL BOOL PPXAPI LoadBMP(HTBMP *hTbmp,const TCHAR *filename,int bright);
#define BMPFIX_TOOLBAR -1000
#define BMPFIX_PREVIEW -1001
extern PPXDLL BOOL PPXAPI DrawBMP(HDC hDC,_In_ HTBMP *hTbmp,int x,int y);
extern PPXDLL BOOL PPXAPI FreeBMP(_Inout_ HTBMP *hTbmp);
extern PPXDLL BOOL PPXAPI InitBMP(HTBMP *hTbmp,const TCHAR *filename,DWORD size,int bright);
