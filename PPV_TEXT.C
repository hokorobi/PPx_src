/*-----------------------------------------------------------------------------
	Paper Plane vUI				Text
-----------------------------------------------------------------------------*/
#pragma setlocale("Japanese")
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop


/*
	CharNextExA

	base64
	↓
	=xx,%xx
	↓
	S-JIS/UNICODE/JIS
	↓
	tag
*/

typedef struct {
	BYTE *dest, *destmax; // 書き込み位置と、書き込みバッファ末尾
	BYTE *textfirst; // 表示文字列先頭
	BYTE *CalcTextPtr; // 文字列幅計算基準位置
	int cnt;
	int PxWidth;
	HDC hDC;
	int vcode;		// 現在の buf への書き出しモード
	BOOL paintmode;
	int Fclr, Bclr;	// 前景色,背景色
} TEXTCODEINFO;

#pragma warning(disable:4245) // VCで、カタカナなどの定義で警告が出るのを抑制
WORD NECCHAR[2][0xc0] = {
// 8540
{'!','"','#','$','%','&','\'',
'(',')','*','+',',','-','.','/',
'0','1','2','3','4','5','6','7',
'8','9',':',';','<','=','>','?',
'@','A','B','C','D','E','F','G',
'H','I','J','K','L','M','N','O',
'P','Q','R','S','T','U','V','W',
'X','Y','Z','[','\\',']','^','_',0x4081, // '　',
'`','a','b','c','d','e','f','g',
'h','i','j','k','l','m','n','o',
'p','q','r','s','t','u','v','w',
'x','y','z','{','|','}','~',
// 859f
0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7, // '｡','｢','｣','､','･','ｦ','ｧ',
0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf, // 'ｨ','ｩ','ｪ','ｫ','ｬ','ｭ','ｮ','ｯ',
0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7, // 'ｰ','ｱ','ｲ','ｳ','ｴ','ｵ','ｶ','ｷ',
0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf, // 'ｸ','ｹ','ｺ','ｻ','ｼ','ｽ','ｾ','ｿ',
0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7, // 'ﾀ','ﾁ','ﾂ','ﾃ','ﾄ','ﾅ','ﾆ','ﾇ',
0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf, // 'ﾈ','ﾉ','ﾊ','ﾋ','ﾌ','ﾍ','ﾎ','ﾏ',
0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7, // 'ﾐ','ﾑ','ﾒ','ﾓ','ﾔ','ﾕ','ﾖ','ﾗ',
0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf, // 'ﾘ','ﾙ','ﾚ','ﾛ','ﾜ','ﾝ','ﾞ','ﾟ',
0x9083,0x9183,0xdc,0xb6,0xb9,0x9483,0x4b83,0x4d83, // 'ヰ','ヱ','ﾜ','ｶ','ｹ','ヴ','ガ','ギ',
0x4f83,0x5183,0x5383,0x5583,0x5783,0x5983,0x5b83,0x5d83, // 'グ','ゲ','ゴ','ザ','ジ','ズ','ゼ','ゾ',
0x5f83,0x6183,0x6483,0x6683,0x6883,0x6f83,0x7083,0x7283, // 'ダ','ヂ','ヅ','デ','ド','バ','パ','ビ',
0x7383,0x7583,0x7683,0x7883,0x7983,0x7b83,0x7c83, // 'ピ','ブ','プ','ベ','ペ','ボ','ポ',
0x4581,0x4581,0x4581}, // '・','・','・'
// 8640
{' ','"','"','-','-','|','|',
'-','-','|','|','-','-','|','|',
'+','+','+','+','+','+','+','+',
'+','+','+','+','+','+','+','+',
'+','+','+','+','+','+','+','+',
'+','+','+','+','+','+','+','+',
'+','+','+','+','+','+','+','+',
'+','+','+','+','+','+','+','+',0x4081, // '　',
'+','+','+','+','+','+','+','+',
'+','+','+','+','+','+','+','+',
'`','･','\'','"','[',']','<','>',
'<','>','｢','｣','[',']','-',
// 869f
0x4081,0x4081,0x4081,0x9f84,0xaa84,0xa084,0xab84, // '　','　','　','─','━','│','┃',
0x9f84,0xaa84,0xa084,0xab84,0x9f84,0xaa84,0xa084,0xab84, // '─','━','│','┃','─','━','│','┃',
0xa184,0xa184,0xa184,0xac84,0xa284,0xa284,0xa284,0xad84, // '┌','┌','┌','┏','┐','┐','┐','┓',
0xa484,0xa484,0xa484,0xaf84,0xa384,0xa384,0xa384,0xae84, // '└','└','└','┗','┘','┘','┘','┛',
0xa584,0xba84,0xa584,0xa584,0xb584,0xa584,0xa584,0xb084, // '├','┝','├','├','┠','├','├','┣',
0xa784,0xbc84,0xa784,0xa784,0xb784,0xa784,0xa784,0xb284, // '┤','┥','┤','┤','┨','┤','┤','┫',
0xa684,0xa684,0xa684,0xb684,0xbb84,0xa684,0xa684,0xb184, // '┬','┬','┬','┯','┰','┬','┬','┳',
0xa884,0xa884,0xa884,0xb884,0xbd84,0xa884,0xa884,0xb384, // '┴','┴','┴','┷','┸','┴','┴','┻',
0xa984,0xa984,0xa984,0xb984,0xa984,0xa984,0xbe84,0xa984, // '┼','┼','┼','┿','┼','┼','╂','┼',
0xb484,0xb484,0xb484,0xb484,0xb484,0xb484,0xb484,0xb484, // '╋','╋','╋','╋','╋','╋','╋','╋',
0x4581,0x4581,0x4581,0x4581,0x4581,0x4581,0x4581,0x4581, // '・','・','・','・','・','・','・','・',
0x4581,0x4581,0x4581,0x4581,0x4581,0x4581,0x4581, // '・','・','・','・','・','・','・',
0x4581,0x4581,0x4581, // '・','・','・'
}};

#define SJIS1st(c) ((BYTE)(c & 0xff))
#define SJIS2nd(c) ((BYTE)(c >> 8))

typedef struct {
	char *name;
	WCHAR uchar;
} UCHARS;
UCHARS unichars[] = {
	{"rlm;",	20},

	{"iexcl;",	161},
	{"cent;",	162},
	{"pound;",	163},
	{"curren;",	164},
	{"yen;",	165},
	{"brvbar;",	166},
	{"sect;",	167},
	{"uml;",	168},
	{"copy;",	169},
	{"laquo;",	171},
	{"not;",	172},
	{"shy;",	173},
	{"reg;",	174},
	{"macr;",	175},
	{"deg;",	176},
	{"plusmn;",	177},
	{"acute;",	180},
	{"micro;",	181},
	{"para;",	182},
	{"middot;",	183},
	{"raquo;",	187},
	{"iquest;",	191},
	{"times;",	215},
	{"divide;",	247},
	{"circ;",	710},
	{"tilde;",	732},
	{"fnof;",	402},
	{"Alpha;",	913},
	{"Beta;",	914},
	{"Gamma;",	915},
	{"Delta;",	916},
	{"Epsilon;",917},
	{"Zeta;",	918},
	{"Eta;",	919},
	{"Theta;",	920},
	{"Iota;",	921},
	{"Kappa;",	922},
	{"Lambda;",	923},
	{"Mu;",		924},
	{"Nu;",		925},
	{"Xi;",		926},
	{"Omicron;",927},
	{"Pi;",		928},
	{"Rho;",	929},
	{"Sigma;",	931},
	{"Tau;",	932},
	{"Upsilon;",933},
	{"Phi;",	934},
	{"Chi;",	935},
	{"Psi;",	936},
	{"Omega;",	937},
	{"alpha;",	945},
	{"beta;",	946},
	{"gamma;",	947},
	{"delta;",	948},
	{"epsilon;",949},
	{"zeta;",	950},
	{"eta;",	951},
	{"theta;",	952},
	{"iota;",	953},
	{"kappa;",	954},
	{"lambda;",	955},
	{"mu;",		956},
	{"nu;",		957},
	{"xi;",		958},
	{"omicron;",959},
	{"pi;",		960},
	{"rho;",	961},
	{"sigmaf;",	962},
	{"sigma;",	963},
	{"tau;",	964},
	{"upsilon;",965},
	{"phi;",	966},
	{"chi;",	967},
	{"psi;",	968},
	{"omega;",	969},
	{"thetasym;",977},
	{"upsih;",	978},
	{"piv;",	982},
	{"ensp;",	8194},
	{"emsp;",	8195},
	{"thinsp;",	8201},
	{"mdash;",	8212},
	{"ldquo;",	8220},
	{"rdquo;",	8221},
	{"bull;",	8226},
	{"hellip;",	8230},
	{"permil;",	8240},
	{"euro;",	8364},
	{"larr;",	8592},
	{"uarr;",	8593},
	{"rarr;",	8594},
	{"darr;",	8595},
	{"harr;",	8596},
	{"crarr;",	8629},
	{"lArr;",	8656},
	{"uArr;",	8657},
	{"rArr;",	8658},
	{"dArr;",	8659},
	{"hArr;",	8660},
	{"forall;",	8704},
	{"part;",	8706},
	{"exist;",	8707},
	{"empty;",	8709},
	{"nabla;",	8711},
	{"isin;",	8712},
	{"notin;",	8713},
	{"ni;",		8715},
	{"prod;",	8719},
	{"sum;",	8721},
	{"minus;",	8722},
	{"lowast;",	8727},
	{"radic;",	8730},
	{"prop;",	8733},
	{"infin;",	8734},
	{"ang;",	8736},
	{"and;",	8743},
	{"or;",		8744},
	{"cap;",	8745},
	{"cup;",	8746},
	{"sub;",	8834},
	{"sup;",	8835},
	{"nsub;",	8836},
	{"sube;",	8838},
	{"supe;",	8839},
	{"loz;",	9674},
	{"spades;",	9824},
	{"clubs;",	9827},
	{"hearts;",	9829},
	{"diams;",	9830},
	{NULL,		0}
};

typedef struct {
	char *name;
	WORD achar;
} ACHARS;

ACHARS anschars[] = {
	{"lt;",	'<'},
	{"gt;",	'>'},
	{"quot;",	'\"'},
	{"amp;",	'&'},
	{"nbsp;",	' '},
	{"apos;",	'\''},
	{"copy;",	'c' + ')' * 0x100}, // (c)
	{"reg;",	'r' + ')' * 0x100}, // (r)
	{"larr;",	'<' + '-' * 0x100}, // ←
	{"rarr;",	'-' + '>' * 0x100}, // →
	{NULL,		0}
};

#define ISTEP	1024
#define DESTBUFSIZE	1024

int B64DecodeBytes(BYTE **src,BYTE *dst);
int USEFASTCALL GetHex(BYTE c);
void SetUchar(TEXTCODEINFO *tci,DWORD code);
void USEFASTCALL SetVcode(TEXTCODEINFO *tci,int setvcode);
int GetHexNumberA(const char **ptr);

typedef struct {
	DWORD cp;
	char *cptext;
} CPTEXTS;

CPTEXTS cptext[] = {
	{VTYPE_JIS,"iso-2022-jp"},
	{28591,"iso-8859-1"},
	{VTYPE_UTF7,"UTF-7"},
	{VTYPE_UTF8,"UTF-8"},
	{0,NULL}
};

const char ReplaceChar[] = "・";

void USEFASTCALL CloseVcode(TEXTCODEINFO *tci);

#ifdef UNICODE
int GetIntNumberA(const char **line)
{
	int n = 0;

	while(Isdigit(**line)){
		n = n * 10 + (BYTE)((BYTE)*(*line)++ - (BYTE)'0');
	}
	return n;
}
#else
#define GetIntNumberA GetNumber
#endif

//                         17,18,19,20,21,22,23
const int ESC_NEC_C1[] = {  1, 3, 6, 2, 4, 5, 7};
//                     30,31,32,33,34,35,36,37,38
const int ESC_C2[] = {  0, 1, 2, 4, 3, 6, 5, 7, 7};

BOOL EscColor(BYTE **text, TEXTCODEINFO *tci, int dcode)
{
	int n,nfg = CV__deftext,nbg = CV__defback;
	BOOL rev = FALSE;
	BYTE *pp;

	pp = *text;
	pp++;

	if ( (*pp == '3') || (*pp == '4') ){ // 3x,4x
		BYTE RGB[4];
		char mode = *pp;

		if ( memcmp(pp + 1,"8;2;",4) == 0 ){ // 38;2;r;g;b / 48;2;r;g;b
			pp += 5;
			RGB[0] = (BYTE)(GetIntNumberA((const char **)&pp) ); // R
			if ( *pp == ';' ){
				pp++;
				RGB[1] = (BYTE)(GetIntNumberA((const char **)&pp) ); // G
				if ( *pp == ';' ){
					pp++;
					RGB[2] = (BYTE)(GetIntNumberA((const char **)&pp) ); // B
					if ( *pp != 'm' ) return FALSE;
					*text = pp + 1;
					RGB[3] = 0;
					CloseVcode(tci);
					tci->vcode = VCODE_END;
					*tci->dest = (BYTE)((mode == '3') ? VCODE_FCOLOR : VCODE_BCOLOR);
					*(DWORD *)(tci->dest + 1) = *(DWORD *)&RGB;
					tci->dest += 5;
					tci->textfirst = tci->dest;
					return TRUE;
				}
			}
		}else if ( memcmp(pp + 1,"8;5;",4) == 0 ){ // 38;5;x / 48;5;x
			pp += 5;
			n = GetIntNumberA((const char **)&pp);
			if ( *pp != 'm' ) return FALSE;

			*text = pp + 1;
			if ( n < 16 ){
				if ( n & 7 ){
					BYTE Light = ( n >= 8 ) ? (BYTE)0xff : (BYTE)0x80;
					RGB[0] = (n & B0) ? Light : (BYTE)0; // R
					RGB[1] = (n & B1) ? Light : (BYTE)0; // G
					RGB[2] = (n & B2) ? Light : (BYTE)0; // B
				}else{
					RGB[0] = RGB[1] = RGB[2] = (BYTE)( (n >= 8) ? 0x60 : 0x40);
				}
			}else if ( n < 232 ){
				n -= 16;
				RGB[0] = (BYTE)((((n / (6 * 6)) % 6) + 1) * 32); // R
				RGB[1] = (BYTE)((((n / 6) % 6) + 1) * 32); // G
				RGB[2] = (BYTE)(((n % 6) + 1) * 32); // B
			}else if ( n <= 255 ){
				RGB[0] = RGB[1] = RGB[2] = (BYTE)((n - 232) * 255 / 23);
			}else{
				return FALSE;
			}
			RGB[3] = 0;
			CloseVcode(tci);
			tci->vcode = VCODE_END;
			*tci->dest = (BYTE)((mode == '3') ? VCODE_FCOLOR : VCODE_BCOLOR);
			*(DWORD *)(tci->dest + 1) = *(DWORD *)&RGB;
			tci->dest += 5;
			tci->textfirst = tci->dest;
			return TRUE;
		}
	}

	for (;;){
		n = GetIntNumberA((const char **)&pp);

		if ( (dcode == VTYPE_SJISNEC) && (n >= 17) && (n <= 23) ){
			// 21～23 ( NEC98 )
			nfg = ESC_NEC_C1[n - 17];
		}else if ( n >= 30 ){ // 30～39,40～49,90～99,100～109
			int Light;
			BOOL Back;

			Light = 8;
			Back = FALSE;

			n -= 30;
			if ( n >= (90 - 30) ){ // 90～99,100～109
				n -= (90 - 30);
				Light = 0;
			}
			if ( n >= (40 - 30) ){
				if ( n <= (49 - 30) ){ // 40～49,100～109
					Back = TRUE;
					n -= 10;
				}else{
					n = -1;
				}
			}
			if ( (n >= 0) && (n <= 8) ){
				Light += ESC_C2[n];
				if ( Back ){
					nbg = Light;
					if ( dcode == VTYPE_SJISNEC ) nfg = CV__defback;
				}else{
					nfg = Light;
					if ( dcode == VTYPE_SJISNEC ) nbg = CV__defback;
				}
			}
		}else switch ( n ){
			case 0: // 元に戻す
			case 28: //隠し解除
				nfg = CV__deftext;
			case 21: // 太字解除
			case 22: // 暗/縦線解除
			case 23: // 斜体解除
			case 24: // 下線解除
			case 25: // 点滅解除
			case 26: // 高速点滅解除
			case 27: // 反転解除
				rev = FALSE;
				break;
			case 1: // 太字
			case 2: // 暗/縦線
			case 3: // 斜体
			case 4: // 下線
			case 5: // 点滅
			case 6: // 高速点滅
			case 7: // 反転
				rev = TRUE;
				break;
			case 8: // 隠し
				nfg = 3; // (青)
				rev = FALSE;
				break;
//			case 9: // 取り消し線
		}
		if ( *pp != ';' ) break;
		pp++;
	}
	if ( *pp != 'm' ) return FALSE; // 書式エラー
	*text = pp + 1;
	if ( rev ){
		tci->Fclr = nbg;
		tci->Bclr = nfg;
	}else{
		tci->Fclr = nfg;
		tci->Bclr = nbg;
	}
	CloseVcode(tci);
	tci->vcode = VCODE_END;
	*tci->dest++ = VCODE_COLOR;
	*tci->dest++ = (BYTE)tci->Fclr;
	*tci->dest++ = (BYTE)tci->Bclr;
	tci->textfirst = tci->dest;
	return TRUE;
}

const TCHAR CodePageListPath[] = T("Software\\Classes\\MIME\\Database\\Charset");
const TCHAR CodePageName[] = T("InternetEncoding");
const TCHAR CodePageAliasName[] = T("AliasForCharset");
DWORD GetCodePage(BYTE **text)
{
#define CPBUFSIZE 20
	CPTEXTS *cpt;
	BYTE *textsrc;
	HKEY HKroot,HKitem;
	DWORD cp = 0;
	BOOL loop = FALSE;
	TCHAR textbuf[CPBUFSIZE],*textdest;

	for ( cpt = cptext ; cpt->cp ; cpt++ ){
		size_t len;

		len = strlen(cpt->cptext);
		if ( !memicmp(*text,cpt->cptext,len) ){
			if ( (cpt->cp >= VTYPE_MAX) && !IsValidCodePage(cpt->cp) ){
				return 0;
			}
			*text += len;
			return cpt->cp;
		}
	}
	// レジストリの MIME から判別する(Win2k/Win98以降)
	textsrc = *text;
	textdest = textbuf;
	for (;;){
		BYTE chr;

		chr = *textsrc;
		if ( (chr <= '\"') || (chr == '?') || (chr == ';') ) break;
		if ( textdest >= (textbuf + CPBUFSIZE - 2) ) return 0;
		*textdest++ = (TCHAR)chr;
		textsrc++;
	}
	*textdest = '\0';
	if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE,CodePageListPath,0,KEY_READ,&HKroot) !=
			ERROR_SUCCESS ){
		return 0;
	}
	for (;;){
		DWORD s,t;

		if ( RegOpenKeyEx(HKroot,textbuf,0,KEY_READ,&HKitem) != ERROR_SUCCESS ){
			break;
		}

		s = sizeof cp;
		if ( ERROR_SUCCESS == RegQueryValueEx(HKitem,CodePageName,NULL,&t,(LPBYTE)&cp,&s) ){
			if ( cp != 0 ) *text += textdest - textbuf;
		}else{
			s = sizeof textbuf;
			if ( (loop == FALSE) && (ERROR_SUCCESS == RegQueryValueEx(HKitem,CodePageAliasName,NULL,&t,(LPBYTE)&textbuf,&s)) ){
				loop = TRUE;
				RegCloseKey(HKitem);
				continue;
			}
		}
		RegCloseKey(HKitem);
		break;
	}
	RegCloseKey(HKroot);
	return cp;
}


BOOL MimeDecode(BYTE **text,BYTE **textmax,int *code,BYTE **text2,BYTE **max2,int *code2,BYTE *destbuf)
{
	BYTE *tp;
	int codetype;
	BYTE *p,*dest;
	int l;

	// ?ISO-2022-JP?B?
	//  ^
	tp = *text + 2;

	codetype = GetCodePage(&tp);
	if ( (codetype != 0) && (*tp == '?') ){
		tp++;

		if ( (codetype == VTYPE_JIS) || (codetype == CP__JIS) || (codetype == 50225/*iso-2022-kr*/ ) ){
			codetype = VTYPE_SYSTEMCP; // 本来は VTYPE_JIS だが、コード切り替え前なので
		}

		if ( codetype >= VTYPE_MAX ){
			VO_CodePage = codetype;
			codetype = VTYPE_OTHER;
		}
	}else{
		return FALSE;
	}
	if ( *(tp + 1) != (BYTE)'?' ) return FALSE;

	p = tp + 2;
	dest = destbuf;
	l = DESTBUFSIZE - 4;

	if ( upper((char)*tp) == 'B' ){	// BASE64
		int len,leni;

		do{
			len = B64DecodeBytes(&p,dest);
			if ( len == 0 ) break;
			leni = len;
			do {
				dest++;
				leni--;
			} while( leni != 0 );
	//		if ( dest >= (BYTE *)destmax ) break;
		}while( len == 3 );
	}else if ( upper((char)*tp) == 'Q' ){	// quoted
		while( (p < *textmax) && (l > 0) ){
			if ( *p == '?' ) break;
			if ( *p != '=' ){
				*dest++ = *(BYTE *)p++;
				l--;
				continue;
			}
			p++;
			if ( (*p == '\r') && (*(p+1) == '\n') ){
				p += 2;
				continue;
			}
			if ( IsxdigitA(*p) && IsxdigitA(*(p+1)) ){
				*dest++ = (BYTE)((GetHex(*p) * 16) + GetHex(*(p+1)));
				p += 2;
				l--;
				continue;
			}
			return FALSE;
		}
	}else{
		return FALSE;
	}
	if ( (*p == '?') && (*(p+1) == '=' ) ) p += 2;
	*text2 = p;
	*max2 = *textmax;
	*code2 = *code;

	*text = destbuf;
	*textmax = dest;
	*code = codetype;
	return TRUE;
}

// Base 64 decode -------------------------------------------------------------
#define B64FLAG_D	B6
#define B64FLAG_ERR	B7
BYTE B64DecodeByte(BYTE code)
{
	char flags = T_CHRTYPE[code];

	if ( flags & T_IS_UPP ) return (BYTE)(code - 'A');
	if ( flags & T_IS_LOW ) return (BYTE)(code - 'a' + 26);
	if ( flags & T_IS_DIG ) return (BYTE)(code - '0' + 52);
	if ( code == '+') return 62;
	if ( code == '/') return 63;
	if ( code == '=') return B64FLAG_D;
	return B64FLAG_ERR;
}

int B64DecodeBytes(BYTE **src,BYTE *dst)
{
	BYTE work[4];
	int i;

	for ( i = 0 ; i < 4 ; i++ ){
		if ( (work[i] = B64DecodeByte(**src)) & B64FLAG_ERR ) break;
		(*src)++;
		if ( work[i] & B64FLAG_D ){
			for ( i++ ; i < 4 ; i++ ){
				if ( **src != '=' ) break;
				(*src)++;
			}
			break;
		}
	}
	if ( (work[0] | work[1]) & (B64FLAG_D | B64FLAG_ERR) ) return 0;
	*dst = (BYTE)((work[0] << 2) | (work[1] >> 4));

	if ( work[2] & (B64FLAG_D | B64FLAG_ERR) ) return 1;
	*(dst + 1) = (BYTE)((work[1] << 4) | (work[2] >> 2));

	if ( work[3] & (B64FLAG_D | B64FLAG_ERR) ) return 2;
	*(dst + 2) = (BYTE)((work[2] << 6) | work[3]);
	return 3;
}
//-----------------------------------------------------------------------------
void VO_error(ERRORCODE num)
{
	CloseViewObject();
	vo_.file.name[0] = '\0';
	if ( !convert ) PPErrorMsg(vo_.file.typeinfo,num);
}

WCHAR GetMimeChar(BYTE **src,int *offset)
{
	WCHAR c;
	const BYTE *srcptr;

	srcptr = *(const BYTE **)src + *offset;
	c = *srcptr;
	if ( c != '=' ){
		(*offset)++;
		return c;
	}
	if ( (*(srcptr + 1) == '\r') &&
		 (*(srcptr + 2) == '\n') ){ // 改行無効
		srcptr += 2;
		(*offset) += 2;
	}
	if ( IsxdigitA(*(srcptr + 1)) &&
		 IsxdigitA(*(srcptr + 2)) ){ // hexコード
		c = (WCHAR)((GetHex(*(srcptr + 1)) * 16) +
			GetHex(*(srcptr + 2)));
		(*offset) += 3;
		if ( (*(srcptr + 3) == '=') &&
			 (*(srcptr + 4) == '\r') &&
			 (*(srcptr + 5) == '\n') ){
				// 改行無効
			(*offset) += 3;
		}
	}else{
		(*offset)++;
	}
	return c;
}

DWORD MimeUtf8toWchar(BYTE **src,int code)
{
	WCHAR c,d,e,f;
	int offset = 0;

	c = (WCHAR)code;				// 1byte 0x00-7f
	(*src)++;

	d = GetMimeChar(src,&offset);
	if ( d == '\0' ) return c;
	if ( (c & 0xe0) == 0xc0 ){	// 2bytes 0x80-7ff
		(*src) += offset;
		return (DWORD)((( c & 0x1f ) <<  6) | (d & 0x3f));
	}

	e = GetMimeChar(src,&offset);
	if ( e == '\0' ) return c;
	if ( (c & 0xf0) == 0xe0 ){	// 3bytes 0x800-ffff
		(*src) += offset;
		return (DWORD)((( c & 0xf ) << 12) | ((d & 0x3f ) << 6) | (e & 0x3f));
	}

	f = GetMimeChar(src,&offset);
	if ( f == '\0' ) return c;
	if ( (c & 0xf8) == 0xf0 ){ // 4bytes 10000-1fffff
		(*src) += offset;
		return (DWORD)((( c & 7 )	<< 18) |
			((d & 0x3f )		<< 12) |
			((e & 0x3f )	<<  6) |
			(f & 0x3f ));
	}
	return c;
}

DWORD Utf8toWchar(BYTE **src,int code)
{
	WCHAR c,d,e,f/*,g,h*/;

//	c = *(*(const BYTE **)src)++;
//	if ( c < 0x80 ) return c;		// 1byte 0x00-7f
	c = (WCHAR)code;
	(*src)++;

	d = *(*(const BYTE **)src + 0);
	if ( d == '\0' ) return c;
	if ( (c & 0xe0) == 0xc0 ){	// 2bytes 0x80-7ff
		(*src)++;
		return (DWORD)((( c & 0x1f ) <<  6) | (d & 0x3f));
	}

	e = *(*(const BYTE **)src + 1);
	if ( e == '\0' ) return c;
	if ( (c & 0xf0) == 0xe0 ){	// 3bytes 0x800-ffff
		(*src) += 2;
		return (DWORD)((( c & 0xf ) << 12) | ((d & 0x3f ) << 6) | (e & 0x3f));
	}

	f = *(*(const BYTE **)src + 2);
	if ( f == '\0' ) return c;
	if ( (c & 0xf8) == 0xf0 ){ // 4bytes 10000-1fffff
		(*src) += 3;
		return (DWORD)((( c & 7 )	<< 18) |
			((d & 0x3f )		<< 12) |
			((e & 0x3f )	<<  6) |
			(f & 0x3f ));
	}
	c = '.';
/*	現在、規格で使用されていない
	g = *(*(const BYTE **)src + 3);
	if ( g == '\0' ) return g;
	if ( (c & 0xfc) == 0xf8 ){
		(*src) += 4;
		return (DWORD)((( c & 3 )	<< 24) |
			((d & 0x3f )		<< 18) |
			((e & 0x3f )	<< 12) |
			((f & 0x3f )	<<  6) |
			(g & 0x3f ));
	}

	h = *(*(const BYTE **)src + 4);
	if ( h == '\0' ) return h;
	if ( (c & 0xfe) == 0xfc ){
		(*src) += 5;
		return (DWORD)((( c & 1 )	<< 30) |
			((d & 0x3f )		<< 24) |
			((e & 0x3f )	<< 18) |
			((f & 0x3f )	<< 12) |
			((g & 0x3f )	<<  6) |
			(h & 0x3f ));
	} // c == 0xfe or 0xff
*/
	return c;
}

int UrlDecode(BYTE **text,BYTE *dest,size_t *size)
{
	BYTE urlsrc[DESTBUFSIZE],*destp,*destmax;
	BYTE *src;
	int codetype;

	src = *text;
	destp = urlsrc;
	destmax = urlsrc + sizeof(urlsrc) - 2;
	while( *src && (destp < destmax) ){
		if ( (*src == '%') && IsxdigitA(*(src+1)) && IsxdigitA(*(src+2)) ){
			*destp++ = (BYTE)((GetHex(*(src+1)) * 16) + GetHex(*(src+2)));
			src += 3;
		}else if ( IsalphaA(*src) ){
			*destp++ = *src++;
		}else{
			break;
		}
	}
	*destp = '\0';
	*(destp + 1) = '\0';
	*text = src;
	*size = destp - urlsrc;

	codetype = GetTextCodeType((const BYTE *)urlsrc,*size);
	if ( codetype >= VTYPE_MAX ){
		if ( (codetype == VTYPE_UTF8) || (codetype == CP_UTF8) ){
			codetype = VTYPE_UTF8;
		}else{
			codetype = VTYPE_SYSTEMCP;
		}
	}
	memcpy(dest,urlsrc,*size + 2);
	return codetype;
}

int DecodeRTF(TEXTCODEINFO *tci,BYTE **textptr)
{
	int c;
	BYTE *text = *textptr;

	c = *text;
	if ( c == '\\' ){
		(*textptr)++;
		SetVcode(tci,VCODE_ASCII);
		*tci->dest++ = '\\';
		tci->cnt--;
		return '\0';
	}
											// \u???? -------------------------
	if ( (c == 'u') &&
			IsxdigitA(*(text+1)) && IsxdigitA(*(text+2)) &&
			IsxdigitA(*(text+3)) && IsxdigitA(*(text+4)) ){

		if ( tci->cnt < 2 ){
			tci->cnt = 0;
			return '\0';
		}
		SetUchar(tci,(DWORD)(
				(GetHex(*(text + 1))<<12) | (GetHex(*(text + 2))<<8) |
				(GetHex(*(text + 3))<<4)  |  GetHex(*(text + 4)) ) );
		(*textptr) += 5;
		return '\0';
	}
											// \'??\'?? -----------------------
	if ( (c == '\'') &&
			IsxdigitA(*(text+1)) && IsxdigitA(*(text+2)) ){

		c = ((GetHex(*(text + 1)) << 4) + GetHex(*(text + 2)));
		SetVcode(tci,VCODE_ASCII);
		if ( IskanjiA(c) && tci->cnt < 2 ){
			tci->cnt = 0;
			(*textptr)--;
			return '\0';
		}
		*tci->dest++ = (char)c;
		tci->cnt--;
		(*textptr) += 3;

		if ( IskanjiA(c) &&
				(*text == '\\') && (*(text+1) == '\'') &&
				IsxdigitA(*(text+2)) && IsxdigitA(*(text+3)) ){
			*tci->dest++ = (char)((GetHex(*(text + 2)) << 4) + GetHex(*(text + 3)));
			tci->cnt --;
			(*textptr) += 4;
		}
	}else{
		BYTE *q,cmd[128];

		q = cmd;
		while( IsalnumA(*text) ) *q++ = *text++;
		*q = '\0';
		while ( *text == ' ' ) text++;
		*textptr = text;
		strlwr((char *)cmd);
		if ( strcmp((char *)cmd,"tab") == 0 ) return '\t';
		if ( strcmp((char *)cmd,"par") == 0 ){
			if ( tci->cnt != VOi->width ) return '\n';
		}
	}
	return '\0';
}

// utf7変換 ( +base64- )
BOOL DecodeUTF7(TEXTCODEINFO *tci,BYTE **text)
{
	BYTE *src,*dst;
	int len,left;
	BYTE tmp[DESTBUFSIZE];

	src = *text + 1;
	if ( *src == '-' ){ // +- は - に変換
		SetVcode(tci,VCODE_ASCII);
		*text += 2;
		*tci->dest++ = '+';
		tci->cnt--;
		return TRUE;
	}
	// base64 を big endian UCS-2 に変換
	dst = (BYTE *)tmp;
	left = sizeof(tmp) - 4;
	do{
		len = B64DecodeBytes(&src,dst);
		dst += len;
		left -= len;
		if ( left < 0 ) break;
	}while( len == 3 );

	// little endian UCS-2 で保存
	if (	((dst - tmp) > 0) &&
			!((dst - tmp) & 1) &&
			((*src == '-') || (*src == '\r')) ){
		BYTE *wptr;

		for ( wptr = tmp ; (wptr + 1) < dst ; wptr += 2 ){
			WORD uchar;

			uchar = (WORD)((WORD)*wptr << 8) | ((WORD)*(wptr + 1));
			if ( uchar == '\0' ) uchar = '.';
			SetUchar(tci,uchar);
		}
		*text = src + ((*src == '-') ? 1 : 0);
		return TRUE;
	}
	return FALSE;
}

BOOL DecodeEUCJP(TEXTCODEINFO *tci,BYTE **textptr)
{
	int c = **textptr,d;
	int c2;

	c2 = *(*textptr + 1);
	if ( c2 < 0x80 ){
		(*textptr)++;
		*tci->dest++ = (BYTE)c;
		tci->cnt--;
		return TRUE;
	}
	if ( c == 0x8e ){		//SS2,1bytes KANA
		(*textptr) += 2;
		*tci->dest++ = (BYTE)c2;
		tci->cnt--;
		return TRUE;
	}
	if ( tci->cnt < 2 ){
		tci->cnt = 0;
		return FALSE;
	}
	(*textptr) += 2;

	// EUC-JP → Shift_JIS
	d = c - 0x80;
	c = c2 - 0x80;

	if ( d & 1 ){
		if ( c < 0x60 ){
			c += 0x1F;
		}else{
			c += 0x20;
		}
	}else{
		c += 0x7E;
	}
	if ( d < 0x5F ){
		d = (d + 0xE1) >> 1;
	}else{
		d = (d + 0x161) >> 1;
	}

	if ( ((d == 0x85) || (d == 0x86)) && (c >= 0x40) ){
		WORD a;
		BYTE sjl;

		a = NECCHAR[d - 0x85][c - 0x40];
		d = SJIS1st(a);
		sjl = SJIS2nd(a);
		if ( sjl == '\0' ){
			*tci->dest++ = (unsigned char)d;
			tci->cnt--;
			return TRUE;
		}else{
			c = sjl;
		}
	}
	if ( (d == 0x81) && (c == 0x40) && XV_bctl[2] ){
		CloseVcode(tci);
		tci->vcode = VCODE_END;
		*tci->dest++ = VCODE_SPACE;
		tci->cnt -= 2;
		return TRUE;
	}
	*tci->dest++ = (unsigned char)d;
	*tci->dest++ = (unsigned char)c;
	tci->cnt -= 2;
	return TRUE;
}

BOOL DecodeJIS(TEXTCODEINFO *tci,BYTE **textptr,BYTE *srcmax)
{
	int c = **textptr,d;

	if ( (c == 0x8b) || (c == 0x8c) ){	// 上/下付き添え字
		(*textptr)++;
		return TRUE;
	}
	if ( c == 0x9b ){	//文字大きさ
		(*textptr)++;
		while( *textptr < srcmax ){
			c = (unsigned char)**textptr;
			if ( c < ' '  ) break;
			if ( c >= 0x80 ) break;
			(*textptr)++;
			if ( IsalphaA(c) ) break;
		}
		return TRUE;
	}
	if ( tci->cnt < 2 ){
		tci->cnt = 0;
		return FALSE;
	}
	(*textptr)++;
	d = c;
	c = **textptr;
	if ( (d < 0x21) || (d >= 0x7f) || (c < 0x21) || (c >= 0x7f) ){
		*tci->dest++ = (unsigned char)d;
		*tci->dest++ = (unsigned char)' ';
		tci->cnt -= 2;
		return TRUE;
	}
	(*textptr)++;

	if ( VO_Tquoted ){
		if ( (c == '=') && (**textptr == '\r') && (*(*textptr + 1)=='\n') ){
			c = *(*textptr + 2);
			(*textptr) += 3;
		}
		if ( (c == '=') && IsxdigitA(**textptr) && IsxdigitA(*(*textptr+1)) ){
			c = (GetHex(**textptr) * 16) + GetHex(*(*textptr+1));
			(*textptr) += 2;
		}
	}

	if ( d & 1 ){
		if ( c < 0x60 ){
			c += 0x1F;
		}else{
			c += 0x20;
		}
	}else{
		c += 0x7E;
	}
	if ( d < 0x5F ){
		d = (d + 0xE1) >> 1;
	}else{
		d = (d + 0x161) >> 1;
	}

	if ( ((d == 0x85) || (d == 0x86)) && (c >= 0x40) ){
		WORD a;
		BYTE sjl;

		a = NECCHAR[d - 0x85][c - 0x40];
		d = SJIS1st(a);
		sjl = SJIS2nd(a);
		if ( sjl == '\0' ){
			*tci->dest++ = (unsigned char)d;
			tci->cnt--;
			return TRUE;
		}else{
			c = sjl;
		}
	}
	if ( (d == 0x81) && (c == 0x40) && XV_bctl[2] ){
		CloseVcode(tci);
		tci->vcode = VCODE_END;
		*tci->dest++ = VCODE_SPACE;
		tci->cnt -= 2;
		return TRUE;
	}
	*tci->dest++ = (unsigned char)d;
	*tci->dest++ = (unsigned char)c;
	tci->cnt -= 2;
	return TRUE;
}

BOOL DecodeHtmlSpecialChar(TEXTCODEINFO *tci,BYTE **textptr)
{
	#define SpecialCharMaxSize 200
	BYTE *ptr;
	int len = SpecialCharMaxSize;

	ptr = *textptr;
	while ( len ){
		if ( *(++ptr) == ';' ) break;
		if ( (*ptr <= 0x20) || (*ptr >= 0x80) ){
			len = 0;
			break;
		}
		len--;
	}
	if ( len ){
		if ( *(*textptr + 1) == '#' ){ // 数値文字参照
			BYTE *q;
			int co;

			q = *textptr + 2;
			if ( *q != 'x' ){ // 10進
				co = GetIntNumberA((const char **)&q);
			}else{ // 16進
				q++;
				co = GetHexNumberA((const char **)&q);
			}
			if ( co && ( q == ptr ) ){
				SetUchar(tci,co);
				*textptr = ptr + 1;
				return TRUE;
			}
		}else{
			BYTE *chrptr;
			ACHARS *ac;

			chrptr = *textptr + 1;
			len = (SpecialCharMaxSize + 1) - len;
/*
			if ( OSver.dwMajorVersion >= 5 ){ // 文字実体参照(UNICODE
				UCHARS *uc;

				for ( uc = unichars ; uc->uchar ; uc++ ){
					if ( memcmp(chrptr,uc->name,len) ) continue;
					SetUchar(tci,uc->uchar);
					*textptr = p + 1;
					return TRUE;
				}
			}
*/
			for ( ac = anschars ; ac->achar ; ac++ ){
				if ( memcmp(chrptr,ac->name,len) ) continue;
				*tci->dest++ = (BYTE)ac->achar;
				tci->cnt--;
				if ( ac->achar > 0x100 ){
					*tci->dest++ = (BYTE)(ac->achar >> 8);
					tci->cnt--;
				}
				*textptr = ptr + 1;
				return TRUE;
			}
		}
	}
	return FALSE;
}

#ifdef UNICODE
BYTE SkipSpaceA(const BYTE **str)
{
	BYTE code;

	for ( ;; ){
		code = **str;
		if ( (code != ' ') && (code != '\t') ) break;
		(*str)++;
	}
	return code;
}

BYTE GetLineParamA(const char **str,TCHAR *param)
{
	BYTE code,bottom;
	const BYTE *src;
	BYTE *dst,*maxptr;
	BYTE paramA[VFPS];

	src = (BYTE *)*str;
	dst = (BYTE *)paramA;
	maxptr = dst + VFPS - 1;
	bottom = code = SkipSpaceA(&src);
	if ( code == '\0' ){
		*str = (char *)src;
		*dst = '\0';
		return 0;
	}
	if ( code == '\"' ){
		src++;
		while( ' ' < (code = *src) ){
			src++;
			if ( (code == '\"') && (*src != '\"') )	break;
			*dst++ = code;
		}
	}else{
		do {
			src++;
			if ( code <= ' ' )	break;
			if ( code == '\"' ){
				*dst++ = code;
				while( ' ' < (code = *src) ){
					src++;
					*dst++ = code;
					if ( (code == '\"') && (*src != '\"') ) break;
					if ( dst >= maxptr ) break;
				}
			}else{
				*dst++ = code;
			}
			if ( dst >= maxptr ) break;
		}while( ' ' < (code = *src) );
	}
	*dst = '\0';
	*str = (char *)src;
	AnsiToUnicode((char *)paramA,param,VFPS);
	return bottom;
}
#define TGetLineParam GetLineParamA
#else
#define TGetLineParam GetLineParam
#endif

BOOL USEFASTCALL CheckTopSpace(TEXTCODEINFO *tci)
{
	BYTE *dest;
	int len;

	if ( VOi->width <= 0 ) return TRUE;

	len = VOi->width - tci->cnt;
	dest = tci->dest - len;
	while ( len ){
		if ( *dest != ' ' ) return TRUE;
		dest++;
		len--;
	}
	return FALSE;
}

BYTE * USEFASTCALL ShrinkTagBlankLine(BYTE *tagend,int skipmode)
{
	BYTE *text = tagend + 1,c,*lasttext;

	if ( skipmode == 0 ){
		c = *text;

		if ( c == '\r' ){
			text += (*(text + 1) == '\n') ? 2 : 1;
		}else if ( c == '\n' ){
			text++;
		}
	}

	lasttext = text;
	for (;;){
		c = *text;
		switch ( c ){
			case '\n':
				lasttext = text;
			case ' ':
				text++;
				continue;

			case '\r':
				lasttext = text;
				text += (*(text + 1) == '\n') ? 2 : 1;
				continue;

			default:
				break;
		}
		break;
	}
	return (skipmode == 2) ? text : lasttext;
}

BOOL DecodeHtmlTag(TEXTCODEINFO *tci,BYTE **textptr,BYTE *srcmax,int *attrs)
{
	BYTE *tagend;
	int len = X_tlen;

	tagend = *textptr;
										// 本当にTAGかを確認
	for ( ; len ; len-- ){
		if ( ++tagend >= srcmax ){
			len = 0;
			break;
		}
		if ( *tagend == '>' ) break;
		if ( (*tagend == '\r') || (*tagend == '\n') ) continue;
		if ( *tagend < 0x20 ){
			len = 0;
			break;
		}
	}

	if ( len ) if ( VO_Ttag > 1 ){ // 色づけのみ
		CloseVcode(tci);
		tci->vcode = VCODE_END;
		*tci->dest++ = VCODE_FCOLOR;
		*(COLORREF *)tci->dest = CV_syn[
			((*(*textptr + 1) == '!') &&
			 (*(*textptr + 2) == '-') &&
			 (*(*textptr + 3) == '-')) ?
			1 : 0
		];
		tci->dest += sizeof(COLORREF);
		SetVcode(tci,VCODE_ASCII);
		setflag(*attrs, VTTF_TAG);
	}else{
		char *s,*dst,tagname[32];
		int offsw = 0,l;

		VO_CodePageChanged = TRUE;
		s = (char *)*textptr + 1;
		SkipSepA(&s);
		if ( *s == '/' ){
			offsw = 1;
			s++;
		}
		SkipSepA(&s);
		dst = tagname;
		for ( l = sizeof(tagname) - 1 ; l ; l-- ){
			if ( !IsalnumA(*s) ) break;
			*dst++ = *s++;
		}
		*dst = '\0';
		strlwr(tagname);

		if ( strcmp(tagname,"a") == 0 ){
			if ( offsw ){
				CloseVcode(tci);
				tci->vcode = VCODE_END;
				*tci->dest++ = VCODE_COLOR;
				*tci->dest++ = (BYTE)tci->Fclr;
				*tci->dest++ = (BYTE)tci->Bclr;
				resetflag(*attrs, VTTF_LINK);
			}else{
				SkipSepA(&s);
				if ( upper(*s) == 'H' ){
					CloseVcode(tci);
					tci->vcode = VCODE_END;
					*tci->dest++ = VCODE_LINK;
					setflag(*attrs, VTTF_LINK);
				}
			}
			*textptr = ShrinkTagBlankLine(tagend,0);
			return TRUE;
		}
		if ( !offsw ){
			if ( !VO_Tshow_script && (strcmp(tagname,"script") == 0) ){
				char *last = strstr((char *)*textptr,
					*(*textptr + 1) != 'S' ? "</script>" : "</SCRIPT>");
				if ( last != NULL ){
					*textptr = (BYTE *)last + 9;
					return TRUE;
				}
			}
			if ( !VO_Tshow_css && (strcmp(tagname,"style") == 0) ){
				char *last = strstr((char *)*textptr,
					*(*textptr + 1) != 'S' ? "</style>" : "</STYLE>");
				if ( last != NULL ){
					*textptr = (BYTE *)last + 8;
					return TRUE;
				}
			}
		}

		// <base なら vo_.file.source を取得する
		if ( !strcmp(tagname,"base") ){
			if ( !offsw ){
				SkipSepA(&s);
				if ( upper(*s) == 'H' ){
					const char *up;

					up = strchr(s,'=');
					if ( (up != NULL) && (up < (char *)tagend) ){
						up++;
						TGetLineParam(&up,vo_.file.source);
					}
				}
				offsw = 1;
			}
		}
		// </tag> を隠す
		if ( offsw ){
			if ( !memcmp(*textptr + 2,"w:p>",4) ){ // Word docx の改段
				CloseVcode(tci);
				tci->vcode = VCODE_END;
				*tci->dest++ = VCODE_RETURN;
				*tci->dest++ = CTRLSIG_CRLF;
				*textptr = tagend + 1;
				tci->cnt = 0;
				return TRUE;
			}
			*textptr = ShrinkTagBlankLine(tagend,2);
			return TRUE;
		}
		// table の各要素に区切り線を入れる
		if ( !strcmp(tagname,"th") || !strcmp(tagname,"td") ){
			if ( (tci->cnt != VOi->width) && !offsw ){
				*tci->dest++ = '|';
				*tci->dest++ = ' ';
				tci->cnt -= 2;
			}
			*textptr = ShrinkTagBlankLine(tagend,0);
			return TRUE;
		}
		// 特定の tag を改行扱いにする
		if (	!strcmp(tagname,"br")	||
				!strcmp(tagname,"p")	||
				!memcmp(*textptr + 1,"w:cr/>",6) || // Word docx の改行
				( (tci->cnt != VOi->width) &&
				  (	!strcmp(tagname,"ol")	||
					!strcmp(tagname,"li")	||
					!strcmp(tagname,"div")	||
					!strcmp(tagname,"dd")	||
					!strcmp(tagname,"dt")	||
					!strcmp(tagname,"hr")	||
					!strcmp(tagname,"blockquote")	||
					((*tagname == 'h') && IsdigitA(tagname[1]))	||
					!strcmp(tagname,"tr")
				  ) && CheckTopSpace(tci)
				) ){
			CloseVcode(tci);
			tci->vcode = VCODE_END;
			*tci->dest++ = VCODE_RETURN;
			*tci->dest++ = CTRLSIG_CRLF;
			*textptr = ShrinkTagBlankLine(tagend,0);
			setflag(*attrs, VTTF_TOP);
			tci->cnt = 0;
			return TRUE;
		}
		// １つのtagのみ有る行を無視する
		if ( (tci->cnt == VOi->width) && ((*(tagend + 1) == 0xa) || (*(tagend + 1) == 0xd)) ){
			*textptr = ShrinkTagBlankLine(tagend,0);
			return TRUE;
		}
		*textptr = ShrinkTagBlankLine(tagend,1);
		return TRUE;
	}
	return FALSE;
}

int DecodeQuotedPrintable(BYTE **textptr)
{
	int c = *(*textptr + 1);

	if ( c == '\n' ){ // 改行無効
		*textptr += 2;
		return '\0';
	}
	if ( c == '\r' ){ // 改行無効
		*textptr += (*(*textptr + 2) == '\n') ? 3 : 2;
		return '\0';
	}

	if ( IsxdigitA(c) && IsxdigitA(*(*textptr + 2)) ){ // hexコード
		c = (GetHex((BYTE)c) * 16) + GetHex(*(*textptr + 2));
		*textptr += 2;

		if ( *(*textptr + 1) == '=' ){
			if ( (*(*textptr + 2) == '\r') && (*(*textptr + 3) == '\n') ){ // 改行無効
				*textptr += 3;
			}else if ( *(*textptr + 2) == '\n' ){
				*textptr += 2;
			}
		}
		return c;
	}else{
		return '='; // 変更無し
	}
}

// テキスト中の文字指定による文字コード切り替え
void CheckCharset(BYTE *text,int *dcode)
{
	DWORD cp;

	text += 8;
	if ( *text == '\"' ) text++;
	cp = GetCodePage(&text);

	if ( (cp > 0) && (cp < VTYPE_MAX) ){
		if ( cp == VTYPE_JIS ) cp = VTYPE_SYSTEMCP;
		*dcode = cp;
	}else if ( cp == CP_UTF8 ){
		*dcode = VTYPE_UTF8;
	}else if ( cp == CP__SJIS ){
		if ( VO_textS[VTYPE_SYSTEMCP] == textcp_sjis ){
			*dcode = VTYPE_SYSTEMCP;
		}else{
			*dcode = VTYPE_OTHER;
			VO_CodePage = CP__SJIS;
		}
	}else if ( (cp == CP__JIS) || (cp == 50225/*iso-2022-kr*/ ) ){
		*dcode = VTYPE_SYSTEMCP; // 本来は VTYPE_JIS だが、コード切り替え前なので
	}else if ( cp == CP__EUCJP ){
		*dcode = VTYPE_EUCJP;
	}else if ( (cp != 0) && (VO_CodePageChanged == FALSE) ){
		*dcode = VTYPE_OTHER;
		VO_CodePage = cp;
	}
}

void DecodeEuro(TEXTCODEINFO *tci,int dcode)
{
	if ( OSver.dwMajorVersion >= 5 ){
		CloseVcode(tci);
		tci->vcode = VCODE_END;

		*tci->dest++ = VCODE_FONT;
		*tci->dest++ = 1;
		*tci->dest++ = VCODE_ASCII;
		*tci->dest++ = 0x80;
		*tci->dest++ = '\0';
		*tci->dest++ = VCODE_FONT;
		*tci->dest++ = (BYTE)(dcode <= VTYPE_ANSI ? dcode : 2);
		tci->cnt -= 1;
	}else{
		SetVcode(tci,VCODE_ASCII);
		*tci->dest++ = '[';
		*tci->dest++ = 'E';
		*tci->dest++ = ']';
		tci->cnt -= 3;
	}
}

BOOL DecodeControlCode(TEXTCODEINFO *tci,BYTE **textptr,int c1st,int *attrs,int *dcode)
{
	int c = c1st;
	// JIS
	if ( (*dcode == VTYPE_JIS ) && (c == 0) &&
			!*(*textptr + 1) && !*(*textptr + 2) && !*(*textptr + 3) ){
		*textptr += 0x81;
		return TRUE;
	}
	(*textptr)++;
									// TAB ----------------------------
	if ( c == '\t' ){
		CloseVcode(tci);
		tci->vcode = VCODE_END;
		for ( ;; ){
			int t;

			t = VOi->tab - ((VOi->width - tci->cnt) % VOi->tab);
			if ( tci->cnt < t ) t = tci->cnt;
			tci->cnt -= t;
			*tci->dest++ = VCODE_TAB;
			if ( tci->cnt == 0 ) break;
			if ( *dcode == VTYPE_UNICODE){
				if ( *(WCHAR *)*textptr != '\t' ) break;
				*textptr += sizeof(WCHAR);
			}else if ( *dcode == VTYPE_UNICODEB ){
				if ( *(WCHAR *)*textptr != ('\t' * 0x100) ) break;
				*textptr += sizeof(WCHAR);
			}else{
				if ( **textptr != '\t' ) break;
				(*textptr)++;
			}
		}
		if ( XV_unff ) tci->PxWidth = tci->cnt * fontX;
		return TRUE;
	}
											// JIS/KANA 切り換え --------------
	if ( (VO_Tmode != 1) && ((*dcode == VTYPE_JIS ) || VO_Tesc) ){
		if ( (c == 0xe) && (**textptr > 6) && (**textptr <= 0x5f) ){ // SO
			*dcode = VTYPE_KANA;	// JISX0201
			return TRUE;
		}
		if ( (c == 0xf) && (*dcode == VTYPE_KANA) ){	// SI
			*dcode = VOi->textC;
			return TRUE;
		}
	}
											// LF -----------------------------
	if ( c == '\n' ){
		CloseVcode(tci);
		tci->vcode = VCODE_END;
		tci->dest[0] = VCODE_RETURN;
		tci->dest[1] = CTRLSIG_LF;
		tci->dest += 2;
		if ( *dcode != VTYPE_UNICODE ){
			if (	(*dcode == VTYPE_UNICODEB) &&
					(**textptr == 0) && (*(*textptr+1) == '\a') ){
				*textptr += 2;
			}
			if ( **textptr == '\a' ) (*textptr)++;
		}else{
			if ( (**textptr == '\a') && (*(*textptr+1) == 0) ) *textptr += 2;
		}
		setflag(*attrs, VTTF_TOP);
		return FALSE;
	}
											// CR -----------------------------
	if ( c == '\r' ){
		CloseVcode(tci);
		tci->vcode = VCODE_END;
		tci->dest[0] = VCODE_RETURN;
		tci->dest[1] = CTRLSIG_CRLF;
		tci->dest += 2;
		if ( *dcode != VTYPE_UNICODE ){
			if (	(*dcode == VTYPE_UNICODEB) &&
					(**textptr == 0) && (*(*textptr + 1) == '\n') ){
				*textptr += 2;
			}
			if ( **textptr == '\n' ){
				(*textptr)++;
			}else{
				*(tci->dest - 1) = CTRLSIG_CR;
			}
		}else{
			if ( (**textptr == '\n') && (*(*textptr + 1) == 0) ){
				*textptr += 2;
			}else{
				*(tci->dest - 1) = CTRLSIG_CR;
			}
		}
		setflag(*attrs, VTTF_TOP);
		return FALSE;
	}
											// ^K 改段 ------------------------
	if ( c == '\xb' ){
		CloseVcode(tci);
		tci->vcode = VCODE_END;
		*tci->dest++ = VCODE_PARA;
		setflag(*attrs, VTTF_TOP);
		return FALSE;
	}
											// ^L 改ページ --------------------
	if ( c == '\xc' ){
		CloseVcode(tci);
		tci->vcode = VCODE_END;
		*tci->dest++ = VCODE_PAGE;
		setflag(*attrs, VTTF_TOP);
		return FALSE;
	}
											// ESC ----------------------------
	if ( (c == 0x1b) && VO_Tesc && (*dcode != VTYPE_UNICODE) ){
		switch(**textptr){
			case '(':
				switch( *(*textptr+1) ){
					case 'A':		// ^[(A ISO646 ENG
					case 'B':		// ^[(B ASCII
//					case 'D':		// ^[(D JIS X 0212-1990
					case 'H':		// ^[(H ISO646 SW
					case 'J':		// ^[(J JIS C 6220-1976 JISx0201 ROMA
						*textptr += 2;
						*dcode = VOi->textC;
						return TRUE;
					case 'I':		// ^[(I JISx0201 KANA
						*textptr += 2;
						*dcode = VTYPE_KANA;
						return TRUE;
//					case 'O':		// ^[(O JIS X 0213:2000 1
//					case 'P':		// ^[(P JIS X 0213:2000 2
//					case 'Q':		// ^[(Q JIS X 0213:2004 1
				}
				break;
			case '$':
				switch( *(*textptr+1) ){
					case '@':		// ^[$@ JIS C 6226-1978 JISx0208 KNJ1978
					case 'B':		// ^[$B JIS C JISx0208-1983 KNJ1983
//					case 'C':		// ^[$C Hangul
//					case 'D':		// ^[$D JISx0212 ExKNJ1990
//									// ^[$)A Chi GB 2312-80
//									// ^[$)B Chi CNS 11643-1992 1
//									// ^[$)E Chi ISO-IR-165
//									// ^[$(C KS X 1001-1992
//									// ^[$)C KS X 1001-1992
//									// ^[$*H Chi CNS 11643-1992 2
						*textptr += 2;
						*dcode = VTYPE_JIS;
						return TRUE;
				}
				break;
//									// ^[.A ISO 8859-1
//									// ^[.F ISO 8859-7
			case '[': // 画面関係 ESC シーケンス ------------
				if ( IsTrue(EscColor(textptr,tci,*dcode)) ){
					return TRUE;
				}
				break;
		}
	}
								// ^? に変換 --------------------------
	if ( (c == 0) || (*dcode != VTYPE_IBM) ){
							// IBM 以外は 20h 未満を変換
		if ( (c == 0) && (VO_Tmode == 0) ) VO_Tmode = 1;
		if ( tci->cnt < 2 ){
			(*textptr)--;
			if ((*dcode == VTYPE_UNICODE) || (*dcode == VTYPE_UNICODEB)) (*textptr)--;
			tci->cnt = 0;
			return FALSE;
		}
/*
		if ( (tci->vcode != VCODE_ASCII) && (tci->vcode != VCODE_UNICODE)){
			SetVcode(&tci,VCODE_ASCII);
		}
		*tci->dest++ = '^';
		if (tci->vcode == VCODE_UNICODE) *tci->dest++ = '\0';
		*tci->dest++ = (unsigned char)(c + '@');
		if (tci->vcode == VCODE_UNICODE) *tci->dest++ = '\0';
*/
		SetVcode(tci,VCODE_CONTROL);
		*tci->dest++ = '^';
		*tci->dest++ = (unsigned char)(c + '@');
		tci->cnt -= 2;
		return TRUE;
	}else{
		if ((*dcode == VTYPE_UNICODE) || (*dcode == VTYPE_UNICODEB)) (*textptr)--;
		SetVcode(tci,VCODE_ASCII);
		*tci->dest++ = (BYTE)c;
		tci->cnt--;
		return TRUE;
	}
}

//-----------------------------------------------------------------------------
char *stristrA(const char *target,const char *findstr)
{
	size_t len,flen;
	const char *p,*max;

	flen = strlen(findstr);
	len = strlen(target);
	max = target + len - flen;

	for ( p = target ; p <= max ; p += ChrlenA(*p) ){
		if ( !strnicmp(p,findstr,flen) ){
			return (char *)p;
		}
	}
	return NULL;
}

WCHAR *stristrW(const WCHAR *target,const WCHAR *findstr)
{
	size_t len,flen;
	const WCHAR *p,*max;

	flen = strlenW(findstr);
	len = strlenW(target);
	max = target + len - flen;

	for ( p = target ; p <= max ; p++ ){
		if ( !strnicmpW(p,findstr,flen) ){
			return (WCHAR *)p;
		}
	}
	return NULL;
}

#define SEARCHHILIGHT_COLORID (CV__deftext + (CV__highlight << 8))
#define SEARCHHILIGHT_COLORREF (SEARCHHILIGHT_COLORID | 0xff000000)
#define COLORREF_ABNORMAL 0x1000000

#define USERHILIGHTA_ADDSIZE 11
void WriteUserHilightA(BYTE *dest, size_t size, COLORREF color,int extend)
{
	// text xxx1 add1[7] firstp[size] add2[4] last
	memmove(dest + VCODE_FCOLOR_SIZE + 2,dest,size); // firstp
	*(dest + 0) = '\0'; // xxx1 term.
	*(dest + 1) = (extend & HILIGHTKEYWORD_R) ? (BYTE)VCODE_BCOLOR : (BYTE)VCODE_FCOLOR; // +1 header
	*(COLORREF *)(dest + 2) = color; // +2～+5 color
	*(dest + 6) = VCODE_ASCII; // +6 header

	dest += size;
	*(dest + 7) = '\0'; // string2 +0 term
	*(dest + 8) = VCODE_COLOR; // string2 +1 header
	*(dest + 9) = CV__deftext; // string2 +2 color1
	*(dest +10) = CV__defback; // string2 +3 color2
}

#define SEARCHHILIGHTA_ADDSIZE 9
void WriteSearchHilightA(BYTE *dest, size_t size)
{
	// text xxx1 add1[5] firstp[size] add2[4] last
	memmove(dest + VCODE_COLOR_SIZE + 2,dest,size);
	*(dest + 0) = '\0';
	*(dest + 1) = VCODE_COLOR;
	*(WORD *)(dest + 2) = SEARCHHILIGHT_COLORID;
	*(dest + 4) = VCODE_ASCII;

	dest += size;
	*(dest + 5) = '\0';
	*(dest + 6) = VCODE_COLOR;
	*(dest + 7) = CV__deftext;
	*(dest + 8) = CV__defback;
}

void CheckSearchAscii(TEXTCODEINFO *tci,const char *search,COLORREF color,int extend)
{
	BYTE *firstp,*text = tci->textfirst,*last = tci->dest;
	size_t size,addsize = 0;

	for ( ;; ){
		firstp = (BYTE *)stristrA((char *)text,search);
		if ( firstp == NULL ){
			BYTE *tmpp,type;

			tmpp = text + strlen((char *)text) + 1;
			if ( tmpp >= last ) break;
			type = *tmpp;
			if ( type == VCODE_FCOLOR ){
				text = tmpp + VCODE_FCOLOR_SIZE + sizeof(char);
				continue;
			}else if ( type == VCODE_COLOR ){
				text = tmpp + VCODE_COLOR_SIZE + sizeof(char);
				if ( text >= last ) break;
				if ( *(WORD *)(tmpp + 1) == SEARCHHILIGHT_COLORID ){
					text += strlen((char *)text);
				}
				continue;
			}
			break; // proof
		}
		size = strlen(search);

		if ( (firstp + size + 1) == last ){ // 末尾の特別処理
			if ( color >= COLORREF_ABNORMAL ){ // SEARCHHILIGHT_COLORREF
				// text xxx1 add1[5] firstp[size] add2[4-1] last
				if ( (last + SEARCHHILIGHTA_ADDSIZE) >= tci->destmax ) break;
				WriteSearchHilightA(firstp,size);
				addsize += SEARCHHILIGHTA_ADDSIZE - sizeof(char);
			}else{
				// text xxx1 add1[7] firstp[size] add2[4-1] last
				if ( (last + USERHILIGHTA_ADDSIZE) >= tci->destmax ) break;
				if ( extend & HILIGHTKEYWORD_T ){
					size += firstp - text;
					firstp = text;
				}
				WriteUserHilightA(firstp, size, color, extend);
				addsize += USERHILIGHTA_ADDSIZE - sizeof(char);
			}
			break;
		}
		if ( color >= COLORREF_ABNORMAL ){ // SEARCHHILIGHT_COLORREF
			// text xxx1 add1[5] firstp[size] add2[4] next[1] xxx2 last
			if ( (last + SEARCHHILIGHTA_ADDSIZE + 1) >= tci->destmax ) break;
			memmove(firstp + size + SEARCHHILIGHTA_ADDSIZE + 1,firstp + size,last - (firstp + size)); // xxx2
			WriteSearchHilightA(firstp,size);
			firstp += size + SEARCHHILIGHTA_ADDSIZE;
			*firstp++ = VCODE_ASCII;
			last += SEARCHHILIGHTA_ADDSIZE + 1;
			addsize += SEARCHHILIGHTA_ADDSIZE + 1;
		}else{
			// text xxxxx add1[7] firstp[size] add2[4] next[1] xxxx last
			if ( (last + USERHILIGHTA_ADDSIZE + 1) >= tci->destmax ) break;
			if ( extend & HILIGHTKEYWORD_T ){
				size += firstp - text;
				firstp = text;
			}
			if ( extend & HILIGHTKEYWORD_B ){
				size += strlen((char *)firstp + size);
			}
			memmove(firstp + size + USERHILIGHTA_ADDSIZE + 1, firstp + size,last - (firstp + size));
			WriteUserHilightA(firstp, size, color, extend);
			firstp += size + USERHILIGHTA_ADDSIZE;
			*firstp++ = VCODE_ASCII;
			last += USERHILIGHTA_ADDSIZE + 1;
			addsize += USERHILIGHTA_ADDSIZE + 1;
		}
		text = firstp;
	}
	tci->dest += addsize;
}

#define USERHILIGHTW_ADDSIZE 13
void WriteUserHilightW(BYTE *dest,size_t size,COLORREF color,int extend)
{
	// text xxx1 add1[8] firstp[size] add2[5] last
	memmove(dest + VCODE_FCOLOR_SIZE + 3,dest,size);
	*(dest + 0) = '\0';
	*(dest + 1) = '\0';
	*(dest + 2) = (extend & HILIGHTKEYWORD_R) ? (BYTE)VCODE_BCOLOR : (BYTE)VCODE_FCOLOR; // +1 header
	*(COLORREF *)(dest + 3) = color;
	*(dest + 7) = VCODE_UNICODE;

	dest += size;
	*(dest + 8) = '\0';
	*(dest + 9) = '\0';
	*(dest +10) = VCODE_COLOR;
	*(dest +11) = CV__deftext;
	*(dest +12) = CV__defback;
}

#define SEARCHHILIGHTW_ADDSIZE 11
void WriteSearchHilightW(BYTE *dest,size_t size)
{
	// text xxx1 add1[6] firstp[size] add2[5] last
	memmove(dest + VCODE_COLOR_SIZE + 3,dest,size);
	*(dest + 0) = '\0';
	*(dest + 1) = '\0';
	*(dest + 2) = VCODE_COLOR;
	*(WORD *)(dest + 3) = SEARCHHILIGHT_COLORID;
	*(dest + 5) = VCODE_UNICODE;

	dest += size;
	*(dest + 6) = '\0';
	*(dest + 7) = '\0';
	*(dest + 8) = VCODE_COLOR;
	*(dest + 9) = CV__deftext;
	*(dest +10) = CV__defback;
}

void CheckSearchUNICODE(TEXTCODEINFO *tci, const WCHAR *search, COLORREF color, int extend)
{
	BYTE *firstp, *text = tci->textfirst, *last = tci->dest;
	size_t size, addsize = 0;

	for ( ;; ){
		firstp = (BYTE *)stristrW((WCHAR *)text, search);
		if ( firstp == NULL ){
			BYTE *tmpp, type;

			tmpp = text + (strlenW((WCHAR *)text) + 1) * sizeof(WCHAR);
			if ( tmpp >= last ) break;
			type = *tmpp;
			if ( type == VCODE_FCOLOR ){
				text = tmpp + VCODE_FCOLOR_SIZE + sizeof(WCHAR);
				continue;
			}else if ( type == VCODE_COLOR ){
				text = tmpp + VCODE_COLOR_SIZE + sizeof(WCHAR);
				if ( text >= last ) break;
				if ( *(WORD *)(tmpp + 1) == SEARCHHILIGHT_COLORID ){
					text += strlenW((WCHAR *)text) * sizeof(WCHAR);
				}
				continue;
			}
			break; // proof
		}
		size = strlenW(search) * sizeof(WCHAR);

		if ( (firstp + size + 2) == last ){
			if ( color >= COLORREF_ABNORMAL ){ // SEARCHHILIGHT_COLORREF
				// text xxx1 add1[6] firstp[size] add2[5-2] last
				if ( (last + SEARCHHILIGHTW_ADDSIZE) >= tci->destmax ) break;
				WriteSearchHilightW(firstp,size);
				addsize += SEARCHHILIGHTW_ADDSIZE - sizeof(WCHAR);
			}else{
				// text xxxxx [8] firstp---size [5-2] last
				if ( (last + USERHILIGHTW_ADDSIZE) >= tci->destmax ) break;
				if ( extend & HILIGHTKEYWORD_T ){
					size += firstp - text;
					firstp = text;
				}
				WriteUserHilightW(firstp, size, color, extend);
				addsize += USERHILIGHTW_ADDSIZE - sizeof(WCHAR);
			}
			break;
		}
		if ( color >= COLORREF_ABNORMAL ){ // SEARCHHILIGHT_COLORREF
			// text xxx1 add1[6] firstp[size] add2[5] next[1] xxx2 last
			if ( (last + 12) >= tci->destmax ) break;
			memmove(firstp + size + SEARCHHILIGHTW_ADDSIZE + 1, firstp + size,last - (firstp + size));
			WriteSearchHilightW(firstp,size);
			firstp += size + SEARCHHILIGHTW_ADDSIZE;
			*firstp++ = VCODE_UNICODE;
			last += SEARCHHILIGHTW_ADDSIZE + 1;
			addsize += SEARCHHILIGHTW_ADDSIZE + 1;
		}else{
			// text xxxxx [8] firstp---size add2[5] next[1] xxx2 last
			if ( (last + USERHILIGHTW_ADDSIZE + 1) >= tci->destmax ) break;
			if ( extend & HILIGHTKEYWORD_T ){
				size += firstp - text;
				firstp = text;
			}
			if ( extend & HILIGHTKEYWORD_B ){
				size += strlenW((WCHAR *)(firstp + size)) * sizeof(WCHAR);
			}
			memmove(firstp + size + USERHILIGHTW_ADDSIZE + 1, firstp + size,last - (firstp + size));
			WriteUserHilightW(firstp, size, color, extend);
			firstp += size + USERHILIGHTW_ADDSIZE;
			*firstp++ = VCODE_UNICODE;
			last += USERHILIGHTW_ADDSIZE + 1;
			addsize += USERHILIGHTW_ADDSIZE + 1;
		}
		text = firstp;
	}
	tci->dest += addsize;
}

int GetHexNumberA(const char **ptr)
{
	BYTE Ctype;
	BYTE c;
	int n = 0;

	for ( ;; ){
		c = **ptr;
		Ctype = T_CHRTYPE[c];
		if ( !(Ctype & (T_IS_DIG | T_IS_HEX)) ) break;	// 0-9,A-F,a-f ではない
		if ( Ctype & T_IS_LOW ) c = (BYTE)(c - 0x20);	// 小文字を大文字に
		if ( !(Ctype & T_IS_DIG) ) c = (BYTE)(c - 7);	// A-F の処理
		c = (BYTE)(c - '0');
		n = (n << 4) + c;
		(*ptr)++;
	}
	return n;
}

int MakeIndexTable(int mode,int param)
{
	VMEM *vo;
	BYTE *of,*vmax;			// 参照中のイメージと最大値
	BYTE buf[TEXTBUFSIZE];
	int next;				// 次のメモリ確保行
	int line;				// 処理行数
	int count;
	MAKETEXTINFO mti;

	if ( mode == MIT_NEXT ){ // 読み込み中の処理
		if ( VOi->reading == FALSE ) return 0;
		count = param;
		line = VOi->cline;
		vo = mtinfo.vo;
		of = ((VT_TABLE *)vo->ptr)[line].ptr;

		vmax = VOi->img + mtinfo.MemSize;
		next = ((VOi->line / ISTEP) + 1) * ISTEP;
	}else if ( mode == MIT_REMAKE ){ // 再計算
		vo = mtinfo.vo;
		line = 0;

		of = VOi->img = mtinfo.img;
		vmax = VOi->img + mtinfo.MemSize;
		next = GlobalSize(vo->mapH) / sizeof(VT_TABLE);
		if ( mtinfo.PresetPos == 0 ){
			mtinfo.PresetY = param; // 読み込み完了後の表示位置を指定(●1.2x暫定)
			mtinfo.PresetPos = ((VT_TABLE *)vo->ptr)[param].ptr - of;
			count = param + VO_sizeY + 10000;
		}else{
			count = mtinfo.PresetY + VO_sizeY + 100;
		}
		VOi->cline = VOi->line = 0;
	}else{ // 新規
		mtinfo.img = VOi->img;
		mtinfo.PresetPos = 0;

		count = 4000;
		line = 0;
		mtinfo.vo = vo = param == MIT_PARAM_TEXT ? &vo_.text.text : &vo_.text.document;
		of = VOi->img;
		vmax = of + mtinfo.MemSize;

		if ( vo->mapH == NULL ){
			if( NULL == (vo->mapH =
					GlobalAlloc(GMEM_MOVEABLE,sizeof(VT_TABLE) * ISTEP))){
				VO_error(PPERROR_GETLASTERROR);
				VOi->reading = FALSE;
				return -1;
			}
			if ( NULL == (vo->ptr = GlobalLock(vo->mapH)) ){
				VO_error(PPERROR_GETLASTERROR);
				VOi->reading = FALSE;
				return -1;
			}
			VOi->line = 0;
			next = ISTEP;
		}else{
			next = ((VOi->line / ISTEP) + 1) * ISTEP;
		}
		((VT_TABLE *)vo->ptr)->ptr  = of;
		((VT_TABLE *)vo->ptr)->Fclr = CV__deftext;
		((VT_TABLE *)vo->ptr)->Bclr = CV__defback;
		((VT_TABLE *)vo->ptr)->type = (BYTE)VOi->textC;
		((VT_TABLE *)vo->ptr)->attrs = VTTF_TOP;
		((VT_TABLE *)vo->ptr)->line = 1;
	}

	mti.destbuf = buf;
	mti.srcmax = vmax;
	mti.writetbl = TRUE;
	mti.paintmode = FALSE;

	while( of < vmax ){
		if ( (line + 5) > next ){ // メモリの再確保処理 -----------------
			HGLOBAL hTmp;

			next += ISTEP;
			GlobalUnlock(vo->mapH);
			hTmp = GlobalReAlloc(vo->mapH,
					sizeof(VT_TABLE) * next,GMEM_MOVEABLE);
			if ( hTmp == NULL ){
				VO_error(PPERROR_GETLASTERROR);
				VOi->reading = FALSE;
				return -1;
			}
			vo->mapH = hTmp;
			if ( (vo->ptr = GlobalLock(vo->mapH)) == NULL ){
				VO_error(PPERROR_GETLASTERROR);
				VOi->reading = FALSE;
				return -1;
			}
		}
		if ( (mtinfo.PresetPos != 0) && (of >= (mtinfo.img + mtinfo.PresetPos) ) ){
			mtinfo.PresetY = line;
			if ( line && (of > (mtinfo.img + mtinfo.PresetPos)) ){
				mtinfo.PresetY--;
			}
			mtinfo.PresetPos = 0;
		}

										// 変換 -------------------------------
		of = VOi->MakeText(&mti,&((VT_TABLE *)vo->ptr)[line]);
		if ( of < VOi->img ){
			VOi->reading = FALSE;
			break;
		}

		if ( (of < vmax) || (ReadingStream == READ_NONE) ) line++;
		if ( !--count ) break;
	}

	if ( (line >= 2) && !((mode == MIT_NEXT) && (VOi->cline == 0)) ){
		BYTE *p;

		p = ((VT_TABLE *)vo->ptr)[2 - 1].ptr;
		if ( !memcmp(p ,"GET ",4) ){
			p += 4;
			tstrcpy(vo_.file.source,T("http://"));
			TGetLineParam((const char **)&p,vo_.file.source + 7);
		}
		if ( !memcmp(p ,"CONNECT ",8) ){
			TCHAR *cp;

			p += 8;
			tstrcpy(vo_.file.source,T("https://"));
			TGetLineParam((const char **)&p,vo_.file.source + 8);
			cp = tstrrchr(vo_.file.source + 8,':');
			if ( cp != NULL ) *cp = '\0';
		}
	}

	if ( of >= vmax ){
		VOi->reading = FALSE;
		mtinfo.PresetPos = 0;
	}else{
		if ( mode == MIT_REMAKE ){ // 再計算
			VOi->reading = TRUE;
			SetTimer(vinfo.info.hWnd,TIMERID_READLINE,TIMER_READLINE,BackReaderProc);
			BackReader = TRUE;
		}
	}

	if ( line >= 0 ){
		VOi->cline = line;
		if ( (VOi->line < line) || (of >= vmax) ) VOi->line = line;
	}
	VOi->ti = (VT_TABLE *)vo->ptr;
	return line;
}
//-----------------------------------------------------------------------------


int USEFASTCALL GetHex(BYTE c)
{
	CharUPR(c);
	c -= (BYTE)'0';
	if ( c > 9 ) c = (BYTE)( c - 7 );
	return c;
}

void RecalcWidthA(TEXTCODEINFO *tci)
{
	SIZE range;

	if ( tci->CalcTextPtr < tci->textfirst ) tci->CalcTextPtr = tci->textfirst;
	GetTextExtentPoint32A(tci->hDC,(char *)tci->CalcTextPtr,(tci->dest - tci->textfirst - 1),&range);
	tci->PxWidth -= range.cx;
	if ( tci->PxWidth < 0 ) tci->PxWidth = 0;
	tci->cnt = tci->PxWidth / fontX;
}

void RecalcWidthW(TEXTCODEINFO *tci)
{
	SIZE range;

#ifndef UNICODE
	if ( OSver.dwPlatformId != VER_PLATFORM_WIN32_NT ) return;
#endif
	if ( tci->CalcTextPtr < tci->textfirst ) tci->CalcTextPtr = tci->textfirst;
	GetTextExtentPoint32W(tci->hDC,(WCHAR *)tci->CalcTextPtr,(tci->dest - tci->textfirst - 1) / sizeof(WCHAR),&range);
	tci->PxWidth -= range.cx;
	if ( tci->PxWidth < 0 ) tci->PxWidth = 0;
	tci->cnt = tci->PxWidth / fontX;
}

void USEFASTCALL CloseVcode(TEXTCODEINFO *tci)
{
	const HILIGHTKEYWORD *hks;

	switch ( tci->vcode ){
		case VCODE_CONTROL:
			*(BYTE *)(tci->dest) = '\0';
			tci->dest += 1;
			if ( XV_unff ) RecalcWidthA(tci);
			return;

		case VCODE_ASCII:{
			*(BYTE *)(tci->dest) = '\0';
			tci->dest += 1;
			if ( XV_unff ) RecalcWidthA(tci);

			if ( tci->paintmode != FALSE ){
				if ( VOsel.highlight ){
					CheckSearchAscii(tci, VOsel.VSstringA, SEARCHHILIGHT_COLORREF, 0);
				}
				for ( hks = X_hkey ; hks ; hks = hks->next ){
					CheckSearchAscii(tci, hks->ascii, hks->color, hks->extend);
				}
			}
			return;
		}

		case VCODE_UNICODE:{
			*(WORD *)tci->dest = '\0';
			tci->dest += 2;
			if ( XV_unff ) RecalcWidthW(tci);

			if ( tci->paintmode != FALSE ){
				if ( VOsel.highlight ){
					CheckSearchUNICODE(tci, VOsel.VSstringW, SEARCHHILIGHT_COLORREF, 0);
				}
				for ( hks = X_hkey ; hks ; hks = hks->next ){
					CheckSearchUNICODE(tci, hks->wide, hks->color, hks->extend);
				}
			}
			return;
		}
		//default:
	}
}

void USEFASTCALL SetVcode(TEXTCODEINFO *tci, int setvcode)
{
	if ( tci->vcode == setvcode ) return;
	CloseVcode(tci);

	if ( (setvcode == VCODE_UNICODE) && !(ALIGNMENT_BITS(tci->dest) & 1) ){
		*tci->dest++ = VCODE_UNICODEF;
	}
	*tci->dest++ = (BYTE)setvcode;
	tci->textfirst = tci->dest;
	tci->vcode = setvcode;
}

#ifdef UNICODE
void SetUchar(TEXTCODEINFO *tci, DWORD code)
{
	const WORD *destptr;
	WORD types[4];

	SetVcode(tci,VCODE_UNICODE);
	destptr = (const WORD *)tci->dest;
	if ( code < 0x10000 ){
		*tci->dest++ = (BYTE)code;
		*tci->dest++ = (BYTE)(code >> 8);
		GetStringTypeExW(0,CT_CTYPE3,destptr,2,types);
	}else{	// サロゲートペア
		DWORD hicode;

		hicode = (code >> 10) + (0xd800 - (0x10000 >> 10));
		*tci->dest++ = (BYTE)hicode;
		*tci->dest++ = (BYTE)(hicode >> 8);
		code = (code & 0x3ff) | 0xdc00;
		*tci->dest++ = (BYTE)code;
		*tci->dest++ = (BYTE)(code >> 8);
		GetStringTypeExW(0,CT_CTYPE3,destptr,2,types);
	}
	tci->cnt -= (types[0] & C3_HALFWIDTH) ? 1 : 2;
}
#else
void SetUchar(TEXTCODEINFO *tci, DWORD code)
{
	const WORD *destptr;

	SetVcode(tci,VCODE_UNICODE);
	destptr = (const WORD *)tci->dest;
	if ( code < 0x10000 ){
		*tci->dest++ = (BYTE)code;
		*tci->dest++ = (BYTE)(code >> 8);
		// Win9x は、GetStringTypeExW が対応していない
		tci->cnt -= WideCharToMultiByte(CP_ACP,0,destptr,1,NULL,0,"・",NULL);
	}else{	// サロゲートペア
		DWORD hicode;

		hicode = (code >> 10) + (0xd800 - (0x10000 >> 10));
		*tci->dest++ = (BYTE)hicode;
		*tci->dest++ = (BYTE)(hicode >> 8);
		code = (code & 0x3ff) | 0xdc00;
		*tci->dest++ = (BYTE)code;
		*tci->dest++ = (BYTE)(code >> 8);
		tci->cnt -= 2;
	}
}
#endif
/*-----------------------------------------------------------------------------
	表示用文字列を作成する
	tbl:	各種情報（次の行の情報を保存するので、少なくとも２以上の配列）
	max:	イメージの末尾のポインタ
-----------------------------------------------------------------------------*/
BYTE *MakeDispText(MAKETEXTINFO *mti,VT_TABLE *tbl)
{
	BYTE *text;		// 読み込み位置
	BYTE tmp[DESTBUFSIZE];
	BYTE *text2 = NULL,*srcmax,*srcmax2 = NULL;
	BOOL urldecode = FALSE;
	int dcode,dcode2 = 0;		// 文字コードの解析方法
	int attrs;
	int ct;
	HFONT hOldFont C4701CHECK;

	TEXTCODEINFO tci;

	tci.dest = tci.textfirst = tci.CalcTextPtr = mti->destbuf;
	tci.cnt = VOi->width;
	tci.destmax = tci.dest + TEXTBUFSIZE - 16;
	tci.vcode = VCODE_END;
	tci.paintmode = mti->paintmode;

	if ( XV_unff ){
		tci.hDC = GetDC(vinfo.info.hWnd);
		hOldFont = SelectObject(tci.hDC,hUnfixedFont);
		tci.PxWidth = tci.cnt * fontX;
	}

	text	= tbl->ptr;
	tci.Fclr	= tbl->Fclr;
	tci.Bclr	= tbl->Bclr;
	dcode	= tbl->type;
	attrs	= tbl->attrs & ~VTTF_TOP;
//	if ( attrs & VTTF_END ) attrs ^= VTTF_END | VTTF_TOP; // VTTF_TOP を立てる

	if ( text == NULL ) tci.cnt = 0;

	srcmax = mti->srcmax;
	while( (tci.cnt > 0) && (tci.dest < tci.destmax) ){
		int c,d;

		if ( text >= srcmax ){
			if ( text2 != NULL ){
				text = text2;
				srcmax = srcmax2;
				dcode = dcode2;
				text2 = NULL;
				continue;
			}
			break;
		}
		c = *text;

		if ( VO_Tquoted && (c == '=') ){ // =xx Quoted-printable を置換 -------
			c = DecodeQuotedPrintable(&text);
			if ( c == '\0' ) continue;
		}
										// URL の %xx を置換 ------------------
		if ( dcode != VTYPE_JIS ){
			if ( (c == ':') && (*(text + 1)=='/') ){
				urldecode = TRUE;
			}else if ( (c == '%') && IsTrue(urldecode) && VO_Ttag &&
						IsxdigitA(*(text+1)) && IsxdigitA(*(text+2)) ){
				if ( text2 == NULL ){
					BYTE *np;
					int newdcode;
					size_t size;

					np = text;
					newdcode = UrlDecode(&text,tmp,&size);
					if ( np < text ){
						dcode2 = dcode;
						dcode = newdcode;
						srcmax2 = srcmax;
						text2 = text;
						text = tmp;
						srcmax = text + size;
						c = *text;
					}
				}
			}
		}
		if ( (c == 'c') && VO_Tesc && (dcode != VTYPE_UNICODE) ){
			if ( !memcmp(text + 1,"harset=",7) ) CheckCharset(text,&dcode);
		}
										// RTF 簡易解析 -----------------------
		if ( dcode == VTYPE_RTF ){
			if ( c == '\\' ){
				text++;
				c = DecodeRTF(&tci,&text);
				if ( c == '\0' ) continue;
			}else if ( (c < ' ') || (c == ';') || (c == '{') ){
				text++;
				continue;
			}
		}else
											//JIS/KANA ------------------------
		if ( dcode == VTYPE_KANA ){
							// 7bit コードなのに 8bit目が使用→誤判別の可能性
			if ( (c & 0x80) && (VOi->textC != VTYPE_KANA) ){
				if ( VO_Tmode == 0 ){
					VO_Tmode = 1;
					dcode = VOi->textC;
				}
			}
			if ( (c >= 0x20) && ( c <= 0x5f) ){
				SetVcode(&tci,VCODE_ASCII);
				*tci.dest++ = (BYTE)(c + 0x80);
				text++;
				tci.cnt--;
				continue;
			}
		}else
											//UNICODE -------------------------
		if ( dcode == VTYPE_UNICODE ){
			text++;
			c += *text << 8;
			if ( c >= 0x20 ){
				text++;
				if ( tci.cnt < 2 ){
					tci.cnt = 0;
					text -= 2;
					break;
				}
				if ( (c == L'　') && XV_bctl[2] ){
					CloseVcode(&tci);
					tci.vcode = VCODE_END;
					*tci.dest++ = VCODE_WSPACE;
					tci.cnt -= 2;
					continue;
				}
				if ( VO_Ttag ){
					if ( c == '<' ){
						WORD *tagend;
						int len = X_tlen;

						tagend = (WCHAR *)text;
												// 本当にTAGかを確認
						for ( ; len ; len-- ){
							if ( ++tagend >= (WORD *)srcmax ){
								len = 0;
								break;
							}
							if ( *tagend == '>' ) break;
							if ( (*tagend == '\r') || (*tagend == '\n') ){
								continue;
							}
							if ( *tagend < 0x20 ){
								len = 0;
								break;
							}
						}

						if ( len ){ // 色づけのみ
							CloseVcode(&tci);
							tci.vcode = VCODE_END;
							*tci.dest++ = VCODE_FCOLOR;
							*(COLORREF *)tci.dest = CV_syn[
								((*((WORD *)text + 1) == '!') &&
								 (*((WORD *)text + 2) == '-') &&
								 (*((WORD *)text + 3) == '-')) ?
								1 : 0
							];
							tci.dest += sizeof(COLORREF);
							setflag(attrs, VTTF_TAG);
						}
					}
					if ( c == '>' ){
						SetUchar(&tci,c);
						CloseVcode(&tci);
						tci.vcode = VCODE_END;
						*tci.dest++ = VCODE_COLOR;
						*tci.dest++ = CV__deftext;
						*tci.dest++ = CV__defback;
						resetflag(attrs, VTTF_TAG);
						continue;
					}
				}

				SetUchar(&tci,c);
				continue;
			}
		}else

		if ( dcode == VTYPE_UNICODEB ){
			text++;
			c = *text + (c << 8);
			if ( c >= 0x20 ){
				text++;
				if ( tci.cnt < 2 ){
					tci.cnt = 0;
					text-=2;
					break;
				}
				if ( (c == L'　') && XV_bctl[2] ){
					CloseVcode(&tci);
					tci.vcode = VCODE_END;
					*tci.dest++ = VCODE_WSPACE;
					tci.cnt -= 2;
					continue;
				}
				SetUchar(&tci,c);
				continue;
			}
		}else
		if ( dcode == VTYPE_UTF8 ){
			if ( c & 0x80 ){
				if ( tci.cnt < 2 ){
					tci.cnt = 0;
					break;
				}
				if ( !VO_Tquoted ){
					c = Utf8toWchar(&text,c);
				}else{
					c = MimeUtf8toWchar(&text,c);
				}
				SetUchar(&tci,c);
				continue;
			}
		}else
											// S-JIS/B ------------------------
		if ( dcode == VTYPE_SJISB ){
			if ( c > 0x20 ){
				d = *(text + 1);

				SetVcode(&tci,VCODE_ASCII);
				if ( IskanjiA(d) ){
					if ( tci.cnt < 2 ){
						tci.cnt = 0;
						break;
					}
					text += 2;
					*tci.dest++ = (BYTE)d;
					tci.cnt -= 2;
				}else{
					if ( (d == 0x20) && (c & B7) ) text++;
					text++;
					tci.cnt--;
				}
				*tci.dest++ = (BYTE)c;
				continue;
			}else{
				if ( (c > '\0') && (c < 0x20) && (*(text + 1) == '\0') ) text++;
			}
		}else
//-----------------------------------------------------------------------------
		if ( (c == 0x80) && (dcode == VTYPE_ANSI) && (OSver.dwMajorVersion < 6) ){	// ユーロ単価
			DecodeEuro(&tci,dcode);
			text++;
			continue;
		}

		ct = T_CHRTYPE[(unsigned char)c];
		if ( ct & T_IS_CTL ){				// コントロールコード -------------
			if ( IsTrue(DecodeControlCode(&tci,&text,c,&attrs,&dcode)) ) continue;
			break;
		}
		SetVcode(&tci,VCODE_ASCII);
										// <tag> ------------------------------
		if ( (c == '<') && VO_Ttag && (dcode != VTYPE_JIS ) ){
			if ( IsTrue(DecodeHtmlTag(&tci,&text,srcmax,&attrs)) ) continue;
		}
		if ( (c == '>') && (VO_Ttag > 1) && (dcode != VTYPE_JIS ) ){
			*tci.dest++ = '>';
			CloseVcode(&tci);
			tci.vcode = VCODE_END;
			*tci.dest++ = VCODE_COLOR;
			*tci.dest++ = CV__deftext;
			*tci.dest++ = CV__defback;
			text++;
			resetflag(attrs, VTTF_TAG);
			continue;
		}
										// html spcial charchter --------------
		if ( (c == '&') && VO_Ttag && (dcode != VTYPE_JIS) ){
			if ( IsTrue(DecodeHtmlSpecialChar(&tci,&text)) ) continue;
		}
										// mime decode ------------------------
		if ( (dcode != VTYPE_JIS) && (c == '=') && VO_Tesc &&
				(*(text + 1) == '?') && (text2 == NULL) ){
			if ( MimeDecode(&text,&srcmax,&dcode,&text2,&srcmax2,&dcode2,tmp) ){
				continue;
			}
		}
										// mime decode(utf-7) -----------------
		if ( (dcode == VTYPE_UTF7) && (c == '+') ){
			if ( IsTrue(DecodeUTF7(&tci,&text)) ) continue;
		}
										// S-JIS ------------------------------
		if ( ((dcode == VTYPE_SYSTEMCP) || (dcode == VTYPE_SJISNEC) || (dcode == VTYPE_RTF) || (VO_CodePage == CP__SJIS)) && (ct & T_IS_KNJ) ){
			BYTE c2;

			if ( tci.cnt < 2 ){
				if ( XV_unff ){
					if ( tci.vcode == VCODE_ASCII ){
						RecalcWidthA(&tci);
					}else if ( tci.vcode == VCODE_UNICODE ){
						RecalcWidthW(&tci);
					}
					if ( tci.cnt < 2 ){
						tci.cnt = 0;
						break;
					}
				}else{
					tci.cnt = 0;
					break;
				}
			}
			*tci.dest++ = (BYTE)c;
			text++;
			c2 = *text;
			if ( c2 != '\0' ){
				if ( VO_Tquoted ){
					if ( c2 == '=' ){
						if ((*(text + 1) == '\r') && (*(text + 2)=='\n') ){
							text += 3;
							c2 = *text;
						}else if ( *(text + 1) == '\n' ){
							text += 2;
							c2 = *text;
						}
					}
					if ( (c2 == '=') && IsxdigitA(*(text + 1)) && IsxdigitA(*(text+2)) ){
						c2 = (BYTE)((GetHex(*(text + 1)) * 16) + GetHex(*(text +2)));
						text += 2;
					}
				}
				text++;
				if ( (c == 0x81) && (c2 == 0x40) && XV_bctl[2] ){ // 全角空白
					tci.dest--;
					CloseVcode(&tci);
					tci.vcode = VCODE_END;
					*tci.dest++ = VCODE_SPACE;
					tci.cnt -= 2;
					continue;
				}else{
					*tci.dest++ = c2;
				}
			}else{
				*tci.dest++ = ' ';
			}

			if ( (dcode == VTYPE_SJISNEC) && ((c == 0x85) || (c == 0x86)) && (*(tci.dest - 1) >= 0x40) ){
				WORD a;
				BYTE sjl;

				a = NECCHAR[c - 0x85][*(tci.dest - 1) - 0x40];
				*(BYTE *)(tci.dest - 2) = SJIS1st(a);
				sjl = SJIS2nd(a);
				if ( sjl == '\0' ){
					tci.cnt++;
					tci.dest--;
				}else{
					*(BYTE *)(tci.dest - 1) = sjl;
				}
			}
			tci.cnt -= 2;
			continue;
		}else
										// EUC JP -----------------------------
		if ( (dcode == VTYPE_EUCJP) && (c >= 0x80) ){
			if ( IsTrue(DecodeEUCJP(&tci,&text)) ) continue;
			break;
		}else
										// JIS --------------------------------
		if ( dcode == VTYPE_JIS ){
			if ( IsTrue(DecodeJIS(&tci,&text,srcmax)) ) continue;
			break;
		}
		*tci.dest++ = (BYTE)c;
		text++;
		tci.cnt--;
		if ( (tci.cnt == 0) && (XV_unff != 0) ){
			if ( tci.vcode == VCODE_ASCII ){
				RecalcWidthA(&tci);
			}else if ( tci.vcode == VCODE_UNICODE ){
				RecalcWidthW(&tci);
			}
		}
	}
	CloseVcode(&tci);
	*tci.dest++ = VCODE_END;
	*tci.dest = VCODE_END;

	if ( text2 != NULL ){
		text = text2;
		dcode = dcode2;
	}
	if ( mti->writetbl ){
		tbl++;
		tbl->ptr	= text;
		tbl->Fclr	= (BYTE)tci.Fclr;
		tbl->Bclr	= (BYTE)tci.Bclr;
		tbl->type	= (BYTE)dcode;
		tbl->attrs	= (BYTE)attrs;
		tbl->line	= (tbl - 1)->line + ((attrs & VTTF_TOP) ? 1 : 0);
	}
	if ( IsTrue(XV_unff) ){
		SelectObject(tci.hDC,hOldFont); // C4701ok
		ReleaseDC(vinfo.info.hWnd,tci.hDC);
	}
	return text;
}
