/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library							Άρμ
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H Μ DLL θ`wθ
#include "WINAPI.H"
#include "PPX.H"
#include "PPD_DEF.H"
#pragma hdrstop

#ifndef UNICODE
const BYTE ZHtbl[] =
	"@Ihfij{C|D^"	// 2x 000
	"OPQRSTUVWXFGH"	// 3x 020
	"`abcdefghijklmn"	// 4x 040
	"opqrstuvwxymnOQ"	// 5x 080
	"e"	// 6x 0a0
	"obp`¬"	// 7x 0c0
	"¬BuvAE@BDFHb"	// ax 0e0
	"[ACEGIJLNPRTVXZ\"	// bx 100
	"^`cegijklmnqtwz}"	// cx 120
	"~JK"	// dx 140

	"KMOQSUWY[]_adfho"	// etc 160
	"rux{psvy|";				//     180-191
#else
const WCHAR ZHtbl[] = L"BuvAE@BDFHb[ACEGIJLNPRTVXZ\^`cegijklmnqtwz}~JKKMOQSUWY[]_adfhorux{psvy|";
#endif

// 1bytes Ά ¨ 2bytes ΆΟ· ---------------------------------------------
PPXDLL TCHAR * PPXAPI Strsd(TCHAR *dststr, const TCHAR *srcstr)
{
	const UTCHAR *src;
	UTCHAR *dst, code;

	src = (const UTCHAR *)srcstr;
	dst = (UTCHAR *)dststr;

	for ( ; (code = *src) != '\0' ; src++ ){
		#ifdef UNICODE
		if ( code == L' ' ){
			*dst++ = L'@';
			continue;
		}
		if ( code == L'\\' ){
			*dst++ = L'';
			continue;
		}
		if ( (code >= L'!') && (code <= L'~') ){
			*dst++ = (UTCHAR)(*src + (L'I' - L'!'));
			continue;
		}
		if ( (code >= L'‘') && (code <= L'ί') ){
			*dst = (UTCHAR)ZHtbl[code - L'‘'];
			if ( *(src + 1) == L'ή' ){
				if ( (code >= L'Ά') && (code <= L'Δ') ){
					src++;
					(*dst)++;
				}else if ( (code >= L'Κ') && (code <= L'Ξ') ){
					src++;
					(*dst)++;
				}else if (code == L'³' ){
					src++;
					*dst = L'';
				}
			}else if ( (*(src + 1) == L'ί') &&
						(code >= L'Κ') && (code <= L'Ξ') ){
				src++;
				*dst += (UTCHAR)2;
			}
			dst++;
			continue;
		}
		#else
		const BYTE *p;

		if ( (code >= 0x20) && (code < 0x80) ){
			p = &ZHtbl[(code - 0x20) << 1];
			*dst++ = *p++;
			*dst++ = *p;
			continue;
		}
		if ( (code >= 0xa0) && (code < 0xe0) ){

			p = &ZHtbl[(code - 0x20 - 0x20) << 1];
			if ( *(src + 1) == (BYTE)'ή' ){
				if ( (code >= (BYTE)'Ά') && (code <= (BYTE)'Δ') ){
					src++;
					p += 0x2a * 2;
				}else if ( (code >= (BYTE)'Κ') && (code <= (BYTE)'Ξ') ){
					src++;
					p += (0x2a - 5) * 2;
				}else if (code == (BYTE)'³' ){
					src++;
					p = (BYTE *)"";
				}
			}else if ( (*(src + 1) == (BYTE)'ί') &&
						(code >= (BYTE)'Κ') && (code <= (BYTE)'Ξ') ){
				src++;
				p += 0x2a * 2;
			}
			*dst++ = *p++;
			*dst++ = *p;
			continue;
		}
		if ( IskanjiA(code) && *(src + 1) ) *dst++ = *src++;
		#endif
		*dst++ = *src;
	}
	*dst = '\0';
	return (TCHAR *)dst;
}

// 2bytes Ά ¨ 1bytes ΆΟ· ---------------------------------------------
PPXDLL TCHAR * PPXAPI Strds(TCHAR *dststr, const TCHAR *srcstr)
{
	const UTCHAR *src;
	UTCHAR *dst;

	src = (const UTCHAR *)srcstr;
	dst = (UTCHAR *)dststr;
	for ( ; *src ; src++ ){
		#ifdef UNICODE
		if ( *src == L'[' ){
			*dst++ = L'°';
			continue;
		}
		if ( *src == L'E' ){
			*dst++ = L'₯';
			continue;
		}
		if ( *src == L'' ){
			*dst++ = L'\\';
			continue;
		}
		if ( (*src >= L'I') && (*src <= L'`') ){
			*dst++ = (UTCHAR)(*src - (L'I' - L'!'));
			continue;
		}
		if ( (*src >= L'@') && (*src <= L'B') ){
			if ( *src == L'@' ){
				*dst++ = L' ';
			}else{
				*dst++ = *src == L'B' ? L'‘' : L'€';
			}
			continue;
		}
		if ( (*src >= L'u') && (*src <= L'v') ){
			*dst++ = (WCHAR)(*src + (L'’' - L'u'));
			continue;
		}

		if ( (*src >= L'J') && (*src <= L'') ){
			WCHAR c, off;
			const WCHAR *p;

			if ( *src == L'' ){
				*dst++ = L'³';
				*dst++ = L'ή';
				continue;
			}

			c = *src;
			for ( p = ZHtbl ; *p ; p++ ){
				if ( *p != c ) continue;
				off = (WCHAR)(p - ZHtbl);
				if ( off < 0x3f ){
					c = (WCHAR)(L'‘' + off);
					break;
				}
				if ( off < (0x3f + 20) ){
					if ( off >= (0x3f + 15) ) off += (WCHAR)5;
					*dst++ = (WCHAR)(L'‘' + off - 0x3f + 21);
					c = L'ή';
					break;
				}
				*dst++ = (WCHAR)(L'‘' + off - 0x3f -20 + 41);
				c = L'ί';
				break;
			}
			*dst++ = c;
			continue;
		}
		#else
		const BYTE *p;
		BYTE c1, c2;

		if ( IskanjiA(*src) ){
			if ( (*src >= 0x81) && (*src <= 0x83) ){
				c1 = *src;
				c2 = *(src + 1);
				for( p = ZHtbl ; *p ; p += 2){
					if ( (*(p + 1) == c2 ) && (*p == c1) ){
						c1 = (BYTE)(((p - ZHtbl) >> 1) + 0x20);
						c2 = 0;
						if ( c1 >= (BYTE)0x80 ) c1 += (BYTE)0x20;
						if ( c1 >= 0xe0 ){
							if ( c1 < 0xf4 ){
								c1 -= (c1 < 0xef) ? (BYTE)0x2a : (BYTE)0x25;
								c2 = 'ή';
							}else{
								c1 -= (BYTE)0x2a;
								c2 = 'ί';
							}
						}
						src++;
						*dst++ = c1;
						if (c2) *dst++ = c2;
						goto next;
					}
				}
				if ( (c1 == (BYTE)0x83) && (c2 == (BYTE)0x94) ){ // 
					src++;
					*dst++ = '³';
					*dst++ = 'ή';
					goto next;
				}
			}
			*dst++ = *src++;
		}
		#endif
		*dst++ = *src;
#ifndef UNICODE
next:
;	// cl.exe ΝK{H
#endif
	}
	*dst = 0;
	return (TCHAR *)dst;
}

// ΏΞ¬Ά» -----------------------------------------------------------
PPXDLL TCHAR * PPXAPI Strlwr(TCHAR *str)
{
	TCHAR *ptr, type;

	for ( ptr = str ; *ptr ; ptr++){
	#ifdef UNICODE
		type = *ptr;
		if ( (type >= 'A') && (type <= 'Z') ){
			*ptr += (TCHAR)('a' - 'A');
			continue;
		}
		if ( (type >= L'`') && (type <= L'y') ){
			*ptr += (TCHAR)(L'' - L'`');
			continue;
		}
	#else
		type = T_CHRTYPE[(unsigned char)(*ptr)];
		if ( type & T_IS_KNJ ){			// ΏΜκΜ
			unsigned char code;

			code = *(unsigned char *)ptr++;
			if ( !code ) break;
			if ( (code == 0x82) &&	((unsigned char)*ptr >= 0x60) &&
									((unsigned char)*ptr <= 0x79) ){
				*ptr += (char)0x21;
			}
		}else{
			if (type & T_IS_UPP) *ptr += (char)0x20;
		}
	#endif
	}
	return ptr;
}

// ΏΞεΆ» -----------------------------------------------------------
PPXDLL TCHAR * PPXAPI Strupr(TCHAR *str)
{
	TCHAR *ptr, type;

	for ( ptr = str ; *ptr ; ptr++){
	#ifdef UNICODE
		type = *ptr;
		if ( (type >= 'a') && (type <= 'z') ){
			*ptr -= (TCHAR)('a' - 'A');
			continue;
		}
		if ( (type >= L'') && (type <= L'') ){
			*ptr -= (TCHAR)(L'' - L'`');
			continue;
		}
	#else
		type = T_CHRTYPE[(unsigned char)(*ptr)];
		if ( type & T_IS_KNJ ){			// ΏΜκΜ
			unsigned char code;

			code = *(unsigned char *)ptr++;
			if ( !code ) break;
			if ( (code == 0x82) &&	((unsigned char)*ptr >= 0x81) &&
									((unsigned char)*ptr <= 0x9A) ){
				*ptr -= (char)0x21;
			}
		}else{
			if (type & T_IS_LOW) *ptr -= (char)0x20;
		}
	#endif
	}
	return ptr;
}

#ifndef UNICODE
/*-----------------------------
PΆOΜSp^ΌpΜ»f(UNICODEΕΝTQ[gπ³΅ΔA}NΕ 1 πΤ·)
-----------------------------*/
int bchrlen(char *str, int off)
{
	int size = 0;
	char *max_str;

	max_str = str + off;

	while( (str < max_str) && ((size = Chrlen(*str)) != 0) ) str += size;
	return size;
}

/*-----------------------------
	"|"ΜΚuπT·
-----------------------------*/
char *SearchPipe(const char *str)
{
	char type;

	for ( ;; ){
		type = *str;
		if ( type == '|' ) return (char *)str;
		type = (char)Chrlen(type);
		if ( !type ) return NULL;
		str += type;
	}
}
#endif

/*-----------------------------------------------------------------------------
	σπΗέςΞ·(«·¦Lθ)
-----------------------------------------------------------------------------*/
TCHAR *SkipSpaceAndFix(TCHAR *p)
{
	while(*p){
		if ( *p == ' ' ){
			p++;
			continue;
		}
		if ( *p == '\t' ){
			*p++ = ' ';
			continue;
		}
		if ( (*p == '\r') || (*p == '\n') ){
			*p = '\0';
			if ( (*(p+1) == '\r') || (*(p+1) == '\n') ) p++;
		}
		break;
	}
	return p;
}

TCHAR *tstristr(const TCHAR *target, const TCHAR *findstr)
{
	size_t len, flen;
	const TCHAR *p, *maxptr;

	flen = tstrlen(findstr);
	len = tstrlen(target);
	maxptr = target + len - flen;

#ifdef UNICODE
	for ( p = target ; p <= maxptr ; p++ ){
#else
	for ( p = target ; p <= maxptr ; p += Chrlen(*p) ){
#endif
		if ( !tstrnicmp(p, findstr, flen) ){
			return (TCHAR *)p;
		}
	}
	return NULL;
}

WCHAR *stpcpyW(WCHAR *deststr, const WCHAR *srcstr)
{
	WCHAR *destptr = deststr;
	const WCHAR *srcptr = srcstr;

	for(;;){
		WCHAR code;

		code = *srcptr;
		*destptr = code;
		if ( code == '\0' ) return destptr;
		srcptr++;
		destptr++;
	}
}

#ifndef __BORLANDC__
char *stpcpyA(char *deststr, const char *srcstr)
{
	char *destptr = deststr;
	const char *srcptr = srcstr;

	for(;;){
		char code;

		code = *srcptr;
		*destptr = code;
		if ( code == '\0' ) return destptr;
		srcptr++;
		destptr++;
	}
}
#endif

#ifdef WINEGCC
WCHAR *strchrW(const WCHAR *text, WCHAR findchar)
{
	const WCHAR *ptr;

	ptr = text;
	while ( *ptr != '\0' ){
		if ( *ptr == findchar ) return (WCHAR *)ptr;
		ptr++;
	}
	return NULL;
}

WCHAR *strrchrW(const WCHAR *text, WCHAR findchar)
{
	const WCHAR *ptr;

	ptr = text + strlenW(text);
	while ( ptr > text ){
		ptr--;
		if ( *ptr == findchar ) return (WCHAR *)ptr;
	}
	return NULL;
}

WCHAR *strstrW(const WCHAR *text1, const WCHAR *text2)
{
	size_t len1, len2;
	const WCHAR *ptr;

	len1 = strlenW(text1);
	len2 = strlenW(text2);
	if ( len1 < len2 ) return NULL;
	len1 -= (len2 - 1);
	ptr = text1;
	while ( len1 ){
		if ( memcmp(ptr, text2, len2 * sizeof(WCHAR)) == 0 ) return (WCHAR *)ptr;
		ptr++;
		len1--;
	}
	return NULL;
}
#endif

char *strlimcpy(char *deststr, const char *srcstr, size_t maxlength)
{
	char code;
	const char *srcmax;

	srcmax = srcstr + maxlength - 1;
	while( srcstr < srcmax ){
		code = *srcstr++;
		if ( code == '\0' ) break;
		*deststr++ = code;
	}
	*deststr = '\0';
	return deststr;
}
#ifdef UNICODE
WCHAR *wcslimcpy(WCHAR *deststr, const WCHAR *srcstr, size_t maxlength)
{
	WCHAR code;
	const WCHAR *srcmax;

	srcmax = srcstr + maxlength - 1;
	while( srcstr < srcmax ){
		code = *srcstr++;
		if ( code == '\0' ) break;
		*deststr++ = code;
	}
	*deststr = '\0';
	return deststr;
}
#endif

void tstrreplace(TCHAR *text, const TCHAR *targetword, const TCHAR *replaceword)
{
	TCHAR *p;
	size_t tlen, rlen;

	if ( (p = tstrstr(text, targetword)) == NULL ) return;
	tlen = tstrlen(targetword);
	rlen = tstrlen(replaceword);

	for (;;){
		if ( tlen != rlen ) memmove(p + rlen, p + tlen, TSTRSIZE(p + tlen));
		memcpy(p, replaceword, TSTROFF(rlen));
		text = p + rlen;
		if ( (p = tstrstr(text, targetword)) != NULL ) continue;
		break;
	}
}

const TCHAR *EscapeMacrochar(const TCHAR *string, TCHAR *buf)
{
	const TCHAR *src;

	src = tstrchr(string, '%');
	if ( src == NULL ) return string;
	tstrcpy(buf, string);
	tstrreplace(buf, T("%"), T("%%"));
	return buf;
}

