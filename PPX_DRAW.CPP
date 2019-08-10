/*-----------------------------------------------------------------------------
	DirectX による GDI 風描画

	座標系:
		Z = 0 の時、
		0 * D3D_ProjScaleX ≦ X ≦ クライアント幅 * D3D_ProjScaleX
		0 * D3D_ProjScaleY ≦ Y ≦ クライアント高 * D3D_ProjScaleY
-----------------------------------------------------------------------------*/
#define DWMGLASS 0 // Vista/7 のガラス効果による半透明を有効にする

#if defined(USEDIRECTX) || defined(USEDIRECTWRITE)
#include "WINAPI.H"
#include <stdio.h>
#include <wchar.h>
#include <ole2.h>
#if DWMGLASS
  #include "Dwmapi.h"
  #pragma comment(lib, "Dwmapi.lib")
#endif
#define HMONITOR_DECLARED
#include "PPX.H"
#include "VFS.H"
#include "PPX_DRAW.H"
#undef HMONITOR_DECLARED

#if DWMGLASS
  #define RenderTargetProperties_OPTION D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
#else
  #define RenderTargetProperties_OPTION
#endif

#define DEFAULT_DPI 96.0f
// Direct Write 版 ------------------------------------------------------------
#if DRAWMODE == DRAWMODE_DW
#include <wincodec.h>
#include <wincodecsdk.h>
#include <d2d1.h>
#include <dwrite.h>

#define D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT (D2D1_DRAW_TEXT_OPTIONS)0x00000004 // 8.1から

#define DXBMPCACHEITEMS 8 // キャッシュ可能なビットマップの数
#define DEFATLASSIZE 0x800 // BmpAtlas に使用するビットマップの辺の長さ

BOOL DxEnableDLL = FALSE;
HMODULE hD2D1 = NULL;
HMODULE hDWRITE = NULL;

DefineWinAPI(HRESULT, D2D1CreateFactory, (D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS *, IUnknown **));
DefineWinAPI(void, D2D1MakeRotateMatrix, (FLOAT, D2D1_POINT_2F, D2D1_MATRIX_3X2_F *));
DefineWinAPI(HRESULT, DWriteCreateFactory, (DWRITE_FACTORY_TYPE, REFIID, IUnknown **));

extern "C" UTCHAR GetCommandParameter(LPCTSTR *commandline, TCHAR *param, size_t paramlen);

D2D1_DRAW_TEXT_OPTIONS TextOption;

struct DXBRUSHSTRUCT {
	ID2D1SolidColorBrush *brush;
	COLORREF color;
};

extern "C" typedef struct tagDXDRAWSTRUCT {
	HWND hWnd;

	ID2D1Factory *d2dFactory;
	IWICImagingFactory *WicFactory;

	ID2D1HwndRenderTarget *HwndRenderTarget;
	int HwndRenderTargetCounter;
	D2D1_MATRIX_3X2_F DefaultTransform;
	D2D1::Matrix3x2F HitPointTransform;
	D2D1::Matrix3x2F BMPTransform;

	D2D1_POINT_2F lastpos;
	COLORREF BackColor;
	DXBRUSHSTRUCT BackBrush;
	DXBRUSHSTRUCT SubBrush;
	D2D1_SIZE_U clientArea;
	RECT clientBox;

	int RotateAngle;
	FLOAT DotScaleRate; // 現在(DWM)の画面dpiの比率 ※逆数
	D2D1_POINT_2F DeskScale; // 起動時(GDI)の画面dpi
	D2D1_POINT_2F DeskScaleRate; // 起動時(GDI)の画面dpi ※逆数

	// 文字描画
	IDWriteFactory *dwFactory;
	IDWriteTextFormat *textFormat; // textFormatMain / textFormatSub を示す
	IDWriteTextFormat *textFormatMain; // メインフォント
	IDWriteTextFormat *textFormatSub; // フォント
	DXBRUSHSTRUCT TextBrush; // 文字描画用ブラシ
	WCHAR fontName[LF_FACESIZE];
	WCHAR fontNameSub[LF_FACESIZE];
	float fontW, fontH;
	COLORREF BackTextColor;
	int BkMode;

	// ビットマップ
	ID2D1Bitmap *bmpCache[DXBMPCACHEITEMS];
	struct {
		ID2D1Bitmap *bmp;

		SIZE ItemSize; // Item の大きさ
		DWORD AtlasItems; // 保存可能数
		DWORD AtlasCols;  // １行分の数

		DWORD IndexMin; // 有効Indexの最小値、最大値
		DWORD IndexNow; // 末尾

		DWORD useIndex; // 読み書きIndex(-1:該当無し)
	} BmpAtlas;
} DXDRAWSTRUCT;

const WCHAR strtail[1] = {L'…'};

BOOL CreateDxDraw(DXDRAWSTRUCT **DxDrawPtr, HWND hWnd)
{
	DXDRAWSTRUCT *DxDraw;

	*DxDrawPtr = NULL;
	if ( DxEnableDLL == FALSE ){
		if ( hD2D1 != NULL ) return FALSE;

		hD2D1 = LoadLibrary(T("d2d1.dll"));
		if ( hD2D1 == NULL ){
			hD2D1 = static_cast<HMODULE>(INVALID_HANDLE_VALUE);
			xmessage(XM_FaERRld, T("d2d1.dll not found"));
			return FALSE;
		}
		GETDLLPROC(hD2D1, D2D1CreateFactory);
		GETDLLPROC(hD2D1, D2D1MakeRotateMatrix);

		if ( hDWRITE != NULL ) return FALSE;

		hDWRITE = LoadLibrary(T("dwrite.dll"));
		if ( hDWRITE == NULL ){
			hDWRITE = static_cast<HMODULE>(INVALID_HANDLE_VALUE);
			xmessage(XM_FaERRld, T("dwrite.dll not found"));
			return FALSE;
		}
		GETDLLPROC(hDWRITE, DWriteCreateFactory);
		DxEnableDLL = TRUE;
	}

#if DWMGLASS
	MARGINS margins = { -1 };

	DwmExtendFrameIntoClientArea(hWnd, &margins);
#endif
	*DxDrawPtr = DxDraw = new DXDRAWSTRUCT;
	DxDraw->hWnd = hWnd;
	DxDraw->HwndRenderTarget = NULL;
	DxDraw->HwndRenderTargetCounter = 0;
	memset(&DxDraw->bmpCache, 0, sizeof(DxDraw->bmpCache));
	DxDraw->BmpAtlas.bmp = NULL;
	DxDraw->BmpAtlas.IndexNow = 1;

	DxDraw->BackBrush.brush = NULL;
	DxDraw->SubBrush.brush = NULL;
	DxDraw->TextBrush.brush = NULL;
	DxDraw->textFormat = NULL;
	DxDraw->textFormatMain = NULL;
	DxDraw->textFormatSub = NULL;
	DxDraw->RotateAngle = 0;
	DxDraw->DotScaleRate = 1.0f;
	DxDraw->DefaultTransform = D2D1::Matrix3x2F::Identity();
	DxDraw->HitPointTransform = D2D1::Matrix3x2F::Identity();
//	DxDraw->BMPTransform = D2D1::Matrix3x2F::Identity();
	wcscpy(DxDraw->fontName, L"ＭＳ ゴシック");
	wcscpy(DxDraw->fontNameSub, L"ＭＳ Ｐゴシック");
	DxDraw->fontH = 12.0f;
	DxDraw->BkMode = OPAQUE;

	DxDraw->WicFactory = NULL;

#if 1
	if ( FAILED(DD2D1CreateFactory(/*D2D1_FACTORY_TYPE_MULTI_THREADED */ D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(*DxDraw->d2dFactory), NULL, reinterpret_cast<IUnknown**>(&DxDraw->d2dFactory))) )
#else
	D2D1_FACTORY_OPTIONS options;
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
	OutputDebugString(T("******************** D2D1CreateFactory *******************"));

	if ( FAILED(DD2D1CreateFactory(/*D2D1_FACTORY_TYPE_MULTI_THREADED */ D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(*DxDraw->d2dFactory), options, reinterpret_cast<IUnknown**>(&DxDraw->d2dFactory))) )
#endif
	{
		return FALSE;
	}
	if ( FAILED(DDWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(*DxDraw->dwFactory),
			reinterpret_cast<IUnknown**>(&DxDraw->dwFactory))) ){
		return FALSE;
	}

	if ( ((OSver.dwMajorVersion << 16) | OSver.dwMinorVersion) >= 0x60003 ){
		TextOption = D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT;
	}else{
		TextOption = D2D1_DRAW_TEXT_OPTIONS_NONE;
	}

	// ログオフしない限り値は変化しない
	DxDraw->d2dFactory->GetDesktopDpi(&DxDraw->DeskScale.x, &DxDraw->DeskScale.y);
	DxDraw->DeskScaleRate.x = DEFAULT_DPI / DxDraw->DeskScale.x;
	DxDraw->DeskScaleRate.y = DEFAULT_DPI / DxDraw->DeskScale.y;
	if ( DxDraw->DeskScale.x != DEFAULT_DPI ){
		DxDraw->DefaultTransform._11 = DxDraw->DeskScaleRate.x;
		DxDraw->DefaultTransform._22 = DxDraw->DeskScaleRate.y;
	}

	return TRUE;
}

void ResetDxDraw(DXDRAWSTRUCT *DxDraw)
{
	if ( DxDraw == NULL ) return;

	if ( DxDraw->HwndRenderTarget != NULL ){
		ResetDxDrawAtlas(DxDraw);
		for ( int i = 0 ; i < DXBMPCACHEITEMS ; i++ ){
			if ( DxDraw->bmpCache[i] != NULL ){
				DxDraw->bmpCache[i]->Release();
				DxDraw->bmpCache[i] = NULL;
			}
		}
		if ( DxDraw->BackBrush.brush != NULL ){
			DxDraw->BackBrush.brush->Release();
			DxDraw->BackBrush.brush = NULL;
		}
		if ( DxDraw->SubBrush.brush != NULL ){
			DxDraw->SubBrush.brush->Release();
			DxDraw->SubBrush.brush = NULL;
		}
		if ( DxDraw->TextBrush.brush != NULL ){
			DxDraw->TextBrush.brush->Release();
			DxDraw->TextBrush.brush = NULL;
		}
		DxDraw->HwndRenderTarget->Release();
		DxDraw->HwndRenderTarget = NULL;
		DxDraw->HwndRenderTargetCounter++;
	}
}

BOOL CloseDxDraw(DXDRAWSTRUCT **DxDrawPtr)
{
	DXDRAWSTRUCT *DxDraw;

	DxDraw = *DxDrawPtr;
	if ( DxDraw == NULL ) return FALSE;

	ResetDxDraw(DxDraw);
	if ( DxDraw->textFormatMain != NULL )	DxDraw->textFormatMain->Release();
	if ( DxDraw->textFormatSub != NULL )	DxDraw->textFormatSub->Release();
	DxDraw->dwFactory->Release();
	DxDraw->d2dFactory->Release();
	if ( DxDraw->WicFactory != NULL ) DxDraw->WicFactory->Release();

	delete DxDraw;
	*DxDrawPtr = NULL;
	return TRUE;
}

void InitRenderTarget(DXDRAWSTRUCT *DxDraw)
{
	// RenderTarget 生成
	GetClientRect(DxDraw->hWnd, &DxDraw->clientBox);
	DxDraw->clientArea.width  = DxDraw->clientBox.right;
	DxDraw->clientArea.height = DxDraw->clientBox.bottom;

	SetRotate(DxDraw, 0);
// BMP の最大サイズ(HwndRenderTarget->GetMaximumBitmapSize())を緩和
// D2D1::RenderTargetProperties();
// properties.type = D2D1_RENDER_TARGET_TYPE_SOFTWARE;
	DxDraw->d2dFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(RenderTargetProperties_OPTION),
			D2D1::HwndRenderTargetProperties(DxDraw->hWnd, DxDraw->clientArea),
			&DxDraw->HwndRenderTarget);

	// パラメータ設定
	TCHAR buf[0x400], param[0x100];
	const TCHAR *ptr, *pptr;
	int size;

	float Gamma, EnhancedContrast, ClearTypeLevel;
	DWRITE_RENDERING_MODE RenderingMode;

	IDWriteRenderingParams *OldRparams = NULL, *NewRparams = NULL;

	DxDraw->dwFactory->CreateRenderingParams(&OldRparams);
	Gamma            = OldRparams->GetGamma();
	EnhancedContrast = OldRparams->GetEnhancedContrast();
	ClearTypeLevel   = OldRparams->GetClearTypeLevel();
	RenderingMode    = OldRparams->GetRenderingMode();

	size = (wsprintf(buf, T("%d,%d,,%d,%d,"),
		static_cast<int>(Gamma * 100.0f),
		static_cast<int>(EnhancedContrast * 100.0f),
		/*grayscaleEnhancedContrast, */
		static_cast<int>(ClearTypeLevel * 100.0f),
		RenderingMode /*, gridFitMode*/ ) + 1 ) * sizeof(TCHAR);
	SetCustTable(T("_others"), T("dwenv_def"), buf, size);

	buf[0] = '\0';
	if ( NO_ERROR != GetCustTable(T("_others"), T("dwenv"), buf, sizeof(buf)) ){
		tstrcpy(buf,T(",,,,5"));
	}
	ptr = buf;
	for (;;){
		// Gamma
		if ( GetCommandParameter(&ptr, param, TSIZEOF(param)) == '\0') break;
		if ( Isdigit(param[0]) ){
			pptr = param;
			Gamma = static_cast<float>(GetNumber(&pptr)) / 100.0f;
		}
		NextParameter(&ptr);
		// EnhancedContrast
		if ( GetCommandParameter(&ptr, param, TSIZEOF(param)) == '\0') break;
		if ( Isdigit(param[0]) ){
			pptr = param;
			EnhancedContrast = static_cast<float>(GetNumber(&pptr)) / 100.0f;
		}
		NextParameter(&ptr);
		// grayscaleEnhancedContrast // IDWriteFactory2 , Win8.1
		if ( GetCommandParameter(&ptr, param, TSIZEOF(param)) == '\0') break;
		NextParameter(&ptr);
		// ClearTypeLevel
		if ( GetCommandParameter(&ptr, param, TSIZEOF(param)) == '\0') break;
		if ( Isdigit(param[0]) ){
			pptr = param;
			ClearTypeLevel = static_cast<float>(GetNumber(&pptr)) / 100.0f;
		}
		NextParameter(&ptr);
		// RenderingMode
		if ( GetCommandParameter(&ptr, param, TSIZEOF(param)) == '\0') break;
		if ( Isdigit(param[0]) ){
			pptr = param;
			RenderingMode = static_cast<DWRITE_RENDERING_MODE>(GetNumber(&pptr));
		}
		// gridFitMode // IDWriteFactory2 , Win8.1
		if ( GetCommandParameter(&ptr, param, TSIZEOF(param)) == '\0') break;
		// COLOR_FONT
		if ( GetCommandParameter(&ptr, param, TSIZEOF(param)) == '\0') break;
		if ( Isdigit(param[0]) ){
			if ( (param[0] == '1') &&
				 (((OSver.dwMajorVersion << 16) | OSver.dwMinorVersion) >= 0x60003) ){
				TextOption = D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT;
			}else{
				TextOption = D2D1_DRAW_TEXT_OPTIONS_NONE;
			}
		}
		break;
	}

	if ( SUCCEEDED(DxDraw->dwFactory->CreateCustomRenderingParams(
			Gamma, EnhancedContrast, ClearTypeLevel,
			OldRparams->GetPixelGeometry(), RenderingMode, &NewRparams)) ){
		DxDraw->HwndRenderTarget->SetTextRenderingParams(NewRparams);
		NewRparams->Release();
	}
	OldRparams->Release();

}

int BeginDrawDxDraw(DXDRAWSTRUCT *DxDraw, PAINTSTRUCT *ps)
{
	if ( DxDraw == NULL ) return DXSTART_GDI;
	if ( GetKeyState(VK_SCROLL) & 1 ){
		BeginPaint(DxDraw->hWnd, ps);
		return DXSTART_GDI;
	}

	if ( DxDraw->HwndRenderTarget == NULL ){
		InitRenderTarget(DxDraw);
	}

	if ( DxDraw->TextBrush.brush == NULL ){ // TextBrush が 初期化されていない時用
		// 今のところ、アクティブ時に１回使用している
		DxDraw->HwndRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D3DCOLORfromCOLORREF(DxDraw->TextBrush.color), 1.0f),
				&DxDraw->TextBrush.brush);
	}

	if ( DxDraw->HwndRenderTarget->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED ){
		ValidateRect(DxDraw->hWnd, NULL);
		return DXSTART_NODRAW; // 描画の必要なし
	}

	DxDraw->HwndRenderTarget->BeginDraw();
	DxDraw->HwndRenderTarget->SetTransform(DxDraw->DefaultTransform);

#if DWMGLASS
	DxDraw->HwndRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
#else
	DxDraw->HwndRenderTarget->Clear(D2D1::ColorF(D3DCOLORfromCOLORREF(DxDraw->BackBrush.color)));
#endif

	ps->hdc = static_cast<HDC>(DXMODEVALUE_DX);
	ps->fErase = FALSE;
	ps->rcPaint = DxDraw->clientBox;
	return DXSTART_DX;
}

BOOL EndDrawDxDraw(DXDRAWSTRUCT *DxDraw)
{
	if ( DxDraw->HwndRenderTarget != NULL ){
		if ( DxDraw->HwndRenderTarget->EndDraw() == D2DERR_RECREATE_TARGET ){
			ResetDxDraw(DxDraw);
			Sleep(50);
			InvalidateRect(DxDraw->hWnd, NULL, TRUE);
			return TRUE;
		}
	}
	ValidateRect(DxDraw->hWnd, NULL);
	return TRUE;
}

void SetRotate(DXDRAWSTRUCT *DxDraw, int rel_angle)
{
	float height;
	D2D1_POINT_2F centerPoint;

	if ( DxDraw == NULL ) return;

	DxDraw->RotateAngle += rel_angle;
	while ( DxDraw->RotateAngle < 0 ) DxDraw->RotateAngle += 360;
	while ( DxDraw->RotateAngle >= 360 ) DxDraw->RotateAngle -= 360;

	height = static_cast<float>(DxDraw->clientArea.height) / 2.0f;
	centerPoint = D2D1::Point2F(0.0f, height / ((DxDraw->DeskScale.y / DEFAULT_DPI - 1.0f) / 2.0f + 1.0f));

	if ( DxDraw->RotateAngle == 0 ){ // DeskScaleRate
		DxDraw->DefaultTransform = D2D1::Matrix3x2F::Identity();
		DxDraw->DefaultTransform._11 = DxDraw->DeskScaleRate.x;
		DxDraw->DefaultTransform._22 = DxDraw->DeskScaleRate.y;

		// 上下反転
		DxDraw->BMPTransform = D2D1::Matrix3x2F::Scale(
				DxDraw->DeskScaleRate.x, -DxDraw->DeskScaleRate.y, centerPoint);
	}else{ // DotScaleRate
		float RotateAngle = static_cast<float>(DxDraw->RotateAngle);
		float width = static_cast<float>(DxDraw->clientArea.width) / 2.0f;

		DD2D1MakeRotateMatrix(RotateAngle,
				D2D1::Point2F(width, height), &DxDraw->DefaultTransform);
		DD2D1MakeRotateMatrix(-RotateAngle,
				D2D1::Point2F(width, height), &DxDraw->HitPointTransform);
		DxDraw->BMPTransform.SetProduct(
				D2D1::Matrix3x2F::Scale(
						DxDraw->DeskScaleRate.x, -DxDraw->DeskScaleRate.y,
						centerPoint),
				static_cast<const D2D1::Matrix3x2F &>(DxDraw->DefaultTransform));
	}
}

void DxTransformPoint(DXDRAWSTRUCT *DxDraw, LPARAM *lParam)
{
	if ( DxDraw == NULL ) return;
	if ( DxDraw->RotateAngle == 0 ) return;

	D2D1_POINT_2F pos = DxDraw->HitPointTransform.TransformPoint(D2D1::Point2F(
			static_cast<float>(static_cast<short>(LOWORD(*lParam))),
			static_cast<float>(static_cast<short>(HIWORD(*lParam)))));
	*lParam = static_cast<WORD>(pos.x) + (static_cast<WORD>(pos.y) << 16);
}

HRESULT InitMainFont(DXDRAWSTRUCT *DxDraw)
{
	HRESULT result;

	if ( DxDraw->textFormatMain != NULL ){
		DxDraw->textFormatMain->Release();
		DxDraw->textFormatMain = NULL;
	}
	// CreateFontFromLOGFONT / CreateFontFaceFromHdc

	result = DxDraw->dwFactory->CreateTextFormat(DxDraw->fontName,
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			DxDraw->fontH * DxDraw->DotScaleRate,
			L"",
			&DxDraw->textFormatMain);
	if ( SUCCEEDED(result) ){
		DxDraw->textFormatMain->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		DxDraw->textFormat = DxDraw->textFormatMain;
	}else{
		DxDraw->textFormatMain = NULL;
	}
	return result;
}

HRESULT InitSubFont(DXDRAWSTRUCT *DxDraw, int type)
{
	HRESULT result;

	if ( DxDraw->textFormatSub != NULL ){
		DxDraw->textFormatSub->Release();
		DxDraw->textFormatSub = NULL;
	}

	result = DxDraw->dwFactory->CreateTextFormat(DxDraw->fontNameSub,
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			DxDraw->fontH * DxDraw->DotScaleRate,
			(type == 2) ? L"en-us" : L"",
			&DxDraw->textFormatSub);
	if ( SUCCEEDED(result) ){
		DxDraw->textFormatSub->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	}else{
		DxDraw->textFormatSub = NULL;
	}
	return result;
}

void DxSelectFont(DXDRAWSTRUCT *DxDraw, int type)
{
	if ( type == 0 ){
		if ( DxDraw->textFormatMain == NULL ){ // 初期化していない時用
			if ( FAILED(InitMainFont(DxDraw)) ) return;
		}
		DxDraw->textFormat = DxDraw->textFormatMain;
	}else{
		if ( DxDraw->textFormatSub == NULL ){ // 初期化していない時用
			if ( FAILED(InitSubFont(DxDraw, type)) ) return;
		}
		DxDraw->textFormat = DxDraw->textFormatSub;
	}
}

DWORD SetFontDxDraw(DXDRAWSTRUCT *DxDraw, HFONT hFont, int type)
{
	LOGFONT lfont;
	#ifndef UNICODE
		char fontnameA[LF_FACESIZE];
	#endif

	if ( DxDraw == NULL ) return 0;

	HDC hDC;
	HGDIOBJ hOldGDI;

	hDC = GetDC(DxDraw->hWnd);
	hOldGDI = SelectObject(hDC, hFont);

	if ( type == 0 ){
		DxDraw->DotScaleRate = DEFAULT_DPI / static_cast<float>(PPxCommonExtCommand(K_GETDISPDPI, (WPARAM)DxDraw->hWnd));

		if ( DxDraw->textFormatMain != NULL ){
			DxDraw->textFormatMain->Release();
			DxDraw->textFormatMain = NULL;
			DxDraw->textFormat = NULL;
		}
		GetObject(hFont, sizeof(LOGFONT), static_cast<LPVOID>(&lfont));
		DxDraw->fontH = static_cast<float>( (lfont.lfHeight >= 0) ?
				 lfont.lfHeight : -lfont.lfHeight) / DxDraw->DotScaleRate;
		strcpyToW(DxDraw->fontName, lfont.lfFaceName, LF_FACESIZE);
		SelectObject(hDC, hOldGDI);
		ReleaseDC(DxDraw->hWnd, hDC);
	}else{
		if ( DxDraw->textFormatSub != NULL ){
			if ( DxDraw->textFormat == DxDraw->textFormatSub ){
				DxDraw->textFormat = NULL;
			}
			DxDraw->textFormatSub->Release();
			DxDraw->textFormatSub = NULL;
		}

		#ifdef UNICODE
			GetTextFace(hDC, LF_FACESIZE, DxDraw->fontNameSub);
		#else
			GetTextFace(hDC, LF_FACESIZE, fontnameA);
			AnsiToUnicode(fontnameA, DxDraw->fontNameSub, LF_FACESIZE);
		#endif
		SelectObject(hDC, hOldGDI);
		ReleaseDC(DxDraw->hWnd, hDC);
		return 1;
	}

	if ( DxDraw->HwndRenderTarget == NULL ){
		InitRenderTarget(DxDraw);
	}

	if ( DxDraw->textFormat == NULL ){ // 初期化していない時用
		if ( FAILED(InitMainFont(DxDraw)) ) return 0;
	}

	WCHAR chr[11] = L"0123456789";
	DWRITE_TEXT_METRICS strMet;
	IDWriteTextLayout *dwLayout;

	if ( SUCCEEDED(DxDraw->dwFactory->CreateGdiCompatibleTextLayout(chr, TSIZEOFW(chr) - 1, DxDraw->textFormat, 32000, 32000, 1.0f, NULL, TRUE, &dwLayout)) ){
		dwLayout->GetMetrics(&strMet);
		dwLayout->Release();

		DxDraw->fontW = (strMet.widthIncludingTrailingWhitespace) / static_cast<float>(TSIZEOFW(chr) - 1);
	}else{
		DxDraw->fontW = 8; // dummy
	}

	return static_cast<int>(DxDraw->fontW);
}

void USEFASTCALL DxMoveToEx(DXDRAWSTRUCT *DxDraw, HDC hDC, int x, int y)
{
	IfGDImode(hDC){
		MoveToEx(hDC, x, y, NULL);
		return;
	}
	DxDraw->lastpos.x = static_cast<float>(x);
	DxDraw->lastpos.y = static_cast<float>(y);
}

void USEFASTCALL DxGetCurrentPositionEx(DXDRAWSTRUCT *DxDraw, HDC hDC, POINT *lp)
{
	IfGDImode(hDC){
		GetCurrentPositionEx(hDC, lp);
		return;
	}
	lp->x = static_cast<LONG>(DxDraw->lastpos.x);
	lp->y = static_cast<LONG>(DxDraw->lastpos.y);
}

int DxSetBkMode(DXDRAWSTRUCT *DxDraw, HDC hDC, int mode)
{
	IfGDImode(hDC) return SetBkMode(hDC, mode);

	int oldmode = DxDraw->BkMode;
	DxDraw->BkMode = mode;
	return mode;
}

COLORREF DxSetBkColor(DXDRAWSTRUCT *DxDraw, HDC hDC, COLORREF color)
{
	IfGDImode(hDC) return SetBkColor(hDC, color);

	COLORREF oldcolor = DxDraw->BackTextColor;
	DxDraw->BackTextColor = color;
	return oldcolor;
}

COLORREF SetDxBrush(DXDRAWSTRUCT *DxDraw, DXBRUSHSTRUCT *brush, COLORREF color)
{
	COLORREF oldcolor = brush->color;

	if ( (color == oldcolor) && (brush->brush != NULL) ) return oldcolor;

	brush->color = color;
	if ( brush->brush != NULL ){
		brush->brush->Release();
		brush->brush = NULL;
	}
	DxDraw->HwndRenderTarget->CreateSolidColorBrush(
			D2D1::ColorF(D3DCOLORfromCOLORREF(color),
			static_cast<float>(color >> 24) / 255.0f),
			&brush->brush);
	return oldcolor;
}

COLORREF DxSetTextColor(DXDRAWSTRUCT *DxDraw, HDC hDC, COLORREF color)
{
	IfGDImode(hDC) return SetTextColor(hDC, color);
	return SetDxBrush(DxDraw, &DxDraw->TextBrush, color | 0xff000000);
}

ID2D1SolidColorBrush *GetDxBrush(DXDRAWSTRUCT *DxDraw, COLORREF color)
{
	if ( color == DxDraw->BackBrush.color ){
		SetDxBrush(DxDraw, &DxDraw->BackBrush, color);
		return DxDraw->BackBrush.brush;
	}else if ( color == DxDraw->TextBrush.color ){
		SetDxBrush(DxDraw, &DxDraw->TextBrush, color);
		return DxDraw->TextBrush.brush;
	}else{
		SetDxBrush(DxDraw, &DxDraw->SubBrush, color);
		return DxDraw->SubBrush.brush;
	}
}

void DxDrawBack(DXDRAWSTRUCT *DxDraw, HDC hDC, const RECT *box, COLORREF color)
{
	IfGDImode(hDC) return;

	D2D1_RECT_F d2box = {
			static_cast<float>(box->left),
			static_cast<float>(box->top),
			static_cast<float>(box->right),
			static_cast<float>(box->bottom) };
	DxDraw->HwndRenderTarget->FillRectangle(&d2box, GetDxBrush(DxDraw, color));
}

void DxDrawFrameRect(DXDRAWSTRUCT *DxDraw, HDC hDC, const RECT *box, COLORREF color)
{
	IfGDImode(hDC) return;

	D2D1_RECT_F d2box = {
			static_cast<float>(box->left),
			static_cast<float>(box->top),
			static_cast<float>(box->right),
			static_cast<float>(box->bottom) };
	DxDraw->HwndRenderTarget->DrawRectangle(&d2box, GetDxBrush(DxDraw, color | 0xff000000));
}

BOOL ChangeSizeDxDraw(DXDRAWSTRUCT *DxDraw, COLORREF backcolor)
{
	if ( DxDraw == NULL ) return FALSE;

	// RenderTarget 生成
	GetClientRect(DxDraw->hWnd, &DxDraw->clientBox);
	if ( (DxDraw->clientArea.width  != DxDraw->clientBox.right) ||
		 (DxDraw->clientArea.height != DxDraw->clientBox.bottom) ){
		DxDraw->clientArea.width  = DxDraw->clientBox.right;
		DxDraw->clientArea.height = DxDraw->clientBox.bottom;
		if ( DxDraw->HwndRenderTarget != NULL ){
			DxDraw->HwndRenderTarget->Resize(DxDraw->clientArea);
		}

		SetRotate(DxDraw, 0);
	}
	if ( DxDraw->BackBrush.color != backcolor ){
		DxDraw->BackColor = backcolor;
		DxDraw->BackBrush.color = backcolor | 0xff000000;
		if ( DxDraw->BackBrush.brush != NULL ){
			DxDraw->BackBrush.brush->Release();
			DxDraw->BackBrush.brush = NULL;
		}
	}
	return TRUE;
}

void DxGetTextExtentExPoint(DXDRAWSTRUCT *DxDraw, HDC hDC, const TCHAR *str, int len, int maxwidth, int *fitlem, int *alptbl, SIZE *blksize)
{
	IDWriteTextLayout *dwLayout;

	IfGDImode(hDC){
		GetTextExtentExPoint(hDC, str, len, maxwidth, fitlem, alptbl, blksize);
		return;
	}

	if ( DxDraw == NULL ){
		*fitlem = 0;
		return;
	}

	if ( DxDraw->textFormat == NULL ){ // 初期化していない時用
		if ( FAILED(InitMainFont(DxDraw)) ) return;
	}

	#ifdef UNICODE
		#define DRAWLEN len
		#define DRAWSTR str
	#else
		WCHAR DRAWSTR[0x200];
		int DRAWLEN = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str, len, DRAWSTR, TSIZEOFW(DRAWSTR));
	#endif

	for (;;){
		if ( SUCCEEDED(DxDraw->dwFactory->CreateGdiCompatibleTextLayout(DRAWSTR, DRAWLEN, DxDraw->textFormat, 32000, 32000, 1.0f, NULL, FALSE, &dwLayout)) ){
			#undef DRAWLEN
			#undef DRAWSTR

			DWRITE_TEXT_METRICS strMet;

			dwLayout->GetMetrics(&strMet);
			dwLayout->Release();
			blksize->cx = static_cast<int>(strMet.widthIncludingTrailingWhitespace);
			if ( blksize->cx < maxwidth ) break;
			if ( len == 0 ) break;
			len--;
		} else {
			len = 0;
			break;
		}
	}
	*fitlem = len;
}

void DxDrawTextBack(DXDRAWSTRUCT *DxDraw, DWRITE_TEXT_METRICS &strMet)
{
	D2D1_RECT_F d2box = {
			DxDraw->lastpos.x,
			DxDraw->lastpos.y,
			DxDraw->lastpos.x + strMet.widthIncludingTrailingWhitespace,
			DxDraw->lastpos.y + strMet.height };
	DxDraw->HwndRenderTarget->FillRectangle(&d2box, GetDxBrush(DxDraw, DxDraw->BackTextColor | 0xff000000));
}

void DxTextOutRel(DXDRAWSTRUCT *DxDraw, HDC hDC, const TCHAR *str, int len)
{
	IDWriteTextLayout *dwLayout;

	IfGDImode(hDC){
		TextOut(hDC, 0, 0, str, len);
		return;
	}
	if ( DxDraw == NULL ) return;

	if ( DxDraw->textFormat == NULL ){ // 初期化していない時用
		if ( FAILED(InitMainFont(DxDraw)) ) return;
	}

	#ifdef UNICODE
		#define DRAWLEN len
		#define DRAWSTR str
	#else
		WCHAR DRAWSTR[0x200];
		int DRAWLEN = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str, len, DRAWSTR, TSIZEOFW(DRAWSTR) );
	#endif

	if ( FAILED(DxDraw->dwFactory->CreateGdiCompatibleTextLayout(DRAWSTR, DRAWLEN, DxDraw->textFormat, 32000, 32000, 1.0f, NULL, FALSE, &dwLayout)) ){
		return;
	}

	DWRITE_TEXT_METRICS strMet;
	dwLayout->GetMetrics(&strMet);

	if ( DxDraw->BackTextColor != DxDraw->BackColor ){
		DxDrawTextBack(DxDraw, strMet);
	}
	DxDraw->HwndRenderTarget->DrawTextLayout(DxDraw->lastpos, dwLayout, DxDraw->TextBrush.brush, TextOption);
	dwLayout->Release();

	DxDraw->lastpos.x += strMet.widthIncludingTrailingWhitespace;
	#undef DRAWLEN
	#undef DRAWSTR
}

#ifndef UNICODE
void DxTextOutRelW(DXDRAWSTRUCT *DxDraw, HDC hDC, const WCHAR *str, int len)
{
	IDWriteTextLayout *dwLayout;

	IfGDImode(hDC){
		TextOutW(hDC, 0, 0, str, len);
		return;
	}
	if ( DxDraw == NULL ) return;

	#define DRAWLEN len
	#define DRAWSTR str

	DxDraw->dwFactory->CreateTextLayout(DRAWSTR, DRAWLEN, DxDraw->textFormat, 32000, 32000, &dwLayout);

	DWRITE_TEXT_METRICS strMet;
	dwLayout->GetMetrics(&strMet);

	if ( DxDraw->BackTextColor != DxDraw->BackColor ){
		DxDrawTextBack(DxDraw, strMet);
	}

	DxDraw->HwndRenderTarget->DrawTextLayout(DxDraw->lastpos, dwLayout, DxDraw->TextBrush.brush, TextOption);
	dwLayout->Release();
	DxDraw->lastpos.x += strMet.widthIncludingTrailingWhitespace;

	#undef DRAWLEN
	#undef DRAWSTR
}
#endif

void DxDrawText(DXDRAWSTRUCT *DxDraw, HDC hDC, const TCHAR *str, int len, RECT *box, DWORD flags)
{
	IDWriteTextLayout *dwLayout;
	float drawWidth;

	IfGDImode(hDC){
		if ( flags & DT_RIGHT ){
			SIZE fsize;
			int slen = len;
			const TCHAR *strp;

			strp = str;
			while ( *strp == ' ' ){
				strp++;
				slen--;
			}
			GetTextExtentPoint32(hDC, strp, slen, &fsize);
			if ( fsize.cx < (box->right - box->left) ){
				RECT tmpbox;
				int drawX = box->right - fsize.cx;

				if ( GetBkMode(hDC) == OPAQUE ){
					HBRUSH hBackBrush;

					tmpbox = *box;
					tmpbox.right = drawX;
					hBackBrush = CreateSolidBrush(GetBkColor(hDC));
					FillRect(hDC, &tmpbox, hBackBrush);
					DeleteObject(hBackBrush);
				}
				MoveToEx(hDC, drawX, box->top, NULL);
				TextOut(hDC, drawX, box->top, strp, len);
				return;
			}
		}
		DrawText(hDC, str, len, box, flags);
		return;
	}
	if ( DxDraw == NULL ) return;

	if ( DxDraw->textFormat == NULL ){ // 初期化していない時用
		if ( FAILED(InitMainFont(DxDraw)) ) return;
	}

	#ifdef UNICODE
		#define DRAWLEN len
		#define DRAWSTR str

		if ( len == -1 ) len = wcslen32(str);
	#else
		WCHAR DRAWSTR[0x200];
		int DRAWLEN = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str, len, DRAWSTR, TSIZEOFW(DRAWSTR) );
	#endif

	if ( flags & DT_WORDBREAK ){
		DxDraw->textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
	}

	drawWidth = static_cast<float>(box->right - box->left);
	if ( FAILED(DxDraw->dwFactory->CreateGdiCompatibleTextLayout(DRAWSTR,
			DRAWLEN, DxDraw->textFormat,
			drawWidth, static_cast<float>(box->bottom - box->top),
			1.0f, NULL, FALSE, &dwLayout)) ){
		return;
	}

	DWRITE_TEXT_METRICS strMet;
	dwLayout->GetMetrics(&strMet);

	// １行の時は、クリッピングして末尾「…」にする
	if ( (flags & DT_SINGLELINE) &&
		 (drawWidth < strMet.widthIncludingTrailingWhitespace) ){
		IDWriteTextLayout *dwTailLayout;
		DWRITE_TEXT_METRICS strMetTail;

		strMet.widthIncludingTrailingWhitespace = drawWidth;

		DxDraw->dwFactory->CreateTextLayout(strtail, 1, DxDraw->textFormat, 32000, 32000, &dwTailLayout);
		dwTailLayout->GetMetrics(&strMetTail);

		if ( DxDraw->BackTextColor != DxDraw->BackColor ){ // 背景描画
			DxDrawTextBack(DxDraw, strMet);
		}

		D2D1_POINT_2F pos = {
			DxDraw->lastpos.x + drawWidth - strMetTail.widthIncludingTrailingWhitespace,
			DxDraw->lastpos.y };

		DxDraw->HwndRenderTarget->DrawTextLayout(pos, dwTailLayout, DxDraw->TextBrush.brush, TextOption);
		dwTailLayout->Release();

		dwLayout->SetMaxWidth(drawWidth - strMetTail.widthIncludingTrailingWhitespace);
	}else if ( DxDraw->BackTextColor != DxDraw->BackColor ){ // 背景描画
		DxDrawTextBack(DxDraw, strMet);
	}
	if ( flags & DT_RIGHT ){
		DxDraw->lastpos.x += drawWidth - strMet.widthIncludingTrailingWhitespace;
	}

	DxDraw->HwndRenderTarget->DrawTextLayout(DxDraw->lastpos, dwLayout,
			DxDraw->TextBrush.brush, TextOption | D2D1_DRAW_TEXT_OPTIONS_CLIP);
	dwLayout->Release();

	if ( flags & DT_WORDBREAK ){
		DxDraw->textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	}

	DxDraw->lastpos.x += strMet.widthIncludingTrailingWhitespace;
	#undef DRAWLEN
	#undef DRAWSTR
}

extern "C" typedef struct tagDXBMPCACHE {
	DXDRAWSTRUCT *DxDraw;
	D2D1_SIZE_U bmpsize;
	int cacheIndex, HwndRenderTargetCounter;
} DXBMPCACHE;

HBITMAP BmpColorConvert(BITMAPINFOHEADER *bmiHeader, LPVOID *lpBits)
{
	LPVOID TemplpBits;
	BITMAPINFO bmi;
	HGDIOBJ hOldBmp;
	HDC hDC;
	HBITMAP hConvertBmp;

	memset(&bmi.bmiHeader, 0, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth    = bmiHeader->biWidth;
	bmi.bmiHeader.biHeight   = bmiHeader->biHeight;
	bmi.bmiHeader.biPlanes   = 1;
	bmi.bmiHeader.biBitCount = 32;

	hConvertBmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &TemplpBits, NULL, 0);
	hDC = CreateCompatibleDC(NULL);
	hOldBmp = SelectObject(hDC, hConvertBmp);

	SetDIBitsToDevice(hDC,
			0, 0, bmi.bmiHeader.biWidth, bmi.bmiHeader.biHeight,
			0, 0,  0, bmi.bmiHeader.biHeight,
			*lpBits,
			(BITMAPINFO *)bmiHeader, DIB_RGB_COLORS);

	*lpBits = TemplpBits;
	SelectObject(hDC, hOldBmp);
	DeleteDC(hDC);
	return hConvertBmp;
}

BOOL DxDrawDIB(DXDRAWSTRUCT *DxDraw, BITMAPINFOHEADER *bmiHeader, LPVOID lpBits, const RECT *box, const RECT *clipArea, DXBMPCACHE **cache)
{
	ID2D1Bitmap *d2bmp;
	D2D1_SIZE_U bmpsize;

	if ( (cache == NULL) || (*cache == NULL) ){ // キャッシュ無し
		D2D1_BITMAP_PROPERTIES bmpprop;
		HBITMAP hConvertBmp;
		BOOL convert = FALSE;

		bmpsize.width = bmiHeader->biWidth;
		bmpsize.height = bmiHeader->biHeight;
		if ( bmpsize.height < 0 ) bmpsize.height = -bmiHeader->biHeight;

		if ( bmiHeader->biBitCount != 32 ){
			convert = TRUE;
			hConvertBmp = BmpColorConvert(bmiHeader, &lpBits);
		}

		bmpprop.dpiX = DxDraw->DeskScale.x;
		bmpprop.dpiY = DxDraw->DeskScale.y;
		bmpprop.pixelFormat = D2D1::PixelFormat(
				DXGI_FORMAT_B8G8R8A8_UNORM,
//				D2D1_ALPHA_MODE_STRAIGHT);
				D2D1_ALPHA_MODE_IGNORE);

		HRESULT hr = DxDraw->HwndRenderTarget->CreateBitmap(bmpsize, lpBits, bmpsize.width * 4/* Alignment済み*/ , bmpprop, &d2bmp);
		if ( convert ) DeleteObject(hConvertBmp); // ビットマップのメモリも削除

		if ( SUCCEEDED(hr) ){
			if ( cache != NULL ){
				for ( int i = 0 ; i < DXBMPCACHEITEMS ; i++ ){
					if ( DxDraw->bmpCache[i] == NULL ){
						*cache = new DXBMPCACHE;
						DxDraw->bmpCache[i] = d2bmp;
						(*cache)->DxDraw = DxDraw;
						(*cache)->cacheIndex = i;
						(*cache)->bmpsize = bmpsize;
						(*cache)->HwndRenderTargetCounter = DxDraw->HwndRenderTargetCounter;
						break;
					}
				}
			}
		}else{
			return FALSE;
		}
	}else{
		d2bmp = DxDraw->bmpCache[(*cache)->cacheIndex];
		// キャッシュ廃棄？
		if ( ((*cache)->HwndRenderTargetCounter != DxDraw->HwndRenderTargetCounter) || (d2bmp == NULL) ){
			DxDrawFreeBMPCache(cache);
			return DxDrawDIB(DxDraw, bmiHeader, lpBits, box, clipArea, cache);
		}
		// キャッシュ使用
		bmpsize = (*cache)->bmpsize;
	}

	if ( clipArea == NULL ) clipArea = &DxDraw->clientBox;

	D2D1_RECT_F d2bmpbox = {
		0.0f, 0.0f,
		static_cast<float>(bmpsize.width), static_cast<float>(bmpsize.height) };
	D2D1_RECT_F d2box = {
		static_cast<float>(box->left), 0.0f,
		static_cast<float>(box->left + box->right), 0.0f};

	if ( bmiHeader->biHeight > 0 ){ // ボトムアップ----------------------------
		d2box.top = static_cast<float>(static_cast<int>(DxDraw->clientArea.height) - (box->top + box->bottom));
		d2box.bottom = static_cast<float>(static_cast<int>(DxDraw->clientArea.height) - box->top);

		if ( box->top < clipArea->top ){ // 上(描画上は下)クリッピング
			d2bmpbox.bottom -= static_cast<float>(clipArea->top - box->top) * static_cast<float>(bmpsize.height) / static_cast<float>(box->bottom);
			d2box.bottom = static_cast<float>(static_cast<int>(DxDraw->clientArea.height) - clipArea->top);
		}
		if ( (box->top + box->bottom) > clipArea->bottom ){ // 下(描画上は上)クリッピング
			// d2bmpbox.top は 0.0f 固定なので演算省略
			d2bmpbox.top = static_cast<float>((box->top + box->bottom) - clipArea->bottom) * static_cast<float>(bmpsize.height) / static_cast<float>(box->bottom) * DxDraw->DeskScaleRate.y;
			d2box.top = static_cast<float>(static_cast<int>(DxDraw->clientArea.height) - clipArea->bottom);
		}
		DxDraw->HwndRenderTarget->SetTransform(DxDraw->BMPTransform);
	}else{ // トップダウン ----------------------------------------------------
		d2box.top = static_cast<float>(box->top);
		d2box.bottom = static_cast<float>(box->top + box->bottom);

		if ( box->top < clipArea->top ){ // 上クリッピング
			// d2bmpbox.top は 0.0f 固定なので演算省略
			d2bmpbox.top = (static_cast<float>(clipArea->top - box->top) *
				static_cast<float>(bmpsize.height)) / static_cast<float>(box->bottom) * DxDraw->DeskScaleRate.y;
			d2box.top = static_cast<float>(clipArea->top);
		}
		if ( (box->top + box->bottom) > clipArea->bottom ){ // 下クリッピング
			d2bmpbox.bottom -= (static_cast<float>((box->top + box->bottom) - clipArea->bottom) *
				static_cast<float>(bmpsize.height)) / static_cast<float>(box->bottom);
			d2box.bottom = static_cast<float>(clipArea->bottom);
		}
	}

	if ( box->left < clipArea->left ){ // 左クリッピング
		// d2bmpbox.left は 0.0f 固定なので演算省略
		d2bmpbox.left = static_cast<float>(clipArea->left - box->left) * static_cast<float>(bmpsize.width) / static_cast<float>(box->right) * DxDraw->DeskScaleRate.x;
		d2box.left = static_cast<float>(clipArea->left);
	}
	if ( (box->left + box->right) > clipArea->right ){ // 右クリッピング
		d2bmpbox.right -= static_cast<float>((box->left + box->right) - clipArea->right) * static_cast<float>(bmpsize.width) / static_cast<float>(box->right);
		d2box.right = static_cast<float>(clipArea->right);
	}

	d2bmpbox.bottom *= DxDraw->DeskScaleRate.y;
	d2bmpbox.right *= DxDraw->DeskScaleRate.x;
	DxDraw->HwndRenderTarget->DrawBitmap(d2bmp, d2box , 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, d2bmpbox );
	DxDraw->HwndRenderTarget->SetTransform(DxDraw->DefaultTransform);
	if ( (cache == NULL) || (*cache == NULL) ) d2bmp->Release();
	return TRUE;
}

void DxGDIDrawIcon(DXDRAWSTRUCT *DxDraw, HICON icon, const RECT *box, DXBMPCACHE **cache)
{
	BITMAPINFO bmi;
	LPVOID lpBits;
	HBITMAP hbmp;
	HGDIOBJ hOldBmp;
	HDC hDC;
	HBRUSH hBackBrush;

	memset(&bmi.bmiHeader, 0, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth    = 32;
	bmi.bmiHeader.biHeight   = 32;
	bmi.bmiHeader.biPlanes   = 1;
	bmi.bmiHeader.biBitCount = 32;
	hbmp = CreateDIBSection(NULL, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, &lpBits, NULL, 0);
	hDC = CreateCompatibleDC(NULL);
	hOldBmp = SelectObject(hDC, hbmp);
	hBackBrush = CreateSolidBrush(DxDraw->BackColor);
	DrawIconEx(hDC, 0, 0, icon, bmi.bmiHeader.biWidth, bmi.bmiHeader.biHeight, 0, hBackBrush, DI_NORMAL);
	DeleteObject(hBackBrush);
	SelectObject(hDC, hOldBmp);
	DeleteDC(hDC);

	DxDrawDIB(DxDraw, &bmi.bmiHeader, lpBits, box, NULL, cache);
	DeleteObject(hbmp); // ビットマップのメモリも削除
}

const IID IID_IWICImagingFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};

void DxDrawIcon(DXDRAWSTRUCT *DxDraw, HICON icon, const RECT *box, DXBMPCACHE **cache)
{
	if ( DxDraw == NULL ) return;

	if ( (cache == NULL) || (*cache == NULL) ||
	   ((*cache)->HwndRenderTargetCounter != DxDraw->HwndRenderTargetCounter) ||
	   (DxDraw->bmpCache[(*cache)->cacheIndex] == NULL) ){
		if ( (DxDraw->WicFactory != NULL) ||
			SUCCEEDED(::CoCreateInstance(CLSID_WICImagingFactory, NULL,
				CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
				reinterpret_cast<void**>(&DxDraw->WicFactory))) ){
			IWICBitmap *wicbmp;
			ID2D1Bitmap *d2bmp;
			D2D1_SIZE_U bmpsize;
			IWICFormatConverter *wicConverter;

			if ( SUCCEEDED(DxDraw->WicFactory->CreateBitmapFromHICON(
					icon, &wicbmp)) ){
				wicbmp->GetSize(&bmpsize.width, &bmpsize.height);

				DxDraw->WicFactory->CreateFormatConverter(&wicConverter);

				wicConverter->Initialize(wicbmp, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeMedianCut);

				if ( SUCCEEDED(DxDraw->HwndRenderTarget->
						CreateBitmapFromWicBitmap(wicConverter, &d2bmp)) ){
					BOOL destroy = TRUE;

					if ( cache != NULL ){
						for ( int i = 0 ; i < DXBMPCACHEITEMS ; i++ ){
							if ( DxDraw->bmpCache[i] == NULL ){
								*cache = new DXBMPCACHE;
								DxDraw->bmpCache[i] = d2bmp;
								(*cache)->DxDraw = DxDraw;
								(*cache)->cacheIndex = i;
								(*cache)->bmpsize = bmpsize;
								(*cache)->HwndRenderTargetCounter = DxDraw->HwndRenderTargetCounter;
								destroy = FALSE;
								break;
							}
						}
					}

					D2D1_RECT_F d2box = {
						static_cast<float>(box->left),
						static_cast<float>(box->top),
						static_cast<float>(box->left + box->right),
						static_cast<float>(box->top + box->bottom) };
					D2D1_RECT_F d2bmpbox = {
						0.0f, 0.0f,
						static_cast<float>(bmpsize.width),
						static_cast<float>(bmpsize.height) };

					DxDraw->HwndRenderTarget->DrawBitmap(d2bmp, d2box , 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, d2bmpbox );
					if ( IsTrue(destroy) ) d2bmp->Release();
				}
				wicConverter->Release();
				wicbmp->Release();
			}
		}else{
			DxGDIDrawIcon(DxDraw, icon, box, cache);
		}
	// キャッシュを使用しようと試みる
	}else if ( DxDraw->WicFactory != NULL ){
		ID2D1Bitmap *d2bmp;

		d2bmp = DxDraw->bmpCache[(*cache)->cacheIndex];
		// キャッシュ廃棄？
		if ( ((*cache)->HwndRenderTargetCounter != DxDraw->HwndRenderTargetCounter) || (d2bmp == NULL) ){
			DxDrawFreeBMPCache(cache);
			DxDrawIcon(DxDraw, icon, box, cache);
			return;
		}
		// キャッシュ使用
		D2D1_RECT_F d2box = {
			static_cast<float>(box->left),
			static_cast<float>(box->top),
			static_cast<float>(box->left + box->right),
			static_cast<float>(box->top + box->bottom) };
		D2D1_RECT_F d2bmpbox = {
			0.0f, 0.0f,
			static_cast<float>((*cache)->bmpsize.width),
			static_cast<float>((*cache)->bmpsize.height) };

		DxDraw->HwndRenderTarget->DrawBitmap(d2bmp, d2box , 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, d2bmpbox );
	}else{ // GDI取得(上下反転)
		DxDrawDIB(DxDraw, NULL, NULL, box, NULL, cache);
	}
}

void DxDrawFreeBMPCache(DXBMPCACHE **cache)
{
	if ( (cache == NULL) || (*cache == NULL) ) return;

	DXDRAWSTRUCT *DxDraw = (*cache)->DxDraw;
	ID2D1Bitmap **d2bmp = &DxDraw->bmpCache[(*cache)->cacheIndex];

	if ( ((*cache)->HwndRenderTargetCounter == DxDraw->HwndRenderTargetCounter) && (*d2bmp != NULL) ){
		(*d2bmp)->Release();
		(*d2bmp) = NULL;
	}
	delete *cache;
	*cache = NULL;
}

void ResetDxDrawAtlas(DXDRAWSTRUCT *DxDraw)
{
	if ( DxDraw->BmpAtlas.bmp == NULL ) return;

	DxDraw->BmpAtlas.bmp->Release();
	DxDraw->BmpAtlas.bmp = NULL;
}

BOOL DxDrawAtlas_Check(DXDRAWSTRUCT *DxDraw, DWORD *CacheID)
{
	DWORD id;

	if ( DxDraw->BmpAtlas.bmp == NULL ){
		*CacheID = DxDraw->BmpAtlas.IndexNow++;
		DxDraw->BmpAtlas.useIndex = *CacheID; // 仮の値
		return FALSE;
	}

	if ( *CacheID != 0 ){ // CacheID が有効化を確認
		id = *CacheID - DxDraw->BmpAtlas.IndexMin;
		if ( id < DxDraw->BmpAtlas.AtlasItems ){ // 存在?
			DxDraw->BmpAtlas.useIndex = (*CacheID - 1) % DxDraw->BmpAtlas.AtlasItems;
			return TRUE;
		}
	}

	// CacheID を新規割当て
	// ※ *CacheID = 0 のときもあるが、即座に無効扱いになるため問題なし
	*CacheID = DxDraw->BmpAtlas.IndexNow++;
	DxDraw->BmpAtlas.useIndex = (*CacheID - 1) % DxDraw->BmpAtlas.AtlasItems;
	if ( (DxDraw->BmpAtlas.IndexNow - DxDraw->BmpAtlas.IndexMin) >=
			DxDraw->BmpAtlas.AtlasItems ){
		DxDraw->BmpAtlas.IndexMin++;
	}
	return FALSE;
}

void DxDrawAtlas(DXDRAWSTRUCT *DxDraw, BITMAPINFOHEADER *bmiHeader, LPVOID lpBits, const RECT *box)
{
	// 保存用の bitmap を用意
	if ( (DxDraw->BmpAtlas.bmp == NULL) &&
		 (bmiHeader->biWidth <= DEFATLASSIZE) &&
		 (bmiHeader->biHeight <= DEFATLASSIZE) ){
		D2D1_SIZE_U atlassize = { DEFATLASSIZE, DEFATLASSIZE };
		D2D1_BITMAP_PROPERTIES bmpprop;

		bmpprop.dpiX = DxDraw->DeskScale.x;
		bmpprop.dpiY = DxDraw->DeskScale.y;
		bmpprop.pixelFormat = D2D1::PixelFormat(
				DXGI_FORMAT_B8G8R8A8_UNORM,
				D2D1_ALPHA_MODE_PREMULTIPLIED);

		if ( SUCCEEDED(DxDraw->HwndRenderTarget->CreateBitmap(atlassize, bmpprop, &DxDraw->BmpAtlas.bmp)) ){
			DxDraw->BmpAtlas.AtlasCols = (DEFATLASSIZE / bmiHeader->biWidth);
			DxDraw->BmpAtlas.AtlasItems =
					DxDraw->BmpAtlas.AtlasCols *
					(DEFATLASSIZE / bmiHeader->biHeight);
			DxDraw->BmpAtlas.ItemSize.cx = bmiHeader->biWidth;
			DxDraw->BmpAtlas.ItemSize.cy = bmiHeader->biHeight;

			DxDraw->BmpAtlas.IndexMin = DxDraw->BmpAtlas.IndexNow - 1;
			DxDraw->BmpAtlas.useIndex = (DxDraw->BmpAtlas.useIndex - 1) % DxDraw->BmpAtlas.AtlasItems; // 仮の値から生成
		}
	}
	D2D1_RECT_F d2bmpbox;
	D2D1_RECT_F d2box = {
			static_cast<float>(box->left),
			static_cast<float>(box->top),
			static_cast<float>(box->left + box->right),
			static_cast<float>(box->top + box->bottom) };

	if ( DxDraw->BmpAtlas.bmp != NULL ){
		if ( bmiHeader == NULL ){ // 該当有り。キャッシュを描画
			d2bmpbox.left = static_cast<float>( (DxDraw->BmpAtlas.useIndex % DxDraw->BmpAtlas.AtlasCols) * DxDraw->BmpAtlas.ItemSize.cx ) * DxDraw->DeskScaleRate.x;
			d2bmpbox.top  = static_cast<float>( (DxDraw->BmpAtlas.useIndex / DxDraw->BmpAtlas.AtlasCols) * DxDraw->BmpAtlas.ItemSize.cy ) * DxDraw->DeskScaleRate.y;
			d2bmpbox.right  = d2bmpbox.left + static_cast<float>(DxDraw->BmpAtlas.ItemSize.cx) * DxDraw->DeskScaleRate.x;
			d2bmpbox.bottom = d2bmpbox.top  + static_cast<float>(DxDraw->BmpAtlas.ItemSize.cy) * DxDraw->DeskScaleRate.y;
		}else{ // キャッシュとして保存＆描画
			D2D1_RECT_U destbox;

			destbox.left = (DxDraw->BmpAtlas.useIndex % DxDraw->BmpAtlas.AtlasCols) * bmiHeader->biWidth;
			destbox.top  = (DxDraw->BmpAtlas.useIndex / DxDraw->BmpAtlas.AtlasCols) * bmiHeader->biHeight;
			destbox.right  = destbox.left + bmiHeader->biWidth;
			destbox.bottom = destbox.top + bmiHeader->biHeight;
			DxDraw->BmpAtlas.bmp->CopyFromMemory(&destbox, lpBits, bmiHeader->biWidth * sizeof(DWORD));

			d2bmpbox.left = static_cast<float>(destbox.left) * DxDraw->DeskScaleRate.x;
			d2bmpbox.top  = static_cast<float>(destbox.top) * DxDraw->DeskScaleRate.x;
			d2bmpbox.right  = static_cast<float>(destbox.right) * DxDraw->DeskScaleRate.x;
			d2bmpbox.bottom = static_cast<float>(destbox.bottom) * DxDraw->DeskScaleRate.x;
		}
		DxDraw->HwndRenderTarget->DrawBitmap(DxDraw->BmpAtlas.bmp, d2box , 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, d2bmpbox );
		return;
	}else{ // キャッシュができないので描画のみする
		ID2D1Bitmap *d2bmp;
		D2D1_SIZE_U bmpsize;
		D2D1_BITMAP_PROPERTIES bmpprop;

		bmpprop.dpiX = DxDraw->DeskScale.x;
		bmpprop.dpiY = DxDraw->DeskScale.y;
		bmpprop.pixelFormat = D2D1::PixelFormat(
				DXGI_FORMAT_B8G8R8A8_UNORM,
				D2D1_ALPHA_MODE_PREMULTIPLIED);
		bmpsize.width = bmiHeader->biWidth;
		bmpsize.height = bmiHeader->biHeight;

		if ( SUCCEEDED(DxDraw->HwndRenderTarget->CreateBitmap(bmpsize, lpBits, bmiHeader->biWidth * 4/* Alignment済み*/ , bmpprop, &d2bmp)) ){
			d2bmpbox.left = 0.0f;
			d2bmpbox.top  = 0.0f;
			d2bmpbox.right  = static_cast<float>(bmpsize.width);
			d2bmpbox.bottom = static_cast<float>(bmpsize.height);

			DxDraw->HwndRenderTarget->DrawBitmap(d2bmp, d2box , 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, d2bmpbox );
			d2bmp->Release();
		}
	}
}

#endif
// DirectX 9 版 ---------------------------------------------------------------
#if DRAWMODE == DRAWMODE_3D
#include <d3d9.h>
#include <d3dx9.h>

#define USEDXTEXT 0

#define TEXSIZE 1024
#define FONTSIZEH 48
#define ROTATE180 D3DX_PI
#define ROTATE90	(D3DX_PI / 2.0f)
#define D3D_ProjScaleX (1.0f/4096.0f) // 縮尺(X)
#define D3D_ProjScaleY (-D3D_ProjScaleX) // 縮尺(Y)
#define D3D_CameraZrate -3.732050808f // -tan(M_PI - (M_PI / 6) / 2)

#define MAG2D 0.001822f
#define OF2D  0.134144f
float fixd = 0;

// Z座標
#define ZPOS_MESH			0.02f	// 飾り
#define ZPOS_CURSOR_FG		0.0005f	// カーソル(前)
#define ZPOS_CURSOR_BACK	0.0001f	// カーソル(背後)
#define ZPOS_TEXT			0.0f	// 一般の文字

// カーソルグラデーション
#define CURSOR_G1_COLOR 0xc0c0c0
#define CURSOR_G2_COLOR 0x0
// カーソル透明度
#define CURSOR_ALPHA 0x70000000

#ifndef SAFE_DELETE
#define SAFE_DELETE(val) { if (val){ delete val; val = NULL; }}
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(tbl) { if (tbl){ delete[] tbl; tbl = NULL; }}
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(ifptr) { if (ifptr){ ifptr->Release(); ifptr = NULL; }}
#endif

struct ScreenInfo {
	HWND hWnd;
	LPDIRECT3DDEVICE9 D3DDev;
	float ClientWidth, ClientHeight;
	int ClientWidthInt, ClientHeightInt;
	int fontH;
	COLORREF backgroundcolor;
	COLORREF TextColor, TextBackColor;
	IDirect3DVertexBuffer9 *PanelVertex;
	POINT lastpos;
	int BkMode;
};

D3DPRESENT_PARAMETERS defaultd3dpp = {0, 0, D3DFMT_UNKNOWN, 0,
		D3DMULTISAMPLE_NONE, 0,
		D3DSWAPEFFECT_DISCARD /*D3DSWAPEFFECT_FLIP*/,
		NULL, TRUE, TRUE, D3DFMT_D16, 0, 0, D3DPRESENT_INTERVAL_DEFAULT};

#define FVF_DEFFVF ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 )
struct VEX_DEFFVF {
	float x, y, z; // 頂点座標
	D3DCOLOR color;
	float u, v; //　テクスチャ座標
};

#define VFS_DEFPANEL 4
const VEX_DEFFVF vf_defpanel[VFS_DEFPANEL]= {
	{  1.0f,  0.0f, 0.0f, D3DCOLOR_RGBA(255, 255, 255, 255), 1.0f, 0.0f},
	{  1.0f,  1.0f, 0.0f, D3DCOLOR_RGBA(255, 255, 255, 255), 1.0f, 1.0f},
	{  0.0f,  0.0f, 0.0f, D3DCOLOR_RGBA(255, 255, 255, 255), 0.0f, 0.0f},
	{  0.0f,  1.0f, 0.0f, D3DCOLOR_RGBA(255, 255, 255, 255), 0.0f, 1.0f}
};

const D3DMATRIX IdentityMatrix = { // 単位行列
	{1.0f, 0.0f, 0.0f, 0.0f,
	 0.0f, 1.0f, 0.0f, 0.0f,
	 0.0f, 0.0f, 1.0f, 0.0f,
	 0.0f, 0.0f, 0.0f, 1.0f}
};

/* 標準プロジェクト行列
	#define D3D_Angle 0.523598667f // (M_PI / 6) = 30°
	#include <stdlib.h>
	char a[300];

	D3DXMatrixPerspectiveFovLH(&matProj, D3D_Angle, 1.0f, 0.01f, 100.0f);
	sprintf(a, "%f %f %f %f / %f %f %f %f / %f %f %f %f / %f %f %f %f",
	matProj._11, matProj._12, matProj._13, matProj._14,
	matProj._21, matProj._22, matProj._23, matProj._24,
	matProj._31, matProj._32, matProj._33, matProj._34,
	matProj._41, matProj._42, matProj._43, matProj._44);
	XMessage(NULL, NULL, XM_DbgLOG, T("%s"), a);
*/
const D3DMATRIX DefProjMatrix = {
	{3.732051618f, 0.0f, 0.0f, 0.0f,
	 0.0f, 3.732051618f, 0.0f, 0.0f,
	 0.0f, 0.0f, 1.0001f, 1.0f,
	 0.0f, 0.0f, -0.010001f, 0.0f}
};

// 大きさと平行移動を 2D で設定する(回転不可)
#define SetMatrix2D_ST(mat, x, y, w, h) { mat._11 = w ; mat._22 = h ; mat._41 = x ;mat._42 = y; }

#define DEBUGMESSIZE 3000
TCHAR g_DebugMes[DEBUGMESSIZE];

void PutLog(const TCHAR *string, ...)
{
	va_list argptr;
	size_t len;

	len = tstrlen(g_DebugMes);
	if ( (len + (DEBUGMESSIZE / 10)) >= DEBUGMESSIZE ) return;
	va_start(argptr, string);
#ifdef UNICODE
	vswprintf(g_DebugMes + len, string, argptr);
#else
	vsprintf(g_DebugMes + len, string, argptr);
#endif
	va_end(argptr);
	tstrcat(g_DebugMes + len, T("\n"));
}

#define DDCI_ReturnLog(result, mes) {Release(); PutLog(mes); return result;}

void DrawDIB(ScreenInfo *sinfo, BITMAPINFOHEADER *bmiHeader, LPVOID lpBits, const RECT *box);
void DrawTex(ScreenInfo *sinfo, IDirect3DTexture9 *Tex, int left, int top, int width, int height);

enum D3DDC_STATE { D3DDC_DRAWING, D3DDC_ENABLE, D3DDC_REQDISABLE, D3DDC_DISABLE, D3DDC_INIT};

class D3DDeviceClass
{
private:
	ScreenInfo *m_sinfo;
	IDirect3D9 *m_D3D;
	D3DPRESENT_PARAMETERS m_d3dpp;
	D3DCOLOR m_BackColor;
#if USEDXTEXT
	ID3DXFont *MesFont;
#endif
	D3DDC_STATE m_State;
	DWORD m_ThreadID; // この D3DDeviceClass が稼働している Window の Thread

public:
	D3DDeviceClass()
	{
		m_D3D = NULL;
		m_State = D3DDC_INIT;
#if USEDXTEXT
		MesFont = NULL;
#endif
	}

	~D3DDeviceClass()
	{
		Release();
	}

	D3DXMATRIX MatView3D, MatView2D;

	BOOL Init(ScreenInfo *sinfo)
	{
		D3DDISPLAYMODE dmode;
		IDirect3DDevice9 *D3DDev;

		m_sinfo = sinfo;
		m_ThreadID = GetWindowThreadProcessId(m_sinfo->hWnd, NULL);
		m_sinfo->D3DDev = NULL;
		if ( m_ThreadID != GetCurrentThreadId() ){
		//	Message(T("D3DDevice->Init thread error!"));
			return FALSE;
		}

		// CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		m_D3D = Direct3DCreate9(D3D_SDK_VERSION);
		if ( m_D3D == NULL ){
			Release();
		//	Message(T("D3DDevice->Init Create9 error!"));
			return FALSE;
		}

		m_D3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dmode);

		m_d3dpp = defaultd3dpp;
		m_d3dpp.hDeviceWindow = sinfo->hWnd;
		m_d3dpp.BackBufferFormat = dmode.Format;
		if( FAILED(m_D3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
				sinfo->hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING,
				&m_d3dpp, &D3DDev)) ){
			if( FAILED(m_D3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
					sinfo->hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
					&m_d3dpp, &D3DDev)) ){
				DDCI_ReturnLog(FALSE, T("CreateDevice error"));
			}
		}
		m_sinfo->D3DDev = D3DDev;
		m_BackColor = 0x00FFFFFF;

		// 汎用頂点バッファ作成
		if( FAILED(m_sinfo->D3DDev->CreateVertexBuffer(
				sizeof(VEX_DEFFVF) * VFS_DEFPANEL, D3DUSAGE_WRITEONLY,
				FVF_DEFFVF, D3DPOOL_MANAGED, &m_sinfo->PanelVertex, NULL))){
			return FALSE;
		}else{ // 頂点情報の書き込み
			void *pData;

			if( FAILED(m_sinfo->PanelVertex->Lock(0,
					sizeof(VEX_DEFFVF) * VFS_DEFPANEL, (void**)&pData, 0)) ){
				return FALSE;
			}
			memcpy(pData, vf_defpanel, sizeof(vf_defpanel));
			m_sinfo->PanelVertex->Unlock();
		}
		InitDevice();
		return TRUE;
	}

	BOOL InitDevice(void)
	{
		IDirect3DDevice9 *D3DDev = m_sinfo->D3DDev;

		if ( m_ThreadID != GetCurrentThreadId() ){
		//	Message(T("D3DDevice->InitDevice thread error!"));
			return FALSE;
		}
#if USEDXTEXT
		if ( FAILED(D3DXCreateFontW(D3DDev, 10, 0, FW_BOLD, 0, FALSE,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY,
				DEFAULT_PITCH | FF_DONTCARE, L"ＭＳ 明朝", &MesFont)) ){
			DDCI_ReturnLog(FALSE, T("D3DXCreateFont error"));
		}
#endif
		D3DDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		D3DDev->SetRenderState(D3DRS_ZENABLE, TRUE);

		D3DDev->SetRenderState(D3DRS_LIGHTING, FALSE);
		// AlphaBlend
		D3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		D3DDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		D3DDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

		D3DDev->SetRenderState(D3DRS_AMBIENT, 0x101010);

		D3DDev->SetRenderState(D3DRS_DITHERENABLE, TRUE);
//		D3DDev->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);

//		D3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
//		D3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		D3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		D3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);

		//	Z軸正の方向から原点
		float Aspect = (m_sinfo->ClientHeight / m_sinfo->ClientWidth);
		float CameraX = (m_sinfo->ClientWidth + 0.0f) * (D3D_ProjScaleX * 0.5f);
		float CameraY = (m_sinfo->ClientHeight + 0.0f) * (D3D_ProjScaleY * 0.5f);
		float CameraZ = D3D_CameraZrate * CameraX * Aspect;

		D3DXVECTOR3 vec3From(CameraX, CameraY, CameraZ);
		D3DXVECTOR3 vec3At(CameraX, CameraY, CameraZ + 1.0f);
		D3DXVECTOR3 vec3Up(0.0f, 1.0f, 0.0f);
		D3DXMatrixLookAtLH(&MatView3D, &vec3From, &vec3At, &vec3Up);

		D3DXMatrixOrthoLH(&MatView2D,
				MAG2D * m_sinfo->ClientHeight, MAG2D * m_sinfo->ClientHeight,
				-1.0f, 1.0f);
		D3DXMATRIX matD = IdentityMatrix;
		matD = IdentityMatrix;
		matD._41 -= OF2D / Aspect;
		matD._42 += OF2D;
		D3DXMatrixMultiply(&MatView2D, &MatView2D, &matD);

		D3DDev->SetTransform(D3DTS_VIEW, &MatView2D);

		//プロジェクション
#if 1
		D3DMATRIX matProj = DefProjMatrix;

		matProj._11 *= m_sinfo->ClientHeight / m_sinfo->ClientWidth;
		D3DDev->SetTransform(D3DTS_PROJECTION, &matProj);
#else // 等価処理
		#define D3D_Angle 0.523598667f // (M_PI / 6) = 30°
		D3DXMATRIX matProj;
		D3DXMatrixPerspectiveFovLH(&matProj, D3D_Angle,
				m_sinfo->ClientWidth / m_sinfo->ClientHeight,
				0.01f, 100.0f);
		D3DDev->SetTransform(D3DTS_PROJECTION, &matProj);
#endif
		m_State = D3DDC_ENABLE;
		return TRUE;
	}

	void SetBackColor(COLORREF backcolor)
	{
		m_BackColor = D3DCOLORfromCOLORREF(backcolor);
	}

	BOOL SetBenchmarkMode(BOOL mode)
	{
		DWORD newmode;

		newmode = mode ?
				D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_DEFAULT;
		if ( m_d3dpp.PresentationInterval != newmode ){
			m_d3dpp.PresentationInterval = newmode;
			return TRUE;
		}else{
			return FALSE;
		}
	}

	void Release(void)
	{
		if ( m_ThreadID != GetCurrentThreadId() ){
		//	Message(T("D3DDevice->Release thread error!"));
		}
#if USEDXTEXT
		SAFE_RELEASE(MesFont);
#endif
		SAFE_RELEASE(m_sinfo->PanelVertex);
		SAFE_RELEASE(m_sinfo->D3DDev);
		SAFE_RELEASE(m_D3D);
		// CoUninitialize();
	}

	void DrawIcon(HICON icon, const RECT *box)
	{
		LPVOID lpBits;
		BITMAPINFO bmi;
		BITMAPINFOHEADER bmiHeader;
		HBITMAP hbmp;
		HGDIOBJ hOldBmp;
		HDC hDC;

		memset(&bmiHeader, 0, sizeof(BITMAPINFOHEADER));
		bmiHeader.biSize      = sizeof(BITMAPINFOHEADER);
		bmiHeader.biWidth     = 32;
		bmiHeader.biHeight    = 32;
		bmiHeader.biPlanes    = 1;
		bmiHeader.biBitCount  = 32;
		bmi.bmiHeader = bmiHeader;
		hbmp = CreateDIBSection(NULL, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS, &lpBits, NULL, 0);
		hDC = CreateCompatibleDC(NULL);
		hOldBmp = SelectObject(hDC, hbmp);
		DrawIconEx(hDC, 0, 0, icon, bmiHeader.biWidth, bmiHeader.biHeight, 0, NULL, DI_NORMAL);
		SelectObject(hDC, hOldBmp);
		DeleteDC(hDC);

		DrawDIB(m_sinfo, &bmiHeader, lpBits, box);

		DeleteObject(hbmp); // ビットマップのメモリも削除
	}

	BOOL BeginRender(void)
	{
		if ( m_State != D3DDC_ENABLE ) return FALSE;

		IDirect3DDevice9 *D3DDev = m_sinfo->D3DDev;

		D3DDev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER /*| D3DCLEAR_STENCIL*/, m_BackColor, 1.0f, 0);
		D3DDev->BeginScene();
		m_State = D3DDC_DRAWING;
		return TRUE;
	}

	BOOL EndRender(void)
	{
		if ( m_State != D3DDC_DRAWING ) return FALSE;
		m_State = D3DDC_ENABLE;

		IDirect3DDevice9 *D3DDev = m_sinfo->D3DDev;

		D3DDev->EndScene();
		if ( D3DDev->Present(NULL, NULL, NULL, NULL) == D3DERR_DEVICELOST ){
			RequestReset();
			return FALSE;
		}
		return TRUE;
	}

	void RequestReset(void)
	{
		if ( m_State == D3DDC_ENABLE ) m_State = D3DDC_REQDISABLE;
	}

	BOOL DeviceCheck(void)
	{
		IDirect3DDevice9 *D3DDev = m_sinfo->D3DDev;

		if ( m_State == D3DDC_INIT ) return FALSE;
		if ( D3D_OK == D3DDev->TestCooperativeLevel() ){
			return TRUE;
		}
		return FALSE;
	}

	BOOL ResetDevice(void)
	{
		D3DDISPLAYMODE dmode;
		HRESULT hr;

		if ( m_ThreadID != GetCurrentThreadId() ){
		//	Message(T("D3DDevice->Reset thread error!"));
			return FALSE;
		}
		if ( m_State != D3DDC_DISABLE ){
			if ( m_State == D3DDC_INIT ) return FALSE;
			if ( m_State == D3DDC_ENABLE ){
#if USEDXTEXT
				SAFE_RELEASE(MesFont);
#endif
			}
			m_State = D3DDC_DISABLE;
		}
		IDirect3DDevice9 *D3DDev = m_sinfo->D3DDev;

		hr = D3DDev->TestCooperativeLevel();
		if ( (hr == D3DERR_DEVICELOST) || (hr == D3DERR_DRIVERINTERNALERROR ) ){
			return FALSE; // まだリセットできない
		}

		m_D3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dmode);
		m_d3dpp.BackBufferWidth = 0;
		m_d3dpp.BackBufferHeight = 0;
		m_d3dpp.BackBufferFormat = dmode.Format;
		if ( D3DDev->Reset(&m_d3dpp) != D3D_OK ){
//			MessageA("Display Reset error");
			return FALSE;
		}else{
			InitDevice();
			return TRUE;
		}
	}
#if USEDXTEXT
	void DrawText(const TCHAR *str, RECT *box)
	{
		if ( box == NULL ){
			RECT dummybox = {0, 0, 800, 80};

			box = &dummybox;
		}
		if ( MesFont != NULL ){
			MesFont->DrawText(NULL, str, -1, box, DT_NOCLIP, D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f ));
		}
	}
#endif
};

struct FontInfoStruct {
	WORD dataX, dataY; // データの位置
	WORD Xoffset; // 表示位置X
	WORD showW; // 表示幅
	WORD cellW; // 字送りの幅
};

const MAT2 GetFontBmpMat = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};

class DxCellCursorClass
{
private:
	ScreenInfo *m_sinfo;
	D3DMATRIX m_ShowPosMat; // 表示位置を決める行列
	D3DMATRIX m_ShowBackPosMat; // 表示位置を決める行列
	IDirect3DTexture9 *m_Tex;
	int m_boxwidth, m_boxheight;

	void MakeTex(void)
	{
		LPVOID lpBits;
		BITMAPINFO bmi;
		BITMAPINFOHEADER bmiHeader;
		HBITMAP hbmp;
		HGDIOBJ hOldBmp;
		HDC hDC;
		TRIVERTEX vt[2];

		SAFE_RELEASE(m_Tex);
		memset(&bmiHeader, 0, sizeof(BITMAPINFOHEADER));
		bmiHeader.biSize      = sizeof(BITMAPINFOHEADER);
		bmiHeader.biWidth     = m_boxwidth;
		bmiHeader.biHeight    = m_boxheight;
		bmiHeader.biPlanes    = 1;
		bmiHeader.biBitCount  = 32;
		bmi.bmiHeader = bmiHeader;
		hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &lpBits, NULL, 0);
		hDC = CreateCompatibleDC(NULL);
		hOldBmp = SelectObject(hDC, hbmp);

		COLORREF c1 = CURSOR_G1_COLOR;
		COLORREF c2 = CURSOR_G2_COLOR;
		vt[0].x = 0;
		vt[0].y = 0;
		vt[0].Red = (COLOR16)(GetRValue(c1) * 0x100);
		vt[0].Green = (COLOR16)(GetGValue(c1) * 0x100);
		vt[0].Blue = (COLOR16)(GetBValue(c1) * 0x100);
		vt[0].Alpha = 0xffff;
		vt[1].x = bmiHeader.biWidth;
		vt[1].y = bmiHeader.biHeight;
		vt[1].Red = (COLOR16)(GetRValue(c2) * 0x100);
		vt[1].Green = (COLOR16)(GetGValue(c2) * 0x100);
		vt[1].Blue = (COLOR16)(GetBValue(c2) * 0x100);
		vt[1].Alpha = 0xffff;

		GRADIENT_RECT gr;
		gr.UpperLeft = 1;
		gr.LowerRight = 0;

		GradientFill(hDC, vt, 2, &gr, 1, GRADIENT_FILL_RECT_V);

		HGDIOBJ hOldPen = SelectObject(hDC, GetStockObject(WHITE_PEN));
		HGDIOBJ hOldBrush = SelectObject(hDC, GetStockObject(HOLLOW_BRUSH));

		RoundRect(hDC, 0, 0, bmiHeader.biWidth, bmiHeader.biHeight, bmiHeader.biHeight / 3, bmiHeader.biHeight / 3);

		SelectObject(hDC, hOldBrush);
		SelectObject(hDC, hOldPen);

		SelectObject(hDC, hOldBmp);
		DeleteDC(hDC);

		D3DLOCKED_RECT LockedRect;

		if ( SUCCEEDED(m_sinfo->D3DDev->CreateTexture(
				bmiHeader.biWidth, bmiHeader.biHeight, 1, D3DUSAGE_DYNAMIC,
				D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_Tex, NULL)) ){
			if( SUCCEEDED(m_Tex->LockRect(0, &LockedRect, NULL, D3DLOCK_DISCARD)) ){
				DWORD blockW = bmiHeader.biWidth;

				for ( int y = bmiHeader.biHeight - 1 ; y >= 0 ; y-- ){
					const DWORD *src = (DWORD *)(BYTE *)lpBits + (bmiHeader.biHeight - 1 - y) * blockW;
					DWORD *dest = (DWORD *)(BYTE *)((BYTE *)LockedRect.pBits + y * LockedRect.Pitch);
					for ( int x = 0 ; x < bmiHeader.biWidth ; x++ ){
						DWORD data = *src++;
						*dest++ = data | (data << 8);
					}
				}
				m_Tex->UnlockRect(0);
			}
		}
		DeleteObject(hbmp); // ビットマップのメモリも削除
	}

public:
	~DxCellCursorClass()
	{
		Free();
	}

	BOOL Init(ScreenInfo *sinfo)
	{
		m_sinfo = sinfo;
		m_Tex = NULL;
		m_boxwidth = -1;
		m_ShowPosMat = IdentityMatrix;
		m_ShowPosMat._43 = ZPOS_CURSOR_FG;
		m_ShowBackPosMat = IdentityMatrix;
		m_ShowBackPosMat._43 = ZPOS_CURSOR_BACK;
		return TRUE;
	}

	void Free(void)
	{
		SAFE_RELEASE(m_Tex);
	}

	int Draw(int x, int y, int width, int height, COLORREF color)
	{
		IDirect3DDevice9 *D3DDev = m_sinfo->D3DDev;

		if ( (m_Tex == NULL) || (width != m_boxwidth) || (height != m_boxheight) ){
			m_boxwidth = width;
			m_boxheight = height;
			MakeTex();
		}

		D3DDev->SetFVF(FVF_DEFFVF);
		D3DDev->SetTexture(0, m_Tex);
		D3DDev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		D3DDev->SetStreamSource(0, m_sinfo->PanelVertex, 0, sizeof(VEX_DEFFVF));

//		D3DDev->SetTextureStageState(0, D3DTSS_COLOROP  , D3DTOP_SELECTARG1);
		D3DDev->SetTextureStageState(0, D3DTSS_COLOROP  , D3DTOP_MODULATE);
		D3DDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		D3DDev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

		D3DDev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLORfromCOLORREF(color));

		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAOP  , D3DTOP_SELECTARG1);
		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		SetMatrix2D_ST(m_ShowPosMat,
				(float)x * D3D_ProjScaleX,		(float)y * D3D_ProjScaleY,
				(float)width * D3D_ProjScaleX,	(float)height * D3D_ProjScaleY
		);
		D3DDev->SetTransform(D3DTS_WORLD, &m_ShowPosMat);

		D3DDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		return TRUE;
	}

	int DrawBack(int x, int y, int width, int height, COLORREF color)
	{
		IDirect3DDevice9 *D3DDev = m_sinfo->D3DDev;

		D3DDev->SetFVF(FVF_DEFFVF);

		D3DDev->SetStreamSource(0, m_sinfo->PanelVertex, 0, sizeof(VEX_DEFFVF));

		D3DDev->SetTextureStageState(0, D3DTSS_COLOROP  , D3DTOP_SELECTARG1);
		D3DDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
		D3DDev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLORfromCOLORREF(color) | CURSOR_ALPHA);

		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAOP  , D3DTOP_SELECTARG1);
		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);

		SetMatrix2D_ST(m_ShowBackPosMat,
				(float)x * D3D_ProjScaleX,		(float)y * D3D_ProjScaleY,
				(float)width * D3D_ProjScaleX,	(float)height * D3D_ProjScaleY
		);
		D3DDev->SetTransform(D3DTS_WORLD, &m_ShowBackPosMat);

		D3DDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		return TRUE;
	}
};

#ifdef UNICODE
	#define GetCodeAndNext(text, max) *text++
#else
	#define GetCodeAndNext(text, max) GetCodeAndNextMain(&text, max)
	WORD USEFASTCALL GetCodeAndNextMain(const char **text, const char *max)
	{
		WORD code;

		code = (WORD)*(BYTE *)(*text)++;
		if ( Iskanji(code) && (*text < max) ){
			return (code << 8) | (WORD)*(BYTE *)(*text)++;
		}else{
			return code;
		}
	}
#endif

class DxTextClass
{
private:
	ScreenInfo *m_sinfo;
	IDirect3DTexture9 *m_Tex;
	D3DMATRIX m_TextureTransMat; // テクスチャの表示範囲を決める行列
	D3DMATRIX m_ShowPosMat; // 表示位置を決める行列
	D3DCOLOR m_textcolor;
	HFONT m_hFont;
	FontInfoStruct m_fontinfo[0x10000]; // 各文字の情報 indexは文字コード
	struct {
		DWORD writeX, writeY; // 書き込み位置
		DWORD tmAveCharWidth, tmHeight;
		DWORD FixY; // ビットマップを書き込むときの補正値
	} m_make;
	DWORD m_omsize; // 省略符号表示幅

public:
	~DxTextClass()
	{
		Release();
	}

	BOOL Init(ScreenInfo *sinfo)
	{
		m_sinfo = sinfo;
		m_Tex = NULL;
		m_textcolor = 0xffffff;
		m_hFont = NULL;
		m_TextureTransMat = IdentityMatrix;
		m_ShowPosMat = IdentityMatrix;
		Free();
		return TRUE;
	}

	void Release(void)
	{
		SAFE_RELEASE(m_Tex);
	}

	void Free(void)
	{
		memset(m_fontinfo, 0, sizeof(m_fontinfo));
		SAFE_RELEASE(m_Tex);
	}

	void SetFontGlyph(WORD code, HDC hDC, D3DLOCKED_RECT LockedRect)
	{
		GLYPHMETRICS gm;
		BYTE fontbmp[FONTSIZEH * FONTSIZEH * 4];
		UINT mode = GGO_BITMAP;
//		UINT mode = GGO_GRAY8_BITMAP;

		if ( (GetGlyphOutline(hDC, code, mode, &gm, sizeof(fontbmp),
				&fontbmp, &GetFontBmpMat) == 0) || (gm.gmBlackBoxX == 0) ){
			gm.gmBlackBoxX = 0;
			gm.gmCellIncX = (WORD)m_make.tmAveCharWidth;
		}else{
			if ( (m_make.writeX + gm.gmBlackBoxX + 1) > TEXSIZE ){
				m_make.writeX = 1;
				if ( (m_make.writeY + m_make.tmHeight * 2) < TEXSIZE ){
					m_make.writeY += m_make.tmHeight;
				}
			}
			DWORD writeY = m_make.FixY - gm.gmptGlyphOrigin.y + m_make.writeY;

			if ( mode == GGO_GRAY8_BITMAP ){
				DWORD blockW = DwordAlignment(gm.gmBlackBoxX);
				for ( int y = gm.gmBlackBoxY ; y >= 0 ; y-- ){
					const BYTE *src = fontbmp + y * blockW;
					DWORD *dest = (DWORD *)(BYTE *)((BYTE *)LockedRect.pBits +
							(writeY + y) * LockedRect.Pitch) + m_make.writeX;
					for ( UINT x = 0 ; x < gm.gmBlackBoxX ; x++ ){
						*dest++ = 0xffffff | (*src++ * (0xff000000 / 64));
					}
				}
			}else{
				DWORD blockW = DwordAlignment((gm.gmBlackBoxX + 7) / 8);
				for ( int y = gm.gmBlackBoxY ; y >= 0 ; y-- ){
					const BYTE *src = fontbmp + y * blockW;
					DWORD *dest = (DWORD *)(BYTE *)((BYTE *)LockedRect.pBits +
							(writeY + y) * LockedRect.Pitch) + m_make.writeX;
					for ( int x = gm.gmBlackBoxX ; x >= 0 ; ){
						BYTE data = *src++;
						for ( UINT b = 0 ; b < 8 ; b++ ){
							*dest++ = (data & B7) ? 0xffffffff : 0;
							x--;
							if ( x < 0 ) break;
							data <<= 1;
						}
					}
				}
			}
		}
		m_fontinfo[code].dataX = (WORD)m_make.writeX;
		m_fontinfo[code].dataY = (WORD)m_make.writeY;
		m_fontinfo[code].Xoffset = (WORD)gm.gmptGlyphOrigin.x;
		m_fontinfo[code].showW = (WORD)gm.gmBlackBoxX;
		m_fontinfo[code].cellW = (WORD)gm.gmCellIncX;
//		XMessage(NULL, NULL, XM_DbgLOG, T("Code:%x (%d,%d) %d %d"), code, m_make.writeX, m_make.writeY, gm.gmBlackBoxX, gm.gmCellIncX);
		m_make.writeX += gm.gmBlackBoxX + 1; // 文字が隣接するとアンチエイリアスによってゴミが入るので隙間を空ける
	}

	void MakeGlyph(const TCHAR *text, const TCHAR *max)
	{
		D3DLOCKED_RECT LockedRect;
		IDirect3DDevice9 *D3DDev = m_sinfo->D3DDev;
		WORD code;
		HDC hDC;
		HGDIOBJ hOldFont;

		if ( m_Tex == NULL ){ // テクスチャ作成・初期化
			TEXTMETRIC tm;
//A4R4G4B4
			if ( FAILED(D3DDev->CreateTexture(TEXSIZE, TEXSIZE, 1,
					D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
					&m_Tex, NULL)) ){
				m_Tex = NULL;
				return;
			}
			if( FAILED(m_Tex->LockRect(0, &LockedRect, NULL, D3DLOCK_DISCARD)) ){
				return;
			}
			memset((BYTE *)LockedRect.pBits, 0, TEXSIZE * TEXSIZE * sizeof(DWORD));
			hDC = GetDC(m_sinfo->hWnd);
			hOldFont = SelectObject(hDC, m_hFont);

			GetAndFixTextMetrics(hDC, &tm);

			m_TextureTransMat._22 = (float)tm.tmHeight * (1.0f / (float)TEXSIZE); // height

			m_make.writeX = 1;
			m_make.writeY = 0;
			m_make.tmAveCharWidth = tm.tmAveCharWidth;
			m_make.tmHeight = tm.tmHeight;
			m_make.FixY = tm.tmAscent;

			for ( WORD code = 0 ; code <= 32 ; code++ ){ // 制御コード・空白
				m_fontinfo[code].dataX = 1;
				m_fontinfo[code].cellW = (WORD)tm.tmAveCharWidth;
			}

			for ( WORD code = 33 ; code < 127 ; code++ ){ // ASCII
				SetFontGlyph(code, hDC, LockedRect);
			}

			m_omsize = min((m_fontinfo['.'].showW + 1) * 3, tm.tmAveCharWidth);
		}else{
			// D3DLOCK_DISCARD だと失敗することがある…
			if( FAILED(m_Tex->LockRect(0, &LockedRect, NULL, 0)) ){
				return;
			}
			hDC = GetDC(m_sinfo->hWnd);
			hOldFont = SelectObject(hDC, m_hFont);
		}

		while ( text < max ){ // 未作成の文字を一気に作成
			code = GetCodeAndNext(text, max);
			if ( m_fontinfo[code].dataX != 0 ) continue;
			SetFontGlyph(code, hDC, LockedRect);
		}
		SelectObject(hDC, hOldFont);
		ReleaseDC(m_sinfo->hWnd, hDC);
		m_Tex->UnlockRect(0);
	}

	void SetFont(HFONT hFont)
	{
		if ( m_hFont != hFont ){
			Free();
			m_hFont = hFont;
		}
	}

	void SetTextColor(COLORREF newcolor)
	{
		if ( newcolor != m_textcolor ) m_textcolor = D3DCOLORfromCOLORREF(newcolor);
	}

	void DrawTest(void)
	{
		if ( m_Tex != NULL ) ::DrawTex(m_sinfo, m_Tex, 0, 0, TEXSIZE, TEXSIZE);
	}

	int Draw(const TCHAR *text, int x, int y, int maxX, int len)
	{
		IDirect3DDevice9 *D3DDev = m_sinfo->D3DDev;

		if ( m_Tex == NULL ){ // テクスチャ作成
			if ( m_hFont == NULL ) return x;
			MakeGlyph(text, text + len);
			if ( m_Tex == NULL ) return x;
		}
		D3DDev->SetFVF(FVF_DEFFVF);
		D3DDev->SetTexture(0, m_Tex);
		D3DDev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		D3DDev->SetStreamSource(0, m_sinfo->PanelVertex, 0, sizeof(VEX_DEFFVF));

		D3DDev->SetTextureStageState(0, D3DTSS_COLOROP  , D3DTOP_SELECTARG1);
		D3DDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
		D3DDev->SetRenderState(D3DRS_TEXTUREFACTOR, m_textcolor);

		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAOP  , D3DTOP_SELECTARG1);
		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		m_ShowPosMat._22 = (float)m_sinfo->fontH * D3D_ProjScaleY * 0.88f; // height
		m_ShowPosMat._42 = (float)y * D3D_ProjScaleY; // Y

		const TCHAR *max = text + len;
		int FmaxX = maxX - m_omsize;
		while ( text < max ){
			WORD code;
			const TCHAR *oldtext;

//			XMessage(NULL, NULL, XM_DbgLOG, T(">%d %d %s"), x, y, text);
			oldtext = text;
			code = GetCodeAndNext(text, max);
//			if ( code == '\0' ) return x;
			if ( m_fontinfo[code].dataX == 0 ){
				MakeGlyph(oldtext, max);
			}
			// 表示範囲をはみ出したときの、dot 表示
			if ( ((x + m_fontinfo[code].cellW) > FmaxX) &&
				  !((text == max) && ((x + m_fontinfo[code].cellW) <= maxX)) ){
				int showW;

				showW = m_fontinfo['.'].showW;
				m_ShowPosMat._11 = (float)showW * D3D_ProjScaleX; // width
				m_TextureTransMat._11 = (float)showW * (1.0f / (float)TEXSIZE); // width
				m_TextureTransMat._31 = (float)m_fontinfo['.'].dataX * (1.0f / (float)TEXSIZE); // delta X
				m_TextureTransMat._32 = (float)m_fontinfo['.'].dataY * (1.0f / (float)TEXSIZE); // delta Y
				D3DDev->SetTransform(D3DTS_TEXTURE0, &m_TextureTransMat);

				while ( (x + showW) <= maxX ){
					m_ShowPosMat._41 = (float)x * D3D_ProjScaleX; // X
					D3DDev->SetTransform(D3DTS_WORLD, &m_ShowPosMat);
					D3DDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
					x += showW + 1;
				}
				break;
			}

#define EXXF (1.0f * D3D_ProjScaleX)
			if ( m_fontinfo[code].showW != 0 ){
				m_TextureTransMat._11 = (float)m_fontinfo[code].showW * (1.0f / (float)TEXSIZE); // width
				m_TextureTransMat._31 = (float)m_fontinfo[code].dataX * (1.0f / (float)TEXSIZE); // delta X
				m_TextureTransMat._32 = (float)m_fontinfo[code].dataY * (1.0f / (float)TEXSIZE); // delta Y
				D3DDev->SetTransform(D3DTS_TEXTURE0, &m_TextureTransMat);
		#if 1
				m_ShowPosMat._41 = (float)(x + m_fontinfo[code].Xoffset) * D3D_ProjScaleX; // X
				m_ShowPosMat._11 = (float)m_fontinfo[code].showW * D3D_ProjScaleX; // width
				D3DDev->SetTransform(D3DTS_WORLD, &m_ShowPosMat);
				D3DDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		#else // 影付き表示
				m_ShowPosMat._41 = (float)(x + m_fontinfo[code].Xoffset) * D3D_ProjScaleX + EXXF; // X
				m_ShowPosMat._11 = (float)m_fontinfo[code].showW * D3D_ProjScaleX; // width


				D3DDev->SetRenderState(D3DRS_TEXTUREFACTOR, 0xffffff);
				m_ShowPosMat._42 -= EXXF; // Y
				m_ShowPosMat._43 += ZPOS_TEXT; // Z
				D3DDev->SetTransform(D3DTS_WORLD, &m_ShowPosMat);
				D3DDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);


				D3DDev->SetRenderState(D3DRS_TEXTUREFACTOR, m_textcolor);
				m_ShowPosMat._41 -= EXXF; // X
				m_ShowPosMat._42 += EXXF; // Y
//				m_ShowPosMat._43 -= ZPOS_TEXT; // Z

				D3DDev->SetTransform(D3DTS_WORLD, &m_ShowPosMat);
				D3DDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		#endif

			}
			x += m_fontinfo[code].cellW;
		}
		return x;
	}

	int Calc(const TCHAR *text, int maxX, int &len)
	{
		IDirect3DDevice9 *D3DDev = m_sinfo->D3DDev;

		if ( m_Tex == NULL ){ // テクスチャ作成
			if ( m_hFont == NULL ) return 0;
			MakeGlyph(text, text + len);
			if ( m_Tex == NULL ) return 0;
		}
		const TCHAR *textptr = text;
		const TCHAR *textmax = text + len;
		int FmaxX = maxX - m_omsize;
		int x = 0;
		while ( textptr < textmax ){
			WORD code;
			const TCHAR *oldtext;

			oldtext = textptr;
			code = GetCodeAndNext(textptr, textmax);
//			if ( code == '\0' ) return x;
			if ( m_fontinfo[code].dataX == 0 ){
				MakeGlyph(oldtext, textmax);
			}
			if ( ((x + m_fontinfo[code].cellW) > FmaxX) &&
				  !((textptr == textmax) && ((x + m_fontinfo[code].cellW) <= maxX)) ){
				int showW;

				showW = m_fontinfo['.'].showW;
				while ( (x + showW) <= maxX ){
					x += showW + 1;
				}
				break;
			}
			x += m_fontinfo[code].cellW;
		}
		len = textptr - text;
		return x;
	}
};

class DxMeshClass
{
private:
	ScreenInfo *m_sinfo;
// class
	ID3DXMesh *m_mesh;
	IDirect3DBaseTexture9 **m_Textures;
// struct/value
	D3DMATERIAL9 *m_Materials;
	D3DXMATRIXA16 m_MatCharFixPos; // 寸法調整
	DWORD m_nummaterials;

	float m_r; // 回転表示位置
public:
	DxMeshClass()
	{
		m_mesh = NULL;
		m_Materials = NULL;
		m_Textures = NULL;

		m_r = 0.0f;
	}

	~DxMeshClass()
	{
		Release();
	}

	void Release(void)
	{
		if ( m_Textures != NULL ){
			for( UINT i = 0 ; i < m_nummaterials ; i++ ){
				SAFE_RELEASE( m_Textures[i] );
			}
		}
		SAFE_DELETE_ARRAY(m_Textures);
		SAFE_DELETE_ARRAY(m_Materials);
		SAFE_RELEASE(m_mesh);
	}

	BOOL Init(ScreenInfo *sinfo, const TCHAR *filename)
	{
		WCHAR path[MAX_PATH], *wp;
		LPD3DXBUFFER pAdjacencyBuffer;
		LPD3DXBUFFER objmaterials;
		IDirect3DDevice9 *D3DDev = sinfo->D3DDev;

		m_sinfo = sinfo;
		if ( FAILED(D3DXLoadMeshFromX(filename, D3DXMESH_MANAGED, D3DDev,
				&pAdjacencyBuffer, &objmaterials,
				NULL, &m_nummaterials, &m_mesh)) ){
			return FALSE;
		}
		m_mesh->OptimizeInplace(D3DXMESHOPT_COMPACT | D3DXMESHOPT_ATTRSORT |
				D3DXMESHOPT_VERTEXCACHE,
				(DWORD*)pAdjacencyBuffer->GetBufferPointer(), NULL, NULL, NULL);

		if( !(m_mesh->GetFVF() & D3DFVF_NORMAL) ){ // 法線の補完
			ID3DXMesh* TempMesh;

			m_mesh->CloneMeshFVF(m_mesh->GetOptions(),
					m_mesh->GetFVF() | D3DFVF_NORMAL, D3DDev, &TempMesh );
			D3DXComputeNormals(TempMesh, NULL);
			SAFE_RELEASE(m_mesh);
			m_mesh = TempMesh;
		}
		strcpyToW(path, filename, MAX_PATH);
		wp = strchrW(path, '\\');
		if ( wp != NULL ){
			*(wp + 1) = '\0';
		}else{
			path[0] = '\0';
		}

		CreateMaterials(path, (D3DXMATERIAL*)objmaterials->GetBufferPointer());
		objmaterials->Release();

		// 大きさ調整を行う
		const float fRadius = 0.5f;
		D3DXMatrixScaling(&m_MatCharFixPos, fRadius, fRadius, fRadius);
		return TRUE;
	}

	HRESULT CreateMaterials(WCHAR *path, D3DXMATERIAL *d3dxMtrls)
	{
		IDirect3DDevice9 *D3DDev = m_sinfo->D3DDev;

		if( !d3dxMtrls || !m_nummaterials ) return S_OK;

		m_Materials = new D3DMATERIAL9[m_nummaterials];
		if( m_Materials == NULL ) return E_OUTOFMEMORY;
		m_Textures = new LPDIRECT3DBASETEXTURE9[m_nummaterials];
		if( m_Textures == NULL ) return E_OUTOFMEMORY;

		for( DWORD i = 0 ; i < m_nummaterials; i++ ){
			WCHAR TexName[MAX_PATH];
			WCHAR TexPath[MAX_PATH];
			D3DXIMAGE_INFO ImgInfo;

			m_Materials[i] = d3dxMtrls[i].MatD3D;
			m_Textures[i] = NULL;

			if( d3dxMtrls[i].pTextureFilename == NULL ) continue;

			AnsiToUnicode(d3dxMtrls[i].pTextureFilename, TexName, MAX_PATH);
			strcpyW(TexPath, path);
			strcatW(TexPath, TexName);
			if ( FAILED( D3DXGetImageInfoFromFileW(TexPath, &ImgInfo) ) ){
				PutLog(T("Load Error %s"), TexPath);
				return E_OUTOFMEMORY;
			}
			switch( ImgInfo.ResourceType ){
				case D3DRTYPE_TEXTURE: {
					IDirect3DTexture9 *pTex;

					if( SUCCEEDED( D3DXCreateTextureFromFileW(D3DDev, TexPath, &pTex) ) ){
						pTex->QueryInterface(IID_IDirect3DBaseTexture9, (LPVOID*)&m_Textures[i] );
						pTex->Release();
					}
					break;
				}
				case D3DRTYPE_CUBETEXTURE: {
					IDirect3DCubeTexture9 *pTex;

					if( SUCCEEDED( D3DXCreateCubeTextureFromFileW(D3DDev, TexPath, &pTex ) ) ){
						pTex->QueryInterface(IID_IDirect3DBaseTexture9, (LPVOID*)&m_Textures[i] );
						pTex->Release();
					}
					break;
				}
				case D3DRTYPE_VOLUMETEXTURE: {
					IDirect3DVolumeTexture9 *pTex;

					if( SUCCEEDED( D3DXCreateVolumeTextureFromFileW( D3DDev, TexPath, &pTex ) ) ){
						pTex->QueryInterface(IID_IDirect3DBaseTexture9, (LPVOID*)&m_Textures[i] );
						pTex->Release();
					}
					break;
				}
			}
		}
		return S_OK;
	}

	void Draw(float x, float y, float z, float rx, float ry, float rz)
	{
		if ( m_mesh == NULL ) return;

		D3DXMATRIXA16 matWorld, matRot, matSub, mxProj;
			// 回転
		D3DXMatrixRotationYawPitchRoll(&matSub, ry + m_r, rx - 0.5f, rz);
		m_r += 0.03f;
		D3DXMatrixMultiply(&matWorld, &m_MatCharFixPos, &matSub);
			// 位置
		D3DXMatrixTranslation(&matSub, x, y, z);
		D3DXMatrixMultiply(&matWorld, &matWorld, &matSub);

		IDirect3DDevice9 *D3DDev = m_sinfo->D3DDev;

		D3DDev->SetRenderState(D3DRS_LIGHTING, TRUE);

		D3DDev->SetTextureStageState(0, D3DTSS_COLOROP  , D3DTOP_SELECTARG1);
		D3DDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);

		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAOP  , D3DTOP_MODULATE);
		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
		D3DDev->SetRenderState(D3DRS_TEXTUREFACTOR, 0x50606060);

		D3DDev->SetTransform(D3DTS_WORLD, &matWorld);
		for ( DWORD i = 0 ; i < m_nummaterials ; i++ ){
			D3DDev->SetMaterial(&m_Materials[i]);
			if ( m_Textures && m_Textures[i] ){
				D3DDev->SetTexture(0, m_Textures[i]);
			}
			m_mesh->DrawSubset(i);
		}

		D3DDev->SetRenderState(D3DRS_LIGHTING, FALSE);
	}
};

class DxCacheDIBClass
{
private:
	ScreenInfo *m_sinfo;
	IDirect3DTexture9 *m_Tex;
	DWORD m_CacheInfo;
	BITMAPINFOHEADER m_bmiHeader;
public:
	DxCacheDIBClass()
	{
		m_Tex = NULL;
		Free();
	}

	~DxCacheDIBClass()
	{
		Free();
	}

	void Free(void)
	{
		SAFE_RELEASE(m_Tex);
		m_CacheInfo = 0;
		m_bmiHeader.biWidth = 0xffffffff;
	}

	BOOL Init(ScreenInfo *sinfo)
	{
		m_CacheInfo = 0;
		m_sinfo = sinfo;
		return TRUE;
	}

	BOOL Check(DWORD *CacheID)
	{
		if ( (*CacheID == 0) || (m_CacheInfo != *CacheID) ){
			*CacheID = ++m_CacheInfo;
			return FALSE;
		}
		return TRUE;
	}

	void Draw(BITMAPINFOHEADER *bmiHeader, LPVOID lpBits, const RECT *box)
	{
		D3DLOCKED_RECT LockedRect;

		if ( (bmiHeader != NULL) &&
			((m_bmiHeader.biWidth != bmiHeader->biWidth) ||
			 (m_bmiHeader.biHeight != bmiHeader->biHeight)) ){
			Free();
			m_bmiHeader = *bmiHeader;

			m_sinfo->D3DDev->CreateTexture(
					m_bmiHeader.biWidth, m_bmiHeader.biHeight, 1,
					D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
					&m_Tex, NULL);
		}
		if ( m_Tex == NULL ) return;

		if ( lpBits != NULL ){
			if( SUCCEEDED(m_Tex->LockRect(0, &LockedRect, NULL, D3DLOCK_DISCARD)) ){
				DWORD blockW = m_bmiHeader.biWidth;

				for ( int y = m_bmiHeader.biHeight - 1 ; y >= 0 ; y-- ){
					const DWORD *src = (DWORD *)(BYTE *)lpBits + (m_bmiHeader.biHeight - 1 - y) * blockW;
					DWORD *dest = (DWORD *)(BYTE *)((BYTE *)LockedRect.pBits + y * LockedRect.Pitch);
					memcpy(dest, src, m_bmiHeader.biWidth * sizeof(DWORD));
				}
				m_Tex->UnlockRect(0);
			}
		}
		::DrawTex(m_sinfo, m_Tex, box->left, box->top, box->right, box->bottom);
	}
};

extern "C" typedef struct tagDXDRAWSTRUCT {
	ScreenInfo sinfo;
	D3DDeviceClass d3d;
	DxTextClass dxText; // 文字表示関連
	DxCellCursorClass dxCursor;
	DxCacheDIBClass dxCacheDIB;

	DxMeshClass dxMesh; // ppc.x 表示用

	// これらは解放が必要
	IDirect3DTexture9 *SubGround; // 裏画面
	IDirect3DSurface9 *SubDepthStencilSurf; // 裏画面
	IDirect3DSurface9 *BackupSurf; // 裏画面
	IDirect3DSurface9 *BackupDepthStencilSurf; // 裏画面
	float MM_r;
	DWORD MM_c;

	// ここまで

	BOOL CaptureMode;
	int MotionMode;
	DWORD MotionTick;
} DXDRAWSTRUCT;

void DrawTex(ScreenInfo *sinfo, IDirect3DTexture9 *Tex, int left, int top, int width, int height)
{
	IDirect3DDevice9 *D3DDev = sinfo->D3DDev;

	D3DDev->SetFVF(FVF_DEFFVF);
	D3DDev->SetTexture(0, Tex);
	D3DDev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	D3DDev->SetStreamSource(0, sinfo->PanelVertex, 0, sizeof(VEX_DEFFVF) );

	D3DDev->SetTextureStageState(0, D3DTSS_COLOROP  , D3DTOP_SELECTARG1);
	D3DDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

	D3DDev->SetTextureStageState(0, D3DTSS_ALPHAOP  , D3DTOP_SELECTARG1);
	D3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
	D3DDev->SetRenderState(D3DRS_TEXTUREFACTOR, 0xffc0c0c0);

	D3DMATRIX mat = IdentityMatrix;
	SetMatrix2D_ST(mat,
			(float)left * D3D_ProjScaleX,	(float)top * D3D_ProjScaleY,
			(float)width * D3D_ProjScaleX,	(float)height * D3D_ProjScaleY
	);
	D3DDev->SetTransform(D3DTS_WORLD, &mat);

	D3DDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
}

BOOL DrawDIB(ScreenInfo *sinfo, BITMAPINFOHEADER *bmiHeader, LPVOID lpBits, const RECT *box)
{
	IDirect3DTexture9 *Tex;
	D3DLOCKED_RECT LockedRect;

	if ( FAILED(sinfo->D3DDev->CreateTexture(
			bmiHeader->biWidth, bmiHeader->biHeight, 1,
			D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &Tex, NULL)) ){
		return FALSE;
	}

	if ( SUCCEEDED(Tex->LockRect(0, &LockedRect, NULL, D3DLOCK_DISCARD)) ){
		switch (bmiHeader->biBitCount){
			case 1: {
				DWORD *palette;
				DWORD blockW = bmiHeader->biWidth;

				palette = (COLORREF *)(BYTE *)((BYTE *)bmiHeader + bmiHeader->biSize);
				for ( int y = bmiHeader->biHeight - 1 ; y >= 0 ; y-- ){
					const BYTE *src = (BYTE *)lpBits + (bmiHeader->biHeight - 1 - y) * DwordAlignment((blockW + 7) / 8);
					DWORD *dest = (DWORD *)(BYTE *)((BYTE *)LockedRect.pBits + y * LockedRect.Pitch);
					int w;
					BYTE pixels;

					for ( w = blockW ; w >= 8 ; w -= 8 ){

						pixels = *src++;
						for ( int b = 0 ; b < 8 ; b++ ){
							*dest++ = palette[pixels >> 7] | 0xff000000;
							pixels <<= 1;
						}
					}
					pixels = *src;
					for ( ; w > 0 ; w-- ){
						*dest++ = palette[pixels >> 7] | 0xff000000;
						pixels <<= 1;
					}
				}
				break;
			}

			case 4: {
				DWORD *palette;
				DWORD blockW = bmiHeader->biWidth;

				palette = (COLORREF *)(BYTE *)((BYTE *)bmiHeader + bmiHeader->biSize);
				for ( int y = bmiHeader->biHeight - 1 ; y >= 0 ; y-- ){
					const BYTE *src = (BYTE *)lpBits + (bmiHeader->biHeight - 1 - y) * DwordAlignment((blockW + 1) / 2);
					DWORD *dest = (DWORD *)(BYTE *)((BYTE *)LockedRect.pBits + y * LockedRect.Pitch);
					int w;
					for ( w = blockW ; w >= 2 ; w -= 2 ){
						BYTE pixels;

						pixels = *src++;
						*dest++ = palette[pixels >> 4] | 0xff000000;
						*dest++ = palette[pixels & 0xf] | 0xff000000;
					}
					if ( w > 1 ){
						*dest++ = palette[*src >> 4] | 0xff000000;
					}
				}
				break;
			}

			case 8: {
				DWORD *palette;
				DWORD blockW = bmiHeader->biWidth;

				palette = (COLORREF *)(BYTE *)((BYTE *)bmiHeader + bmiHeader->biSize);
				for ( int y = bmiHeader->biHeight - 1 ; y >= 0 ; y-- ){
					const BYTE *src = (BYTE *)lpBits + (bmiHeader->biHeight - 1 - y) * DwordAlignment(blockW);
					DWORD *dest = (DWORD *)(BYTE *)((BYTE *)LockedRect.pBits + y * LockedRect.Pitch);
					for ( int w = blockW ; w > 0 ; w-- ){
						*dest++ = palette[*src++] | 0xff000000;
					}
				}
				break;
			}

			case 24: {
				DWORD blockW = bmiHeader->biWidth;

				for ( int y = bmiHeader->biHeight - 1 ; y >= 0 ; y-- ){
					const BYTE *src = (BYTE *)lpBits + (bmiHeader->biHeight - 1 - y) * DwordAlignment(blockW * 3);
					DWORD *dest = (DWORD *)(BYTE *)((BYTE *)LockedRect.pBits + y * LockedRect.Pitch);
					for ( int w = blockW ; w > 0 ; w-- ){
						*dest++ = *(DWORD *)src;
						src += 3;
					}
				}
				break;
			}

			case 32: {
				DWORD blockW = bmiHeader->biWidth;

				for ( int y = bmiHeader->biHeight - 1 ; y >= 0 ; y-- ){
					const DWORD *src = (DWORD *)(BYTE *)lpBits + (bmiHeader->biHeight - 1 - y) * blockW;
					DWORD *dest = (DWORD *)(BYTE *)((BYTE *)LockedRect.pBits + y * LockedRect.Pitch);
					memcpy(dest, src, blockW * sizeof(DWORD));
				}
				break;
			}

			default:
//				Messagef("Bad format %d", bmiHeader->biBitCount);
				break;
		}
		Tex->UnlockRect(0);
		DrawTex(sinfo, Tex, box->left, box->top, box->right, box->bottom);
	}
	Tex->Release();
	return TRUE;
}

void EndMotion(DXDRAWSTRUCT *DxDraw)
{
	SAFE_RELEASE(DxDraw->SubDepthStencilSurf);
	SAFE_RELEASE(DxDraw->SubGround);
	DxDraw->MotionMode = 0;
}

void USEFASTCALL FreeDxObject(DXDRAWSTRUCT *DxDraw)
{
	int MotionMode = DxDraw->MotionMode;

	DxDraw->dxCursor.Free();
	DxDraw->dxText.Free();
	DxDraw->dxCacheDIB.Free();

	EndMotion(DxDraw);
	if ( MotionMode ){ // Motion中なら、続行できるようにする
		DxDraw->MotionMode = MotionMode;
		DxDraw->CaptureMode = TRUE;
	}
}

BOOL CreateDxDraw(DXDRAWSTRUCT **DxDrawPtr, HWND hWnd)
{
	DXDRAWSTRUCT *DxDraw;
	TCHAR path[MAX_PATH];

	*DxDrawPtr = DxDraw = new DXDRAWSTRUCT;
	DxDraw->sinfo.hWnd = hWnd;

	if ( DxDraw->d3d.Init(&DxDraw->sinfo) == FALSE ){
	//	Message(g_DebugMes);
		delete DxDraw;
		*DxDrawPtr = NULL;
		return FALSE;
	}
	DxDraw->dxText.Init(&DxDraw->sinfo);
	DxDraw->dxCursor.Init(&DxDraw->sinfo);
	DxDraw->dxCacheDIB.Init(&DxDraw->sinfo);

	GetModuleFileName(NULL, path, MAX_PATH);
	tstrcpy(tstrrchr(path, '\\'), T("\\ppc.x"));
	DxDraw->dxMesh.Init(&DxDraw->sinfo, path);

	DxDraw->SubGround = NULL;
	DxDraw->SubDepthStencilSurf = NULL;

	DxDraw->CaptureMode = FALSE;
	DxDraw->MotionMode = 0;
	return TRUE;
}

BOOL CloseDxDraw(DXDRAWSTRUCT **DxDrawPtr)
{
	if ( (*DxDrawPtr) == NULL ) return FALSE;

	FreeDxObject(*DxDrawPtr);

	delete (*DxDrawPtr);
	*DxDrawPtr = NULL;
	return TRUE;
}

int BeginDrawDxDraw(DXDRAWSTRUCT *DxDraw, PAINTSTRUCT *ps)
{
	int result;

	if ( DxDraw == NULL ) return DXSTART_GDI;
	if ( GetKeyState(VK_SCROLL) & 1 ){
		BeginPaint(DxDraw->sinfo.hWnd, ps);
		return DXSTART_GDI;
	}

	result = DxDraw->d3d.BeginRender();
	if ( result == FALSE ){
		FreeDxObject(DxDraw);

		DxDraw->d3d.ResetDevice();
		DxDraw->d3d.SetBackColor(DxDraw->sinfo.backgroundcolor);
		result = DxDraw->d3d.BeginRender();
	}
	if ( result == FALSE ){
		ValidateRect(DxDraw->sinfo.hWnd, NULL);
		return DXSTART_NODRAW;
	}

	if ( IsTrue(DxDraw->CaptureMode) ){
		if ( DxDraw->SubGround == NULL ){
			if ( FAILED(DxDraw->sinfo.D3DDev->CreateTexture(
					DxDraw->sinfo.ClientWidthInt, DxDraw->sinfo.ClientHeightInt,
					1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
					&DxDraw->SubGround, NULL)) ){
				DxDraw->SubGround = NULL;
			}else{
				DxDraw->sinfo.D3DDev->CreateDepthStencilSurface(
						DxDraw->sinfo.ClientWidthInt,
						DxDraw->sinfo.ClientHeightInt,
						D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, FALSE,
						&DxDraw->SubDepthStencilSurf, NULL);
			}
		}
		if ( DxDraw->SubGround != NULL ){
			// ↓AddRefされることに注意
			DxDraw->sinfo.D3DDev->GetRenderTarget(0, &DxDraw->BackupSurf);
			DxDraw->sinfo.D3DDev->GetDepthStencilSurface(&DxDraw->BackupDepthStencilSurf);

			IDirect3DSurface9 *pSurf = NULL;
			DxDraw->SubGround->GetSurfaceLevel(0, &pSurf);
			DxDraw->sinfo.D3DDev->SetRenderTarget(0, pSurf);
			DxDraw->sinfo.D3DDev->SetDepthStencilSurface(DxDraw->SubDepthStencilSurf);
			pSurf->Release();
			DxDraw->sinfo.D3DDev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER /*| D3DCLEAR_STENCIL*/, 0x00FFFFFF, 1.0f, 0);

		}
	}
	ps->hdc = static_cast<HDC>(DXMODEVALUE_DX);
	ps->fErase = FALSE;
	ps->rcPaint.left = ps->rcPaint.top = 0;
	ps->rcPaint.right  = DxDraw->sinfo.ClientWidthInt;
	ps->rcPaint.bottom = DxDraw->sinfo.ClientHeightInt;

	DxExtDraw(DxDraw, &ps->rcPaint);
	return DXSTART_DX;
}


BOOL EndDrawDxDraw(DXDRAWSTRUCT *DxDraw)
{
	RECT box = {0, 0, 800, 80};

	if ( DxDraw == NULL ) return FALSE;
	ValidateRect(DxDraw->sinfo.hWnd, NULL);

	if ( DxDraw->SubGround != NULL ){
		if ( IsTrue(DxDraw->CaptureMode) ){
			DxDraw->sinfo.D3DDev->SetRenderTarget(0, DxDraw->BackupSurf);
			DxDraw->BackupSurf->Release();
			DxDraw->sinfo.D3DDev->SetDepthStencilSurface(DxDraw->BackupDepthStencilSurf);
			DxDraw->BackupDepthStencilSurf->Release();
		}
		IDirect3DDevice9 *D3DDev = DxDraw->sinfo.D3DDev;

		D3DDev->SetFVF(FVF_DEFFVF);
		D3DDev->SetTexture(0, DxDraw->SubGround);
		D3DDev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		D3DDev->SetStreamSource(0, DxDraw->sinfo.PanelVertex, 0, sizeof(VEX_DEFFVF) );
		D3DDev->SetTextureStageState(0, D3DTSS_COLOROP  , D3DTOP_SELECTARG1);
		D3DDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAOP  , D3DTOP_SELECTARG1);
		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		D3DXMATRIXA16 mat = IdentityMatrix;

		SetMatrix2D_ST(mat,
				0.0f , 0.0f , //0 * D3D_ProjScaleX,	(float)0 * D3D_ProjScaleY,
				DxDraw->sinfo.ClientWidth * D3D_ProjScaleX,	DxDraw->sinfo.ClientHeight * D3D_ProjScaleY
		);

		if ( IsTrue(DxDraw->CaptureMode) ){
			D3DDev->SetTransform(D3DTS_WORLD, &mat);
			D3DDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
			DxDraw->CaptureMode = FALSE;
		}

		// 調整
		D3DXMATRIXA16 matV = IdentityMatrix;

		switch ( DxDraw->MotionMode ){
			case DXMOTION_Launch:
				SetMatrix2D_ST(matV, -DxDraw->MM_r / 10.0f, DxDraw->MM_r / 10.0f, DxDraw->MM_r + 1.0f, DxDraw->MM_r + 1.0f);
				break;

			case DXMOTION_Root: {
				float mag = 1.0f - (DxDraw->MM_r / (D3D_ProjScaleX * DxDraw->sinfo.ClientWidth)) * 0.4f;
				SetMatrix2D_ST(matV, DxDraw->MM_r, 0.0f, mag, mag);
				break;
			}

			case DXMOTION_NewWindow:
			case DXMOTION_ChangePath:

			case DXMOTION_ChangeDrive:
				SetMatrix2D_ST(matV, DxDraw->MM_r, 0.0f, 1.0f, 1.0f);
				break;

			case DXMOTION_UpDir:
			case DXMOTION_DownDir:
				SetMatrix2D_ST(matV, 0.0f, DxDraw->MM_r, 1.0f, 1.0f);
				break;

			case DXMOTION_Busy:
				D3DXMatrixRotationYawPitchRoll(&matV, 0.0f + DxDraw->MM_r, 0.0f - 0.5f, 0.0f);
				break;
		}

		D3DXMatrixMultiply(&mat, &mat, &matV);

		// 斜視投影で描画
		D3DDev->SetTransform(D3DTS_VIEW, &DxDraw->d3d.MatView3D);
		D3DDev->SetTransform(D3DTS_WORLD, &mat);
		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAOP  , D3DTOP_MODULATE);
		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
		D3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
		D3DDev->SetRenderState(D3DRS_TEXTUREFACTOR, DxDraw->MM_c);
		D3DDev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		D3DDev->SetTransform(D3DTS_VIEW, &DxDraw->d3d.MatView2D);

		#define MM_STEP 10
		#define MM_WAIT 20
		if ( DxDraw->MotionMode ){
			DWORD tick = GetTickCount(), oldtick;

			oldtick = DxDraw->MotionTick;
			if ( (tick - oldtick) > MM_WAIT ){
				DxDraw->MotionTick = tick;
				tick -= oldtick;
				if ( tick > 1000 ) tick = 1000;
				for ( ; tick <= 1000 ; tick -= MM_WAIT ) switch ( DxDraw->MotionMode ){
					case DXMOTION_Busy:
						DxDraw->MM_r += 0.5f;
						break;

					case DXMOTION_NewWindow:
						DxDraw->MM_r -= D3D_ProjScaleX * (DxDraw->sinfo.ClientWidth / MM_STEP);
						DxDraw->MM_c += ((0xff000000 / MM_STEP) & 0xff000000);

						if ( DxDraw->MM_r <= 0.0f ) EndMotion(DxDraw);
						break;

					case DXMOTION_Root:
					case DXMOTION_ChangePath:
					case DXMOTION_Launch:

					case DXMOTION_ChangeDrive:
						DxDraw->MM_r += D3D_ProjScaleX * (DxDraw->sinfo.ClientWidth / MM_STEP);
						DxDraw->MM_c -= ((0xff000000 / MM_STEP) & 0xff000000);

						if ( DxDraw->MM_r > (DxDraw->sinfo.ClientWidth * D3D_ProjScaleX) ){
							EndMotion(DxDraw);
						}
						break;

					case DXMOTION_UpDir:
						DxDraw->MM_r += D3D_ProjScaleY * (DxDraw->sinfo.ClientHeight / MM_STEP);
						DxDraw->MM_c -= ((0xff000000 / MM_STEP) & 0xff000000);

						if ( DxDraw->MM_r < (DxDraw->sinfo.ClientHeight * D3D_ProjScaleY) ){
							EndMotion(DxDraw);
						}
						break;

					case DXMOTION_DownDir:
						DxDraw->MM_r -= D3D_ProjScaleY * (DxDraw->sinfo.ClientHeight / MM_STEP);
						DxDraw->MM_c -= ((0xff000000 / MM_STEP) & 0xff000000);

						if ( DxDraw->MM_r >= 0.0f ) EndMotion(DxDraw);
						break;
				}
			}
		}
	}

	if ( g_DebugMes[0] ){
#if USEDXTEXT
		DxDraw->d3d.DrawText(g_DebugMes, &box);
#else
		DxDraw->sinfo.lastpos.x = 0;
		DxDraw->sinfo.lastpos.y = 0;
		DxDraw->dxText.Draw(g_DebugMes, 0, 0, 0x10000, tstrlen(g_DebugMes));
#endif
	}
	DxDraw->d3d.EndRender();
	return TRUE;
}


BOOL ChangeSizeDxDraw(DXDRAWSTRUCT *DxDraw, COLORREF backcolor)
{
	RECT ClientBox;

	if ( DxDraw == NULL ) return FALSE;
	DxDraw->sinfo.backgroundcolor = backcolor;
	GetClientRect(DxDraw->sinfo.hWnd, &ClientBox);
	if ( (ClientBox.right > 0) && (ClientBox.bottom > 0) &&
		((DxDraw->sinfo.ClientWidthInt != ClientBox.right) ||
		(DxDraw->sinfo.ClientHeightInt != ClientBox.bottom))
	){
		DxDraw->sinfo.ClientWidth = static_cast<float>(ClientBox.right);
		DxDraw->sinfo.ClientHeight = static_cast<float>(ClientBox.bottom);
		DxDraw->sinfo.ClientWidthInt = ClientBox.right;
		DxDraw->sinfo.ClientHeightInt = ClientBox.bottom;
		if ( IsTrue(DxDraw->d3d.DeviceCheck()) ){
			FreeDxObject(DxDraw);
//			DxDraw->d3d.RequestReset();
			DxDraw->d3d.ResetDevice();
			DxDraw->d3d.SetBackColor(backcolor);
		}
	}
	return TRUE;
}

DWORD SetFontDxDraw(DXDRAWSTRUCT *DxDraw, HFONT hFont, int type)
{
	LOGFONT lfont;

	if ( DxDraw == NULL ) return 0;
	DxDraw->dxText.SetFont(hFont);

	if ( type == 0 ){
		GetObject(hFont, sizeof(LOGFONT), static_cast<LPVOID>(&lfont));
		DxDraw->sinfo.fontH =
				(lfont.lfHeight >= 0) ? lfont.lfHeight : -lfont.lfHeight;
	}
	return 1;
}

void USEFASTCALL DxMoveToEx(DXDRAWSTRUCT *DxDraw, HDC hDC, int x, int y)
{
	IfGDImode(hDC){
		MoveToEx(hDC, x, y, NULL);
		return;
	}
	DxDraw->sinfo.lastpos.x = x;
	DxDraw->sinfo.lastpos.y = y;
}

void USEFASTCALL DxGetCurrentPositionEx(DXDRAWSTRUCT *DxDraw, HDC hDC, POINT *lp)
{
	IfGDImode(hDC){
		GetCurrentPositionEx(hDC, lp);
		return;
	}
	*lp = DxDraw->sinfo.lastpos;
}

void DxTextOutRel(DXDRAWSTRUCT *DxDraw, HDC hDC, const TCHAR *str, int len)
{
	IfGDImode(hDC){
		TextOut(hDC, 0, 0, str, len);
		return;
	}
	DxDraw->sinfo.lastpos.x = DxDraw->dxText.Draw(str, DxDraw->sinfo.lastpos.x, DxDraw->sinfo.lastpos.y, 0xffffff, len);
}

void DxTextOutBack(DXDRAWSTRUCT *DxDraw, HDC hDC, const TCHAR *str, int len)
{
	IfGDImode(hDC){
		TextOut(hDC, 0, 0, str, len);
		return;
	}
	int width = DxDraw->dxText.Calc(str, 0x70ffffff, len);
	DxDraw->dxCursor.DrawBack(DxDraw->sinfo.lastpos.x, DxDraw->sinfo.lastpos.y,
			width, DxDraw->sinfo.fontH, DxDraw->sinfo.TextBackColor);
	DxDraw->sinfo.lastpos.x = DxDraw->dxText.Draw(str, DxDraw->sinfo.lastpos.x, DxDraw->sinfo.lastpos.y, 0x70ffffff, len);
}

void DxGetTextExtentExPoint(DXDRAWSTRUCT *DxDraw, HDC hDC, const TCHAR *str, int len, int maxwidth, int *fitlem, int *alptbl, SIZE *blksize)
{
	IfGDImode(hDC){
		GetTextExtentExPoint(hDC, str, len, maxwidth, fitlem, alptbl, blksize);
		return;
	}
	*fitlem = len;
	blksize->cx = DxDraw->dxText.Calc(str, maxwidth, *fitlem);
}

void DxDrawCursor(DXDRAWSTRUCT *DxDraw, HDC hDC, const RECT *box, COLORREF color)
{
	IfGDImode(hDC) return;
	DxDraw->dxCursor.Draw(box->left, box->top, box->right - box->left, box->bottom - box->top, color);
}

void DxDrawBack(DXDRAWSTRUCT *DxDraw, HDC hDC, const RECT *box, COLORREF color)
{
	IfGDImode(hDC) return;
	DxDraw->dxCursor.DrawBack(box->left, box->top, box->right - box->left, box->bottom - box->top, color);
}

BOOL DxDrawAtlas_Check(DXDRAWSTRUCT *DxDraw, DWORD *CacheID)
{
	return DxDraw->dxCacheDIB.Check(CacheID);
}

void DxDrawAtlas(DXDRAWSTRUCT *DxDraw, BITMAPINFOHEADER *bmiHeader, LPVOID lpBits, const RECT *box)
{
	DxDraw->dxCacheDIB.Draw(bmiHeader, lpBits, box);
}

BOOL DxDrawDIB(DXDRAWSTRUCT *DxDraw, BITMAPINFOHEADER *bmiHeader, LPVOID lpBits, const RECT *box, const RECT *, DXBMPCACHE **)
{
	return DrawDIB(&DxDraw->sinfo, bmiHeader, lpBits, box);
}

void DxDrawIcon(DXDRAWSTRUCT *DxDraw, HICON icon, const RECT *box, DXBMPCACHE **)
{
	if ( DxDraw == NULL ) return;
	DxDraw->d3d.DrawIcon(icon, box);
}

void DxDrawFreeBMPCache(DXBMPCACHE **)
{
}

void USEFASTCALL DxDrawText(DXDRAWSTRUCT *DxDraw, HDC hDC, const TCHAR *str, int len, const RECT *box, DWORD flags)
{
	IfGDImode(hDC){
		DrawText(hDC, str, len, (LPRECT)box, flags);
		return;
	}
	if ( len < 0 ) len = tstrlen(str);
	DxDraw->sinfo.lastpos.x = DxDraw->dxText.Draw(str, box->left, box->top, box->right, len);
}

COLORREF DxSetTextColor(DXDRAWSTRUCT *DxDraw, HDC hDC, COLORREF color)
{
	COLORREF oldcolor;

	IfGDImode(hDC) return SetTextColor(hDC, color);

	DxDraw->dxText.SetTextColor(color);
	oldcolor = DxDraw->sinfo.TextColor;
	DxDraw->sinfo.TextColor = color;
	return oldcolor;
}

COLORREF USEFASTCALL DxGetTextColor(DXDRAWSTRUCT *DxDraw, HDC hDC)
{
	IfGDImode(hDC) return GetTextColor(hDC);

	return DxDraw->sinfo.TextColor;
}

COLORREF DxSetBkColor(DXDRAWSTRUCT *DxDraw, HDC hDC, COLORREF color)
{
	COLORREF oldcolor;

	IfGDImode(hDC) return SetBkColor(hDC, color);

	oldcolor = DxDraw->sinfo.TextBackColor;
	DxDraw->sinfo.TextBackColor = color;
	return oldcolor;
}

COLORREF USEFASTCALL DxGetBkColor(DXDRAWSTRUCT *DxDraw, HDC hDC)
{
	IfGDImode(hDC) return GetBkColor(hDC);

	return DxDraw->sinfo.TextBackColor;
}

void USEFASTCALL DxSetBenchmarkMode(DXDRAWSTRUCT *DxDraw, BOOL mode)
{
	if ( DxDraw == NULL ) return;
	if ( IsTrue(DxDraw->d3d.SetBenchmarkMode(mode)) ){
		FreeDxObject(DxDraw);
		DxDraw->d3d.ResetDevice();
	}
}

// おまけ表示
void USEFASTCALL DxExtDraw(DXDRAWSTRUCT *DxDraw, RECT *client)
{
	DxDraw->dxMesh.Draw(
			(static_cast<float>(client->right) - 23.0f) * D3D_ProjScaleX,
			(static_cast<float>(client->top) + 28.0f) * D3D_ProjScaleY,
			ZPOS_MESH,  0.0f, 0.0f, 0.0f);
//	DxDraw->dxText.DrawTest();
}

void DxSetMotion(DXDRAWSTRUCT *DxDraw, int mode)
{
	if ( DxDraw == NULL ) return;
	switch ( mode ){
		case DXMOTION_NewWindow:
			DxDraw->MM_r = DxDraw->sinfo.ClientWidth * D3D_ProjScaleX * (MM_STEP - 0.9f) / MM_STEP;
			DxDraw->MM_c = 0;
			break;

		case DXMOTION_DownDir:
			DxDraw->MM_r = DxDraw->sinfo.ClientHeight * D3D_ProjScaleY * 0.91f;
			DxDraw->MM_c = 0xffffffff;
			break;

		case DXMOTION_StopBusy:
			if ( DxDraw->MotionMode != DXMOTION_Busy ) return;
			EndMotion(DxDraw);
			return;

		case DXMOTION_Busy:
			if ( DxDraw->MotionMode == DXMOTION_Busy ) return;

		case DXMOTION_UpDir:
		case DXMOTION_Root:
		case DXMOTION_ChangeDrive:
		case DXMOTION_ChangePath:
		case DXMOTION_Launch:
			DxDraw->MM_r = 0.0f;
			DxDraw->MM_c = 0xffffffff;
			break;
	}

	EndMotion(DxDraw);
	// ※ FreeDxObject にも記載あり
	DxDraw->MotionMode = mode;
	DxDraw->CaptureMode = TRUE;
	DxDraw->MotionTick = GetTickCount();

	InvalidateRect(DxDraw->sinfo.hWnd, NULL, TRUE);
	UpdateWindow(DxDraw->sinfo.hWnd);
}

int DxSetBkMode(DXDRAWSTRUCT *DxDraw, HDC hDC, int mode)
{
	IfGDImode(hDC) return SetBkMode(hDC, mode);

	int oldmode = DxDraw->sinfo.BkMode;
	DxDraw->sinfo.BkMode = mode;
	return mode;
}

void ResetDxDraw(DXDRAWSTRUCT *DxDraw)
{
	if ( DxDraw == NULL ) return;
	FreeDxObject(DxDraw);
	DxDraw->d3d.ResetDevice();
}
#ifndef UNICODE
void DxTextOutRelW(DXDRAWSTRUCT *DxDraw, HDC hDC, const WCHAR *str, int len)
{
	char text[0x1000];

	IfGDImode(hDC){
		TextOutW(hDC, 0, 0, str, len);
		return;
	}

	int lenA = WideCharToMultiByte(CP_ACP, 0, str, len, text, 0x1000, NULL, NULL);
	DxTextOutBack(DxDraw, hDC, text, lenA);
}
#endif
#endif // DRAWMODE_3D
#endif // USEDIRECTX
