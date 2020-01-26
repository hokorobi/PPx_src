/*-----------------------------------------------------------------------------
	Paper Plane xUI	 commom library		コマンドライン解析
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include "PPX.H"
#include "PPD_DEF.H"
#pragma hdrstop

/*-----------------------------------------------------------------------------
	行末(\0,\a,\d)か？

RET:	==0 :EOL	!=0 :Code
-----------------------------------------------------------------------------*/
UTCHAR USEFASTCALL IsEOL(const TCHAR **str)
{
	UTCHAR code;

	code = **str;
	if ( code == '\0' ) return '\0';
	if ( code == '\xd' ){
		if ( *(*str + 1) == '\xa' ) (*str)++;
		return '\0';
	}
	if ( code == '\xa' ){
		if ( *(*str + 1) == '\xd' ) (*str)++;
		return '\0';
	}
	return code;
}
/*-----------------------------------------------------------------------------
	空白(space, tab）をスキップする

RET:	==0 :EOL	!=0 :Code
-----------------------------------------------------------------------------*/
PPXDLL UTCHAR PPXAPI SkipSpace(LPCTSTR *str)
{
	UTCHAR code;

	for ( ; ; ){
		code = IsEOL(str);
		if ( (code != ' ') && (code != '\t') ) break;
		(*str)++;
	}
	return code;
}

PPXDLL BOOL PPXAPI NextParameter(LPCTSTR *str)
{
	UTCHAR code;

	if ( IsEOL(str) == '\0' ) return FALSE;
	code = SkipSpace(str);
	if ( code != ',' ){ // 空白のみの区切り？
		return (code != '\0');
	}
	(*str)++;
	SkipSpace(str);
	return TRUE;
}

/*-----------------------------------------------------------------------------
	一つ分のパラメータを抽出する。先頭と末尾の空白は除去する

RET:	先頭の文字(何もなかったら 0)
-----------------------------------------------------------------------------*/
PPXDLL UTCHAR PPXAPI GetLineParam(LPCTSTR *str, TCHAR *param)
{
	UTCHAR code, bottom;
	const TCHAR *src;
	TCHAR *dst, *dstmax;

	src = *str;
	dst = param;
	dstmax = dst + CMDLINESIZE - 1;
	bottom = code = SkipSpace(&src);
	if ( code == '\0' ){
		*str = src;
		*dst = '\0';
		return '\0';
	}
	if ( code == '\"' ){
		GetQuotedParameter(&src, dst, dstmax);
	}else{
		do {
			src++;
			if ( code <= ' ' )	break;
			if ( code == '\"' ){
				*dst++ = code;
				while( '\0' != (code = IsEOL(&src)) ){
					src++;
					*dst++ = code;
					if ( (code == '\"') && (*src != '\"') ) break;
					if ( dst >= dstmax ) break;
				}
			}else{
				*dst++ = code;
			}
			if ( dst >= dstmax ) break;
		}while( '\0' != (code = IsEOL(&src)) );
		*dst = '\0';
	}
	*str = src;
	return bottom;
}
/*-----------------------------------------------------------------------------
	パラメータを入手して、オプションかどうかを判別する
	オプションの開始文字は、「/」又は「-」

RET:	== '-':found
-----------------------------------------------------------------------------*/
PPXDLL UTCHAR PPXAPI GetOptionParameter(LPCTSTR *commandline, TCHAR *optionname, TCHAR **optionparameter)
{
	UTCHAR code;

	code = SkipSpace(commandline);
	if ( (code == '-') || (code == '/') ){
		const TCHAR *src;
		TCHAR *dest, *maxptr;

		dest = optionname;
		*dest++ = code;
		maxptr = dest + CMDLINESIZE - 1;
		src = *commandline + 1;
		for ( ;; ){
			code = *src;
			if ( Isalnum(code) == FALSE ) break;
			*dest++ = code;
			if ( dest >= maxptr ) break;
			src++;
		}
		*dest = '\0';
		Strupr(optionname + 1);
		if ( *src == ':' ){
			if ( optionparameter != NULL ){
				*optionparameter = dest + 1;
				src++;
				dest++;
			}else{
				*dest++ = *src++;
			}
			GetLineParam(&src, dest);
		}else{
			if ( optionparameter != NULL ) *optionparameter = dest;
		}
		*commandline = src;
		return '-';
	}else{
		return GetLineParam(commandline, optionname);
	}
}

PPXDLL UTCHAR PPXAPI GetOption(LPCTSTR *commandline, TCHAR *param)
{
	return GetOptionParameter(commandline, param, NULL);
}

void GetQuotedParameter(LPCTSTR *commandline, TCHAR *param, const TCHAR *parammax)
{
	const TCHAR *ptr, *ptrfirst, *ptrlast;
	TCHAR *dest;

	dest = param;
	ptrfirst = ptr = *commandline + 1;
	for ( ; ; ){
		TCHAR code;

		code = *ptr;
		if ( (code == '\0') || (code == '\r') || (code == '\n') ){
			ptrlast = ptr;
			break;
		}
		if ( code != '\"' ){
			ptr++;
			continue;
		}
		// " を見つけた場合の処理
		if ( *(ptr + 1) != '\"' ){	// "" エスケープ?
			ptrlast = ptr++; // 単独 " … ここで終わり
			break;
		}
		// エスケープ処理
		{
			size_t copysize;

			copysize = ptr - ptrfirst + 1;
			if ( (dest + copysize) >= parammax ){ // buffer overflow?
				// " が 1文字エスケープされるので ">=" でok
				ptrlast = ptr;
				break;
			}
			memcpy(dest, ptrfirst, TSTROFF(copysize));
			dest += copysize;
			ptrfirst = (ptr += 2); // " x 2
			continue;
		}
	}
	*commandline = ptr;
	{
		size_t ptrsize;

		ptrsize = ptrlast - ptrfirst;
		if ( (dest + ptrsize) > parammax ){ // buffer overflow?
			tstrcpy(param, T("<flow!!>"));
		}else{
			memcpy(dest, ptrfirst, TSTROFF(ptrsize));
			*(dest + ptrsize) = '\0';
		}
	}
}

// ,/改行 区切りのパラメータを１つ取得する ※PPC_SUB.Cにも
UTCHAR GetCommandParameter(LPCTSTR *commandline, TCHAR *param, size_t paramlen)
{
	const TCHAR *src;
	TCHAR *dest, *destmax;
	UTCHAR firstcode, code;

	firstcode = SkipSpace(commandline);
	if ( (firstcode == '\0') || (firstcode == ',') ){ // パラメータ無し
		*param = '\0';
		return firstcode;
	}
	dest = param;
	destmax = dest + paramlen - 1;
	if ( firstcode == '\"' ){
		GetQuotedParameter(commandline, param, destmax);
		return *param;
	}
	src = *commandline + 1;
	code = firstcode;
	for ( ;; ){
		if ( dest < destmax ) *dest++ = code;
		code = *src;
		if ( (code == ',') || // (code == ' ') ||
			 ((code < ' ') && ((code == '\0') || (code == '\t') ||
							   (code == '\r') || (code == '\n'))) ){
			break;
		}
		src++;
	}
	while ( (dest > param) && (*(dest - 1) == ' ') ) dest--;
	*dest = '\0';
	*commandline = src;
	return firstcode;
}

// 改行使用可能なパラメータ取得
void GetLfGetParam(const TCHAR **param, TCHAR *dest, DWORD destlength)
{
	const TCHAR *src = *param;
	TCHAR *destptr;

	destptr = dest;
	if ( SkipSpace(&src) == '\"' ){ // " 処理
		src++;
		for (;;){
			TCHAR c;

			c = *src;
			if ( c == '\0' ) goto end;
			if ( c == '\"' ){
				if ( *(src + 1) != '\"' ){ // 末尾？
					src++;
					break;
				}
				src++; // "" ... " 自身
			}
			if ( destlength ){
				*destptr++ = c;
				destlength--;
			}
			src++;
			continue;
		}
	}

	for (;;){
		TCHAR c;

		c = *src;
		if ( c == '\0' ) break;

		if ( ((UTCHAR)c <= ' ') || (c == ',') ){
			break;
		}
		if ( destlength ){
			*destptr++ = c;
			destlength--;
		}
		src++;
		continue;
	}
	while ( (destptr > dest) && (*(destptr - 1) == ' ') ) destptr--;
end:
	*param = src;
	*destptr = '\0';
}
