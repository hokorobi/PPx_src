/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library		Convert
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include "PPX.H"
#include "PPX_64.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#pragma hdrstop

#ifndef RELEASE
int CheckCodeTable = 1;
#endif

static CTCHAR UnitD[8] = T("kMGTPEZ");
static CTCHAR UnitH[8] = T("KMGTPEZ");

										/* エイリアス */
typedef struct {
	const TCHAR *str;	// ラベル名
	int num;			// 値
} label;

#define EKEY_INTERNAL 82
#define EKEY_MAX 134
label s_ekey[EKEY_MAX + 1] = {
// キーボードのキー名称
//01 マウスボタン
	{T("LMB"),		K_v | VK_LBUTTON},
	{T("RMB"),		K_v | VK_RBUTTON},
	{T("CANCEL"),	K_v | VK_CANCEL},	// ?
	{T("MMB"),		K_v | VK_MBUTTON},
	{T("XMB"),		K_v | VK_XBUTTON1},
	{T("YMB"),		K_v | VK_XBUTTON2},
//08
	{T("BS"),		K_v | VK_BACK},
	{T("TAB"),		K_v | VK_TAB},
//0c
	{T("CLEAR"),	K_v | VK_CLEAR},
	{T("ENTER"),	K_v | VK_RETURN},
	{T("RETURN"),	K_v | VK_RETURN},
//10
	{T("SHIFT"),	K_v | VK_SHIFT},
	{T("CTRL"),		K_v | VK_CONTROL},
	{T("ALT"),		K_v | VK_MENU},
	{T("GRPH"),		K_v | VK_MENU},
	{T("PAUSE"),	K_v | VK_PAUSE},
	{T("CAPS"),		K_v | VK_CAPITAL},
	{T("KANA"),		K_v | VK_KANA},
//19
	{T("KANJI"),	K_v | VK_KANJI},
//1b
	{T("ESC"),		K_v | VK_ESCAPE},
	{T("XFER"),		K_v | VK_CONVERT},
	{T("NFER"),		K_v | VK_NONCONVERT},
//	{T("ACCEPT"),	K_v | VK_ACCEPT},
///	{T("MODECHANGE"), K_v | VK_MODECHANGE},
//20
	{T("SPACE"),	K_v | VK_SPACE},
	{T("PUP"),		K_v | VK_PRIOR},
	{T("PDOWN"),	K_v | VK_NEXT},
	{T("END"),		K_v | VK_END},
	{T("HOME"),		K_v | VK_HOME},
	{T("LEFT"),		K_v | VK_LEFT},
	{T("UP"),		K_v | VK_UP},
	{T("RIGHT"),	K_v | VK_RIGHT},
	{T("DOWN"),		K_v | VK_DOWN},
	{T("SELECT"),	K_v | VK_SELECT},
//2b
	{T("EXEC"),		K_v | VK_EXECUTE},
	{T("COPY"),		K_v | VK_SNAPSHOT},
	{T("INS"),		K_v | VK_INSERT},
	{T("DEL"),		K_v | VK_DELETE},
	{T("HELP"),		K_v | VK_HELP},
//5b
	{T("LWIN"),		K_v | VK_LWIN},
	{T("RWIN"),		K_v | VK_RWIN},
	{T("APPS"),		K_v | VK_APPS},
//60
	{T("NUM0"),		K_v | VK_NUMPAD0},
	{T("NUM1"),		K_v | VK_NUMPAD1},
	{T("NUM2"),		K_v | VK_NUMPAD2},
	{T("NUM3"),		K_v | VK_NUMPAD3},
	{T("NUM4"),		K_v | VK_NUMPAD4},
	{T("NUM5"),		K_v | VK_NUMPAD5},
	{T("NUM6"),		K_v | VK_NUMPAD6},
	{T("NUM7"),		K_v | VK_NUMPAD7},
	{T("NUM8"),		K_v | VK_NUMPAD8},
	{T("NUM9"),		K_v | VK_NUMPAD9},
	{T("NUM*"),		K_v | VK_MULTIPLY},
	{T("NUM+"),		K_v | VK_ADD},
	{T("SEPARATOR"), K_v | VK_SEPARATOR},
	{T("NUM-"),		K_v | VK_SUBTRACT},
	{T("NUM."),		K_v | VK_DECIMAL},
	{T("NUM/"),		K_v | VK_DIVIDE},
//70
	{T("F1"),		K_v | VK_F1},
	{T("F2"),		K_v | VK_F2},
	{T("F3"),		K_v | VK_F3},
	{T("F4"),		K_v | VK_F4},
	{T("F5"),		K_v | VK_F5},
	{T("F6"),		K_v | VK_F6},
	{T("F7"),		K_v | VK_F7},
	{T("F8"),		K_v | VK_F8},
	{T("F9"),		K_v | VK_F9},
	{T("F10"),		K_v | VK_F10},
	{T("F11"),		K_v | VK_F11},
	{T("F12"),		K_v | VK_F12},
	{T("F13"),		K_v | VK_F13},
	{T("F14"),		K_v | VK_F14},
	{T("F15"),		K_v | VK_F15},
	{T("F16"),		K_v | VK_F16},
	{T("F17"),		K_v | VK_F17},
	{T("F18"),		K_v | VK_F18},
	{T("F19"),		K_v | VK_F19},
	{T("F20"),		K_v | VK_F20},
	{T("F21"),		K_v | VK_F21},
	{T("F22"),		K_v | VK_F22},
	{T("F23"),		K_v | VK_F23},
	{T("F24"),		K_v | VK_F24},
//90
	{T("NUMLOCK"),	K_v | VK_NUMLOCK},
	{T("SCROLL"),	K_v | VK_SCROLL},
//a0-a5:GetAsyncKeyState()/GetKeyState()のみで使われる。
//F0:英数

// 内蔵コマンド (EKEY_INTERNAL)
	{T("C_WIND"),	KC_WIND},

	{T("SSAVER"),	K_SSav},
	{T("NETUSE"),	K_ANDV},
	{T("NETUNUSE"),	K_FNDV},

	{T("PWTOP"),	KC_PTOP},
	{T("PWBOTTOM"),	KC_PBOT},
	{T("TOGGLEVFS"), KC_Tvfs},
	{T("C_DIR"),	KC_Edir},

	{T("LAYOUT"),	K_layout},

//	{T("FIRSTXEVENT"),	K_E_FIRSTX},
//	{T("EXITEVENT"),	K_E_EXIT},
	{T("FIRSTEVENT"),	K_E_FIRST},
	{T("CLOSEEVENT"),	K_E_CLOSE},
	{T("LOADEVENT"),	K_E_LOAD},
	{T("SELECTEVENT"),	K_E_SELECT},
	{T("ACTIVEEVENT"),	K_E_ACTIVE},
//	{T("INACTIVEEVENT"), K_E_INACTIVE},
//	{T("TABLETMODEEVENT"),	K_E_TABLET},
//	{T("PCMODEEVENT"), K_E_PC},

	{T("RANGEEVENT1"),	K_E_RANGE1},
	{T("RANGEEVENT2"),	K_E_RANGE2},
	{T("RANGEEVENT3"),	K_E_RANGE3},
	{T("RANGEEVENT4"),	K_E_RANGE4},
	{T("RANGEEVENT5"),	K_E_RANGE5},
	{T("RANGEEVENT6"),	K_E_RANGE6},
	{T("RANGEEVENT7"),	K_E_RANGE7},
	{T("RANGEEVENT8"),	K_E_RANGE8},

	{T("COMMENTEVENT"),		K_E_COMMENT},
	{T("COMMENTEVENT1"),	K_E_COMMENT1},
	{T("COMMENTEVENT2"),	K_E_COMMENT2},
	{T("COMMENTEVENT3"),	K_E_COMMENT3},
	{T("COMMENTEVENT4"),	K_E_COMMENT4},
	{T("COMMENTEVENT5"),	K_E_COMMENT5},
	{T("COMMENTEVENT6"),	K_E_COMMENT6},
	{T("COMMENTEVENT7"),	K_E_COMMENT7},
	{T("COMMENTEVENT8"),	K_E_COMMENT8},
	{T("COMMENTEVENT9"),	K_E_COMMENT9},
	{T("COMMENTEVENT10"),	K_E_COMMENT10},

	{T("WTOP"),		K_WTOP},
	{T("WBOTTOM"),	K_WBOT},
	{T("LOGOFF"),	K_Loff},
	{T("POWEROFF"),	K_Poff},
	{T("REBOOT"),	K_Rbt},
	{T("SHUTDOWN"),	K_Sdw},
	{T("TERMINATE"), K_Fsdw},
	{T("SUSPEND"),	K_Suspend},
	{T("HIBERNATE"), K_Hibernate},
	{T("ABOUT"),	K_about},
	{T("SUPPORT"),	K_supot},
	{T("LOADCUST"),	K_Lcust},
	{T("SAVECUST"),	K_Scust},
	{T("CUSTOMIZE"), K_cust},
	{T("LOADVFS"),	K_Lvfs},
	{T("FREEVFS"),	K_Fvfs},

	{T("--"),		K_MSEP},
	{T("||"),		K_MBRK},
	{T("NULL"),		K_NULL},
// EKEY_MAX
	{NilStr, 0}
};

COLORLABEL guicolor[] = {
	{T("_BLA"), C_BLACK},
	{T("_BLU"), C_BLUE},
	{T("_RED"), C_RED},
	{T("_MAG"), C_MAGENTA},

	{T("_GRE"), C_GREEN},
	{T("_CYA"), C_CYAN},
	{T("_BRO"), C_YELLOW},
	{T("_WHI"), C_WHITE},

	{T("_DBLA"), C_DBLACK},
	{T("_DBLU"), C_DBLUE},
	{T("_DRED"), C_DRED},
	{T("_DMAG"), C_DMAGENTA},

	{T("_DGRE"), C_DGREEN},
	{T("_DCYA"), C_DCYAN},
	{T("_DBRO"), C_DYELLOW},
	{T("_DWHI"), C_DWHITE},

	{T("_MGRE"), C_MGREEN},
	{T("_SBLU"), C_SBLUE},
	{T("_CREM"), C_CREAM},
	{T("_GRAY"), C_GRAY},

	{T("_AUTO"), C_AUTO}
};

/*-----------------------------------------------------------------------------
								キーのエイリアス
-----------------------------------------------------------------------------*/
#ifdef UNICODE
#define upperA(c) (char)upper((TCHAR)c)
int USEFASTCALL GetHexCharA(char **p)
{
	int i;

	if ( !IsxdigitA( i = upperA(**p)) ) return -1000;
	(*p)++;
	if ( (i -= '0') > 9 ) i -= 7;
	return i;
}
#else
#define GetHexCharA GetHexChar
#endif
/*-----------------------------------------------------------------------------
	大文字に変換する
-----------------------------------------------------------------------------*/
TCHAR USEFASTCALL upper(TCHAR c)
{
	if ( Islower(c) ) c -= (TCHAR)0x20;
	return c;
}
/*-----------------------------------------------------------------------------
	10進文字列を取得する
-----------------------------------------------------------------------------*/
INT_PTR GetDigitNumber(const TCHAR **ptr)
{
	INT_PTR n = 0;

	while( Isdigit(**ptr) ){
		n = n * 10 + (UTCHAR)((UTCHAR)*(*ptr)++ - (UTCHAR)'0');
	}
	return n;
}
/*-----------------------------------------------------------------------------
	16進文字列を取得する
	※GetNumberで16進を入手する場合に用いられる
-----------------------------------------------------------------------------*/
INT_PTR USEFASTCALL GetHexNumber(LPCTSTR *ptr)
{
	TCHAR	Ctype;
	UTCHAR	c;
	INT_PTR n = 0;

	for ( ; ; ){
		c = **ptr;
#ifdef UNICODE
		if ( c > 'f' ) break;
#endif
		Ctype = T_CHRTYPE[c];
		if ( !(Ctype & (T_IS_DIG | T_IS_HEX)) ) break;	// 0-9,A-F,a-f ではない
		if ( Ctype & T_IS_LOW ) c = (UTCHAR)(c - 0x20);	// 小文字を大文字に
		if ( !(Ctype & T_IS_DIG) ) c = (UTCHAR)(c - 7);	// A-F の処理
		c = (UTCHAR)(c - '0');
		n = (n << 4) + c;
		(*ptr)++;
	}
	return n;
}
// １文字１６進数を数値化 -----------------------------------------------------
int USEFASTCALL GetHexChar(TCHAR **p)
{
	int i;

	if (!Isxdigit( i = upper(**p))) return -1000;
	(*p)++;
	if ( (i -= '0') > 9 )	i -= 7;
	return i;
}
/*-----------------------------------------------------------------------------
	値を入手。入手できない時は、para が変化しない
-----------------------------------------------------------------------------*/
#ifdef UNICODE
INT_PTR GetNumberA(const char **line)
{
	INT_PTR n = 0;

	while( Isdigit(**line) ){
		n = n * 10 + (INT_PTR)(BYTE)((BYTE)*(*line)++ - (BYTE)'0');
	}
	return n;
}
#endif

PPXDLL INT_PTR PPXAPI GetNumber(LPCTSTR *ptr)
{
#define NUM_GOT		B0
#define NUM_MINUS	B1
#define NUM_NOT		B2
	int flag = 0;	// b0:1=入手成功  b1:-  b2:NOT
	INT_PTR n = 0;
	UTCHAR c;
	const UTCHAR *p;

	p = (UTCHAR *)*ptr;
//------------------------------------- 一項演算子の処理
	for ( ; ; ){
		SkipSPC(p);
		switch( *p ){
			case '~':		// 全ビットの反転
				p++;
				setflag(flag, NUM_NOT);
				continue;
			case '-':		// 負数
				p++;
				setflag(flag, NUM_MINUS);
				continue;
			case '+':		// 正数
				p++;
				break;
		}
		break;
	}
	c = *p;
	CharUPR(c);
														// Hnnnn 形式(16) -----
	if ( (c == 'H') && Isxdigit(*(p + 1)) ){
		p++;
		setflag(flag, NUM_GOT);
		n = GetHexNumber((const TCHAR **)&p);
														// Bnnnn 形式(2) ------
	}else if ( (c == 'B') && ((UTCHAR)(*(p+1) - '0') < 2) ){
		setflag(flag, NUM_GOT);
		while(*(++p)){
			c = (UTCHAR)(*p - '0');
			if ( c >= 2 ) break;
			n = (n << 1) + c;
		}
	}else if ( c == '\'' ){
														// 'あ' 形式(char) ----
#ifndef UNICODE
		if ( IskanjiA(*(p+1)) && *(p+2) && (*(p+3) == '\'') ){
			setflag(flag, NUM_GOT);
			n = *(WORD *)(p + 1);
			p += 4;
														// 'A' 形式(char) -----
		}else
#endif
		 if ( *(p+1) && (*(p+2) == '\'') ){
			setflag(flag, NUM_GOT);
			n = *(p + 1);
			p += 3;
		}
														// 0xnnnn 形式(16) ----
	}else if ( (c == '0') &&
			((*(p+1) == 'X') || (*(p+1) == 'x')) && Isxdigit(*(p+2)) ){
		p += 2;
		setflag(flag, NUM_GOT);
		n = GetHexNumber((const TCHAR **)&p);
	}else{												// nnnn 形式(10) ------
		while( Isdigit(*p) ){
			setflag(flag, NUM_GOT);
			n = n * 10 + (INT_PTR)(UTCHAR)(*p++ - (UTCHAR)'0');
		}
	}
	if ( flag & NUM_GOT ){
		if ( flag & NUM_MINUS ) n = -n;
		if ( flag & NUM_NOT   ) n = ~n;
		*ptr = (TCHAR *)p;
	}
	return n;
}

PPXDLL BOOL PPXAPI GetSizeNumber(LPCTSTR *ptr, DWORD *Low, DWORD *High)
{
	INT_PTR basesize;
	DWORD scaleL = 0, scaleH = 0;
	const TCHAR *oldp;

	oldp = *ptr;
	basesize = GetNumber(ptr);
	if ( oldp == *ptr ) return FALSE;
	if ( Isalpha(**ptr) ) switch (*(*ptr)++){
		case 'k':
			scaleL = KBd;
			break;
		case 'K':
			scaleL = KB;
			break;
		case 'm':
			scaleL = MBd;
			break;
		case 'M':
			scaleL = MB;
			break;
		case 'g':
			scaleL = GBd;
			break;
		case 'G':
			scaleL = GB;
			break;
		case 't':
			scaleL = TBdL;
			scaleH = (DWORD)basesize * TBdH;
			break;
		case 'T':
			if ( High != NULL ) *High = (DWORD)basesize * TBiH;
			*Low = 0; // TBiL == 0 のため
			return TRUE;
/*
		case 'p':
			scaleL = PBdL;
			scaleH = (DWORD)basesize * PBdH;
			break;
		case 'P':
			if ( High != NULL ) *High = (DWORD)basesize * PBiH;
			*Low = 0; // PBiL == 0 のため
			return TRUE;
*/
		default:
			(*ptr)--;
			break;
	}
	if ( scaleL == 0 ){
#ifdef _WIN64
		if ( High != NULL ) *High = (DWORD)(INT_PTR)(basesize >> 32);
#else
		if ( High != NULL ) *High = 0;
#endif
		*Low = (DWORD)basesize;
	}else{
		DWORD sizeL, sizeH;

#ifdef _WIN64
		basesize *= (INT_PTR)scaleL;
		sizeL = (DWORD)basesize;
		sizeH = (DWORD)(INT_PTR)(basesize >> 32) + scaleH;
#else
		DDmul(basesize, scaleL, &sizeL, &sizeH);
		if ( scaleH != 0 ) sizeH += scaleH;
#endif
		if ( High != NULL ) *High = sizeH;
		*Low = sizeL;
	}
	return TRUE;
}
/*-----------------------------------------------------------------------------
	シフトキーを得る
-----------------------------------------------------------------------------*/
int USEFASTCALL GetShiftCode(const TCHAR **ptr)
{
	int shift = 0;			/* シフトフラグ */
	const TCHAR	*p;
	TCHAR c;

	p = *ptr;
	SkipSpace(&p);
	for ( ; ; ){
		c = *p;
		switch(c){
			case '@':			/* エイリアス禁止 */
				shift |= K_raw;
				p++;
				continue;
			case '\\':			/* Shift */
				shift |= K_s;
				p++;
				continue;
			case '^':			/* Ctrl */
				shift |= K_c;
				p++;
				continue;
			case '&':			/* Alt / Grph */
				shift |= K_a;
				p++;
				continue;
			case '~':			/* ExShift */
				shift |= K_e;
				p++;
				continue;
		}
		break;
	}
	*ptr = p;
	return shift;
}
/*-----------------------------------------------------------------------------
	シフトキー文字列を得る
-----------------------------------------------------------------------------*/
PPXDLL TCHAR * PPXAPI PutShiftCode(TCHAR *str, int key)
{
	if (key & K_raw) *str++ = '@';
	if (key & K_e) *str++ = '~';
	if (key & K_a) *str++ = '&';
	if (key & K_c) *str++ = '^';
	if (key & K_s) *str++ = '\\';
	*str = '\0';
	return str;
}
/*-----------------------------------------------------------------------------
	キーコードを得る
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI GetKeyCode(LPCTSTR *ptr)
{
	int shift;			/* シフトフラグ */
	int brk = 0;		/* '' 使用中フラグ */
	int key, i;			/* 現在のキーコード */
	TCHAR buf[256], *buf_p;
	const TCHAR *lp, *lp_bak;

	shift = GetShiftCode(ptr);
	lp = *ptr;
	key = *lp++;
								// 数値指定 ===================================
	if ( (key == 'V') && (*lp == '_') ){
		*ptr = lp + 1;
		return GetIntNumber(ptr) | shift | K_v;
	}
								// ブラケットを使用するか？ ===================
	if (key == '\'' ){
		brk = 1;
		key = *lp++;
	}
								// 行端チェック ===============================
	if ( key == 0 ) return -1;
	if ( (brk == 0) && (key == ',') ) return -2;

								// キーの名称の部分を分離 =====================
	buf_p = buf;
	lp_bak = lp;
	while( buf_p < (buf + TSIZEOF(buf) - 1) ){
		*buf_p++ = upper((TCHAR)key);
		key = *lp++;
		if (brk){				/* ブラケット有り */
			if ( key == '\'' || key < ' ' ){
				lp_bak = lp;
				break;
			}
		}else{					/* ブラケット無し */
			if ( !( Isalnum(key) || (key == '_') ||
				 ((key >= '*') && (key <= '/') && (key != ','))) ){
				lp--;
				break;
			}
		}
	}
	*buf_p = '\0';
	key = buf[0];
	if ( buf[1] ){				/* ２文字以上ならエイリアス判断を行う ********/
		for ( i = 0 ; s_ekey[i].num ; i++ ){
			if ( !tstrcmp(buf, s_ekey[i].str) ){
				key = s_ekey[i].num;
				break;
			}
		}
		if (!s_ekey[i].num) lp = lp_bak;	/* エイリアスが見つからなかった */
	}
	*ptr = lp;
	return key | shift;
}
/*-----------------------------------------------------------------------------
	キー文字列を得る
-----------------------------------------------------------------------------*/
PPXDLL void PPXAPI PutKeyCode(TCHAR *str, int key)
{
#ifndef RELEASE
	if ( CheckCodeTable ){
		int i, num, oldnum = 0;
		CheckCodeTable = 0;
		for ( i = 0 ; s_ekey[i].num ; i++ ){
			num = s_ekey[i].num;
			if ( (num & K_e) != (oldnum & K_e) ){
				if ( i != EKEY_INTERNAL ){
					XMessage(NULL, NULL, XM_DbgDIA, T("PutKeyCode - EKEY_INTERNAL error : %d"), i);
				}
			}
			if ( num < oldnum ){
				XMessage(NULL, NULL, XM_DbgDIA, T("PutKeyCode - s_ekey error : %d %s"), i, s_ekey[i].str);
			}
			oldnum = num;
		}
		if ( i != EKEY_MAX ){
			XMessage(NULL, NULL, XM_DbgDIA, T("PutKeyCode - 	EKEY_MAX error : %d"), i);
		}
	}
#endif
	if ( !(key & (K_ex | K_internal)) ){	// 一般キーならシフトキーを解析
		str = PutShiftCode(str, key);
		resetflag(key, K_raw | K_e | K_a | K_c | K_s);
	}
	if ( key & (K_v | K_ex) ){
		int mini, maxi;
		if ( key & K_ex ){
			mini = EKEY_INTERNAL;
			maxi = EKEY_MAX;
		}else{
			mini = 0;
			maxi = EKEY_INTERNAL;
		}
		while ( mini < maxi ){
			int mid, result;

			mid = (mini + maxi) / 2;
			result = s_ekey[mid].num - key;
			if ( result < 0 ){
				mini = mid + 1;
			}else if ( result > 0 ){
				maxi = mid;
			}else{
				if ( (mid > 0) && (s_ekey[mid - 1].num == key ) ) mid--;
				tstrcpy(str, s_ekey[mid].str);
				return;
			}
		}
		wsprintf(str, T("V_H%02X"), key & ~K_v);
	}else if ( IsalnumA(key) ){
		wsprintf(str, T("%c"), key); // 8bit に丸められる
	}else{
		wsprintf(str, T("\'%c\'"), key); // 8bit に丸められる
	}
}

DWORD DDShift(DWORD low, DWORD high, DWORD shift)
{
	if ( shift < 32 ){
		return (low >> shift) + (high << (32 - shift));
	}else{
		return high >> (shift - 32);
	}
}

int USEFASTCALL HexFormat(TCHAR *buf, int unit, DWORD low, DWORD high)
{
	int shift;

	shift = unit * 10;
	if ( shift > 32 ){
		low = high >> (shift - 32);
	}else{
		low = (low >> shift) + (high << (32 - shift));
		high = high >> shift;

		if ( high ){
#ifndef _WIN64 // 32bit
			DWORD low2, high2;

			DDwordToDten(low, high, &low2, &high2);
			return wsprintf(buf, T("%u%09u"), high2, low2);
#else // 64bit
			DWORD_PTR l64;

			l64 = (DWORD_PTR)low + ((DWORD_PTR)high << 32);
			return wsprintf(buf, T("%I64u"), l64);
#endif
		}
	}
	return wsprintf(buf, T("%u"), low);
}

/*-----------------------------------------------------------------------------
	数値を所定の10進文字列に変換する

	str 出力先
-----------------------------------------------------------------------------*/
PPXDLL void PPXAPI FormatNumber(TCHAR *str, DWORD flags, int length, DWORD low, DWORD high)
{
	TCHAR buf[0x20], unitname = '\0', *src, *dest;
	int vlen; // 数値部(数字+',')の実際の長さ
	int max_vlen; // 数値部の最大長

	max_vlen = length;
	if ( flags & XFN_MUL ) DDmul(low, high, &low, &high);
	if ( flags & XFN_UNITSPACE ){ // 単位桁を確保
		unitname = ' ';
		max_vlen--;
	}
										// 桁区切り分だけ桁数を減らす
	if ( flags & XFN_SEPARATOR ) max_vlen -= max_vlen >> 2; // 4桁毎に桁区切り

	// 10進文字列化
	if ( high != 0 ){
#ifndef _WIN64 // 32bit
		DWORD low2, high2;

		DDwordToDten(low, high, &low2, &high2);
		vlen = wsprintf(buf, T("%u%09u"), high2, low2);
#else // 64bit
		DWORD_PTR l64;

		l64 = (DWORD_PTR)low + ((DWORD_PTR)high << 32);
		vlen = wsprintf(buf, T("%I64u"), l64);
#endif
	}else{
		vlen = wsprintf(buf, T("%u"), low);
	}

	// 単位付き処理
	if ( (vlen > max_vlen) || (flags & XFN_MINUNITMASK) ){
		int overunits;
		int minunit;

		if ( unitname == '\0' ){		// 単位の文字を確保
			if ( flags & XFN_SEPARATOR ){ // カンマありの時は桁数を再計算
				max_vlen = length - 1;
				max_vlen -= max_vlen >> 2;
			}else{
				max_vlen--;
			}
		}
														// 減らす桁を算出
		overunits = (vlen - max_vlen - 1) / 3; // 単位の数を求める
		minunit = (flags & XFN_MINUNITMASK) >> XFN_MINUNITSHIFT;
		if ( minunit > (overunits + 1) ) overunits = minunit - 1;

		if ( flags & XFN_HEXUNIT ){ // 1024単位
			int deci = 0;
			// 仮減らし
			vlen = HexFormat(buf, overunits + 1, low, high);

			if ( (flags & XFN_DECIMALPOINT) && (max_vlen >= 4) ){
				overunits += (vlen - 1) / 3; // 単位の数を求める
				vlen = HexFormat(buf, overunits + 1, low, high);
				resetflag(flags,XFN_SEPARATOR);
				deci = 1;
			}else if ( (overunits > minunit) && ((max_vlen - vlen) >= 3) && (buf[0] == '9') && (buf[1] < '7') ){ // 何とか収まるので調整
				overunits--;
				vlen = HexFormat(buf, overunits + 1, low, high);
			}
			unitname = UnitH[overunits];
			if ( deci || ((vlen < 2) && (max_vlen >= 2)) ){ // 小数点1桁
				buf[vlen] = NumberDecimalSeparator;
				buf[vlen + 1] = (TCHAR)('0' + (((DDShift(low, high,
						overunits * 10) & 0x3ff) * 10) >> 10));
				if ( (vlen == 1) && (buf[0] == '0') && (buf[2] == '0') && low ){
					buf[vlen + 1] = '1';
				}
				vlen += 2;
			}
			buf[vlen] = '\0';
		}else{ // 1000単位
			// 小数桁数を決定
			if ( (length >= 6) && ((flags & XFN_DECIMALPOINT) || (length <= 7)) ){
				// 6桁:小数1桁(123.4k) 7以上=小数2桁(123.45k)
				int deci = (length >= 7) ? 2 : 1;

				overunits = (vlen < 7) ? 0 : (vlen - 4) / 3; // 単位の数を求める
				resetflag(flags,XFN_SEPARATOR);
				vlen -= (overunits + 1) * 3;
				if ( vlen <= 0 ){ // 0.xx になる場合
					if ( deci == 2 ){
						switch ( vlen ){
							case 0: // "ab"→"0ab"
								if ( (buf[0] == '0') && (buf[1] <= '0') ){
									buf[1] = low ? (TCHAR)'1' : (TCHAR)'0';
								}
								buf[2] = buf[1];
								buf[1] = buf[0];
								break;
							case -1: // "a"→"00a"
								buf[2] = low ? (TCHAR)'1' : buf[0];
								buf[1] = '0';
								break;
							default: // ""→"000"
								buf[2] = low ? (TCHAR)'1' : (TCHAR)'0';
								buf[1] = '0';
						}
					}else{
						if ( vlen == 0 ){ // "a"→"0a"
							if ( buf[0] == '0' ){
								buf[1] = low ? (TCHAR)'1' : (TCHAR)'0';
							}else{
								buf[1] = buf[0];
							}
						}else{ // ""→"00"
							buf[1] = low ? (TCHAR)'1' : (TCHAR)'0';
						}
					}
					buf[0] = '0';
					vlen = 1;
				}
				// 1234 → 12.34 / 12.3 にずらす
				if ( deci > 1 ) buf[vlen + 2] = buf[vlen + 1]; // 小数2桁
				buf[vlen + 1] = buf[vlen];
				buf[vlen] = NumberDecimalSeparator;
				vlen += deci + 1;
			}else{ // 小数点無し
				vlen -= (overunits + 1) * 3;
				if ( vlen <= 0 ){
					buf[0] = low ? (TCHAR)'1' : (TCHAR)'0';
					vlen = 1;
				}else if ( (vlen < 2) && (max_vlen >= 2) ){
					buf[vlen + 1] = buf[vlen];
					buf[vlen] = NumberDecimalSeparator;
					vlen += 2;
				}
			}
			buf[vlen] = '\0';
			unitname = UnitD[overunits];
		}
	}

	src = buf;
	dest = str;

	if ( flags & XFN_RIGHT ){ // 右詰用の空白を挿入
		int leftlen;

		leftlen = vlen;
		if ( flags & XFN_SEPARATOR ) leftlen += (vlen - 1) / 3;
		if ( unitname != '\0' ) leftlen++;
		for ( ; leftlen < length ; leftlen++ ) *dest++ = ' ';
	}

	if ( flags & XFN_SEPARATOR ){	// 区切り付きあり
		int tag;
		TCHAR UnitSeparatorChar;

		for ( tag = (vlen - 1) % 3 + 1; tag ; tag-- ) *dest++ = *src++;
		UnitSeparatorChar = NumberUnitSeparator;
		while( *src != '\0' ){
			*dest++ = UnitSeparatorChar;
			*dest++ = *src++;
			*dest++ = *src++;
			*dest++ = *src++;
		}
	}else{
		while ( *src != '\0' ) *dest++ = *src++;
	}
	if ( unitname != '\0' ) *dest++ = unitname;
	if ( flags & XFN_LEFT ){ // 左詰用の空白を挿入
		int leftpos;

		for ( leftpos = vlen ; leftpos < length ; leftpos++ ) *dest++ = ' ';
	}
	*dest = '\0';
	return;
}

// 色設定 ---------------------------------------------------------------------
PPXDLL COLORREF PPXAPI GetColor(LPCTSTR *linesrc, BOOL usealias)
{
	TCHAR buf[100], *dst, data;
	const TCHAR *pt;
	int i;
	COLORREF color;
										// キーワードを抽出
	SkipSpace(linesrc);
	pt = *linesrc;
	dst = buf;
	while((UTCHAR)(data = *pt++) >= (UTCHAR)'A' ) *dst++ = data;
	*dst = '\0';
										// 追加識別子を判別
	if ( usealias ){	// A_color は自己参照させない
		if ( NO_ERROR == GetCustTable(T("A_color"), buf, &color, sizeof(color)) ){
			*linesrc = pt - 1;
			return color;
		}
	}
										// 定義済み識別子を判別
	for ( i = 0 ; i < color_s ; i++ ){
		if ( !tstrcmp(buf, guicolor[i].str) ){
			*linesrc = pt - 1;
			return guicolor[i].num;
		}
	}
										// 数字指定
	return GetDwordNumber(linesrc);
}

// Win95等の CP_UTF8 未対応環境用
#ifndef _WIN64
int WINAPI MultiByteToWideCharU8_Win4(UINT CodePage, DWORD dwFlags, const char *lpMultiByteStr, int cchMultiByte, WCHAR *lpWideCharStr, int cchWideChar)
{
	if ( CodePage == CP_UTF8 ){
		const char *src = lpMultiByteStr, *srcmax = lpMultiByteStr + cchMultiByte;
		if ( cchWideChar == 0 ){
			int count = (cchMultiByte < 0) ? 1 : 0;

			for (;;){
				WORD c;

				if ( cchMultiByte < 0 ){
					if ( (c = *src) == '\0' ) break;
				}else{
					if ( src >= srcmax ) break;
					c = *src;
				}

				if ( (cchMultiByte < 0) ? (c == '\0') : (src >= srcmax) ) break;
				if ( c < 0x80 ){
					src++;
				}else if ( (c & 0xe0) == 0xc0 ){ // 2bytes 0x80-7ff
					src += 2;
				}else if ( (c & 0xf0) == 0xe0 ){ // 3bytes 0x800-ffff
					src += 3;
				}else if ( (c & 0xf8) == 0xf0 ){ // 4bytes 10000-1fffff
					src += 4;
					count += 2;
/* 現在、規格で使用されていない
				}else if ( (c & 0xfc) == 0xf8 ){
				}else if ( (c & 0xfe) == 0xfc ){
*/
				}else{ // c == 0xfe or 0xff
					src++;
				}
				count++;
			}
			return count;
		}else{
			WCHAR *dst = lpWideCharStr;
			WCHAR *dstmax = lpWideCharStr + cchWideChar - 1;

			for (;;){
				WORD c;

				if ( cchMultiByte < 0 ){
					if ( (c = *src) == '\0' ) break;
				}else{
					if ( src >= srcmax ) break;
					c = *src;
				}
				if ( dst >= dstmax ) break;
				if ( c < 0x80 ){
					src++;
				}else if ( (c & 0xe0) == 0xc0 ){ // 2bytes 0x80-7ff
					c = (WORD)((( c & 0x1f )	<<  6) |
						(*(src + 1) & 0x3f));
					src += 2;
				}else if ( (c & 0xf0) == 0xe0 ){ // 3bytes 0x800-ffff
					c = (WORD)((( c & 0xf )		<< 12) |
						((*(src + 1) & 0x3f )	<<  6) |
						(*(src + 2) & 0x3f ));
					src += 3;
				}else if ( (c & 0xf8) == 0xf0 ){ // 4bytes 10000-1fffff
					DWORD dwc;

					if ( (dst + 1) >= dstmax ) break;
					dwc = (DWORD)((( c & 7 )	<< 18) |
						((*(src + 1) & 0x3f )	<< 12) |
						((*(src + 2) & 0x3f )	<<  6) |
						(*(src + 3) & 0x3f ));
					src += 4;
					// サロゲートペアに変換
					*dst++ = (WORD)(DWORD)((dwc >> 10) + (0xd800 - (0x10000 >> 10)));
					c   = (WORD)(DWORD)((dwc & 0x3ff) | 0xdc00);
/* 現在、規格で使用されていない
				}else if ( (c & 0xfc) == 0xf8 ){
					c = (WORD)((( c & 3 )	<< 24) |
						((*p & 0x3f )		<< 18) |
						((*(p + 1) & 0x3f )	<< 12) |
						((*(p + 2) & 0x3f )	<<  6) |
						(*(p + 3) & 0x3f ));
					p += 4;
				}else if ( (c & 0xfe) == 0xfc ){
					c = (WORD)((( c & 1 )	<< 30) |
						((*p & 0x3f )		<< 24) |
						((*(p + 1) & 0x3f )	<< 18) |
						((*(p + 2) & 0x3f )	<< 12) |
						((*(p + 3) & 0x3f )	<<  6) |
						(*(p + 4) & 0x3f ));
					p += 5;
*/
				}else{ // c == 0xfe or 0xff
					src++;
				}
				*dst++ = c;
			}
			if ( cchMultiByte < 0 ){
				if ( dst >= dstmax ){
					*(dstmax - 1) = '\0';
				}else{
					*dst++ = '\0';
				}
			}
			return dst - lpWideCharStr;
		}
	}else{
		return MultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cchMultiByte, lpWideCharStr, cchWideChar);
	}
}

int WINAPI WideCharToMultiByteU8_Win4(UINT CodePage, DWORD dwFlags, const WCHAR *lpWideCharStr, int cchWideChar, char *lpMultiByteStr, int cchMultiByte, const char *lpDefaultChar, BOOL *lpfUsedDefaultChar)
{
	if ( CodePage == CP_UTF8 ){
		const WCHAR *src = lpWideCharStr, *srcmax = lpWideCharStr + cchWideChar;

		if ( cchMultiByte == 0 ){
			int count = (cchWideChar < 0) ? 1 : 0;

			for (;;){
				WORD c;

				if ( cchWideChar < 0 ){
					if ( (c = *src++) == '\0' ) break;
				}else{
					if ( src >= srcmax ) break;
					c = *src++;
				}

				if ( c < 0x80 ){
					count++;
					continue;
				}
				if ( c < 0x800 ){
					count += 2;
					continue;
				}
				if ( (c >= 0xd800) && (c < 0xdc00) ){ // サロゲートペア
					DWORD c2;

					c2 = (DWORD)*src;
					if ( (c2 >= 0xdc00) && (c2 < 0xe000) ){
						src++;
						count += 4;
						continue;
					}
				}
				{// if (c < 0x10000)
					count += 3;
					continue;
				}
			}
			return count;
		}else{
			char *dst, *dstmax;

			dst = lpMultiByteStr;
			dstmax = lpMultiByteStr + cchMultiByte;

			for (;;){
				WORD c;

				if ( cchWideChar < 0 ){
					if ( (c = *src++) == '\0' ) break;
				}else{
					if ( src >= srcmax ) break;
					c = *src++;
				}
				if ( c < 0x80 ){
					if ( (dst + 1) >= dstmax ) break;
					*dst++ = (BYTE)c;
					continue;
				}
				if ( c < 0x800 ){
					if ( (dst + 2) >= dstmax ) break;
					*dst++ = (BYTE)(((c >> 6) & 0x1f) | 0xc0);
					*dst++ = (BYTE)((c & 0x3f) | 0x80);
					continue;
				}
				if ( (c >= 0xd800) && (c < 0xdc00) ){ // サロゲートペア
					DWORD c2;

					c2 = (DWORD)*src;
					if ( (c2 >= 0xdc00) && (c2 < 0xe000) ){
						if ( (dst + 4) >= dstmax ) break;

						src++;
						c2 = (c2 & 0x3ff) + ((c & 0x3ff) << 10) + 0x10000;
						*dst++ = (BYTE)(((c2 >> 18) &  0x7) | 0xf0);
						*dst++ = (BYTE)(((c2 >> 12) & 0x3f) | 0x80);
						*dst++ = (BYTE)(((c2 >>  6) & 0x3f) | 0x80);
						*dst++ = (BYTE)((c2 & 0x3f) | 0x80);
						continue;
					}
				}
				{// if (c < 0x10000)
					if ( (dst + 3) >= dstmax ) break;
					*dst++ = (BYTE)(((c >> 12) &  0xf) | 0xe0);
					*dst++ = (BYTE)(((c >>  6) & 0x3f) | 0x80);
					*dst++ = (BYTE)((c & 0x3f) | 0x80);
					continue;
				}
			}
			if ( cchWideChar < 0 ){
				if ( dst >= dstmax ){
					*(dstmax - 1) = '\0';
				}else{
					*dst++ = '\0';
				}
			}
			return dst - lpMultiByteStr;
		}
	}else{
		return WideCharToMultiByte(CodePage, dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cchMultiByte, lpDefaultChar, lpfUsedDefaultChar);
	}
}

int WINAPI MultiByteToWideCharU8_Init(UINT CodePage, DWORD dwFlags, const char *lpMultiByteStr, int cchMultiByte, WCHAR *lpWideCharStr, int cchWideChar)
{
	if ( WinType < WINTYPE_2000 ){
		DMultiByteToWideCharU8 = MultiByteToWideCharU8_Win4;
	}else{
		DMultiByteToWideCharU8 = (impMultiByteToWideCharU8)GetProcAddress(hKernel32, "MultiByteToWideChar");
	}
	return DMultiByteToWideCharU8(CodePage, dwFlags, lpMultiByteStr, cchMultiByte, lpWideCharStr, cchWideChar);
}

int WINAPI WideCharToMultiByteU8_Init(UINT CodePage, DWORD dwFlags, const WCHAR *lpWideCharStr, int cchWideChar, char *lpMultiByteStr, int cchMultiByte, const char *lpDefaultChar, BOOL *lpfUsedDefaultChar)
{
	if ( WinType < WINTYPE_2000 ){
		DWideCharToMultiByteU8 = WideCharToMultiByteU8_Win4;
	}else{
		DWideCharToMultiByteU8 = (impWideCharToMultiByteU8)GetProcAddress(hKernel32, "WideCharToMultiByte");
	}
	return DWideCharToMultiByteU8(CodePage, dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cchMultiByte, lpDefaultChar, lpfUsedDefaultChar);

}

ValueWinAPI(MultiByteToWideCharU8) = MultiByteToWideCharU8_Init;
ValueWinAPI(WideCharToMultiByteU8) = WideCharToMultiByteU8_Init;
#endif
