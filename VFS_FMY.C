/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System						「:」処理
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "VFS_FF.H"
#pragma hdrstop

void MakeDriveList(FF_MC *mc)
{
	DWORD X_dlf;
	TCHAR textbuf[VFPS + VFPS + 16];
	TCHAR pathbuf[VFPS + 2];

	X_dlf = GetCustDword(StrX_dlf,0);
							// 変数の初期化
	ThInit(&mc->dirs);
	ThInit(&mc->files);
	mc->d_off = 0;
	mc->f_off = 0;
										// 「#:」==============================
	ThAddString(&mc->dirs,(X_dlf & XDLF_ROOTJUMP) ? T("#:\\") : T("#:"));
	if ( X_dlf & XDLF_DISPDRIVETITLE ){
		mc->dirs.top -= sizeof(TCHAR);
		ThCatString(&mc->dirs,T("> "));
		ThCatString(&mc->dirs,MessageText(MES_FEXP));
		mc->dirs.top += sizeof(TCHAR);
	}
										// 「\\」==============================
	ThAddString(&mc->dirs,T("\\\\"));
	if ( X_dlf & XDLF_DISPDRIVETITLE ){
		mc->dirs.top -= sizeof(TCHAR);
		ThCatString(&mc->dirs,T("> "));
		ThCatString(&mc->dirs,MessageText(MES_FNET));
		mc->dirs.top += sizeof(TCHAR);
	}
										// 「A:」～「Z:」 =====================
	if ( !(X_dlf & XDLF_NODRIVES) ){
		DWORD drive;
		int i;
		TCHAR name[] = T("A:\\");
		#define DRIVE_OFFLINE B30

		if ( !(X_dlf & XDLF_ROOTJUMP) ) name[2] = '\0';
		drive = GetLogicalDrives();
		for ( i = 0 ; i < 26 ; i++ ){
			if ( !(drive & LSBIT) ){ // ドライブ無し…未接続ネットワークの確認
				pathbuf[0] = '\0';
				wsprintf(textbuf,T("Network\\%c"),(TCHAR)(i + 'A'));
				GetRegString(HKEY_CURRENT_USER,textbuf,RPATHSTR,pathbuf,TSIZEOF(pathbuf));
				if ( pathbuf[0] != '\0' ){
					setflag(drive,LSBIT | DRIVE_OFFLINE);
				}
			}
			if ( drive & LSBIT ){
				if ( X_dlf & XDLF_DISPDRIVETITLE ){
					TCHAR *p;

					if ( drive & DRIVE_OFFLINE ){
						wsprintf(textbuf,T("%s> offline(%s)"),name,pathbuf);
					}else{
						p = textbuf + wsprintf(textbuf,T("%s> "),name);
						GetDriveNameTitle(p,name[0]);
					}
					ThAddString(&mc->dirs,textbuf);
				}else{
					ThAddString(&mc->dirs,name);
				}
			}
			drive >>= 1;
			name[0]++;
		}
		#undef DRIVE_OFFLINE
	}
								// Net share History ==========================
	if ( !(X_dlf & XDLF_NODISPSHARE) ){
		int index = 0;

		UsePPx();
		for ( ; ; ){
			const TCHAR *hisp;
			TCHAR *p,*lp;
			int mode;

			hisp = EnumHistory(PPXH_PPCPATH,index++);
			if ( hisp == NULL ) break;
										// GNC, #: は無視
			p = VFSGetDriveType(hisp,&mode,NULL);
			if ( (p == NULL) || (mode != VFSPT_UNC) )
			{
				continue;
			}
			if ( mode == VFSPT_UNC ){
				p = FindPathSeparator(p);
				if ( (p == NULL) || !*(p + 1) ) continue;	//「\\」「\\xxx」「\\xxx\」無視
										// 「\\xxx\yyy」形式になる末端を検索
				lp = FindPathSeparator(p + 1);
				if ( lp != NULL ){
					p = lp;
				}else{
					p += tstrlen(p);
				}
			}
												// とりあえず、取得
			tstrcpy(textbuf,hisp);
			p = textbuf + (p - hisp);
			if ( (mode == VFSPT_UNC) && !(X_dlf & XDLF_ROOTJUMP) ){
				*p++ = '\\';
				*p++ = ':';
			}
			*p = '\0';
			AddDriveList(&mc->dirs,textbuf);
		}
		FreePPx();
	}
										// Menu ============================
	if ( !(X_dlf & XDLF_NOM_PJUMP) ){
		const TCHAR *MenuName;
		int index = 0;

		MenuName = (X_dlf & XDLF_USEMDRIVES) ? StrMDrives : PathJumpName;

		pathbuf[0] = ':';
		while ( EnumCustTable(index++,MenuName,
				pathbuf + 1,textbuf,sizeof(textbuf)) >= 0 ){
			PP_ExtractMacro(NULL,NULL,NULL,textbuf,textbuf,XEO_DISPONLY);
			if ( textbuf[0] == '\0' ) continue;
			if ( textbuf[0] == ':' ) continue;
			if ( textbuf[0] == '?' ) continue;
			if ( (textbuf[0]=='%') && (textbuf[1]=='M') ) continue; // メニュー

			if ( X_dlf & XDLF_DISPALIAS ){
				AddDriveList(&mc->dirs,pathbuf);
			}else{
				VFSFullPath(NULL,textbuf,NULL);

				if ( !(X_dlf & XDLF_NODRIVES) ){
					if ( X_dlf & XDLF_ROOTJUMP ){
						if ( textbuf[3] == '\0' ) continue;
					}else{
						if ( textbuf[2] == '\0' ) continue;
					}
				}
//				wsprintf(textbuf,T("%s\\> %s"),param,pathbuf + 1);
				AddDriveList(&mc->dirs,textbuf);
			}
		}
	}
	ThAddString(&mc->files,NilStr);
}
