/*-----------------------------------------------------------------------------
	Paper Plane bUI								～ コマンドライン編集処理 ～
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <wincon.h>
#include <string.h>
#include "PPX.H"
#include "VFS.H"
#include "PPB.H"
#include "TCONSOLE.H"
#pragma hdrstop

typedef struct {
	WORD type;		// 検索するヒストリの種類
	int index;		// 参照している古さ。-1:編集中
	int count;		// 検索中のパラメータ 0:検索していない
		#define HISCOUNT_MAX 9999
		#define HIST_CMD_ALL 0
		#define HIST_CMD_FIND 1
		#define HIST_PARAM_ALL 2
		#define HIST_PARAM_FIND 3
	int mode;		// (前回の)検索方法 0:全体 1:コマンド前方一致
						//                2:パラメータ 3:パラメータ前方一致
	TCHAR *findstr;	// 検索文字列の開始位置
	DWORD findsize;	// 検索文字列の長さ
} HISTVAR;

int ShowOffset;		// 相対表示開始位置
int EdX = 0;		// 編集位置

int SelStart = 0;	// 選択開始位置
int SelEnd = 0;		// 選択終了位置

typedef enum {
	CMODE_EDIT = 0,CMODE_SCROLL,CMODE_SELECT
} CMODEINFO;

CMODEINFO cmode;	// CMODE_
struct tagSCROLLMODEINFO {
	int cx,cy;	// スクロールモード時のカーソル座標
	int sx,sy;	// スクロールモード時の選択座標
	int BackupY;	// スクロールモード開始時の編集行
	BOOL LineMode;
} sinfo;
#define TCI_TI_DRAW 1
#define TCI_TI_SCROLL 2
int eflag;		// 正:TCI_TI_DRAW:再描画	負:TCI_
int istrt = 0;	// 0:書き込んだ文字列の右にカーソル 1:カーソル移動なし
				// 2:右端の一つ左（括り付き用）
#define INS_INS		50
#define INS_OVER	100
int insmode;	// カーソルの高さ+挿入モード INS_INS / INS_OVER
int mouseSX;	// マウスで範囲選択を開始したX座標

size_t maxsize; // 最大入力可能文字数
TCHAR *EditText;		// 表示内容
WORD *RevBuf;	// スクロークモードの反転状態を保存するバッファ。
				// NULL なら反転状態でない。
TCHAR backuptext[CMDLINESIZE];	// バックアップ
struct tagIncSearchInfo {
	TCHAR text[64]; // インクリメンタルサーチテキスト
	int len;	// searchtext の長さ
	COORD pos;	// 最後に検索した位置
} incsearch;

ESTRUCT ComplED;
HISTVAR hvar;
TCHAR *baseptr;
WORD histype;

DWORD OldScrollTick = 0;
int WheelOffset = 0;
DWORD OldMouseButton = 0;

struct {
	WORD ed,select;
} CB_edit[2] = {
	{T_CYA | TR_DGR | T_DL,TR_CYA | T_DGR | T_DL},
	{T_GRE | TR_DCY | T_DL,TR_GRE | T_DCY | T_DL}
};
#define FORMLINEWIDTH 60

TMENU editmenu[] = {
	{1,MES_EDBL},
	{K_c | 'C',MES_EDCP},
	{K_c | 'X',MES_EDCU},
	{K_c | 'V',MES_EDPA},
	{K_c | 'A',MES_EDAL},
	{0,MES_EDCA},
	{0,NULL}
};

TMENU selectmenu[] = {
	{K_c | 'C',MES_EDCP},
	{K_c | 'A',MES_EDAL},
	{0,MES_EDCA},
	{0,NULL}
};

int MouseCommand(MOUSE_EVENT_RECORD *me);
void CommonCommand(int key);
void EditModeCommand(int key);
void ScrollModeCommand(int key);
void MoveCursorS(int offX,int offY,int key);
int PPbContextMenu(void);

#ifdef UNICODE
#define TChrlen(c) CCharWide(c)
#else
#define TChrlen(c) Chrlen(c)
#endif

typedef struct {
	ULONG cbSize;
	DWORD nFont;
	COORD dwFontSize;
	UINT FontFamily;
	UINT FontWeight;
	WCHAR FaceName[LF_FACESIZE];
} xCONSOLE_FONT_INFOEX;

DefineWinAPI(BOOL,GetCurrentConsoleFontEx,(HANDLE hConsoleOutput,BOOL bMaximumWindow,xCONSOLE_FONT_INFOEX *lpConsoleCurrentFontEx)) = NULL;
DefineWinAPI(BOOL,SetCurrentConsoleFontEx,(HANDLE hConsoleOutput,BOOL bMaximumWindow,xCONSOLE_FONT_INFOEX *lpConsoleCurrentFontEx));

const TCHAR Kernel32DLL[] = T("kernel32.dll");
SHORT DefaultFontY = 0; // 元のフォントサイズ(未設定:0)

void ConChangeFontSize(SHORT delta)
{
	xCONSOLE_FONT_INFOEX cfi;

	if ( DGetCurrentConsoleFontEx == NULL ){
		HMODULE hKernel32;

		hKernel32 = GetModuleHandle(Kernel32DLL);
		GETDLLPROC(hKernel32,GetCurrentConsoleFontEx);
		GETDLLPROC(hKernel32,SetCurrentConsoleFontEx);
		if ( DSetCurrentConsoleFontEx == NULL ) return;
	}
	cfi.cbSize = sizeof(cfi);
	DGetCurrentConsoleFontEx(hStdout,FALSE,&cfi);
	if ( DefaultFontY == 0 ) DefaultFontY = cfi.dwFontSize.Y;
	if ( delta != -2 ){
		if ( (cfi.dwFontSize.Y + delta) < 5 ) return;
		cfi.dwFontSize.Y += delta;
	}else{
		cfi.dwFontSize.Y = DefaultFontY;
	}
	DSetCurrentConsoleFontEx(hStdout,FALSE,&cfi);
}

void ScrollArea(int offY)
{
	SMALL_RECT box;
	CONSOLE_SCREEN_BUFFER_INFO cinfo;

	GetConsoleWindowInfo(hStdout,&cinfo);

	box = cinfo.srWindow;
	if ( (box.Top + offY) < 0 ){
		offY = -box.Top;
	}
	if ( (box.Bottom + offY) >= cinfo.dwSize.Y ){
		offY = cinfo.dwSize.Y - box.Bottom - 1;
	}
	box.Top += (short)offY;
	box.Bottom += (short)offY;
	SetConsoleWindowInfo(hStdout,TRUE,&box);
	eflag = TCI_TI_SCROLL;
}

int ForwardEditWord(int x)
{
	if ( (size_t)x < maxsize ){
		while (*(EditText + x) && ((UTCHAR)*(EditText + x) > ' ') ){
			if ( (size_t)x >= maxsize ) break;
			x++;
		}
	}
	return x;
}

int BackEditWord(int x)
{
	while ( x && (*(EditText + (x - 1)) == ' ') ) x--;
	while ( x && ((UTCHAR)*(EditText + (x - 1)) > ' ') ) x--;
	return x;
}

TCHAR *ClipText_FixPtr(TCHAR *src,int x)
{
	int charlen;
	while ( x > 0 ){
		charlen = TChrlen(*src);
		if ( charlen == 0 ) break;
		x -= charlen;
		#ifdef UNICODE
			src++;
		#else
			src += charlen;
		#endif
	}
	return src;
}

void ClipText(void)
{
	COORD pos;
	SIZE range;
	int i;
	TCHAR *src,*srcO,*dst,*spc;
	DWORD readsize,col;
	HGLOBAL hG;

	if ( (RevBuf == NULL) || !IsTrue(OpenClipboard(NULL)) ) return;

	if ( sinfo.sx > sinfo.cx ){
		pos.X = (SHORT)sinfo.cx;
		range.cx = sinfo.sx - sinfo.cx + 1;
	}else{
		pos.X = (SHORT)sinfo.sx;
		range.cx = sinfo.cx - sinfo.sx + 1;
	}

	if ( sinfo.sy > sinfo.cy){
		pos.Y = (SHORT)sinfo.cy;
		range.cy = sinfo.sy - sinfo.cy + 1;
	}else{
		pos.Y = (SHORT)sinfo.sy;
		range.cy = sinfo.cy - sinfo.sy + 1;
	}
									// 読み込み
	if ( sinfo.LineMode ){
		DWORD allocsize;
		int x;

		if ( range.cy <= 1 ){ // 1行
			allocsize = range.cx + 2;
		}else{ // 複数行
			if ( sinfo.sy > sinfo.cy ){
				pos.X = (SHORT)sinfo.cx;
				range.cx = sinfo.sx;
			}else{
				pos.X = (SHORT)sinfo.sx;
				range.cx = sinfo.cx;
			}
			allocsize = screen.dwSize.X - pos.X + 2; // 先頭行
			 // 中間行
			if ( range.cy >= 3 ) allocsize += (range.cy - 2) * (screen.dwSize.X + 2);
			allocsize += range.cx + 2; // 末尾行
		}
		srcO = HeapAlloc(GetProcessHeap(),0,TSTROFF(screen.dwSize.X));
		hG = GlobalAlloc(GMEM_MOVEABLE,TSTROFF(allocsize + 2));
		if ( hG == NULL ) return;
		dst = GlobalLock(hG);
		x = pos.X;
		pos.X = 0;
		for ( i = 0 ; i < range.cy ; i++ ){
			#ifndef WINEGCC
			ReadConsoleOutputCharacter(hStdout,srcO,screen.dwSize.X,pos,&readsize);
			#else
				tCsrPos(0,pos.Y);
				innstr(srcO,screen.dwSize.X);
			#endif
			pos.Y++;
			src = srcO;
			if ( range.cy <= 1 ){ // 1行
				src = ClipText_FixPtr(src,x);
				readsize = ClipText_FixPtr(src,range.cx) - src;
			}else{
				if ( i == 0 ){ // 1行
					src = ClipText_FixPtr(src,x);
					readsize = ClipText_FixPtr(src,screen.dwSize.X - x) - src;
				}else if ( i == (range.cy - 1) ){ // 末尾行
					readsize = ClipText_FixPtr(src,range.cx) - src;
				} // 中間行
			}
			spc = dst;
			for ( col = 0 ; col < readsize ; src++,col++ ){
				UTCHAR c;

				c = (UTCHAR)*src;
				if ( c < (UTCHAR)' ' ) continue;	// 0
				*dst++ = c;
				if ( c != ' ' ) spc = dst;
			}
			// 最終行でない &&(末尾に空白がない || 最終桁まで選択無し)
			if ( ((i + 1) < range.cy) && (dst != spc) ){
				*spc++ = '\r';
				*spc++ = '\n';
				dst = spc;
			}
		}
	}else{
		srcO = HeapAlloc(GetProcessHeap(),0,TSTROFF(range.cx));
		hG = GlobalAlloc(GMEM_MOVEABLE,TSTROFF((range.cx + 2) * range.cy + 2));
		if ( hG == NULL ) return;
		dst = GlobalLock(hG);
		for ( i = 0 ; i < range.cy ; i++ ){
			#ifndef WINEGCC
			ReadConsoleOutputCharacter(hStdout,srcO,range.cx,pos,&readsize);
			#else
				tCsrPos(pos.X,pos.Y);
				innstr(srcO,range.cx);
			#endif
			pos.Y++;
			src = srcO;
			spc = dst;
			for ( col = 0 ; col < readsize ; src++,col++ ){
				UTCHAR c;

				c = (UTCHAR)*src;
				if ( c < (UTCHAR)' ' ) continue;	// 0
				*dst++ = c;
				if ( c != ' ' ) spc = dst;
			}
			// 最終行でない &&(末尾に空白がない || 最終桁まで選択無し)
			if ( ((i + 1) < range.cy) && ((dst != spc) ||
					((pos.X + range.cx) < screen.dwSize.X)) ){
				*spc++ = '\r';
				*spc++ = '\n';
			}
			dst = spc;
		}
	}
	*dst = '\0';
	HeapFree(GetProcessHeap(),0,srcO);
	GlobalUnlock(hG);
	EmptyClipboard();
	SetClipboardData(CF_TTEXT,hG);
	CloseClipboard();
}


int CallKeyHook(PPXAPPINFO *info,WORD key)
{
	PPXMKEYHOOKSTRUCT keyhookinfo;
	PPXMODULEPARAM pmp;

	keyhookinfo.key = key;
	pmp.keyhook = &keyhookinfo;
	return CallModule(info,PPXMEVENT_KEYHOOK,pmp,KeyHookEntry);
}

void MoveCursor(int newX,int key)
{
	if ( newX < 0 ) newX = 0;

	if ( key & K_s ){	// 選択
		if ( SelStart == SelEnd ) SelEnd = SelStart = EdX; // 選択開始
		if ( EdX == SelStart ){ // 新しい範囲を決定
			SelStart = newX;
		}else{
			SelEnd = newX;
		}
		if ( SelStart == SelEnd ){
			SelEnd = SelStart = 0; // 選択解除
		}else if ( SelStart > SelEnd ){
			int tempX;

			tempX = SelStart;
			SelStart = SelEnd;
			SelEnd = tempX;
		}
		eflag = TCI_TI_DRAW;
	}else{				// 非選択
		if ( SelStart != SelEnd ){
			SelStart = SelEnd = 0;
			eflag = TCI_TI_DRAW;
		}
	}
	EdX = newX;
}

// ----------------------------------------------------------------------------
void Replace(const TCHAR *istr,BOOL all)
{
	size_t len,sellen;

	len = tstrlen(istr);
	sellen = 0;
	if ( IsTrue(all) ){								// 置き換え
		if ( maxsize < len + 1 ) return;
		sellen = tstrlen(EditText);
		EdX = 0;
	}else if ( SelStart != SelEnd ){					// 選択
		if ( maxsize < (SelStart +len+ tstrlen(EditText + SelEnd) + 1)) return;
		sellen = SelEnd - SelStart;
		EdX = SelStart;
	}else if( insmode == INS_INS ){						// 挿入
		if ( maxsize < (tstrlen(EditText) + len + 1) ) return;
	}else{												// 上書
		do{
			#ifdef UNICODE
				if ( *(EditText + EdX + sellen) ) sellen++;
			#else
			{
				int clen;

				clen = Chrlen(*(EditText + EdX + sellen));
				if ( clen == 0 ) break;
				sellen += clen;
			}
			#endif
		}while( sellen < len );
		if ( maxsize < (EdX + len + tstrlen(EditText + EdX + sellen)+ 1)) return;
	}
	memmove(EditText+EdX+len,EditText+EdX+sellen,TSTRSIZE(EditText+EdX+sellen));
	memcpy (EditText+EdX    ,istr              ,TSTROFF(len));

	SelStart = SelEnd = 0;
	if ( istrt == 0 ) EdX += len;
	if ( istrt == 2 ) EdX += len - 1;
	eflag = TCI_TI_DRAW;
}

int GetMouseX(int ox,int mousex,TCHAR *dl)
{
	int dispx,x;

	dispx = 0;
	for ( x = ox ; *(dl + x) != '\0' ; ){
		#ifdef UNICODE
			dispx += CCharWide(*(dl + x));
			if ( dispx > mousex ) break;
			x++;
		#else
			dispx += Chrlen(*(dl + x));
			if ( dispx > mousex ) break;
			x += Chrlen(*(dl + x));
		#endif
	}
	return x;
}
// ----------------------------------------------------------------------------
void ClearFormLine(void)
{
	TCHAR buf[FORMLINEWIDTH + 1],*p;
	COORD pos;
	int i;
	DWORD dtmp;

	p = buf;
	for ( i = 0 ; i < FORMLINEWIDTH ; i++ ) *p++ = ' ';
	*p = '\0';

	pos.X = 0;
	pos.Y = (SHORT)(screen.dwCursorPosition.Y + 1);
	SetConsoleCursorPosition(hStdout,pos);
	WriteConsole(hStdout,buf,FORMLINEWIDTH,&dtmp,NULL);
}

// ----------------------------------------------------------------------------
void USEFASTCALL ReverseLine(WORD *ptr,int len)
{
	int i;

	for ( i = 0 ; i < len ; i++ ){
		*ptr = (WORD)((*ptr & 0xff00) | (~(*ptr) & 0xff) );
		ptr++;
	}
}

void ReverseRange(BOOL revon)
{
	COORD pos;
	SIZE range;
	int i;
	WORD *p;
	DWORD s;

	if ( !revon && (RevBuf == NULL) ) return;
	if ( sinfo.sx > sinfo.cx ){
		pos.X = (SHORT)sinfo.cx;
		range.cx = sinfo.sx - sinfo.cx + 1;
	}else{
		pos.X = (SHORT)sinfo.sx;
		range.cx = sinfo.cx - sinfo.sx + 1;
	}

	if ( sinfo.sy > sinfo.cy ){
		pos.Y = (SHORT)sinfo.cy;
		range.cy = sinfo.sy - sinfo.cy + 1;
	}else{
		pos.Y = (SHORT)sinfo.sy;
		range.cy = sinfo.cy - sinfo.sy + 1;
	}
	if ( revon ){
		if ( sinfo.LineMode ){
			SHORT a;
												// 読み込み
			p = RevBuf = HeapAlloc( GetProcessHeap(),0,screen.dwSize.X * range.cy * sizeof(WORD));
			if ( p == NULL ) return;
			a = pos.X;
			pos.X = 0;
			for ( i = 0 ; i < range.cy ; i++ ){
				ReadConsoleOutputAttribute(hStdout,p,screen.dwSize.X,pos,&s);
				p += s;
				pos.Y++;
			}
			pos.X = a;
		}else{
												// 読み込み
			p = RevBuf = HeapAlloc( GetProcessHeap(),0,range.cx * range.cy * sizeof(WORD));
			if ( p == NULL ) return;
			for ( i = 0 ; i < range.cy ; i++ ){
				ReadConsoleOutputAttribute(hStdout,p,range.cx,pos,&s);
				p += s;
				pos.Y++;
			}
		}
	}
												// 反転
	if ( sinfo.LineMode ){
		if ( range.cy <= 1 ){ // 1行
			ReverseLine(RevBuf + pos.X,range.cx);
		}else{ // 複数行
			if ( sinfo.sy > sinfo.cy ){
				pos.X = (SHORT)sinfo.cx;
				range.cx = sinfo.sx;
			}else{
				pos.X = (SHORT)sinfo.sx;
				range.cx = sinfo.cx;
			}

			ReverseLine(RevBuf + pos.X,screen.dwSize.X - pos.X); // 先頭行
			if ( range.cy >= 3 ){ // 中間行
				for ( i = 1 ; i < (range.cy - 1) ; i++ ){
					ReverseLine(RevBuf + screen.dwSize.X * i,screen.dwSize.X);
				}
			}
			ReverseLine(RevBuf + screen.dwSize.X * (range.cy - 1),range.cx); // 末尾行
		}
		range.cx = screen.dwSize.X;
		pos.X = 0;
	}else{
		ReverseLine(RevBuf,range.cx * range.cy);
	}
												// 表示
	if ( revon ) pos.Y -= (SHORT)range.cy;

	p = RevBuf;
	for ( i = 0 ; i < range.cy ; i++ ){
		#ifndef WINEGCC
			WriteConsoleOutputAttribute(hStdout,p,range.cx,pos,&s);
		#else
			tFillAtr(pos.X,pos.Y,pos.X + range.cx,pos.Y,0xf0);
		#endif
		p += s;
		pos.Y++;
	}

	if ( !revon ){								// 解放
		HeapFree(GetProcessHeap(),0,RevBuf);
		RevBuf = NULL;
	}
}

// ----------------------------------------------------------------------------
void ReverseText(int len)
{
	WORD temp[TSIZEOF(incsearch.text)];
	DWORD tempsize;

	ReadConsoleOutputAttribute(hStdout,temp,len,incsearch.pos,&tempsize);
												// 反転
	ReverseLine(temp,len);
	WriteConsoleOutputAttribute(hStdout,temp,len,incsearch.pos,&tempsize);
}

BOOL ScrIncSearch(int next)
{
	COORD pos;
	TCHAR temptext[TSIZEOF(incsearch.text)];
	int oldlen;

	pos = incsearch.pos;
	oldlen = incsearch.len;
	if ( next ){
		pos.X++;
		if ( next == 2 ) pos.X = pos.Y = 0;
	}else{
		oldlen--;
	}

	for ( ; pos.Y < screen.dwSize.Y ; pos.Y++ ){
		for ( ; pos.X < screen.dwSize.X ; pos.X++ ){
			DWORD temp;

			if ( FALSE == ReadConsoleOutputCharacter(
					hStdout,temptext,incsearch.len,pos,&temp) ){
				break;
			}
			temptext[incsearch.len] = '\0';
			if ( tstricmp(incsearch.text,temptext) == 0 ){
				if ( oldlen ) ReverseText(oldlen);
				incsearch.pos = pos;
				MoveCursorS(pos.X - sinfo.cx,pos.Y - sinfo.cy,0);
				ReverseText(incsearch.len);
				return TRUE;
			}
		}
		pos.X = 0;
	}
	return FALSE;
}

/*-----------------------------
１文字前の全角／半角の判断
-----------------------------*/
#ifdef UNICODE
	#define bchrlen(str,off) (off ? 1 : 0)
#else
#if NODLL
extern int bchrlen(TCHAR *str,int off);
#else
int bchrlen(TCHAR *str,int off)
{
	int size;
	TCHAR *max_str;

	max_str = str + off;

	while( str < max_str ){
		str += size = Chrlen( *str );
	}
	return size;
}
#endif
#endif
//----------------------------------------------------------------------------
void USEFASTCALL HilightLine(WORD atr)
{
	tFillAtr(	0					,screen.dwCursorPosition.Y,
				screen.dwSize.X - 1	,screen.dwCursorPosition.Y,atr);
}

#ifdef UNICODE
int GetScreenX(int x)
{
	int i,nx = 0;

	for ( i = 0 ; i < x ; i++ ){
		WCHAR c;

		c = *(EditText + ShowOffset + i);
		if ( c == '\0' ) break;
		nx += CCharWide(c);
	}
	return nx;
}
#else
#define GetScreenX(x) x
#endif
//----------------------------------------------------------------------------
void DisplayLine(TCHAR *buf)
{
	#ifdef UNICODE
	WCHAR c;
	#endif
	TCHAR dispbuf[CMDLINESIZE],*p;
	int s = screen.dwSize.X,i,j,linecolor;
	COORD pos;
	DWORD tmp;
	pos.X = 0;
	pos.Y = screen.dwCursorPosition.Y;
	p = dispbuf;
	buf += ShowOffset;
					// 表示用データを作成する
	for ( ; s ; s-- ){
				// 未使用領域
		if ( *buf == '\0' ){
			*p++ = ' ';
			continue;
		}
		#ifdef UNICODE
		c = *buf++;
		if ( CCharWide(c) > 1 ){
			if ( s > 1 ){
				s--;
			}else{
				c = ' ';
			}
		}
		*p++ = c;
		#else
		if (IskanjiA(*p++ = *buf++)){
			if ( s > 1 ){
				*p++ = *buf++;
				s--;
			}else{
				*(p - 1) = ' ';
			}
		}
		#endif
	}
	*p = '\0';
#ifndef WINEGCC
	WriteConsoleOutputCharacter(hStdout,dispbuf,screen.dwSize.X,pos,&tmp);
#else
	tputposstr(0,screen.dwCursorPosition.Y,dispbuf);
#endif

								// 範囲選択処理 -------------------------------
	linecolor = (cmode == CMODE_EDIT) ? 0 : 1;
	s = ShowOffset + GetScreenX(screen.dwSize.X);
	if ( (ShowOffset >= SelEnd) || (s <= SelStart)){	// 選択範囲が画面上にない
		HilightLine(CB_edit[linecolor].ed);
	}else{
		i = 0;
		j = s - ShowOffset;

		if ( ShowOffset < SelStart ){	// 範囲左端が画面内→左端まで選択解除
			i = GetScreenX(SelStart - ShowOffset);
			tFillAtr(	0	 ,screen.dwCursorPosition.Y,
						i - 1,screen.dwCursorPosition.Y,CB_edit[linecolor].ed);
		}
		if ( s > SelEnd ){	// 範囲右端が画面内→右端以降を選択解除
			tFillAtr(	GetScreenX(SelEnd - ShowOffset)	,screen.dwCursorPosition.Y,
						j - 1,screen.dwCursorPosition.Y,CB_edit[linecolor].ed);
			j = GetScreenX(SelEnd - ShowOffset);
		}
						// 範囲を選択
		tFillAtr(	i    ,screen.dwCursorPosition.Y,
					j - 1,screen.dwCursorPosition.Y,CB_edit[linecolor].select);
	}
}
// ----------------------------------------------------------------------------
#define BackupLine(dl,bak) tstrcpy(bak,dl)
// ----------------------------------------------------------------------------
void SetScrollMode(void)
{
	if ( cmode == CMODE_EDIT ){
		cmode = CMODE_SCROLL;
		incsearch.len = 0;
		sinfo.cx = EdX - ShowOffset;
		sinfo.cy = sinfo.BackupY = screen.dwCursorPosition.Y;
		eflag = TCI_TI_DRAW;
	}
}
void SetSelectMode(MOUSE_EVENT_RECORD *me)
{
	sinfo.sx = sinfo.cx = me->dwMousePosition.X;
	sinfo.sy = sinfo.cy = me->dwMousePosition.Y;
	sinfo.LineMode = TRUE;
	tCsrPos(sinfo.cx,sinfo.cy);
	cmode = CMODE_SELECT;
}

// ----------------------------------------------------------------------------
void TPushEditStack(TCHAR mode,TCHAR *str,int b,int t)
{
	TCHAR buf[CMDLINESIZE];

	memcpy(buf,str + b,TSTROFF(t - b));
	buf[t - b] = '\0';
	PushTextStack(mode,buf);
}
// ----------------------------------------------------------------------------
void InitHistorySearch(HISTVAR *hivar,TCHAR *dl)
{
	hivar->index = -1;
	hivar->count = 0;
	hivar->mode = -1;
	hivar->findstr = dl;
	hivar->findsize = 0;
}
// ----------------------------------------------------------------------------
// ヒストリの検索方法を決定する
void GetHistorySearchMode(HISTVAR *hivar,TCHAR *dl)
{
	TCHAR *p,*q,*cmd,*s;

	if ( EdX == 0 ){							// カーソルが先頭…全体
		InitHistorySearch(hivar,dl);
		hivar->mode = HIST_CMD_ALL;
		return;
	}
												// カーソルが末尾…前回と同じ
	if ( (hivar->mode != -1) && (*(dl + EdX) == '\0') ) return;

										// カーソルがどこを示しているか調べる
	p = dl;
	q = dl + EdX;
	s = NULL;
	while ( (p < q) && (*p == ' ') ) p++;	// 先頭の空白は無視
	cmd = p;
	while ( p < q ){
		if (*p == ' ') s = p + 1;
		p++;
	}
	InitHistorySearch(hivar,dl);
	if ( s == NULL ){		// 空白がなかった→コマンド検索
		hivar->mode = (cmd == p) ? HIST_CMD_ALL : HIST_CMD_FIND;	// 文字列なし→全体検索
		hivar->findstr = cmd;				// 文字列有り→コマンド前方一致検索
		hivar->findsize = p - cmd;
	}else{					// 空白有り→パラメータ検索
		hivar->mode = (s == p) ? HIST_PARAM_ALL : HIST_PARAM_FIND;		// 文字列なし→パラ検索
		hivar->findstr = s;					// 文字列有り→パラ前方一致検索
		hivar->findsize = p - s;
		hivar->count = 1;
	}
	return;
}
// ----------------------------------------------------------------------------
const TCHAR *NextWord(const TCHAR *p)
{
	while ( *p && ((UTCHAR)*p <= (UTCHAR)' ') ) p++; // 単語前の空白をスキップ
	while ( *p && ((UTCHAR)*p >  (UTCHAR)' ') ) p++; // 単語自体をスキップ
	while ( *p && ((UTCHAR)*p <= (UTCHAR)' ') ) p++; // 単語の後の空白をスキップ
	return p;
}
// ----------------------------------------------------------------------------
const TCHAR *SearchLineHistory2(HISTVAR *hivar,const TCHAR *p)
{
	switch( hivar->mode ){
		case HIST_CMD_ALL:			// 全体検索
			break;

		case HIST_CMD_FIND:			// コマンド前方一致検索
			while ( *p == ' ' ) p++;
			if ( (tstrlen(p) < hivar->findsize) ||
				 tstrnicmp(p,hivar->findstr,hivar->findsize) ){
				p = NULL;
			}
			break;

		case HIST_PARAM_ALL: {		// パラメータ検索
			int i;

			for ( i = hivar->count ; i ; i-- ){
				p = NextWord(p);
				if ( *p == '\0' ){
					if ( hivar->count == HISCOUNT_MAX ){
						hivar->count -= i;
						p = NULL;
					}
					return p;
				}
			}
			break;
		}
		case HIST_PARAM_FIND:{		// パラメータ前方一致検索
			int i;

			for ( i = hivar->count ; i ; i-- ){
				p = NextWord(p);
				if ( *p == '\0' ){
					if ( hivar->count == HISCOUNT_MAX ){
						hivar->count -= i;
						p = NULL;
					}
					return p;
				}
			}
			if ( (tstrlen(p) < hivar->findsize) ||
				 tstrnicmp(p,hivar->findstr,hivar->findsize) ){
				p = NULL;
			}
			break;
		}
	}
	if ( p ){
		if ( hivar->mode <= HIST_CMD_FIND ){ // HIST_CMD_ALL / HIST_CMD_FIND
			tstrcpy(hivar->findstr,p);
		}else{
			const UTCHAR *src;
			UTCHAR *dst;

			src = (const UTCHAR *)p;
			dst = (UTCHAR *)hivar->findstr;
			while( *src > ' ' ) *dst++ = *src++;
			*dst = '\0';
		}
	}
	return p;
}
// ----------------------------------------------------------------------------
const TCHAR *SearchLineHistory(HISTVAR *hivar,int offset)
{
	const TCHAR *p;

#ifdef DEBUG
	TCHAR buf[100];
	COORD pos;
	DWORD tmp;
	wsprintf(buf,T("Mode:%d Index:%3d Count:%3d"),
			hivar->mode,hivar->index,hivar->count);
	pos.X = 0;
	pos.Y = (SHORT)(screen.dwCursorPosition.Y + 1);
	WriteConsoleOutputCharacter(hStdout,buf,tstrlen(buf),pos,&tmp);
#endif
	if ( offset >= 0 ){			// 古いほうへ検索
		if ( hivar->count != 0 ){
			if ( hivar->index == -1 ) hivar->index = 0;
			p = EnumHistory(hivar->type,hivar->index);
			if ( p ) for ( ; ; ){
				for ( ; ; ){
					const TCHAR *q;

					q = SearchLineHistory2(hivar,p);
					if ( q != NULL ){
						if ( *q == '\0' ) break;
						hivar->count++;
						return q;
					}
					hivar->count++;
				}
				p = EnumHistory(hivar->type,hivar->index + 1);
				if ( p == NULL ) break;
				hivar->index++;
				hivar->count = 1;
			}
		}else{
			do{
				p = EnumHistory(hivar->type,hivar->index + 1);
				if ( p == NULL) break;
				hivar->index++;
				p = SearchLineHistory2(hivar,p);
			}while( p == NULL );
		}
	}else{						// 新しいほうへ検索
		p = NULL;
		if ( hivar->index != -1 ){
			if ( hivar->count != 0 ){
				for ( ; ; ){
					p = EnumHistory(hivar->type,hivar->index);
					if ( p != NULL ){
						if ( hivar->count < 1 ) hivar->count = HISCOUNT_MAX + 1;
						hivar->count--;

						while ( hivar->count != 0 ){
							const TCHAR *q;

							q = SearchLineHistory2(hivar,p);
							if ( q != NULL ){
								if ( *q == '\0' ) break;
								return q;
							}
							hivar->count--;
						}
					}
					hivar->index--;
					if ( hivar->index == -1 ) break;
				}
			}else{
				hivar->index--;
				while( hivar->index > -1 ){
					p = EnumHistory(hivar->type,hivar->index);
					if ( p == NULL ) break;
					p = SearchLineHistory2(hivar,p);
					if ( p != NULL ) break;
					hivar->index--;
				}
			}
		}
	}
	return p;
}

// ----------------------------------------------------------------------------
int tCInput(TCHAR *buf,size_t bsize,WORD htype)
{
	INPUT_RECORD cin;
	int X_calc = 1;
	int formlen = 0;

	ComplED.info = &ppbappinfo;
	ComplED.hF = NULL;
	ComplED.romahandle = 0;

	mouseSX = -1;
	maxsize = bsize;
	insmode = INS_INS;
	ShowOffset = 0;
	EdX = 0;
	SelStart = SelEnd = 0;
	cmode = CMODE_EDIT;
	baseptr = EditText = buf;
	eflag = TCI_TI_DRAW;
	tstrcpy(backuptext,EditText);
	RevBuf = NULL;

	SetIMEDefaultStatus(hMainWnd);
	InitHistorySearch(&hvar,EditText);
	histype = hvar.type = htype;

	tInit(NULL);
	if ( screen.dwCursorPosition.X != 0 ){ // 改行していないなら改行
		tputstr(T("\r\n"));
		screen.dwCursorPosition.Y++;
	}
	// 現在位置が画面外なら調整
	if ( (screen.dwCursorPosition.Y + 1) >= screen.dwSize.Y ){
		COORD pos;

		pos.Y = screen.dwCursorPosition.Y;
		pos.X = (SHORT)(screen.dwSize.X - 1);
		SetConsoleCursorPosition(hStdout,pos);
		tputstr(T(" ")); // 反映
		screen.dwCursorPosition.Y--;
	}

	GetCustData(T("CB_edit"),&CB_edit,sizeof(CB_edit));
	GetCustData(T("X_calc"),&X_calc,sizeof X_calc);
	do{
		int key;			// 現在のキーコード

		istrt = 0;
											// 表示位置の補正
		if ( ShowOffset > EdX ){
			ShowOffset = EdX;
			eflag = TCI_TI_DRAW;
		}
		#ifdef UNICODE
		{
			int x,wide = 0;
			for ( x = ShowOffset ; x < EdX ; x++ ){
				wide += CCharWide(*(EditText + x));
			}
			while ( screen.dwSize.X <= wide ){
				wide -= CCharWide(*(EditText + ShowOffset));
				ShowOffset++;
				eflag = TCI_TI_DRAW;
			}
		}
		#else
			while ( screen.dwSize.X <= (EdX - ShowOffset) ){
				ShowOffset += Chrlen(*(EditText + ShowOffset));
				eflag = TCI_TI_DRAW;
			}
		#endif
											// 表示処理
		if ( eflag == TCI_TI_DRAW ){
			TCHAR form[80];

			DisplayLine(EditText);
			eflag = 0;

			if ( formlen ){
				formlen = 0;
				if ( (screen.dwCursorPosition.Y + 1) < screen.dwSize.Y ){
					ClearFormLine();
				}
			}
			if ( X_calc && IsTrue(GetCalc(EditText,form,NULL)) ){
				DWORD dtmp;
				COORD pos;

				pos.X = 0;
				pos.Y = (SHORT)(screen.dwCursorPosition.Y + 1);
				formlen = tstrlen32(form);
				WriteConsoleOutputCharacter(hStdout,form,formlen,pos,&dtmp);
			}
		}
		if ( eflag == TCI_TI_SCROLL ){
			eflag = 0;
		}else if ( cmode == CMODE_EDIT ){
			#ifdef UNICODE
				WCHAR *p;
				int i,x;

				tCsrMode(insmode);
				for ( i = EdX - ShowOffset,x = 0,p = EditText + ShowOffset ; i ; i--,p++ ){
					x += TChrlen(*p);
				}
				tCsrPos(x,screen.dwCursorPosition.Y);
			#else
				tCsrMode(insmode);
				tCsrPos(EdX - ShowOffset,screen.dwCursorPosition.Y);
			#endif
		// カーソルが末尾でないなら、ヒストリの検索方法のキャッシュを解除する。
			if (*(EditText + EdX)) InitHistorySearch(&hvar,EditText);
		}else{
			tCsrMode(100);
			tCsrPos(sinfo.cx,sinfo.cy);
		}
		do{
			key = tgetc(&cin);
			if ( cin.EventType == MOUSE_EVENT ){
				key = MouseCommand(&cin.Event.MouseEvent);
			}
		}while( key == 0 );
		if ( key > 0 ){
			if ( KeyHookEntry != NULL ){
				if ( CallKeyHook(&ppbappinfo,(WORD)key) != PPXMRESULT_SKIP ){
					continue;
				}
			}
			CommonCommand(key);
		}else{
			if ( key == KEY_RECV ){			// MailSlot 受信（PPB用）
				if ( EditText != buf ) tstrcpy(buf,EditText);
				eflag = TCI_RECV;
			}
		}
	}while( eflag >= 0 );
	// 反転を戻す
	ReverseRange(FALSE);
	if ( incsearch.len ) ReverseText(incsearch.len);
	if ( cmode != CMODE_EDIT ) screen.dwCursorPosition.Y = (SHORT)sinfo.BackupY;
	SearchFileIned(&ComplED, T(""), NULL, 0);
	{
		COORD pos;
		WORD atr = T_CYA;

		GetCustData(T("CB_com"),&atr,sizeof(atr));
		HilightLine(atr);				// 編集位置の強調解除

		if ( formlen ) ClearFormLine();
		pos.X = 0;
		pos.Y = screen.dwCursorPosition.Y;
		SetConsoleCursorPosition(hStdout,pos);
	}
	return eflag;
}

void CommonCommand(int key)
{
	TCHAR buf[CMDLINESIZE];

	if ( !(key & K_raw) ){
		PutKeyCode(buf,key);
		if ( NO_ERROR == GetCustTable(T("KB_edit"),buf,buf,sizeof(buf)) ){
			WORD *p;

			if ( (UTCHAR)buf[0] == EXTCMD_CMD ){
				PP_ExtractMacro(hMainWnd,&ppbappinfo,NULL,buf + 1,NULL,(XEO_CONSOLE | XEO_NOUSEPPB) );
				return;
			}
			p = (WORD *)(((UTCHAR)buf[0] == EXTCMD_KEY) ? (buf + 1) : buf);
			key = *p;
			if ( key == 0 ) return;
			while( *(++p) ){
				CommonCommand(key);
				key = *p;
			}
			if ( !(key & K_raw) ){
				CommonCommand(key);
				return;
			}
		}
	}
	resetflag(key,K_raw);	// エイリアスビットを無効にする
	switch ( key ){
		case K_F1:
			PPxHelp(hMainWnd,HELP_CONTEXT,IDH_PPB);
			break;

		case K_a | K_F4:			// &[F4] 終了
			eflag = TCI_QUIT;
			break;

		case K_s | K_esc:			// \ESC:最小化(PPB用)
			if ( hBackWnd != NULL ){
				DWORD XV_minf = 2;

				GetCustData (T("XV_minf"),&XV_minf,sizeof(XV_minf));
				if ( XV_minf == 1 ){
					SendMessage(hMainWnd,WM_SYSCOMMAND,SC_MINIMIZE,MAX32);
					ForceSetForegroundWindow(hBackWnd);
					SetWindowPos(hMainWnd,HWND_BOTTOM,0,0,0,0,SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
					hBackWnd = NULL;
					break;
				}else if ( XV_minf == 2 ){
					SetForegroundWindow(hBackWnd);
				}
				hBackWnd = NULL;
			}
			ShowWindow(hMainWnd,SW_MINIMIZE);
			break;

		case K_a | K_up:				// [↑]
			MoveWindowByKey(hMainWnd,0,-1);
			break;
		case K_a | K_dw:			// [↓]
			MoveWindowByKey(hMainWnd,0,1);
			break;
		case K_a | K_lf:			// [←]
			MoveWindowByKey(hMainWnd,-1,0);
			break;
		case K_a | K_ri:			// [→]
			MoveWindowByKey(hMainWnd,1,0);
			break;

		case K_c | K_Pup:				// ^[PgUP]
			ScrollArea(-(screen.srWindow.Bottom - screen.srWindow.Top));
			break;

		case K_c | K_Pdw:				// ^[PgUP]
			ScrollArea(screen.srWindow.Bottom - screen.srWindow.Top);
			break;

		case K_c | K_up:				// ^[↑]
			ScrollArea(-1);
			break;
		case K_c | K_dw:			// ^[↓]
			ScrollArea(1);
			break;
//-----------------------------------------------
		case K_a | K_home: {	// &[home]
			WINPOS WPos;

			GetWindowRect(hMainWnd,&WinPos.pos);
			wsprintf(buf,T("%s_"),RegCID);
			if ( NO_ERROR == GetCustTable(T("_WinPos"),buf,&WPos,sizeof(WPos)) ){
				MoveWindow(hMainWnd,WPos.pos.left,WPos.pos.top,
						WinPos.pos.right - WinPos.pos.left,
						WinPos.pos.bottom - WinPos.pos.top,TRUE);
			}else{
				CenterWindow(hMainWnd);
			}
			break;
		}
//-----------------------------------------------
		case K_a | K_s | K_home:	// &\[home]
			GetWindowRect(hMainWnd,&WinPos.pos);
			wsprintf(buf,T("%s_"),RegCID);
			SetCustTable(T("_WinPos"),buf,&WinPos,sizeof(WinPos));
			break;

		case K_v | VK_APPS: {
			int index = PPbContextMenu();
			if ( index > 0 ) CommonCommand(index);
			break;
		}

		case K_c | K_s | K_v | VK_ADD:
		case K_c | K_s | K_v | VK_OEM_PLUS: // US[=/+] JIS[;/+]
		case K_c | K_s | K_v | VK_SUBTRACT:
		case K_c | K_s | K_v | VK_OEM_MINUS: // US[-/_] JIS[-/=]
			PPxCommonCommand(hMainWnd,0,(WORD)key);
			break;

		case K_c | K_v | VK_ADD:
		case K_c | K_v | VK_OEM_PLUS: // US[=/+] JIS[;/+]
			ConChangeFontSize(1);
			break;
		case K_c | K_v | VK_SUBTRACT:
		case K_c | K_v | VK_OEM_MINUS: // US[-/_] JIS[-/=]
			ConChangeFontSize(-1);
			break;

		case K_c | K_v | VK_NUMPAD0:
			ConChangeFontSize(-2);
			break;

		default:
			if ( cmode == CMODE_EDIT ){
				EditModeCommand(key);
			}else{
				ScrollModeCommand(key);
			}
	}
}

void EditModeCommand(int key)
{
	TCHAR *istr;		/* 入力された文字列（0:未入力） */
	TCHAR tmp[CMDLINESIZE];	// 一時編集領域

	istr = NULL;

	switch(key){
		case K_a | '0':
		case K_c | '0':
			istr = PPxPath;
			break;

		case K_a | '1':
		case K_c | '1':
			istr = EditPath;
			break;

		case K_c | 'A': {				//	^A:全て選択
			int i;

			i = tstrlen32(EditText);
			if ( (SelStart != 0) || (SelEnd != i) ){
				SelStart = 0;
				SelEnd = i;
				eflag = TCI_TI_DRAW;
			}
			break;
		}

		case K_c | K_s | 'A':			//	^\A:選択解除
			if ( SelStart != SelEnd ){
				SelStart = SelEnd = 0;
				eflag = TCI_TI_DRAW;
			}
			break;

		case K_c | 'C':				//	^C:クリップ
		case K_c | K_s | 'C':
		case K_c | 'X':				//	^X:カット
		case K_c | K_s | 'X':
			if ( *EditText && IsTrue(OpenClipboard(NULL)) ){
				HGLOBAL hG;
				int i,j;
				TCHAR *p;

				if ( SelStart != SelEnd ){
					i = SelEnd - SelStart;
					j = SelStart;
				}else{
					i = tstrlen32(EditText);
					j = 0;
				}
				hG = GlobalAlloc(GMEM_MOVEABLE,TSTROFF(i + 1));
				if ( hG != NULL ){
					p = GlobalLock(hG);
					memcpy(p,(char *)(TCHAR *)(EditText + j),TSTROFF(i));
					*(p + i) = 0;
					GlobalUnlock(hG);
					EmptyClipboard();
					SetClipboardData(CF_TTEXT,hG);
				}
				CloseClipboard();

				if ( (key & 0xff) == 'X' ){
					if ( SelStart != SelEnd ){
						tstrcpy(EditText + SelStart,EditText + SelEnd);
						EdX = SelStart;
						SelStart = SelEnd = 0;
					}else{
						*EditText = 0;
						EdX = SelStart = SelEnd = 0;
					}
					eflag = TCI_TI_DRAW;
				}
			}
			break;

		case K_c | 'V':				//	^V:ペースト
			if ( IsTrue(OpenClipboard(NULL)) ){
				HGLOBAL hG;

				hG = GetClipboardData(CF_TTEXT);
				if ( hG != NULL ){
					TCHAR *src,*srcmax,*dest;
					size_t len;

					src = GlobalLock(hG);
					len = tstrlen(src);
					if ( len >= (TSIZEOF(tmp) - 1)) len = TSIZEOF(tmp) - 1;
					srcmax = src + len;
					dest = tmp;
					while( src < srcmax ){
						if ( (*src != '\r') && (*src != '\n') ){
							*dest++ = *src;
						}
						src++;
					}
					*dest = '\0';
					GlobalUnlock(hG);
					istr = tmp;
				}
				CloseClipboard();
			}
			break;

		case K_c | 'M':
		case K_cr:					// CR:確定
			if ( EditText != baseptr ) tstrcpy(baseptr,EditText);
			eflag = TCI_EXECUTE;
			break;

		case K_c | K_s | 'M':
		case K_s | K_cr: {			// Shift+CR:GUI一行編集
			TINPUT tinput;

			tinput.hOwnerWnd= hMainWnd;
			tinput.hRtype	= histype;
			tinput.hWtype	= histype;
			tinput.title	= T("PPb");
			tinput.buff		= EditText;
			tinput.size		= CMDLINESIZE;
			tinput.flag		= TIEX_TOP;
			if ( tInputEx(&tinput) > 0 ){
				EdX = 0;
				SelStart = SelEnd = 0;
				eflag = TCI_TI_DRAW;
			}
			break;
		}
		case K_esc:					// ESC:中止
			eflag = TCI_QUIT;
			break;

		case K_tab:					// \SPACE/TAB:ファイル名補完
		case K_c | 'I': {
			TCHAR *p;
			ECURSOR cursor;

			tstrcpy(tmp,EditText);
			if ( SelStart == SelEnd ){
				cursor.start = EdX;
				cursor.end	 = EdX;
			}else{
				cursor.start = SelStart;
				cursor.end	 = SelEnd;
			}

			p = SearchFileIned(&ComplED,tmp,&cursor,
				((histype & PPXH_COMMAND) ? CMDSEARCH_CURRENT : CMDSEARCH_OFF) |
				((key != (K_c | 'I')) ? 0 : CMDSEARCH_FLOAT) |
				CMDSEARCH_MULTI );

			if ( p != NULL ){	// 検索成功
				size_t len;

				len = tstrlen(p);
				SelStart = cursor.start;
				EdX = SelEnd = cursor.end;

				istr = p;
				istrt = 0;
				if ( len && (*(p + len - 1) == '\"') ) istrt = 2;
				ShowOffset = 0;	// 表示開始桁を補正させる
			}
			break;
		}
		case       K_ri:			//  →:右へ移動
		case K_s | K_ri: {			// \→:右へ移動+選択
			int newX;

			newX = EdX;
			#ifdef UNICODE
			if ( ((size_t)newX < maxsize) && *(EditText + newX) ) newX++;
			#else
			if ((size_t)newX < maxsize) newX += Chrlen(*(EditText + newX));
			#endif
			MoveCursor(newX,key);
			break;
		}
		case       K_c | K_ri:		//  ^→:右へ単語移動
		case K_s | K_c | K_ri: {	// ^\→:右へ単語移動+選択
			int newX = ForwardEditWord(EdX);

			while ( *(EditText + newX) == ' ' ){
				if ( (size_t)newX >= maxsize ) break;
				newX++;
			}
			MoveCursor(newX,key);
			break;
		}
		case       K_lf:			//  ←:左へ移動
		case K_s | K_lf: {			// \←:左へ移動+選択
			int newX;

			newX = EdX;
			#ifdef UNICODE
			if ( newX != 0 ) newX--;
			#else
			if ( newX != 0 ) newX -= bchrlen(EditText,newX);
			#endif
			MoveCursor(newX,key);
			break;
		}
		case       K_c | K_lf:		// ^←:左へ単語移動
		case K_s | K_c | K_lf:	// ^\←:左へ単語移動+選択
			MoveCursor(BackEditWord(EdX),key);
			break;

		case       K_home:			//  HOME:左端
		case K_s | K_home:			// \HOME:左端+選択
			MoveCursor(0,key);
			break;

		case       K_end:			//  END:右端
		case K_s | K_end:			// \END:右端+選択
			MoveCursor(tstrlen32(EditText),key);
			break;

		case K_up:					// ↑:古いヒストリ
		case K_dw: {				// ↓:新しいヒストリ
			const TCHAR *p;

			GetHistorySearchMode(&hvar,EditText);
			UsePPx();
			p = SearchLineHistory(&hvar,(key == K_up) ? 1 : -1);
			if ( p == NULL ){
				if ( hvar.mode == HIST_CMD_FIND ){
					hvar.mode = HIST_CMD_ALL;
					hvar.index = -1;
					hvar.count = 0;
					p = SearchLineHistory(&hvar,(key == K_up) ? 1 : -1);
				}
				if ( hvar.mode == HIST_PARAM_FIND ){
					hvar.mode = HIST_PARAM_ALL;
					hvar.index = -1;
					hvar.count = 1;
					p = SearchLineHistory(&hvar,(key == K_up) ? 1 : -1);
				}
			}
			if ( p != NULL ){
				ShowOffset = 0;
				EdX = tstrlen32(EditText);
				SelStart = SelEnd = 0;
				eflag = TCI_TI_DRAW;
			}
			FreePPx();
			break;
		}

		case K_bs:					//	BS:左側の１文字を削除
			InitHistorySearch(&hvar,EditText);
			if ( SelStart != SelEnd ){
				BackupLine(EditText,backuptext);

				TPushEditStack(0,EditText,SelStart,SelEnd);
				tstrcpy(EditText + SelStart,EditText + SelEnd);
				EdX = SelStart;
				SelStart = SelEnd = 0;
				eflag = TCI_TI_DRAW;
			}else if ( EdX != 0 ){
				int len;

				BackupLine(EditText,backuptext);
				EdX -= len = bchrlen(EditText,EdX);
				TPushEditStack(0,EditText,EdX,EdX + len);
				tstrcpy(EditText + EdX,EditText + EdX + len);
				eflag = TCI_TI_DRAW;
			}
			break;

		case K_s | K_bs:			//	\BS:左側を全て削除
			if ( EdX != 0 ){
				InitHistorySearch(&hvar,EditText);
				BackupLine(EditText,backuptext);
				TPushEditStack(0,EditText,0,EdX);
				tstrcpy(EditText,EditText + EdX);
				EdX = SelStart = SelEnd = 0;
				eflag = TCI_TI_DRAW;
			}
			break;

		case K_a | K_bs:			//	&BS:元に戻す
		case K_c | 'Z':				//	^Z
			if ( tstrcmp(backuptext,EditText) != 0 ){
				tstrcpy(tmp,EditText);
				tstrcpy(EditText,backuptext);
				tstrcpy(backuptext,tmp);

				ShowOffset = 0;
				SelStart = SelEnd = 0;
				EdX = tstrlen32(EditText);
				eflag = TCI_TI_DRAW;
			}
			break;

		case K_del:					//	DEL:現在の文字を削除
			if ( SelStart != SelEnd ){
				BackupLine(EditText,backuptext);
				TPushEditStack(B0,EditText,SelStart,SelEnd);
				tstrcpy(EditText + SelStart,EditText + SelEnd);
				EdX = SelStart;
				SelStart = SelEnd = 0;
				eflag = TCI_TI_DRAW;
			}else if( *(EditText + EdX) != '\0' ){
				int len;

				BackupLine(EditText,backuptext);
				#ifdef UNICODE
				len = *(EditText + EdX) ? 1 : 0;
				#else
				len = Chrlen(*(EditText + EdX));
				#endif
				TPushEditStack(B0,EditText,EdX,EdX + len);
				tstrcpy(EditText + EdX,EditText + EdX + len);
				eflag = TCI_TI_DRAW;
			}
			break;

		case K_s | K_del:			//	\DEL:現在以降を削除
		case K_c | K_del:
			if( *(EditText + EdX) != '\0' ){
				BackupLine(EditText,backuptext);
				TPushEditStack(B0,EditText,EdX,tstrlen32(EditText));
				*(EditText + EdX) = '\0';
				SelStart = SelEnd = 0;
				eflag = TCI_TI_DRAW;
			}
			break;

		case K_ins:					//	INS:挿入状態の変更
			insmode ^= (INS_INS ^ INS_OVER);
			break;

		case K_c | 'U':{	// ^[U]
			TCHAR mode;

			PopTextStack(&mode,tmp);
			if ( tmp[0] ){
				istr = tmp;
				if ( mode ) istrt = 1;
			}
			break;
		}

		case K_s | K_up:			// \↑:スクロールモード
		case K_s | K_dw:			// \↓:スクロールモード
		case K_Pup:			// Pup:スクロールモード
		case K_Pdw:			// Pdw:スクロールモード
			SetScrollMode();
			break;

		default:					//	文字入力
			#ifdef UNICODE
			if ( key & (K_v | K_e | K_c | K_a) ) break;
			key = (key & 0xff) | ((key >> 8) & 0xff00);
			if ( ' ' <= key ){
			#else
			key &= 0xff | K_v | K_e | K_c | K_a;	/* shift 情報を破棄 */
			if ( (' ' <= key) && (key < 0x100) ){
			#endif
				BackupLine(EditText,backuptext);
				InitHistorySearch(&hvar,EditText);
				istr = tmp;
				tmp[0] = (TCHAR)key;
				tmp[1] = '\0';
				#ifndef UNICODE
				if ( IskanjiA(key) ){
					INPUT_RECORD cin;
					int key;

					key = tgetc(&cin);
					if ( (key < 0) || (cin.EventType != KEY_EVENT) ){
						tmp[0] = '\0';
					}else{
						tmp[1] = (TCHAR)key;
						tmp[2] = '\0';
					}
				}
				#endif
			}
	}
	if ( istr ) Replace(istr,FALSE);
}

void MoveCursorS(int offX,int offY,int key)
{
	int newX,newY;

	newX = sinfo.cx + offX;
	if ( newX < 0 ){
		newX = 0;
	}else if ( newX >= screen.dwSize.X ){
		newX = screen.dwSize.X - 1;
	}
	newY = sinfo.cy + offY;
	if ( newY < 0 ){
		newY = 0;
	}else if ( newY >= screen.dwSize.Y ){
		newY = screen.dwSize.Y - 1;
	}
	if ( (sinfo.cx == newX) && (sinfo.cy == newY) ){
		if ( !(key & K_s) ) ReverseRange(FALSE);
		return;
	}

	if ( key & K_s ){	// 選択
		if ( RevBuf == NULL ){
			sinfo.sx = sinfo.cx;
			sinfo.sy = sinfo.cy;
		}
		ReverseRange(FALSE);
		sinfo.cx = newX;
		sinfo.cy = newY;
		sinfo.LineMode = offX;
		ReverseRange(TRUE);
	}else{
		ReverseRange(FALSE);
		sinfo.cx = newX;
		sinfo.cy = newY;
	}
}
// mode = 0:→↑ 1:←↓
void JumpWordCursor(BOOL x,int mode,int key)
{
	COORD newXY;
	TCHAR c;
	DWORD temp;
	SHORT *cur, maxpos;

	newXY.X = (SHORT)sinfo.cx;
	newXY.Y = (SHORT)sinfo.cy;

	if ( IsTrue(x) ){
		cur = &newXY.X;
		maxpos = (SHORT)(screen.dwSize.X - 1);
	}else{
		cur = &newXY.Y;
		maxpos = (SHORT)(screen.dwSize.Y - 1);
	}

	for ( ; ; ){
		ReadConsoleOutputCharacter(hStdout,&c,1,newXY,&temp);
		if ( (mode == 0) || (mode == 3) ){
			if ( (UTCHAR)c <= ' ' ) mode += 2;
		}else{
			if ( (UTCHAR)c != ' ' ) mode += 2;
		}
		if ( mode > 3 ) break;

		if ( mode & 1 ){
			if ( *cur == '\0' ) break;
			(*cur)--;
		}else{
			if ( *cur >= maxpos ) break;
			(*cur)++;
		}
	}
	MoveCursorS(newXY.X - sinfo.cx,newXY.Y - sinfo.cy,key);
}

void ScrollModeCommand(int key)
{
	switch(key){
		case K_c | 'A':				//	^A:全て選択
			ReverseRange(FALSE);
			sinfo.cx = screen.dwSize.X - 1;
			sinfo.cy = screen.dwSize.Y - 1;
			sinfo.sx = 0;
			sinfo.sy = 0;
			sinfo.LineMode = FALSE;
			ReverseRange(TRUE);
			break;
		case K_c | 'C':				// ^C:クリップ
		case K_c | K_s | 'C':
		case K_c | 'M':
		case K_cr:					// CR:確定
			ClipText();
			// K_esc へ
		case K_esc:					// ESC:入力モードへ
			ReverseRange(FALSE);
			cmode = CMODE_EDIT;
			screen.dwCursorPosition.Y = (SHORT)sinfo.BackupY;
			eflag = TCI_TI_DRAW;
			break;

		case K_ri:					// →
		case K_s | K_ri:			// \→
			MoveCursorS(1,0,key);
			break;

		case K_lf:					// ←
		case K_s | K_lf:			// \←
			MoveCursorS(-1,0,key);
			break;

		case K_up:					// ↑
		case K_s | K_up:			// \↑
			MoveCursorS(0,-1,key);
			break;

		case K_dw:					// ↓
		case K_s | K_dw:			// \↓
			MoveCursorS(0,1,key);
			break;

		case K_Pup:					// Pup
		case K_s | K_Pup:			// \Pup
			MoveCursorS(0,-(screen.srWindow.Bottom - screen.srWindow.Top),key);
			break;

		case K_Pdw:					// Pdw
		case K_s | K_Pdw:			// \Pdw
			MoveCursorS(0,(screen.srWindow.Bottom - screen.srWindow.Top),key);
			break;

		case K_home:				// home
		case K_s | K_home:			// \home
			MoveCursorS(-sinfo.cx,0,key);
			break;

		case K_end:					// end
		case K_s | K_end:			// \end
			MoveCursorS(screen.dwSize.X,0,key);
			break;

		case K_c | K_ri:			// ^→
		case K_s | K_c | K_ri:		// \^→
			JumpWordCursor(TRUE,0,key);
			break;

		case K_c | K_lf:			// ^←
		case K_s | K_c | K_lf:		// \^←
			JumpWordCursor(TRUE,1,key);
			break;

		case K_s | K_c | K_dw:		// \^↓
			JumpWordCursor(FALSE,0,key);
			break;

		case K_s | K_c | K_up:		// \^↑
			JumpWordCursor(FALSE,1,key);
			break;

		case K_tab:
			if ( incsearch.len == 0 ) break;
			if ( ScrIncSearch(1) == FALSE ){
				ScrIncSearch(2);
			}
			return;

		case K_bs:
			if ( incsearch.len != 0 ){
				ReverseText(incsearch.len);
				incsearch.len--;
				ReverseText(incsearch.len);
			}
			return;

		default:
			#ifdef UNICODE
			if ( key & (K_v | K_e | K_c | K_a) ) return;
			key = (key & 0xff) | ((key >> 8) & 0xff00);
			if ( ' ' <= key ){
			#else
			key &= 0xff | K_v | K_e | K_c | K_a;	/* shift 情報を破棄 */
			if ( (' ' <= key) && (key < 0x100) ){
			#endif
				if ( incsearch.len == 0 ) incsearch.pos.X = incsearch.pos.Y = 0;
				if ( (size_t)incsearch.len < (TSIZEOF(incsearch.text) - 1) ){
					incsearch.text[incsearch.len++] = (TCHAR)key;
					incsearch.text[incsearch.len] = '\0';
					if ( ScrIncSearch(0) == FALSE ) incsearch.len--;
				}
			}
			return;
	}
	if ( incsearch.len ){
		ReverseText(incsearch.len);
		incsearch.len = 0;
	}
}

int MouseCommand(MOUSE_EVENT_RECORD *me)
{
	switch ( me->dwEventFlags ){
		case MOUSE_MOVED:{ //範囲
			CONSOLE_SCREEN_BUFFER_INFO cinfo;
			DWORD NewScrollTick;

			// カーソル位置の補正(NT4.0だとはみでる)
			GetConsoleWindowInfo(hStdout,&cinfo);
			if ( me->dwMousePosition.X < 0 ){
				me->dwMousePosition.X = 0;
			}
			if ( me->dwMousePosition.X >= cinfo.dwSize.X ){
				me->dwMousePosition.X = (SHORT)(cinfo.dwSize.X - 1);
			}
			if ( me->dwMousePosition.Y < 0 ){
				me->dwMousePosition.Y = 0;
			}
			if ( me->dwMousePosition.Y >= cinfo.dwSize.Y ){
				me->dwMousePosition.Y = (SHORT)(cinfo.dwSize.Y - 1);
			}

			// 範囲選択
			if ( (cmode == CMODE_SELECT) &&
				 (me->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) &&
				 ((sinfo.cx != me->dwMousePosition.X) ||
				  (sinfo.cy != me->dwMousePosition.Y)) ){
				ReverseRange(FALSE);
				sinfo.LineMode = (sinfo.cx != me->dwMousePosition.X);
				if ( me->dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED) ){
					sinfo.LineMode = !sinfo.LineMode;
				}
				sinfo.cx = me->dwMousePosition.X;
				sinfo.cy = me->dwMousePosition.Y;
				ReverseRange(TRUE);
				tCsrPos(sinfo.cx,sinfo.cy);
				NewScrollTick = GetTickCount();
				// overflow は無視
				if ( (OldScrollTick + 10) < NewScrollTick ){
					BOOL modify = FALSE;

					OldScrollTick = NewScrollTick;
					if ( cinfo.srWindow.Left &&
							(cinfo.srWindow.Left >= me->dwMousePosition.X) ){
						modify = TRUE;
						cinfo.srWindow.Left--;
						cinfo.srWindow.Right--;
					}else if ( (cinfo.srWindow.Right < (cinfo.dwSize.X - 1) )
						&& (cinfo.srWindow.Right <= me->dwMousePosition.X) ){
						modify = TRUE;
						cinfo.srWindow.Left++;
						cinfo.srWindow.Right++;
					}
					if ( cinfo.srWindow.Top &&
							(cinfo.srWindow.Top >= me->dwMousePosition.Y) ){
						modify = TRUE;
						cinfo.srWindow.Top--;
						cinfo.srWindow.Bottom--;
					}else if ( (cinfo.srWindow.Bottom < (cinfo.dwSize.Y - 1) )
						&& (cinfo.srWindow.Bottom <= me->dwMousePosition.Y) ){
						modify = TRUE;
						cinfo.srWindow.Top++;
						cinfo.srWindow.Bottom++;
					}
					if ( IsTrue(modify) ){
						SetConsoleWindowInfo(hStdout,TRUE,&cinfo.srWindow);
					}
				}
			}
			if ( (mouseSX >= 0) &&
				 (me->dwButtonState & (FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED)) ){
				int nx;

				nx = GetMouseX(ShowOffset,me->dwMousePosition.X,EditText);
				if ( mouseSX < nx ){
					SelStart = mouseSX;
					EdX = SelEnd = nx;
				}else{
					EdX = SelStart = nx;
					SelEnd = mouseSX;
				}
				NewScrollTick = GetTickCount();
				// overflow は無視
				if ( (OldScrollTick + 10) < NewScrollTick){
					OldScrollTick = NewScrollTick;
					if ( ShowOffset && !me->dwMousePosition.X ) ShowOffset--;
					if ( (me->dwMousePosition.X >= (screen.dwSize.X - 1)) &&
						((size_t)EdX < maxsize) && *(EditText + EdX) ){
						ShowOffset++;
					}
				}
				eflag = TCI_TI_DRAW;
				return KEY_DUMMY;
			}
			break;
		}
		case DOUBLE_CLICK:
			if ( cmode == CMODE_EDIT ){
				// 編集行なら、編集行の選択
				if ( me->dwMousePosition.Y == screen.dwCursorPosition.Y ){
					int nx;

					nx = GetMouseX(ShowOffset,me->dwMousePosition.X,EditText);
					if ( nx >= 0 ){
						SelEnd = ForwardEditWord(nx);
						SelStart = BackEditWord(nx);
						eflag = TCI_TI_DRAW;
						return KEY_DUMMY;
					}
				}else{ // ログ
/*
					if ( SelStart != SelEnd ){ // 編集行選択中なら解除
						SelStart = SelEnd = 0;
						eflag = TCI_TI_DRAW;
						return KEY_DUMMY;
					}else{
						SetScrollMode();
						SetSelectMode(me);
						return KEY_DUMMY;
					}
*/
				}
			}else{
			/*
				JumpWordCursor(TRUE,0,K_s | K_ri);
						eflag = TCI_TI_DRAW;
						return KEY_DUMMY;
			*/
			}

			break;
		#ifndef MOUSE_HWHEELED
		#define MOUSE_HWHEELED 8
		#endif
		case MOUSE_HWHEELED:
		case MOUSE_WHEELED: {
			int now;

			now = -(short)HIWORD(me->dwButtonState);
			if ( (WheelOffset ^ now) & B31 ) WheelOffset = 0; // Overflow
			WheelOffset += now;
			now = WheelOffset / (WHEEL_DELTA / 3);
			WheelOffset -= now * (WHEEL_DELTA / 3);

			if ( me->dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED) ){
				if ( me->dwControlKeyState & SHIFT_PRESSED ){
#if 0 // 現在、正常に機能しない
					PPxCommonCommand(hMainWnd,0,
						(now < 0) ? (WORD)(K_c | K_s | K_v | VK_ADD) : (WORD)(K_c | K_s | K_v | VK_SUBTRACT));
#endif
				}else{
					ConChangeFontSize((SHORT)-now);
				}
			}else{
				ScrollArea(now);
			}
			break;
		}

		default:{		// マウスボタンの状態が変化した
			DWORD b,c;
			int button = 0;
													// 左右同時押し
			if ( me->dwButtonState == (FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED) ){
				return K_s | K_esc;
			}
													// ボタン変化を抽出
			b = OldMouseButton ^ me->dwButtonState;
			c = me->dwButtonState;
			OldMouseButton = me->dwButtonState;

			while ( button < 30 ){
				if ( !(b & LSBIT) ){	// このボタンは変化していない
					b = b >> 1;
					c = c >> 1;
					button++;
					continue;
				}
				c = c & LSBIT;
				break;
			}
			if ( c ){ // down 状態
				if ( button == 0 ){	// 左down
					if ( cmode == CMODE_EDIT ){
						// 編集行なら、編集行の選択
						if ( me->dwMousePosition.Y ==
								screen.dwCursorPosition.Y ){
							int nx;

							nx = GetMouseX(ShowOffset,
								 me->dwMousePosition.X,EditText);
							if ( (nx >= 0) &&
								 ((button == 0) ||
								 	((nx < SelStart) || (nx > SelEnd )) ) ){
								mouseSX = SelStart = SelEnd = EdX = nx;
								eflag = TCI_TI_DRAW;
								return KEY_DUMMY;
							}
						}else{ // ログ
							if ( SelStart != SelEnd ){ // 編集行選択中なら解除
								SelStart = SelEnd = 0;
								eflag = TCI_TI_DRAW;
								return KEY_DUMMY;
							}else{
								SetScrollMode();
								SetSelectMode(me);
								return KEY_DUMMY;
							}
						}
					}else{
						ReverseRange(FALSE);
						SetSelectMode(me);
					}
					break;
				}
			}
			if ( c ) break;
			mouseSX = -1;
			// up 状態
/*
			if ( button == 0 ){	// 左up
				if ( (cmode >= CMODE_SELECT) && (sx == cx) && (sy == cy) ){
					ReverseRange(FALSE);
					cmode = CMODE_EDIT;
					screen.dwCursorPosition.Y = (SHORT)BackupY;
					eflag = TCI_TI_DRAW;
					return KEY_DUMMY;
				}
			}
*/
			if ( button == 2 ){		// 中up
				return K_s | K_esc;
			}
			if ( button == 1 ){		// 右up
				return PPbContextMenu();
			}
		}
	}
	return 0;
}

int PPbContextMenu(void)
{
	int index;

	if ( cmode != CMODE_EDIT ){
		index = Select(selectmenu);
		if ( index <= 0 ){ // キャンセルなら元に戻す
			ReverseRange(FALSE);
			cmode = CMODE_EDIT;
			screen.dwCursorPosition.Y = (SHORT)sinfo.BackupY;
			eflag = TCI_TI_DRAW;
			return KEY_DUMMY;
		}
		return index;
	}else{
		index = Select(editmenu);

		if ( index <= 0 ) return 0;
		if ( index == 1 ){ // 範囲選択モードへ
			SetScrollMode();
			return K_s;
		}
		return index;
	}
}
