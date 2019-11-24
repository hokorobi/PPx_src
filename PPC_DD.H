/*-----------------------------------------------------------------------------
	Paper Plane cUI										Drag & Drop 定義
-----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

#define USE_URLDROP 0 // FileContents が無くても cfURL を使って保存する時用
#define MAX_NUM_FORMAT 5

#define DROPMODE_TREE B0
#define DROPMODE_TREELIST B1
#define DROPMODE_EDGE B2
#define DROPMODE_TOOLBAR B3
#define DROPMODE_FILEFOLDER B4

// C / C++ ====================================================================
										// クリップボードID
extern UINT cfFileGroupDescriptor;
extern UINT cfFileContents;
#if USE_URLDROP
extern UINT cfURL;
#endif
										// Drop の受付処理
extern void dd_init(PPC_APPINFO *cinfo);
extern void dd_close(PPC_APPINFO *cinfo);
extern void dd_combo_init(void);
extern void dd_combo_close(void);
										// Drag を開始する
extern void PPcDragFile(PPC_APPINFO *cinfo);

extern int GetCellNoFromPoint(POINTL pos, int *flag);

extern HRESULT Link(char *LinkedFile, char *LinkFname, char *DestPath);
extern BOOL ContextMenu(HWND hWnd, char *path, char *entry, LPPOINT lppt, char *cmd);
extern void ExecSendTo(PPC_APPINFO *cinfo, const TCHAR *cmd);
//----------------------------------------------------------------------------
extern void MarkDragArea(PPC_APPINFO *cinfo, RECT *area, int mode);
extern void DrawDragFrame(HWND hWnd, const RECT *area);
extern void CalcDragTarget(PPC_APPINFO *cinfo, POINT *pos, RECT *area);

extern BOOL AutoDD_UseDLL(PPC_APPINFO *cinfo, HWND hTargetWnd, const TCHAR *src, BOOL droptype);
extern void * GetPathInterface(HWND hWnd, const TCHAR *name, REFIID riid, const TCHAR *path);
extern BOOL CopyToShnPathFiles(PPC_APPINFO *cinfo, const TCHAR *destpath, DWORD alloweffect);
extern HRESULT PPcCopyToDropTarget(IDataObject *DataObject, IDropTarget *DropTarget, BOOL droptype, HWND hTargetWnd, DWORD alloweffect);

// C++ only ===================================================================
#ifdef __cplusplus
}
class CImpIDataObject: public IDataObject{
	private:
		long m_cRefCount;
		int m_cFormatsAvailable;
		FORMATETC m_strFormatEtc[MAX_NUM_FORMAT];
		STGMEDIUM m_strStgMedium[MAX_NUM_FORMAT];

		PPC_APPINFO *m_cinfo;
		int m_arcmode;
		TCHAR m_arctemp[VFPS];

	public:
		CImpIDataObject();
		~CImpIDataObject();

										// IUnknown members -------------------
		STDMETHOD(QueryInterface)(REFIID riid, void **ppvObj);
		STDMETHOD_(ULONG, AddRef)(void);
		STDMETHOD_(ULONG, Release)(void);
										// IDataObject methods ----------------
		STDMETHOD(GetData)(LPFORMATETC pformatetcIn,  LPSTGMEDIUM pmedium);
		STDMETHOD(GetDataHere)(LPFORMATETC pformatetc, LPSTGMEDIUM pmedium);
		STDMETHOD(QueryGetData)(LPFORMATETC pformatetc);
		STDMETHOD(GetCanonicalFormatEtc)(LPFORMATETC pformatetc,
												LPFORMATETC pformatetcOut);
		STDMETHOD(SetData)(LPFORMATETC pformatetc,
									STGMEDIUM *pmedium, BOOL fRelease);
		STDMETHOD(EnumFormatEtc)(DWORD dwDirection,
									LPENUMFORMATETC *ppenumFormatEtc);
		STDMETHOD(DAdvise)(FORMATETC *pFormatetc, DWORD advf,
							LPADVISESINK pAdvSink, DWORD *pdwConnection);
		STDMETHOD(DUnadvise)(DWORD dwConnection);
		STDMETHOD(EnumDAdvise)(LPENUMSTATDATA *ppenumAdvise);
										// PPx's ----------------
		void Init(PPC_APPINFO *cinfo);
};
//-----------------------------------------------------------------------------
class CEnumFormatEtc : public IEnumFORMATETC{
	private:
		ULONG		m_cRefCount;
		LPFORMATETC	m_pStrFE;
		ULONG		m_iCur;
		ULONG		m_cItems;

	public:
		CEnumFormatEtc(LPFORMATETC pFE, int numberItems);
		~CEnumFormatEtc(void);
										// IUnknown members -------------------
		STDMETHOD(QueryInterface)(REFIID, void **);
		STDMETHOD_(ULONG, AddRef)(void);
		STDMETHOD_(ULONG, Release)(void);
										// IEnumFORMATETC members -------------
		STDMETHOD(Next)(ULONG, LPFORMATETC, ULONG *);
		STDMETHOD(Skip)(ULONG);
		STDMETHOD(Reset)(void);
		STDMETHOD(Clone)(IEnumFORMATETC **);
};
//-----------------------------------------------------------------------------
class CImpIDropSource: public IDropSource{
	private:
		long m_cRefCount;
	public:
		CImpIDropSource();
		~CImpIDropSource();
										// IUnknown members -------------------
		STDMETHOD(QueryInterface)(REFIID, void **);
		STDMETHOD_(ULONG, AddRef)(void);
		STDMETHOD_(ULONG, Release)(void);
										// IDataObject members ----------------
		STDMETHOD(GiveFeedback)(DWORD dwEffect);
		STDMETHOD(QueryContinueDrag)(BOOL fEscapePressed, DWORD grfKeyState);
};
//-----------------------------------------------------------------------------
typedef struct {
	UINT priority;
	BOOL hdrop;
	BOOL pidl;
	BOOL filecontents;
#if USE_URLDROP
	BOOL url;
#endif
	BOOL text;
	BOOL etc;
} DROPTYPE;


union uuFGD {
	FILEGROUPDESCRIPTORA *a;
	FILEGROUPDESCRIPTORW *w;
};

typedef struct {
	BOOL unicode;	// 文字コード
	union uuFGD uFGD;
	STGMEDIUM medium;
} FGDSTRUCT;

extern BOOL QueryDrop_CheckToolBarSub(HWND hToolBarWnd, POINT *pos, ThSTRUCT *ToolBarTh, TCHAR *TargetPath);

class PPcDropTargetBase {
	public:
		const TCHAR *mToolBarName; // ドロップ先のツールバー名
		TCHAR mSourcePath[VFPS];	// ドロップ元のドライブ名参照用パス
		TCHAR mTargetPath[VFPS];	// ドロップ先のパス(ツリーで使用)
		DWORD mDefAction;	// ドロップ動作のデフォルト
		DWORD mDropModeFlags;
		DWORD mKeyState;	// クリック時のキー状態(Dropではクリック解除済み)
		DWORD mTitem;		// 現在のツリー選択

		// PPc 各窓用
		int		mCell;		// ドロップ先	-1:空欄(このディレクトリ) -2:ツリー
							// 				-3:再設定用ダミー  0以上:セル上
		// Combo 用
		int mTargettype;	// 現在のドロップ先

		STDMETHODIMP QueryDrop_DriveBar(DWORD grfKeyState, DWORD *pdwEffect, HWND hToolBarWnd, POINT *pos);
		STDMETHODIMP QueryDrop_ToolBar(DWORD grfKeyState, DWORD *pdwEffect, HWND hToolBarWnd, POINT *pos, ThSTRUCT *ToolBarTh);
		STDMETHODIMP QueryDrop_Dock(DWORD grfKeyState, DWORD *pdwEffect, HWND hTargetWnd, POINT *pos, PPXDOCK *dock);
		BOOL QueryDrop_Tree(HWND hTreeWnd, POINTL *pt, DWORD *pdwEffect);
		void SetDirectoryDefAction(void);
};

// PPc 各窓用 IDropTarget =====================================================
class CDropTarget:public IDropTarget, private PPcDropTargetBase{
	public:
		CDropTarget();
										// IUnknown methods -------------------
		STDMETHOD(QueryInterface)(REFIID riid, void **ppvObj);
		STDMETHOD_(ULONG, AddRef)(void);
		STDMETHOD_(ULONG, Release)(void);
										// IDropTarget methods ----------------
		STDMETHOD(DragEnter)(LPDATAOBJECT pDataObj, DWORD grfKeyState,
							 POINTL pt, LPDWORD pdwEffect);
		STDMETHOD(DragOver)(DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect);
		STDMETHOD(DragLeave)(void);
		STDMETHOD(Drop)(LPDATAOBJECT pDataObj, DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect);

		void SetPPcAppInfo(PPC_APPINFO *cinfo);
	private:
		ULONG	mRef;		// ref counter
		int		mEdge;		// 境界位置(挿入)なら !0
		DROPTYPE mDDtype;
#if FREEPOSMODE
		POINT	mCellPos;	// 自D&D時に表示させるCellの影の位置
#endif
		PPC_APPINFO *mCinfo;

		STDMETHODIMP QueryDrop(DWORD grfKeyState, POINTL *pt, DWORD *pdwEffect);

		BOOL DropDDFormat(LPDATAOBJECT pDataObj, POINTL *pt, DWORD pdwEffect);
		BOOL DropEtcFormat(LPDATAOBJECT pDataObj, POINTL *pt, DWORD pdwEffect);
		STDMETHODIMP DropPidlLink(LPDATAOBJECT pDataObj, DWORD pdwEffect);
#if USE_URLDROP
		STDMETHODIMP DropUrl(LPDATAOBJECT pDataObj);
#endif
		void *GetGlobalData(LPDATAOBJECT pDataObj, UINT format, STGMEDIUM *medium);
		BOOL OpenFGD(FGDSTRUCT *FGD, LPDATAOBJECT pDataObj);
		void CloseFGD(FGDSTRUCT *FGD);
		void GetFGDFileName(FGDSTRUCT *FGD, int index, TCHAR *dest, const TCHAR *path);
		IDropTarget *mShnDropTarget; // SHN形式のときに使う IDropTarget
};

// PPc Combo 窓用 IDropTarget =================================================
class ComboDropTarget:public IDropTarget, private PPcDropTargetBase{
	public:
		ComboDropTarget();
										// IUnknown methods -------------------
		STDMETHOD(QueryInterface)(REFIID riid, void **ppvObj);
		STDMETHOD_(ULONG, AddRef)(void);
		STDMETHOD_(ULONG, Release)(void);
										// IDropTarget methods ----------------
		STDMETHOD(DragEnter)(LPDATAOBJECT pDataObj, DWORD grfKeyState,
							 POINTL pt, LPDWORD pdwEffect);
		STDMETHOD(DragOver)(DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect);
		STDMETHOD(DragLeave)(void);
		STDMETHOD(Drop)(LPDATAOBJECT pDataObj, DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect);
	private:
		ULONG refs;		// ref counter

		void ComboDropDDFormat(LPDATAOBJECT pDataObj, DWORD pdwEffect, TCHAR *TargetPath);
		STDMETHODIMP QueryDrop(DWORD grfKeyState, POINTL *pt, DWORD *pdwEffect);
};

#define DDCELL_BADPOSITION -3 // D&D対象外
#define DDCELL_CONTROL -2 // ツールバー等のコントロール上
#define DDCELL_SPACE -1 // comboのD&D対象外
extern void AddToolBarButton(HWND hWnd, const TCHAR *sDrop, const TCHAR *ToolBarName);

extern void GetSourcePath(LPDATAOBJECT pDataObj, TCHAR *SourcePath);

extern BOOL GetDropData(LPDATAOBJECT pDataObj, TMS_struct *files);
extern void ListDrop(HWND hWnd, TCHAR *sDrop, TCHAR *targetpath, TCHAR *destpath);
extern void LinkDrop(HWND hWnd, TCHAR *sDrop, TCHAR *targetpath);
extern BOOL ExecFileOperation(DWORD pdwEffect, const TCHAR *targetpath, TMS_struct *files, PPC_APPINFO *cinfo, const TCHAR *option);

extern void GetDropFormat(DROPTYPE &DDtype, LPDATAOBJECT pDataObj);
extern void DDMenu(HWND hWnd, POINTL *pt, DWORD *pdwEffect, DWORD orgEffect, DWORD key, LPDATAOBJECT pDataObj, int Cell, DROPTYPE &DDtype, PPC_APPINFO *cinfo, const TCHAR *TargetPath, DWORD DropModeFlags);
extern DWORD GetDDEffect(DWORD targetattr, DWORD keystate, DWORD *result, DWORD defaction);

#ifdef RELEASE
	#define Debug_DispIID(mes, riid)
#else
	void Debug_DispIID(const TCHAR *mes, REFIID riid);
#endif

#define ESINDEX 0xf000		// DragDropHandler が使うメニューインデックス開始
#define ESINDEXSTEP 0x40	// １つの DragDropHandler が使うメニュー数
enum {
	DROPEFFECT_FILEEXTSTART = 0xff00,
	DROPEFFECT_DETAIL,	// ファイル操作詳細ダイアログ
	DROPEFFECT_JUMP,	// 該当エントリにジャンプ
	DROPEFFECT_SWAP,	// エントリ入れ替え
	DROPEFFECT_DUP,		// 複写
	DROPEFFECT_OPENWITH,	// 開く
//	DROPEFFECT_NEWPANE,		// 新規タブで表示
	DROPEFFECT_MAKELIST,	// ListFile 作成
	DROPEFFECT_ADDTREEITEM,	// ツリーに項目作成
	DROPEFFECT_ADDTOOLBAR,	// ツールバーに項目作成
	DROPEFFECT_OBJECTSTART, // 指定オブジェクトで保存
#if USE_URLDROP
	DROPEFFECT_URL,
#endif
	DROPEFFECT_TEXT,
	DROPEFFECT_ETC,		// メニューで選択
	DROPEFFECT_FILECONTENTS,
	DROPEFFECT_OBJECTEND
};

#define TARGET_NONE	0
#define TARGET_TREE	1
#define TARGET_TAB	2
#define TARGET_ADDR	3
#define TARGET_REPORT	4
#define TARGET_TOOLBAR	5
#define TARGET_REBAR	6
#define TARGET_PPC	7
#endif
