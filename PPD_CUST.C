/*-----------------------------------------------------------------------------
	Paper Plane xUI	customizer										Main
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPD_DEF.H"
#include "PPD_CUST.H"
#pragma hdrstop

#define MAXCUSTDATA 0x10000 // CustData で作成可能な最大値

// SMes buf の確保サイズ
#define FIRSTDUMPSIZE 0x12000 // CD で出力するテキストの初期サイズ
#define FIRSTSTORESIZE 0x800 // CS で出力するログの初期サイズ
#define SMESSIZE_MARGIN 64 // CheckSmes で指定したサイズに足す値。一部はみ出すため(文字列の後の追加改行とか)
#define SMESSIZE_MES 200 // 警告やエラーメッセージの予約サイズ

const TCHAR DefCommentStr[] = T("\t** comment **");
const char Nilfmt[] = "";

typedef struct {	// エイリアス ---------------------------------------------
	const TCHAR *str;	//	ラベル名
	int num;			//	値
} LABEL;

/*-----------------------------------------------------------------------------
								色エイリアス
-----------------------------------------------------------------------------*/
LABEL ConColor[] = {
	{T("_BLA"), 0},
	{T("_BLU"), FOREGROUND_BLUE  | FOREGROUND_INTENSITY},
	{T("_RED"), FOREGROUND_RED   | FOREGROUND_INTENSITY},
	{T("_MAG"), FOREGROUND_BLUE  | FOREGROUND_RED | FOREGROUND_INTENSITY},

	{T("_GRE"), FOREGROUND_GREEN | FOREGROUND_INTENSITY},
	{T("_CYA"), FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_INTENSITY},
	{T("_BRO"), FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_INTENSITY},
	{T("_WHI"), FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY},

	{T("_DBLA"), FOREGROUND_INTENSITY},
	{T("_DBLU"), FOREGROUND_BLUE},
	{T("_DRED"), FOREGROUND_RED},
	{T("_DMAG"), FOREGROUND_BLUE  | FOREGROUND_RED},

	{T("_DGRE"), FOREGROUND_GREEN},
	{T("_DCYA"), FOREGROUND_BLUE  | FOREGROUND_GREEN},
	{T("_DBRO"), FOREGROUND_RED   | FOREGROUND_GREEN},
	{T("_DWHI"), FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_RED},

	{T("R_BLA"), 0},
	{T("R_BLU"), BACKGROUND_BLUE  | BACKGROUND_INTENSITY},
	{T("R_RED"), BACKGROUND_RED   | BACKGROUND_INTENSITY},
	{T("R_MAG"), BACKGROUND_BLUE  | BACKGROUND_RED | BACKGROUND_INTENSITY},

	{T("R_GRE"), BACKGROUND_GREEN | BACKGROUND_INTENSITY},
	{T("R_CYA"), BACKGROUND_BLUE  | BACKGROUND_GREEN | BACKGROUND_INTENSITY},
	{T("R_BRO"), BACKGROUND_RED   | BACKGROUND_GREEN | BACKGROUND_INTENSITY},
	{T("R_WHI"), BACKGROUND_BLUE  | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY},

	{T("R_DBLA"), BACKGROUND_INTENSITY},
	{T("R_DBLU"), BACKGROUND_BLUE},
	{T("R_DRED"), BACKGROUND_RED},
	{T("R_DMAG"), BACKGROUND_BLUE  | BACKGROUND_RED},

	{T("R_DGRE"), BACKGROUND_GREEN},
	{T("R_DCYA"), BACKGROUND_BLUE  | BACKGROUND_GREEN},
	{T("R_DBRO"), BACKGROUND_RED   | BACKGROUND_GREEN},
	{T("R_DWHI"), BACKGROUND_BLUE  | BACKGROUND_GREEN | BACKGROUND_RED},

	{T("_TL"), 0x0400},	// 上線
	{T("_LL"), 0x0800},	// 左線
	{T("_RL"), 0x1000},	// 右線
	{T("_UL"), 0x8000},	// 下線
};
#define concolor_s 36

// ユーザ定義が可能な項目
const CLABEL usermenu =	{NULL, f_POPMENU, "=M", NilStr};	// メニュー
const CLABEL userext =	{NULL, f_EXTRUN, "/X", NilStr};	// ファイル判別
const CLABEL userlang =	{NULL, f_MSGS, "=s", NilStr};	// メッセージ
const CLABEL userbar =	{NULL, f_TOOLBAR, ",d4x", NilStr};	// ツールバー
const CLABEL userkey =	{NULL, f_KEY, "/X", NilStr};	// キー割当て
const CLABEL usersettings	= {NULL, fT, "=s", NilStr};	// 任意設定

typedef struct {
	ThSTRUCT UsersIndex, UsersNames;
	DWORD *UsersIndexMin, *UsersIndexMax;
} USERCUSTNAMES;

// Smes の空き容量の調整 ------------------------------------------------------
void CheckSmes(_Inout_ PPCUSTSTRUCT *PCS, size_t req_len)
{
	if ( (PCS->Smes + req_len + SMESSIZE_MARGIN) >= PCS->SmesLim ){
		TCHAR *newptr;
		size_t size;

		size = TSTROFF(PCS->SmesLim - PCS->SmesBuf + req_len);
		size += ThNextAllocSizeM(size);
		if ( PCS->Xmode == PPXCUSTMODE_DUMP_PART ){
			newptr = HeapAlloc(ProcHeap, 0, size + SMESSIZE_MARGIN);
			if ( newptr != NULL ){
				PCS->Xmode = PPXCUSTMODE_DUMP_ALL;
				tstrcpy(newptr, PCS->SmesBuf);
			}
		}else{
			newptr = HeapReAlloc(ProcHeap, 0, PCS->SmesBuf, size + SMESSIZE_MARGIN);
		}
		if ( newptr == NULL ){
			xmessage(XM_FaERRd, T("Custmize Memory error"));
			PCS->Smes = PCS->SmesBuf; // とりあえずあふれないようにしておく
			return;
		}
		PCS->Smes = newptr + (PCS->Smes - PCS->SmesBuf);
		PCS->SmesBuf = newptr;
		PCS->SmesLim = newptr + size / sizeof(TCHAR);
	}
}

size_t StrCpyToSmes(PPCUSTSTRUCT *PCS, const TCHAR *text)
{
	size_t len = tstrlen(text);
	CheckSmes(PCS, len + 1);
	memcpy(PCS->Smes, text, TSTROFF(len + 1) );
	PCS->Smes += len;
	return len;
}

// Smes にエラーを書き込む ----------------------------------------------------
TCHAR *GetNoSepMessage(BYTE *p, TCHAR code) // セパレータが見つからない
{
	wsprintf((TCHAR *)p, MessageText(MES_ENSP), code);
	return (TCHAR *)p;
}

void SetMes(PPCUSTSTRUCT *PCS, const TCHAR *mes, const TCHAR *type_text)
{
	CheckSmes(PCS, SMESSIZE_MES);
	PCS->Smes += wsprintf(PCS->Smes, T("%s(%4d): %s")TNL,
			MessageText(type_text), PCS->Dnum, MessageText(mes));
}
void ErrorMes(PPCUSTSTRUCT *PCS, const TCHAR *mes)
{
	SetMes(PCS, mes, MES_LGER);
}
void WarningMes(PPCUSTSTRUCT *PCS, const TCHAR *mes)
{
	SetMes(PCS, mes, MES_LGWA);
}

void SetItemMes(PPCUSTSTRUCT *PCS, const TCHAR *mes, const TCHAR *kword, const TCHAR *sname, const TCHAR *type_text)
{
	CheckSmes(PCS, SMESSIZE_MES);
	if ( sname == NULL ){
		PCS->Smes += wsprintf(PCS->Smes, T("%s(%4d): %s - %s")TNL,
				MessageText(type_text), PCS->Dnum, kword, MessageText(mes));
	}else{
		PCS->Smes += wsprintf(PCS->Smes, T("%s(%4d): %s:%s - %s")TNL,
				MessageText(type_text), PCS->Dnum, kword, sname, MessageText(mes));
	}
}
void ErrorItemMes(PPCUSTSTRUCT *PCS, const TCHAR *mes, const TCHAR *kword, const TCHAR *sname)
{
	SetItemMes(PCS, mes, kword, sname, MES_LGER);
}
void WarningItemMes(PPCUSTSTRUCT *PCS, const TCHAR *mes, const TCHAR *kword, const TCHAR *sname)
{
	SetItemMes(PCS, mes, kword, sname, MES_LGWA);
}
//================================================================ カスタマイズ
PPXDLL int PPXAPI CheckRegistKey(const TCHAR *src, TCHAR *dest, const TCHAR *custid)
{
	int key;

	key = GetKeyCode(&src);
	if ( key < 0 ) return CHECKREGISTKEY_BADNAME; // キーコードが得られなかった
	if ( (UTCHAR)*src > ' ' ) return CHECKREGISTKEY_BADNAME; // コードがおかしい(未解析の文字がある)

										// キーのチェック
	// KC_main / KV_main / KV_page / KV_crt / KV_img
	if ( ((*(custid + 1) == 'C') && (*(custid + 3) == 'm')) ||
		 (*(custid + 1) == 'V') ){
		// ' '/\' 'は SPACE/\SPACE に変換
		if ( (key == ' ') || (key == (K_s | ' ')) ){
			setflag(key, K_v);
		}
	}else{
		// EDITCONTROL系+PPbでSPACE/\SPACEは ' '/\' ' に変換
		if ( ((key == K_space) || (key == (K_s | K_space))) &&
			 (!tstrcmp(custid, T("K_edit")) || !tstrcmp(custid, T("K_lied")) || !tstrcmp(custid, T("K_ppe")) || !tstrcmp(custid, T("KB_edit"))) ){
			resetflag(key, K_v);
		}
	}

	if ( IsupperA(key) && (key & K_c) && !(key & (K_a | K_internal)) ){ // Ctrl+英字 は、K_v 不要
		resetflag(key, K_v);
	}

	// ^J ^\J 等は 仮想キーコードに変換(^Enter が ^J も出力するため)KB_edit除く
	// ^ScrollLock で ^C になるが、これはチェック省略中
	if ( ((key & ~(K_s | K_e)) == (K_c | 'J')) &&
		 (*(custid + 1) != 'B') ){
		setflag(key, K_v);
	}

	PutKeyCode(dest, key);

	// EDITCONTROL系でK_edit で \Enter, ^Enter は機能しないので警告
	if ( ((key == (K_s | K_cr)) || (key == (K_c | K_cr))) &&
		 (!tstrcmp(custid, T("K_edit")) || !tstrcmp(custid, T("K_lied")) || !tstrcmp(custid, T("K_ppe"))) ){
		return CHECKREGISTKEY_WARNKEY;
	}
	// 数字キーは、Shift→記号 Ctrl→仮想キー になるので警告する
	if ( !(key & K_v) && Isdigit(key & 0xff) && (key & (K_s | K_c)) ){
		return CHECKREGISTKEY_WARNKEY;
	}

	// カスタマイズ読み込みが必要なキー
	if ( (key == K_E_LOAD) || (key == K_E_SELECT) ){
		return CHECKREGISTKEY_RELOADKEY;
	}

	return CHECKREGISTKEY_OK;
}
/*-----------------------------------------------------------------------------
	一行を取得する
-----------------------------------------------------------------------------*/
_Ret_notnull_ TCHAR *GetConfigLine(_Inout_ PPCUSTSTRUCT *PCS,_Inout_ TCHAR **mem,_In_ TCHAR *max)
{
	TCHAR *p, *bottom;

	bottom = p = *mem;
	while ( p < max ){
		PCS->Dnum++;
#ifdef _WIN64				// CPU bit数	;64; … x64bit版専用指定
		if ( (*p == ';') && (*(p+1) == '6') &&
			(*(p+2) == '4') && (*(p+3) == '|') ){
			p += 4;
		}
#else						// ;32; … x32bit版専用指定
		if ( (*p == ';') && (*(p+1) == '3') &&
			(*(p+2) == '2') && (*(p+3) == '|') ){
			p += 4;
		}
#endif
		bottom = p = SkipSpaceAndFix(p);
		if ( *p == '\0' ){		// 空行?
			p++;
			PCS->Dnum--;
			continue;
		}
		while( *p != '\0' ){		// コメントの除去
			if ( *p == ';'){
				if ((bottom == p) || (*(p - 1) == ' ') || (*(p - 1) == '\t')){
					*p++ = '\0';
					continue;
				}
			}
			if ( (*p == '\r') || (*p == '\n') ){
				*p++ = '\0';
				if ( (*p == '\r') || (*p == '\n') ) p++;
				break;
			}
			p++;
		}
		break;
	}
	*mem = p;
	return bottom;
}
// 行末空白を除去２ -----------------------------------------------------------
void DeleteEndspace(TCHAR *min, TCHAR *max)
{
	for( ; min < max ; max-- ){
		if ( (*(max-1) == ' ') || (*(max-1) == '\t') ) continue;
		break;
	}
	*max = 0;
}
/*
	「?<>0.00|」をチェックする。0:「?」ではない 正:該当 負:あったが対象外
*/
int CheckPrecode(const TCHAR *ptr, TCHAR command, TCHAR **keyword, const TCHAR *CVer)
{
	TCHAR *p, *q, ver[64];
	int ab = 0;
										// 先頭が command か？
	if ( (*ptr != command) &&
		 ( (command != '+') || ((*ptr != '?') && (*ptr != '^'))) ){
		return 0;
	}
	ptr++;
										// 末尾が '|' か？
	p = tstrchr(ptr, '|');
	if ( p == NULL ) return 0;
	if ( keyword != NULL ) *keyword = p + 1;
										// 符号チェック
	if ( *ptr == '<' ){			// カスタマイズバージョン < 指定バージョン
		ptr++;
	}else if ( *ptr == '>' ){	// カスタマイズバージョン > 指定バージョン
		ptr++;
		ab = -1;
	}
										// バージョンを抽出 & 正当性チェック
	q = ver;
	while ( ptr < p ){
		if ( (*ptr < '.') || (*ptr > '9') ) return 0;
		*q++ = *ptr++;
	}
	*q = 0;
										// バージョンチェック
	if ( !ver[0] ) return 1;	// Ver 指定無し→無条件に該当
	return (tstrcmp(ver, CVer) ^ ab) | 1;
}

BYTE GetNumberWith(const TCHAR **line, BYTE def, DWORD maxnum)
{
	if ( Isdigit(**line) ){
		DWORD num;

		num = (DWORD)GetDigitNumber(line);
		if ( num > maxnum ) num = maxnum;
		return (BYTE)num;
	}else{
		return def;
	}
}

/*-----------------------------------------------------------------------------
	カスタマイズ(格納)行解析部

	TRUE	:正常終了/中止の必要が無いエラー
	FALSE	:中止する必要があるエラー
-----------------------------------------------------------------------------*/
BOOL CSline(PPCUSTSTRUCT *PCS, const TCHAR *kword, TCHAR *sname, TCHAR *line, WORD flags, const char *fmt, TCHAR **mem, TCHAR *max)
{
	BYTE bin[0x10000];	// 格納する内容の形成場所
	BYTE *destp;		// 格納内容の末尾

	// 削除処理の確認
	if ( sname != NULL ){ // 配列
		TCHAR *sep;
		int vercmp;

		vercmp = CheckPrecode(sname, '-', &sname, PCS->CVer);
		if ( vercmp < 0 ) return TRUE;	// 該当しない
							// kword から「x|」を削除
		sep = tstrchr(kword, '|');
		if ( sep != NULL ) kword = sep + 1;

		if ( vercmp > 0 ){
			if ( IsExistCustTable(kword, sname) ){
				CheckSmes(PCS, SMESSIZE_MES);
				PCS->Smes += wsprintf(PCS->Smes, T("%s: %s:%s")TNL,
						MessageText(MES_LGDE), kword, sname);
				DeleteCustTable(kword, sname, 0);
				PCS->reloadflag &= flags;
			}
			return TRUE;
		}
	}else{ // 単純
		int vercmp;

		vercmp = CheckPrecode(kword, '-', &sname, PCS->CVer);
		if ( vercmp < 0 ) return TRUE;	// 該当しない
		if ( vercmp > 0 ){
			if ( IsExistCustData(kword) ){
				CheckSmes(PCS, SMESSIZE_MES);
				PCS->Smes += wsprintf(PCS->Smes, T("%s: %s")TNL,
						MessageText(MES_LGDE), kword);
				DeleteCustData(kword);
				PCS->reloadflag &= flags;
			}
			return TRUE;
		}
	}

	if ( flags & fOld ){
		if ( sname == NULL ){
			PCS->Smes += wsprintf(PCS->Smes, T("Readonly item:%s")TNL, kword);
		}
		return TRUE;
	}

	destp = bin;
	/* if (!(flags & fC)) */ line = SkipSpaceAndFix(line);	// 行頭空白を削除
	if ( *line == '\0' ){
		if ( (flags == fSwarn) && (sname == NULL) ){
			DeleteCustData(kword);
			PCS->reloadflag &= flags;
			return TRUE;
		}
		if ( !(flags & fB) ) return TRUE;
	}
	if ( *fmt != '\0' ) fmt++;						// Separetor 部分から移動
//--------------------------------------------------------- Dump
	if ( *fmt == '\0' ){
		while( *line != '\0' ){
			int high, low;

			high = GetHexChar(&line);
			if ( high < 0 ) break;
			low = GetHexChar(&line);
			if ( low < 0 ){
				ErrorMes(PCS, T("Hex string"));
				return TRUE;
			}
			*destp++ = (BYTE)((high << 4) + low);
		}
//--------------------------------------------------------- Formats
	}else while( *fmt != '\0' ){
		switch(*fmt++){
//=============================================================================
// C:色 -----------------------------------------------------------------------
case 'C':
	*(COLORREF *)destp = CS_color(&line, flags);
	destp += sizeof(COLORREF);
	break;

// c:色/console ---------------------------------------------------------------
case 'c':{
	TCHAR buf[100], *pt, *dst, data;
	int i, c = 0;

	for ( ; ; ){
										// キーワードを抽出
		pt = line = SkipSpaceAndFix(line);
		dst = buf;
		while((UTCHAR)(data = *pt++) >= (UTCHAR)'A' )	*dst++ = data;
		*dst = 0;
										// 定義済み識別子を判別
		for ( i = 0 ; i < concolor_s ; i++ ){
			if ( !tstrcmp(buf, ConColor[i].str) ){
				line = pt - 1;
				c |= ConColor[i].num;
				break;
			}
		}
		if ( i == concolor_s ){ // 数字指定
			c |= GetNumber((const TCHAR **)&line);
		}
		SkipSPC(line);
		if ( (*line == '+') || (*line == '|') ){
			line++;
			continue;
		}
		break;
	}
	*(WORD *)destp = (WORD)c;
	destp += 2;
	break;
}
// B:2進指定 ------------------------------------------------------------------
case 'B':
	while ( *fmt++ != '/' );			// 表示ビット数指定をスキップ
// D:符号無し10進指定 ---------------------------------------------------------
case 'D':
// d:符号有り10進指定 ---------------------------------------------------------
case 'd':

	*(int *)destp = GetIntNumber((const TCHAR **)&line);
	destp += *fmt++ - '0';
	SkipSPC(line);
	if ( (*fmt == ',') && (*line != ',') && ((UTCHAR)*line >= ' ') ){
		ErrorItemMes(PCS, GetNoSepMessage(destp, ','), kword, sname);
		return TRUE;
	}
	break;

// E:(PPC)エントリ表示 --------------------------------------------------------
case 'E':
	if ( CS_ppcdisp(PCS, &line, &destp) == FALSE ){
		return TRUE;
	}
	break;

// S:文字列 -------------------------------------------------------------------
case 's':
case 'S': {
	int binlen;
	TCHAR *sep, mode;
	int destlen, linelen;

	mode = *(fmt - 1);
	if ( *fmt == 'D' ){
		mode = 'D';
		fmt++;
	}

	linelen = tstrlen32(line);
	if ( !Isdigit(*fmt) ){				// 長さ指定なし
		binlen = 0;
	}else{								// 長さ指定あり
		binlen = GetNumberA(&fmt);
	}
	if ( *fmt == ',' ){					// この文字列のあとにも書式がある？
		sep = tstrchr(line, ',');
		if ( sep != NULL ) linelen = sep - line;
	}
	if ( binlen != 0 ){					// 長さ指定があるなら大きさ確認
		if ( binlen <= linelen ){
			ErrorItemMes(PCS, MES_EFLW, kword, sname);
			return TRUE;
		}
		destlen = binlen;
	}else{
		destlen = linelen + 1;
	}
	tstrcpy((TCHAR *)destp, line);
	*(TCHAR *)((TCHAR *)destp + linelen) = '\0';

	if ( mode == 'D' ){
		tstrreplace( (TCHAR *)destp, T("\""), NilStr);
		if ( (*line == '%') && (*(line + 1) == '0') ){
			int dellen = 2;

			if ( *(line + 2) == '\\' ){
				dellen = 3;
			}else if ( (*(line + 2) == '%') && (*(line + 3) == '\\') ){
				dellen = 4;
			}
			destlen -= dellen;
			memmove( destp , (TCHAR *)((TCHAR *)line + dellen), destlen * sizeof(TCHAR) );
			WarningItemMes(PCS, T("Replace %0 to relative path"), kword, sname);
		}
		if ( tstrchr( (TCHAR *)destp, '%') != NULL ){
			WarningItemMes(PCS, T("Macro character not support"), kword, sname);
		}
	}else if ( mode == 's' ){
		TCHAR *src, *dest;

		destlen = 1;
		dest = (TCHAR *)destp;
		for ( src = (TCHAR *)destp ; *src ; destlen++ ){
#ifndef UNICODE
			if ( IskanjiA(*src) ){
				*dest++ = *src++;
				if ( *src ){
					*dest++ = *src++;
					destlen++;
				}
				continue;
			}
#endif
			if ( *src == '\\' ){
				if ( *(src + 1) == 'n' ){
					*dest++ = '\n';
					src += 2;
					continue;
				}
				if ( *(src + 1) == 't' ){
					*dest++ = '\t';
					src += 2;
					continue;
				}
			}
			*dest++ = *src++;
		}
		*dest = '\0';
	}
	destp += TSTROFF(destlen);
	line += linelen;
	break;
}
// M;複数行記載 ---------------------------------------------------------------
case 'M':
case 'm':
	for ( ; ; ){
		int len;
		size_t size;

		len = tstrlen32(line);
		size = TSTROFF(len + 1);
		memcpy(destp, line, size);
		destp += size;
		line += len;

		if ( (**mem != ' ') && (**mem != '\t') ) break; // 字下げがないので終
		line = GetConfigLine(PCS, mem, max);
		if ( *line == '\0' ) break;
		#ifdef UNICODE
			*(destp - 2) = (BYTE)((*(fmt - 1) == 'M') ? '\n' : '\0');
			*(destp - 1) = '\0';
		#else
			*(destp - 1) = (BYTE)((*(fmt - 1) == 'M') ? '\n' : '\0');
		#endif
	}
	if ( *(fmt - 1) == 'M' ) break;
	#ifdef UNICODE
		*destp = '\0';
		*(destp + 1) = '\0';
		destp += 2;
	#else
		*destp++ = '\0';
	#endif
	break;

// k;keys ---------------------------------------------------------------------
case 'k': {
	int i;
	i = MAX32;	// @ チェック回避用
	for ( ; ; ){
		line = SkipSpaceAndFix(line);
		if ( !*line ) break;
		i = GetKeyCode((const TCHAR **)&line);
		if ( i < 0 ){
			ErrorItemMes(PCS, MES_EKFT, kword, sname);
			return TRUE;
		}
		*(WORD *)destp = (WORD)i;
		destp += 2;
	}
	// E:edit P:pack
	if ( !(i & (K_raw | K_ex)) && ((i & 0x1ff) != 'E')&&((i & 0x1ff) != 'P')){
											// 0.40 まで、K_rawがなかった対策
		if ( (i == (K_c | K_s | K_F10)) || (i == (K_tab)) ){
			*(WORD *)(destp - 2) = (WORD)(i | K_raw);
		}else{ // キー割当ての時は、再エイリアスの警告を行う
			if ( kword[0] == 'K' ) WarningItemMes(PCS, MES_WNOA, kword, sname);
		}
	}
	*(WORD *)destp = 0;
	destp += 2;
	break;
}
// K;key - １キーのみ指定 -----------------------------------------------------
case 'K': {
	int i;

	line = SkipSpaceAndFix(line);
	if ( !*line ) break;
	i = GetKeyCode((const TCHAR **)&line);
	if ( i < 0 ){
		ErrorItemMes(PCS, MES_EKFT, kword, sname);
		return TRUE;
	}
	*(WORD *)destp = (WORD)i;
	destp += 2;
	break;
}
// X:Exec/Keys 判別 -----------------------------------------------------------
case 'x':
case 'X':
	line = SkipSpaceAndFix(line);
	if ( *line++ == '=' ){
		*destp = EXTCMD_KEY;
		fmt = "k";
	}else{
		line = SkipSpaceAndFix(line);
		*destp = EXTCMD_CMD;
		fmt = "M";
	}
	destp++;
	#ifdef UNICODE
	*destp++ = '\0';
	#endif
	continue;

// V:ID/Version ---------------------------------------------------------------
case 'V': {
	TCHAR *comment;
	int i;
										// バージョンを抽出 -------------------
	comment = tstrchr(line, ',');
	if ( comment == NULL ) comment = line + tstrlen(line);
	i = comment - line;
	memcpy(destp, line, TSTROFF(i));
	destp += i * sizeof(TCHAR);
	*((TCHAR *)destp) = '\0';
	destp += sizeof(TCHAR);
	PCS->Smes += wsprintf(PCS->Smes,
			T("File version : %s")TNL, destp - TSTROFF(i + 1));
										// コメント/格納チェック --------------
	if ( *comment == ',' ) comment++;
	if ( *comment == '\0' ) break;
	CheckSmes(PCS, tstrlen(comment));
	if ( *comment == '?' ){
		comment++;
		wsprintf(PCS->Smes, T("File comment : %s") TNL
				T("Include this file?"), comment);
		if ( PCS->Sure(PCS->Smes) != IDYES ) return FALSE;
	}else{
		PCS->Smes += wsprintf(PCS->Smes, T("File comment : %s")TNL, comment);
	}
	break;
}
// 不明・セパレータ -----------------------------------------------------------
case ' ':
	continue;
default:
	line = SkipSpaceAndFix(line);
	if ( *line != *(fmt - 1) ){
		if ( (*(fmt - 1) != *fmt) && !(flags & fSwarn) ){
			WarningItemMes(PCS, GetNoSepMessage(destp, *(fmt-1)), kword, sname);
		}
		fmt = Nilfmt;
		break;
	}else{
		if ( *fmt == *(fmt - 1) ) fmt++;
	}
	line++;
	break;
//=============================================================================
		}
	}
	if ( sname != NULL ){	// 配列内の項目を保存する--------------------------
		TCHAR newSname[VFPS], sn;
		int pre;
		sn = *sname;
												// 項目の強制追加(update時のみ)
		pre = CheckPrecode(sname, '+', &sname, PCS->CVer);
				// (update) && (無条件登録でない) → 条件別処理
		if ( (PCS->Xmode == PPXCUSTMODE_UPDATE) && !PCS->XupdateTbl ){
			if ( pre < 0 ) return TRUE;	// 該当バージョンでないため、更新しない
			if ( pre == 0 ) return TRUE; // "+|" でない→登録せず
										 // "+|" →状況メッセージありで登録
			CheckSmes(PCS, SMESSIZE_MES);
			if ( IsExistCustTable(kword, sname) ){
				if ( sn == '?' ) return TRUE; // "?|" は上書きしない
				PCS->Smes += wsprintf(PCS->Smes, T("%s: %s:%s")TNL,
						MessageText(MES_LGOW), kword, sname);
			}else{
				PCS->Smes += wsprintf(PCS->Smes, T("%s: %s:%s")TNL,
						MessageText(MES_LGCR), kword, sname);
			}
		}
		if ( flags & fK_Mouse ) tstrupr(sname);	// 大文字化
		if ( flags & fKeyFix ){				// キーコードの正規化 -------------
			switch ( CheckRegistKey(sname, newSname, kword) ){
				case CHECKREGISTKEY_BADNAME:
					ErrorMes(PCS, MES_EKFT);
					return TRUE;
				case CHECKREGISTKEY_WARNKEY:
					WarningItemMes(PCS, MES_WKSI, kword, sname);
					break;
				case CHECKREGISTKEY_RELOADKEY:
					resetflag(PCS->reloadflag, fNoRelo); // カスタマイズ読み込みが必要
					break;
				// default:
			}
			sname = newSname;
		}else if ( flags & fK_HMenu ){ // 隠しメニュー用に加工 ---------------
			sname[4] = '\0';
			wsprintf(newSname, T("%-4s"), sname);
			sname = newSname;
		}else if ( flags & (fK_ExtWild | fK_PathWild) ){ // ワイルドカード検出
			const TCHAR *sp;

			if ( !((sname[0] == '*') && (sname[1] == '\0')) ){
				sp = sname;
				if ( flags & fK_ExtWild ){
					if ( *sp != ':' ){ // ファイル種別名以外ならワイルドカードチェック
						for ( ; *sp ; sp++ ){
							if ( (*sp >= '!') && (*sp <= '?') ){
								if ( tstrchr(DetectWildcardLetter, *sp) != NULL ){
									newSname[0] = '/';
									tstrcpy(newSname + 1, sname);
									sname = newSname;
									break;
								}
							}
						}
					}
					if ( sname != newSname ) tstrupr(sname);
				}else{ // fK_PathWild
					if ( *sp != ':' ){ // 強制パス指定(/区切り用)以外ならワイルドカードチェック
						if ( tstrchr(sp, '/') != NULL ){ // 正規表現か ftp:/ 系か
							int drive;
							if ( VFSGetDriveType(sp, &drive, NULL) != NULL ){
								if ( drive >= VFSPT_FILESCHEME ){ // VFSPT_FTP, VFSPT_SHELLSCHEME, VFSPT_HTTP
									sp = NilStr;
								}
							}
						}
						for ( ; *sp ; sp++ ){
							if ( (*sp >= '!') && (*sp <= '?') ){
								if ( tstrchr(DetectPathWildcardLetter, *sp) != NULL ){
									newSname[0] = '/';
									tstrcpy(newSname + 1, sname);
									sname = newSname;
									break;
								}
							}
						}
					}
				}
			}
		}
		if ( (flags & fK_sep) &&
			 ( ((sname[0] == '-') && (sname[1] == '-')) ||
			   ((sname[0] == '|') && (sname[1] == '|'))) ){
			InsertCustTable(kword, sname, 0x7fffffff, bin, destp - bin);
		}else{
			if ( NO_ERROR != SetCustTable(kword, sname, bin, destp - bin) ){
				goto noareaerror;
			}
		}
	}else{					// 単純項目の保存 ---------------------------------
		TCHAR sn;
		int pre;
		sn = *kword;

		if ( (sn == '^') && (*(kword + 1) == '|') ){ // 項目の一部追加
			int size;

			kword += 2;
			if ( PCS->Xmode == PPXCUSTMODE_UPDATE ){
				size = GetCustDataSize(kword);
				if ( size >= 0 ){
					if ( (destp - bin) > size ){ // 項目が少ない
						CheckSmes(PCS, SMESSIZE_MES);
						PCS->Smes += wsprintf(PCS->Smes, T("%s: %s")TNL,
								MessageText(MES_LGAP), kword);
						GetCustData(kword, bin, sizeof(bin));
					}else{ // 項目数に問題ない
						return TRUE;
					}
				}
			}
		}else{
															// 項目の強制追加
			pre = CheckPrecode(kword, '+', (TCHAR **)&kword, PCS->CVer);
			if ( PCS->Xmode == PPXCUSTMODE_UPDATE ){	// update
				if ( pre < 0 ) return TRUE;	// 該当バージョンでないため、未更新
				if ( pre > 0 ){
					if ( sn == '?' ) return TRUE; // "?|" は上書きしない
					if ( IsExistCustData(kword) ){
						// 上書き
						CheckSmes(PCS, SMESSIZE_MES);
						PCS->Smes += wsprintf(PCS->Smes, T("%s: %s")TNL,
								MessageText(MES_LGOW), kword);
					}
				}else{	// "+|" なし	／ 既に項目があるなら保存しない
					if ( IsExistCustData(kword) ) return TRUE;
					// 新規
					CheckSmes(PCS, SMESSIZE_MES);
					PCS->Smes += wsprintf(PCS->Smes, T("%s: %s")TNL,
							MessageText(MES_LGCR), kword);
				}
			}
		}
		if ( NO_ERROR != SetCustData(kword, bin, destp - bin) ) goto noareaerror;
	}
	PCS->reloadflag &= flags;
	return TRUE;
noareaerror:
	ErrorMes(PCS, MES_ECFL);
	return FALSE;
}
/*-----------------------------------------------------------------------------
	カスタマイズ(格納)項目処理
-----------------------------------------------------------------------------*/
int CSitem(PPCUSTSTRUCT *PCS,
		const CLABEL *clbl,	// Form
		TCHAR sepC,		// Separator キャラクタ
		TCHAR *kword,	// Keyword
		TCHAR *ptr,		// 解析位置
		TCHAR **mem,	// ファイル内容の処理位置
		TCHAR *max)		// ファイル内容の末端
{
	if ( !(clbl->flag & fT) ){		// 単純 -----------------------------------
		if ( *clbl->fmt != sepC){
			ErrorItemMes(PCS, MES_ESEP, kword, NULL);
			return TRUE;
		}
		return CSline(PCS, kword, NULL, ptr, clbl->flag, clbl->fmt, mem, max);
	}else{							// 配列 -----------------------------------
		if ( sepC != '=' ){
			ErrorItemMes(PCS, MES_ESEP, kword, NULL);
			return TRUE;
		}
		ptr = SkipSpaceAndFix(ptr);
		if ( *ptr++ != '{' ){
			ErrorItemMes(PCS, MES_EBRK, kword, NULL);
			return TRUE;
		}

		if ( clbl->flag & fOld ){
			PCS->Smes += wsprintf(PCS->Smes, T("Readonly item:%s")TNL, kword);
		}else{
			TCHAR *p;

			if ( PCS->Xmode == PPXCUSTMODE_UPDATE ){
				int pre;

				p = kword;
				pre = CheckPrecode(kword, '+', &p, PCS->CVer);
				if ( !IsExistCustData(p) ){	// 配列自体を新規作成
					PCS->XupdateTbl = 1;
					CheckSmes(PCS, SMESSIZE_MES);
					PCS->Smes += wsprintf(PCS->Smes, T("%s: %s")TNL,
							MessageText(MES_LGCR), p);
				}else{
					if ( pre > 0 ){
						PCS->XupdateTbl = 1;
						CheckSmes(PCS, SMESSIZE_MES);
						PCS->Smes += wsprintf(PCS->Smes, T("%s: %s")TNL,
								MessageText(MES_LGOW), p);
					}else{
						PCS->XupdateTbl = 0;
					}
				}
			}
			if ( PCS->Xmode == PPXCUSTMODE_STORE ){
				DeleteCustData(kword); // 初期化
				p = ptr;
				while ( (*p == ' ') || (*p == '\t') ) p++;
				if ( ((UTCHAR)*p > ' ') && (tstrcmp(DefCommentStr + 1, p) != 0) ){
					int len;

					len = tstrlen32(ptr);
					if ( len >= MAX_PATH ) len = MAX_PATH - 1;
					SetCustTable(T("#Comment"), kword, ptr, TSTROFF(len + 1));
				}else if ( *clbl->comment == '\0' ){
					DeleteCustTable(T("#Comment"), kword, 0);
				}
			}
		}
		while( *mem < max ){
			TCHAR *p;
			TCHAR kwordbuf[0x80];

			p = GetConfigLine(PCS, mem, max);
			if ( *p == '\0' ) continue;
			if ( *p == '}' ) break;
												// セパレータの識別 ----------
			ptr = p;
			while( *ptr ){
				if ( *ptr == '\'' ){
					ptr++;
					if ( !*ptr ) break;
					ptr = tstrchr(ptr + 1, '\'');
					if ( ptr == NULL ) break;
				}else if ( *clbl->fmt != '/' ){	// 通常のセパレータ
					if ( *ptr == *clbl->fmt ) break;
				}else{							// 切替式
					if ( *ptr == '=' ) break;
					if ( *ptr == ',' ) break;
				}
				ptr++;
			}
			if ( (ptr == NULL) || (*ptr == '\0') ){
				ErrorMes(PCS, MES_ESEP);
				continue;
			}
												// 解析 ----------------------
			if ( *clbl->fmt == '/' ){
				if ( (size_t)(ptr - p) > (TSIZEOF(kwordbuf) - 2) ){
					ErrorMes(PCS, MES_ESEP);
					continue;
				}
				memcpy(kwordbuf, p, TSTROFF(ptr - p));
				DeleteEndspace(kwordbuf, kwordbuf + (ptr - p));
				p = kwordbuf;
			}else{
				DeleteEndspace(p, ptr);
			}
			if ( *clbl->fmt != '/' ) ptr++;
			DeleteEndspace(ptr, ptr + tstrlen(ptr));
			if ( !CSline(PCS, kword, p, ptr, clbl->flag, clbl->fmt, mem, max) ){
				return FALSE;
			}
		}
		return TRUE;
	}
}

/*-----------------------------------------------------------------------------
	カスタマイズ(格納)本体
-----------------------------------------------------------------------------*/
PPXDLL int PPXAPI PPcustCStore(TCHAR *mem, TCHAR *memmax, int appendmode, TCHAR **log, int (USECDECL * Sure)(const TCHAR *message))
{
	PPCUSTSTRUCT PCS;
	TCHAR *kword, *ck;				// 項目名
	TCHAR *p, sepC;
	const CLABEL *clbl;
	BOOL result = TRUE;

	PCS.Smes = PCS.SmesBuf = HeapAlloc(ProcHeap, 0, FIRSTSTORESIZE + SMESSIZE_MARGIN);
	PCS.SmesLim = PCS.Smes + FIRSTSTORESIZE / sizeof(TCHAR);
	PCS.Dnum = 0;
	PCS.Xmode = appendmode;
	PCS.Sure = Sure;
	PCS.reloadflag = fNoRelo | fReSta;

	tstrcpy(PCS.CVer, T("0.00"));
	GetCustData(T("PPxCFG"), &PCS.CVer, sizeof(PCS.CVer));

										// 解析処理 ---------------------------
	while( (mem < memmax) && result ){
		int pre;
		TCHAR *ne;
										// Keyword の抽出 ----------
		kword = GetConfigLine(&PCS, &mem, memmax);
		if ( *kword == '\0' ) continue;
		p = tstrchr(kword, '=');
		if ( p == NULL ) p = tstrrchr(kword, ':');
		if ( (p == NULL) || (p == kword) ){
			ErrorMes(&PCS, MES_EKNF);
			continue;
		}
		DeleteEndspace(kword, p);
		sepC = *p++;
		DeleteEndspace(p, p + tstrlen(p));
										// Keyword の解析 ----------
		ck = kword;

		CheckPrecode(ck, '+', &ck, NilStr);	// '+'はここでは無視
		clbl = Ctbl;
		while( clbl->name != NULL ){
			if ( tstricmp(ck, clbl->name) == 0 ){
				result = CSitem(&PCS, clbl, sepC, kword, p, &mem, memmax);
				break;
			}
			clbl++;
		}
		if ( clbl->name == NULL ){	// 特殊な Keyword の解析 ==============
			if ( sepC == ':' ){			// dump 形式 ----------------
				result = CSline(&PCS, kword, NULL, p, clbl->flag, clbl->fmt, &mem, memmax);
										// ユーザ定義メニュー --------
			}else if ( (ck[0] == 'M') && (ck[1] == '_') ){
				result = CSitem(&PCS, &usermenu, sepC, kword, p, &mem, memmax);
										// ユーザ定義拡張子判別 --------
			}else if ( (ck[0] == 'E') && (ck[1] == '_') ){
				result = CSitem(&PCS, &userext, sepC, kword, p, &mem, memmax);
										// メッセージ --------
			}else if ( (tstrlen(ck) == 7) && !memcmp(ck, T("Mes"), TSTROFF(3)) ){
				result = CSitem(&PCS, &userlang, sepC, kword, p, &mem, memmax);
										// ユーザ定義ツールバー --------
			}else if ( (ck[0] == 'B') && (ck[1] == '_') ){
				result = CSitem(&PCS, &userbar, sepC, kword, p, &mem, memmax);
										// ユーザ定義キー割当て --------
			}else if ( (ck[0] == 'K') && (ck[1] == '_') ){
				result = CSitem(&PCS, &userkey, sepC, kword, p, &mem, memmax);
										// 任意設定 --------
			}else if ( (ck[0] == 'S') && (ck[1] == '_') ){
				result = CSitem(&PCS, &usersettings, sepC, kword, p, &mem, memmax);
										// 削除 ------------------------
			}else{
											// update時は古いバージョンが対象
				pre = CheckPrecode(kword, '-', &ne, PCS.CVer);
				if ( pre < 0 ) continue;

				if ( pre > 0 ){
					kword = ne;
					if ( IsExistCustData(kword) ){
						PCS.Smes += wsprintf(PCS.Smes, T("%s: %s")TNL,
								MessageText(MES_LGDE), kword);
						DeleteCustData(kword);
					}
				}else{						// 不明の形式 ----------------
					if ( sepC == '=' ){
						p = SkipSpaceAndFix(p);
						if ( *p++ == '{' ){
							while ( mem < memmax ){
								p = GetConfigLine(&PCS, &mem, memmax);
								if ( *p == '\0' ) continue;
								if ( *p == '}' ) break;
							}
						}
						ErrorMes(&PCS, MES_EUKW);
					}else{
						ErrorMes(&PCS, MES_ESEP);
					}
				}
			}
		}
	}
	PCS.Smes += wsprintf(PCS.Smes, TNL);
	if ( log != NULL ) *log = PCS.SmesBuf;
	return result ? (PCS.reloadflag | 1) : 0;
}
/*-----------------------------------------------------------------------------
	カスタマイズ(書き出し)出力部
-----------------------------------------------------------------------------*/
void CDsub(PPCUSTSTRUCT *PCS, const TCHAR *name, BYTE *bin, int size, WORD flag, const char *fmt)
{
	BYTE *binend;
											// キーワード/セパレータ表示
	CheckSmes(PCS, SMESSIZE_MES); // name + α
	switch (*fmt){
		case '/':
			break;

		case '\0':
		case ':': {
			int i;
			CheckSmes(PCS, size * 2);
			PCS->Smes += wsprintf(PCS->Smes, T("%s\t:"), name);
			for ( i = 0 ; i < size ; i++ ){
				PCS->Smes += wsprintf(PCS->Smes, T("%02x"), bin[i]);
			}
			return;
		}
		default:
			if ( flag & (fK_ExtWild | fK_PathWild) ){ // ワイルドカード有
				if ( *name == '/' ) name++;
			}
			PCS->Smes += wsprintf(PCS->Smes, T("%s\t%c "), name, *fmt);
	}
	fmt++;
	binend = bin + size;
											// パラメータ表示
	while( *fmt ){
		if ( bin >= binend ) break;
		switch( *fmt++ ){
			// C:色 -----------------------------------------------------------
			case 'C':
				PCS->Smes += CD_color(PCS->Smes, *(COLORREF *)bin, flag);
				bin += sizeof(COLORREF);
				break;

			// c:console 色 ---------------------------------------------------
			case 'c': {
				int c, i;

				c = (int)(*(WORD *)bin);
				bin += 2;
											// 前景色 -------------------
				for ( i = 0 ; i <= 15 ; i++ ){
					if ( (c & 0x0f) == ConColor[i].num ){
						PCS->Smes += wsprintf(PCS->Smes, ConColor[i].str);
						break;
					}
				}
											// 背景色 -------------------
				for ( i = 16 ; i <= 31 ; i++ ){
					if ( (c & 0xf0) == ConColor[i].num ){
						PCS->Smes +=
							wsprintf(PCS->Smes, T("+%s"), ConColor[i].str);
						break;
					}
				}
											// その他 -------------------
				for ( i = 32 ; i <= 35 ; i++ ){
					if ( c & ConColor[i].num ){
						PCS->Smes +=
							wsprintf(PCS->Smes, T("+%s"), ConColor[i].str);
					}
				}
				break;
			}
			// B:2進表示-------------------------------------------------------
			case 'B': {
				int bits, i;
				DWORD data;

				bits = GetNumberA(&fmt);
				data = *(DWORD *)bin;
				if ( *(fmt + 1) <= 2 ) data &= 0xffff;
				// 指定ビット数よりも大きい値ならビット数を拡張
				while ( -(1 << bits) & data && (bits < 32) ) bits++;
				*PCS->Smes++ = 'B';
				for ( i = 1 ; i <= bits ; i++ ){
					*PCS->Smes++ = (TCHAR)((data & (1 << (bits - i))) ? '1':'0');
				}
				bin += *(fmt + 1) - '0'; // byte数
				fmt += 2; // / と byte数をスキップ
				break;
			}
			// D:符号無し10進表示----------------------------------------------
			case 'D':
				switch(*fmt++){
					case '1':
						PCS->Smes += wsprintf(PCS->Smes, T("%u"), *(BYTE *)bin);
						bin++;
						break;
					case '2':
						PCS->Smes += wsprintf(PCS->Smes, T("%u"), *(WORD *)bin);
						bin += 2;
						break;
					case '4':
						PCS->Smes += wsprintf(PCS->Smes, T("%u"), *(DWORD *)bin);
						bin += 4;
						break;
				}
				break;
			// D:符号有り10進表示----------------------------------------------
			case 'd':
				switch(*fmt++){
					case '1':
						PCS->Smes += wsprintf(PCS->Smes, T("%d"), *(char *)bin);
						bin++;
						break;
					case '2':
						PCS->Smes += wsprintf(PCS->Smes, T("%d"), *(short *)bin);
						bin += 2;
						break;
					case '4':
						PCS->Smes += wsprintf(PCS->Smes, T("%d"), *(long *)bin);
						bin += 4;
						break;
				}
				break;
			// E:(PPC)エントリ表示書式-----------------------------------------
			case 'E':
			// CheckSmes
				CD_ppcdisp(PCS, &bin, binend);
				break;

			// V:ID/Version ---------------------------------------------------
			case 'V':
			// S:文字列表示----------------------------------------------------
			case 'S': {
				size_t binlen = 0, smeslen;

				if ( *fmt == 'D' ) fmt++;
				if ( Isdigit(*fmt) ) binlen = GetNumberA(&fmt);
				smeslen = StrCpyToSmes(PCS, (TCHAR *)bin);
				bin += binlen ? TSTROFF(binlen) : TSTROFF(smeslen + 1);
				break;
			}
			// s:文字列表示----------------------------------------------------
			case 's': {
				TCHAR *last;

				last = (TCHAR *)bin + tstrlen((TCHAR *)bin);
				CheckSmes(PCS, last - (TCHAR *)bin + 1);
				if ( Isdigit(*fmt) ) GetNumberA(&fmt);	// 長さ指定をスキップ
				for ( ; *(TCHAR *)bin != '\0' ; bin += TSTROFF(1) ){
					if ( *(TCHAR *)bin == '\t' ){
						CheckSmes(PCS, last - (TCHAR *)bin + 2);
						*PCS->Smes++ = '\\';
						*PCS->Smes++ = 't';
						continue;
					}
					if ( *(TCHAR *)bin == '\n' ){
						CheckSmes(PCS, last - (TCHAR *)bin + 2);
						*PCS->Smes++ = '\\';
						*PCS->Smes++ = 'n';
						continue;
					}
					*PCS->Smes++ = *(TCHAR *)bin;
				}
				*PCS->Smes = '\0';
				bin += TSTROFF(1);
				break;
			}
			case 'M': {
				TCHAR *nl, *bp;

				fmt = Nilfmt;
				bp = (TCHAR *)bin;
				while( (nl = tstrchr(bp, '\n')) != NULL ){
					*nl = '\0';
					CheckSmes(PCS, nl - bp + 4); // bp \r \n \t \0
					PCS->Smes += wsprintf(PCS->Smes, T("%s") TNL T("\t"), bp);
					bp = nl + 1;
				}
				bin = (BYTE *)(TCHAR *)(bp + StrCpyToSmes(PCS, bp) + 1);
				break;
			}

			case 'm': {
				TCHAR *bp;
				size_t smeslen;

				fmt = Nilfmt;
				bp = (TCHAR *)bin;
				for (;;){
					smeslen = StrCpyToSmes(PCS, bp);
					bp += smeslen + 1;
					if ( *bp == '\0' ) break;
					PCS->Smes += wsprintf(PCS->Smes, TNL T("\t"));
				}
				bin = (BYTE *)(TCHAR *)(bp + 1);
				break;
			}
			// k:keys ---------------------------------------------------------
			case 'k':
				if ( *(WORD *)bin ){
					while ( bin < binend ){
						TCHAR buf[100];

						// CheckSmes
						PutKeyCode(buf, *(WORD *)bin);
						PCS->Smes += wsprintf(PCS->Smes, buf);
						bin += 2;
						if( *(WORD *)bin == 0 ) break;
						*PCS->Smes++ = ' ';
					}
				}
				bin += 2;
				break;
			// K:key ----------------------------------------------------------
			case 'K':
				PutKeyCode(PCS->Smes, *(WORD *)bin);
				PCS->Smes += tstrlen(PCS->Smes);
				bin += 2;
				break;
			// X:Exec/keys ----------------------------------------------------
			case 'X':
				if ( flag & fK_ExtWild ){ // ワイルドカード有
					if ( *name == '/' ) name++;
				}

				if ( *bin == EXTCMD_CMD ){
					bin += TSTROFF(1);
					PCS->Smes += wsprintf(PCS->Smes, T("%s\t,"), name);
					fmt = "M";
					continue;
				}else{
					if ( *bin == EXTCMD_KEY ) bin += TSTROFF(1);
					PCS->Smes += wsprintf(PCS->Smes, T("%s\t= "), name);
					fmt = "k";
					continue;
				}
			case 'x':
				if ( *bin == EXTCMD_CMD ){
					bin += TSTROFF(1);
					PCS->Smes += wsprintf(PCS->Smes, T(" , "));
					fmt = "S";
					continue;
				}else{
					if ( *bin == EXTCMD_KEY ) bin += TSTROFF(1);
					PCS->Smes += wsprintf(PCS->Smes, T(" = "));
					fmt = "k";
					continue;
				}
			case ',':
				if ( *fmt == ',' ) fmt++;
			// ?:そのまま表示--------------------------------------------------
			default:
				*PCS->Smes++ = *(fmt - 1);
				break;
		}
	}
}
/*-----------------------------------------------------------------------------
	カスタマイズ(書き出し)項目処理
-----------------------------------------------------------------------------*/
void CDitem(PPCUSTSTRUCT *PCS, const CLABEL *clbl)
{
	TCHAR sname[VFPS];
	BYTE bin[MAXCUSTDATA];
	int size;

	size = GetCustDataSize(clbl->name);
	if ( size < 0 ){								// 未登録 ---------
		if ( clbl->flag & (fInternal | fHide | fOld) ) return; // 書き出さない

		if ( !(clbl->flag & fT) ){		// 単純
			PCS->Smes += wsprintf(PCS->Smes,
					T("%s\t= %s")TNL, clbl->name, clbl->comment);
		}else{							// 配列
			sname[0] = '\0';
			GetCustTable(T("#Comment"), clbl->name, sname, TSTROFF(MAX_PATH));
			sname[MAX_PATH - 1] = '\0';
			if ( (sname[0] == '\0') && (*clbl->comment == '\0') ){
				tstrcpy(sname, DefCommentStr);
			}
			PCS->Smes += wsprintf(PCS->Smes, T("%s\t= {%s%s") TNL T("}")TNL,
					clbl->name, sname, clbl->comment);
		}
	}else{											// 登録済み -------
		if ( clbl->flag & fInternal ) return;	 // 書き出さない
		if ( !(clbl->flag & fT) ){		// 単純
			GetCustData(clbl->name, bin, sizeof(bin));
			CDsub(PCS, clbl->name, bin, size, clbl->flag, clbl->fmt);
			PCS->Smes = tstpcpy(PCS->Smes, clbl->comment);
			PCS->Smes += wsprintf(PCS->Smes, TNL);
		}else{							// 配列
			int scnt;

			scnt = 0;
			sname[0] = '\0';
			GetCustTable(T("#Comment"), clbl->name, sname, TSTROFF(MAX_PATH));
			sname[MAX_PATH - 1] = '\0';
			if ( (sname[0] == '\0') && (*clbl->comment == '\0') ){
				tstrcpy(sname, DefCommentStr);
			}
			PCS->Smes += wsprintf(PCS->Smes, T("%s\t= {%s%s")TNL,
									clbl->name, sname, clbl->comment);
			for ( ; ; ){
				size = EnumCustTable(scnt, clbl->name, sname, bin, sizeof(bin));
				if ( 0 > size ) break;
				CDsub(PCS, sname, bin, size, clbl->flag, clbl->fmt);
				PCS->Smes += wsprintf(PCS->Smes, TNL);
				scnt++;
			}
			PCS->Smes += wsprintf(PCS->Smes, T("}")TNL);
		}
	}
}

void MakeUserCustNames(USERCUSTNAMES *ucndata)
{
	USERCUSTNAMES *ucn;
	BYTE bin[4]; // 使わないので最小限
	TCHAR name[VFPS];

	int count = 0;
	const CLABEL *lbl;

	ucn = ucndata;

	ThInit(&ucn->UsersIndex);
	ThInit(&ucn->UsersNames);

	for ( ; ; ){
		if ( EnumCustData(count++, name, bin, 0) < 0 ) break;
		if ( memcmp(name, T("Mes"), TSTROFF(3)) &&
			((name[1] != '_') ||
			 !((name[0] == 'M') || (name[0] == 'E') || (name[0] == 'S') ||
			   (name[0] == 'B') || (name[0] == 'K')) ) ){
			continue;
		}
		lbl = Ctbl;
		while( lbl->name ){
			if ( !tstricmp(name, lbl->name) ) break;
			lbl++;
		}
		if ( lbl->name == NULL ){
			DWORD offset;
			DWORD *index;
			DWORD mini, maxi, midi, indexmax;

			offset = ucn->UsersNames.top;
			ThAddString(&ucn->UsersNames, name);
			ThAppend(&ucn->UsersIndex, &offset, sizeof(offset));
			index = POINTERCAST(DWORD *, ucn->UsersIndex.bottom);

			mini = midi = 0;
			maxi = indexmax = (ucn->UsersIndex.top / sizeof(DWORD)) - 1;
			while ( mini < maxi ){ // バイナリサーチで検索
				int result;

				midi = (mini + maxi) / 2;
				result = tstricmp(name,
					(TCHAR *)(BYTE *)(ucn->UsersNames.bottom + *(index + midi)));
				if ( result < 0 ){
					maxi = midi;
				}else{
					midi++;
					if ( result > 0 ){
						mini = midi;
					}else{
						break;
					}
				}
			}
			if ( midi < indexmax ){
				index += midi;
				memmove(index + 1, index, (indexmax - midi) * sizeof(DWORD));
				*index = offset;
			}
		}
	}
	ucn->UsersIndexMin = POINTERCAST(DWORD *, ucn->UsersIndex.bottom);
	ucn->UsersIndexMax = ucn->UsersIndexMin + (ucn->UsersIndex.top / sizeof(DWORD));
/*
		{
			DWORD usercount, *index;
			usercount = ucn->UsersIndex.top / sizeof(DWORD);
			index = POINTERCAST(DWORD *, ucn->UsersIndex.bottom);
			XMessage(NULL, NULL, XM_DbgLOG, T("count %d"), usercount);
			while ( usercount-- ){
				XMessage(NULL, NULL, XM_DbgLOG, T("X: %s"), (TCHAR *)(BYTE *)(ucn->UsersNames.bottom + *index));
				index++;

			}
		}
*/
}

/*-----------------------------------------------------------------------------
	カスタマイズ(書き出し)本体
-----------------------------------------------------------------------------*/
void DumpUserCusts(PPCUSTSTRUCT *PCS, USERCUSTNAMES *ucndata, const CLABEL *clbl, const TCHAR *chars)
{
	DWORD *index;
	TCHAR *name, char1, char2;
	USERCUSTNAMES *ucn;
	CLABEL clblbuf = *clbl;

	char1 = chars[0];
	char2 = chars[1];
	ucn = ucndata;
	for ( index = ucn->UsersIndexMin; index < ucn->UsersIndexMax ; index++ ){
		name = (TCHAR *)(BYTE *)(ucn->UsersNames.bottom + *index);
		if ( (name[0] != char1) || (name[1] != char2) ) continue;
		clblbuf.name = name;
		CDitem(PCS, &clblbuf);
	}
}

PPXDLL TCHAR * PPXAPI PPcustCDump(void)
{
	PPCUSTSTRUCT PCS;
	BYTE bin[MAXCUSTDATA];
	const CLABEL *clbl;
	int count, size;
	TCHAR name[VFPS];
	USERCUSTNAMES ucn;

	PCS.Smes = PCS.SmesBuf = HeapAlloc(ProcHeap, 0, FIRSTDUMPSIZE + SMESSIZE_MARGIN);
	PCS.SmesLim = PCS.Smes + FIRSTDUMPSIZE / sizeof(TCHAR);
	PCS.Dnum = 0;
	PCS.Xmode = PPXCUSTMODE_DUMP_ALL;
#ifdef UNICODE
	*PCS.Smes++ = 0xfeff;	// UCF2HEADER を設定
#endif
	MakeUserCustNames(&ucn);
//--------------------------------------------------------- 定義済み内容の出力
	clbl = Ctbl;
	while ( clbl->name ){
		switch ( *clbl->name ){
			case 0:						// Comment ----------------------------
				CheckSmes(&PCS, SMESSIZE_MES);
				tstrcpy(PCS.Smes, clbl->comment);
				PCS.Smes += tstrlen(PCS.Smes);
				PCS.Smes += wsprintf(PCS.Smes, TNL);
				break;

			case 1:						// ユーザメニュー ---------------------
				DumpUserCusts(&PCS, &ucn, &usermenu, T("M_"));
				break;

			case 2:						// ユーザファイル判別 -----------------
				DumpUserCusts(&PCS, &ucn, &userext, T("E_"));
				break;

			case 3:						// 不明言語 ---------------------------
				DumpUserCusts(&PCS, &ucn, &userlang, T("Me"));
				break;

			case 4:						// ツールバー -----------------
				DumpUserCusts(&PCS, &ucn, &userbar, T("B_"));
				break;

			case 5:						// キー割当て -----------------
				DumpUserCusts(&PCS, &ucn, &userkey, T("K_"));
				break;

			case 6:						// 任意設定 -----------------
				DumpUserCusts(&PCS, &ucn, &usersettings, T("S_"));
				break;

			default:					// 本体 -------------------------------
				if ( clbl->flag & fSort ) SortCustTable(clbl->name, NULL);
				CDitem(&PCS, clbl);
		}
		clbl++;
	}
//--------------------------------------------------------- 未定義内容の出力
	for( count = 0 ; ; count++ ){
		size = EnumCustData(count, name, bin, sizeof(bin));
		if ( 0 > size ) break;

		if ( name[1] == '_' ){
			if ( (name[0] == 'M') || (name[0] == 'E') || (name[0] == 'B') || (name[0] == 'K') || (name[0] == 'S') ){
				continue;
			}
		}
		if ( !memcmp(name, T("Mes"), TSTROFF(3)) ) continue;

		clbl = Ctbl;
		while( clbl->name ){
			if ( !tstricmp(name, clbl->name) ) break;
			clbl++;
		}
		if ( clbl->name == NULL ){
			CDsub(&PCS, name, bin, size, clbl->flag, clbl->fmt);
			PCS.Smes += wsprintf(PCS.Smes, TNL);
		}
	}
	return PCS.SmesBuf;
}

void PPcustCDumpPart(PPCUSTSTRUCT *PCS, const TCHAR *str, const TCHAR *sub)
{
	const CLABEL *clbl = NULL;
	BYTE bin[MAXCUSTDATA];
	int size;

	if ( str[1] == '_' ){
		switch ( str[0] ){
			case 'M': // ユーザメニュー -------
				clbl = &usermenu;
				break;

			case 'S': // ユーザリスト -------
				clbl = &usersettings;
				break;

			case 'K': // ユーザキー割当て -------
				clbl = &userkey;
				break;

			case 'E': // ユーザファイル判別 -------
				clbl = &userext;
				break;

			case 'B': // ツールバー -------
				clbl = &userbar;
				break;
		}
	}else if ( !memcmp(str, T("Mes"), TSTROFF(3)) ){
		clbl = &userlang;
	}
	if ( clbl == NULL ){ // 定義済み
		clbl = Ctbl;
		for (;;){
			if ( !tstricmp(clbl->name, str) ) break;
			clbl++;
			if ( clbl->name != NULL ) continue;
			clbl = NULL;
			break;
		}
	}
	if ( clbl != NULL ){
		if ( sub == NULL ){
			if ( clbl->flag & fT ){ // 配列全体
				CLABEL clblbuf = *clbl;

				clblbuf.name = str;
				CDitem(PCS, &clblbuf);
			}else{ // 単純
				size = GetCustDataSize(str);
				if ( size > 0 ){
					GetCustData(str, bin, sizeof(bin));
					CDsub(PCS, NilStr, bin, size, clbl->flag, clbl->fmt);
				}
			}
		}else{					// 配列項目
			size = GetCustTableSize(str, sub);
			if ( size > 0 ){
				GetCustTable(str, sub, bin, sizeof(bin));
				CDsub(PCS, NilStr, bin, size, clbl->flag, clbl->fmt);
			}
		}
	}
}

void PPcustCDumpWildCard(PPCUSTSTRUCT *PCS, const TCHAR *wildcard)
{
	FN_REGEXP fn;
	int count, size;
	TCHAR name[CMDLINESIZE];
	BYTE bin[4]; // 使わないので最小限

	MakeFN_REGEXP(&fn, wildcard);
	for( count = 0 ; ; count++ ){
		size = EnumCustData(count, name, bin, 0);
		if ( size < 0 ) break;
		if ( FilenameRegularExpression(name, &fn) ){
			PPcustCDumpPart(PCS, name, NULL);
		}
	}
	FreeFN_REGEXP(&fn);
}

void PPcustCDumpText(const TCHAR *str, const TCHAR *sub, TCHAR **result)
{
	PPCUSTSTRUCT PCS;

	PCS.Smes = PCS.SmesBuf = *result;
	PCS.SmesLim = PCS.Smes + CMDLINESIZE - SMESSIZE_MARGIN;
	PCS.Dnum = 0;
	PCS.Xmode = PPXCUSTMODE_DUMP_PART;
	*PCS.Smes = '\0';

	if ( *str == '#' ){
		PPcustCDumpWildCard(&PCS, str + 1);
	}else{
		PPcustCDumpPart(&PCS, str, sub);
	}
	*result = PCS.SmesBuf;
}

PPXDLL TCHAR * PPXAPI PPcust(int mode, const TCHAR *param)
{
	PPCUSTSTRUCT PCS;

	switch ( mode ){
		case PPXCUSTMODE_DUMP_ALL:
			return PPcustCDump();

		case PPXCUSTMODE_DUMP_PART: {
			PCS.Smes = PCS.SmesBuf = HeapAlloc(ProcHeap, 0, FIRSTDUMPSIZE + SMESSIZE_MARGIN);
			PCS.SmesLim = PCS.Smes + FIRSTDUMPSIZE / sizeof(TCHAR);
			PCS.Dnum = 0;
			PCS.Xmode = PPXCUSTMODE_DUMP_PART;
#ifdef UNICODE
			*PCS.Smes++ = 0xfeff;	// UCF2HEADER を設定
#endif
			*PCS.Smes = '\0';

			PPcustCDumpWildCard(&PCS, param);
			return PCS.SmesBuf;
		}
		default:
			return NULL;
	}
}
