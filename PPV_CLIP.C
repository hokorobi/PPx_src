/*-----------------------------------------------------------------------------
	Paper Plane vUI		クリップ処理
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <windowsx.h>
#include "PPX.H"
#include "VFS.H"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop

#define PASTETYPEPROP "PPxPTYPE"

#if 0 // UNICODE版SelCol は、現在不要

#ifdef UNICODE
#define TSelCol SelColW
WCHAR *SelColW(WCHAR *str,int col)
{
	SIZE szt;
	int colx,x = 0,left = 0;
	HDC hDC;
	HGDIOBJ hOldFont;

	hDC = GetDC(hMainWnd);
	hOldFont = SelectObject(hDC,hBoxFont);

	colx = col * fontX;
	szt.cx = 0;
	if ( *str ) for ( ;; ){
		if ( *(str + x) == '\t' ){
			int tabwidth;

			tabwidth = VOi->tab * fontX;
			left += ((szt.cx / tabwidth) + 1) * tabwidth;
			if ( left > colx ) break;
			str += x + 1;
			x = 0;
			szt.cx = 0;
		}else{
			x++;
			GetTextExtentPoint32W(hDC,str,x,&szt);
			if ( (left + szt.cx) > colx ){
				x--;
				break;
			}
		}
		if ( *(str + x) == '\0' ) break;
	}
	SelectObject(hDC,hOldFont);
	ReleaseDC(hMainWnd,hDC);
	return str + x;
}
#else
#define TSelCol SelColA
#endif

#endif

char *SelColA(char *str,int col)
{
	int i;

	i = col;
	while ( i > 0 ){
		if ( !*str ) break;
		if ( IskanjiA(*str & 0xff) ){
						// 0xff : どうしても signed に解釈されるため
			str++;
			i--;
			if ( !*str ) break;
		}
		if ( *str == '\t' ){
			i -= (((col - i) / (VOi->tab)) + 1) * VOi->tab - (col - i);
		}else{
			i--;
		}
		str++;
	}
	return str;
}

int CalcHexX(int off)
{
	if ( off >= (HEXNWIDTH * (HEXSTRWIDTH / 2)) ) off--;
	off /= HEXNWIDTH;
	if ( off > 16 ) off = 16;
	return off;
}

BOOL ClipHexMem(TMS_struct *text,int StartLine,int EndLine)
{
	char *bottom;
	int	size,tsize;

	if ( StartLine < 0 ){
		bottom = (char *)vo_.file.image + VOsel.bottom.y.line * 16;
		size = (VOsel.top.y.line - VOsel.bottom.y.line + 1) * 16;

		if ( !VOsel.line ){
			tsize = CalcHexX(VOsel.bottom.x.offset);
			bottom += tsize;
			size -= tsize + (16 - CalcHexX(VOsel.top.x.offset));
		}
	}else{
		bottom = (char *)vo_.file.image + StartLine * 16;
		size = (EndLine - StartLine + 1) * 16;
	}

	TMS_reset(text);
	if ( (size == 0) || (TM_check(&text->tm,size + 2) == FALSE) ) return FALSE;
	text->p = size;

	memcpy(text->tm.p,bottom,size);

	((BYTE *)text->tm.p)[size] = '\0';
	((BYTE *)text->tm.p)[size + 1] = '\0';
	return TRUE;
}

BOOL ClipMem(TMS_struct *text,int StartLine,int EndLine)
{
	#ifdef UNICODE
		WCHAR wbuf[TEXTBUFSIZE];
	#else
		char abuf[TEXTBUFSIZE];
	#endif
	BYTE form[TEXTBUFSIZE],*p;
	int off;
	int bottom,top;
	int XV_bctl3bk;
	MAKETEXTINFO mti;

	if ( vo_.DModeType == DISPT_HEX ) return ClipHexMem(text,StartLine,EndLine);

	XV_bctl3bk = XV_bctl[2];
	XV_bctl[2] = 0;

	if ( StartLine >= 0 ){
		bottom = StartLine;
		top = EndLine;
	}else if ( VOsel.select != FALSE ){
		bottom = VOsel.bottom.y.line;
		top = VOsel.top.y.line;
	}else{
		if ( VOsel.cursor != FALSE ){
			bottom = VOsel.now.y.line;
		}else{
			bottom = VOi->offY;
		}
		top = bottom + VO_sizeY - 1;
	}
	if ( top >= VOi->line ) top = VOi->line - 1;

	mti.destbuf = form;
	mti.srcmax = vo_.file.image + vo_.file.size.l;
	mti.writetbl = FALSE;
	mti.paintmode = FALSE;

	TMS_reset(text);
	for ( off = bottom ; off <= top ; off++ ){
		int CharX;

		VOi->MakeText(&mti,&VOi->ti[off]);
		p = form;

		CharX = 0;
		while ( *p != VCODE_END ) switch (*p){ // VCODE_SWITCH
			case VCODE_CONTROL:
			case VCODE_ASCII: 	// ASCII Text ---------------------------------
			{
				int length,copylength;
				char *src;

				src = (char *)p + 1;
				copylength = length = strlen32(src);
				if ( VOsel.line == FALSE ){
					if ( off == top ){	// 末尾調節
						if ( (CharX + copylength) > VOsel.top.x.offset ){
							copylength = VOsel.top.x.offset - CharX;
							if ( copylength <= 0 ){
								copylength = 0;
							}
						}
						*(src + copylength) = '\0';
					}
					if ( off == bottom ){	// 先頭調節
						int len;

						len = VOsel.bottom.x.offset - CharX;
						if ( len < 0 ) len = 0;
						if ( len > copylength ) len = copylength;
						src += len;
					}
				}

			#ifdef UNICODE
				AnsiToUnicode(src,wbuf,TEXTBUFSIZE);
				TMS_set(text,wbuf);
				text->p -= 2;
			#else
				TMS_setA(text,src);
				text->p--;
			#endif
				p += length + 1 + 1; // VCODE_ASCII + 文字列 + \0
				CharX += length;
				break;
			}

			case VCODE_UNICODEF:
				p++;
			case VCODE_UNICODE:		// UNICODE Text ---------------------------
			{
				int length,copylength;
				WCHAR *src;

				src = (WCHAR *)(p + 1);
				copylength = length = strlenW32(src);
				if ( VOsel.line == FALSE ){
					if ( off == top ){	// 末尾調節
						if ( (CharX + copylength) > VOsel.top.x.offset ){
							copylength = VOsel.top.x.offset - CharX;
							if ( copylength <= 0 ){
								copylength = 0;
							}
						}
						*(src + copylength) = '\0';
					}
					if ( off == bottom ){	// 先頭調節
						int len;

						len = VOsel.bottom.x.offset - CharX;
						if ( len < 0 ) len = 0;
						if ( len > copylength ) len = copylength;
						src += len;
					}
				}

			#ifdef UNICODE
				TMS_set(text,src);
				text->p -= 2;
			#else
				WideCharToMultiByte(CP_ACP,0,src,-1,abuf,sizeof abuf,"・",NULL);
				TMS_setA(text,abuf);
				text->p--;
			#endif
				p += (length + 1) * sizeof(WCHAR) + 1; // VCODE_UNICODE + 文字列 + \0
				CharX += length;
				break;
			}
			case VCODE_COLOR:			// Color -----------------------------
				p += 3;
				break;

			case VCODE_BCOLOR:			// F Color ---------------------------
			case VCODE_FCOLOR:			// F Color ---------------------------
				p += 1 + sizeof(COLORREF);
				break;

			case VCODE_FONT:			// Font ------------------------------
				p += 2;
				break;

			case VCODE_TAB:				// Tab -------------------------------
				p++;
				if ( VOsel.line == FALSE ){
					if ( off == top ){	// 末尾調節
						if ( CharX >= VOsel.top.x.offset ) break;
					}
					if ( off == bottom ){	// 先頭調節
						if ( CharX < VOsel.bottom.x.offset ){
							CharX++;
							break;
						}
					}
				}
				TMS_set(text,T("\t"));
				text->p -= sizeof(TCHAR);
				CharX++;
				break;

			case VCODE_RETURN:
				p++;
			case VCODE_PAGE:
			case VCODE_PARA:
				TMS_set(text,T("\r\n"));
				text->p -= sizeof(TCHAR);
				p++;
				break;

			case VCODE_LINK:			// Link ------------------------------
				p++;
				break;

			default:		// 未定義コード -----------------------------------
				p = (BYTE *)"";
				break;
		}
	}
	// 末尾の改行を削除
	if ( text->tm.p == NULL ) return FALSE;
	if ( *(TCHAR *)(char *)((char *)text->tm.p + text->p - sizeof(TCHAR)) == '\n' ){
		text->p -= sizeof(TCHAR);
		if ( text->p && (*(TCHAR *)(char *)((char *)text->tm.p + text->p - sizeof(TCHAR)) == '\r') ){
			text->p -= sizeof(TCHAR);
		}
	}
	*(TCHAR *)(char *)((char *)text->tm.p + text->p) = '\0';
	XV_bctl[2] = XV_bctl3bk;
	return TRUE;
}

void ClipText(HWND hWnd)
{
	TMS_struct text = {{NULL,0,NULL},0};
	HGLOBAL hTmp;

	if ( ClipMem(&text,-1,-1) == FALSE ) return;
	TM_off(&text.tm);
	hTmp = GlobalReAlloc(text.tm.h,text.p + sizeof(TCHAR),GMEM_MOVEABLE);
	if ( hTmp != NULL ) text.tm.h = hTmp;

	OpenClipboard(hWnd);
	EmptyClipboard();
	#ifdef UNICODE
		if ( (vo_.DModeType == DISPT_HEX) || (OSver.dwPlatformId != VER_PLATFORM_WIN32_NT) ){
			SetClipboardData(CF_TEXT,text.tm.h);
		}else{
			SetClipboardData(CF_UNICODETEXT,text.tm.h);
		}
	#else
		SetClipboardData(CF_TEXT,text.tm.h);
	#endif
	CloseClipboard();
}


#define URI_NO 0
#define URI_URL 1
#define URI_MAIL 2
#define URI_PATH 3
#define URI_ANCHOR 0x10

BOOL MakeURIs2(HMENU hMenu,int *index,const char *bottom,const char *start,const char *end,int use)
{
	char buf[0x400],tmp[0x400],*destp;
	int i;
	BOOL ahres = FALSE;

	destp = buf;
	if ( use == URI_URL ){
		const char *sptr,*newstart = start;

		if ( *start == ':' ) start++; // URLでない : と思われる
		for ( sptr = start ; sptr < end ; sptr++ ){ // 記号を除去
			if ( *sptr == ':') break;
			if ( !IslowerA(*sptr) && (strchr(".-_%#?=&/\\",*sptr) == NULL) ){
				newstart = sptr + 1;
			}
		}
		if ( *sptr == ':' ){
			start = newstart;
			if ( (sptr >= (start + 3)) && !memcmp(sptr - 3,"ttp",3) ){
				start = sptr + 1;
				sptr = NULL;
			}
			if ( (sptr >= (start + 4)) && !memcmp(sptr - 4,"ttps",4) ){
				start = sptr - 4;
				*destp++ = 'h';
			}
		}else{
			if ( vo_.file.source[0] == '\0' ) sptr = NULL;
		}

		if ( sptr == NULL ){
			strcpy(destp,"http:");
			destp += 5;

			if ( (vo_.file.source[0] == '\0') && (*start != '/') ){
				*destp++ = '/';
				*destp++ = '/';
			}
		}
	}
	if ( start >= end ) return FALSE;

	if ( use == URI_PATH ){
		if ( *end != '\"' ) return FALSE;
	}

	{
		const char *ptr;

		ptr = start;
		if ( (bottom < ptr) && (*(ptr - 1) == '\"') ) ptr--;
		while ( (bottom < ptr) && (*(ptr - 1) == ' ') ) ptr--;
		if ( (bottom < ptr) && (*(ptr - 1) == '=') ){
			ptr--;
			while ( (bottom < ptr) && (*(ptr - 1) == ' ') ) ptr--;
			if ( ((bottom + 3) < ptr) && (memicmp(ptr - 4,"href",4) == 0) ){
				ahres = TRUE;
			}
		}
	}
												// URI 候補のきりだし
	while ( (start < end) && ((size_t)(destp - buf) < (sizeof(buf) - 1)) ){
		if ( (*start == '\r') || (*start == '\n') ){
			start++;
			continue;
		}
		*destp++ = *start++;
	}
	while ( (destp > buf) && (strchr(">)]",*(destp - 1)) != NULL) ) destp--;
	*destp = '\0';

	if ( use == URI_URL ){	// 「:」
		destp = buf;
		if ( *destp == ':' ) return FALSE;
		if ( *destp == '#' ){
			destp++;
			for ( ; *destp ; destp++ ){
				if ( *destp == ':' ) break;
				if ( !IsdigitA(*destp) ) return FALSE;
			}
		}else if ( strchr(destp,':') != NULL ){
			for ( ; *destp ; destp++ ){
				if ( *destp == ':' ) break;
				if ( !IslowerA(*destp) ) return FALSE;
			}
		}
		if ( (*destp == ':') && !*(destp + 1) ) return FALSE;
	}else if ( use == URI_MAIL ){	// 「@」
		if ( strchr(buf,'$') ) return FALSE;
		if ( *(destp - 1) == '@' ) return FALSE;
	}else if ( use == URI_PATH ){
//		if ( buf[0] != '#' )
		if ( strchr(buf,'/') && !strchr(buf,'\\') && !strchr(buf,'.') ){
			return FALSE;
		}
	}
	if ( ((strchr(buf,'/') != NULL) || (ahres && (buf[0] != '#'))) &&
		 (vo_.file.source[0] != '\0') ){
		#ifdef UNICODE
			TCHAR name[VFPS];

			AnsiToUnicode(buf,name,VFPS);
			VFSFullPath(NULL,name,vo_.file.source);
			UnicodeToAnsi(name,buf,VFPS);
		#else
			VFSFullPath(NULL,buf,vo_.file.source);
		#endif
	}

	for ( i = MENUID_URI ; i <= *index ; i++ ){
		GetMenuStringA(hMenu,i,tmp,TSIZEOF(tmp),MF_BYCOMMAND);
		if ( strcmp(tmp,buf) == 0 ){
			i = -1;
			break;
		}
	}
	if ( i >= 0 ) AppendMenuA(hMenu,MF_CHKES(ahres),(*index)++,buf);
	return TRUE;
}

void URIDUMP(const char *bottom,const char *top,const TCHAR *title)
{
	TCHAR buf[0x800];

	if ( (top - bottom) > 900 ) top = bottom + 900;
	#ifdef UNICODE
	{
		int len;

		len = (int)MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,bottom,top - bottom,buf,0x800);
		buf[len] = '\0';
	}
	#else
		memcpy(buf,bottom,top - bottom);
		buf[top - bottom] = '\0';
	#endif
	XMessage(NULL,NULL,XM_DbgLOG,T("%s %s"),title,buf);
}

void MakeURIs(HMENU hMenu,int index)
{
	char *p,*start = NULL;
	char *textbottom,*texttop,*maxptr;
	int use = URI_NO,top;

	if ( vo_.file.source[0] != '\0' ){
		AppendMenu(hMenu,MF_ES,index++,vo_.file.source);
	}

	if ( VOsel.select != FALSE ){	// 選択中
		top = VOsel.top.y.line + 1;
		if ( top > VOi->line ) top = VOi->line;
		texttop = (char *)VOi->ti[top].ptr; // 下端
		textbottom = (char *)VOi->ti[VOsel.bottom.y.line].ptr; // 上端
	}else{ // 非選択、カーソル使用中
		if ( VOsel.cursor == FALSE ) return;
		textbottom = (char *)VOi->ti[VOsel.now.y.line].ptr;
		texttop = (char *)VOi->ti[VOsel.now.y.line + 1].ptr;
	}
	maxptr = (char *)VOi->ti[VOi->cline].ptr;

	if ( GetAsyncKeyState(0xF0 /*VK_CAPITAL*/) & KEYSTATE_PUSH ){
		XMessage(NULL,NULL,XM_DbgLOG,T("MakeURI bottom:%x top:%x cline:%d max:%x  Base:%s"),textbottom,texttop,VOi->cline,maxptr,vo_.file.source);
		URIDUMP(textbottom,texttop,T("top"));
		URIDUMP(textbottom,texttop,T("max"));
	}

	for ( p = textbottom ; p < maxptr ; ){
		UCHAR code;

		code = *p;
		if ( (code == 0xd) || (code == 0xa) ){ // 改行後も連結する
			char *skipp;

			p++;
			if ( (code == 0xd) || (code == 0xa) ) p++;
			skipp = p;
			while ( skipp < maxptr ){
				char c;

				c = *skipp;
				if ( Isalnum(c) || (strchr("\\/?%#.~",c) != NULL) ){
					skipp++;
					continue;
				}
				break;
			}
			if ( skipp != p ){ // 改行後も連結可能
				if ( use != URI_NO ){
					MakeURIs2(hMenu,&index,textbottom,start,p,use);
					if ( index > MENUID_URIMAX ) break;
					MakeURIs2(hMenu,&index,textbottom,start,skipp,use);
					use = URI_NO;
					if ( index > MENUID_URIMAX ) break;
					start = NULL;
				}
			}else{ // 改行で終わり
				if ( use != URI_NO ){
					MakeURIs2(hMenu,&index,textbottom,start,p,use);
					use = URI_NO;
					if ( index > MENUID_URIMAX ) break;
				}
				if ( p >= texttop ) break;
				start = NULL;
				use = URI_NO;
			}
			continue;
		}
		if ( (code > (UCHAR)' ') && (code < (UCHAR)0x7f ) &&
			 (strchr("\"<>|",*p) == NULL) ){ // 該当文字( ASCII 内で "<>| 除く)
			if ( start == NULL ){
				if ( strchr("([{",*p) == NULL ){ // 括弧でなければ検索開始地点
					start = p;
/*
					if ( (p > textbottom) && (*(p - 1) == '\"') ){
						use = URI_PATH;
					}
*/
				}
			}else{
				if ( *p == '\\' ) use = URI_PATH;
				if ( *p == ':' ) use = URI_URL;
				if ( *p == '/' ) use = URI_URL;
				if ( *p == '@' ) use = URI_MAIL;
			}
		}else{
			if ( use != URI_NO ){
				MakeURIs2(hMenu,&index,textbottom,start,p,use);
				use = URI_NO;
				if ( index > MENUID_URIMAX ) break;
			}
			if ( p >= texttop ) break;
			start = NULL;
			use = URI_NO;
#ifndef UNICODE
			if (IskanjiA(*p) && *(p + 1)) p++;
#endif
		}
		p++;
	}
	if ( use != URI_NO ) MakeURIs2(hMenu,&index,textbottom,start,p,use);
}
