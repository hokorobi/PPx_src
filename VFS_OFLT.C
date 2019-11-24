/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System		ファイル操作,名前フィルタ
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include "WINOLE.H"
#include "PPX.H"
#include "VFS.H"
#include "PPD_DEF.H"
#include "VFS_STRU.H"
#include "VFS_FOP.H"
#pragma hdrstop

const TCHAR TagStr[] = T("～ ");
const TCHAR HeaderTagStr1[] = T("ｺﾋﾟｰ ");
const TCHAR HeaderTagStr2[] = T("コピー ");
const TCHAR HeaderTagStr3[] = T("Copy of ");
const TCHAR FooterTagStr1[] = T(" - コピー");

TCHAR *RenameFilter(TCHAR *dest, TCHAR *src, TCHAR *format, TCHAR *formatlast, BOOL *UseRenum)
{
	while ( format < formatlast ){
		switch( *format ){
			case '?':
				format++;
				if ( *src != '\0' ){
					#ifndef UNICODE
						if ( IskanjiA(*src) ) *dest++ = *src++;
					#endif
					*dest++ = *src++;
				}
				break;
			case '*':
				format++;
				while ( *src != '\0' ) *dest++ = *src++;
				break;
			case RENAME_NUMBERING:
				*UseRenum = TRUE;
				// break 省略
			default:
				#ifndef UNICODE
					if ( IskanjiA(*format) ) *dest++ = *format++;
				#endif
				*dest++ = *format++;
				if ( *src != '\0' ){
					#ifndef UNICODE
					if ( IskanjiA(*src) ) src++;
					#endif
					src++;
				}
		}
	}
	return dest;
}

TCHAR *BktNum(TCHAR *srcptr, TCHAR bk_l, TCHAR bk_r)
{
	if ( *srcptr != bk_l ) return NULL;
	srcptr++;
	while ( Isdigit(*srcptr) ) srcptr++;
	if ( *srcptr != bk_r ) return NULL;
	return srcptr + 1;
}

int DeleteHeadTag(TCHAR *name, const TCHAR *tag, size_t taglen)
{
	TCHAR *src, *bkptr;

	if ( memcmp(name, tag, TSTROFF(taglen)) ) return 0;
	src = name + taglen;
	bkptr = BktNum(src, '(', ')');
	if ( (bkptr != NULL) && (*bkptr == ' ') ) src = bkptr + 1;
	if ( !memcmp(src, TagStr, SIZEOFTSTR(TagStr)) ) src += TSIZEOF(TagStr) - 1;
	tstrcpy(name, src);
	return 1;
}

int DeleteFootTag(TCHAR *name, const TCHAR *tag, size_t taglen)
{
	TCHAR *ptr;
	size_t len;

	len = tstrlen(name);
	if ( len <= taglen ) return 0;

	ptr = VFSFindLastEntry(name);
	ptr = tstrrchr(ptr, '.');
	if ( ptr == NULL ){
		ptr = name + len;
	}

	if ( memcmp((char *)(TCHAR *)(ptr - taglen), tag, TSTROFF(taglen)) ){
		return 0;
	}
	tstrcpy(ptr - taglen, ptr);
	return 1;
}

ERRORCODE LFNfilter(struct FopOption *opt, TCHAR *src)
{
	TCHAR *name;

	name = FindLastEntryPoint(src);
	if ( opt->UseNameFilter & NameFilter_Use ){
		BOOL UseRenum = FALSE;

		// Rename
		if ( opt->UseNameFilter & NameFilter_Rename ){
			if ( opt->rexps != NULL ){	// 正規表現による加工
				if ( RegularExpressionReplace(opt->rexps, name) == FALSE ){
					return ERROR_CANCELLED;
				}
				if ( FindPathSeparator(src) != NULL ) UseRenum = TRUE; // RENAME_NUMBERING
			}else if ( opt->UseNameFilter & NameFilter_ExtractName ){ // マクロ展開
				FILENAMEINFOSTRUCT finfo;
				const TCHAR *format;

				finfo.info.Function = (PPXAPPINFOFUNCTION)FilenameInfoFunc;
				finfo.info.Name = STR_FOP;
				finfo.info.RegID = NilStr;
				finfo.filename = src;
				format = opt->rename;
				if ( opt->rename[0] == RENAME_EXTRACTNAME ) format++;
				if ( NO_ERROR != PP_ExtractMacro(NULL, &finfo.info, NULL, format, name, XEO_NOEDIT | XEO_EXTRACTEXEC) ){
					return ERROR_CANCELLED;
				}
				if ( FindPathSeparator(src) != NULL ) UseRenum = TRUE; // RENAME_NUMBERING
			}else {	// ワイルドカードによる加工
				TCHAR *tailptr, *extptr;
				TCHAR orgname[MAX_PATH], orgext[MAX_PATH];
												// Split
				extptr = name + FindExtSeparator(name);
				memcpy(orgname, name, TSTROFF(extptr - name));
				orgname[extptr - name] = '\0';
				tstrcpy(orgext, extptr);

				// ファイル名部
				extptr = opt->rename + FindExtSeparator(opt->rename);
				tailptr = RenameFilter(name, orgname, opt->rename, extptr, &UseRenum);
				// 拡張子部
				tailptr = RenameFilter(tailptr, orgext, extptr, extptr + tstrlen(extptr), &UseRenum);
				*tailptr = '\0';
			}
		}
		if ( opt->fop.delspc ){ // 空白等を削除
			TCHAR *ptr, *dest;

			ptr = dest = name;
			while( *ptr != '\0' ){
				switch( *ptr ){
					case ' ':
					case ',':
					case '/':
					case ';':
					case '\"':
					case '\'':
						break;
					default:
						*dest++ = *ptr;
				}
				ptr++;
			}
			*dest = '\0';
		}
		if ( opt->fop.filter & VFSFOP_FILTER_DELNUM ){ // 連番削除
			int done = 0;

			for ( ; ; ){
				if ( DeleteHeadTag(name, HeaderTagStr1, TSIZEOF(HeaderTagStr1) - 1) ||
					 DeleteHeadTag(name, HeaderTagStr2, TSIZEOF(HeaderTagStr2) - 1) ||
					 DeleteHeadTag(name, HeaderTagStr3, TSIZEOF(HeaderTagStr3) - 1) ||
					 DeleteFootTag(name, FooterTagStr1, TSIZEOF(FooterTagStr1) - 1)){
					done = 1;
					continue;
				}
				break;
			}
			if ( !done ){
				TCHAR *orgext;
				TCHAR *p, *q C4701CHECK;
													// Split
				p = name;
				orgext = name + FindExtSeparator(name);
				while ( p < orgext ){
					if ( (*p == '-') || (*p == '_') ){
						q = p + 1;
						if ( Isdigit(*q) ){
							q++;
							while ( Isdigit(*q) ) q++;
							if ( q == orgext ){
								done = 1;
								break;
							}
						}
					}
					q = BktNum(p, '(', ')');
					if ( q == orgext ){
						done = 1;
						break;
					}
					q = BktNum(p, '[', ']');
					if ( q == orgext ){
						done = 1;
						break;
					}
				#ifdef UNICODE
					p++;
				#else
					p += Chrlen(*p);
				#endif
				}
				if ( done ){ // C4701ok
					while( *q ) *p++ = *q++;
					*p = 0;
				}
			}
		}

		if ( IsTrue(UseRenum) ){ // 連番
			TCHAR buf[VFPS], *bsrc, *dst, *p;
			int cy = 1;

			tstrcpy(buf, name);
			bsrc = buf;
			dst = name;
			for ( ; ; ){
				p = FindPathSeparator(bsrc); // RENAME_NUMBERING
				if ( p == NULL ){
					tstrcpy(dst, bsrc);
					break;
				}
				memcpy(dst, bsrc, TSTROFF(p - bsrc));
				dst += p - bsrc;
				bsrc = p + 1;
				tstrcpy(dst, opt->renum);
				dst += tstrlen(dst);
			}
												// Inc
			p = opt->renum + tstrlen(opt->renum);
			while ( p > opt->renum ){
				p--;
				*p += (TCHAR)cy;
				if ( (UTCHAR)*p > (UTCHAR)'9' ){
					*p = '0';
					cy = 1;
				}else{
					cy = 0;
				}
			}
			if ( cy ){
				memmove(opt->renum + 1, opt->renum, TSTRSIZE(opt->renum));
				opt->renum[0] = '1';
			}
		}

		if ( opt->fop.sfn ){ // 8.3
			TCHAR *p, *q;
			DWORD i, e;

			p = name;
			e = FindExtSeparator(p);			// ファイル名を8
			q = p + e;
			if ( e <= 8 ){
				p = q;
			}else{
				i = 0;
				while( i < 8 ){
					#ifndef UNICODE
						if ( IskanjiA(*p++) ){
							if ( i >= 7 ){
								p--;
								break;
							}
							p++;
							i++;
						}
					#else
						p++;
					#endif
					i++;
				}
			}									// 拡張子を3(.を含めて4)
			e = tstrlen32(q);
			if ( e <= 4 ){
				tstrcpy(p, q);
			}else{
				i = 0;
				while( i < 4 ){
					#ifndef UNICODE
						if ( IskanjiA(*p++ = *q++) ){
							if ( i >= 3 ) break;
							*p++ = *q++;
							i++;
						}
					#else
						*p++ = *q++;
					#endif
					i++;
				}
				*p = '\0';
			}
		}
		if ( opt->fop.chrcase != 0 ){
			if ( opt->fop.chrcase == 1 ){
				CharUpper(src); // 大文字化
			}else{ // if ( opt->fop.chrcase == 2 )
				CharLower(src); // 小文字化
			}
		}
	}
	// 末尾の空白・拡張子を除去
	{
		TCHAR *p;

		p = name + tstrlen(name);
		while ( p > name){
			p--;
			if ( (*p != ' ') && (*p != '.') ) break;
			*p = '\0';
		}
	}
	return NO_ERROR;
}
