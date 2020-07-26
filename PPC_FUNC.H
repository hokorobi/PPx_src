/*-----------------------------------------------------------------------------
	Paper Plane cUI													関数定義
-----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

// 画面左上を0基準としてcellの表示位置を計算する
#define CalcCellX(cinfo, index) (((index) / cinfo->cel.Area.cy) * cinfo->cel.Size.cx + cinfo->BoxEntries.left)
#define CalcCellY(cinfo, index) (((index) % cinfo->cel.Area.cy) * cinfo->cel.Size.cy + cinfo->BoxEntries.top)

#define CalcFreePosOffX(cinfo) ((cinfo->cellWMin / cinfo->cel.Area.cy) * cinfo->cel.Size.cx - cinfo->BoxEntries.left)
#define CalcFreePosOffY(cinfo) ((cinfo->cellWMin % cinfo->cel.Area.cy) * cinfo->cel.Size.cy - cinfo->BoxEntries.top)

#ifdef UNICODE
	#define GetNT_9xValue(ValueNT, Value9x) ((OSver.dwPlatformId == VER_PLATFORM_WIN32_NT) ? (ValueNT) : Value9x )
#else
	#define GetNT_9xValue(ValueNT, Value9x) ValueNT
#endif


#define CellFileNameIndex(ppcinfo, No) ((ENTRYCELL *)ppcinfo->e.CELLDATA.p)[((ENTRYINDEX *)ppcinfo->e.INDEXDATA.p)[No]].f.cFileName
#define CellFileName(cell) cell->f.cFileName

#define FreeInfoIconCache(cinfo) {DIRECTXDEFINE(DxDrawFreeBMPCache(&cinfo->InfoIcon_Cache);cinfo->InfoIcon_DirtyCache = FALSE);}

#define IsCellPtrMarked(cell) (cell->mark_fw != NO_MARK_ID)
#define IsCEL_Marked(no) (CEL(no).mark_fw != NO_MARK_ID)
//------------------------------------------------------------------ ppcui.c
extern void PPCui(HWND hWnd, const TCHAR *cmdline);
extern void PPCuiWithPath(HWND hWnd, const TCHAR *path);
extern void USEFASTCALL PPCuiWithPathForLock(PPC_APPINFO *cinfo, const TCHAR *path);
extern void SendCallPPc(COPYDATASTRUCT *copydata);
extern BOOL CreatePPcWindow(PPCSTARTPARAM *psp, MAINWINDOWSTRUCT *mws);
extern BOOL FixPPcWindowList(MAINWINDOWSTRUCT *mws, BOOL final);
extern int GetPPcSubID(const TCHAR *idstr, TCHAR *dest);

//------------------------------------------------------------------ ppc_add.c
extern void AutoDDDialog(PPC_APPINFO *cinfo, const TCHAR *src, DWORD droptype);
extern ERRORCODE StartAutoDD(PPC_APPINFO *cinfo, HWND hTargetWnd, const TCHAR *src, DWORD droptype);

//------------------------------------------------------------------ ppc_arch.c
extern ERRORCODE PPC_Unpack(PPC_APPINFO *cinfo, const TCHAR *destpath);
extern ERRORCODE UnpackMenu(PPC_APPINFO *cinfo);

#define UFA_ALL		0	// パス生成+展開
#define UFA_PATH	1	// パス生成のみ
#define UFA_EXTRACT	2	// 展開のみ
extern BOOL PPcUnpackForAction(PPC_APPINFO *cinfo, TCHAR *newpath, int mode);
extern ERRORCODE PPcUnpackSelectedEntry(PPC_APPINFO *cinfo, const TCHAR *destpath, const TCHAR *action);
extern BOOL OnArcPathMode(PPC_APPINFO *cinfo);
extern void OffArcPathMode(PPC_APPINFO *cinfo);

//------------------------------------------------------------------ ppc_ccmp.c
extern ERRORCODE PPcCompare(PPC_APPINFO *cinfo, int mode);
extern BOOL PPcCompareMain(PPC_APPINFO *cinfo, HWND hPairWnd, COMPAREMARKPACKET *cmp);
extern BOOL PPcCompareSend(PPC_APPINFO *cinfo, COMPAREMARKPACKET *cmp, HWND PairHWnd);
extern ERRORCODE CompareMarkEntry(PPC_APPINFO *cinfo, const TCHAR *param);
extern DWORD CompareHashFromClipBoard(PPC_APPINFO *cinfo, int comparemode);

//------------------------------------------------------------------ ppc_cell.c
extern DWORD GetFileHeader(const TCHAR *filename, BYTE *header, DWORD headersize);
extern BOOL MoveCellCursor(PPC_APPINFO *cinfo, CURSORMOVER *cm);
extern BOOL MoveCellCursorR(PPC_APPINFO *cinfo, CURSORMOVER *cm);
extern BOOL CellLook(PPC_APPINFO *cinfo, int IsFileRead);
extern ERRORCODE DirChk(const TCHAR *name, TCHAR *dest);
#define MARK_HIGHLIGHT1	3 // FileMaskDialog 用
#define MARK_HIGHLIGHTOFF	2 // FileMaskDialog 用
#define MARK_CHECK	1
#define MARK_REMOVE	0
#define MARK_REVERSE	-1
extern void CellMark(PPC_APPINFO *cinfo, ENTRYINDEX cell, int markmode);
extern void USEFASTCALL ResetMark(PPC_APPINFO *cinfo, ENTRYCELL *cell);
#define CMOVER_POINT ((const CURSORMOVER *)(DWORD_PTR)1)
extern BOOL MoveCellCsr(PPC_APPINFO *cinfo, ENTRYINDEX offset, const CURSORMOVER *cm);
extern BOOL IsFileDir(PPC_APPINFO *cinfo, const TCHAR *filename, TCHAR *newpath, TCHAR *newjumpname, HMENU hPopupMenu);
extern void GetCellRealFullName(PPC_APPINFO *cinfo, ENTRYCELL *cell, TCHAR *dest);

//------------------------------------------------------------------ ppc_cinf.c
extern ENTRYINDEX USEFASTCALL GetCellIndexFromCellData(PPC_APPINFO *cinfo, ENTRYDATAOFFSET index);
extern ENTRYCELL * USEFASTCALL GetCellData_HS(PPC_APPINFO *cinfo, ENTRYINDEX index);
extern void PPcSyncProperties(PPC_APPINFO *cinfo, const TCHAR *param);
extern int PPcGetSite(PPC_APPINFO *cinfo);
extern int GetStringCommand(const TCHAR **param, const TCHAR *commands);

extern DWORD_PTR USEFASTCALL PPcExecExMenu(PPC_APPINFO *cinfo, EXECEXMENUINFO *execmenu);
extern void InsertEntry(PPC_APPINFO *cinfo, ENTRYINDEX index, const TCHAR *name, WIN32_FIND_DATA *ff);
extern void USEFASTCALL SetExtParam(const TCHAR *src, TCHAR *dest, size_t size);
extern const TCHAR *GetCellSfnName(PPC_APPINFO *cinfo, ENTRYCELL *cell, BOOL uselfn);
extern const TCHAR *GetCellFileName(PPC_APPINFO *cinfo, ENTRYCELL *cell, TCHAR *namebuf);
extern void GetPairVpath(PPC_APPINFO *cinfo, TCHAR *destbuf);


extern void EntryExtData_SetDATA(PPC_APPINFO *cinfo, ENTRYEXTDATASTRUCT *eeds, ENTRYCELL *cell);
extern void EntryExtData_SetString(PPC_APPINFO *cinfo, DWORD CommentID, ENTRYCELL *cell, const TCHAR *comment);
extern BOOL EntryExtData_GetDATA(PPC_APPINFO *cinfo, ENTRYEXTDATASTRUCT *eeds, ENTRYCELL *cell);
extern BYTE *EntryExtData_GetDATAptr(PPC_APPINFO *cinfo, WORD id, ENTRYCELL *cell);

//------------------------------------------------------------------ ppc_com.c
extern ERRORCODE PPXAPI PPcCommand(PPC_APPINFO *cinfo, WORD key);

//------------------------------------------------------------------ ppc_com2.c
extern void PPc_SetTreeFlags(HWND hParentWnd, HWND hTreeWnd);
extern void FixRootEntryCursor(PPC_APPINFO *cinfo);
extern void PPC_AllMark(PPC_APPINFO *cinfo);
extern ERRORCODE PPC_DriveJump(PPC_APPINFO *cinfo, BOOL advanced);
extern ERRORCODE PPC_FindMark(PPC_APPINFO *cinfo, const TCHAR *defmask, int mode);
extern void SetMaskTarget(HWND hDlg, FILEMASKDIALOGSTRUCT *PFS);
extern void MaskTargetMenu(HWND hDlg, HWND hButtonWnd, FILEMASKDIALOGSTRUCT *PFS);
extern void SortMenuCheck(HMENU hMenu, const TCHAR *str, int indexmax, XC_SORT *xc, int descending_sort);
extern void SaveSortSetting(PPC_APPINFO *cinfo, int mode, const TCHAR *path, XC_SORT *xc);
extern HMENU AddPPcSortMenu(PPC_APPINFO *cinfo, HMENU hMenu, ThSTRUCT *PopupTbl, DWORD *mmmode, DWORD *sid, DWORD sortmode, const TCHAR *path);
extern HMENU DirOptionMenu(PPC_APPINFO *cinfo, HMENU hPopupMenu, DWORD *index, ThSTRUCT *TH, DWORD nowmode, const TCHAR *path);
extern void AddPPcCellDisplayMenu(PPC_APPINFO *cinfo, HMENU hMenu, int *rmode, DWORD *rfmtsize, const TCHAR *path);
extern ERRORCODE PPC_PathJump(PPC_APPINFO *cinfo);
extern void PPC_ReverseMark(PPC_APPINFO *cinfo);
extern void PPC_RootDir(PPC_APPINFO *cinfo);
extern ERRORCODE SortKeyCommand(PPC_APPINFO *cinfo, DWORD sortmode, const TCHAR *path);
extern void PPC_Tree(PPC_APPINFO *cinfo, int mode);
extern void PPC_CloseTree(PPC_APPINFO *cinfo);
extern void PPC_UpDir(PPC_APPINFO *cinfo);
extern void PPC_View(PPC_APPINFO *cinfo);

#define PPCHGWIN_PAIR CGETW_NEXTNOPREV
#define PPCHGWIN_NEXT CGETW_NEXT
#define PPCHGWIN_BACK CGETW_BACK

extern void PPcChangeWindow(PPC_APPINFO *cinfo, int direction);
extern ERRORCODE CommentCommand(PPC_APPINFO *cinfo, const TCHAR *param);
extern ERRORCODE WriteComment(PPC_APPINFO *cinfo, TCHAR *cname);
extern void SyncFileInfo(PPC_APPINFO *cinfo);
extern void FileInfo(PPC_APPINFO *cinfo);
extern void JumpDrive(PPC_APPINFO *cinfo, WORD key);
extern void LaunchMenu(PPC_APPINFO *cinfo, WORD key);
extern void CountOrEdit(PPC_APPINFO *cinfo);
extern ERRORCODE ExecuteEntry(PPC_APPINFO *cinfo);
extern ERRORCODE ExecuteCommandline(PPC_APPINFO *cinfo);
extern void SwapWindow(PPC_APPINFO *cinfo);
extern ERRORCODE MaskEntry(PPC_APPINFO *cinfo, int mode, const TCHAR *defmask, const TCHAR *path);
extern void OpenExplorer(PPC_APPINFO *cinfo);
extern void OpenFileWithPPe(PPC_APPINFO *cinfo);
extern void ClipDirectory(PPC_APPINFO *cinfo);
#define CFT_FILE	B0		// Drag & Drop 形式
#define CFT_TEXT	B1		// Text 形式
extern void ClipFiles(PPC_APPINFO *cinfo, DWORD effect, DWORD cliptypes);
extern void SetMag(PPC_APPINFO *cinfo, int offset);
extern BOOL SetPairPath(PPC_APPINFO *cinfo, const TCHAR *path, const TCHAR *entry);
extern ERRORCODE SetCellDisplayFormat(PPC_APPINFO *cinfo, int selectindex, const TCHAR *path);

#define ITEMSETTING_DISP 0
#define ITEMSETTING_MASK 1
#define ITEMSETTING_CMD 2
#define ITEMSETTING_DIROPT 3
#define ITEMSETTING_SORT 7
extern int FindDirSetting(PPC_APPINFO *cinfo, int type, TCHAR *findpath, TCHAR *finditem);
extern int FindSortSetting(PPC_APPINFO *cinfo, TCHAR *findpath);
extern void SetNewXdir(const TCHAR *path, const TCHAR *header, const TCHAR *param);

extern void PPcShellExecute(PPC_APPINFO *cinfo);
extern BOOL MaskEntryMain(PPC_APPINFO *cinfo, XC_MASK *maskorg, const TCHAR *filename);
extern void PPC_SortMain(PPC_APPINFO *cinfo, XC_SORT *xc);
extern void ViewOnCursor(PPC_APPINFO *cinfo, int flags);
extern ERRORCODE LogDisk(PPC_APPINFO *cinfo);
extern ERRORCODE LogBarDisk(PPC_APPINFO *cinfo);
extern void PPcPPv(PPC_APPINFO *cinfo);
extern void PPcBackDirectory(PPC_APPINFO *cinfo);
extern void PPcBackDirectoryList(PPC_APPINFO *cinfo);
extern void SetGrayStatus(PPC_APPINFO *cinfo);
extern ERRORCODE PPcDirContextMenu(PPC_APPINFO *cinfo);

extern void SetComment(PPC_APPINFO *cinfo, DWORD CommentID, ENTRYCELL *cell, const TCHAR *comment);
extern ERRORCODE MarkEntry(PPC_APPINFO *cinfo, const TCHAR *wildcard, int mode);
extern BOOL PathTrackingListMenu(PPC_APPINFO *cinfo, int dest);
extern DWORD crc32(const BYTE *bin, DWORD size, DWORD r);
extern void SetSyncView(PPC_APPINFO *cinfo, int mode);
extern void PPcReload(PPC_APPINFO *cinfo);
extern void PPcPaste(PPC_APPINFO *cinfo, BOOL makeshortcut);
extern void SaveLs(const TCHAR *path, LOADSETTINGS *ls);
extern void LoadLs(const TCHAR *path, LOADSETTINGS *ls);

extern void PPcPopCell(PPC_APPINFO *cinfo);
extern void PPcPushCell(PPC_APPINFO *cinfo);
extern void PPcReloadCustomize(PPC_APPINFO *cinfo, LPARAM idparam);
extern void PPcSaveWindowPosition(PPC_APPINFO *cinfo);
extern void PPcMoveWindowSavedPosition(PPC_APPINFO *cinfo);
extern void PPcSystemMenu(PPC_APPINFO *cinfo);
extern BOOL PPcSHContextMenu(PPC_APPINFO *cinfo, const TCHAR *dir, const TCHAR *cmd);
extern HMENU MakeDriveJumpMenu(PPC_APPINFO *cinfo, HMENU hPopupMenu, DWORD *index, ThSTRUCT *TH, BOOL advanced);
extern void FixCellDisplayFormat(PPC_APPINFO *cinfo);
extern BOOL USEFASTCALL FocusAddressBars(HWND hSingleWnd, PPXDOCKS *docks);
extern void USEFASTCALL UnpackExec(PPC_APPINFO *cinfo, const TCHAR *param);
extern void FirstCommand(PPC_APPINFO *cinfo);
extern void DivMark(PPC_APPINFO *cinfo);
extern BOOL OpenClipboardCheck(PPC_APPINFO *cinfo);

//------------------------------------------------------------------ ppc_cr.c
extern void CrMenu(PPC_APPINFO *cinfo, BOOL ShowMenu);

extern BOOL ShellExecEntries(PPC_APPINFO *cinfo, const TCHAR *command, const TCHAR *csrentry, const TCHAR *path, const TCHAR *execpath, BOOL archivemode);

extern BOOL IsArcInArc(PPC_APPINFO *cinfo);

typedef struct {
	PPXMENUINFO x;
	ENTRYINDEX cellindex; // このメニューの生成に用いたCELLのindex
	DWORD comID; // 共通メニューの開始ID
	DWORD extitemID; // 遅延追加項目の開始ID
	TCHAR PathName[VFPS], TypeName[MAX_PATH]; // CELL のフルパス, 種類(:type.ext)
	const TCHAR *regext;
} PPCMENUINFO;
extern void PPcCRMenuOnMenu(PPC_APPINFO *cinfo, PPCMENUINFO *cminfo);
extern void WINAPI DirTypeDelayLoad(DelayLoadMenuStruct *DelayMenus);
extern void DoActionMenu(PPC_APPINFO *cinfo, HMENU hMenu, UINT index, ENTRYINDEX target, ThSTRUCT *THmenu, PPXMENUDATAINFO *pmdi, DWORD extmenuID);

#ifdef _SHLOBJ_H_
extern BOOL RPIDL2DisplayNameOf(TCHAR *name, LPSHELLFOLDER sfolder, LPCITEMIDLIST pidl);
#endif

//------------------------------------------------------------------ ppc_datr.c
extern int CnvDateTime(TCHAR *pack, TCHAR *date, TCHAR *time, const FILETIME *ftime);
extern ERRORCODE PPC_attribute(PPC_APPINFO *cinfo);

//------------------------------------------------------------------ ppc_dir.c
extern void SetCellInfo(PPC_APPINFO *cinfo, ENTRYCELL *cell, BYTE *extcolor);
extern void CellSort(PPC_APPINFO *cinfo, XC_SORT *xs);
extern void ExtCommentExecute(PPC_APPINFO *cinfo, DWORD id);
extern void MaskPathMain(PPC_APPINFO *cinfo, int defaultdepth, const TCHAR *defaultpath);

// Cell の編集同期処理

// 編集中なら true
#define TinyCheckCellEdit(cinfo) cinfo->edit.ref

#define EnterCellEdit(cinfo) EnterCriticalSection(&cinfo->edit.section)
#define LeaveCellEdit(cinfo) LeaveCriticalSection(&cinfo->edit.section)

// 編集モードのブロックに使う
extern int StartCellEdit(PPC_APPINFO *cinfo);
#define EndCellEdit(cinfo) {if ( cinfo->edit.ref > 0 ) cinfo->edit.ref--;}

#define RENTRY_READ		0	// おまかせで読み込み
#define RENTRY_UPDATE	B0	// 最新情報で更新
#define RENTRY_JUMPNAME	B1	// Jfname と同じ名前のエントリに移動
#define RENTRY_NOHIST	B2	// ヒストリを使った移動をしない
#define RENTRY_SAVEOFF	B3	// 現在位置を維持(RENTRY_JUMPNAMEより優先度低い)
#define RENTRY_USEPATHMASK		B4	// 仮想dir用パスマスクを強制使用
#define RENTRY_NOUSEPATHMASK	B5	// 仮想dir用パスマスクを強制不使用
#define RENTRY_NOFIXDIR			B6	// パスがないとき上位に移動しない
#define RENTRY_NEWFILE			B7	// Jfname は新規作成したファイル
#define RENTRY_NEWDIR			B8	// Jfname は新規作成したディレクトリ
#define RENTRY_CACHEREFRESH		B9	// キャッシュを更新
#define RENTRY_MODIFYUP			B10	// 最新情報で更新(非同期遅延読み込みを廃棄する)
#define RENTRY_JUMPNAME_INC		B11	// RENTRY_JUMPNAME を前方一致で行う
#define RENTRY_USECACHE			B12	// キャッシュを有効にする
#define RENTRY_NOASYNC			B13	// 非同期読み込みを無効にする
#define RENTRY_NOLOCK			B14	// タブロック処理を無効にする
#define RENTRY_DRIVEFIX			B15	// ドライブが接続されたとき向けの処理をする
#define RENTRY_ENTERSUB			B16	// サブディレクトリ内へ移動する処理をする
// read_entry内部用
#define RENTRYI_NOSETHISTORY	B22	// 現在パスをヒストリに記憶しない
#define RENTRYI_GETFREESIZE		B23	// 空き容量の取得を行う
#define RENTRYI_NOREADCOMMENT	B24	// コメントの読み込みをおこなわない

#define RENTRYI_ASYNCREAD		B25	// DSET_ASYNCREAD通知用,非 同期読み込みを試行
#define RENTRYI_REFRESHCACHE	B26	// DSET_REFRESHCACHE 通知用
#define RENTRYI_SAVECACHE		B27	// キャッシュの保存を要する
#define RENTRYI_EXECLOADCMD		B28	// X_dset の cmd: を実行する
#define RENTRYI_CACHE			B30	// キャッシュ利用中

// 非同期読み込み時に外すフラグ
#define RENTRY_FLAGS_ARELOAD	(RENTRY_MODIFYUP | RENTRYI_CACHE | RENTRYI_NOREADCOMMENT)
// 新規作成時用
#define RETRY_FLAGS_NEWFILE	(RENTRY_FLAGS_ARELOAD | RENTRY_JUMPNAME | RENTRY_SAVEOFF | RENTRY_NOHIST | RENTRY_NEWFILE)
#define RETRY_FLAGS_NEWDIR	(RENTRY_FLAGS_ARELOAD | RENTRY_JUMPNAME | RENTRY_SAVEOFF | RENTRY_NOHIST | RENTRY_NEWDIR)

extern void GetDirectoryFreeSpace(PPC_APPINFO *cinfo, BOOL subthread);
extern void read_entry(PPC_APPINFO *cinfo, int flag);
extern BOOL CheckWarningName(const TCHAR *filename);
extern void SetCaption(PPC_APPINFO *cinfo);
extern void ClearCellIconImage(PPC_APPINFO *cinfo);
extern void LoadSettingMain(LOADSETTINGS *ls, const TCHAR *path);
extern void DirTaskCommand(PPC_APPINFO *cinfo);
extern BOOL ReadDirBreakCheck(PPC_APPINFO *cinfo);
extern void GetCommentText(ThSTRUCT *comments, const TCHAR *commentfile);
extern void SetNewPath(PPC_APPINFO *cinfo, const TCHAR *path);

extern void USEFASTCALL PPC_DriveJumpMain(PPC_APPINFO *cinfo, TCHAR *menubuf);

//------------------------------------------------------------------ ppc_dir.c
extern BOOL GetCache_Path(TCHAR *filename, const TCHAR *dpath, int *type);
extern HANDLE FindFirstAsync(HWND hWnd, LPARAM lParam, const TCHAR *path, WIN32_FIND_DATA *ff, VFSDIRTYPEINFO *Dtype, int *flag);
extern BOOL FindNextAsync(HANDLE hFF, WIN32_FIND_DATA *ff, int flag);
extern void FindCloseAsync(HANDLE hFF, int flag);
extern BOOL FindOptionDataAsync(HANDLE hFF, DWORD optionID, void *data, int flag);
extern DWORD MiniHash(TCHAR *name);
//------------------------------------------------------------------ ppc_disp.c
#define OCX_IE	0	// IE を使って表示(各種ファイル、種類によっては保存になる)
#define OCX_WMP	1	// WMP を使って表示(各種メディアファイル)
extern BOOL LoadOcx(HWND hParentWnd, TCHAR *filename, int showmode);

#define HideEntryTip(cinfo) {if (cinfo->Tip.states & STIP_STATE_READYMASK) EndEntryTip(cinfo);}
extern void EndEntryTip(PPC_APPINFO *cinfo);
extern void ShowEntryTip(PPC_APPINFO *cinfo, DWORD flags, int mode, ENTRYINDEX tagetcell);
extern int USEFASTCALL GetDispFormatSkip(const BYTE *fmt);
#define GetDispFormatSkip_string(fmt) (1 + TSTRSIZE((TCHAR *)(fmt + 1)))
#define GetDispFormatSkip_time2(fmt) (2 + strlen((char *)fmt + 2) + 1)
#define GetDispFormatSkip_column(fmt) (((DISPFMT_COLUMN *)(fmt + 1))->fmtsize)

#define DISPENTRY_NO_INPANE -1
#define DISPENTRY_NO_OUTPANE -2
extern int DispEntry(PPC_APPINFO *cinfo, HDC hDC, const XC_CFMT *cfmt, ENTRYINDEX No, int maxX, const RECT *BaseBox);
extern COLORREF GetGrayColorB(COLORREF color);

#if USEGRADIENTFILL
extern BOOL DrawGradientBox(HDC hDC, int left, int top, int right, int bottom, COLORREF c1, COLORREF c2);
#endif
extern void USEFASTCALL MakeAttributesString(TCHAR *buf, ENTRYCELL *cell);
extern void MakeStatString(TCHAR *buf, DWORD FileStat, int stateID, int displen);
extern int GetExtGENGOU(const SYSTEMTIME *sTime, TCHAR *dest, const TCHAR option);

//------------------------------------------------------------------ ppc_dlg.c
extern void SetAttibuteSettings(HWND hDlg, DWORD atr);
extern DWORD GetAttibuteSettings(HWND hDlg);

extern INT_PTR CALLBACK SortDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
extern INT_PTR CALLBACK DiskinfoDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
#define PPC_diskinfo(hWnd) PPxDialogBoxParam(hInst, MAKEINTRESOURCE(IDD_DSKI), hWnd, DiskinfoDlgBox, (LPARAM)cinfo)

extern INT_PTR CALLBACK FileMaskDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
extern INT_PTR CALLBACK WindowDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
#define PPC_window(hWnd) PPxDialogBoxParam(hInst, MAKEINTRESOURCE(IDD_CWINDOW), hWnd, WindowDlgBox, (LPARAM)cinfo)
extern void CheckDlgButtonGroup(HWND hDlg, UINT *checkgroup, int Check, int Enable);
extern void EnableDlgWindow(HWND hDlg, int id, BOOL state);
extern void CenterPPcDialog(HWND hDlg, PPC_APPINFO *cinfo);

//------------------------------------------------------------------ ppc_finf.c
extern void MakeFileInformation(PPC_APPINFO *cinfo, ThSTRUCT *text, ENTRYCELL *cell);

//------------------------------------------------------------------ ppc_fop.c
extern void PPc_DoFileOperation(PPC_APPINFO *cinfo, const TCHAR *action, TMS_struct *files, const TCHAR *destdir, const TCHAR *option, DWORD flags);

extern ERRORCODE PPC_MakeDir(PPC_APPINFO *cinfo);
extern ERRORCODE PPC_WriteDir(PPC_APPINFO *cinfo);
extern ERRORCODE PPC_Rename(PPC_APPINFO *cinfo, BOOL continuous);
extern void PPcFileOperation(PPC_APPINFO *cinfo, const TCHAR *action, const TCHAR *destdir, const TCHAR *option);
extern ERRORCODE DeleteEntry(PPC_APPINFO *cinfo);
extern ERRORCODE DeleteEntrySH(PPC_APPINFO *cinfo);
extern ERRORCODE PPC_ExplorerCopy(PPC_APPINFO *cinfo, BOOL move);
extern ERRORCODE PPC_MakeEntry(PPC_APPINFO *cinfo);
extern void PPcCreateHarkLink(PPC_APPINFO *cinfo);
extern ERRORCODE PPcDupFile(PPC_APPINFO *cinfo);
extern ERRORCODE EraseListEntry(PPC_APPINFO *cinfo);
extern void GetNewName(TCHAR *dest, const TCHAR *base, const TCHAR *path);
extern void MakeMakeEntryItem(HMENU hMenu);
extern ERRORCODE USEFASTCALL MakeEntryMain(PPC_APPINFO *cinfo, int type, TCHAR *name);
extern BOOL IsNodirShnPath(PPC_APPINFO *cinfo);

//------------------------------------------------------------------ ppc_fsiz.c
extern int DispMarkSize(PPC_APPINFO *cinfo);
extern void CountMarkSize(PPC_APPINFO *cinfo);
extern void ClearMarkSize(PPC_APPINFO *cinfo);

//------------------------------------------------------------------ ppc_incs.c
extern void InitIncSearch(PPC_APPINFO *cinfo, TCHAR c);
extern BOOL WmCharSearch(PPC_APPINFO *cinfo, WORD key);
extern BOOL IncSearchKeyDown(PPC_APPINFO *cinfo, WORD key, WPARAM wParam, LPARAM lParam);
extern void SearchEntry(PPC_APPINFO *cinfo);
extern void SearchEntryOnekey(PPC_APPINFO *cinfo, WORD key);
extern void ShowSearchState(PPC_APPINFO *cinfo);

//------------------------------------------------------------------ ppc_init.c
extern void InitPPcGlobal(void);						// 初期化(全PPc共用)
extern void InitPPcWindow(PPC_APPINFO *cinfo, BOOL usepath);	// 初期化(個別初期化)
extern void PreClosePPc(PPC_APPINFO *cinfo);
extern void ClosePPc(PPC_APPINFO *cinfo);
extern void USEFASTCALL SetLinebrush(PPC_APPINFO *cinfo, COLORREF color);
extern void USEFASTCALL CreateScrollBar(PPC_APPINFO *cinfo);
extern void USEFASTCALL HideScrollBar(PPC_APPINFO *cinfo);
extern void InitFont(PPC_APPINFO *cinfo);
extern void InitGuiControl(PPC_APPINFO *cinfo);
extern void PPcLoadCust(PPC_APPINFO *cinfo);
extern void CloseGuiControl(PPC_APPINFO *cinfo);
extern void PPcSaveCust(PPC_APPINFO *cinfo);
extern BOOL CheckReady(PPC_APPINFO *cinfo);
extern BOOL RegisterID(PPC_APPINFO *cinfo, PPCSTARTPARAM *psp, BOOL *usepath);
extern BOOL LoadParam(PPCSTARTPARAM *psp, const TCHAR *param);
extern BOOL CallPPc(PPCSTARTPARAM *psp, HWND hWnd);
extern BOOL CallPPcParam(HWND hWnd, const TCHAR *param);
extern void MorePPc(const TCHAR *cmdline, MAINWINDOWSTRUCT *mws);
extern void SetWindowMinMax(HWND hWnd, PPCSTARTPARAM *psp);
extern HFONT GetControlFont(DWORD dpi, ControlFontStruct *cfs);
extern HFONT CreateMesFont(int mag, DWORD FontDPI);
extern void SaveTreeSettings(HWND hTreeWnd, const TCHAR *custid, DWORD mode, DWORD width);
#ifdef _SHLOBJ_H_
extern void GetIDLSub(TCHAR *path, LPSHELLFOLDER pSF, LPITEMIDLIST pSHidl);
#endif

//------------------------------------------------------------------ ppc_main.c
extern BOOL USEFASTCALL IsTouchMessage(void);
extern int USEFASTCALL GetPointType(void);
extern void ReuseFix(PPCSTARTPARAM *psp);
extern void WmWindowPosChanged(PPC_APPINFO *cinfo);
extern int WINAPI PPcMain(PPCSTARTPARAM *psp);
extern void PostWindowClosed(void);
extern LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern VOID CALLBACK DelayLogShowProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
extern void SetDDScroll(PPC_APPINFO *cinfo, POINT *pos);
extern void ClearMark(PPC_APPINFO *cinfo);
extern void ExecDualParam(PPC_APPINFO *cinfo, const TCHAR *param);
extern void WmWindowPosChanged(PPC_APPINFO *cinfo);
extern LRESULT WmPPxCommand(PPC_APPINFO *cinfo, WPARAM wParam, LPARAM lParam);
extern BOOL USEFASTCALL MoveWinOff(PPC_APPINFO *cinfo, ENTRYINDEX offset);

//------------------------------------------------------------------ ppc_pain.c
extern void Repaint(PPC_APPINFO *cinfo);
extern void RefleshInfoBox(PPC_APPINFO *cinfo, int flags);
extern void RefleshStatusLine(PPC_APPINFO *cinfo);
extern void RefleshCell(PPC_APPINFO *cinfo, ENTRYINDEX cell);
extern void Paint(PPC_APPINFO *cinfo);
extern void PaintStatusLine(PPC_APPINFO *cinfo, PAINTSTRUCT *ps, RECT *BoxStatus, ENTRYINDEX EI_No);
extern void PaintInfoLine(PPC_APPINFO *cinfo, PAINTSTRUCT *ps, RECT *BoxInfo, ENTRYINDEX EI_No);

#ifndef USEDIRECTX
#define UpdateWindow_Part(hwnd) {if (UseOffScreen == FALSE) UpdateWindow(hwnd);}
#else
#define UpdateWindow_Part(hwnd) {}
#endif
//------------------------------------------------------------------ ppc_sub.c
#define GETFILES_FULLPATH B0
#define GETFILES_SETPATHITEM B1
#define GETFILES_REALPATH B2
#define GETFILES_DATAINDEX B3
extern TCHAR *GetFiles(PPC_APPINFO *cinfo, int flags);
extern TCHAR *GetFilesForListfile(PPC_APPINFO *cinfo);

extern void InitLog(PPC_APPINFO *cinfo);
extern void WriteLogs(PPC_APPINFO *cinfo, TCHAR *mes, int error);
extern void InitEnumMarkCell(PPC_APPINFO *cinfo, int *work);
extern ENTRYCELL *EnumMarkCell(PPC_APPINFO *cinfo, int *work);
#define StopEnumMarkCell(work) {work = -2;}
extern TCHAR *GetDriveRoot(TCHAR *path);

extern void USEFASTCALL FixHideEntry(PPC_APPINFO *cinfo);
extern void USEFASTCALL HideOneEntry(PPC_APPINFO *cinfo, ENTRYINDEX index);

extern void SetRefreshAfterList(PPC_APPINFO *cinfo, int actiontype, TCHAR drivename);
extern void RefreshAfterList(PPC_APPINFO *cinfo, int actiontype);
extern int GetEntryDepth(const TCHAR *src, const TCHAR **last);
extern HWND USEFASTCALL GetJoinWnd(PPC_APPINFO *cinfo);

// GetItemTypeFromPoint の戻り値
#define PPCR_UNKNOWN	0		//不明
#define PPCR_PATH		0x0f	//PATH				0行目
#define PPCR_STATUS		0x10	//LINE Status line	1行目
#define PPCR_MENU		0x11	//HMNU Hidden Menu	2, 3行目、マウス有り
#define PPCR_INFOTEXT	0x12	//INFO Info line	2, 3行目、マウスなし
#define PPCR_INFOICON	0x13	//ICON Info icon	2, 3行目、左端
#define PPCR_CELLBLANK	0x20	//SPC  cell blank	セル領域、該当セルなし
#define PPCR_CELLMARK	0x21	//MARK cell Mark	セル、左端(マーク用)
#define PPCR_CELLTEXT	0x22	//ENT  cell cell	セル、テキスト部分
#define PPCR_CELLTAIL	0x23	//TAIL  cell tail	セル、右端(ボタン用)
extern int GetItemTypeFromPoint(PPC_APPINFO *cinfo, POINT *pos, ENTRYINDEX *ItemNo);

typedef struct {
	int TouchWidth; // タッチ検出のための最小幅
	int RightBorder; // ここより右は、常に空欄扱いにする・Tailの右端
	int TailRightOffset;
	int TailWidth;
} CELLRIGHTRANGES;

extern void GetCellRightRanges(PPC_APPINFO *cinfo, CELLRIGHTRANGES *ranges);

extern void IOX_win(PPC_APPINFO *cinfo, BOOL save);

extern DWORD_PTR USECDECL PPcGetIInfo(PPC_APPINFO *cinfo, DWORD cmdID, PPXAPPINFOUNION *uptr);

extern ERRORCODE SCmenu(PPC_APPINFO *cinfo, const TCHAR *action);
extern void SendX_win(PPC_APPINFO *cinfo);
extern void SetPPcDirPos(PPC_APPINFO *cinfo);
extern void SavePPcDir(PPC_APPINFO *cinfo, BOOL newdir);

extern void USEFASTCALL SetPopMsg(PPC_APPINFO *cinfo, ERRORCODE err, const TCHAR *msg);
extern void StopPopMsg(PPC_APPINFO *cinfo, int mask);

#define REPORT_GETERROR MAX32
extern BOOL WriteReport(const TCHAR *title, const TCHAR *name, ERRORCODE errcode);
#ifdef _SHLOBJ_H_
extern ENTRYINDEX MakePIDLTable(PPC_APPINFO *cinfo, LPITEMIDLIST **pidls, LPSHELLFOLDER *pSF);
extern void FreePIDLS(LPITEMIDLIST *pidls, ENTRYINDEX cnt);
#endif
extern void USEFASTCALL SetDummyCell(ENTRYCELL *cell, const TCHAR *lfn);
extern LPVOID USEFASTCALL PPcHeapAlloc(DWORD dwBytes);
extern BOOL USEFASTCALL PPcHeapFree(LPVOID mem);
extern TCHAR * USEFASTCALL PPcStrDup(const TCHAR *string);

extern void ClearChangeState(PPC_APPINFO *cinfo);
extern BOOL FindCell(PPC_APPINFO *cinfo, const TCHAR *name);

extern DWORD GetCustXDword(const TCHAR *kword, const TCHAR *subkword, DWORD defaultvalue);

extern DWORD BackPathTrackingList(PPC_APPINFO *cinfo, DWORD top);
extern void JumpPathTrackingList(PPC_APPINFO *cinfo, int dest);
extern int PPctInput(PPC_APPINFO *cinfo, const TCHAR *title, TCHAR *string, int maxlen, WORD readhist, WORD writehist);
extern ERRORCODE InputTargetDir(PPC_APPINFO *cinfo, const TCHAR *title, TCHAR *string, int maxlen);
extern void USEFASTCALL GetPairPath(PPC_APPINFO *cinfo, TCHAR *path);
extern HWND USEFASTCALL GetPairWnd(PPC_APPINFO *cinfo);
extern ENTRYINDEX GetFirstMarkCell(PPC_APPINFO *cinfo);
extern ENTRYINDEX GetNextMarkCell(PPC_APPINFO *cinfo, ENTRYINDEX cellindex);
extern void LoadSortOpt(const TCHAR **param, XC_SORT *xc);

#define REPORTTEXT_CLOSE (const TCHAR *)(DWORD_PTR)1
#define REPORTTEXT_OPEN (const TCHAR *)(DWORD_PTR)0
#define REPORTTEXT_FOCUS (const TCHAR *)(DWORD_PTR)2
extern void SetReportText(const TCHAR *text);
extern void DelayedFileOperation(PPC_APPINFO *cinfo);
extern void UpdateEntryData(PPC_APPINFO *cinfo);
extern void USEFASTCALL InitJobinfo(JOBINFO *jinfo);
extern void USEFASTCALL FinishJobinfo(PPC_APPINFO *cinfo, JOBINFO *jinfo, ERRORCODE result);
extern BOOL BreakCheck(PPC_APPINFO *cinfo, JOBINFO *jinfo, const TCHAR *memo);

extern void WriteListFileForUser(PPC_APPINFO *cinfo, const TCHAR *filename, int wlfc_flags);
extern BOOL WriteListFileForRaw(PPC_APPINFO *cinfo, const TCHAR *filename);
extern void SetReportTextMain(HWND hLogWnd, const TCHAR *text);
extern BOOL HdropdataToFiles(HGLOBAL hDrop, TMS_struct *files);

extern void LoadCFMT(XC_CFMT *cfmt, const TCHAR *name, const TCHAR *sub, const XC_CFMT *defaultdata);
extern void FreeCFMT(XC_CFMT *cfmt);
extern int PPcTrackPopupMenu(PPC_APPINFO *cinfo, HMENU hMenu);
extern void GetPopupPosition(PPC_APPINFO *cinfo, POINT *pos);
extern void PPcLayoutCommand(PPC_APPINFO *cinfo, const TCHAR *param);
extern void DuplicateMenu(HMENU hDestMenu, HMENU hSrcMenu);
extern void ToggleMenuBar(PPC_APPINFO *cinfo);
extern void DoChooseResult(PPC_APPINFO *cinfo, TCHAR *Param);
extern void GetDriveVolumeName(PPC_APPINFO *cinfo);
extern void USEFASTCALL PeekLoop(void);
extern UTCHAR GetCommandParameter(LPCTSTR *commandline, TCHAR *param, size_t paramlen);
extern UTCHAR GetCommandParameterDual(LPCTSTR *commandline, TCHAR *param, size_t paramlen);

extern void PPcChangeDirectory(PPC_APPINFO *cinfo, const TCHAR *newpath, DWORD flags);
extern void JumpPathEntry(PPC_APPINFO *cinfo, const TCHAR *newpath, DWORD flags);
extern DWORD GetReparsePath(const TCHAR *path, TCHAR *pathbuf);
extern HMENU MakeLayoutMenu(PPC_APPINFO *cinfo, HMENU hPopupMenu, DWORD *index, ThSTRUCT *TH);
extern void USEFASTCALL tstrlimcpy(TCHAR *dest, const TCHAR *src, DWORD maxlength);
extern BOOL LoadImageSaveAPI(void);
extern int ImageSaveByAPI(BITMAPINFO *bfh, DWORD bfhsize, char *bmp, size_t bmpsize, const TCHAR *filename);

extern void WINAPI DummyNotifyWinEvent(DWORD event, HWND hwnd, LONG idObject, LONG idChild);
extern void USEFASTCALL CreateNewPane(const TCHAR *param);
extern int CheckOffScreenMark(PPC_APPINFO *cinfo, const TCHAR *title);

extern void BackupLog(void);
extern void RestoreLog(void);
extern ENTRYINDEX UpSearchMarkCell(PPC_APPINFO *cinfo, ENTRYINDEX cellindex);
extern ENTRYINDEX DownSearchMarkCell(PPC_APPINFO *cinfo, ENTRYINDEX cellindex);

extern TCHAR *FindBothLastEntry(const TCHAR *path);
extern void AppendPath(TCHAR *path, const TCHAR *appendpath, TCHAR sepchar);
extern void TinyGetMenuPopPos(HWND hWnd, POINT *pos);
extern void tstrreplace(TCHAR *text, const TCHAR *targetword, const TCHAR *replaceword);
extern BOOL IsShellExBlocked(const TCHAR *ClsID);

//------------------------------------------------------------------ ppc_subi.c
extern BOOL USEFASTCALL LoadCellImage(PPC_APPINFO *cinfo, ENTRYCELL *celltmp, BYTE *fmt, const TCHAR *UseFile);
extern void SaveCacheFile(PPC_APPINFO *cinfo, TCHAR *filename, int seplen, BITMAPINFO *bmpinfo, LPVOID lpBits);
typedef void OverlayClassTable;
extern HICON LoadFileIcon(const TCHAR *filename, DWORD attr, DWORD flags, UINT iconsize, OverlayClassTable **LayPtr);
extern void FreeOverlayCom(void);

#define LIDX_FILE	0x10000 // .icoファイル等を読む(なければリソース)
// index : 正順番を示すインデックス、負リソースID or LIDX_FILE
extern HICON LoadIconDx(const TCHAR *IDorDLLname, int index, int iconsize);

//---------------------------------------------------------------- ppc_subp.cpp
extern void ExtExec(PPC_APPINFO *cinfo, ThSTRUCT *thEcdata, DWORD index, DWORD CommentID);
extern void ExtInfoTip(PPC_APPINFO *cinfo);
extern void CellInfoTip(PPC_APPINFO *cinfo);
extern HRESULT GetLink(HWND hWnd, const TCHAR *LinkFile, TCHAR *Original);
typedef void (WINAPI * GETINFOTIPCALLBACK)(void *data, const TCHAR *text);
extern BOOL GetInfoTipText(const TCHAR *filename, int extlen, GETINFOTIPCALLBACK callbackfunc, void *data);
extern void ExtGetData(ThSTRUCT *thEcdata, DWORD index, const TCHAR *filename, DWORD attributes, GETINFOTIPCALLBACK callbackfunc, void *data);
extern void SaveClipboardData(HGLOBAL hGlobal, UINT cpdtype, PPC_APPINFO *cinfo);
extern void ExistCheck(TCHAR *dst, const TCHAR *path, const TCHAR *name);
extern LRESULT WmGetObject(PPC_APPINFO *cinfo, WPARAM wParam);
extern void FreeAccServer(PPC_APPINFO *cinfo);

typedef struct {
	WIN32_FIND_DATA ff;
	TCHAR filename[VFPS];
} SYNCPROPINFO;
extern void SyncProperties(HWND hWnd, SYNCPROPINFO *info);


extern TCHAR * FindLastEntryPoint(const TCHAR *src);

extern WORD GetColumnExtItemIndex(PPC_APPINFO *cinfo, const TCHAR *itemname);
extern TCHAR *GetColumnExtTextInfo(PPC_APPINFO *cinfo, int cellcolumn, TCHAR *dst);

extern void VistaProperties(const TCHAR *filename, ThSTRUCT *text);
extern void PPcEnterTabletMode(PPC_APPINFO *cinfo);
extern void GetVistaFileProps(PPC_APPINFO *cinfo, TCHAR *dest, int len, ENTRYCELL *cell);
extern void EndButtonMenu(void);
extern BOOL IsShowButtonMenu(void);

//------------------------------------------------------------------ ppc_subt.c
extern DWORD WINAPI SubThread(LPDWORD lpdwParam);
extern void CloseAnySizeIcon(ANYSIZEICON *icons);
extern HICON LoadUnknownIcon(PPC_APPINFO *cinfo, int iconsize);
extern HICON LoadDefaultDirIcon(PPC_APPINFO *cinfo, int iconsize);
extern void CreateIconList(PPC_APPINFO *cinfo, ICONCACHESTRUCT *icons, int width, int height);
extern int USEFASTCALL FindCellFormatImagePosition(BYTE *fmt);

extern void TerminateFopThreadCheck(PPC_APPINFO *cinfo);
//------------------------------------------------------------------ ppc_wher.c
#define WHEREIS_NORMAL 0
#define WHEREIS_INVFS 1
extern ERRORCODE WhereIsDialog(PPC_APPINFO *cinfo, int mode);
extern ERRORCODE WhereIsCommand(PPC_APPINFO *cinfo, const TCHAR *param, BOOL usedialog);
extern void WriteFF(HANDLE hFile, WIN32_FIND_DATA *ff, const TCHAR *name);
extern const TCHAR *ZGetParam(const TCHAR *(*ptr), TCHAR *buf);
extern void ButtonHelpMenu(PPC_APPINFO *cinfo, HWND hDlg, HWND hButtonWnd, UINT editID, int type);
extern void GetMaskTextFromEdit(HWND hDlg, UINT control, TCHAR *text, DWORD wordmatch);

//------------------------------------------------------------------ ppc_wind.c
extern void FixWindowSize(PPC_APPINFO *cinfo, int offsetx, int offsety);
extern void FixTwinWindow(PPC_APPINFO *cinfo);
extern void InitCli(PPC_APPINFO *cinfo);
extern void SetScrollBar(PPC_APPINFO *cinfo, int mode);
extern void JoinWindow(PPC_APPINFO *cinfo);
extern void JointWindowMain(PPC_APPINFO *cinfo, HWND PairHWnd);
extern BOOL PPcMouseCommand(PPC_APPINFO *cinfo, const TCHAR *click, const TCHAR *type);

extern LRESULT PPcNCMouseCommand(PPC_APPINFO *cinfo, UINT message, WPARAM wParam, LPARAM lParam);
extern void FixCellWideV(PPC_APPINFO *cinfo);
extern void BootPairPPc(PPC_APPINFO *cinfo);

#define FPS_KEYBOARD 0
#define FPS_RATE 1
#define FPS_FONTRATE 2
extern void FixPaneSize(PPC_APPINFO *cinfo, int offsetx, int offsety, int mode);
extern void FixHeader(PPC_APPINFO *cinfo);
extern void CalcClickWidth(PPC_APPINFO *cinfo);

//---------------------------------------------------------------- rendrdat.cpp
extern HGLOBAL CreateHText(PPC_APPINFO *cinfo);
extern HGLOBAL CreateHDrop(PPC_APPINFO *cinfo);
extern HGLOBAL CreateShellIdList(PPC_APPINFO *cinfo);


//------------------------------------------------------------------ MD5.c
#define PROTO_LIST(list) list
typedef unsigned char *POINTER;		// POINTER defines a generic pointer type
typedef unsigned short int UINT2;	// UINT2 defines a two byte word
typedef unsigned long int UINT4;	// UINT4 defines a four byte word
/* MD5 context. */
typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;
extern void MD5Init PROTO_LIST ((MD5_CTX *));
extern void MD5Update PROTO_LIST  ((MD5_CTX *, unsigned char *, unsigned int));
extern void MD5Final PROTO_LIST ((unsigned char [16], MD5_CTX *));

//------------------------------------------------------------------ PPCOMBO.c
extern HWND InitCombo(PPCSTARTPARAM *psp);	// Combo window を生成/HWND を得る
extern PPC_APPINFO *GetComboTarget(HWND hTargetWnd, POINT *pos);

extern TCHAR *tstristr(const TCHAR *target, const TCHAR *findstr);
extern void LoadCombos(void);
extern int GetComboRegZID(void);

#ifdef __cplusplus
}
#endif
