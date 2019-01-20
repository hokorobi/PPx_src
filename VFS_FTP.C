/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System				～ FTP処理 ～
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <windowsx.h>
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "VFS_FF.H"
#pragma hdrstop

const TCHAR loginrestring[] = T("&Save ID,password");

ValueWinAPI(InternetOpen);
ValueWinAPI(InternetConnect);
// ValueWinAPI(FtpGetCurrentDirectory);
// DefineWinAPI(BOOL,InternetGetLastResponseInfo,(DWORD *lpdwError,TCHAR *lpszBuffer,DWORD *lpdwBufferLength));

LOADWINAPISTRUCT FTPDLL[] = {
	LOADWINAPI1T(InternetOpen),
	LOADWINAPI1T(InternetConnect),
	LOADWINAPI1 (InternetCloseHandle),

//	LOADWINAPI1T(InternetGetLastResponseInfo),

	LOADWINAPI1T(FtpFindFirstFile),
	LOADWINAPI1T(InternetFindNextFile),
	LOADWINAPI1T(FtpSetCurrentDirectory),
//	LOADWINAPI1T(FtpGetCurrentDirectory),
	LOADWINAPI1T(FtpCreateDirectory),
	LOADWINAPI1T(FtpRemoveDirectory),

	LOADWINAPI1T(FtpGetFile),
	LOADWINAPI1T(FtpPutFile),
	LOADWINAPI1T(FtpRenameFile),
	LOADWINAPI1T(FtpDeleteFile),
	{NULL,NULL}
};

/*--------------------------------------
 初期化
--------------------------------------*/
BOOL LoadFtp32(void)
{
	if ( hWininet != NULL ) return TRUE;
	hWininet = LoadWinAPI("WININET.DLL",NULL,FTPDLL,LOADWINAPI_LOAD_ERRMSG);
	if ( hWininet == NULL ) return FALSE;
//		xmessage(XM_FaERRd,MES_WITD);
	return TRUE;
}
// ftp://[[user[:password]@]host[:port]/path[A formcode | E formcode | I | L digits]

void SaveFtpAccount(HWND hDlg)
{
	TCHAR host[VFPS],buf[0x1000],*idpass,*p;

	idpass = (TCHAR *)GetWindowLongPtr(hDlg,DWLP_USER);
	GetDlgItemText(hDlg,IDS_LOGINPATH,host,VFPS);
	p = idpass + tstrlen(idpass) + 1;
	p = p + tstrlen(p) + 1;
	WriteEString(buf,(BYTE *)idpass,TSTROFF32(p - idpass));
	SetCustTable(T("_IDpwd"),host,buf,TSTRSIZE(buf));
}

ERRORCODE GetInetError(void)
{
	ERRORCODE err;

	err = GetLastError();
	if ( err == ERROR_INTERNET_LOGIN_FAILURE ){
		err = ERROR_LOGON_FAILURE;
	}else if ( err == ERROR_INTERNET_CANNOT_CONNECT ){
		err = ERROR_PATH_NOT_FOUND;
	}else if ( err == ERROR_INTERNET_EXTENDED_ERROR ){
		err = ERROR_REQ_NOT_ACCEP;
	}else if ( (err >= 12000) && (err < 12800) ){
		XMessage(NULL,NULL,XM_DbgLOG,T("Ftp Error : %d"),err);
		err = ERROR_NETWORK_ACCESS_DENIED;
	}
	return err;
}

INT_PTR CALLBACK FtpPassDlgBox(HWND hDlg,UINT iMsg,WPARAM wParam,LPARAM lParam)
{
	switch(iMsg){
//-----------------------------------------------------------------------------
		case WM_INITDIALOG: {
			TCHAR *p;

			SetWindowLongPtr(hDlg,DWLP_USER,(LONG_PTR)lParam);
			CenterWindow(hDlg);
			LocalizeDialogText(hDlg,IDD_LOGIN);
			p = (TCHAR *)lParam;
			SetDlgItemText(hDlg,IDS_LOGINPATH,p);
			SetDlgItemText(hDlg,IDX_LOGINRE,loginrestring);
			p += tstrlen(p) + 1;
			SetDlgItemText(hDlg,IDE_LOGINUSER,p);
			SetDlgItemText(hDlg,IDE_LOGINPASS,p + tstrlen(p) + 1);
			break;
		}
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDOK: {
					TCHAR *p;

					p = (TCHAR *)GetWindowLongPtr(hDlg,DWLP_USER);
					GetDlgItemText(hDlg,IDE_LOGINUSER,p,100);
					p += tstrlen(p) + 1;
					GetDlgItemText(hDlg,IDE_LOGINPASS,p,100);
					if ( IsDlgButtonChecked(hDlg,IDX_LOGINRE) ){
						SaveFtpAccount(hDlg);
					}
					EndDialog(hDlg,1);
					break;
				}
				case IDCANCEL:
					EndDialog(hDlg,0);
					break;
			}
			break;

		default:
			return FALSE;
	}
	return TRUE;
}

// ftp://[[user[:password]@]host[:port]/path[A formcode | E formcode | I | L digits]

DWORD OpenFtp(TCHAR *url,HANDLE *hFtp)
{
	TCHAR *user = NULL,*pass = NULL,*host;
	INTERNET_PORT port = INTERNET_DEFAULT_FTP_PORT;
	TCHAR hostbuf[VFPS],path[VFPS],*p;
	BOOL usedlg = FALSE;
	DWORD result,mode = INTERNET_FLAG_PASSIVE;

	if ( LoadFtp32() == FALSE ) return ERROR_DLL_INIT_FAILED;

	hFtp[FTPHOST] = NULL;
	hFtp[FTPINET] = DInternetOpen(T("ftp"),
						INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,0);
	if ( hFtp[FTPINET] == NULL ) return GetInetError();

	url += 6; //"ftp://"
	tstrcpy(hostbuf,url);
								// path抽出
	p = tstrchr(hostbuf,'/');
	if ( p ){
		tstrcpy(path,p);
		*p = '\0';
	}else{
		tstrcpy(path,T("/"));
	}

								// user抽出
	p = tstrchr(hostbuf,'@');
	if ( p ){
		host = p + 1;
		user = hostbuf;
		*p = '\0';
		p = tstrchr(hostbuf,':');
		if ( p ){
			pass = p + 1;
			*p = '\0';
		}
		if ( host == (hostbuf + 1) ) usedlg = TRUE;
	}else{
		host = hostbuf;
	}
								// port抽出
	p = tstrchr(host,':');
	if ( p ){
		*p++ = '\0';
		port = (INTERNET_PORT)GetNumber((const TCHAR **)&p);
	}
								// キャッシュを使うか
	if ( (pass == NULL) && !tstrcmp(AuthHostCache,host) ){
		user = AuthUserCache;
		pass = AuthPassCache;
	}
								// 記憶済み設定を利用するか
	if ( pass == NULL ){
		TCHAR buf[0x1000],strbuf[VFPS],*bufp;

		if ( NO_ERROR == GetCustTable(T("_IDpwd"),host,buf,sizeof(buf)) ){
			bufp = buf;

			ReadEString(&bufp,strbuf,sizeof(strbuf));
			user = strbuf;
			pass = strbuf + tstrlen(strbuf) + 1;
		}
	}

	for ( ; ; ){
		TCHAR buf[VFPS * 2];

		if ( usedlg ){
			tstrcpy(buf,host);
			p = buf + tstrlen(host) + 1;
			if ( user != NULL ){
				tstrcpy(p,user);
			}else{
				*p = '\0';
			}
			p += tstrlen(p) + 1;
			if ( pass != NULL ){
				tstrcpy(p,pass);
			}else{
				*p = '\0';
			}
			if ( PPxDialogBoxParam(DLLhInst,MAKEINTRESOURCE(IDD_LOGIN),
					NULL,FtpPassDlgBox,(LPARAM)buf) <= 0 ){
				result = ERROR_CANCELLED;
				goto error2;
			}
			user = buf;
			pass = buf + tstrlen(buf) + 1;
		}
		hFtp[FTPHOST] = DInternetConnect(hFtp[FTPINET],host,
				port,user,pass,INTERNET_SERVICE_FTP,mode,0);
		if ( hFtp[FTPHOST] != NULL ) break;

		result = GetInetError();
		// パッシブでは無理だった
		if ( (result == ERROR_REQ_NOT_ACCEP) && (mode == INTERNET_FLAG_PASSIVE) ){
			mode = 0;
			continue;
		}
		if ( result == ERROR_LOGON_FAILURE ){
			usedlg = TRUE;
			continue;
		}
		goto error2;
	}
	if ( (tstrlen(host) < TSIZEOF(AuthHostCache) ) &&
		 ((user != NULL) || (pass != NULL)) ){
		tstrcpy(AuthHostCache,host);
		tstrcpy(AuthUserCache,(user != NULL) ? user : NilStr);
		tstrcpy(AuthPassCache,(pass != NULL) ? pass : NilStr);
	}

	if ( DFtpSetCurrentDirectory(hFtp[FTPHOST],path) == FALSE ){
		result = GetInetError();
		goto error;
	}
	return NO_ERROR;

error:
	DInternetCloseHandle(hFtp[FTPHOST]);
error2:
	DInternetCloseHandle(hFtp[FTPINET]);
	return result;
}

DWORD FTPff(FF_FTP *FTP,const TCHAR *url,WIN32_FIND_DATA *findfile)
{
	TCHAR *mask;
	TCHAR hostbuf[VFPS],*p;
	int result;

	tstrcpy(hostbuf,url);
								// マスク抽出
	p = tstrchr(hostbuf,'\\');
	if ( p ){
		*p = '\0';
		mask = p + 1;
	}else{
		mask = T("*");
	}
	result = OpenFtp(hostbuf,FTP->hFtp);
	if ( result != NO_ERROR ) return result;

	FTP->hFtpFF = DFtpFindFirstFile(FTP->hFtp[FTPHOST],mask,findfile,INTERNET_FLAG_RELOAD,0);
	if ( FTP->hFtpFF == NULL ) return GetInetError();
	if ( findfile->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
		findfile->nFileSizeLow = findfile->nFileSizeHigh = 0;
	}
	return NO_ERROR;
}

BOOL FTPnf(FF_FTP *FTP,WIN32_FIND_DATA *findfile)
{
	BOOL result;

	result = DInternetFindNextFile(FTP->hFtpFF,findfile);
	if ( IsTrue(result) && (findfile->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
		findfile->nFileSizeLow = findfile->nFileSizeHigh = 0;
	}
	return result;
}

BOOL FTPclose(FF_FTP *FTP)
{
	if ( hWininet == NULL ) return FALSE;
	DInternetCloseHandle(FTP->hFtpFF);
	DInternetCloseHandle(FTP->hFtp[FTPHOST]);
	DInternetCloseHandle(FTP->hFtp[FTPINET]);
	return TRUE;
}

ERRORCODE DeleteFtpEntry(const TCHAR *path,DWORD attributes)
{
	TCHAR pathbuf[VFPS],*name;
	HINTERNET hFtp[2];
	ERRORCODE result;

	tstrcpy(pathbuf,path);
	name = VFSFindLastEntry(pathbuf);
	*name++ = '\0';
	result = OpenFtp(pathbuf,hFtp);
	if ( result != NO_ERROR ) return result;
	if ( attributes & FILE_ATTRIBUTE_DIRECTORY ){
		if ( IsTrue(DFtpRemoveDirectory(hFtp[FTPHOST],name)) ){
			result = NO_ERROR;
		}else{
			result = GetInetError();
		}
	}else{
		if ( IsTrue(DFtpDeleteFile(hFtp[FTPHOST],name)) ){
			result = NO_ERROR;
		}else{
			result = GetInetError();
		}
	}
	DInternetCloseHandle(hFtp[FTPHOST]);
	DInternetCloseHandle(hFtp[FTPINET]);
	return result;
}

ERRORCODE CreateFtpDirectory(const TCHAR *path)
{
	TCHAR pathbuf[VFPS],*name;
	HINTERNET hFtp[2];
	ERRORCODE result;

	tstrcpy(pathbuf,path);
	name = VFSFindLastEntry(pathbuf);
	*name++ = '\0';
	result = OpenFtp(pathbuf,hFtp);
	if ( result != NO_ERROR ) return result;
	if ( IsTrue(DFtpCreateDirectory(hFtp[FTPHOST],name)) ){
		result = NO_ERROR;
	}else{
		result = GetInetError();
	}
	DInternetCloseHandle(hFtp[FTPHOST]);
	DInternetCloseHandle(hFtp[FTPINET]);
	return result;
}


ERRORCODE MoveFtpFile(const TCHAR *ExistingFileName,const TCHAR *NewFileName)
{
	TCHAR pathbuf[VFPS],newpathbuf[VFPS],*name,*newname;
	HINTERNET hFtp[2];
	ERRORCODE result;

	tstrcpy(pathbuf,ExistingFileName);
	name = VFSFindLastEntry(pathbuf);
	*name++ = '\0';

	tstrcpy(newpathbuf,NewFileName);
	newname = VFSFindLastEntry(newpathbuf) + 1;

	result = OpenFtp(pathbuf,hFtp);
	if ( result != NO_ERROR ) return result;
	if ( IsTrue(DFtpRenameFile(hFtp[FTPHOST],name,newname)) ){
		result = NO_ERROR;
	}else{
		result = GetInetError();
	}
	DInternetCloseHandle(hFtp[FTPHOST]);
	DInternetCloseHandle(hFtp[FTPINET]);
	return result;
}
