/*-----------------------------------------------------------------------------
	Paper Plane vUI										～ コードページ切替 ～
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX.H"
#include "PPVUI.RH"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop

#define FULLCP 0

typedef struct {
	UINT MaxCharSize;
	BYTE DefaultChar[MAX_DEFAULTCHAR];
	BYTE LeadByte[MAX_LEADBYTES];
	WCHAR UnicodeDefaultChar;
	UINT CodePage;
	TCHAR CodePageName[MAX_PATH];
} impCPINFOEX;

DefineWinAPI(BOOL, GetCPInfoEx, (UINT CodePage, DWORD dwFlags, impCPINFOEX *lpCPInfoEx)) = INVALID_VALUE(impGetCPInfoEx);

typedef struct {
	DWORD cp;
	const TCHAR *name;
} CPTBLSTRUCT;
const CPTBLSTRUCT cptbl[] = {
	{CP_ACP,	T("system locale")},
	{CP_OEMCP,	T("system console locale")},
#if FULLCP
	{CP_MACCP,	T("Mac")},
	{37,	T("EBCDIC 米国/カナダ")},
	{38,	T("International(old)EBCDIC")},
	{42,	T("Symbol")},
	{111,	T("Greek")},
	{112,	T("Turkish")},
	{113,	T("Yugoslavian")},
	{161,	T("Arabic Linux")},
	{162,	T("Arabic Linux")},
	{163,	T("Arabic Linux")},
	{164,	T("Arabic Linux")},
	{165,	T("Arabic Linux")},
	{237,	T("Germany EBCDIC")},
	{274,	T("Belgium EBCDIC")},
	{275,	T("Brazilian EBCDIC")},
	{277,	T("Danish/Norwegian EBCDIC")},
	{281,	T("Japanese-E EBCDIC")},
	{290,	T("Japanese Kana EBCDIC")},
	{367,	T("US-ASCII ISO 646-US")},
#endif
	{CP__US, T("OEM US(MSDOS)")},
#if FULLCP
	{500,	T("EBCDIC インターナショナル")},
	{708,	T("Arabic ASMO-708")},
	{709,	T("Arabic ASMO-449")},
	{710,	T("Arabic Transparent Arabic")},
	{720,	T("Arabic Transparent ASMO")},
	{737,	T("OEM Greek 437G")},
	{775,	T("OEM Baltic")},
#endif
	{850,	T("OEM ML Latin I")},
#if FULLCP
	{852,	T("OEM スラブ ラテン II")},
	{855,	T("OEM キリル ロシア")},
	{857,	T("OEM トルコ")},
	{858,	T("OEM ラテン I + ヨーロッパ")},
	{860,	T("OEM ポルトガル")},
	{861,	T("OEM アイスランド")},
	{862,	T("OEM ヘブライ")},
	{863,	T("OEM カナダ フランス語")},
	{864,	T("OEM Arabic")},
	{865,	T("OEM ノルディック")},
	{866,	T("OEM ロシア")},
	{869,	T("OEM 近代ギリシャ")},
	{870,	T("EBCDIC マルチリンガル")},
	{874,	T("ANSI/OEM Thailand")},
	{875,	T("EBCDIC モダンギリシャ")},
	{881,	T("ISO 8859-1 Latin 1")},
	{882,	T("ISO 8859-2 Latin 2")},
	{883,	T("ISO 8859-3 Latin 3")},
	{884,	T("ISO 8859-4 Latin 4")},
	{885,	T("ISO 8859-5 Latin 5")},
#endif
	{CP__SJIS,	T("ANSI/OEM Shift-JIS")},
#if FULLCP
	{934,	T("ANSI/OEM 韓国")},
	{936,	T("ANSI/OEM 中国簡体 GBK")},
	{938,	T("ANSI/OEM 台湾")},
	{949,	T("ANSI/OEM 韓国")},
	{950,	T("ANSI/OEM 中国繁体 Big5")},
	{951,	T("ANSI/OEM 中国簡体")},
	{1026,	T("EBCDIC Turkish(Latin 5)")},
	{1047,	T("EBCDIC Latin 1  Open System")},
	{1140,	T("EBCDIC 米国/カナダ")},
	{1141,	T("EBCDIC ドイツ")},
	{1142,	T("EBCDIC デンマーク/ノルウェー")},
	{1143,	T("EBCDIC フィンランド/スウェーデン")},
	{1144,	T("EBCDIC イタリア")},
	{1145,	T("EBCDIC ラテン/スペイン")},
	{1146,	T("EBCDIC 英国")},
	{1147,	T("EBCDIC フランス")},
	{1148,	T("EBCDIC インターナショナル")},
	{1149,	T("EBCDIC アイスランド")},
	{CP__UTF16L, T("UNICODE little endian ISO10646")},
	{CP__UTF16B, T("UNICODE big endian ISO10646")},
	{1250,	T("ANSI 中央 Europe Latin2")},
	{1251,	T("ANSI Cyrillic")},
#endif
	{CP__LATIN1,	T("ANSI Latin 1")},
#if FULLCP
	{1253,	T("ANSI Greek")},
	{1254,	T("ANSI Turkish")},
	{1255,	T("ANSI Hebrew")},
	{1256,	T("ANSI Arabic")},
	{1257,	T("ANSI Baltic")},
	{1258,	T("ANSI Vietnamese")},
	{1361,	T("Korean Johab")},
	{10000,	T("Mac Standard(roman)")},
	{10001,	T("Mac 日本語")},
	{10002,	T("Mac Big5")},
	{10003,	T("Mac Korea")},
	{10004,	T("Mac Arabic")},
	{10005,	T("Mac Hebrew")},
	{10006,	T("Mac Greek I")},
	{10007,	T("Mac Cyrillic")},
	{10008,	T("Mac GB 2312")},
	{10010,	T("Mac ルーマニア")},
	{10017,	T("Mac ウクライナ")},
	{10021,	T("Mac Thai")},
	{10029,	T("Mac Latin 2")},
	{10079,	T("Mac Icelandic")},
	{10081,	T("Mac Turkish")},
	{10082,	T("Mac クロアチア")},
	{20000,	T("台湾 CNS")},
	{20001,	T("台湾 TCA")},
	{20002,	T("台湾 Eten")},
	{20003,	T("台湾 IBM5550")},
	{20004,	T("台湾 TeleText")},
	{20005,	T("台湾 Wang")},
	{20105,	T("IA5 IRV インターナショナル No.5")},
	{20106,	T("IA5 ドイツ")},
	{20107,	T("IA5 スウェーデン")},
	{20108,	T("IA5 ノルウェー")},
	{20127,	T("ASCII US")},
	{20261,	T("T.61")},
	{20269,	T("ISO 6937 Non-Spacing Accent")},
	{20273,	T("EBCDIC ドイツ")},
	{20277,	T("EBCDIC デンマーク/ノルウェー")},
	{20278,	T("EBCDIC フィンランド/スウェーデン")},
	{20280,	T("EBCDIC イタリア")},
	{20284,	T("EBCDIC ラテン/スペイン")},
	{20285,	T("EBCDIC 英国")},
	{20290,	T("EBCDIC 日本語(カナ拡張)")},
	{20297,	T("EBCDIC フランス")},
	{20420,	T("EBCDIC アラビア")},
	{20423,	T("EBCDIC ギリシャ")},
	{20424,	T("EBCDIC ヘブライ")},
	{20833,	T("EBCDIC 韓国語")},
	{20866,	T("ロシア KOI8")},
	{20871,	T("EBCDIC アイスランド")},
	{20880,	T("EBCDIC キリル文字(ロシア)")},
	{20905,	T("EBCDIC トルコ")},
	{20924,	T("EBCDIC ラテン 1/Open System")},
	{20932,	T("日本 JIS X 0208-1990 0212-1990(EUC-JP)")},
	{20936,	T("中国簡体 GB2312")},
	{20949,	T("EUC-KR?")},
	{21025,	T("EBCDIC キリル(セルビア,ブルガリア)")},
	{21027,	T("Ext Alpha Lowercase")},
	{21866,	T("ウクライナ")},
	{28591,	T("ISO 8859-1 ラテン 1")},
	{28592,	T("ISO 8859-2 中央ヨーロッパ")},
	{28593,	T("ISO 8859-3 ラテン 3")},
	{28594,	T("ISO 8859-4 バルト")},
	{28595,	T("ISO 8859-5 キリル")},
	{28596,	T("ISO 8859-6 アラビア")},
	{28597,	T("ISO 8859-7 ギリシャ")},
	{28598,	T("ISO 8859-8 ヘブライ")},
	{28599,	T("ISO 8859-9 ラテン 5")},
	{28605,	T("ISO 8859-15 ラテン 9")},
	{38598,	T("ISO 8859-8 ヘブライ")},
	{CP__JIS,	T("ISO-2022 日本 JIS X 0202-1984 1byte カタカナ無")},
	{50221,	T("ISO-2022 日本 JIS X 0202-1984 1byte カタカナ有")},
	{CP__JISSI,	T("ISO-2022 日本 JIS X 0201-1989 (RFC 1468)")},
	{50225,	T("ISO-2022 韓国")},
	{50227,	T("ISO-2022 中国簡体字")},
	{50229,	T("ISO-2022 中国繁体字")},
	{50932,	T("JIS auto detect")},
	{50949,	T("KR auto detect")},
	{CP__EUCJP,	T("euc-jp")},
	{51936,	T("GB2312")},
	{51949,	T("EUC 韓国")},
	{51950,	T("EUC 台湾")},
	{52936,	T("HZ-GB2312 中国簡体字")},
	{54936,	T("GB18030")},
	{57002,	T("ISCII デバナガリ")},
	{57003,	T("ISCII ベンガル")},
	{57004,	T("ISCII タミール")},
	{57005,	T("ISCII テルグ")},
	{57006,	T("ISCII アッサム")},
	{57007,	T("ISCII オリヤー")},
	{57008,	T("ISCII カナラ")},
	{57009,	T("ISCII マラヤラム")},
	{57010,	T("ISCII グジャラート")},
	{57011,	T("ISCII グルムキー")},
#endif
	{CP_UTF7,	T("UTF-7")},
	{CP_UTF8,	T("UTF-8")},
	{0xffff, NULL}
};

const TCHAR StrDefault[] = MES_DEFA;
const TCHAR StrSelCP[] = MES_TSCP;
const TCHAR StrUnknown[] = MES_UNKN;

HWND hCodePageWnd;
int CodePageIndex;

BOOL CALLBACK EnumCodePageProc(TCHAR *str)
{
	TCHAR buf[VFPS], buf2[VFPS];
	const TCHAR *p;
	DWORD cp;
	impCPINFOEX ciex;
	HWND hListWnd;

	p = str;
	cp = (LCID)GetNumber(&p);
	wsprintf(buf, T("%05d - "), cp);

	if ( DGetCPInfoEx == INVALID_VALUE(impGetCPInfoEx) ){
		GETDLLPROCT(GetModuleHandle(StrKernel32DLL), GetCPInfoEx);
	}
	if ( (DGetCPInfoEx != NULL) && (FALSE != DGetCPInfoEx(cp, 0, &ciex)) ){
		p = ciex.CodePageName;
		while ( Isdigit(*p) ) p++;
		while ( *p == ' ' ) p++;
	}else{
		const CPTBLSTRUCT *ct = cptbl;

		for( ;; ){
			if ( ct->cp == 0xffff ){
				p = MessageText(StrUnknown);
				break;
			}
			if ( ct->cp == cp ){
				p = ct->name;
				break;
			}
			ct++;
		}
	}
	tstrcpy(buf + 5 + 3, p);

	hListWnd = GetDlgItem(hCodePageWnd, IDL_PT_LIST);
	{
		int midindex, minindex, maxindex;

		minindex = midindex = 1;
		maxindex = CodePageIndex - 1;
		for (;;){
			if ( minindex >= maxindex ){
				if ( minindex == maxindex ){
					midindex = minindex;
				}
				break;
			}

			midindex = (maxindex + minindex) / 2;

			SendMessage(hListWnd, LB_GETTEXT, (WPARAM)midindex, (LPARAM)buf2);
			if ( tstrcmp(buf, buf2) > 0 ){
				minindex = midindex + 1;
			}else{
				maxindex = midindex;
			}
		}

		SendMessage(hListWnd, LB_SETITEMDATA,
			SendMessage(hListWnd, LB_INSERTSTRING, (WPARAM)midindex, (LPARAM)buf)
			, (LPARAM)cp);
	}

	CodePageIndex++;
	return TRUE;
}

INT_PTR CALLBACK GetCodePageDialog(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch(iMsg){
		case WM_INITDIALOG: {
			hCodePageWnd = hDlg;
			CodePageIndex = 0;

			LocalizeDialogText(hDlg, 0);
			SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)lParam);
			SetWindowText(hDlg, MessageText(StrSelCP));
			SendDlgItemMessage(hCodePageWnd, IDL_PT_LIST, LB_ADDSTRING, 0,
					(LPARAM)MessageText(StrDefault));
			SendDlgItemMessage(hCodePageWnd,
					IDL_PT_LIST, LB_SETITEMDATA, (WPARAM)CodePageIndex++, 0);

			EnumSystemCodePages(EnumCodePageProc, CP_INSTALLED);
			SendDlgItemMessage(hDlg, IDL_PT_LIST, LB_SETCURSEL, 0, 0);
			PostMessage(hDlg, WM_COMMAND, TMAKELPARAM(IDL_PT_LIST, LBN_SELCHANGE), 0);
			return TRUE;
		}
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDOK:
					EndDialog(hDlg, 1);
					break;
				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;
				case IDL_PT_LIST:
					if ( HIWORD(wParam) == LBN_SELCHANGE ){
						LRESULT type;

						type = SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0);
						if ( type == LB_ERR ) break;
						*(UINT *)GetWindowLongPtr(hDlg, DWLP_USER) =
								SendMessage((HWND)lParam,
											LB_GETITEMDATA, (WPARAM)type, 0);
					}
					break;
			}
			break;

		case WM_CLOSE:
			EndDialog(hDlg, 0);
			break;

		default:
			return PPxDialogHelper(hDlg, iMsg, wParam, lParam);
	}
	return FALSE;
}

DWORD GetCodePageType(HWND hWnd)
{
	DWORD result = MAX32;

	if ( PPxDialogBoxParam(hInst, MAKEINTRESOURCE(IDD_PASTETYPE),
			hWnd, GetCodePageDialog, (LPARAM)&result) > 0 ){
		return result;
	}else{
		return MAX32;
	}
}
