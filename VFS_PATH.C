/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System				path 文字列 操作

※S-JIS 対応（漢字中に「\」があってもよい）
※頭に VFS が付加された関数は、VFS 用の処理が加えられている。
※頭に VFS が付加されていない関数は、VFS 用の処理がないが、VFS 用に使える
-----------------------------------------------------------------------------*/
#define ONVFSDLL // VFS.H の DLL export 指定
#include "WINAPI.H"
#include <winioctl.h>
#include <shlobj.h>
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#pragma hdrstop

#ifndef ERROR_CANT_ACCESS_FILE
#define ERROR_CANT_ACCESS_FILE 1920
#endif

const TCHAR StrHttp[] = T("http://");
const TCHAR StrHttps[] = T("https://");
const TCHAR StrFileScheme[] = T("file://");
const TCHAR DomainNameRegPathString[] = T("SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters");
const TCHAR DomainNameRegNameString[] = T("Domain");

HWND PasswordDialogWnd = NULL;

/*-----------------------------------------------------------------------------
	拡張子の位置を取得する			※階層指定「xx.xxx\yyy」には非対応
-----------------------------------------------------------------------------*/
VFSDLL int PPXAPI FindExtSeparator(const TCHAR *src)
{
	TCHAR *p;

	if ( *src == '\0' ) return 0;
	p = tstrrchr(src + 1, '.');
	if ( p != NULL ) return ToSIZE32_T(p - src);
	return tstrlen32(src);
}
/*-----------------------------------------------------------------------------
	次のパス区切りを取得する		※パス区切りは「\」のみ
-----------------------------------------------------------------------------*/
VFSDLL TCHAR * PPXAPI FindPathSeparator(const TCHAR *src)
{
	for ( ; ; ){
		TCHAR type;

		type = *src;
		if ( (type == '\\') || (type == '/') ) return (TCHAR *)src;
		if ( type == '\0' ) return NULL;
#ifndef UNICODE
		if ( Iskanji(type) ) src++;
#endif
		src++;
	}
}

/*-----------------------------------------------------------------------------
	最下層のエントリを示すポインタを入手する(root以外は \ を指す)
-----------------------------------------------------------------------------*/
VFSDLL TCHAR * PPXAPI VFSFindLastEntry(const TCHAR *src)
{
	const TCHAR *rp, *tp;
	TCHAR rpchr;

	rp = VFSGetDriveType(src, NULL, NULL);	// ドライブ指定をスキップ
	if ( rp == NULL ) rp = src;	// ドライブ指定が無い
	rpchr = *rp;
	if ( rpchr != '\0' ){			// root なら *rp == 0
		if ( (rpchr == '/') || (rpchr == '\\') ) rp++;
		tp = FindPathSeparator(rp);
		if ( tp != NULL ){
			do {
				rp = tp;
				tp = FindPathSeparator(rp + 1);
			} while( tp != NULL);
		}
	}
	return (TCHAR *)rp;
}

TCHAR * FindLastEntryPoint(const TCHAR *src)
{
	const TCHAR *rp, *tp;

	rp = VFSGetDriveType(src, NULL, NULL);	// ドライブ指定をスキップ
	if ( rp == NULL ) rp = src;	// ドライブ指定が無い
	if ( *rp ){				// root なら *rp == 0
		tp = rp;
		for ( ;; ){
			TCHAR type;

			type = *tp;
			if ( (type == '/') || (type == '\\') ){
				rp = tp + 1;
			}else if ( type == '\0' ){
				break;
#ifndef UNICODE
			}else{
				if ( Iskanji(type) ) tp++;
#endif
			}
			tp++;
		}
	}
	return (TCHAR *)rp;
}

const TCHAR * USEFASTCALL FindSepPtr(const TCHAR *vfp)
{
	TCHAR *p;

	p = tstrchr(vfp, '/');
	return (p != NULL) ? p : (vfp + tstrlen(vfp));
}

/*-----------------------------------------------------------------------------
	ドライブの種類を調べる
-----------------------------------------------------------------------------*/
_Success_(return != NULL)
VFSDLL TCHAR * PPXAPI VFSGetDriveType(_In_z_ const TCHAR *vfp, _Out_opt_ int *resultmode, _Out_opt_ int *force)
{
	int Mode = VFSPT_UNKNOWN;
	int Force = VFSPTF_AUTO;
										// 括りをスキップ
	if ( *vfp == '\"' ) vfp++;

										// force 指定(+/-)を確認
	if ( *vfp == '+' ){
		Force = VFSPTF_SHN;
		vfp++;
	}else if ( *vfp == '-' ){
		Force = VFSPTF_FS;
		vfp++;
	}
										// : / \\ の判別
	if ( (*vfp == '\\') && (*(vfp + 1) == '\\') ){
		vfp += 2;								// long 指定(\\?)の確認
		if ( (*vfp == '?') && (*(vfp + 1) == '\\') ){
			vfp += 2;
			if ( !tstrnicmp(vfp, T("UNC"), 3) ){
				vfp += 3;
			}
		}
		Mode = VFSPT_UNC;
		goto fin;
	}
										// DriveList ?
	if ( *vfp == ':' ){
		vfp++;

		if ( (*vfp == ':') && (*(vfp + 1) == '{') ){
			TCHAR *p;

			p = FindPathSeparator(vfp);
			vfp = (p != NULL) ? p : (vfp + tstrlen(vfp));
			Mode = VFSPT_SHELLSCHEME;
			goto fin;
		}

		while ( *vfp ){
			if ( *vfp == '\\' ) break;
			#ifndef UNICODE
				if ( IskanjiA(*vfp) ) vfp++;
			#endif
			vfp++;
		}
		Mode = VFSPT_DRIVELIST;
		goto fin;
	}
	if ( *vfp == '#' ){
		vfp++;
										// raw disk ?
		if ( Isalpha(*vfp) && (*(vfp + 1) == ':') ){
			Mode = VFSPT_RAWDISK;
			vfp += 2;
			goto fin;
		}
										// PIDL folder ?
		Mode = 0;
		for ( ; ; ){
			if ( *vfp == ':' ){
				vfp++;
				Mode = VFSPT_SHN_DESK - Mode;
				goto fin;
			}
			if ( !Isdigit(*vfp) ) return NULL;
			Mode = (Mode * 10) + (*vfp++ - '0');
		}
	}
										// A-Z: ?
	if ( Isalpha(*vfp) && (*(vfp + 1) == ':') ){
		vfp += 2;
		Mode = VFSPT_DRIVE;
		goto fin;
	}

	switch ( *vfp ){
		case 'a':						// aux: ?
			if ( !memcmp(vfp + 1, StrAux + 1, StrAuxSize - sizeof(TCHAR)) ){
				vfp += 4;
				if ( *vfp == '/' ){
					while ( *(++vfp) == '/' );
					vfp = FindSepPtr(vfp);
				}else{
					TCHAR *p = FindPathSeparator(vfp);
					if ( p != NULL ) vfp = p;
				}
				Mode = VFSPT_AUXOP;
				goto fin;
			}
			break;

		case 'f':
										// ftp: ?
			if ( !memcmp(vfp + 1, StrFtp + 1, StrFtpSize - sizeof(TCHAR)) ){
				vfp += 4;
				while ( *vfp == '/' ) vfp++;
				vfp = FindSepPtr(vfp + 6);
				Mode = VFSPT_FTP;
				goto fin;
			}
										// file: ?
			if ( !memcmp(vfp + 1, StrFileScheme + 1, TSTROFF(5 - 1)) && vfp[5] && vfp[6] ){
				vfp += 7;
				Mode = VFSPT_FILESCHEME;
				goto fin;
			}
			break;

		case 'h':
										// http: ?
			if ( !memcmp(vfp + 1, StrHttp + 1, TSTROFF(7 - 1)) ){
				vfp = FindSepPtr(vfp + 7);
				Mode = VFSPT_HTTP;
				goto fin;
			}
										// https: ?
			if ( !memcmp(vfp + 1, StrHttps + 1, TSTROFF(8 - 1)) ){
				vfp = FindSepPtr(vfp + 8);
				Mode = VFSPT_HTTP;
				goto fin;
			}
			break;

		case 's':
										// shell: ?
			if ( !memcmp(vfp + 1, StrShellScheme + 1, SIZEOFTSTR(StrShellScheme) - sizeof(TCHAR)) ){
				TCHAR *p;

				p = FindPathSeparator(vfp);
				vfp = (p != NULL) ? p : (vfp + tstrlen(vfp));
				Mode = VFSPT_SHELLSCHEME;
				goto fin;
			}
			break;
	}
#ifdef USESLASHPATH
										// / ?
	if ( *vfp == '/' ){
		Mode = VFSPT_SLASH;
		goto fin;
	}
#endif
	if ( Mode == VFSPT_UNKNOWN ) return NULL;
fin:
	if ( resultmode != NULL ) *resultmode = Mode;
	if ( force != NULL ) *force = Force;
	return (TCHAR *)vfp;
}
/*-----------------------------------------------------------------------------
	src と fname の間に「\」 が入った文字列を生成する
	src の末端に「\」があってもなくてもよい
-----------------------------------------------------------------------------*/
VFSDLL void PPXAPI CatPath(_Out_writes_opt_z_(VFPS) TCHAR *dst, TCHAR *src, _In_z_ const TCHAR *fname)
{
	TCHAR *sep;	// 「\」の位置を覚えておく
	TCHAR *readp;
	int len;

	sep = src;
	if ( dst == NULL ){				// src に結果を格納する -------------------
		readp = src;
		while ( *readp ){
			if ( *readp == '\\' ) sep = readp + 1;
			#ifndef UNICODE
				if ( IskanjiA(*readp++) ){
					if ( *readp ) readp++;
				}
			#else
				readp++;
			#endif
		}
		if ( sep != readp ) *(readp++) = '\\';

		len = tstrlen32(fname) + 1;
		if ( (readp - src + len) >= VFPS ){
			tstrcpy(src, T("<too long>"));
			return;
		}
		memcpy(readp, fname, TSTROFF(len));
		return;
	}else{							// dst に結果を格納する -------------------
		readp = src;
		while ( *readp ){
			if ( *readp == '\\' ) sep = readp + 1;
			#ifndef UNICODE
				if ( IskanjiA(*dst++ = *readp++) ){
					if ( *readp ) *dst++ = *readp++;
				}
			#else
				*dst++ = *readp++;
			#endif
		}
		if ( sep != readp ){
			*(dst++) = '\\';
			readp++;
		}
		len = tstrlen32(fname) + 1;
		if ( (readp - src + len) >= VFPS ){
			tstrcpy(dst - (readp - src), T("<too long>"));
			return;
		}
		memcpy(dst, fname, TSTROFF(len));
		return;
	}
}

//BOOL CheckProtcol(TCHAR *path, TCHAR *sep, int flag, const TCHAR *cur)
BOOL CheckProtcol(TCHAR *path, int flag, const TCHAR *cur)
{
//	const TCHAR *scheme;
	TCHAR *p1;	// , *p2;
	TCHAR buf[VFPS];
//	DWORD size;
//							// ドメイン名(xxx.xxx)があるか判別
//	p1 = tstrchr(path, '.');
//	if ( (p1 == NULL) || (p1 > sep) ) return FALSE;
							// 同名ディレクトリがあるか確認
						// '\' に統一
	p1 = path;
	while ( (p1 = tstrchr(p1, '/')) != NULL ) *p1++ = '\\';
	p1 = path;
	if ( flag & VFSFIX_FULLPATH ){
		buf[0] = '\0';
		ExpandEnvironmentStrings(p1, buf, TSIZEOF(buf));
		VFSFullPath(NULL, buf, cur);
//		p1 = buf;
	}
	return FALSE;
/*
	if ( GetFileAttributesL(p1) != BADATTR ) return FALSE;
	scheme = StrHttp;
	size = SIZEOFTSTR(StrHttp);

							// ftp / http の判別
	if (tstrchr(VFSFindLastEntry(p1), '.') != NULL){
		scheme = StrHttp;
		size = SIZEOFTSTR(StrHttp);
	}else{
		scheme = StrFtp;
		size = StrFtpSize;
	}

							// セパレータを '/' に統一
	p2 = path;
	while ( (p2 = FindPathSeparator(p2)) != NULL ) *p2++ = '/';
							// 追加
	memmove(path + size, path, TSTRSIZE(path));
	memcpy(path, scheme, size);
	return TRUE;
*/
}

/*-----------------------------------------------------------------------------
	src を正規化したパスに変換する
-----------------------------------------------------------------------------*/
VFSDLL TCHAR * PPXAPI VFSFixPath(TCHAR *dst, TCHAR *src, _In_opt_z_ const TCHAR *cur, int flag)
{
	TCHAR buf[VFPS];
	TCHAR buf2[VFPS];

	if ( dst == NULL ) dst = src;
	if ( flag & VFSFIX_NOFIXEDGE ){
		tstrcpy(buf, src);
	}else{ // 前方の空白を削除
		TCHAR *sptr, *dptr;

		sptr = src;
		while ( (*sptr == ' ') || (*sptr == '\t') ) sptr++;

		dptr = buf;
		for ( ;; ){
			TCHAR c;

			c = *sptr++;
			if ( c == '\0' ) break;
			if ( c != '\"' ){
				*dptr++ = c;
				continue;
			}
			// セパレータ有り…１文字なら除去
			if ( *sptr != '\"' ) continue;
			sptr++;
			*dptr++ = c;
/*
			for ( ;; ){
				TCHAR sc;

				sc = *sptr;
				if ( sc == '\0' ) break;
				sptr++;
				if ( sc != '\"' ){
					*dptr++ = sc;
					continue;
				}
				if ( *sptr == '\"' ){
					sptr++;
					*dptr++ = sc;
					continue;
				}
				break;
			}
*/
		}
		*dptr = '\0';
	}

	{
		TCHAR *cptr, lastchar;
										// 末尾の空白・「.」を削除
		cptr = buf + tstrlen(buf);
		while ( cptr > buf ){
			lastchar = *(cptr - 1);
			if ( (lastchar == ' ') ||
				 (lastchar == '.') ||
				 (lastchar == '\t') ||
				 (lastchar == '\r') ||
				 (lastchar == '\n') ){
				cptr--;
				continue;
			}
			break;
		}
							// 削った末尾ピリオドが、相対指定であったら元に戻す
		if ( *cptr == '.' ){
			TCHAR *lastentry;

			lastentry = FindLastEntryPoint(buf);
			if ( cptr == lastentry ){
				do {
					cptr++;
				} while ( *cptr == '.' );
			}
		}
		*cptr = '\0';
	}
										// 空欄ならエラーに
	if ( (flag & VFSFIX_NOBLANK) && (buf[0] == '\0') ) return NULL;

										// １文字ドライブ指定の補正
	if ( (flag & VFSFIX_DRIVE) && Isalnum(buf[0]) && (buf[1] == '\0') ){
		int drivename;
		DWORD X_odrv;
										// 指定文字のドライブが実在するか確認
		drivename = buf[0];
		X_odrv = GetCustDword(T("X_odrv"), 1);

		if ( drivename <= '9' ){
			if ( X_odrv == 1 ) X_odrv = 0;
			drivename += ('A' - '1');
		}
		if ( drivename >= 'a' ) drivename -= ('a' - 'A');
		if ( X_odrv && (GetLogicalDrives() & (1 << (drivename - 'A'))) ){
			TCHAR curtmp[VFPS];
			const TCHAR *curp;

			if ( cur == NULL ){
				GetCurrentDirectory(TSIZEOF(curtmp), curtmp);
				curp = curtmp;
			}else{
				curp = cur;
			}
			CatPath(&buf[2], (TCHAR *)curp, buf);
			for ( ; ; ){
				DWORD atr;

				atr = GetFileAttributesL(&buf[2]); // 指定文字dirがあるか
				if ( (atr != BADATTR) && (atr & FILE_ATTRIBUTE_DIRECTORY) ){
					wsprintf(buf2, MessageText(MES_QISD), buf);
					if ( PMessageBox(NULL, buf2, MES_TISD, MB_YESNO) != IDYES ){
						break;
					}
				}
				buf[0] = (TCHAR)drivename;
				buf[1] = ':';
				buf[2] = '\0';
				break;
			}
		}
	}
														// セパレータの補正
	if ( flag & VFSFIX_SEPARATOR ){
		// file: の除去
		if ( !memcmp(buf, StrFileScheme, SIZEOFTSTR(StrFileScheme)) ){
			memmove(buf, buf + TSIZEOFSTR(StrFileScheme),
					TSTRSIZE(buf + TSIZEOFSTR(StrFileScheme)) );
		}
#ifdef USESLASHPATH						// http/ftp ヘッダ無しの確認
		if ( buf[0] != '/' ) // 補正させない
#endif
		{
			if ( memcmp(buf, StrHttp, SIZEOFTSTR(StrHttp) ) &&
				 memcmp(buf, StrHttps, SIZEOFTSTR(StrHttps) ) &&
				 memcmp(buf, StrFtp , StrFtpSize) &&
				 !(!memcmp(buf, StrAux, StrAuxSize) && (buf[4] == '/')) ){
				if ( tstrchr(buf, '/') != NULL ) CheckProtcol(buf, flag, cur);
			}else{
				TCHAR *bsptr;

				bsptr = buf;
				while ( (bsptr = FindPathSeparator(bsptr)) != NULL ){
					*bsptr++ = '/';
				}
			}
		}
	}
	{
		TCHAR *result;

		result = buf;
		if ( flag & VFSFIX_FULLPATH ){	// 絶対化
			TCHAR srcbuf[VFPS];

			tstrcpy(srcbuf, buf);
			ExpandEnvironmentStrings(srcbuf, buf, TSIZEOF(buf));
			result = VFSFullPath(NULL, buf, cur);
			if ( result == NULL ) return NULL;
		}
														// 実体化
		if ( flag & VFSFIX_REALPATH ){
			int mode;

			if ( VFSGetDriveType(buf, &mode, NULL) != NULL ){
				if ( (mode <= VFSPT_SHN_DESK) || (mode == VFSPT_SHELLSCHEME) ){
					HRESULT ComInitResult;
					ComInitResult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
					if ( flag & VFSFIX_VREALPATH ){
						if ( IsTrue(VFSGetRealPath(NULL, buf2, buf)) ){
							tstrcpy(buf, buf2);
						}
					}else{
						if ( VFSGetRealPath(NULL, buf, buf) == FALSE ){
							buf[0] = '?';
							buf[1] = '\0';
						}
					}
					if ( SUCCEEDED(ComInitResult) ) CoUninitialize();
				}
			}
		}
#if 0 // 現在使用していないため、1.12 より 無効化中
		if ( flag & VFSFIX_CASE ) CharUpper(buf);			// 大文字化
#endif
		tstrcpy(dst, buf);
		return dst + (result - buf);
	}
}

/*-----------------------------------------------------------------------------
	相対指定の src を絶対指定の cur より絶対指定に変換する
	階層区切りは'\'のみ
-----------------------------------------------------------------------------*/
// \\pcname\sharename\path1\path2
//   ^               ^
//------------------------------------- UNC の root を決定する
// \\pc\share\path....
//   ^ ^     ^
TCHAR *GetUncRootPtr(TCHAR *path)
{
	TCHAR *pc, *share;

	pc = FindPathSeparator(path);	// PC名をスキップ
	if ( pc == NULL ) return path + tstrlen(path);

	pc++;	// '\\' をスキップ
	share = FindPathSeparator(pc);	// 共有名をスキップ
	if ( share != NULL ){
		if ( share > pc ) return share;	// 共有名以降もあり
		return pc - 1;				// \\pc\\.... 形式の異常→pcまで
	}
	if ( *pc == '\0' ) return pc - 1;	// 共有名無し→pcまで
	return pc + tstrlen(pc);			// 共有名の末尾まで
}

VFSDLL TCHAR * PPXAPI VFSFullPath(_Out_writes_opt_z_(VFPS) TCHAR *dst, TCHAR *src, _In_opt_z_ const TCHAR *cur)
{
	// 加工中パス関連
		TCHAR	dir[VFPS * 2];	// 加工中のディレクトリ
		int		sc = 0;			// ディレクトリの階層 0:root -1:drive list
		TCHAR	*drvp;			// ドライブ名の末尾(root位置記憶用)
		TCHAR	*cutp;			// 整形用最終書き込み位置(末尾の\を取り除ける)
		TCHAR	*dstp;			// 最終書き込み位置
		TCHAR	*bp;			// 末端エントリの先頭
		TCHAR	sep = '\\';		// パス区切り(url mode なら /)
	// src関連
		TCHAR	*srcp;		// 参照位置
		int		mode = VFSPT_UNKNOWN;
		BOOL	relative = FALSE;	// カレント相対指定有り
		int		force = VFSPTF_AUTO;

	TCHAR *drvlastp;

	if ( dst == NULL ) dst = src;
								// src のパス種類と相対かを判断 ===============
	srcp = src;
	drvlastp = VFSGetDriveType(src, &mode, &force);
	if ( drvlastp == NULL ){	// ドライブ名等がない→相対
		relative = TRUE;
		dstp = dir;
	}else{
		switch( mode ){
			case VFSPT_FILESCHEME:
				srcp = VFSGetDriveType(drvlastp, &mode, &force);
				if ( srcp == NULL ){
					dir[0] = '\\';
					dir[1] = '\\';
					tstrcpy(dir + 2, drvlastp);
					drvlastp = dir;
				}
				return VFSFullPath(dst, (TCHAR *)drvlastp, cur);

			case VFSPT_AUXOP:
				if ( *(src + 4) != '/' ) break;
				// VFSPT_SLASH へ
								// FTP/HTTP 指定付なら必ず絶対指定
			case VFSPT_SLASH:
			case VFSPT_FTP:
			case VFSPT_HTTP:
				sep = '/';
				break;
								// UNC 指定付なら必ず絶対指定
			case VFSPT_UNC:
						// \\server\share をドライブ名相当にする
				if ( *drvlastp != '>' ) drvlastp = GetUncRootPtr(drvlastp);
				break;
								// shell: 指定付なら必ず絶対指定
			case VFSPT_SHELLSCHEME:
				break;

			default:			// GNC 等なら、セパレータの有無で判断する
				if ( *drvlastp != sep ) relative = TRUE;
		}
								// ドライブ名部分を転送
		memcpy(dir, src, TSTROFF(drvlastp - src));
		dstp = dir + (drvlastp - src);
		srcp = drvlastp;
		#ifdef WINEGCC // system で実行
			if ( mode == VFSPT_SLASH ){
				tstrcpy(dir, T("Z:/"));
				dstp = dir + 2;
				mode = VFSPT_DRIVE;
				srcp++;
			}
		#endif
								// ドライブ名を大文字に
		if ( mode == VFSPT_DRIVE ) *(dstp - 2) &= 0x5f;

		if ( mode == VFSPT_DRIVELIST ){	// DriveList か エイリアスかを判定
			*dstp = '\0';
			if ( (dir[1] != '\0') && (dir[1] != ':') ){
				TCHAR *dlp;

				if ( dir[1] == '<' ){ // :<comment>path 処理
					TCHAR *lastp;
					lastp = tstrchr(src, '>');
					if ( lastp != NULL ) return VFSFullPath(dst, lastp + 1, cur);
				}
					// ドライブリスト用コメント
				dlp = tstrchr(src, '\t');
				if ( dlp != NULL ){
					tstrcpy(dir, src + 1);
					*(dir + (dlp - src) - 1) = '\0';
					return VFSFullPath(dst, dir, cur);
				}

				if ( NO_ERROR == GetCustTable(
					(GetCustDword(StrX_dlf, 0) & XDLF_USEMDRIVES) ?
							StrMDrives : PathJumpName,
					dir + 1, dir, sizeof(dir)) ){
												// エイリアスだった
					PP_ExtractMacro(NULL, NULL, NULL, dir, dir, XEO_DISPONLY);
					VFSFullPath(NULL, dir, cur);
					if ( *srcp == sep ) srcp++;
					return VFSFullPath(dst, (TCHAR *)srcp, dir);
				}
			}
		}
	}
	if ( relative == FALSE ){	// 絶対指定なら変数初期化のみ ====
		*dstp++ = sep;	// root 用セパレータを追加
		bp = cutp = drvp = dstp;
		if ( *srcp == sep ) srcp++; // root の '\' をスキップ
	}else{						// 相対指定なら、カレントを用意する =======
		TCHAR curtmp[VFPS];	//
		TCHAR *curp;			// カレント cp
		int cmode = VFSPT_UNKNOWN;

		if ( cur == NULL ){
			GetCurrentDirectory(TSIZEOF(curtmp), curtmp);
		}else{
			tstrcpy(curtmp, cur);
		}
		curp = VFSGetDriveType(curtmp, &cmode, NULL);
		if ( curp == NULL ) return NULL;
		if ( cmode == VFSPT_DRIVE) *(curp - 2) &= 0x5f; // ドライブ名を大文字に
		if ( (cmode == VFSPT_SLASH) || (cmode == VFSPT_FTP) || (cmode == VFSPT_HTTP) || ((cmode == VFSPT_AUXOP) && (curtmp[4] == '/')) ){
			sep = '/';
		}
							// curドライブ名がsrcと同じか確認、違う場合は再取得
		if ( ((mode != VFSPT_UNKNOWN) && (mode != cmode) ) ||
			 ((mode == VFSPT_DRIVE)   && (*(curp - 2) != *(dstp - 2))) ){
			const TCHAR *hist;

			*dstp = '\0';
			UsePPx();
			hist = SearchPHistory(PPXH_PPCPATH, dir);	// ヒストリから取得
			if ( hist != NULL ){
				tstrcpy(curtmp, hist);
				FreePPx();
			}else{								// ヒストリから取得失敗
				FreePPx();
				if ( mode == VFSPT_DRIVE ){	// 'A:'形式なら API で取得
					*dstp = '\0';
					if ( !GetFullPathName(dir, VFPS, curtmp, &drvlastp) ){
						*dst = '\0';
						return NULL;
					}
				}else{						// そのほか→カレント無いのでroot扱
					CatPath(curtmp, dir, NilStr);
				}
			}
			cmode = VFSPT_UNKNOWN;
			curp = VFSGetDriveType(curtmp, &cmode, NULL);
			if ( cmode == VFSPT_DRIVE ) *(curp - 2) &= 0x5f;
		}
		if ( cmode == VFSPT_UNKNOWN ){
			*dst = '\0';
			return NULL;
		}
										// カレントパス種類毎の補正
		if ( cmode == VFSPT_FTP ){
			sep = '/';
		}else if ( cmode == VFSPT_HTTP ){ // カレントを補正する
			TCHAR *clp, *cl;

			sep = '/';
			cl = curp;
			if ( (*srcp == '/') && (*(srcp + 1) == '/') ){ // //～ のとき
				clp = tstrchr(curtmp, '/');
				if ( clp != NULL ) curp = cl = clp + 1;
			}else{ // curtmp の最後のパス区切りを取得
				clp = tstrchr(cl, '/');
				if ( clp != NULL ){
					for ( ; ; ){
						cl = clp;
						clp = tstrchr(cl + 1, '/');
						if ( clp == NULL ) break;
					}
				}
			}
			*cl = '\0';
		}else if ( cmode == VFSPT_UNC ){
			curp = GetUncRootPtr(curp);
			if ( curp == (curtmp + 2) ) curp--;	// \\ の時は補正
		}
		if ( *curp == sep ){
			curp++;
		}else if ( *curp == '\0' ){
			*curp++ = sep;
			*curp = '\0';
		}
		tstrcpy(dir, curtmp);
		mode = cmode;
		dstp = dir + (curp - curtmp);
		bp = cutp = drvp = dstp;

		if ( *srcp == sep ){	// ドライブ名のみ無く、あとは絶対指定
			srcp++;
		}else{					// カレント相対指定→カレントの階層を取得
			for ( ; ; ){
				TCHAR *sepp;

				sepp = dstp;
				for ( ; ; ){ // FindPathSeparator
					TCHAR type;

					type = *sepp;
					if ( type == sep ) break;
					if ( type == '\0' ){
						sepp = NULL;
						break;
					}
				#ifndef UNICODE
					if ( Iskanji(type) ) sepp++;
				#endif
					sepp++;
				}
				if ( sepp == NULL ){
					if ( *dstp != '\0' ){
						bp = dstp;
						sc++;
						dstp += tstrlen(dstp);
						cutp = dstp;
						*dstp++ = sep;
					}
					break;
				}
				sc++;
				bp = dstp;
				cutp = sepp;
				dstp = sepp + 1;
				if ( *dstp == '\0' ) break;
			}
		}
	}
	if ( mode == VFSPT_DRIVELIST ) sc = -1;
										// src のディレクトリを設定 ===========
										// srcp:末尾の「\」を除去できる位置
	// ※ ドライブ名直後の > 以降はコメントとして無視
	if ( *srcp != '>' ) while( *srcp != '\0' ){
		int i;
		TCHAR *srcpp;

		if ( sc <= 0 ) bp = cutp = dstp = drvp;	// 戻りすぎの場合の対策
		if ( *srcp == sep ){ // 連続する「\」を除去
			if ( mode != VFSPT_AUXOP ){ // aux: は途中で // が必要なことがある
				do {
					srcp++;
				}while ( *srcp == sep );
				if ( *srcp == '\0' ) break;
			}
		}
		if ( *srcp == ':' ){		// 疑似ドライブ(quasi-drive)指定
			const TCHAR *hist, *histr;

			srcp++;
			*dstp = '\0';

			UsePPx();
			hist = SearchPHistory(PPXH_PPCPATH, dir);	// ヒストリから取得
			if ( hist == NULL ){	// カレント無し→疑似ルート
				FreePPx();
				continue;
			}
										// 疑似ルートの方が、疑似サブより前？
			if ( *cutp == sep ) *cutp = '\0';	// 念のため、比較
			histr = SearchHistory(PPXH_PPCPATH, dir);	// ヒストリから取得
			if ( histr != NULL ){
				if ( histr < hist ) hist = histr;
			}

			CatPath(dir, (TCHAR *)hist, NilStr);
			FreePPx();
			bp = dstp = dir + tstrlen(dir);
			cutp = dstp - 1;
			continue;
		}
										// 相対指定を検索
		srcpp = srcp;
		for ( i = 0; *srcpp != '\0' ; i++ ){
			if ( *srcpp == sep ) break;
			if ( *srcpp != '.' ){
				i = 0;
				break;
			}
			srcpp++;
		}
		if ( i == 0 ){						// 一般指定
			bp = dstp;
			srcpp = FindPathSeparator(srcpp);
			sc++;
			if ( srcpp != NULL ){		// 続き有
				size_t len;

				srcpp++;
				len = srcpp - srcp;
				if ( (dstp + len) >= (dir + VFPS * 2) ) goto lenerror;
				memcpy(dstp, srcp, TSTROFF(len));
				srcp = srcpp;
				dstp += len;
				cutp = dstp - 1;
				if ( mode != VFSPT_HTTP ) continue;
				cutp++; // http では末尾セパレータを残す
				continue;
			}else{					// 続きなし
				size_t len = tstrlen(srcp);

				if ( (dstp + len) >= (dir + VFPS) ) goto lenerror;
				memcpy(dstp, srcp, TSTROFF(len));
				cutp = dstp + len;
				break;
			}
		}else{							// 相対指定
			if ( *srcpp != '\0' ) srcpp++;
			srcp = srcpp;
			i--;
			if ( i == 0 ) continue;	// '.'
			sc -= i;
			cutp = dstp = drvp;
			for ( i = 0 ; i < sc ; i++ ){
				TCHAR *dstpp;
				bp = dstp;
				dstpp = FindPathSeparator(dstp);
				if ( dstpp == NULL ){
					dstp += tstrlen(dstp);
					break;
				}
				cutp = dstpp;
				dstp = dstpp + 1;
			}
			continue;
		}
	}
	*cutp = '\0';
	if ( sc < 0 ){
		tstrcpy(dir, T(":"));
	}else if ( (sc == 0) && (mode == VFSPT_UNC) ){
		if ( (drvp - dir) > 2 ){	// \\pc\share\ → \\pc\share に加工
			*(--bp) = '\0';
		}
	}
	{
		DWORD len;

		len = tstrlen32(dir) + 1;
		if ( len > VFPS ) goto lenerror;
		if ( force == VFSPTF_FS ){
			if ( FALSE == VFSGetRealPath(NULL, dst, dir + 1) ){
				memcpy(dst, dir + 1, TSTROFF(len - 1));
			}
		}else{
			memcpy(dst, dir, TSTROFF(len));
		}
	}
	return dst + (bp - dir);

lenerror:
	tstrcpy(dst, T("<too long>"));
	SetLastError(ERROR_FILENAME_EXCED_RANGE);
	return NULL;
}
/*-----------------------------------------------------------------------------
	階層ディレクトリの一括作成
-----------------------------------------------------------------------------*/
VFSDLL ERRORCODE PPXAPI MakeDirectories(const TCHAR *dst, const TCHAR *src)
{
	BOOL err;
	ERRORCODE result = NO_ERROR;
	TCHAR *shortsrc = NULL, shortsrcbuf[VFPS], shortdst[VFPS], *wp;

	#ifndef UNICODE
		if ( WinType == WINTYPE_9x ) src = NULL;
	#endif
	if ( src != NULL ){
		err = CreateDirectoryExL(src, dst, NULL);
	}else{
		err = CreateDirectoryL(dst, NULL);
	}
	if ( err == FALSE ){
		result = GetLastError();

		if ( !memcmp(dst, StrFtp, StrFtpSize) ){
			result = CreateFtpDirectory(dst);
			if ( result == NO_ERROR ) return NO_ERROR;
		}
		if ( !memcmp(dst, StrAux, StrAuxSize) ){
			result = FileOperationAux(T("makedir"), dst, NilStr, NilStr);
			if ( result == NO_ERROR ) return NO_ERROR;
		}
		// FILE_ATTRIBUTE_VIRTUAL のときは、ERROR_INVALID_PARAMETER がでる
		if ( (src != NULL) &&
			 ((result == ERROR_ACCESS_DENIED) || (result == ERROR_INVALID_PARAMETER)) ){
			result = NO_ERROR;
			if ( CreateDirectoryL(dst, NULL) == FALSE ) result = GetLastError();
		}
		if ( result == ERROR_PATH_NOT_FOUND ){
			tstrcpy(shortdst, dst);
			wp = VFSFindLastEntry(shortdst);
			if ( *wp ){
				*wp = '\0';
				if ( src != NULL ){
					tstrcpy(shortsrcbuf, src);
					wp = VFSFindLastEntry(shortsrcbuf);
					if ( *wp ) *wp = 0;
					shortsrc = shortsrcbuf;
				}
				result = MakeDirectories(shortdst, shortsrc);
				if ( result == NO_ERROR ){
					if ( src != NULL ){
						err = CreateDirectoryExL(src, dst, NULL);
					}else{
						err = CreateDirectoryL(dst, NULL);
					}
					if ( err == FALSE ) result = GetLastError();
				}
			}
		}
	}
	// 時刻の複写
	while ( (result == NO_ERROR) && (src != NULL) ){
		HANDLE hFile;
		FILETIME fc, fa, fw;

		hFile = CreateFileL(src, GENERIC_READ, FILE_SHARE_READ, NULL,
				OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL, NULL);
		if ( hFile == INVALID_HANDLE_VALUE ) break;
		if ( !GetFileTime(hFile, &fc, &fa, &fw) ){
			CloseHandle(hFile);
			break;
		}
		CloseHandle(hFile);
		hFile = CreateFileL(dst, GENERIC_WRITE, FILE_SHARE_READ, NULL,
					OPEN_EXISTING,
					FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL, NULL);
		if ( hFile == INVALID_HANDLE_VALUE ) break;
		SetFileTime(hFile, &fc, &fa, &fw);
		CloseHandle(hFile);
		break;
	}
	return result;
}

typedef struct {
	TCHAR path[VFPS];
	TCHAR username[VFPS];
	TCHAR password[VFPS];
} TRYLOGINSTRUCT;

// 指定ディレクトリへの認証を行うためのダイアログボックス ---------------------
INT_PTR CALLBACK VFSChangeDirectoryDlgBox(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch(iMsg){
//-----------------------------------------------------------------------------
		case WM_INITDIALOG: {
			TRYLOGINSTRUCT *tls;

			CenterWindow(hDlg);
			LocalizeDialogText(hDlg, IDD_LOGIN);
			tls = (TRYLOGINSTRUCT *)lParam;
			SetDlgItemText(hDlg, IDS_LOGINPATH, tls->path);
			SetDlgItemText(hDlg, IDE_LOGINUSER, tls->username);
//			SetDlgItemText(hDlg, IDE_LOGINPASS, tls->password);
			SendDlgItemMessage(hDlg, IDE_LOGINUSER, EM_SETSEL, 0, EC_LAST);
			SetDlgFocus(hDlg, IDE_LOGINUSER);
			return FALSE;
		}
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
//-----------------------------------------------------------------------------
				case IDOK: {
					TCHAR user[MAX_PATH * 2];
					TCHAR buf1[0x1000], buf2[VFPS];
					TCHAR path[MAX_PATH];
					NETRESOURCE nr;
					DWORD connectflag;

					GetDlgItemText(hDlg, IDS_LOGINPATH, path, TSIZEOF(path));
					GetDlgItemText(hDlg, IDE_LOGINUSER, user, TSIZEOF(user));
					GetDlgItemText(hDlg, IDE_LOGINPASS, buf2, TSIZEOF(buf2));
					connectflag = IsDlgButtonChecked(hDlg, IDX_LOGINRE) ?
											CONNECT_UPDATE_PROFILE : 0;
					nr.dwType		= RESOURCETYPE_DISK;
					nr.lpLocalName	= NULL;
					nr.lpRemoteName	= path;
					nr.lpProvider	= NULL;

					SetMessageOnCaption(hDlg, T("connecting..."));

					// ※ VFSChangeDirectory 内で登録
					if ( DWNetAddConnection3(hDlg, &nr, buf2, user, connectflag)
							== NO_ERROR ){
						if ( IsDlgButtonChecked(hDlg, IDX_LOGINRE) ){
							TCHAR *p;

							GetDlgItemText(hDlg, IDE_LOGINUSER, buf2, 120);
							p = buf2 + tstrlen(buf2) + 1;
							GetDlgItemText(hDlg, IDE_LOGINPASS, p, 120);
							p = p + tstrlen(p) + 1;
							WriteEString(buf1, (BYTE *)buf2, TSTROFF32(p - buf2));
							SetCustStringTable(T("_IDpwd"), path, buf1, 0);
						}
						EndDialog(hDlg, 1);
					}else{
/*	別のユーザで接続していたときに、切断してみようとしても、
	別のリソースに接続しているときは切断できないので、エラー表示の処理とする。
*/
						PPErrorBox(hDlg, T("connect"), PPERROR_GETLASTERROR);
						SetMessageOnCaption(hDlg, NULL);
					}
					ClearMemory(buf2, sizeof(buf2));
					break;
				}
				case IDCANCEL:
					EndDialog(hDlg, 0);
					break;
			}
			break;

		default:
			return FALSE;
	}
	return TRUE;
}

// 対象がネットワークドライブならエクスプローラを使って再接続してみる
ERRORCODE VFSTryDirectory_Local(const TCHAR *vpath)
{
	HANDLE hFF;
	WIN32_FIND_DATA ff;
	TCHAR path[VFPS];

	wsprintf(path, T("+%c:\\*"), *vpath);

	hFF = VFSFindFirst(path, &ff);
	if ( hFF != INVALID_HANDLE_VALUE ){
		VFSFindClose(hFF);
		return NO_ERROR;
	}
	return GetLastError();
}

ERRORCODE VFSTryDirectory_UNC(HWND hWnd, const TCHAR *vpath)
{
	NETRESOURCE nr;
	TRYLOGINSTRUCT tls;
	TCHAR phrasebuf[0x1000], *ptr;
	ERRORCODE er;

	// メインスレッドのウィンドウがメッセージループに入る前に
	// サブスレッドでダイアログを表示させようとすると、
	// ATOK のスレッド内でデッドロックに陥るのを回避するコード
	if ( hWnd != NULL ){
		DWORD WndThreadID = GetWindowThreadProcessId(hWnd, NULL);

		if ( WndThreadID != GetCurrentThreadId() ){ // サブスレッド?
			DWORD_PTR sendresult;

			// メッセージポンプが動いているとすぐ戻ってくる。
			// そうでなければ TIMEOUT まで待機する
			SendMessageTimeout(hWnd, WM_NULL, 0, 0, 0, 10000, &sendresult);
		}
	}

	tstrcpy(tls.path, vpath);

	// ログイン先のパスを用意する/共有名\ -------------------------------------
	ptr = VFSGetDriveType(tls.path, NULL, NULL);
	if ( ptr != NULL ){
		ptr = FindPathSeparator(ptr);
		if ( ptr != NULL ){
			ptr = FindPathSeparator(ptr + 1);
			if ( ptr != NULL ) *ptr = '\0';
		}
	}

	// デフォルトのユーザ名を準備する(ドメインならPC名を用意) ---------
	tls.password[0] = '\0';
	GetRegString(HKEY_LOCAL_MACHINE, DomainNameRegPathString, DomainNameRegNameString, tls.password, TSIZEOF(tls.password));
	if ( tls.password[0] != '\0' ){
		TCHAR *src, *dest;

		src = tls.path + 2; // 「\\」のあと
		dest = tls.username;
		while ( *src && (*src != '\\') ) *dest++ = *src++;
		*dest++ = '\\';
		*dest = '\0';
	}else{
		tstrcpy(tls.username, UserName);
	}

	// キャッシュされたユーザ名とパスワードを取り出す -----------------
	if ( NO_ERROR == GetCustTable(T("_IDpwd"), tls.path, phrasebuf, sizeof(phrasebuf)) ){
		ERRORCODE ec;
		TCHAR *ptr;

		ptr = phrasebuf;
		ReadEString(&ptr, tls.username, sizeof(tls.username));
		ptr = tls.username + tstrlen(tls.username) + 1;
		tstrcpy(tls.password, ptr);
		ClearMemory(ptr, sizeof(tls.password) - (ptr - tls.username) * sizeof(TCHAR) );
		nr.dwType = RESOURCETYPE_DISK;
		nr.lpLocalName = NULL;
		nr.lpRemoteName = tls.path;
		nr.lpProvider = NULL;
		ec = DWNetAddConnection3(hWnd, &nr, tls.password, tls.username, 0);
		ClearMemory(tls.password, sizeof(tls.password));
		if ( ec == NO_ERROR ){
			SetCurrentDirectory(vpath);
			return NO_ERROR;
		}
	}
	if ( (PasswordDialogWnd != NULL) && IsWindow(PasswordDialogWnd) ){
		er = ERROR_PATH_BUSY;
	}else{
		PasswordDialogWnd = hWnd;
		er = (PPxDialogBoxParam(DLLhInst, MAKEINTRESOURCE(IDD_LOGIN), hWnd,
				VFSChangeDirectoryDlgBox, (LPARAM)&tls) > 0) ?
				NO_ERROR : ERROR_ACCESS_DENIED;
		PasswordDialogWnd = NULL;
	}
	return er;
}

// ユーザ認証付き SetCurrentDirectory -----------------------------------------
VFSDLL ERRORCODE PPXAPI VFSChangeDirectory(HWND hWnd, const TCHAR *path)
{
	return VFSTryDirectory(hWnd, path, FALSE);
}

VFSDLL ERRORCODE PPXAPI VFSTryDirectory(HWND hWnd, const TCHAR *path, BOOL trymode)
{
	ERRORCODE result;
	TCHAR FullPath[VFPS];
	int mode;
	TCHAR *vp;
	TCHAR regpath[VFPS], rpath[MAX_PATH];

	vp = VFSGetDriveType(path, &mode, NULL);
	if ( vp == NULL ){		// 種類が分からない→相対指定の可能性→絶対化
		VFSFullPath(FullPath, (TCHAR *)path, NULL);
		vp = VFSGetDriveType(FullPath, &mode, NULL);
		if ( vp == NULL ){	// それでも種類が分からない→エラー
			return ERROR_BAD_PATHNAME;
		}
	}

	if ( mode == VFSPT_DRIVE ){	// A:
		wsprintf(regpath, T("Network\\%c"), *(vp - 2));
		rpath[0] = '\0';
		GetRegString(HKEY_CURRENT_USER, regpath, RPATHSTR, rpath, TSIZEOF(rpath));
		if ( rpath[0] != '\0' ){ // ネットワークドライブである→存在チェック
			TCHAR *p;

			p = FindPathSeparator(rpath);
			if ( p == NULL ) p = rpath;
			if ( CheckComputerActive(rpath, p - rpath) == FALSE ){
				return ERROR_BAD_NETPATH;
			}
		}else{
			if ( trymode ) return NO_ERROR;
		}
	}else if ( mode == VFSPT_UNC ){ // unc
		TCHAR *p;

		if ( *vp == '\0' ) return NO_ERROR;	// '\\'...処理できないので終了

		p = FindPathSeparator(vp);
		if ( (p == NULL) || (*(p + 1) == '\0') ) return NO_ERROR; // \\pcname

		// WinXP以前の場合、SetCurrentDirectory でブロックが掛かるので、
		// 予め存在するかを調べておく。Vista以降はここで時間掛かることがある
		if ( CheckComputerActive(vp - 2, p - path) == FALSE ){
			return ERROR_BAD_NETPATH;
		}
	}else{ // GNC/UNC 以外は失敗確実なので終了
		return NO_ERROR;
	}
	vp -= 2;

	if ( tstrlen(vp) >= (MAX_PATH - 4) ) return ERROR_FILENAME_EXCED_RANGE;

	if ( IsTrue(SetCurrentDirectory(vp)) ) return NO_ERROR;
	result = GetLastError();
	if ( result == ERROR_INVALID_NAME ) return NO_ERROR; // 極長パスの可能性

	// trymode のとき、属性取得が可能なときならエラー無しに
	// ※ SetCurrentDirectory のみできないことがあるようだ？
	if ( trymode && (result == ERROR_ACCESS_DENIED) && (GetFileAttributesL(vp) != BADATTR) ){
		return NO_ERROR;
	}

	if ( mode == VFSPT_DRIVE ){	// A:
		if ( (result == ERROR_PATH_NOT_FOUND) && (rpath[0] != '\0') ){
			result = VFSTryDirectory_Local(vp);
		}
	}else{
		if ( (result == ERROR_ACCESS_DENIED) ||
			 (result == ERROR_INVALID_PASSWORD) ||
			 (result == ERROR_ACCOUNT_DISABLED) ||
			 (result == ERROR_LOGON_FAILURE) ||
			 (result == ERROR_CANT_ACCESS_FILE) ||
			 (result == ERROR_LOGON_TYPE_NOT_GRANTED) ){
			// UNCなら自前でダイアログを表示
			if ( IsTrue(LoadNetFunctions()) ){
				ERRORCODE dlgresult;

				dlgresult = VFSTryDirectory_UNC(hWnd, vp);
				if ( dlgresult != ERROR_ACCESS_DENIED ) result = dlgresult;
			}
		}
	}
	return result;
}


BOOL IsRelativeDirectory(const TCHAR *path)
{
	return IsRelativeDir(path);
}

BOOL IsParentDirectory(const TCHAR *path)
{
	return IsParentDir(path);
}

DWORD PPXAPI GetSelAttr(const TCHAR *name)
{
	TCHAR buf[VFPS];

#ifdef UNICODE
	tstrcpy(buf, name);
	tstrreplace(buf, T("|"), NilStr);
#else
	{
		char type, *dest;

		dest = buf;
		for ( ;; ){
			type = *name++;
			if ( type == '\0' ) break;
			if ( type == '|' ) continue;
			*dest++ = type;
			if ( Iskanji(type) ) *dest++ = *name++;
		}
		*dest = '\0';
	}
#endif
	return GetFileAttributesL(buf);
}

VFSDLL BOOL PPXAPI GetUniqueEntryName(TCHAR *src)
{
	TCHAR name[VFPS], ext[VFPS];
	int addnum = 0;
	int mulnum = 1;
	int maxtry = 10000;	// 最大試行数
	TCHAR *ptr;
	DWORD (PPXAPI * GetAttr)(const TCHAR *name);

	GetAttr = (SearchPipe(src) == NULL) ? GetFileAttributesL : GetSelAttr;
	if ( GetAttr(src) == BADATTR ) return TRUE;
	if ( tstrlen(src) >= (VFPS - 5) ) return TRUE; // 名前の変更保証ができない
						// 拡張子を隔離 -----------------------
	tstrcpy(name, src);
	ptr = VFSFindLastEntry(name);
	ptr += FindExtSeparator(ptr);

#ifdef UNICODE
	while ( (ptr > name) && (*(ptr - 1) == '|') ) ptr--;
#else
	// 漢字判定は簡易
	while ( (ptr > (name + 1) ) && (*(ptr - 1) == '|') && !Iskanji(*(ptr - 2)) ){
		ptr--;
	}
#endif
	tstrcpy(ext, ptr);
	*ptr = '\0';
						// 以前の番号を抽出 -------------------
	for ( ; name < ptr ; mulnum *= 10 ){
		ptr--;
		if ( *ptr == '-' ){
			*ptr = '\0';
			break;
		}
		if ( !Isdigit(*ptr) ){
			addnum = 0;
			break;
		}
		addnum += ((int)*ptr - '0') * mulnum;
	}
						// 空きを捜す -------------------------
	do{
		if ( addnum != 0 ){
			wsprintf(src, T("%s-%d%s"), name, addnum, ext);
		}else{
			wsprintf(src, T("%s%s"), name, ext);
		}
		addnum++;
		maxtry--;
		if ( maxtry == 0 ) return FALSE;
	}while( GetAttr(src) != BADATTR );
	return TRUE;
}

// ドライブ名を抽出する -------------------------------------------------------
VFSDLL TCHAR * PPXAPI GetDriveName(_Out_writes_z_(VFPS) TCHAR *dst, _In_z_ const TCHAR *src)
{
	TCHAR *p, *q, *r;
	int mode;

	*dst = '\0';
	p = VFSGetDriveType(src, &mode, NULL);
	if ( p == NULL ) return NULL;
	if ( mode == VFSPT_DRIVE ){
		memcpy(dst, p - 2, TSTROFF(2));
		*(dst + 2) = '\\';
		*(dst + 3) = '\0';
		return p;
	}else if ( mode == VFSPT_UNC ){
		q = FindPathSeparator(p);
		if ( q == NULL ) return NULL;
		r = FindPathSeparator(q + 1);
		if ( r == NULL ){
			r = q + tstrlen(q);
		}
		memcpy(dst, p - 2, TSTROFF(r - p + 2));
		*(dst + (r - p + 2)) = '\\';
		*(dst + (r - p + 3)) = '\0';
		return r;
	}
	return NULL;
}

void GetIDLSub(TCHAR *path, LPSHELLFOLDER pSF, LPITEMIDLIST pSHidl)
{
	TCHAR *destp;
	BYTE *idlPtr;

	tstrcpy(path, T("#:\\"));
	destp = path + 3; // '\0'
	idlPtr = (BYTE *)pSHidl;
	while( *(WORD *)idlPtr != 0 ){
		WORD *nextp, old;

		nextp = (WORD *)(BYTE *)(idlPtr + *(WORD *)idlPtr);
		old = *nextp;
		*nextp = 0;
		if ( FALSE == PIDL2DisplayNameOf(destp, pSF, pSHidl) ) break;
		*(destp - 1) = '\\'; // '\0'→'\\'
		destp += tstrlen(destp) + 1;
		*nextp = old;
		idlPtr = (BYTE *)nextp;
	}
}

DWORD GetReparsePath(const TCHAR *path, TCHAR *pathbuf)
{
	HANDLE hFile;
	REPARSE_DATA_IOBUFFER rdio;
	DWORD size;

	*pathbuf = '\0';
	hFile = CreateFileL(path, 0, 0, NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ) return 0;

	if ( IsTrue(DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT,
			NULL, 0, &rdio, sizeof(rdio), &size, NULL)) ){
		WCHAR *src;

		src = rdio.ReparseGuid.PathBuffer + (rdio.ReparseGuid.SubstituteNameOffset / sizeof(WCHAR));
		if ( rdio.ReparseTag == IO_REPARSE_TAG_SYMLINK ) src += 2;

		if ( (((BYTE *)src - (BYTE *)&rdio) + rdio.ReparseGuid.SubstituteNameLength) >= sizeof(rdio) ){
			rdio.ReparseTag = 0;
		}else{
			src[rdio.ReparseGuid.SubstituteNameLength / sizeof(WCHAR)] = '\0';
			if ( memcmp(src, L"\\??\\", 4 * sizeof(WCHAR)) == 0 ) src += 4;
			strcpyWToT(pathbuf, src, VFPS);
		}
	}
	CloseHandle(hFile);
	return rdio.ReparseTag;
}
