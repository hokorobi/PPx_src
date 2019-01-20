/*-----------------------------------------------------------------------------
	Paper Plane xUI	customizer							PPc表示書式
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include "PPX.H"
#include "PPD_DEF.H"
#include "PPD_CUST.H"
#include "PPC_DISP.H"
#pragma hdrstop

COLORREF CS_color(TCHAR **linesrc, int flags)			// キーワードを抽出
{
	return GetColor((const TCHAR **)linesrc, !(flags & fRC)); // A_color は自己参照させない
}

int CD_color(TCHAR *dest, COLORREF color, int flags)
{
	int i;
	// 追加識別子を判別
	if ( !(flags & fRC) ){	// A_color は自己参照させない
		i = 0;
		for ( ; ; ){
			TCHAR name[MAX_PATH];
			COLORREF rc;
			int r;

			r = EnumCustTable(i, T("A_color"), name, &rc, sizeof(rc));
			if ( 0 > r ) break;
			if ( color == rc ) return wsprintf(dest, name);
			i++;
		}
	}

	for ( i = 0; i < color_s; i++ ){
		if ( color == guicolor[i].num ) return wsprintf(dest, guicolor[i].str);
	}
	return wsprintf(dest, T("H%06X"), color);
}

void WidthCheck(PPCUSTSTRUCT *PCS, int nowline, int maxws, int ws)
{
	TCHAR buf[MAX_PATH];

	wsprintf(buf, MessageText(MES_WUML), nowline, ws, maxws);
	WarningMes(PCS, buf);
}

// エントリ表示書式 -----------------------------------------------------------
BOOL CS_ppcdisp(PPCUSTSTRUCT *PCS, TCHAR **linePtr, BYTE **binPtr)
{
	BYTE *p;			// 格納内容の末尾
	TCHAR *line;

	int nowline = 1;	// 現在の行数
	int ws = 0;			// 現在の桁数
	int maxws = 0;		// 今までの最大桁数
	int skipws = 0;		// c 指定によるコメント表示時の総桁数
	int leftmargin = 0; // 桁下げ量(アイコン表示時)
	BYTE *widthsave;	// 幅書き込み先
	int useMemoSkip = -1;	// c 指定があった場所の桁数
	BYTE *lineleft;
	BYTE *lastFM = NULL;	// 最後の FM を示す

	p = *binPtr;
	line = *linePtr;

	widthsave = p;
	p += 4;
	lineleft = p;
	*(WORD *)(lineleft - 2) = 0;
	while ( *line != '\0' ){
		SkipSPC(line);
		switch ( *line++ ){
			case ',':
				break;
			case 'S':
				*p++ = DE_SPC;
				ws += *p++ = GetNumberWith((const TCHAR **)&line, 1, 255);
				break;
			case 's':
				*p++ = DE_BLANK;
				ws += *p++ = GetNumberWith((const TCHAR **)&line, 1, 255);
				break;
			case 'M':
				*p++ = DE_MARK;
				ws++;
				break;
			case 'b':
				*p++ = DE_CHECK;
				ws += 2;
				break;
			case 'B':
				*p++ = DE_CHECKBOX;
				ws += 2;
				break;
			case 'N': {
				BYTE iconsize;

				iconsize = GetNumberWith((const TCHAR **)&line, 0, 255);
				if ( iconsize != 0 ){
					*p++ = DE_ICON2;
					*p++ = iconsize;
					leftmargin = GetIcon2Len(iconsize);
				} else{
					*p++ = DE_ICON;
					leftmargin = 2;
				}
				ws += leftmargin;
				break;
			}
			case 'n': {
				BYTE len;

				*p++ = DE_IMAGE;
				leftmargin = GetNumberWith((const TCHAR **)&line, 20, 255);
				ws += *p++ = (BYTE)leftmargin;
				len = 8;
				if ( *line == ',' ){
					line++;
					len = (BYTE)GetNumber((const TCHAR **)&line);
				}
				*p++ = len;
				break;
			}
			case 'F':
			case 'f': {
				BYTE len;

				if ( *line == 'E' ){
					*p++ = (BYTE)((*(line - 1) == 'F') ? DE_LFN_EXT : DE_SFN_EXT);
					line++;
				}else if ( *line == 'M' ){
					line++;
					lastFM = p;
					*p++ = (BYTE)DE_LFN_MUL;
				} else{
					*p++ = (BYTE)((*(line - 1) == 'F') ? DE_LFN : DE_SFN);
				}
				len = GetNumberWith((const TCHAR **)&line, 255, 255);
				ws += *p++ = len;
				len = 255;
				if ( *line == ',' ){
					line++;
					ws += len = (BYTE)GetNumber((const TCHAR **)&line);
				}
				*p++ = len;
				break;
			}
			case 'w':
			case 'W': {
				TCHAR *q;

				if ( (p > lineleft) &&
					((*lineleft == DE_WIDEV) || (*lineleft == DE_WIDEW)) ){
					ErrorMes(PCS, MES_ETMW);
					return FALSE;
				}

				memmove(lineleft + 4, lineleft, p - lineleft);
				p += 4;
				lineleft[0] =
					(BYTE)((*(line - 1) == 'W') ? DE_WIDEV : DE_WIDEW);
				lineleft[2] = GetNumberWith((const TCHAR **)&line, 255, 255);
				lineleft[3] = (BYTE)(p - lineleft + 1);
				SkipSPC(line);
				if ( (*line != 'F') && (*line != 'f') &&
					//					(tstrchr(T("ACDHRSTUVXmZnsuz"),*line) != NULL)
					!((lineleft[0] == DE_WIDEW) && (tstrchr(DE_ENABLE_FIXLENGTH, *line) != NULL)) ){
					ErrorMes(PCS, MES_EWFT);
					return FALSE;
				}
				q = line + 1;
				if ( *q == 'M' ) q++;
				lineleft[1] = GetNumberWith((const TCHAR **)&q, 255, 255);
				break;
			}
			case 'Z':
				*p++ = DE_SIZE3;
				ws += *p++ = GetNumberWith((const TCHAR **)&line, 7, 80);
				break;
			case 'z':
				if ( *line == 'K' ){
					line++;
					*p++ = DE_SIZE4;
				} else{
					*p++ = DE_SIZE2;
				}
				ws += *p++ = GetNumberWith((const TCHAR **)&line, 13, 80);
				break;
			case 'T':
				*p++ = DE_TIME1;
				ws += *p++ = GetNumberWith((const TCHAR **)&line, 17, 17);
				break;
			case 'A':
				*p++ = DE_ATTR1;
				ws += *p++ = GetNumberWith((const TCHAR **)&line, 10, 10);
				break;
			case 'C': {
				int param;

				param = GetNumberWith((const TCHAR **)&line, 255, 255);
				if ( skipws ){	// スキップあり
					*p++ = DE_MEMOS;
					*p++ = (BYTE)min(skipws, 255);
					skipws = 0;
				} else{			// スキップ無し
					*p++ = DE_MEMO;
					*p++ = (BYTE)param;
					ws += param;
				}
				break;
			}
			case 'c':
				useMemoSkip = ws;
				*p++ = DE_MSKIP;
				continue;

			case 'u':
				*p++ = DE_MEMOEX;
				*(p + 1) = GetNumberWith((const TCHAR **)&line, 1, 255); // ID
				SkipSPC(line);
				if ( *line == ',' ) line++;
				ws += *p = GetNumberWith((const TCHAR **)&line, 8, 255); // 幅
				p += 2;
				break;

			case 'L':
				*p++ = DE_sepline;
				ws++;
				break;

			case 'H':
				*p++ = DE_hline;
				ws += *p++ = GetNumberWith((const TCHAR **)&line, 0, 255);
				break;

			case 'O':
				if ( *line == 'd' ){
					line++;
					*p++ = DE_fc_def;
					*p++ = GetNumberWith((const TCHAR **)&line, 0, 0);
					break;
				} else if ( *line == 'g' ){
					line++;
					*p++ = DE_bc_def;
					*p++ = GetNumberWith((const TCHAR **)&line, 0, 0);
					break;
				} else if ( *line == 'b' ){
					line++;
					*p++ = DE_bcolor;
				} else{
					*p++ = DE_fcolor;
				}
				if ( *line == '\"' ) line++;
				*(COLORREF *)p = CS_color(&line, 0);
				p += sizeof(COLORREF);
				if ( *line == '\"' ) line++;
				break;

			case '/':
				*p++ = DE_NEWLINE;
				*p++ = 0;
				*p++ = 0;
				maxws = max(ws, maxws);
				nowline++;
				ws = leftmargin;
				skipws = 0;

				*(WORD *)(lineleft - 2) = (WORD)(p - lineleft);
				lineleft = p;
				break;

			case 'Y':
				*p++ = DE_FStype;
				ws += 3;
				break;
			case 'v':
			case 'i':
			case 'I':
				#ifdef UNICODE
				if ( !((p - *binPtr) & 1) ){	// WORD境界合わせ
					*p++ = DE_SKIP;
				}
				#endif
				{
					BYTE vt;

					switch ( *(line - 1) ){
						case 'i':
							vt = DE_string;
							break;
						case 'v':
							if ( *line == 'i' ) line++;
							vt = DE_ivalue;
							break;
						default: // 'I'
							vt = DE_itemname;
							break;
					}
					*p++ = vt;
				}
				if ( *line != '\"' ){
					ErrorMes(PCS, MES_ESEP);
					return FALSE;
				}
				line++;
				while ( ((UTCHAR)(*line) >= ' ') && (*line != '\"') ){
					#ifdef UNICODE
					TCHAR chr;
					chr = *line++;
					*p++ = (BYTE)chr;
					*p++ = (BYTE)(chr >> 8);
					ws += 2;
					#else
					*p++ = *line++;
					ws++;
					#endif
				}
				if ( *line == '\"' ) line++;
				#ifdef UNICODE
				*(TCHAR *)p = '\0';
				p += 2;
				#else
				*p++ = '\0';
				#endif
				break;
			case 'P':
				*p++ = DE_ALLPAGE;
				ws += 3;
				break;
			case 'p':
				*p++ = DE_NOWPAGE;
				ws += 3;
				break;

			case 'E':
				if ( Isdigit(*line) ){
					int n = 0;

					const BYTE d[] = {DE_ENTRYA0, DE_ENTRYA1};

					if ( Isdigit(*line) ){
						n = *line - '0';
						line++;
						if ( n > 1 ){
							ErrorMes(PCS, MES_EENM);
							return FALSE;
						}
					}
					*p++ = d[n];
					ws += 3;
				} else{
					*p++ = DE_ENTRYSET;
					ws += 7;
				}
				break;

			case 'e': {
				int n = 0;

				const BYTE d[] = {DE_ENTRYV0, DE_ENTRYV1,
					DE_ENTRYV2, DE_ENTRYV3};

				if ( Isdigit(*line) ){
					n = *line - '0';
					line++;
					if ( n > 3 ){
						ErrorMes(PCS, MES_EENM);
						return FALSE;
					}
				}
				*p++ = d[n];
				ws += 3;
				break;
			}
			case 'm':{
				BYTE deflen;

				switch ( *line ){
					case 'n':
						*p++ = DE_MNUMS;
						deflen = 0;
						ws += 3;
						break;
					case 'S':
						*p++ = DE_MSIZE1;
						deflen = 7;
						break;
					case 's':
						*p++ = DE_MSIZE2;
						deflen = 13;
						break;
					case 'K':
						*p++ = DE_MSIZE3;
						deflen = 13;
						break;
					default:
						ErrorMes(PCS, MES_EUFM);
						return FALSE;
				}
				line++;
				if ( deflen ){
					deflen = GetNumberWith((const TCHAR **)&line, deflen, 40);
					if ( deflen < 3 ) deflen = 3;
					ws += *p++ = deflen;
				}
				break;
			}
			case 'D':{
				BYTE deflen;

				switch ( *line ){
					case 'F':
						*p++ = DE_DFREE1;
						deflen = 7;
						break;
					case 'f':
						*p++ = DE_DFREE2;
						deflen = 13;
						break;
					case 'U':
						*p++ = DE_DUSE1;
						deflen = 7;
						break;
					case 'u':
						*p++ = DE_DUSE2;
						deflen = 13;
						break;
					case 'T':
						*p++ = DE_DTOTAL1;
						deflen = 7;
						break;
					case 't':
						*p++ = DE_DTOTAL2;
						deflen = 13;
						break;
					default:
						ErrorMes(PCS, MES_EUFM);
						return FALSE;
				}
				line++;
				ws += *p++ = GetNumberWith((const TCHAR **)&line, deflen, 40);
				break;
			}
			case 't':
				*p++ = DE_TIME2;
				if ( *line == 'C' ){
					line++;
					*p++ = 0;
				} else if ( *line == 'A' ){
					line++;
					*p++ = 1;
				} else{
					if ( *line == 'W' ) line++;
					*p++ = 2;
				}
				if ( *line != '\"' ){
					ErrorMes(PCS, MES_ESEP);
					return FALSE;
				}
				line++;
				while ( ((UTCHAR)(*line) >= ' ') && (*line != '\"') ){
					switch ( *line ){
						case 'y':
						case 'n':
						case 'N':
						case 'd':
						case 'D':
						case 'W':
						case 'h':
						case 'H':
						case 'm':
						case 'M':
						case 's':
						case 'S':
						case 'u':
						case 'U':
						case 't':
							ws += 2;
							break;
						case 'I':
						case 'g':
						case 'w':
						case 'a':
							ws += 3;
							break;
						case 'Y':
						case 'T':
							ws += 4;
							break;
						case 'G':
							ws += 6;
							break;
						default:
							ws++;
					}
					*p++ = (BYTE)*line++;
				}
				if ( *line == '\"' ) line++;
				*p++ = '\0';
				break;

			case 'V':
				*p++ = DE_vlabel;
				ws += *p++ = GetNumberWith((const TCHAR **)&line, 14, 255);
				break;

			case 'R':
				if ( *line == 'M' ){
					line++;
					*p++ = DE_pathmask;
				} else{
					*p++ = DE_path;
				}
				ws += *p++ = GetNumberWith((const TCHAR **)&line, 60, 255);
				break;

			case 'U': {
				TCHAR *dst;

				*p++ = DE_COLUMN;
				dst = ((DISPFMT_COLUMN *)p)->name;
				if ( *line == '\"' ){
					line++;
					while ( *line ){
						if ( *line == '\"' ){
							line++;
							break;
						}
						*dst++ = *line++;
					}
				}
				*dst++ = '\0';
				if ( *line == ',' ) line++;
				ws += ((DISPFMT_COLUMN *)p)->width = GetNumberWith((const TCHAR **)&line, 10, 255);
				((DISPFMT_COLUMN *)p)->itemindex = DFC_UNDEF;
				((DISPFMT_COLUMN *)p)->fmtsize = (BYTE)(((BYTE *)dst - p) + 1);
				p = (BYTE *)dst;
				break;
			}

			case 'X': {
				BYTE *dst;
				BYTE lines;
				TCHAR c;
				DWORD hash = 0;

				*p++ = DE_MODULE;
				// 名前
				dst = p + 2 + 4;
				if ( *line == '\"' ){
					line++;
					while ( *line ){
						c = upper(*line++);
						if ( c == '\"' ){
							line++;
							break;
						}
						*dst++ = (BYTE)c;
						hash = (DWORD)(hash << 6) | (DWORD)(hash >> (32 - 6)) | (DWORD)c;
					}
				}
				*(p + 4 + 8) = '\0';
				*dst = '\0';
				// ハッシュ
				*(DWORD *)(p + 2) = hash | B31;
				// 桁数
				if ( *line == ',' ) line++;
				ws += *p = GetNumberWith((const TCHAR **)&line, 4, 255);
				// 行数
				lines = 1;
				if ( *line == ',' ){
					line++;
					lines = (BYTE)GetNumber((const TCHAR **)&line);
				}
				*(p + 1) = lines;
				p += 4 + 8 + 2 + 1;
				break;
			}

			default:
				ErrorMes(PCS, MES_EUFM);
				return FALSE;
		}
		if ( (size_t)(p - widthsave) > (0x10000 - 0x100) ){
			ErrorMes(PCS, MES_EFLW);
			return FALSE;
		}
		if ( useMemoSkip >= 0 ){	// 直前が c ならスキップ幅算出
			skipws += ws - useMemoSkip;
			useMemoSkip = -1;
		}
	}
	*p++ = 0;

	if ( maxws && (ws < maxws) ) WidthCheck(PCS, nowline, maxws, ws);
	*(WORD *)widthsave = (WORD)max(ws, maxws);
	if ( lastFM != NULL ) *lastFM = (BYTE)DE_LFN_LMUL;
	*binPtr = p;
	*linePtr = line;
	return TRUE;
}


void CD_ppcdisp(PPCUSTSTRUCT *PCS, BYTE **binPtr, BYTE *binend)
{
	int widev = 0;
	BYTE widecode = DE_WIDEW, *widec = NULL, *bin;

	bin = *binPtr;
	bin += 4;
	for ( ;;){
		BYTE binc;

		if ( bin >= binend ) break;
		binc = *bin++;
		if ( binc == 0 ) break;

		if ( widec == bin ){
			*PCS->Smes++ = (BYTE)((widecode == DE_WIDEV) ? 'W' : 'w');
			if ( widev < 255 ){
				PCS->Smes += wsprintf(PCS->Smes, T("%d"), widev);
			}
		}

		switch ( binc ){
			case DE_SPC:
				PCS->Smes += wsprintf(PCS->Smes, T("S%d"), *bin);
				bin++;
				break;
			case DE_BLANK:
				PCS->Smes += wsprintf(PCS->Smes, T("s%d"), *bin);
				bin++;
				break;
			case DE_MARK:
				*PCS->Smes++ = 'M';
				break;
			case DE_CHECK:
				*PCS->Smes++ = 'b';
				break;
			case DE_CHECKBOX:
				*PCS->Smes++ = 'B';
				break;
			case DE_ICON:
				*PCS->Smes++ = 'N';
				break;
			case DE_ICON2:
				PCS->Smes += wsprintf(PCS->Smes, T("N%d"), *bin);
				bin++;
				break;
			case DE_IMAGE:
				PCS->Smes += wsprintf(PCS->Smes, T("n%d,%d"), *bin, *(bin + 1));
				bin += 2;
				break;
			case DE_LFN:
			case DE_SFN:
			case DE_LFN_MUL:
			case DE_LFN_LMUL:
			case DE_LFN_EXT:
			case DE_SFN_EXT:
			{
				int i;

				*PCS->Smes++ = (BYTE)((*(bin - 1) == DE_SFN) ? 'f' : 'F');
				if ( *(bin - 1) >= DE_LFN_MUL ){
					if ( *(bin - 1) == DE_SFN_EXT ) *(PCS->Smes - 1) = 'f';
					*PCS->Smes++ = (*(bin - 1) >= DE_LFN_EXT) ? (BYTE)'E' : (BYTE)'M';
				}
				i = *bin++;						// ファイル名
				if ( i != 255 ){
					PCS->Smes += wsprintf(PCS->Smes, T("%d"), i);
				}
				i = *bin++;						// 拡張子
				if ( i != 255 ){
					*PCS->Smes++ = ',';
					PCS->Smes += wsprintf(PCS->Smes, T("%d"), i);
				}
				break;
			}
			case DE_SIZE1:
				*PCS->Smes++ = 'Z';
				break;
			case DE_SIZE2:
				PCS->Smes += wsprintf(PCS->Smes, T("z%d"), *bin);
				bin++;
				break;
			case DE_SIZE3:
				PCS->Smes += wsprintf(PCS->Smes, T("Z%d"), *bin);
				bin++;
				break;
			case DE_SIZE4:
				PCS->Smes += wsprintf(PCS->Smes, T("zK%d"), *bin);
				bin++;
				break;
			case DE_TIME1:
				PCS->Smes += wsprintf(PCS->Smes, T("T%d"), *bin);
				bin++;
				break;
			case DE_TIME2:
				*PCS->Smes++ = 't';
				switch ( *bin++ ){
					case 0:
						*PCS->Smes++ = 'C';
						break;
					case 1:
						*PCS->Smes++ = 'A';
						break;
					case 2:
						*PCS->Smes++ = 'W';
						break;
				}
				*PCS->Smes++ = '\"';
				while ( *bin ) *PCS->Smes++ = *bin++;
				bin++;
				*PCS->Smes++ = '\"';
				break;
			case DE_ATTR1:
				PCS->Smes += wsprintf(PCS->Smes, T("A%d"), *bin);
				bin++;
				break;
			case DE_MEMOS:
				*PCS->Smes++ = 'C';
				bin++;	// 桁数は出力しない
				break;
			case DE_MEMO: {
				int i;
				*PCS->Smes++ = 'C';
				i = *bin++;
				if ( i != 255 ){
					PCS->Smes += wsprintf(PCS->Smes, T("%d"), i);
				}
				break;
			}
			case DE_MSKIP:
				*PCS->Smes++ = 'c';
				continue;
			case DE_MEMOEX:
				PCS->Smes += wsprintf(PCS->Smes, T("u%d,%d"), *(bin + 1), *bin);
				bin += 2;
				break;
			case DE_sepline:
				*PCS->Smes++ = 'L';
				break;
			case DE_hline:
				PCS->Smes += wsprintf(PCS->Smes, T("H%d"), *bin);
				bin++;
				break;

			case DE_fcolor:
			case DE_bcolor:
				*PCS->Smes++ = 'O';
				if ( *(bin - 1) == DE_bcolor ) *PCS->Smes++ = 'b';
				*PCS->Smes++ = '\"';
				PCS->Smes += CD_color(PCS->Smes, *(COLORREF *)bin, 0);
				*PCS->Smes++ = '\"';
				bin += sizeof(COLORREF);
				break;

			case DE_fc_def:
				PCS->Smes += wsprintf(PCS->Smes, T("Od%d"), *bin);
				bin++;
				break;
			case DE_bc_def:
				PCS->Smes += wsprintf(PCS->Smes, T("Og%d"), *bin);
				bin++;
				break;

			case DE_NEWLINE:
				*PCS->Smes++ = '/';
				bin += 2;
				break;
			case DE_FStype:
				*PCS->Smes++ = 'Y';
				break;
			case DE_ivalue:
			case DE_itemname:
			case DE_string: {
				TCHAR *str, vc;

				switch ( *(bin - 1) ){
					case DE_string:
						vc = 'i';
						break;
					case DE_ivalue:
						*PCS->Smes++ = 'v';
						vc = 'i';
						break;
					default: // DE_itemname
						vc = 'I';
						break;
				}
				*PCS->Smes++ = vc;
				*PCS->Smes++ = '\"';
				str = (TCHAR *)bin;
				while ( *str ) *PCS->Smes++ = *str++;
				bin = (BYTE *)(TCHAR *)(str + 1);
				*PCS->Smes++ = '\"';
				break;
			}

			case DE_ALLPAGE:
				*PCS->Smes++ = 'P';
				break;
			case DE_NOWPAGE:
				*PCS->Smes++ = 'p';
				break;
			case DE_MNUMS:
				*PCS->Smes++ = 'm';
				*PCS->Smes++ = 'n';
				break;
			case DE_ENTRYSET:
				*PCS->Smes++ = 'E';
				break;
			case DE_ENTRYV0:
				*PCS->Smes++ = 'e';
				break;
			case DE_ENTRYV1:
				*PCS->Smes++ = 'e';
				*PCS->Smes++ = '1';
				break;
			case DE_ENTRYV2:
				*PCS->Smes++ = 'e';
				*PCS->Smes++ = '2';
				break;
			case DE_ENTRYV3:
				*PCS->Smes++ = 'e';
				*PCS->Smes++ = '3';
				break;
			case DE_ENTRYA0:
				*PCS->Smes++ = 'E';
				*PCS->Smes++ = '0';
				break;
			case DE_ENTRYA1:
				*PCS->Smes++ = 'E';
				*PCS->Smes++ = '1';
				break;
			case DE_MSIZE1:
				PCS->Smes += wsprintf(PCS->Smes, T("mS%d"), *bin);
				bin++;
				break;
			case DE_MSIZE2:
				PCS->Smes += wsprintf(PCS->Smes, T("ms%d"), *bin);
				bin++;
				break;
			case DE_MSIZE3:
				PCS->Smes += wsprintf(PCS->Smes, T("mK%d"), *bin);
				bin++;
				break;
			case DE_DFREE1:
				PCS->Smes += wsprintf(PCS->Smes, T("DF%d"), *bin);
				bin++;
				break;
			case DE_DFREE2:
				PCS->Smes += wsprintf(PCS->Smes, T("Df%d"), *bin);
				bin++;
				break;
			case DE_DUSE1:
				PCS->Smes += wsprintf(PCS->Smes, T("DU%d"), *bin);
				bin++;
				break;
			case DE_DUSE2:
				PCS->Smes += wsprintf(PCS->Smes, T("Du%d"), *bin);
				bin++;
				break;
			case DE_DTOTAL1:
				PCS->Smes += wsprintf(PCS->Smes, T("DT%d"), *bin);
				bin++;
				break;
			case DE_DTOTAL2:
				PCS->Smes += wsprintf(PCS->Smes, T("Dt%d"), *bin);
				bin++;
				break;
			case DE_WIDEW:
			case DE_WIDEV:
				widecode = *(bin - 1);
				widev = *(bin + 1);
				widec = bin + *(bin + 2) - 1;
				*widec = *bin;	// Fn を元に戻す
				bin += 3;
				continue;
			case DE_vlabel:
				PCS->Smes += wsprintf(PCS->Smes, T("V%d"), *bin);
				bin++;
				break;
			case DE_path:
				PCS->Smes += wsprintf(PCS->Smes, T("R%d"), *bin);
				bin++;
				break;
			case DE_pathmask:
				PCS->Smes += wsprintf(PCS->Smes, T("RM%d"), *bin);
				bin++;
				break;
			case DE_COLUMN: {
				int nextoffset;

				PCS->Smes += wsprintf(PCS->Smes, T("U\"%s\",%d"),
					((DISPFMT_COLUMN *)bin)->name,
					((DISPFMT_COLUMN *)bin)->width);
				nextoffset = ((DISPFMT_COLUMN *)bin)->fmtsize - 1;
				if ( nextoffset >= 0 ) bin += nextoffset;
				break;
			}
			case DE_MODULE: {
				#ifdef UNICODE
				WCHAR bufw[8];

				AnsiToUnicode((char *)(bin + 2 + 4), bufw, 8);
				#define CNAME bufw
				#else
				#define CNAME (bin + 2 + 4)
				#endif
				PCS->Smes += wsprintf(PCS->Smes, T("X\"%s\",%d,%d"),
					CNAME, *bin, *(bin + 1));
				bin += 16 - 1;
				break;
			}
			case DE_SKIP:
				continue;	// 空白を挿入させない

			default:
				goto term;
		}
		if ( *bin ) *PCS->Smes++ = ' ';
	}
term:
	*binPtr = bin;
	*PCS->Smes = '\0';
}
