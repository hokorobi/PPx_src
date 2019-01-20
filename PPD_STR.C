/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library							ï∂éöóÒëÄçÏ
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H ÇÃ DLL íËã`éwíË
#include "WINAPI.H"
#include "PPX.H"
#include "PPD_DEF.H"
#pragma hdrstop

#ifndef UNICODE
const BYTE ZHtbl[] =
	"Å@ÅIÅhÅîÅêÅìÅïÅfÅiÅjÅñÅ{ÅCÅ|ÅDÅ^"	// 2x 000
	"ÇOÇPÇQÇRÇSÇTÇUÇVÇWÇXÅFÅGÅÉÅÅÅÑÅH"	// 3x 020
	"ÅóÇ`ÇaÇbÇcÇdÇeÇfÇgÇhÇiÇjÇkÇlÇmÇn"	// 4x 040
	"ÇoÇpÇqÇrÇsÇtÇuÇvÇwÇxÇyÅmÅèÅnÅOÅQ"	// 5x 080
	"ÅeÇÅÇÇÇÉÇÑÇÖÇÜÇáÇàÇâÇäÇãÇåÇçÇéÇè"	// 6x 0a0
	"ÇêÇëÇíÇìÇîÇïÇñÇóÇòÇôÇöÅoÅbÅpÅ`Å¨"	// 7x 0c0
	"Å¨ÅBÅuÅvÅAÅEÉíÉ@ÉBÉDÉFÉHÉÉÉÖÉáÉb"	// ax 0e0
	"Å[ÉAÉCÉEÉGÉIÉJÉLÉNÉPÉRÉTÉVÉXÉZÉ\"	// bx 100
	"É^É`ÉcÉeÉgÉiÉjÉkÉlÉmÉnÉqÉtÉwÉzÉ}"	// cx 120
	"É~ÉÄÉÅÉÇÉÑÉÜÉàÉâÉäÉãÉåÉçÉèÉìÅJÅK"	// dx 140

	"ÉKÉMÉOÉQÉSÉUÉWÉYÉ[É]É_ÉaÉdÉfÉhÉo"	// etc 160
	"ÉrÉuÉxÉ{ÉpÉsÉvÉyÉ|";				//     180-191
#else
const WCHAR ZHtbl[] = L"ÅBÅuÅvÅAÅEÉíÉ@ÉBÉDÉFÉHÉÉÉÖÉáÉbÅ[ÉAÉCÉEÉGÉIÉJÉLÉNÉPÉRÉTÉVÉXÉZÉ\É^É`ÉcÉeÉgÉiÉjÉkÉlÉmÉnÉqÉtÉwÉzÉ}É~ÉÄÉÅÉÇÉÑÉÜÉàÉâÉäÉãÉåÉçÉèÉìÅJÅKÉKÉMÉOÉQÉSÉUÉWÉYÉ[É]É_ÉaÉdÉfÉhÉoÉrÉuÉxÉ{ÉpÉsÉvÉyÉ|";
#endif

// 1bytes ï∂éö Å® 2bytes ï∂éöïœä∑ ---------------------------------------------
PPXDLL TCHAR * PPXAPI Strsd(TCHAR *dststr,const TCHAR *srcstr)
{
	const UTCHAR *src;
	UTCHAR *dst,code;

	src = (const UTCHAR *)srcstr;
	dst = (UTCHAR *)dststr;

	for ( ; (code = *src) != '\0' ; src++ ){
		#ifdef UNICODE
		if ( code == L' ' ){
			*dst++ = L'Å@';
			continue;
		}
		if ( code == L'\\' ){
			*dst++ = L'Åè';
			continue;
		}
		if ( (code >= L'!') && (code <= L'~') ){
			*dst++ = (UTCHAR)(*src + (L'ÅI' - L'!'));
			continue;
		}
		if ( (code >= L'°') && (code <= L'ﬂ') ){
			*dst = (UTCHAR)ZHtbl[code - L'°'];
			if ( *(src + 1) == L'ﬁ' ){
				if ( (code >= L'∂') && (code <= L'ƒ') ){
					src++;
					(*dst)++;
				}else if ( (code >= L' ') && (code <= L'Œ') ){
					src++;
					(*dst)++;
				}else if (code == L'≥' ){
					src++;
					*dst = L'Éî';
				}
			}else if ( (*(src + 1) == L'ﬂ') &&
						(code >= L' ') && (code <= L'Œ') ){
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
			if ( *(src + 1) == (BYTE)'ﬁ' ){
				if ( (code >= (BYTE)'∂') && (code <= (BYTE)'ƒ') ){
					src++;
					p += 0x2a * 2;
				}else if ( (code >= (BYTE)' ') && (code <= (BYTE)'Œ') ){
					src++;
					p += (0x2a - 5) * 2;
				}else if (code == (BYTE)'≥' ){
					src++;
					p = (BYTE *)"Éî";
				}
			}else if ( (*(src + 1) == (BYTE)'ﬂ') &&
						(code >= (BYTE)' ') && (code <= (BYTE)'Œ') ){
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

// 2bytes ï∂éö Å® 1bytes ï∂éöïœä∑ ---------------------------------------------
PPXDLL TCHAR * PPXAPI Strds(TCHAR *dststr,const TCHAR *srcstr)
{
	const UTCHAR *src;
	UTCHAR *dst;

	src = (const UTCHAR *)srcstr;
	dst = (UTCHAR *)dststr;
	for ( ; *src ; src++ ){
		#ifdef UNICODE
		if ( *src == L'Å[' ){
			*dst++ = L'∞';
			continue;
		}
		if ( *src == L'ÅE' ){
			*dst++ = L'•';
			continue;
		}
		if ( *src == L'Åè' ){
			*dst++ = L'\\';
			continue;
		}
		if ( (*src >= L'ÅI') && (*src <= L'Å`') ){
			*dst++ = (UTCHAR)(*src - (L'ÅI' - L'!'));
			continue;
		}
		if ( (*src >= L'Å@') && (*src <= L'ÅB') ){
			if ( *src == L'Å@' ){
				*dst++ = L' ';
			}else{
				*dst++ = *src == L'ÅB' ? L'°' : L'§';
			}
			continue;
		}
		if ( (*src >= L'Åu') && (*src <= L'Åv') ){
			*dst++ = (WCHAR)(*src + (L'¢' - L'Åu'));
			continue;
		}

		if ( (*src >= L'ÅJ') && (*src <= L'Éî') ){
			WCHAR c,off;
			const WCHAR *p;

			if ( *src == L'Éî' ){
				*dst++ = L'≥';
				*dst++ = L'ﬁ';
				continue;
			}

			c = *src;
			for ( p = ZHtbl ; *p ; p++ ){
				if ( *p != c ) continue;
				off = (WCHAR)(p - ZHtbl);
				if ( off < 0x3f ){
					c = (WCHAR)(L'°' + off);
					break;
				}
				if ( off < (0x3f + 20) ){
					if ( off >= (0x3f + 15) ) off += (WCHAR)5;
					*dst++ = (WCHAR)(L'°' + off - 0x3f + 21);
					c = L'ﬁ';
					break;
				}
				*dst++ = (WCHAR)(L'°' + off - 0x3f -20 + 41);
				c = L'ﬂ';
				break;
			}
			*dst++ = c;
			continue;
		}
		#else
		const BYTE *p;
		BYTE c1,c2;

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
								c2 = 'ﬁ';
							}else{
								c1 -= (BYTE)0x2a;
								c2 = 'ﬂ';
							}
						}
						src++;
						*dst++ = c1;
						if (c2) *dst++ = c2;
						goto next;
					}
				}
				if ( (c1 == (BYTE)0x83) && (c2 == (BYTE)0x94) ){ // Éî
					src++;
					*dst++ = '≥';
					*dst++ = 'ﬁ';
					goto next;
				}
			}
			*dst++ = *src++;
		}
		#endif
		*dst++ = *src;
#ifndef UNICODE
next:
;	// cl.exe ÇÕïKê{ÅH
#endif
	}
	*dst = 0;
	return (TCHAR *)dst;
}

// äøéöëŒâûè¨ï∂éöâª -----------------------------------------------------------
PPXDLL TCHAR * PPXAPI Strlwr(TCHAR *str)
{
	TCHAR *ptr,type;

	for ( ptr = str ; *ptr ; ptr++){
	#ifdef UNICODE
		type = *ptr;
		if ( (type >= 'A') && (type <= 'Z') ){
			*ptr += (TCHAR)('a' - 'A');
			continue;
		}
		if ( (type >= L'Ç`') && (type <= L'Çy') ){
			*ptr += (TCHAR)(L'ÇÅ' - L'Ç`');
			continue;
		}
	#else
		type = T_CHRTYPE[(unsigned char)(*ptr)];
		if ( type & T_IS_KNJ ){			// äøéöÇÃèÍçáÇÃèàóù
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

// äøéöëŒâûëÂï∂éöâª -----------------------------------------------------------
PPXDLL TCHAR * PPXAPI Strupr(TCHAR *str)
{
	TCHAR *ptr,type;

	for ( ptr = str ; *ptr ; ptr++){
	#ifdef UNICODE
		type = *ptr;
		if ( (type >= 'a') && (type <= 'z') ){
			*ptr -= (TCHAR)('a' - 'A');
			continue;
		}
		if ( (type >= L'ÇÅ') && (type <= L'Çö') ){
			*ptr -= (TCHAR)(L'ÇÅ' - L'Ç`');
			continue;
		}
	#else
		type = T_CHRTYPE[(unsigned char)(*ptr)];
		if ( type & T_IS_KNJ ){			// äøéöÇÃèÍçáÇÃèàóù
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
ÇPï∂éöëOÇÃëSäpÅ^îºäpÇÃîªíf(UNICODEî≈ÇÕÉTÉçÉQÅ[ÉgÇñ≥éãÇµÇƒÅAÉ}ÉNÉçÇ≈ 1 Çï‘Ç∑)
-----------------------------*/
int bchrlen(char *str,int off)
{
	int size = 0;
	char *max_str;

	max_str = str + off;

	while( (str < max_str) && ((size = Chrlen(*str)) != 0) ) str += size;
	return size;
}

/*-----------------------------
	"|"ÇÃà íuÇíTÇ∑
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
	ãÛîíÇì«Ç›îÚÇŒÇ∑(èëÇ´ä∑Ç¶óLÇË)
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

TCHAR *tstristr(const TCHAR *target,const TCHAR *findstr)
{
	size_t len,flen;
	const TCHAR *p,*max;

	flen = tstrlen(findstr);
	len = tstrlen(target);
	max = target + len - flen;

#ifdef UNICODE
	for ( p = target ; p <= max ; p++ ){
#else
	for ( p = target ; p <= max ; p += Chrlen(*p) ){
#endif
		if ( !tstrnicmp(p,findstr,flen) ){
			return (TCHAR *)p;
		}
	}
	return NULL;
}

#ifdef WINEGCC
WCHAR *strchrW(const WCHAR *text,WCHAR findchar)
{
	const WCHAR *ptr;

	ptr = text;
	while ( *ptr != '\0' ){
		if ( *ptr == findchar ) return (WCHAR *)ptr;
		ptr++;
	}
	return NULL;
}

WCHAR *strrchrW(const WCHAR *text,WCHAR findchar)
{
	const WCHAR *ptr;

	ptr = text + strlenW(text);
	while ( ptr > text ){
		ptr--;
		if ( *ptr == findchar ) return (WCHAR *)ptr;
	}
	return NULL;
}

WCHAR *strstrW(const WCHAR *text1,const WCHAR *text2)
{
	size_t len1,len2;
	const WCHAR *ptr;

	len1 = strlenW(text1);
	len2 = strlenW(text2);
	if ( len1 < len2 ) return NULL;
	len1 -= (len2 - 1);
	ptr = text1;
	while ( len1 ){
		if ( memcmp(ptr,text2,len2 * sizeof(WCHAR)) == 0 ) return (WCHAR *)ptr;
		ptr++;
		len1--;
	}
	return NULL;
}
#endif

char *strlimcpy(char *deststr,const char *srcstr,size_t maxlength)
{
	size_t len;

	len = strlen(srcstr);
	if ( len >= maxlength ){
		char *last;

		memcpy(deststr,srcstr,maxlength - 1);
		last = deststr + (maxlength - 1);
		*last = '\0';
		return last;
	}else{
		memcpy(deststr,srcstr,len + 1);
		return deststr + len;
	}
}
#ifdef UNICODE
WCHAR *wcslimcpy(WCHAR *deststr,const WCHAR *srcstr,size_t maxlength)
{
	size_t len;

	len = strlenW(srcstr);
	if ( len >= maxlength ){
		WCHAR *last;

		memcpy(deststr,srcstr,(maxlength - 1) * sizeof(WCHAR));
		last = deststr + (maxlength - 1);
		*last = '\0';
		return last;
	}else{
		memcpy(deststr,srcstr,(len + 1) * sizeof(WCHAR));
		return deststr + len;
	}
}
#endif

void tstrreplace(TCHAR *text,const TCHAR *targetword,const TCHAR *replaceword)
{
	TCHAR *p;

	while ( (p = tstrstr(text,targetword)) != NULL ){
		int tlen = tstrlen32(targetword);
		int rlen = tstrlen32(replaceword);

		if ( tlen != rlen ) memmove(p + rlen,p + tlen,TSTRSIZE(p + tlen));
		memcpy(p,replaceword,TSTROFF(rlen));
		text = p + rlen;
	}
}

const TCHAR *EscapeMacrochar(const TCHAR *string,TCHAR *buf)
{
	const TCHAR *src;

	src = tstrchr(string,'%');
	if ( src == NULL ) return string;
	tstrcpy(buf,string);
	tstrreplace(buf,T("%"),T("%%"));
	return buf;
}

