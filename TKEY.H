/*-----------------------------------------------------------------------------
		キーの定義
-----------------------------------------------------------------------------*/
//				b0-7				// キーコード
#define K_v		B8					//	  仮想キー
#define K_e		B9					//	~ 拡張SHIFT
#define K_s		B10					//	\ SHIFT
#define K_c		B11					//	^ CTRL

#define K_a		B12					//	& ALT/GRPH
#define K_ex	B13					//	  非キーの追加機能
#define K_raw	B14					//	@ エイリアス禁止(raw)
#define K_internal B15				//	  reserved(プロセス間通信用)

#ifndef VK_OEM_PLUS				 // US  JIS
	#define VK_OEM_1		0xBA // ;:
	#define VK_OEM_PLUS		0xBB // =+  ;+
	#define VK_OEM_COMMA	0xBC // ,
	#define VK_OEM_MINUS	0xBD // -_  -=
	#define VK_OEM_PERIOD	0xBE // .
	#define VK_OEM_2		0xBF // /?
	#define VK_OEM_3		0xC0 // `~
#endif
/*
	3Fxx/BFxx	PPx 共通
	3Exx/BExx	PPc
	3Dxx/BDxx	PPv
	3Cxx/BCxx	PPb
	3Bxx/BBxx	PPe/edit
	3Axx/BAxx	↓その他
	bae0-baff	  PPtray
	ba80-badf	  PPtree
	ba40-ba7f	  combo window
	390x		user timer
	38xx/b8xx	user define
	x000-x7FF(x:3, B)WM_SYSCOMMAND
*/

#define K_alt	(K_v | VK_MENU)		// [ALT][GRPH]
#define K_apps	(K_v | VK_APPS)		// [APPS]

#define K_bs	(K_v | VK_BACK)		//	[BS][BackSpace]
#define K_tab	(K_v | VK_TAB)		//	[Tab]
#define K_cr	(K_v | VK_RETURN)	//	[RETURN][Enter]
#define K_esc	(K_v | VK_ESCAPE)	//	[ESC]
#define K_Pdw	(K_v | VK_NEXT)		//	[Page Down][ROLL UP]
#define K_Pup	(K_v | VK_PRIOR)	//	[Page Up][ROLL DOWN]
#define K_ins	(K_v | VK_INSERT)	//	[Page Up][ROLL DOWN]
#define K_del	(K_v | VK_DELETE)	//	[Delete]
#define K_up	(K_v | VK_UP)		//	[↑]
#define K_lf	(K_v | VK_LEFT)		//	[←]
#define K_ri	(K_v | VK_RIGHT)	//	[→]
#define K_dw	(K_v | VK_DOWN)		//	[↓]
#define K_home	(K_v | VK_HOME)		//	[HOME]
#define K_help	(K_v | VK_END)		//	[HELP]
#define K_end	(K_v | VK_END)		//	[END]
#define K_xfer	(K_v | VK_CONVERT)	//	[XFER]
#define K_nfer	(K_v | VK_CONVERT + 1)//	[NFER]
#define K_space	(K_v | VK_SPACE)	//	[Space]

#define K_F1	(K_v | VK_F1)
#define K_F2	(K_v | VK_F2)
#define K_F3	(K_v | VK_F3)
#define K_F4	(K_v | VK_F4)
#define K_F5	(K_v | VK_F5)
#define K_F6	(K_v | VK_F6)
#define K_F7	(K_v | VK_F7)
#define K_F8	(K_v | VK_F8)
#define K_F9	(K_v | VK_F9)
#define K_F10	(K_v | VK_F10)
#define K_F11	(K_v | VK_F11)
#define K_F12	(K_v | VK_F12)
#define K_F13	(K_v | VK_F13)
#define K_F14	(K_v | VK_F14)
#define K_F15	(K_v | VK_F15)

#define EXTCMD_KEY ((UTCHAR)0x9e)	// キーマクロ
#define EXTCMD_CMD ((UTCHAR)0x9f)	// コマンドライン
//-----------------------------------------------------------------------------
//0x3fxx	PPx 共通機能/Event
#define K_E_FIRSTX	0x3f00			// 最初のPPx起動 (未)
#define K_E_EXIT	0x3f01			// 最後のPPx終了
#define K_E_FIRST	0x3f02			// PPx起動
#define K_E_CLOSE	0x3f03			// PPx終了
#define K_E_LOAD	0x3f04			// 読み込み完了
#define K_E_SELECT	0x3f05			// カーソル移動
#define K_E_ACTIVE	0x3f06			// アクティブ
#define K_E_INACTIVE 0x3f07			// 非アクティブ (未)
#define K_E_TABLET	0x3f08			// タッチ操作モードに移行
// #define K_E_PC		0x3f09			//
#define K_E_RANGE1	0x3f18			// カーソル範囲外
#define K_E_RANGE2	0x3f19
#define K_E_RANGE3	0x3f1a
#define K_E_RANGE4	0x3f1b
#define K_E_RANGE5	0x3f1c
#define K_E_RANGE6	0x3f1d
#define K_E_RANGE7	0x3f1e
#define K_E_RANGE8	0x3f1f
#define K_E_COMMENT  0x3f20			// コメント使用開始
#define K_E_COMMENT1 0x3f21			// 拡張コメント1のソート前
#define K_E_COMMENT2 0x3f22			//	:
#define K_E_COMMENT3 0x3f23			//	:
#define K_E_COMMENT4 0x3f24			//	:
#define K_E_COMMENT5 0x3f25			//	:
#define K_E_COMMENT6 0x3f26			//	:
#define K_E_COMMENT7 0x3f27			//	:
#define K_E_COMMENT8 0x3f28			//	:
#define K_E_COMMENT9 0x3f29			//	:
#define K_E_COMMENT10 0x3f2a		// 拡張コメント10のソート前

//0x3f60- PPx 共通機能/Command
#define K_WTOP	0x3f60				// Top Window
#define K_WBOT	0x3f61				// Bottom Window
//#define K_RESTORE	0x3f67			// Restore Window
#define K_HIDE	0x3f68				// Hide Window
#define K_UNHIDEALL	0x3f69			// Un Hide All PPx

#define K_Loff	0x3f70				// Logoff
#define K_Poff	0x3f71				// poweroff
#define K_Rbt	0x3f72				// Reboot
#define K_Sdw	0x3f73				// Shutdown
#define K_Fsdw	0x3f74				// Force Shutdown
#define K_Suspend	0x3f75			// Suspend
#define K_Hibernate	0x3f76			// Hibernate

#define K_SSav	0x3e78				// Screen Saver
#define K_ANDV	0x3e79				// Allocate Network drive
#define K_FNDV	0x3e7a				// Free Network drive

#define K_about	0x3f80				// about
#define K_supot	0x3f81				// support

#define K_Lcust	0x3f90				// Load cust
#define K_Scust	0x3f91				// Save cust
#define K_cust	0x3f92				// Customizer 起動
#define K_menukeycust	0x3f93		// メニュー表記設定
#define K_layout	0x3e94			// layout(X_win)

#define K_GETJOBWINDOW	0x3fea		// Job list windowを登録
#define K_ADDJOBTASK	0x3feb		// Jobを登録
#define K_DELETEJOBTASK	0x3fec		// Jobを削除

#define K_CHECKUPDATE 0x3fed		// カスタマイズ内容のバージョンチェック
#define K_FIRSTCMD	0x3fee			// 起動後コマンド実行(/k)
#define K_CLEANUP	0x3fef			// Clean up (終了前の処理、但し続行もok)
#define K_Lvfs	0x3ff0				// Load VFS
#define K_Fvfs	0x3ff1				// Free VFS

#define K_MSEP	0x3ffd				// Menu sparator 用
#define K_MBRK	0x3ffe				// Menu bar break 用
#define K_NULL	0x3fff				// NULL code

// 通信用
#define KE__FREE	0xbf00			// hook 解除指令用
#define K_ENDATTR	0xbf10			// Attribute変更終了通知
#define K_ENDCOPY	0xbf11			// Copy終了通知
#define K_ENDDEL	0xbf12			// Delete終了通知
#define K_FREEDRIVEUSE	0xbf13		// ドライブの使用をやめる (lparam:drv)
#define K_REPORTSHUTDOWN 0xbf14		// シャットダウン開始の通知
#define K_EDITCHANGE 0xbf15			// tinput の内容が更新
#define K_SETTINGCHANGE 0xbf16		// WM_SETTINGCHANGE受信
#define K_GETDISPDPI 0xbf17			// 指定WindowがあるディスプレイのDPIを取得
#define K_CHENGEDDISPDPI 0xbf18		// ディスプレイのDPIが変更された(lparam:dpi

#define K_SETAPPID	0xbf40			// Windows APPIDを登録 wParam:ID 1文字
#define K_TBB_INIT	0xbf41			// ツールバーボタン関連の初期化を行う
#define K_TBB_PROGRESS	0xbf42		// ツールバーボタンプログレス表示 wParam
#define K_TBB_STOPPROGRESS	0xbf43	// ツールバーボタンプログレス表示 wParam
typedef struct {
	HWND hWnd;
	int nowcount;
	int maxcount;
} TASKBARBUTTONPROGRESSINFO;
#define K_FLASHWINDOW	0xbf44		// タイトルバーのフラッシュ
#define K_STOPFLASHWINDOW 0xbf45	// タイトルバーのフラッシュを止める
#define K_SETACTIVEFOCUS 0xbf46		// アクティブにしたときにフォーカスを得る
									// ウィンドウ(lParam)を指定する NULL:解除
#define K_SETIME	0xbf47			// X_IMEに従いIME状態を変更する
#define K_INITROMA	0xbf48			// migemo.dllをロードする
#define K_IMEOFF	0xbf49			// IME を offにする
#define K_ENABLE_NC_SCALE	0xbf4a	// 非クライアント領域の拡大縮小を有効に
#define K_DRAWCCBACK	0xbf4b		// ツールバー等の背景を描画 wParam:NMH

#define K_COMMAND	0xbff0			// Command (lparam:hMap)
#define K_EXTRACT	0xbff1			// Extract (lparam:hMap)

#define K_THREADUNREG	0xbff6		// スレッドを管理外にする
#define K_THREADRESTART	0xbff7		// スレッドが異常終了したので再起動
#define K_POPOPS	0xbff8			// ポップアップ用に使う座標を指定・取得
				// HIWORD(wParam)=PPT_ lParam=MAKELPARAM(x, y)  (SAVED用)
	#define PPT_FOCUS	0	// キャレット/エントリ等のフォーカス位置を基準に (キーボード時)
	#define PPT_MOUSE	1	// マウス位置を基準に (マウス時)
	#define PPT_SAVED	2	// PopupPosを基準に
	#define PPT_GET		0xffff	// 座標を取得する
#define K_CPOPMENU	0xbff9			// コンソール用のポップアップメニュー
#define K_SETLOGWINDOW	0xbffa
#define K_SETFAULTOPTIONINFO	0xbffb	// FaultOptionInfo 設定
#define K_SETPOPMSG	0xbffc	// ステータス行に表示 (wparam:POPMSG_ lparam:text)
#define K_SETPOPLINENOLOG	0xbffd	// ステータス行に表示 (lparam:text)
#define K_WINDDOWLOG	0xbffe		// ログウィンドウにログを表示 (lparam:text)
#define K_SENDREPORT	0xbfff		// 異常報告送信を行う

//0x3exx	PPc 特殊機能	---------------------------------------------------
#define KC_WIND	0x3e00				// Window Dialog
#define KC_DODO	0x3e02				// Do Delayed File Operation
#define K_SETGRAYSTATUS	0x3e03		// 全エントリの更新情報を GRAY にする
#define KC_RELOAD		0x3e04		// 遅延読み込み完了したので読み込み反映
#define KC_UNFOCUS		0x3e05		// フォーカスが外れているかを再判定し反映
#define KC_CHECKRELOAD	0x3e06		// アクティブ時に未更新なら更新する
#define KC_DRIVERELOAD	0x3e07		// ドライブが接続したので再読み込みする

#define KC_PTOP	0x3e80				// Top Window(pair)
#define KC_PBOT	0x3e81				// Top Window(pair)

#define KC_Tvfs	0x3e91				// toggle VFS
#define KC_Edir	0x3e92				// enter dir
#define KC_JMPE	0x3e93				// jump entry
#define KC_UpdateEntryData	0x3e95	// 選択エントリのファイル情報を再取得する

// 通信用
#define KC_POPTENDFIX 0xbe00		// スレッド終了時デッドロックからの復帰通知
#define KC_Join	0xbe01				// Window 連結の通知
#define KC_DoJW	0xbe02				// Window 連結の実行 lParam : Pait HWND
#define KC_StDS	0xbe03				// ディレクトリのサイズ通知(WM_COPYDATA内)
#define KC_MOREPPC	0xbe05			// PPcを起動等(WM_COPYDATA内)
#define KC_MOUSECMD	0xbe06			// 指定のマウスコマンドを実行
#define KC_ADDENTRY	0xbe07			// エントリの追加(WM_COPYDATA内)
#define KC_ENDCOMPARE	0xbe08		// 比較の完了
#define KC_PRECLOSE	0xbe09			// CLOSE指示
#define KC_COMMENTEVENT	0xbe0a		// COMMENTEVENT生成の指示 lParam:
#define KC_GETCRCHECK	0xbe38		// CRMENUSTACKCHECK取得
//								AutoD&D用
#define KC_REGISTADDPROC	0x0bebbeb0	// IFをOLE ROTに登録する
#define KC_REVOKEADDPROC	0x1bebbeb1	// IFをOLE ROTから削除する
#define KC_HOOKADDPROC		0xbeb2		// HOOKを設定する wParam:hWnd
#define KC_UNHOOKADDPROC	0xbeb3		// HOOKを解除する wParam:hWnd
//								連動プロパティ内用
#define KC_SYNCPROP_INIT	0xbec0	// 初期化
#define KC_SYNCPROP_SETFILE	0xbec1	// 表示ファイルを指定
#define KC_GETSITEHWND		0xbeff	// 指定位置の hWnd を取得
		#define PPCSITE_SINGLE 0
		#define PPCSITE_LEFT 1
		#define PPCSITE_RIGHT 2
		#define KC_GETSITEHWND_BASEWND	-1 // 一体化窓の hWnd。なければ自分自身
		#define KC_GETSITEHWND_CURRENT	0 // 現在窓
		#define KC_GETSITEHWND_LEFT		PPCSITE_LEFT  // 左
		#define KC_GETSITEHWND_RIGHT	PPCSITE_RIGHT // 右(右側反対窓)
		#define KC_GETSITEHWND_PAIR		3 // 反対窓
		#define KC_GETSITEHWND_LEFTENUM	4 // 左からの連番
//0x3dxx	PPv 特殊機能	---------------------------------------------------
#define KV_ICC	0x3b80				// toggle Enable ICC
// 通信用
#define KV_Load	0xbd80				// load file lPatam : 戻り先 HWND
#define KV_FOCUS	0xbd81			// set focus KV_Load で登録した hWnd へ移動

//0x3bxx	PPe 特殊機能	---------------------------------------------------
#define KE_eo	0x3b00				// "&Open Files
#define KE_ec	0x3b01				// "&Close
#define KE_es	0x3b02				// "&Save as
//#define KE_en	0x3b03				// "&New Files
//#define KE_el	0x3b05				// "&Load Files
#define KE_ea 	0x3b06				// "&Append to
#define KE_ed	0x3b08				// "&Duplicate
//#define KE_eu	0x3b09				// "&Undo Edit
#define KE_ei	0x3b0a				// "&Insert File
//#define KE_ex	0x3b0b				// "Close all(&X)
#define KE_ee	0x3b0c				// "&Exec command
#define KE_eq	0x3b0d				// "&Quit
#define KE_er	0x3b07				// "&Run as admin

#define KE_kp	0x3b0e				// "&Print
#define KE_ku	0x3b0f				// "&Undo Paste
#define KE_kc	0x3b10				// "Paste(&Copy)
#define KE_kk	0x3b11				// "Copy Line/Bloc&k
#define KE_kd	0x3b12				// "&Duplicate Line
#define KE_kr	0x3b22				// "&Read only
#define KE_kz	0x3b13				// "&Zen/han convert
#define KE_kb	0x3b14				// "Column &Block
#define KE_ky	0x3b15				// "&Y clear stack
#define KE_k0	0x3b16				// "Set Marker #&0
#define KE_k1	0x3b17				// "Set Marker #&1
#define KE_k2	0x3b18				// "Set Marker #&2
#define KE_k3	0x3b19				// "Set Marker #&3
#define KE_k4	0x3b1a				// "Set Marker #&4

#define KE_qh	0x3b1b				// "Cut word left(&H)
#define KE_qt	0x3b1c				// "Cu&t BOL
#define KE_qy	0x3b1d				// "Cut EOL(&Y)
#define KE_qu	0x3b1e				// "Word case(&U)
#define KE_ql	0x3b1f				// "Restore &Line
#define KE_qf	0x3b20				// "Set &Find string
#define KE_qo	0x3b21				// "Replece Next(&O)
//#define KE_qp	0x3b22				// "&Put File Name
#define KE_qi	0x3b23				// "&Insert find str
#define KE_qj	0x3b24				// "&Jump to Line
#define KE_qp	0x3b27				// "Last &Position
#define KE_qlw	0x3b29				// "Left of Window(&[)
#define KE_qrw	0x3b2a				// "Right of Window(&])
#define KE_qs	0x3b2b				// "Top of Line(&S)
#define KE_qd	0x3b2c				// "End of Line(&D)
//#define KE_qe	0x3b2d				// "Top of Window(&E)
//#define KE_qx	0x3b2e				// "End of Window(&X)
#define KE_qk	0x3b2f				// "Jump Brac&ket
#define KE_qm	0x3b30				// "Set &Marker #0
#define KE_qv	0x3b2d				// "&View mode
#define KE_q0	0x3b31				// "Jump to Marker #&0
#define KE_q1	0x3b32				// "Jump to Marker #&1
#define KE_q2	0x3b33				// "Jump to Marker #&2
#define KE_q3	0x3b34				// "Jump to Marker #&3
#define KE_q4	0x3b35				// "Jump to Marker #&4
#define KE_qb	0x3b36				// "&Block Top/End

#define KE_2b	0x3b38				//
#define KE_2l	0x3b39				//
#define KE_2s	0x3b3a				//
#define KE_2c	0x3b3b				//
#define KE_2r	0x3b3c				//
#define KE_2I	0x3b3d				//
#define KE_2t	0x3b3e				//
#define KE_2p	0x3b3f				//
#define KE_2a	0x3b40				//
#define KE_closeUList	0x3b41		// 上側補完リストを閉じる(使用中なら無視)
// 通信用
#define KE_insertsel	0xbb00		// *insertsel lParam : 文字列
#define KE_insert	0xbb01			// *insert lParam : 文字列
#define KE_replace	0xbb02			// *replace lParam : 文字列
#define KE_openfile	0xbb03			// 指定のファイルを開く lParam : ファイル名
#define KE_seltext	0xbb04			// %*selecttext
#define KE_edtext	0xbb05			// %*edittext
#define KE_closecheck	0xbb06		// 終了時の保存チェック。0なら中止要求
#define KE_setkeya	0xbb07			// 追加キー割当て lParam : static 文字列
#define KE_defmenu	0xbb08			// *defaultmenu
#define KE_opennewfile	0xbb09		// 指定のファイルを開く lParam : ファイル名
#define KE_excmdopen	0xbb0a		// *ppe/*edit用 lParam : PPE_CMDMODEDATA
#define KE_execprecmd	0xbb0b		// Input_FirstCmd を実行
#define KE_clearmodify	0xbb0c		// 更新初期化
#define KE_getHWND	0xbbff			// (WM_PPXCOMMAND経由)editのhWndを得る

//0x3a8x/ba8x	PPtree 特殊機能 ----------------------------------------------
			// 通知用
#define KTN_escape	0xba80			// ESCを押した…フォーカス移動 or Tree終了
#define KTN_focus	0xba81			// foucus移動(tab key)
#define KTN_select	0xba82			// 選択(cursor)
#define KTN_selected 0xba83			// 選択完了(enter key)
#define KTN_size	0xba84			// 大きさ変更
#define KTN_close	0xba85			// 選択しないで終了

//0x3aex/baex	PPtray 特殊機能 ----------------------------------------------
			// 通知用
#define KRN_getcwnd		0xbae8		// 共用hWnd確保 戻り値 : 共用 hWnd
#define KRN_freecwnd	0xbae9		// 共用hWnd解放

//0x3a4x/ba4x～ba6x combowindow 特殊機能 --------------------------------------
#define KCW_entry		0xba40		// ウィンドウを登録する lParam : HWND
		#define KCW_entry_NOACTIVE B16 // フォーカス設定なし
		#define KCW_entry_SELECTNA B17 // NOACTIVE & SELECT
		#define KCW_entry_DEFPANE B18 // ペインの位置(PSPONE_PANE_)を指定する
#define KCW_focus		0xba41		// フォーカス移動通知 lParam : HWND
#define KCW_size		0xba42		// ウィンドウサイズ変更通知 lParam : HWND
#define KCW_swap		0xba43		// 窓交換 lParam : HWND
#define KCW_setpath		0xba44		// パス変更 lParam : HWND
#define KCW_setforeground	0xba45	// 指定の窓を前面に lP : HWND
#define KCW_drawinfo	0xba46		// 情報行の描画を指令する
#define KCW_nextppc		0xba47		// 次／前のPPcへ移動  PPCHGWIN_xxx

#define KCW_getpath		0xba48		// lP in:*hwnd out:path
#define KCW_getpairwnd	0xba49		// 反対窓のHWNDを得る lParam:HWND
#define KCW_swapwnd		0xba4a		// 窓を交換する

#define KCW_capture		0xba4b		// 任意のウィンドウを取り込む lParam:HWND
#define KCW_tree		0xba4c		// ツリー表示する lParam:TCHAR*カレントpath
#define KCW_tabrotate	0xba4d		// タブのローテーション HI(wParam), lParam
#define KCW_reqsize		0xba4e		// ウィンドウサイズ変更依頼通知 lParam : HWND
#define KCW_drawstatus	0xba4f		// ステータス行の描画を指令する
typedef struct {
	HWND hWnd;
	int offsetx, offsety, mode;
} SCW_REQSIZE;
#define KCW_panecommand	0xba50		// *pane コマンド
#define KCW_addressbar	0xba51		// addressbar にフォーカス設定
#define KCW_dock		0xba52		// *Dock コマンド実行 HI(wParam), lParam:chr
#define KCW_ready		0xba53		// 全窓の初期化完了の通知、lP:HWND
#define KCW_setmenu		0xba54		// メニュー変更の通知 lP:HMENU
#define KCW_getsite		0xba55		// 指定窓の左右位置を取得 lParam:HWND
#define KCW_layout		0xba56		// layout更新
#define KCW_eject		0xba57		// ペインを独立化 lParam:baseindex
#define KCW_pathfocus	0xba58		// 指定パスの窓があればフォーカス lParam:pat
#define KCW_closetabs	0xba59		// ロック無しタブを閉じる lParam: first/last
#define KCW_ActivateWnd	0xba5a		// HIWORD wParam:showind lParam:baseind
#define KCW_SelectWnd	0xba5b
#define KCW_ppclist		0xba5c		// ppc一覧のメモリを取得
#define KCW_closealltabs 0xba5d		// ロック有・PPc以外も閉じる

#define KCW_captureEx	0xba5f		// キャプチャされた窓がPPx対応かを確認する
									// 戻り値が MAKELPARAM(1, KCW_captureEx)なら
									// PPx対応
#define KCW_enteraddress 0xba60		// address bar に入力があったことが通知
#define KCW_showjoblist  0xba61		// job一覧窓を表示
#define KCW_GetIDWnd	0xba62		// 指定されたhWndを取得
