/*-----------------------------------------------------------------------------
	Paper Plane vUI												ä÷êîíËã`
-----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------ ppvui.c
extern LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern BOOL CheckParam(VIEWOPTIONS *viewopts, const TCHAR *param, TCHAR *name);
extern void MoveCsrkey(int offx, int offy, BOOL select);
extern void ExecDualParam(PPV_APPINFO *vinfo, const TCHAR *param);
extern void WmWindowPosChanged(HWND hWnd);
extern void SetCursorCaret(SELPOSINFO *posinfo);
extern int CalcTextXPoint(int ParamX, int csrY, int istate);

//------------------------------------------------------------------ ppv_clip.c
extern BOOL ClipMem(TMS_struct *text, int StartLine, int EndLine);
extern void ClipText(HWND hWnd);
extern void MakeURIs(HMENU hMenu, int index);
extern int CalcHexX(int off);
extern BOOL OpenClipboardV(HWND hWnd);

//------------------------------------------------------------------ ppv_com.c
extern ERRORCODE PPXAPI PPvCommand(PPV_APPINFO *vinfo, WORD key);

//------------------------------------------------------------------ ppv_cp.c
extern DWORD GetCodePageType(HWND hWnd);

//------------------------------------------------------------------ ppv_doas.c
extern BYTE *VD_oasys_mdt(MAKETEXTINFO *mti, VT_TABLE *tbl);

//------------------------------------------------------------------ ppv_img.c
extern void CreateDIBtoPalette(PPvViewObject *vo);
extern void ClipBitmap(HWND hWnd);
extern void Rotate(HWND hWnd, int d);
extern void ModifyAlpha(HWND hWnd);

//------------------------------------------------------------------ ppv_init.c
extern BOOL InitializePPv(int *result);
extern void PPvLoadCust(void);
extern void PPvSaveCust(void);
extern void DeleteFonts(void);
extern void MakeFonts(HDC hDC, int mag);
extern void InitViewOptions(VIEWOPTIONS *viewopts);
extern void MakeUnfixedFont(void);
extern void InitGui(void);
extern void ClosePPv(void);
extern void InitSymbolFont(HDC hDC);
extern HFONT GetIBMFont(void);
extern HFONT GetANSIFont(void);

//------------------------------------------------------------------ ppv_open.c
extern void InitViewObject(VIEWOPTIONS *viewopt, TCHAR *type);
extern BOOL OpenViewObject(const TCHAR *filename, HGLOBAL mem, VIEWOPTIONS *viewopt, int flags);
extern void OpenAndFollowViewObject(PPV_APPINFO *vinfo, const TCHAR *filename, HGLOBAL mem, VIEWOPTIONS *viewopt, int flags);
extern void FollowOpenView(PPV_APPINFO *vinfo);
extern void ChangePage(int delta);
extern void ReMakeIndexes(HWND hWnd);
extern void CALLBACK BackReaderProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
extern void SetOpts(VIEWOPTIONS *viewopts);
extern void GetMemo(void);
extern void InitRawImageRectSize(void);
extern void SetHighlight(PPV_APPINFO *vinfo);

//------------------------------------------------------------------ ppv_pain.c
extern void Paint(HWND hWnd);
extern void DrawHex(PPVPAINTSTRUCT *pps, PPvViewObject *vo);

//------------------------------------------------------------------ ppv_pait.c
extern void DrawSelectedTextA(HDC hDC, SELINFO *si, char *text, int length, int charX, int offsetY);
extern void DrawSelectedTextW(HDC hDC, SELINFO *si, WCHAR *text, int length, int charX, int offsetY);

extern void TextFixOut(HDC hDC, char *str, int len);
#ifdef UNICODE
#define DrawSelectedText DrawSelectedTextW
#else
#define DrawSelectedText DrawSelectedTextA
#endif
extern void PaintText(PPVPAINTSTRUCT *pps, PPvViewObject *vo);

//------------------------------------------------------------------ ppv_prin.c
extern BOOL PPVPrint(HWND hWnd);
extern BOOL PPVPrintFiles(HWND hWnd, HDROP hDrop);

//------------------------------------------------------------------ ppv_sub.c
extern void CloseViewObject(void);
extern void FixWindowSize(HWND hWnd, int offx, int offy);
extern void USEFASTCALL MoveCsr(int x, int y, BOOL detailmode);
extern ERRORCODE PPV_DisplayType(HWND hWnd, BOOL extend);
extern ERRORCODE PPV_TextCode(HWND hWnd, int defaultindex);
extern void SetPopMsg(ERRORCODE err, const TCHAR *msg);
extern void StopPopMsg(int mask);
extern void SetScrollBar(void);
extern void SkipSepA(char **p);

extern DWORD_PTR USECDECL PPxGetIInfo(PPV_APPINFO *ppc, DWORD cmdID, PPXAPPINFOUNION *uptr);

extern void SetMag(int offset);
extern BOOL PPvMouseCommand(PPV_APPINFO *vinfo, POINT *pos, const TCHAR *click, const TCHAR *type);
extern LRESULT PPvNCMouseCommand(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern BOOL PPvMouseCommandPos(PPV_APPINFO *vinfo, POINT *pos, const TCHAR *click, int Y);
extern BOOL FindInputBox(HWND hWnd, const TCHAR *title);

extern void InputAspectRate(HWND hWnd);
extern void PPvAddDisplayTypeMenu(HMENU hPopupMenu, BOOL extend);
extern void PPvMenu(HWND hWnd, WORD key);
extern void PPvContextMenu(PPV_APPINFO *vinfo);
extern void PPvEditFile(HWND hWnd);
extern ERRORCODE PPvShell(HWND hWnd);
extern ERRORCODE PPvExecute(HWND hWnd);
extern void PPvReceiveRequest(HWND hWnd);
extern void PPvReload(PPV_APPINFO *vinfo);
extern BOOL PPvSave(HWND hWnd);
extern void ConvertMain(void);
extern void DoFind(HWND hWnd, int mode);
extern void JumpLine(const TCHAR *linestr);
extern void ResetSelect(BOOL csrOff);
extern void FixSelectRange(void);
extern void InitCursorMode(HWND hWnd, BOOL initpos);
extern int PPvTrackPopupMenu(HMENU hMenu);
extern void GetPopupPosition(POINT *pos);
extern void PPvMinimize(HWND hWnd);
extern char *SelColA(char *str, int col);
extern void DumpEmf(void);
extern void PPvLayoutCommand(void);
extern void ReverseBackground(HWND hWnd);
extern void ViewfileContextMenu(HWND hWnd);
extern void DivChange(int offset);
extern ERRORCODE ChangeWidth(HWND hWnd);
extern void LoadWinpos(HWND hWnd);
extern void FixChangeMode(HWND hWnd);
extern void ReadSizeChange(PPV_APPINFO *vinfo);
extern int USEFASTCALL FixedWidthRange(int cols);
extern void SaveBookmark(int index);
extern BOOL GotoBookmark(PPV_APPINFO *vinfo, int index);
extern ERRORCODE SaveBookmarkMenu(PPV_APPINFO *vinfo);
extern ERRORCODE GotoBookmarkMenu(PPV_APPINFO *vinfo);
extern void WINAPI DummyNotifyWinEvent(DWORD event, HWND hwnd, LONG idObject, LONG idChild);
extern int LogicalLineToDisplayLine(DWORD line);
extern void FixBitmapShowSize(PPvViewObject *vo);
extern void PPvEnterTabletMode(HWND hWnd);
extern void GetDivMax(DDWORDST *divptr);

//---------------------------------------------------------------- ppv_subp.cpp
extern LRESULT WmGetObject(WPARAM wParam);
extern void FreeAccServer(void);

//------------------------------------------------------------------ ppv_text.c
extern BYTE *MakeDispText(MAKETEXTINFO *mti, VT_TABLE *tbl);

#define MIT_FIRST  0
#define MIT_NEXT   1
#define MIT_REMAKE 2

#define MIT_PARAM_TEXT 0
#define MIT_PARAM_DOCUMENT 1
extern int MakeIndexTable(int mode, int param);
extern void VO_error(ERRORCODE num);

//------------------------------------------------------------------ ppv_pste.c
extern HGLOBAL PasteMain(UINT type, VIEWOPTIONS *viewopts);
extern UINT GetPasteType(HWND hWnd);
extern ERRORCODE PPvPasteType(HWND hWnd);
extern void PPvPaste(HWND hWnd);

#ifndef USEDIRECTX
#define UpdateWindow_Part(hwnd) UpdateWindow(hwnd)
#else
#define UpdateWindow_Part(hwnd)
#endif

#ifdef __cplusplus
}
#endif
