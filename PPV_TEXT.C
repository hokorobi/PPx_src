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

#define EUROCHAR 0 // code:0x80 をユーロ単価となるように調整する

/*
	CharNextExA

	base64
	↓
	=xx, %xx
	↓
	S-JIS/UNICODE/JIS
	↓
	tag
*/
#define ISTEP	1024
#define DESTBUFSIZE	1024

#define DECODE_NONE		0
#define DECODE_PROP		1	// =?xxx?B?xxxx?=
#define DECODE_BASE64	2	// 1行BASE64
#define DECODE_URL		3	// http://～%xx%xx%xx

typedef struct {
	BYTE *dest, *destmax, *extlast; // 書き込み位置と、書き込みバッファ末尾
	BYTE *text, *srcmax;
	BYTE *destfirst; // 表示文字列先頭
	BYTE *CalcTextPtr; // 文字列幅計算基準位置
	int cnt, extcnt;
	int PxWidth;
	HDC hDC;
	int vcode;		// 現在の buf への書き出しモード
	BOOL paintmode;
	int Fclr, Bclr;	// 前景色, 背景色
	int attrs;
	int dcode;		// 文字コードの解析方法

	struct {
		BYTE *text, *textfirst;
		BYTE *srcmax;
		int dcode;
		int type;
		int offset;
		BYTE TextBuf[DESTBUFSIZE];
	} oldtext;
} TEXTCODEINFO;

#pragma warning(disable:4245) // VCで、カタカナなどの定義で警告が出るのを抑制
WORD NECCHAR[2][0xc0] = {
// 8540
{'!', '"', '#', '$', '%', '&', '\'',
 '(', ')', '*', '+', ',', '-', '.', '/',
 '0', '1', '2', '3', '4', '5', '6', '7',
 '8', '9', ':', ';', '<', '=', '>', '?',
 '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
 'X', 'Y', 'Z', '[', '\\',']', '^', '_', 0x4081, // '　',
 '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
 'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
 'x', 'y', 'z', '{', '|', '}', '~',
// 859f
 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, // '｡', '｢', '｣', '､', '･', 'ｦ', 'ｧ',
 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, // 'ｨ', 'ｩ', 'ｪ', 'ｫ', 'ｬ', 'ｭ', 'ｮ', 'ｯ',
 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, // 'ｰ', 'ｱ', 'ｲ', 'ｳ', 'ｴ', 'ｵ', 'ｶ', 'ｷ',
 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, // 'ｸ', 'ｹ', 'ｺ', 'ｻ', 'ｼ', 'ｽ', 'ｾ', 'ｿ',
 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, // 'ﾀ', 'ﾁ', 'ﾂ', 'ﾃ', 'ﾄ', 'ﾅ', 'ﾆ', 'ﾇ',
 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, // 'ﾈ', 'ﾉ', 'ﾊ', 'ﾋ', 'ﾌ', 'ﾍ', 'ﾎ', 'ﾏ',
 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, // 'ﾐ', 'ﾑ', 'ﾒ', 'ﾓ', 'ﾔ', 'ﾕ', 'ﾖ', 'ﾗ',
 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, // 'ﾘ', 'ﾙ', 'ﾚ', 'ﾛ', 'ﾜ', 'ﾝ', 'ﾞ', 'ﾟ',
0x9083, 0x9183, 0xdc, 0xb6, 0xb9, 0x9483, 0x4b83, 0x4d83, // 'ヰ', 'ヱ', 'ﾜ', 'ｶ', 'ｹ', 'ヴ', 'ガ', 'ギ',
0x4f83, 0x5183, 0x5383, 0x5583, 0x5783, 0x5983, 0x5b83, 0x5d83, // 'グ', 'ゲ', 'ゴ', 'ザ', 'ジ', 'ズ', 'ゼ', 'ゾ',
0x5f83, 0x6183, 0x6483, 0x6683, 0x6883, 0x6f83, 0x7083, 0x7283, // 'ダ', 'ヂ', 'ヅ', 'デ', 'ド', 'バ', 'パ', 'ビ',
0x7383, 0x7583, 0x7683, 0x7883, 0x7983, 0x7b83, 0x7c83, // 'ピ', 'ブ', 'プ', 'ベ', 'ペ', 'ボ', 'ポ',
0x4581, 0x4581, 0x4581}, // '・', '・', '・'
// 8640
{' ', '"', '"', '-', '-', '|', '|',
 '-', '-', '|', '|', '-', '-', '|', '|',
 '+', '+', '+', '+', '+', '+', '+', '+',
 '+', '+', '+', '+', '+', '+', '+', '+',
 '+', '+', '+', '+', '+', '+', '+', '+',
 '+', '+', '+', '+', '+', '+', '+', '+',
 '+', '+', '+', '+', '+', '+', '+', '+',
 '+', '+', '+', '+', '+', '+', '+', '+', 0x4081, // '　',
 '+', '+', '+', '+', '+', '+', '+', '+',
 '+', '+', '+', '+', '+', '+', '+', '+',
 '`', '･', '\'', '"', '[', ']', '<', '>',
 '<', '>', '｢', '｣', '[', ']', '-',
// 869f
0x4081, 0x4081, 0x4081, 0x9f84, 0xaa84, 0xa084, 0xab84, // '　', '　', '　', '─', '━', '│', '┃',
0x9f84, 0xaa84, 0xa084, 0xab84, 0x9f84, 0xaa84, 0xa084, 0xab84, // '─', '━', '│', '┃', '─', '━', '│', '┃',
0xa184, 0xa184, 0xa184, 0xac84, 0xa284, 0xa284, 0xa284, 0xad84, // '┌', '┌', '┌', '┏', '┐', '┐', '┐', '┓',
0xa484, 0xa484, 0xa484, 0xaf84, 0xa384, 0xa384, 0xa384, 0xae84, // '└', '└', '└', '┗', '┘', '┘', '┘', '┛',
0xa584, 0xba84, 0xa584, 0xa584, 0xb584, 0xa584, 0xa584, 0xb084, // '├', '┝', '├', '├', '┠', '├', '├', '┣',
0xa784, 0xbc84, 0xa784, 0xa784, 0xb784, 0xa784, 0xa784, 0xb284, // '┤', '┥', '┤', '┤', '┨', '┤', '┤', '┫',
0xa684, 0xa684, 0xa684, 0xb684, 0xbb84, 0xa684, 0xa684, 0xb184, // '┬', '┬', '┬', '┯', '┰', '┬', '┬', '┳',
0xa884, 0xa884, 0xa884, 0xb884, 0xbd84, 0xa884, 0xa884, 0xb384, // '┴', '┴', '┴', '┷', '┸', '┴', '┴', '┻',
0xa984, 0xa984, 0xa984, 0xb984, 0xa984, 0xa984, 0xbe84, 0xa984, // '┼', '┼', '┼', '┿', '┼', '┼', '╂', '┼',
0xb484, 0xb484, 0xb484, 0xb484, 0xb484, 0xb484, 0xb484, 0xb484, // '╋', '╋', '╋', '╋', '╋', '╋', '╋', '╋',
0x4581, 0x4581, 0x4581, 0x4581, 0x4581, 0x4581, 0x4581, 0x4581, // '・', '・', '・', '・', '・', '・', '・', '・',
0x4581, 0x4581, 0x4581, 0x4581, 0x4581, 0x4581, 0x4581, // '・', '・', '・', '・', '・', '・', '・',
0x4581, 0x4581, 0x4581, // '・', '・', '・'
}};
#pragma warning(default:4245)

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
	{"Epsilon;", 917},
	{"Zeta;",	918},
	{"Eta;",	919},
	{"Theta;",	920},
	{"Iota;",	921},
	{"Kappa;",	922},
	{"Lambda;",	923},
	{"Mu;",		924},
	{"Nu;",		925},
	{"Xi;",		926},
	{"Omicron;", 927},
	{"Pi;",		928},
	{"Rho;",	929},
	{"Sigma;",	931},
	{"Tau;",	932},
	{"Upsilon;", 933},
	{"Phi;",	934},
	{"Chi;",	935},
	{"Psi;",	936},
	{"Omega;",	937},
	{"alpha;",	945},
	{"beta;",	946},
	{"gamma;",	947},
	{"delta;",	948},
	{"epsilon;", 949},
	{"zeta;",	950},
	{"eta;",	951},
	{"theta;",	952},
	{"iota;",	953},
	{"kappa;",	954},
	{"lambda;",	955},
	{"mu;",		956},
	{"nu;",		957},
	{"xi;",		958},
	{"omicron;", 959},
	{"pi;",		960},
	{"rho;",	961},
	{"sigmaf;",	962},
	{"sigma;",	963},
	{"tau;",	964},
	{"upsilon;", 965},
	{"phi;",	966},
	{"chi;",	967},
	{"psi;",	968},
	{"omega;",	969},
	{"thetasym;", 977},
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


int B64DecodeBytes(BYTE **src, BYTE *dst);
int USEFASTCALL GetHex(BYTE c);
void SetUchar(TEXTCODEINFO *tci, DWORD code);
void USEFASTCALL SetVcode(TEXTCODEINFO *tci, int setvcode);
void USEFASTCALL CloseVcode(TEXTCODEINFO *tci);
int GetHexNumberA(const char **ptr);

typedef struct {
	DWORD cp;
	char *cptext;
} CPTEXTS;

CPTEXTS cptext[] = {
	{VTYPE_JIS, "iso-2022-jp"},
	{28591, "iso-8859-1"},
	{VTYPE_UTF7, "UTF-7"},
	{VTYPE_UTF8, "UTF-8"},
	{0, NULL}
};

const char ReplaceChar[] = "・";

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

//                         17, 18, 19, 20, 21, 22, 23
const int ESC_NEC_C1[] = {  1,  3,  6,  2,  4,  5,  7};
//                     30, 31, 32, 33, 34, 35, 36, 37, 38
const int ESC_C2[] = {  0,  1,  2,  4,  3,  6,  5,  7, 7};

BOOL EscCSI(TEXTCODEINFO *tci, int dcode)
{
	int n, nfg = CV__deftext, nbg = CV__defback;
	BOOL rev = FALSE;
	BYTE *pp;

	pp = tci->text;
	pp++;

// ESC [ n @	ICH
// ESC [ n A	CUU Up
// ESC [ n B	CUD Down
// ESC [ n C	CUF Right
// ESC [ n D	CUB Left
// ESC [ n E	CNK
// ESC [ n F	CPL End
// ESC [ n G	CHA Num5
// ESC [ y;x H	CUP
// ESC [ n J	ED
// ESC [ n K	EL
// ESC [ n L	IL
// ESC [ n M	DL
// ESC [ n P	DCH
// ESC [ n S	SU
// ESC [ n T	SD
// ESC [ n X	ECH
// ESC [ n a	HPR
// ESC [ n c	DA
// ESC [ n d	VPA
// ESC [ n e	VPR
// ESC [ y;x f	HVP
// ESC [ n g	TBC
// ESC [ n h	SM
	// ESC [ 3 h	DECCRM
	// ESC [ 4 h	DECCIM
	// ESC [ 20 h	LF/NL
// ESC [ n l	RM
// ESC [ n n	DSR
	// ESC [ 5 n	DSR
	// ESC [ 6 n	DSR
// ESC [ n q	DECLL
// ESC [ n r	DECSTBM
// ESC [ s		SAVE CURSOR POS
// ESC [ u		RESTORE CURSOR POS
// ESC [ m;n`	HPA
// ESC [ ? 1 h	DECCKM
// ESC [ ? 3 h	DECCLM
// ESC [ ? 5 h	DECSCNM
// ESC [ ? 6 h	DECOM
// ESC [ ? 7 h	DECAWM
// ESC [ ? 8 h	DECARM
// ESC [ ? 9 h	X10 mouse report 1
// ESC [ ? 25 h	DECTECM show mouse
// ESC [ ? 1000 h	X10 mouse report 2
// ESC [ ? 25 l	DECTCEM hide mouse
// ESC [ 1 ; n ]	Linux Underline color
// ESC [ 2 ; n ]	Linux DIM color
// ESC [ 8 ]	Linux save defalut color
// ESC [ 9 ; n ]	Black out 分
// ESC [ 10 ; n ]	bell Hz
// ESC [ 11 ; n ]	bell msec
// ESC [ 12 ; n ]	select console
// ESC [ 13 ]		unblank
// ESC [ 14 ]		reset blank out timer

// ESC [ 1 ~	vt Home
// ESC [ 2 ~	vt Insert
// ESC [ 3 ~	vt Delete
// ESC [ 10 ~	vt F0
// ESC [ 11 ~	vt F1
// ESC [ 34 ~	vt F20

// ESC [ A ~	vt F20

// ESC [ n m	SGR
	if ( (*pp == '3') || (*pp == '4') ){ // 3x, 4x
		BYTE RGB[4];
		char mode = *pp;

		if ( memcmp(pp + 1, "8;2;", 4) == 0 ){ // 38;2;r;g;b / 48;2;r;g;b
			pp += 5;
			RGB[0] = (BYTE)(GetIntNumberA((const char **)&pp) ); // R
			if ( *pp == ';' ){
				pp++;
				RGB[1] = (BYTE)(GetIntNumberA((const char **)&pp) ); // G
				if ( *pp == ';' ){
					pp++;
					RGB[2] = (BYTE)(GetIntNumberA((const char **)&pp) ); // B
					if ( *pp != 'm' ) return FALSE;
					tci->text = pp + 1;
					RGB[3] = 0;
					CloseVcode(tci);
					tci->vcode = VCODE_END;
					*tci->dest = (BYTE)((mode == '3') ? VCODE_FCOLOR : VCODE_BCOLOR);
					*(DWORD *)(tci->dest + 1) = *(DWORD *)&RGB;
					tci->dest += 5;
					tci->destfirst = tci->dest;
					return TRUE;
				}
			}
		}else if ( memcmp(pp + 1, "8;5;", 4) == 0 ){ // 38;5;x / 48;5;x
			pp += 5;
			n = GetIntNumberA((const char **)&pp);
			if ( *pp != 'm' ) return FALSE;

			tci->text = pp + 1;
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
			tci->destfirst = tci->dest;
			return TRUE;
		}
	}

	for (;;){
		n = GetIntNumberA((const char **)&pp);

		if ( (dcode == VTYPE_SJISNEC) && (n >= 17) && (n <= 23) ){
			// 21～23 ( NEC98 )
			nfg = ESC_NEC_C1[n - 17];
		}else if ( n >= 30 ){ // 30～39, 40～49, 90～99, 100～109
			int Light;
			BOOL Back;

			Light = 8;
			Back = FALSE;

			n -= 30;
			if ( n >= (90 - 30) ){ // 90～99, 100～109
				n -= (90 - 30);
				Light = 0;
			}
			if ( n >= (40 - 30) ){
				if ( n <= (49 - 30) ){ // 40～49, 100～109
					Back = TRUE;
					n -= 10;
				}else{
					n = -1;
				}
			}
			if ( (n >= 0) && (n <= 8) ){ // x0-x8 色
				Light += ESC_C2[n];
				if ( Back ){
					nbg = Light;
					if ( dcode == VTYPE_SJISNEC ) nfg = CV__defback;
				}else{
					nfg = Light;
					if ( dcode == VTYPE_SJISNEC ) nbg = CV__defback;
				}
			}
			// 38 下線+文字初期色
			// 39 下線なし+文字初期色
			// 49 背景初期色
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
//			case 10: // primary font
//			case 11: // first alternate font
//			case 12: // second alternate font
		}
		if ( *pp != ';' ) break;
		pp++;
	}
	if ( *pp != 'm' ) return FALSE; // 書式エラー

	tci->text = pp + 1;
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
	tci->destfirst = tci->dest;
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
	HKEY HKroot, HKitem;
	DWORD cp = 0;
	BOOL loop = FALSE;
	TCHAR textbuf[CPBUFSIZE], *textdest;

	for ( cpt = cptext ; cpt->cp ; cpt++ ){
		size_t len;

		len = strlen(cpt->cptext);
		if ( !memicmp(*text, cpt->cptext, len) ){
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
	if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE, CodePageListPath, 0, KEY_READ, &HKroot) !=
			ERROR_SUCCESS ){
		return 0;
	}
	for (;;){
		DWORD size;

		if ( RegOpenKeyEx(HKroot, textbuf, 0, KEY_READ, &HKitem) != ERROR_SUCCESS ){
			break;
		}

		size = sizeof cp;
		if ( ERROR_SUCCESS == RegQueryValueEx(HKitem, CodePageName, NULL, NULL, (LPBYTE)&cp, &size) ){
			if ( cp != 0 ) *text += textdest - textbuf;
		}else{
			size = sizeof textbuf;
			if ( (loop == FALSE) && (ERROR_SUCCESS == RegQueryValueEx(HKitem, CodePageAliasName, NULL, NULL, (LPBYTE)&textbuf, &size)) ){
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


BOOL MimeDecode(TEXTCODEINFO *tci)
{
	BYTE *tp;
	int codetype;
	BYTE *textp, *dest;
	int l;

	// ?ISO-2022-JP?B?
	//  ^
	tp = tci->text + 2;

	codetype = GetCodePage(&tp);
	if ( (codetype != 0) && (*tp == '?') ){
		tp++;

		if ( (codetype == VTYPE_JIS) || (codetype == CP__JIS) || (codetype == 50225/*iso-2022-kr*/ ) ){
			codetype = VTYPE_SYSTEMCP; // 本来は VTYPE_JIS だが、コード切り替え前なので
		}

		if ( codetype >= VTYPE_MAX ){
			VO_CodePage = codetype;
			VO_CodePageValid = IsValidCodePage(VO_CodePage);
			codetype = VTYPE_OTHER;
		}
	}else{
		return FALSE;
	}
	if ( *(tp + 1) != (BYTE)'?' ) return FALSE;

	textp = tp + 2;
	dest = tci->oldtext.TextBuf;
	l = DESTBUFSIZE - 4;

	if ( upper((char)*tp) == 'B' ){	// BASE64
		int len, leni;

		do{
			len = B64DecodeBytes(&textp, dest);
			if ( len == 0 ) break;
			leni = len;
			do {
				dest++;
				leni--;
			} while( leni != 0 );
	//		if ( dest >= (BYTE *)destmax ) break;
		}while( len == 3 );
	}else if ( upper((char)*tp) == 'Q' ){	// quoted
		while( (textp < tci->srcmax) && (l > 0) ){
			if ( *textp == '?' ) break;
			if ( *textp != '=' ){
				*dest++ = *(BYTE *)textp++;
				l--;
				continue;
			}
			textp++;
			if ( (*textp == '\r') && (*(textp + 1) == '\n') ){
				textp += 2;
				continue;
			}
			if ( IsxdigitA(*textp) && IsxdigitA(*(textp + 1)) ){
				*dest++ = (BYTE)((GetHex(*textp) * 16) + GetHex(*(textp + 1)));
				textp += 2;
				l--;
				continue;
			}
			return FALSE;
		}
	}else{
		return FALSE;
	}
	if ( (*textp == '?') && (*(textp + 1) == '=' ) ){
		BYTE *textp2;

		textp += 2;
		textp2 = textp;
		if ( *textp2 == '\r' ){
			textp2 += (*(textp2 + 1) == '\n') ? 2 : 1;
		}else if ( *textp2 == '\n' ){
			textp2++;
		}
		if ( ((*textp2 == '\t') || (*textp2 == ' ')) && (*(textp2 + 1) == '=') && (*(textp2 + 2) == '?') ){
			textp = textp2 + 1;
		}
	}
	tci->oldtext.text = textp;
	tci->oldtext.srcmax = tci->srcmax;
	tci->oldtext.dcode = tci->dcode;
	tci->oldtext.type = DECODE_PROP;

	tci->text = tci->oldtext.TextBuf;
	tci->srcmax = dest;
	tci->dcode = codetype;
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
	if ( code == '+' ) return 62;
	if ( code == '/' ) return 63;
	if ( code == '=' ) return B64FLAG_D;
	return B64FLAG_ERR;
}

int B64DecodeBytes(BYTE **src, BYTE *dst)
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

void DecodeMailLine(TEXTCODEINFO *tci/*, int tbl_attrs*/)
{
	BYTE *b64src, *b64src_first;
	BYTE *textdest;
	int len, left, leftsize;

	if ( tci->oldtext.type == DECODE_BASE64 ){ // 追加
		// 残部を先頭に移動
		leftsize = tci->srcmax - tci->text;
		memmove(tci->oldtext.TextBuf, tci->text, leftsize);

		b64src = tci->oldtext.text;
		if ( *b64src == '\r' ){
			b64src += (*(b64src + 1) == '\n') ? 2 : 1;
		}else if ( *b64src == '\n' ) b64src++;
		b64src_first = b64src;

		textdest = tci->oldtext.TextBuf + leftsize;
		left = DESTBUFSIZE - 4 - leftsize;
	}else{
		b64src = tci->text;
		textdest = tci->oldtext.TextBuf;
		left = DESTBUFSIZE - 4;
	}
	do{
		len = B64DecodeBytes(&b64src, textdest);
		textdest += len;
		left -= len;
		if ( left < 0 ) break;
	}while( len == 3 );
	if ( (*b64src == '\r') || (*b64src == '\n') ){
		*textdest = '\0';
		if ( tci->oldtext.type == DECODE_BASE64 ){ // 追加
			tci->text = tci->oldtext.TextBuf;

			tci->oldtext.textfirst = b64src_first;
			tci->oldtext.offset = leftsize;
		}else{
			if ( textdest == tci->oldtext.TextBuf ){
				tci->attrs = tci->attrs & ~VTTF_BASEOFFMASK;
				return;
			}
			tci->oldtext.textfirst = tci->text;
			tci->oldtext.srcmax = tci->srcmax;
			tci->oldtext.dcode = tci->dcode;
			tci->oldtext.type = DECODE_BASE64;
			tci->oldtext.offset = 0;

			tci->text = tci->oldtext.TextBuf + (tci->attrs >> VTTF_BASEOFFSHIFT);
			tci->dcode = VTYPE_UTF8;
			tci->attrs = tci->attrs & ~VTTF_BASEOFFMASK;
		}
		tci->oldtext.text = b64src;
		tci->srcmax = textdest;
	}else{
		tci->attrs = tci->attrs & ~VTTF_BASEOFFMASK;

		if ( tci->oldtext.type == DECODE_BASE64 ){ // 追加
			if ( textdest > tci->text ){ // 残部が破損したので元に戻す
				memmove(tci->text, tci->oldtext.TextBuf, leftsize);
			}
		}
	}
}

//-----------------------------------------------------------------------------
void VO_error(ERRORCODE num)
{
	CloseViewObject();
	vo_.file.name[0] = '\0';
	if ( !convert ) PPErrorMsg(vo_.file.typeinfo, num);
}

WCHAR GetMimeChar(BYTE **src, int *offset)
{
	WCHAR c;
	const BYTE *srcptr;

	srcptr = *(const BYTE **)src + *offset;
	c = *srcptr;
	if ( c != '=' ){
		(*offset)++;
		return c;
	}
/*
	if ( (*(srcptr + 1) == '\r') &&
		 (*(srcptr + 2) == '\n') ){ // =\r\n 改行無効
		srcptr += 2;
		(*offset) += 2;
	}else if ( *(srcptr + 1) == '\n' ){ // =\n 改行無効
		srcptr += 1;
		(*offset) += 1;
	}
*/
	if ( IsxdigitA(*(srcptr + 1)) &&
		 IsxdigitA(*(srcptr + 2)) ){ // hexコード
		c = (WCHAR)((GetHex(*(srcptr + 1)) * 16) +
			GetHex(*(srcptr + 2)));
		(*offset) += 3;
		if ( *(srcptr + 3) == '=' ){
			if ( *(srcptr + 4) == '\n' ){ // =\n 改行無効
				(*offset) += 2;
			}else if ( (*(srcptr + 4) == '\r') &&
					 (*(srcptr + 5) == '\n') ){ // =\r\n 改行無効
				(*offset) += 3;
			}
		}
	}else{
		(*offset)++;
	}
	return c;
}

DWORD MimeUtf8toWchar(BYTE **src, int code)
{
	WCHAR c, d, e, f;
	int offset = 0;

	c = (WCHAR)code;				// 1byte 0x00-7f
	(*src)++;

	d = GetMimeChar(src, &offset);
	if ( d == '\0' ) return c;
	if ( (c & 0xe0) == 0xc0 ){	// 2bytes 0x80-7ff
		(*src) += offset;
		return (DWORD)((( c & 0x1f ) <<  6) | (d & 0x3f));
	}

	e = GetMimeChar(src, &offset);
	if ( e == '\0' ) return c;
	if ( (c & 0xf0) == 0xe0 ){	// 3bytes 0x800-ffff
		(*src) += offset;
		return (DWORD)((( c & 0xf ) << 12) | ((d & 0x3f ) << 6) | (e & 0x3f));
	}

	f = GetMimeChar(src, &offset);
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

DWORD Utf8toWchar(BYTE **src, int code)
{
	WCHAR c, d, e, f/*, g, h*/;

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

int UrlDecode(BYTE **text, BYTE *dest, size_t *size)
{
	BYTE urlsrc[DESTBUFSIZE], *destp, *destmax;
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

	codetype = GetTextCodeType((const BYTE *)urlsrc, *size);
	if ( codetype >= VTYPE_MAX ){
		if ( (codetype == VTYPE_UTF8) || (codetype == CP_UTF8) ){
			codetype = VTYPE_UTF8;
		}else{
			codetype = VTYPE_SYSTEMCP;
		}
	}
	memcpy(dest, urlsrc, *size + 2);
	return codetype;
}

int DecodeRTF(TEXTCODEINFO *tci)
{
	int c;
	BYTE *text = tci->text;

	c = *text;
	if ( c == '\\' ){
		tci->text++;
		SetVcode(tci, VCODE_ASCII);
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
		SetUchar(tci, (DWORD)(
				(GetHex(*(text + 1))<<12) | (GetHex(*(text + 2))<<8) |
				(GetHex(*(text + 3))<<4)  |  GetHex(*(text + 4)) ) );
		tci->text += 5;
		return '\0';
	}
											// \'??\'?? -----------------------
	if ( (c == '\'') &&
			IsxdigitA(*(text+1)) && IsxdigitA(*(text+2)) ){

		c = ((GetHex(*(text + 1)) << 4) + GetHex(*(text + 2)));
		SetVcode(tci, VCODE_ASCII);
		if ( IskanjiA(c) && tci->cnt < 2 ){
			tci->cnt = 0;
			tci->text--;
			return '\0';
		}
		*tci->dest++ = (char)c;
		tci->cnt--;
		tci->text += 3;

		if ( IskanjiA(c) &&
				(*text == '\\') && (*(text+1) == '\'') &&
				IsxdigitA(*(text+2)) && IsxdigitA(*(text+3)) ){
			*tci->dest++ = (char)((GetHex(*(text + 2)) << 4) + GetHex(*(text + 3)));
			tci->cnt --;
			tci->text += 4;
		}
	}else{
		BYTE *q, cmd[128];

		q = cmd;
		while( IsalnumA(*text) ) *q++ = *text++;
		*q = '\0';
		while ( *text == ' ' ) text++;
		tci->text = text;
		strlwr((char *)cmd);
		if ( strcmp((char *)cmd, "tab") == 0 ) return '\t';
		if ( strcmp((char *)cmd, "par") == 0 ){
			if ( tci->cnt != VOi->width ) return '\n';
		}
	}
	return '\0';
}

// utf7変換 ( +base64- )
BOOL DecodeUTF7(TEXTCODEINFO *tci)
{
	BYTE *src, *dst;
	int len, left;
	BYTE tmp[DESTBUFSIZE];

	src = tci->text + 1;
	if ( *src == '-' ){ // +- は - に変換
		SetVcode(tci, VCODE_ASCII);
		tci->text += 2;
		*tci->dest++ = '+';
		tci->cnt--;
		return TRUE;
	}
	// base64 を big endian UCS-2 に変換
	dst = (BYTE *)tmp;
	left = sizeof(tmp) - 4;
	do{
		len = B64DecodeBytes(&src, dst);
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
			SetUchar(tci, uchar);
		}
		tci->text = src + ((*src == '-') ? 1 : 0);
		return TRUE;
	}
	return FALSE;
}

BOOL DecodeEUCJP(TEXTCODEINFO *tci)
{
	int c = *tci->text, d;
	int c2;

	c2 = *(tci->text + 1);
	if ( c2 < 0x80 ){
		tci->text++;
		*tci->dest++ = (BYTE)c;
		tci->cnt--;
		return TRUE;
	}
	if ( c == 0x8e ){		//SS2, 1bytes KANA
		tci->text += 2;
		*tci->dest++ = (BYTE)c2;
		tci->cnt--;
		return TRUE;
	}
	if ( tci->cnt < 2 ){
		tci->cnt = 0;
		return FALSE;
	}
	tci->text += 2;

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

BOOL DecodeJIS(TEXTCODEINFO *tci)
{
	int c = *tci->text, d;

	if ( (c == 0x8b) || (c == 0x8c) ){	// 上/下付き添え字
		tci->text++;
		return TRUE;
	}
	if ( c == 0x9b ){	// 文字大きさ( CSI 0x9b, ESC [ 扱い)
		tci->text++;
		while( tci->text < tci->srcmax ){
			c = (unsigned char)*tci->text;
			if ( c < ' '  ) break;
			if ( c >= 0x80 ) break;
			tci->text++;
			if ( IsalphaA(c) ) break;
		}
		return TRUE;
	}
	if ( tci->cnt < 2 ){
		tci->cnt = 0;
		return FALSE;
	}
	tci->text++;
	d = c;
	c = *tci->text;
	if ( (d < 0x21) || (d >= 0x7f) || (c < 0x21) || (c >= 0x7f) ){
		*tci->dest++ = (unsigned char)d;
		*tci->dest++ = (unsigned char)' ';
		tci->cnt -= 2;
		return TRUE;
	}
	tci->text++;

	if ( VO_Tmime ){
		if ( (c == '=') && (*tci->text == '\r') && (*(tci->text + 1)=='\n') ){
			c = *(tci->text + 2);
			tci->text += 3;
		}
		if ( (c == '=') && IsxdigitA(*tci->text) && IsxdigitA(*(tci->text+1)) ){
			c = (GetHex(*tci->text) * 16) + GetHex(*(tci->text + 1));
			tci->text += 2;
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

BOOL DecodeHtmlSpecialChar(TEXTCODEINFO *tci)
{
	#define SpecialCharMaxSize 200
	BYTE *ptr;
	int len = SpecialCharMaxSize;

	ptr = tci->text;
	while ( len ){
		if ( *(++ptr) == ';' ) break;
		if ( (*ptr <= 0x20) || (*ptr >= 0x80) ){
			len = 0;
			break;
		}
		len--;
	}
	if ( len ){
		if ( *(tci->text + 1) == '#' ){ // 数値文字参照
			BYTE *q;
			int co;

			q = tci->text + 2;
			if ( *q != 'x' ){ // 10進
				co = GetIntNumberA((const char **)&q);
			}else{ // 16進
				q++;
				co = GetHexNumberA((const char **)&q);
			}
			if ( co && ( q == ptr ) ){
				SetUchar(tci, co);
				tci->text = ptr + 1;
				return TRUE;
			}
		}else{
			BYTE *chrptr;
			ACHARS *ac;

			chrptr = tci->text + 1;
			len = (SpecialCharMaxSize + 1) - len;
/*
			if ( OSver.dwMajorVersion >= 5 ){ // 文字実体参照(UNICODE
				UCHARS *uc;

				for ( uc = unichars ; uc->uchar ; uc++ ){
					if ( memcmp(chrptr, uc->name, len) ) continue;
					SetUchar(tci, uc->uchar);
					*textptr = p + 1;
					return TRUE;
				}
			}
*/
			for ( ac = anschars ; ac->achar ; ac++ ){
				if ( memcmp(chrptr, ac->name, len) ) continue;
				*tci->dest++ = (BYTE)ac->achar;
				tci->cnt--;
				if ( ac->achar > 0x100 ){
					*tci->dest++ = (BYTE)(ac->achar >> 8);
					tci->cnt--;
				}
				tci->text = ptr + 1;
				return TRUE;
			}
		}
	}
	return FALSE;
}

// テキスト中の文字指定による文字コード切り替え
void CheckCharset(BYTE *text, int *dcode)
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
			VO_CodePageValid = IsValidCodePage(VO_CodePage);
		}
	}else if ( (cp == CP__JIS) || (cp == 50225/*iso-2022-kr*/ ) ){
		*dcode = VTYPE_SYSTEMCP; // 本来は VTYPE_JIS だが、コード切り替え前なので
	}else if ( cp == CP__EUCJP ){
		*dcode = VTYPE_EUCJP;
	}else if ( (cp != 0) && (VO_CodePageChanged == FALSE) ){
		*dcode = VTYPE_OTHER;
		VO_CodePage = cp;
		VO_CodePageValid = IsValidCodePage(VO_CodePage);
	}
}

#if EUROCHAR
void DecodeEuro(TEXTCODEINFO *tci)
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
		*tci->dest++ = (BYTE)(tci->dcode <= VTYPE_ANSI ? tci->dcode : 2);
		tci->cnt -= 1;
	}else{
		SetVcode(tci, VCODE_ASCII);
		*tci->dest++ = '[';
		*tci->dest++ = 'E';
		*tci->dest++ = ']';
		tci->cnt -= 3;
	}
}
#endif
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

BYTE GetLineParamA(const char **str, TCHAR *param)
{
	BYTE code, bottom;
	const BYTE *src;
	BYTE *dst, *maxptr;
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
	AnsiToUnicode((char *)paramA, param, VFPS);
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

BYTE * USEFASTCALL ShrinkTagBlankLine(BYTE *tagend, int skipmode)
{
	BYTE *text = tagend + 1, c, *lasttext;

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

BOOL DecodeHtmlTag(TEXTCODEINFO *tci)
{
	BYTE *tagend;
	int len = X_tlen;

	tagend = tci->text;
										// 本当にTAGかを確認
	for ( ; len ; len-- ){
		if ( ++tagend >= tci->srcmax ){
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
			((*(tci->text + 1) == '!') &&
			 (*(tci->text + 2) == '-') &&
			 (*(tci->text + 3) == '-')) ?
			1 : 0
		];
		tci->dest += sizeof(COLORREF);
		SetVcode(tci, VCODE_ASCII);
		if ( tci->extlast == NULL ) setflag(tci->attrs, VTTF_TAG);
	}else{
		char *s, *dst, tagname[32];
		int offsw = 0, l;

		VO_CodePageChanged = TRUE;
		s = (char *)tci->text + 1;
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

		if ( strcmp(tagname, "a") == 0 ){
			if ( offsw ){
				CloseVcode(tci);
				tci->vcode = VCODE_END;
				*tci->dest++ = VCODE_COLOR;
				*tci->dest++ = (BYTE)tci->Fclr;
				*tci->dest++ = (BYTE)tci->Bclr;
				if ( tci->extlast == NULL ) resetflag(tci->attrs, VTTF_LINK);
			}else{
				SkipSepA(&s);
				if ( upper(*s) == 'H' ){
					CloseVcode(tci);
					tci->vcode = VCODE_END;
					*tci->dest++ = VCODE_LINK;
					if ( tci->extlast == NULL ) setflag(tci->attrs, VTTF_LINK);
				}
			}
			tci->text = ShrinkTagBlankLine(tagend, 0);
			return TRUE;
		}
		if ( !offsw ){
			if ( !VO_Tshow_script && (strcmp(tagname, "script") == 0) ){
				char *last = strstr((char *)tci->text,
					*(tci->text + 1) != 'S' ? "</script>" : "</SCRIPT>");
				if ( last != NULL ){
					tci->text = (BYTE *)last + 9;
					return TRUE;
				}
			}
			if ( !VO_Tshow_css && (strcmp(tagname, "style") == 0) ){
				char *last = strstr((char *)tci->text,
					*(tci->text + 1) != 'S' ? "</style>" : "</STYLE>");
				if ( last != NULL ){
					tci->text = (BYTE *)last + 8;
					return TRUE;
				}
			}
		}

		// <base なら vo_.file.source を取得する
		if ( !strcmp(tagname, "base") ){
			if ( !offsw ){
				SkipSepA(&s);
				if ( upper(*s) == 'H' ){
					const char *up;

					up = strchr(s, '=');
					if ( (up != NULL) && (up < (char *)tagend) ){
						up++;
						TGetLineParam(&up, vo_.file.source);
					}
				}
				offsw = 1;
			}
		}
		// </tag> を隠す
		if ( offsw ){
			if ( !memcmp(tci->text + 2, "w:p>", 4) ){ // Word docx の改段
				CloseVcode(tci);
				tci->vcode = VCODE_END;
				*tci->dest++ = VCODE_RETURN;
				*tci->dest++ = CTRLSIG_CRLF;
				tci->text = tagend + 1;
				tci->cnt = 0;
				return TRUE;
			}
			tci->text = ShrinkTagBlankLine(tagend, 2);
			return TRUE;
		}
		// table の各要素に区切り線を入れる
		if ( !strcmp(tagname, "th") || !strcmp(tagname, "td") ){
			if ( (tci->cnt != VOi->width) && !offsw ){
				*tci->dest++ = '|';
				*tci->dest++ = ' ';
				tci->cnt -= 2;
			}
			tci->text = ShrinkTagBlankLine(tagend, 0);
			return TRUE;
		}
		// 特定の tag を改行扱いにする
		if (	!strcmp(tagname, "br")	||
				!strcmp(tagname, "p")	||
				!memcmp(tci->text + 1, "w:cr/>", 6) || // Word docx の改行
				( (tci->cnt != VOi->width) &&
				  (	!strcmp(tagname, "ol")	||
					!strcmp(tagname, "li")	||
					!strcmp(tagname, "div")	||
					!strcmp(tagname, "dd")	||
					!strcmp(tagname, "dt")	||
					!strcmp(tagname, "hr")	||
					!strcmp(tagname, "blockquote")	||
					((*tagname == 'h') && IsdigitA(tagname[1]))	||
					!strcmp(tagname, "tr")
				  ) && CheckTopSpace(tci)
				) ){
			CloseVcode(tci);
			tci->vcode = VCODE_END;
			*tci->dest++ = VCODE_RETURN;
			*tci->dest++ = CTRLSIG_CRLF;
			tci->text = ShrinkTagBlankLine(tagend, 0);
			if ( tci->extlast == NULL ) setflag(tci->attrs, VTTF_TOP);
			tci->cnt = 0;
			return TRUE;
		}
		// １つのtagのみ有る行を無視する
		if ( (tci->cnt == VOi->width) && ((*(tagend + 1) == 0xa) || (*(tagend + 1) == 0xd)) ){
			tci->text = ShrinkTagBlankLine(tagend, 0);
			return TRUE;
		}
		tci->text = ShrinkTagBlankLine(tagend, 1);
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
	}else if ( c == '\r' ){ // 改行無効
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

BOOL DecodeControlCode(TEXTCODEINFO *tci, int c1st)
{
	int c = c1st;
	// JIS
	if ( (tci->dcode == VTYPE_JIS ) && (c == 0) &&
			!*(tci->text + 1) && !*(tci->text + 2) && !*(tci->text + 3) ){
		tci->text += 0x81;
		return TRUE;
	}
	tci->text++;
									// (BEL 0x07)
									// (BS  0x08)
									// (CAN  0x18)
									// (SUB  0x1A)
									// TAB (HT 0x09) --------------------------
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
			if ( tci->dcode == VTYPE_UNICODE){
				if ( *(WCHAR *)tci->text != '\t' ) break;
				tci->text += sizeof(WCHAR);
			}else if ( tci->dcode == VTYPE_UNICODEB ){
				if ( *(WCHAR *)tci->text != ('\t' * 0x100) ) break;
				tci->text += sizeof(WCHAR);
			}else{
				if ( *tci->text != '\t' ) break;
				tci->text++;
			}
		}
		if ( XV_unff ) tci->PxWidth = tci->cnt * fontX;
		return TRUE;
	}
									// JIS/KANA 切り換え (SO 0x0e, SI 0x0f) ---
	if ( (VO_Tmode != 1) && ((tci->dcode == VTYPE_JIS ) || VO_Tesc) ){
		if ( (c == 0xe) && (*tci->text > 6) && (*tci->text <= 0x5f) ){ // SO
			tci->dcode = VTYPE_KANA;	// JISX0201
			return TRUE;
		}
		if ( (c == 0xf) && (tci->dcode == VTYPE_KANA) ){	// SI
			tci->dcode = VOi->textC;
			return TRUE;
		}
	}
											// LF 0x0a ------------------------
	if ( c == '\n' ){
		CloseVcode(tci);
		tci->vcode = VCODE_END;
		if ( tci->extlast != NULL ) return FALSE;
		tci->dest[0] = VCODE_RETURN;
		tci->dest[1] = CTRLSIG_LF;
		tci->dest += 2;
		if ( tci->dcode != VTYPE_UNICODE ){
			if (	(tci->dcode == VTYPE_UNICODEB) &&
					(*tci->text == 0) && (*(tci->text + 1) == '\a') ){
				tci->text += 2;
			}
			if ( *tci->text == '\a' ) tci->text++;
		}else{
			if ( (*tci->text == '\a') && (*(tci->text + 1) == 0) ) tci->text += 2;
		}
		if ( tci->extlast == NULL ) setflag(tci->attrs, VTTF_TOP);
		return FALSE;
	}
											// CR 0x0d ------------------------
	if ( c == '\r' ){
		CloseVcode(tci);
		tci->vcode = VCODE_END;
		if ( tci->extlast != NULL ) return FALSE;
		tci->dest[0] = VCODE_RETURN;
		tci->dest[1] = CTRLSIG_CRLF;
		tci->dest += 2;
		if ( tci->dcode != VTYPE_UNICODE ){
			if (	(tci->dcode == VTYPE_UNICODEB) &&
					(*tci->text == 0) && (*(tci->text + 1) == '\n') ){
				tci->text += 2;
			}
			if ( *tci->text == '\n' ){
				tci->text++;
			}else{
				*(tci->dest - 1) = CTRLSIG_CR;
			}
		}else{
			if ( (*tci->text == '\n') && (*(tci->text + 1) == 0) ){
				tci->text += 2;
			}else{
				*(tci->dest - 1) = CTRLSIG_CR;
			}
		}
		if ( tci->extlast == NULL ) setflag(tci->attrs, VTTF_TOP);
		return FALSE;
	}
											// ^K 改段(VT 0x0b) ---------------
	if ( c == '\xb' ){
		CloseVcode(tci);
		tci->vcode = VCODE_END;
		*tci->dest++ = VCODE_PARA;
		if ( tci->extlast == NULL ) setflag(tci->attrs, VTTF_TOP);
		return FALSE;
	}
											// ^L 改ページ(FF 0x0c) -----------
	if ( c == '\xc' ){
		CloseVcode(tci);
		tci->vcode = VCODE_END;
		*tci->dest++ = VCODE_PAGE;
		if ( tci->extlast == NULL ) setflag(tci->attrs, VTTF_TOP);
		return FALSE;
	}
											// ESC 0x1b (ECMA-48 / ANSI X3.64)-
	if ( (c == 0x1b) && VO_Tesc && (tci->dcode != VTYPE_UNICODE) ){
		switch(*tci->text){
//			case '7':	// DECSC
//			case '8':	// DECRC
//			case 'D':	// IND
//			case 'E':	// NEL
//			case 'H':	// HTS
//			case 'M':	// RI
//			case 'N':	// SS2
//			case 'O':	// SS3
//			case 'P':	// DCS
//			case 'X':	// SOS
//			case 'Z':	// DECID
//			case 'c':	// RIS
//			case 'n':	// LS2
//			case 'o':	// LS3
//			case '|':	// LS3R
//			case '}':	// LS2R
//			case '~':	// LS1R
//			case '#':	//
//				case '8':	// DECALN
//			case '%':	// 文字コード指定
//				switch( *(tci->text + 1) ){
//					case '@':	// ISO646
//					case 'G':	// UTF-8 new
//					case '8':	// UTF-8 old

			case '(': // G0 設定
				switch( *(tci->text + 1) ){
//					case '0':		// ^[(0 VT100 グラフィック
					case 'A':		// ^[(A ISO646 ENG
					case 'B':		// ^[(B ASCII
//					case 'D':		// ^[(D JIS X 0212-1990
					case 'H':		// ^[(H ISO646 SW
					case 'J':		// ^[(J JIS C 6220-1976 JISx0201 ROMA
						tci->text += 2;
						tci->dcode = VOi->textC;
						return TRUE;
//					case 'K':		// Linux user defined
					case 'I':		// ^[(I JISx0201 KANA
						tci->text += 2;
						tci->dcode = VTYPE_KANA;
						return TRUE;
//					case 'O':		// ^[(O JIS X 0213:2000 1
//					case 'P':		// ^[(P JIS X 0213:2000 2
//					case 'Q':		// ^[(Q JIS X 0213:2004 1
//					case 'U':		// hardware font
				}
				break;
//			case ')': // G1 設定
//			case '>': // DECPNM
//			case '=': // DECPAM
			case '$':
				switch( *(tci->text + 1) ){
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
						tci->text += 2;
						tci->dcode = VTYPE_JIS;
						return TRUE;
				}
				break;
//									// ^[.A ISO 8859-1
//									// ^[.F ISO 8859-7
			case '[': // CSI 画面関係 ESC シーケンス ------------
				if ( (tci->extlast == NULL) && IsTrue(EscCSI(tci, tci->dcode)) ){
					return TRUE;
				}
				break;
//			case ']': // OSC
//				case 'P': // ESC ] P n rr gg bb	palette
//				case 'R': // ESC ] R

//						// ESC ] 0 ; text ESC \		Icon name & Window title
//						// ESC ] 1 ; text ESC \		Icon name
//						// ESC ] 2 ; text ESC \		Window title
//						// ESC ] 4 ; n ; text ESC \		highlight?
//						// ESC ] 10 ; text ESC \		highlight?
//						// ESC ] 46 ; filename ESC \	set log name
//						// ESC ] 50 ; fontname ESC \	set font name
		}
	}
								// ^? に変換 --------------------------
	if ( (c == 0) || (tci->dcode != VTYPE_IBM) ){
							// IBM 以外は 20h 未満を変換
		if ( (c == 0) && (VO_Tmode == 0) ) VO_Tmode = 1;
		if ( tci->cnt < 2 ){
			tci->text--;
			if ((tci->dcode == VTYPE_UNICODE) || (tci->dcode == VTYPE_UNICODEB)) tci->text--;
			tci->cnt = 0;
			return FALSE;
		}
/*
		if ( (tci->vcode != VCODE_ASCII) && (tci->vcode != VCODE_UNICODE)){
			SetVcode(&tci, VCODE_ASCII);
		}
		*tci->dest++ = '^';
		if (tci->vcode == VCODE_UNICODE) *tci->dest++ = '\0';
		*tci->dest++ = (unsigned char)(c + '@');
		if (tci->vcode == VCODE_UNICODE) *tci->dest++ = '\0';
*/
		SetVcode(tci, VCODE_CONTROL);
		*tci->dest++ = '^';
		*tci->dest++ = (unsigned char)(c + '@');
		tci->cnt -= 2;
		return TRUE;
	}else{
		if ((tci->dcode == VTYPE_UNICODE) || (tci->dcode == VTYPE_UNICODEB)) tci->text--;
		SetVcode(tci, VCODE_ASCII);
		*tci->dest++ = (BYTE)c;
		tci->cnt--;
		return TRUE;
	}
}

//-----------------------------------------------------------------------------
char *stristrVA(const char *target, size_t targetlen, const char *findstr)
{
	size_t flen;
	const char *p, *maxptr;

	flen = strlen(findstr);
	maxptr = target + targetlen - flen;

	for ( p = target ; p <= maxptr ; p += ChrlenA(*p) ){
		if ( !strnicmp(p, findstr, flen) ){
			return (char *)p;
		}
	}
	return NULL;
}

WCHAR *stristrW(const WCHAR *target, const WCHAR *findstr)
{
	size_t len, flen;
	const WCHAR *p, *maxptr;

	flen = strlenW(findstr);
	len = strlenW(target);
	maxptr = target + len - flen;

	for ( p = target ; p <= maxptr ; p++ ){
		if ( !strnicmpW(p, findstr, flen) ){
			return (WCHAR *)p;
		}
	}
	return NULL;
}

#define SEARCHHILIGHT_COLORID (CV__deftext + (CV__highlight << 8))
#define SEARCHHILIGHT_COLORREF (SEARCHHILIGHT_COLORID | 0xff000000)
#define COLORREF_ABNORMAL 0x1000000

#define USER_HILIGHT_A_ADDSIZE 11
void WriteUserHilightA(BYTE *dest, size_t size, COLORREF color, int extend)
{
	// text xxx1 add1[7] firstp[size] add2[4] last
	memmove(dest + VCODE_FCOLOR_SIZE + 2, dest, size); // firstp
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

#define SEARCH_HILIGHT_A_ADDSIZE 9
void WriteSearchHilightA(BYTE *dest, size_t size)
{
	// text xxx1 add1[5] firstp[size] add2[4] last
	memmove(dest + VCODE_COLOR_SIZE + 2, dest, size);
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

void CheckSearchAscii(TEXTCODEINFO *tci, const char *search, COLORREF color, int extend)
{
	BYTE *firstp, *text = tci->destfirst, *last = tci->dest;
	size_t searchlen, addsize = 0;

	for ( ;; ){
		size_t textlen;

		textlen = strlen((char *)text);
		firstp = (BYTE *)stristrVA((char *)text, textlen, search);
		if ( firstp == NULL ){ // 色指定があればスキップ
			BYTE *tmpp, type;

			tmpp = text + textlen + 1;
			if ( tmpp >= last ) break;
			type = *tmpp;
			if ( (type == VCODE_FCOLOR) || (type == VCODE_BCOLOR) ){
				text = tmpp + VCODE_FCOLOR_SIZE + sizeof(char);
				continue;
			}
			if ( type == VCODE_COLOR ){
				text = tmpp + VCODE_COLOR_SIZE + sizeof(char);
				if ( text >= last ) break;
				if ( *(WORD *)(tmpp + 1) == SEARCHHILIGHT_COLORID ){
					text += strlen((char *)text);
				}
				continue;
			}
			break; // proof
		}
		// ハイライト対象発見
		if ( tci->extlast != NULL ){
			if ( firstp >= tci->extlast ) break; // 行末以前
			// 行末と被る
			searchlen = strlen(search);
			if ( (firstp + searchlen) >= tci->extlast ){ // 対象の途中で改行
				searchlen = tci->extlast - firstp;
				last = tci->extlast;
			}
		}else{
			searchlen = strlen(search);
		}
		if ( (firstp + searchlen + 1) >= last ){ // 末尾の特別処理
			if ( color >= COLORREF_ABNORMAL ){ // SEARCHHILIGHT_COLORREF
				// text xxx1 add1[5] firstp[searchlen] add2[4-1] last
				if ( (last + SEARCH_HILIGHT_A_ADDSIZE) >= tci->destmax ) break;
				WriteSearchHilightA(firstp, searchlen);
				addsize += SEARCH_HILIGHT_A_ADDSIZE - sizeof(char);
				if ( tci->extlast ) tci->extlast += SEARCH_HILIGHT_A_ADDSIZE;
			}else{
				// text xxx1 add1[7] firstp[searchlen] add2[4-1] last
				if ( (last + USER_HILIGHT_A_ADDSIZE) >= tci->destmax ) break;
				if ( extend & HILIGHTKEYWORD_T ){ // 先頭側へ拡張
					searchlen += firstp - text;
					firstp = text;
				}
				WriteUserHilightA(firstp, searchlen, color, extend);
				addsize += USER_HILIGHT_A_ADDSIZE - sizeof(char);
				if ( tci->extlast ) tci->extlast += USER_HILIGHT_A_ADDSIZE;
			}
			break;
		}
		if ( color >= COLORREF_ABNORMAL ){ // SEARCHHILIGHT_COLORREF
			// text xxx1 add1[5] firstp[searchlen] add2[4] next[1] xxx2 last
			if ( (last + SEARCH_HILIGHT_A_ADDSIZE + 1) >= tci->destmax ) break;
			memmove(firstp + searchlen + SEARCH_HILIGHT_A_ADDSIZE + 1, firstp + searchlen, last - (firstp + searchlen)); // xxx2
			WriteSearchHilightA(firstp, searchlen);
			firstp += searchlen + SEARCH_HILIGHT_A_ADDSIZE;
			*firstp++ = VCODE_ASCII;
			last += SEARCH_HILIGHT_A_ADDSIZE + 1;
			addsize += SEARCH_HILIGHT_A_ADDSIZE + 1;
			if ( tci->extlast ) tci->extlast += SEARCH_HILIGHT_A_ADDSIZE + 1;
		}else{
			// text xxxxx add1[7] firstp[size] add2[4] next[1] xxxx last
			if ( (last + USER_HILIGHT_A_ADDSIZE + 1) >= tci->destmax ) break;
			if ( extend & HILIGHTKEYWORD_T ){ // 先頭側に拡張
				searchlen += firstp - text;
				firstp = text;
			}
			if ( extend & HILIGHTKEYWORD_B ){ // 末尾側に拡張
				searchlen += strlen((char *)firstp + searchlen);
				if ( tci->extlast != NULL ){
					if ( (firstp + searchlen) >= tci->extlast ){ // 対象の途中で改行
						searchlen = tci->extlast - firstp;
					}
				}
			}
			memmove(firstp + searchlen + USER_HILIGHT_A_ADDSIZE + 1, firstp + searchlen, last - (firstp + searchlen));
			WriteUserHilightA(firstp, searchlen, color, extend);
			firstp += searchlen + USER_HILIGHT_A_ADDSIZE;
			*firstp++ = VCODE_ASCII;
			last += USER_HILIGHT_A_ADDSIZE + 1;
			addsize += USER_HILIGHT_A_ADDSIZE + 1;
			if ( tci->extlast ) tci->extlast += USER_HILIGHT_A_ADDSIZE + 1;
		}
		text = firstp;
	}
	tci->dest += addsize;
}

#define USER_HILIGHT_W_ADDSIZE 13
void WriteUserHilightW(BYTE *dest, size_t size, COLORREF color, int extend)
{
	// text xxx1 add1[8] firstp[size] add2[5] last
	memmove(dest + VCODE_FCOLOR_SIZE + 3, dest, size);
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

#define SEARCH_HILIGHT_W_ADDSIZE 11
void WriteSearchHilightW(BYTE *dest, size_t size)
{
	// text xxx1 add1[6] firstp[size] add2[5] last
	memmove(dest + VCODE_COLOR_SIZE + 3, dest, size);
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
	BYTE *firstp, *text = tci->destfirst, *last = tci->dest;
	size_t size, addsize = 0;

	for ( ;; ){
		firstp = (BYTE *)stristrW((WCHAR *)text, search);
		if ( firstp == NULL ){ // 色指定があればスキップ
			BYTE *tmpp, type;

			tmpp = text + (strlenW((WCHAR *)text) + 1) * sizeof(WCHAR);
			if ( tmpp >= last ) break;
			type = *tmpp;
			if ( (type == VCODE_FCOLOR) || (type == VCODE_BCOLOR) ){
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
		// ハイライト対象発見
		if ( tci->extlast != NULL ){
			if ( firstp >= tci->extlast ) break;
			size = strlenW(search) * sizeof(WCHAR);
			if ( (firstp + size) >= tci->extlast ){
				size = tci->extlast - firstp;
				last = tci->extlast;
			}
		}else{
			size = strlenW(search) * sizeof(WCHAR);
		}

		if ( (firstp + size + 2) == last ){
			if ( color >= COLORREF_ABNORMAL ){ // SEARCHHILIGHT_COLORREF
				// text xxx1 add1[6] firstp[size] add2[5-2] last
				if ( (last + SEARCH_HILIGHT_W_ADDSIZE) >= tci->destmax ) break;
				WriteSearchHilightW(firstp, size);
				addsize += SEARCH_HILIGHT_W_ADDSIZE - sizeof(WCHAR);
				if ( tci->extlast ) tci->extlast += SEARCH_HILIGHT_W_ADDSIZE;
			}else{
				// text xxxxx [8] firstp---size [5-2] last
				if ( (last + USER_HILIGHT_W_ADDSIZE) >= tci->destmax ) break;
				if ( extend & HILIGHTKEYWORD_T ){
					size += firstp - text;
					firstp = text;
				}
				WriteUserHilightW(firstp, size, color, extend);
				addsize += USER_HILIGHT_W_ADDSIZE - sizeof(WCHAR);
				if ( tci->extlast ) tci->extlast += USER_HILIGHT_W_ADDSIZE;
			}
			break;
		}
		if ( color >= COLORREF_ABNORMAL ){ // SEARCHHILIGHT_COLORREF
			// text xxx1 add1[6] firstp[size] add2[5] next[1] xxx2 last
			if ( (last + SEARCH_HILIGHT_W_ADDSIZE + 1) >= tci->destmax ) break;
			memmove(firstp + size + SEARCH_HILIGHT_W_ADDSIZE + 1, firstp + size, last - (firstp + size));
			WriteSearchHilightW(firstp, size);
			firstp += size + SEARCH_HILIGHT_W_ADDSIZE;
			*firstp++ = VCODE_UNICODE;
			last += SEARCH_HILIGHT_W_ADDSIZE + 1;
			addsize += SEARCH_HILIGHT_W_ADDSIZE + 1;
			if ( tci->extlast ) tci->extlast += SEARCH_HILIGHT_W_ADDSIZE + 1;
		}else{
			// text xxxxx [8] firstp---size add2[5] next[1] xxx2 last
			if ( (last + USER_HILIGHT_W_ADDSIZE + 1) >= tci->destmax ) break;
			if ( extend & HILIGHTKEYWORD_T ){
				size += firstp - text;
				firstp = text;
			}
			if ( extend & HILIGHTKEYWORD_B ){
				size += strlenW((WCHAR *)(firstp + size)) * sizeof(WCHAR);
			}
			memmove(firstp + size + USER_HILIGHT_W_ADDSIZE + 1, firstp + size, last - (firstp + size));
			WriteUserHilightW(firstp, size, color, extend);
			firstp += size + USER_HILIGHT_W_ADDSIZE;
			*firstp++ = VCODE_UNICODE;
			last += USER_HILIGHT_W_ADDSIZE + 1;
			addsize += USER_HILIGHT_W_ADDSIZE + 1;
			if ( tci->extlast ) tci->extlast += USER_HILIGHT_W_ADDSIZE + 1;
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
		if ( !(Ctype & (T_IS_DIG | T_IS_HEX)) ) break;	// 0-9, A-F, a-f ではない
		if ( Ctype & T_IS_LOW ) c = (BYTE)(c - 0x20);	// 小文字を大文字に
		if ( !(Ctype & T_IS_DIG) ) c = (BYTE)(c - 7);	// A-F の処理
		c = (BYTE)(c - '0');
		n = (n << 4) + c;
		(*ptr)++;
	}
	return n;
}

int MakeIndexTable(int mode, int param)
{
	VMEM *vo;
	BYTE *of, *vmax;			// 参照中のイメージと最大値
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
					GlobalAlloc(GMEM_MOVEABLE, sizeof(VT_TABLE) * ISTEP))){
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
					sizeof(VT_TABLE) * next, GMEM_MOVEABLE);
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
		of = VOi->MakeText(&mti, &((VT_TABLE *)vo->ptr)[line]);
		if ( of < VOi->img ){
			VOi->reading = FALSE;
			break;
		}

		if ( (of < vmax) || (ReadingStream == READ_NONE) ) line++;
		if ( !--count ) break;
	}

	if ( (line >= 2) && !((mode == MIT_NEXT) && (VOi->cline == 0)) ){
		BYTE *linep;

		linep = ((VT_TABLE *)vo->ptr)[2 - 1].ptr;
		if ( linep != NULL ){
			if ( !memcmp(linep , "GET ", 4) ){
				linep += 4;
				tstrcpy(vo_.file.source, T("http://"));
				TGetLineParam((const char **)&linep, vo_.file.source + 7);
			}
			if ( !memcmp(linep , "CONNECT ", 8) ){
				TCHAR *cp;

				linep += 8;
				tstrcpy(vo_.file.source, T("https://"));
				TGetLineParam((const char **)&linep, vo_.file.source + 8);
				cp = tstrrchr(vo_.file.source + 8, ':');
				if ( cp != NULL ) *cp = '\0';
			}
		}
	}

	if ( of >= vmax ){
		VOi->reading = FALSE;
		mtinfo.PresetPos = 0;
	}else{
		if ( mode == MIT_REMAKE ){ // 再計算
			VOi->reading = TRUE;
			SetTimer(vinfo.info.hWnd, TIMERID_READLINE, TIMER_READLINE, BackReaderProc);
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

	if ( tci->CalcTextPtr < tci->destfirst ) tci->CalcTextPtr = tci->destfirst;
	GetTextExtentPoint32A(tci->hDC, (char *)tci->CalcTextPtr, (tci->dest - tci->destfirst - 1), &range);
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
	if ( tci->CalcTextPtr < tci->destfirst ) tci->CalcTextPtr = tci->destfirst;
	GetTextExtentPoint32W(tci->hDC, (WCHAR *)tci->CalcTextPtr, (tci->dest - tci->destfirst - 1) / sizeof(WCHAR), &range);
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

				if ( tci->extlast != NULL ){
					*(BYTE *)(tci->extlast) = '\0';
					tci->dest = tci->extlast + sizeof(BYTE);
				}
			}
			return;
		}

		case VCODE_UNICODE:{
			*(WCHAR *)tci->dest = '\0';
			tci->dest += sizeof(WCHAR);
			if ( XV_unff ) RecalcWidthW(tci);

			if ( tci->paintmode != FALSE ){
				if ( VOsel.highlight ){
					CheckSearchUNICODE(tci, VOsel.VSstringW, SEARCHHILIGHT_COLORREF, 0);
				}
				for ( hks = X_hkey ; hks ; hks = hks->next ){
					CheckSearchUNICODE(tci, hks->wide, hks->color, hks->extend);
				}
				if ( tci->extlast != NULL ){
					*(WCHAR *)(tci->extlast) = '\0';
					tci->dest = tci->extlast + sizeof(WCHAR);
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
	tci->destfirst = tci->dest;
	tci->vcode = setvcode;
}

#ifdef UNICODE
void SetUchar(TEXTCODEINFO *tci, DWORD code)
{
	const WORD *destptr;
	WORD types[4];

	SetVcode(tci, VCODE_UNICODE);
	destptr = (const WORD *)tci->dest;
	if ( code < 0x10000 ){
		*tci->dest++ = (BYTE)code;
		*tci->dest++ = (BYTE)(code >> 8);
		GetStringTypeExW(0, CT_CTYPE3, destptr, 2, types);
	}else{	// サロゲートペア
		DWORD hicode;

		hicode = (code >> 10) + (0xd800 - (0x10000 >> 10));
		*tci->dest++ = (BYTE)hicode;
		*tci->dest++ = (BYTE)(hicode >> 8);
		code = (code & 0x3ff) | 0xdc00;
		*tci->dest++ = (BYTE)code;
		*tci->dest++ = (BYTE)(code >> 8);
		GetStringTypeExW(0, CT_CTYPE3, destptr, 2, types);
	}
	tci->cnt -= (types[0] & C3_HALFWIDTH) ? 1 : 2;
	if ( (tci->cnt == 0) && (tci->extcnt != 0) ){
		tci->cnt = tci->extcnt;
		tci->extlast = tci->dest;
		tci->extcnt = 0;
	}
}
#else
void SetUchar(TEXTCODEINFO *tci, DWORD code)
{
	const WORD *destptr;

	SetVcode(tci, VCODE_UNICODE);
	destptr = (const WORD *)tci->dest;
	if ( code < 0x10000 ){
		*tci->dest++ = (BYTE)code;
		*tci->dest++ = (BYTE)(code >> 8);
		// Win9x は、GetStringTypeExW が対応していない
		tci->cnt -= WideCharToMultiByte(CP_ACP, 0, destptr, 1, NULL, 0, "・", NULL);
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
	if ( (tci->cnt == 0) && (tci->extcnt != 0) ){
		tci->cnt = tci->extcnt;
		tci->extlast = tci->dest;
		tci->extcnt = 0;
	}
}
#endif
/*-----------------------------------------------------------------------------
	表示用文字列を作成する
	tbl:	各種情報（次の行の情報を保存するので、少なくとも２以上の配列）
	max:	イメージの末尾のポインタ
-----------------------------------------------------------------------------*/
BYTE *MakeDispText(MAKETEXTINFO *mti, VT_TABLE *tbl)
{
	TEXTCODEINFO tci;
	BOOL urldecode = FALSE;
	int ct;
	HFONT hOldFont C4701CHECK;

	tci.dest = tci.destfirst = tci.CalcTextPtr = mti->destbuf;
	tci.extlast = NULL;
	tci.cnt = VOi->width;
	tci.extcnt = 0;
	tci.destmax = tci.dest + TEXTBUFSIZE - 16;
	tci.vcode = VCODE_END;
	tci.paintmode = mti->paintmode;

	if ( (tci.paintmode != FALSE) && (VOsel.highlight || X_hkey) ){
		tci.extcnt = 10;
	}

	if ( IsTrue(XV_unff) ){
		tci.hDC = GetDC(vinfo.info.hWnd);
		hOldFont = SelectObject(tci.hDC, hUnfixedFont);
		tci.PxWidth = tci.cnt * fontX;
	}

	tci.text = tbl->ptr;
	tci.Fclr = tbl->Fclr;
	tci.Bclr = tbl->Bclr;

	tci.dcode = tbl->type;
	tci.attrs = tbl->attrs & ~VTTF_TOP;

	tci.srcmax = mti->srcmax;
	tci.oldtext.type = DECODE_NONE;

	if ( tci.text == NULL ){
		tci.cnt = 0;
	}else{
	if ( VO_Tmime ) DecodeMailLine(&tci/*, tbl->attrs*/);
	while( (tci.cnt > 0) && (tci.dest < tci.destmax) ){
		int c;

		if ( tci.text >= tci.srcmax ){
			if ( tci.oldtext.type != DECODE_NONE ){
				tci.text = tci.oldtext.text;
				tci.srcmax = tci.oldtext.srcmax;
				tci.dcode = tci.oldtext.dcode;
				tci.oldtext.type = DECODE_NONE;
				continue;
			}
			break;
		}
		c = *tci.text;

		if ( VO_Tmime && (c == '=') ){ // =xx Quoted-printable を置換 -------
			c = DecodeQuotedPrintable(&tci.text);
			if ( c == '\0' ) continue;
		}
										// URL の %xx を置換 ------------------
		if ( tci.dcode != VTYPE_JIS ){
			if ( (c == ':') && (*(tci.text + 1) == '/') ){
				urldecode = TRUE;
			}else if ( (c == '%') && IsTrue(urldecode) && VO_Ttag &&
						IsxdigitA(*(tci.text+1)) && IsxdigitA(*(tci.text+2)) ){
				if ( tci.oldtext.type == DECODE_NONE ){
					BYTE *np;
					int newdcode;
					size_t size;

					np = tci.text;
					newdcode = UrlDecode(&tci.text, tci.oldtext.TextBuf, &size);
					if ( np < tci.text ){
						tci.oldtext.text = tci.text;
						tci.oldtext.srcmax = tci.srcmax;
						tci.oldtext.dcode = tci.dcode;
						tci.oldtext.type = DECODE_URL;
						tci.text = tci.oldtext.TextBuf;
						tci.srcmax = tci.text + size;
						tci.dcode = newdcode;
						c = *tci.text;
					}
				}
			}
		}
		if ( (c == 'c') && (VO_Tesc || VO_Tmime) && (tci.dcode != VTYPE_UNICODE) ){
			if ( !memcmp(tci.text + 1, "harset=", 7) ){
				CheckCharset(tci.text, &tci.dcode);
			}
		}
										// RTF 簡易解析 -----------------------
		if ( tci.dcode == VTYPE_RTF ){
			if ( c == '\\' ){
				tci.text++;
				c = DecodeRTF(&tci);
				if ( c == '\0' ) continue;
			}else if ( (c < ' ') || (c == ';') || (c == '{') ){
				tci.text++;
				continue;
			}
		}else
											//JIS/KANA ------------------------
		if ( tci.dcode == VTYPE_KANA ){
							// 7bit コードなのに 8bit目が使用→誤判別の可能性
			if ( (c & 0x80) && (VOi->textC != VTYPE_KANA) ){
				if ( VO_Tmode == 0 ){
					VO_Tmode = 1;
					tci.dcode = VOi->textC;
				}
			}
			if ( (c >= 0x20) && ( c <= 0x5f) ){
				SetVcode(&tci, VCODE_ASCII);
				*tci.dest++ = (BYTE)(c + 0x80);
				tci.text++;
				tci.cnt--;
				continue;
			}
		}else
											//UNICODE -------------------------
		if ( tci.dcode == VTYPE_UNICODE ){
			tci.text++;
			c += *tci.text << 8;
			if ( c >= 0x20 ){
				tci.text++;
				if ( tci.cnt < 2 ){
					if ( tci.extcnt ){
						tci.cnt = tci.extcnt;
						tci.extlast = tci.dest;
						tci.extcnt = 0;
						tci.text -= 2;
						continue;
					}else{
						tci.cnt = 0;
						tci.text -= 2;
						break;
					}
				}
				if ( (c == L'　') && XV_bctl[2] ){
					CloseVcode(&tci);
					tci.vcode = VCODE_END;
					*tci.dest++ = VCODE_WSPACE;
					tci.cnt -= 2;
					if ( tci.cnt == 0 ){
						if ( tci.extcnt ){
							tci.cnt = tci.extcnt;
							tci.extlast = tci.dest;
							tci.extcnt = 0;
						}
					}
					continue;
				}
				if ( VO_Ttag ){
					if ( c == '<' ){
						WORD *tagend;
						int len = X_tlen;

						tagend = (WCHAR *)tci.text;
												// 本当にTAGかを確認
						for ( ; len ; len-- ){
							if ( ++tagend >= (WORD *)tci.srcmax ){
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
								((*((WORD *)tci.text + 1) == '!') &&
								 (*((WORD *)tci.text + 2) == '-') &&
								 (*((WORD *)tci.text + 3) == '-')) ?
								1 : 0
							];
							tci.dest += sizeof(COLORREF);
							if ( tci.extlast == NULL ){
								setflag(tci.attrs, VTTF_TAG);
							}
						}
					}
					if ( c == '>' ){
						SetUchar(&tci, c);
						CloseVcode(&tci);
						tci.vcode = VCODE_END;
						*tci.dest++ = VCODE_COLOR;
						*tci.dest++ = CV__deftext;
						*tci.dest++ = CV__defback;
						if ( tci.extlast == NULL ){
							resetflag(tci.attrs, VTTF_TAG);
						}
						continue;
					}
				}

				SetUchar(&tci, c);
				continue;
			}
		}else

		if ( tci.dcode == VTYPE_UNICODEB ){
			tci.text++;
			c = *tci.text + (c << 8);
			if ( c >= 0x20 ){
				tci.text++;
				if ( tci.cnt < 2 ){
					tci.cnt = 0;
					tci.text -= 2;
					break;
				}
				if ( (c == L'　') && XV_bctl[2] ){
					CloseVcode(&tci);
					tci.vcode = VCODE_END;
					*tci.dest++ = VCODE_WSPACE;
					tci.cnt -= 2;
					continue;
				}
				SetUchar(&tci, c);
				continue;
			}
		}else
		if ( tci.dcode == VTYPE_UTF8 ){
			if ( c & 0x80 ){
				if ( tci.cnt < 2 ){
					if ( tci.extcnt ){
						tci.cnt = tci.extcnt;
						tci.extlast = tci.dest;
						tci.extcnt = 0;
						continue;
					}else{
						tci.cnt = 0;
						break;
					}
				}
				if ( !VO_Tmime ){
					c = Utf8toWchar(&tci.text, c);
				}else{
					if ( (tci.oldtext.type == DECODE_BASE64) &&
						 ((tci.srcmax - tci.text) < 4) ){
						DecodeMailLine(&tci/*, 0*/);
					}
					c = MimeUtf8toWchar(&tci.text, c);
				}
				SetUchar(&tci, c);
				continue;
			}
		}else
											// S-JIS/B ------------------------
		if ( tci.dcode == VTYPE_SJISB ){
			if ( c > 0x20 ){
				int d;

				d = *(tci.text + 1);

				SetVcode(&tci, VCODE_ASCII);
				if ( IskanjiA(d) ){
					if ( tci.cnt < 2 ){
						tci.cnt = 0;
						break;
					}
					tci.text += 2;
					*tci.dest++ = (BYTE)d;
					tci.cnt -= 2;
				}else{
					if ( (d == 0x20) && (c & B7) ) tci.text++;
					tci.text++;
					tci.cnt--;
				}
				*tci.dest++ = (BYTE)c;
				continue;
			}else{
				if ( (c > '\0') && (c < 0x20) && (*(tci.text + 1) == '\0') ) tci.text++;
			}
#if EUROCHAR
		}else
//-----------------------------------------------------------------------------
		if ( (c == 0x80) && (tci.dcode == VTYPE_ANSI) && (OSver.dwMajorVersion < 6) ){	// ユーロ単価
			DecodeEuro(&tci);
			tci.text++;
			continue;
#endif
		}
		ct = T_CHRTYPE[(unsigned char)c];
		if ( ct & T_IS_CTL ){				// コントロールコード -------------
			if ( IsTrue(DecodeControlCode(&tci, c)) ) continue;
			break;
		}
		SetVcode(&tci, VCODE_ASCII);

		if ( tci.dcode != VTYPE_JIS ){
										// <tag> ------------------------------
			if ( VO_Ttag && (tci.extlast == NULL) ){
				if ( c == '<' ){
					if ( IsTrue(DecodeHtmlTag(&tci)) ) continue;
				}
				if ( (c == '>') && (VO_Ttag > 1) ){
					*tci.dest++ = '>';
					CloseVcode(&tci);
					tci.vcode = VCODE_END;
					*tci.dest++ = VCODE_COLOR;
					*tci.dest++ = CV__deftext;
					*tci.dest++ = CV__defback;
					tci.text++;
					resetflag(tci.attrs, VTTF_TAG);
					continue;
				}
										// html spcial charchter --------------
				if ( c == '&' ){
					if ( IsTrue(DecodeHtmlSpecialChar(&tci)) ) continue;
				}
			}
										// MIME decode =?xxx?B?xxx= -----------
			if ( (c == '=') &&
				 VO_Tmime &&
				 (*(tci.text + 1) == '?') &&
				 (tci.oldtext.type == DECODE_NONE) ){
				if ( MimeDecode(&tci) ){
					continue;
				}
			}
										// MIME decode(utf-7) -----------------
			if ( (tci.dcode == VTYPE_UTF7) && (c == '+') ){
				if ( IsTrue(DecodeUTF7(&tci)) ) continue;
			}
		}
										// S-JIS ------------------------------
		if ( ((tci.dcode == VTYPE_SYSTEMCP) ||
			  (tci.dcode == VTYPE_SJISNEC) ||
			  (tci.dcode == VTYPE_RTF) ||
			  (VO_CodePage == CP__SJIS)) &&
			 (ct & T_IS_KNJ) ){
			BYTE c2;

			if ( tci.cnt < 2 ){
				if ( XV_unff ){
					if ( tci.vcode == VCODE_ASCII ){
						RecalcWidthA(&tci);
					}else if ( tci.vcode == VCODE_UNICODE ){
						RecalcWidthW(&tci);
					}
					if ( tci.cnt < 2 ){
						if ( tci.extcnt ){
							tci.cnt = tci.extcnt;
							tci.extlast = tci.dest;
							tci.extcnt = 0;
							continue;
						}else{
							tci.cnt = 0;
							break;
						}
					}
				}else{
					if ( tci.extcnt ){
						tci.cnt = tci.extcnt;
						tci.extlast = tci.dest;
						tci.extcnt = 0;
						continue;
					}else{
						tci.cnt = 0;
						break;
					}
				}
			}
			*tci.dest++ = (BYTE)c;
			tci.text++;
			c2 = *tci.text;
			if ( c2 != '\0' ){
				if ( VO_Tmime ){
					if ( c2 == '=' ){
						if ( (*(tci.text + 1) == '\r') &&
							 (*(tci.text + 2) == '\n') ){
							tci.text += 3;
							c2 = *tci.text;
						}else if ( *(tci.text + 1) == '\n' ){
							tci.text += 2;
							c2 = *tci.text;
						}
					}
					if ( (c2 == '=') && IsxdigitA(*(tci.text + 1)) && IsxdigitA(*(tci.text + 2)) ){
						c2 = (BYTE)((GetHex(*(tci.text + 1)) * 16) + GetHex(*(tci.text +2)));
						tci.text += 2;
					}
				}
				tci.text++;
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

			if ( (tci.dcode == VTYPE_SJISNEC) && ((c == 0x85) || (c == 0x86)) && (*(tci.dest - 1) >= 0x40) ){
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
			if ( tci.cnt == 0 ){
				if ( tci.extcnt ){
					tci.cnt = tci.extcnt;
					tci.extlast = tci.dest;
					tci.extcnt = 0;
				}
			}
			continue;
		}else
										// EUC JP -----------------------------
		if ( (tci.dcode == VTYPE_EUCJP) && (c >= 0x80) ){
			if ( IsTrue(DecodeEUCJP(&tci)) ) continue;
			break;
		}else
										// JIS --------------------------------
		if ( tci.dcode == VTYPE_JIS ){
			if ( IsTrue(DecodeJIS(&tci)) ) continue;
			break;
		}
		*tci.dest++ = (BYTE)c;
		tci.text++;
		tci.cnt--;
		if ( tci.cnt == 0 ){
			if ( XV_unff != 0 ){
				if ( tci.vcode == VCODE_ASCII ){
					RecalcWidthA(&tci);
				}else if ( tci.vcode == VCODE_UNICODE ){
					RecalcWidthW(&tci);
				}
			}
			if ( tci.extcnt && (tci.cnt == 0)){
				tci.cnt = tci.extcnt;
				tci.extlast = tci.dest;
				tci.extcnt = 0;
				continue;
			}
		}
	}
		if ( (VOi->defwidth == WIDTH_NOWARP) && (ScrollWidth < (TEXTBUFSIZE - tci.cnt)) ) {
			ScrollWidth = TEXTBUFSIZE - tci.cnt;
		}
	}
	CloseVcode(&tci);
	*tci.dest++ = VCODE_END;
	*tci.dest = VCODE_END;

	if ( tci.oldtext.type != DECODE_NONE ){
		// BASE64 の未使用分を戻す

		if ( (tci.oldtext.type == DECODE_BASE64) && (tci.text < tci.srcmax) ){
			int usesize = tci.text - (tci.oldtext.TextBuf + tci.oldtext.offset);

			if ( (usesize >= 3) ||
				 ((usesize > 0) )){//&& ((tbl->attrs >> VTTF_BASEOFFSHIFT) != usesize)) ){
				tci.attrs |= (usesize % 3) << VTTF_BASEOFFSHIFT;
				tci.oldtext.text = tci.oldtext.textfirst + (usesize / 3) * 4;
			}
		}

		tci.text = tci.oldtext.text;
		tci.dcode = tci.oldtext.dcode;
	}
	if ( mti->writetbl ){
		tbl++;
		tbl->ptr = tci.text;
		tbl->Fclr = (BYTE)tci.Fclr;
		tbl->Bclr = (BYTE)tci.Bclr;
		tbl->type = (BYTE)tci.dcode;
		tbl->attrs = (BYTE)tci.attrs;
		tbl->line = (tbl - 1)->line + ((tci.attrs & VTTF_TOP) ? 1 : 0);
	}
	if ( IsTrue(XV_unff) ){
		SelectObject(tci.hDC, hOldFont); // C4701ok
		ReleaseDC(vinfo.info.hWnd, tci.hDC);
	}
	return tci.text;
}
