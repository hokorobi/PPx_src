/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System		ファイル操作,名前フィルタ
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_FOP.H"
#include "VFS_STRU.H"
#pragma hdrstop

const TCHAR TagStr[] = T("～ ");
const TCHAR HeaderTagStr1[] = T("ｺﾋﾟｰ ");
const TCHAR HeaderTagStr2[] = T("コピー ");
const TCHAR HeaderTagStr3[] = T("Copy of ");
const TCHAR FooterTagStr1[] = T(" - コピー");

TCHAR *Namefilter(TCHAR *p,TCHAR *q,TCHAR *r,TCHAR *s,BOOL *UseRenum)
{
	while ( r < s ){
		switch( *r ){
			case '?':
				r++;
				if ( *q != '\0' ){
					#ifndef UNICODE
						if ( IskanjiA(*q) ) *p++ = *q++;
					#endif
					*p++ = *q++;
				}
				break;
			case '*':
				r++;
				while ( *q != '\0' ) *p++ = *q++;
				break;
			case '\\':
				*UseRenum = TRUE;
				// break 省略
			default:
				#ifndef UNICODE
					if ( IskanjiA(*r) ) *p++ = *r++;
				#endif
				*p++ = *r++;
				if ( *q != '\0' ){
					#ifndef UNICODE
					if ( IskanjiA(*q) ) q++;
					#endif
					q++;
				}
		}
	}
	return p;
}

TCHAR *BktNum(TCHAR *p,TCHAR f,TCHAR e)
{
	if ( *p != f ) return NULL;
	p++;
	while ( Isdigit(*p) ) p++;
	if ( *p != e ) return NULL;
	return p + 1;
}

int DeleteHeadTag(TCHAR *name,const TCHAR *tag,size_t taglen)
{
	TCHAR *src,*p;

	if ( memcmp(name,tag,TSTROFF(taglen)) ) return 0;
	src = name + taglen;
	p = BktNum(src,'(',')');
	if ( p && (*p == ' ') ) src = p + 1;
	if ( !memcmp(src,TagStr,SIZEOFTSTR(TagStr)) ) src += TSIZEOF(TagStr) - 1;
	tstrcpy(name,src);
	return 1;
}

int DeleteFootTag(TCHAR *name,const TCHAR *tag,size_t taglen)
{
	TCHAR *p;
	size_t len;

	len = tstrlen(name);
	if ( len <= taglen ) return 0;

	p = VFSFindLastEntry(name);
	p = tstrrchr(p,'.');
	if ( p == NULL ){
		p = name + len;
	}

	if ( memcmp((char *)(TCHAR *)(p - taglen),tag,TSTROFF(taglen)) ){
		return 0;
	}
	tstrcpy(p - taglen,p);
	return 1;
}

ERRORCODE LFNfilter(struct FopOption *opt,TCHAR *src)
{
	TCHAR *name;
	BOOL UseRenum = FALSE;

	name = FindLastEntryPoint(src);

	if ( opt->rexps != NULL ){	// 正規表現による加工
		if ( RegularExpressionReplace(opt->rexps,name) == FALSE ){
			return ERROR_CANCELLED;
		}
		if ( FindPathSeparator(src) != NULL ) UseRenum = TRUE;
	}else{
		if ( opt->fop.filter & VFSFOP_FILTER_EXTRACTNAME ){
			FILENAMEINFOSTRUCT finfo;

			finfo.info.Function = (PPXAPPINFOFUNCTION)FilenameInfoFunc;
			finfo.info.Name = STR_FOP;
			finfo.info.RegID = NilStr;
			finfo.filename = src;
			if ( NO_ERROR != PP_ExtractMacro(NULL,&finfo.info,NULL,opt->rename,name,XEO_NOEDIT | XEO_EXTRACTEXEC) ){
				return ERROR_CANCELLED;
			}
			if ( FindPathSeparator(src) != NULL ) UseRenum = TRUE;
		}else if ( opt->rename[0] != '\0' ){	// ワイルドカードによる加工
			TCHAR *p,*s;
			TCHAR orgname[MAX_PATH],orgext[MAX_PATH];
												// Split
			s = name + FindExtSeparator(name);
			memcpy(orgname,name,TSTROFF(s - name));
			orgname[s - name] = '\0';
			tstrcpy(orgext,s);
												// Convert
			s = opt->rename + FindExtSeparator(opt->rename);
												// File name
			p = Namefilter(name,orgname,opt->rename,s,&UseRenum);
			p = Namefilter(p,orgext,s,s + tstrlen(s),&UseRenum);
			*p = '\0';
		}
	}
	if ( opt->fop.delspc ){ // 空白等を削除
		TCHAR *p,*q;

		p = q = name;
		while( *p != '\0' ){
			switch( *p ){
				case ' ':
				case ',':
				case '/':
				case ';':
				case '\"':
				case '\'':
					break;
				default:
					*q++ = *p;
			}
			p++;
		}
		*q = '\0';
	}
	if ( opt->fop.filter & VFSFOP_FILTER_DELNUM ){
		int done = 0;

		for ( ; ; ){
			if ( DeleteHeadTag(name,HeaderTagStr1,TSIZEOF(HeaderTagStr1) - 1) ||
				 DeleteHeadTag(name,HeaderTagStr2,TSIZEOF(HeaderTagStr2) - 1) ||
				 DeleteHeadTag(name,HeaderTagStr3,TSIZEOF(HeaderTagStr3) - 1) ||
				 DeleteFootTag(name,FooterTagStr1,TSIZEOF(FooterTagStr1) - 1)){
				done = 1;
				continue;
			}
			break;
		}
		if ( !done ){
			TCHAR *orgext;
			TCHAR *p,*q C4701CHECK;
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
				q = BktNum(p,'(',')');
				if ( q == orgext ){
					done = 1;
					break;
				}
				q = BktNum(p,'[',']');
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
		TCHAR buf[VFPS],*bsrc,*dst,*p;
		int cy = 1;

		tstrcpy(buf,name);
		bsrc = buf;
		dst = name;
		for ( ; ; ){
			p = FindPathSeparator(bsrc);
			if ( p == NULL ){
				tstrcpy(dst,bsrc);
				break;
			}
			memcpy(dst,bsrc,TSTROFF(p - bsrc));
			dst += p - bsrc;
			bsrc = p + 1;
			tstrcpy(dst,opt->renum);
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
			memmove(opt->renum + 1,opt->renum,TSTRSIZE(opt->renum));
			opt->renum[0] = '1';
		}
	}

	if ( opt->fop.sfn ){ // 8.3
		TCHAR *p,*q;
		DWORD i,e;

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
			tstrcpy(p,q);
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

	// 末尾の空白を除去
	{
		TCHAR *p;

		p = name + tstrlen(name);
		while ( p > name){
			p--;
			if ( *p != ' ' ) break;
			*p = '\0';
		}
	}
	return NO_ERROR;
}
