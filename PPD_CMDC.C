/*-----------------------------------------------------------------------------
	Paper Plane cUI									各種コマンド - *command
-----------------------------------------------------------------------------*/
#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "WINAPI.H"
#include <shlobj.h>
#include "PPX.H"
#include "PPXVER.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "PPCOMMON.RH"
#include "PPD_DEF.H"
#include "CALC.H"
#include "PPX_64.H"
#include "PPXCMDS.C"
#pragma hdrstop

#define USEWINHASH 0

const TCHAR Str1[] = T("1");

const int KeysShift[] = { VK_SHIFT, VK_LSHIFT, VK_RSHIFT };
const int KeysCtrl[] = { VK_CONTROL, VK_LCONTROL, VK_RCONTROL };
const int KeysAlt[] = { VK_MENU, VK_LMENU, VK_RMENU };

#define SKEY_BOTH 0
#define SKEY_LEFT 1
#define SKEY_RIGHT 2
#define SKEY_MASK 3
#define SKEY_FIX B8

struct PACKINFO {
	PPXAPPINFO info, *parent;
	TCHAR *path; // 作成する書庫ファイル名
	TCHAR *files; // 書庫に入れるファイル・ディレクトリ(indiv mode)
	DWORD attr; // files の属性(indiv mode)
};


#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((LONG)0x00000000L)
#endif

#ifndef BCRYPT_SHA1_ALGORITHM
#define BCRYPT_MD5_ALGORITHM  L"MD5"
#define BCRYPT_SHA1_ALGORITHM L"SHA1"
#define BCRYPT_OBJECT_LENGTH L"ObjectLength"
#define BCRYPT_HASH_LENGTH L"HashDigestLength"
#endif

#ifndef BCRYPT_ECCPUBLIC_BLOB
#define BCRYPT_ECDSA_P256_ALGORITHM L"ECDSA_P256"
#define BCRYPT_ECCPUBLIC_BLOB L"ECCPUBLICBLOB"
#endif

#define UseHashAlgorithm	BCRYPT_SHA1_ALGORITHM
#define UseKeyAlgorithm		BCRYPT_ECDSA_P256_ALGORITHM

typedef PVOID BCRYPT_HANDLE;
typedef PVOID BCRYPT_ALG_HANDLE;
typedef PVOID BCRYPT_KEY_HANDLE;
typedef PVOID BCRYPT_HASH_HANDLE;

typedef struct {
  LPWSTR pszName;
  ULONG  dwClass;
  ULONG  dwFlags;
} xBCRYPT_ALGORITHM_IDENTIFIER;

DefineWinAPI(LONG, BCryptOpenAlgorithmProvider, (BCRYPT_ALG_HANDLE *, LPCWSTR, LPCWSTR, ULONG));
DefineWinAPI(LONG, BCryptGetProperty, (BCRYPT_HANDLE, LPCWSTR, PUCHAR, ULONG, ULONG *, ULONG));
DefineWinAPI(LONG, BCryptCreateHash, (BCRYPT_ALG_HANDLE, BCRYPT_HASH_HANDLE *, PUCHAR, ULONG, PUCHAR, ULONG, ULONG));
DefineWinAPI(LONG, BCryptHashData, (BCRYPT_HASH_HANDLE, PUCHAR, ULONG, ULONG));
DefineWinAPI(LONG, BCryptFinishHash, (BCRYPT_HASH_HANDLE, PUCHAR, ULONG, ULONG));
DefineWinAPI(LONG, BCryptDestroyHash, (BCRYPT_HASH_HANDLE));
DefineWinAPI(LONG, BCryptCloseAlgorithmProvider, (BCRYPT_ALG_HANDLE, ULONG));
DefineWinAPI(LONG, BCryptImportKeyPair, (BCRYPT_ALG_HANDLE, BCRYPT_KEY_HANDLE, LPCWSTR, BCRYPT_KEY_HANDLE *, PUCHAR, ULONG, ULONG));
DefineWinAPI(LONG, BCryptVerifySignature, (BCRYPT_KEY_HANDLE, VOID *, PUCHAR, ULONG, PUCHAR, ULONG, ULONG));
DefineWinAPI(LONG, BCryptDestroyKey, (BCRYPT_KEY_HANDLE));
#if USEWINHASH
DefineWinAPI(LONG, BCryptEnumAlgorithms, (ULONG, ULONG *, xBCRYPT_ALGORITHM_IDENTIFIER **, ULONG));
DefineWinAPI(VOID, BCryptFreeBuffer, (PVOID));
#endif

LOADWINAPISTRUCT BCRYPTDLL[] = {
	LOADWINAPI1(BCryptOpenAlgorithmProvider),
	LOADWINAPI1(BCryptGetProperty),
	LOADWINAPI1(BCryptCreateHash),
	LOADWINAPI1(BCryptHashData),
	LOADWINAPI1(BCryptFinishHash),
	LOADWINAPI1(BCryptDestroyHash),
	LOADWINAPI1(BCryptCloseAlgorithmProvider),
	LOADWINAPI1(BCryptImportKeyPair),
	LOADWINAPI1(BCryptVerifySignature),
	LOADWINAPI1(BCryptDestroyKey),
#if USEWINHASH
	LOADWINAPI1(BCryptEnumAlgorithms),
	LOADWINAPI1(BCryptFreeBuffer),
#endif
	{NULL, NULL}
};

BOOL GetImageHash(BYTE *Image, DWORD ImageSize, BYTE **HashData, DWORD *HashSize)
{
	BCRYPT_ALG_HANDLE hAlg;
	BCRYPT_HASH_HANDLE hHash;
	DWORD resultsize;
	DWORD worksize;
	BYTE *hashwork;

	if ( STATUS_SUCCESS != DBCryptOpenAlgorithmProvider(&hAlg, UseHashAlgorithm, NULL, 0) ){
		return FALSE;
	}

	DBCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (LPBYTE)&worksize, sizeof(worksize), &resultsize, 0);
	hashwork = (BYTE *)HeapAlloc(ProcHeap, 0, worksize);
	if ( STATUS_SUCCESS != DBCryptCreateHash(hAlg, &hHash, hashwork, worksize, NULL, 0, 0) ){
		HeapFree(ProcHeap, 0, hashwork);
		DBCryptCloseAlgorithmProvider(hAlg, 0);
		return FALSE;
	}
	DBCryptHashData(hHash, Image, ImageSize, 0);
	DBCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (LPBYTE)HashSize, sizeof(DWORD), &resultsize, 0);
	*HashData = (LPBYTE)HeapAlloc(ProcHeap, 0, *HashSize);
	DBCryptFinishHash(hHash, *HashData, *HashSize, 0);

	DBCryptDestroyHash(hHash);
	HeapFree(ProcHeap, 0, hashwork);
	DBCryptCloseAlgorithmProvider(hAlg, 0);
	return TRUE;
}

BYTE DefaultPublicKey[] = {
 0x45, 0x43, 0x53, 0x31, 0x20, 0x00, 0x00, 0x00,
 0x1D, 0xA8, 0x8F, 0xA9, 0xB1, 0xD7, 0x80, 0xAD,
 0x4B, 0x72, 0x1E, 0x76, 0x60, 0xD3, 0x8C, 0xD7,
 0xB1, 0x6A, 0x65, 0x74, 0x72, 0x37, 0x93, 0x06,
 0x43, 0x90, 0x92, 0x8A, 0x1D, 0x4A, 0x76, 0x9D,
 0xA8, 0x09, 0xAB, 0x13, 0x92, 0x54, 0xEA, 0x36,
 0x3C, 0x97, 0x1C, 0xEB, 0x53, 0xA9, 0x7C, 0xD6,
 0xFB, 0x72, 0x84, 0x24, 0xFB, 0x3A, 0x80, 0x86,
 0x6D, 0x1F, 0xFF, 0xC1, 0xB5, 0xD9, 0x84, 0x3F,
};
#define DefaultPublicKeySize 0x48

#define VERIFYZIP_SUCCEDD 0
#define VERIFYZIP_FAILED 1
#define VERIFYZIP_NOSUPPORTVERIFY 2

int VerifyZipImage(const TCHAR *SignedFileName)
{
	BYTE *fileimage = NULL;
	DWORD filesize;
	int success = VERIFYZIP_FAILED;

	BCRYPT_ALG_HANDLE hAlg = NULL;
	BCRYPT_KEY_HANDLE hKey = NULL;

	HANDLE hBCRYPT = LoadWinAPI("BCRYPT.DLL", NULL, BCRYPTDLL, LOADWINAPI_LOAD);

	if ( hBCRYPT == NULL ) return VERIFYZIP_NOSUPPORTVERIFY;

	if ( NO_ERROR != LoadFileImage(SignedFileName, 0x1000, (char **)&fileimage, NULL, &filesize) ){
		FreeLibrary(hBCRYPT);
		return VERIFYZIP_NOSUPPORTVERIFY;
	}

	if ( STATUS_SUCCESS != DBCryptOpenAlgorithmProvider(&hAlg, UseKeyAlgorithm, NULL, 0) ){
		success = VERIFYZIP_NOSUPPORTVERIFY;
	}

	if ( success ){
		if ( STATUS_SUCCESS != DBCryptImportKeyPair(hAlg, NULL, BCRYPT_ECCPUBLIC_BLOB, &hKey, DefaultPublicKey, DefaultPublicKeySize, 0) ){
			success = VERIFYZIP_NOSUPPORTVERIFY;
		}
	}

	if ( success ) for(;;){
		DWORD DataSize;
		DWORD SignatureSize;
		DWORD HashDataSize;
		BYTE *HashData;
		BYTE *Signature;

		success = VERIFYZIP_FAILED;
		if ( filesize < (DWORD)0x100 ) break;
		SignatureSize = ((*(fileimage + filesize - 4) - 'a') << 8) + ((*(fileimage + filesize - 3) - 'a') << 4) + (*(fileimage + filesize - 2) - 'a');
		if ( SignatureSize >= 0x100 ) break;;

		DataSize = filesize - SignatureSize;
		if ( *fileimage == 'P' ) DataSize -= sizeof(WORD);
		Signature = fileimage + filesize - SignatureSize;
		SignatureSize = (SignatureSize - 3 - 1) / 2;

		{
			BYTE *src, *dest;
			DWORD size = SignatureSize;

			src = dest = Signature;
			while ( size ){
				*dest++ = (BYTE)(((*src - 'a') << 4) + (*(src + 1) - 'a'));
				src += 2;
				size--;
			}
		}

		if ( !GetImageHash(fileimage, DataSize, &HashData, &HashDataSize) ){
			break;
		}

		if ( STATUS_SUCCESS == DBCryptVerifySignature(hKey, NULL, HashData, HashDataSize, Signature, SignatureSize, 0) ){
			success = VERIFYZIP_SUCCEDD;
		}
		HeapFree(ProcHeap, 0, HashData);
		break;
	}
	if ( hKey != NULL ) DBCryptDestroyKey(hKey);
	if ( hAlg != NULL ) DBCryptCloseAlgorithmProvider(hAlg, 0);
	HeapFree(ProcHeap, 0, fileimage);
	FreeLibrary(hBCRYPT);
	return success;
}
#if USEWINHASH
void GetWindowHashList(HMENU hMenuDest, DWORD *PopupID)
{
	HANDLE hBCRYPT = LoadWinAPI("BCRYPT.DLL", NULL, BCRYPTDLL, LOADWINAPI_LOAD);
	ULONG algcount;
	xBCRYPT_ALGORITHM_IDENTIFIER *alglist;
#ifndef UNICODE
	char bufA[128];
	#define AlgName bufA
#else
	#define AlgName alglist[i].pszName
#endif
	if ( hBCRYPT == NULL ) return;

	if ( NO_ERROR ==
			DBCryptEnumAlgorithms(2, &algcount, &alglist, 0) ){
		ULONG i;

		for ( i = 0 ; i < algcount ; i++ ){
			#ifndef UNICODE
				UnicodeToAnsi(alglist[i].pszName, AlgName, 128);
			#endif
			AppendMenuString(hMenuDest, (*PopupID)++, AlgName);
		}
		DBCryptFreeBuffer(alglist);
	}
	FreeLibrary(hBCRYPT);
	return;
}
#endif
typedef struct {
	PPXAPPINFO info, *parent;
	const TCHAR *cmdname;
	const TCHAR *arg;
} USERCOMMANDSTRUCT;

DWORD_PTR USECDECL UserCommandInfo(USERCOMMANDSTRUCT *info, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case PPXCMDID_FUNCTION:
			if ( !tstrcmp(uptr->funcparam.param, T("ARG")) ){
				const TCHAR *ptr;
				int index;

				ptr = uptr->funcparam.optparam;
				index = GetDigitNumber32(&ptr);
				if ( index <= 0 ){
					if ( index == 0 ){ // arg(0) コマンド名
						tstrcpy( uptr->funcparam.dest,
								(info->cmdname != NULL) ? info->cmdname : info->arg);
					}else{
						*uptr->funcparam.dest = '\0';
					}
				}else{ // arg(1...)
					ptr = info->arg;
					if ( info->cmdname == NULL ) ptr += tstrlen(ptr) + 1;
					for (;;){
						const TCHAR *oldptr;

						oldptr = ptr;
						GetCommandParameter(&ptr, uptr->funcparam.dest, CMDLINESIZE);
						if ( --index == 0 ){
							size_t len = ptr - oldptr;

							if ( len >= CMDLINESIZE ){
								TCHAR *longbuf;

								len += 1; // Nil分
								longbuf = HeapAlloc(ProcHeap, 0, TSTROFF(len));
								if ( longbuf != NULL ){
									ptr = oldptr;
									GetCommandParameter(&ptr, longbuf, len);
									uptr->funcparam.dest = longbuf;
								}
							}
							break;
						}
						*uptr->funcparam.dest = '\0';
						if ( NextParameter(&ptr) == FALSE ) break;
					}
				}
				return 1;
			}
			// default へ

		default:
			return info->parent->Function(info->parent, cmdID, uptr);
	}
}

void UserCommand(EXECSTRUCT *Z, const TCHAR *cmdname, const TCHAR *args, const TCHAR *cmdline, TCHAR *dest)
{
	USERCOMMANDSTRUCT ucs;

	ucs.info = *Z->Info;
	ucs.info.Function = (PPXAPPINFOFUNCTION)UserCommandInfo;
	ucs.parent = Z->Info;
	ucs.cmdname = cmdname;
	ucs.arg = args;
	Z->result = PP_ExtractMacro(ucs.info.hWnd, &ucs.info, NULL, cmdline, dest,
			(dest == NULL) ? XEO_EXTRACTEXEC : XEO_EXTRACTEXEC | XEO_EXTRACTLONG);
	if ( Z->result == ERROR_EX_RETURN ) Z->result = NO_ERROR;
}

void USEFASTCALL ExecOnConsole(EXECSTRUCT *Z, const TCHAR *param)
{
	PPXCMD_PPBEXEC ppbe;

	ppbe.name = param;
	ppbe.flag = Z->flags;
	ppbe.path = Z->curdir; // CID_FILE_EXEC - ZSetCurrentDir で取得済み
	Z->ExitCode = (DWORD)PPxInfoFunc(Z->Info, PPXCMDID_PPBEXEC, &ppbe);
}

TCHAR * USEFASTCALL ZGetFilePathParam(EXECSTRUCT *Z, const TCHAR **ptr, TCHAR *path)
{
	GetCommandParameter(ptr, path, VFPS);
	if ( *path == '\0' ) return path;
	return VFSFixPath(NULL, path, GetZCurDir(Z), VFSFIX_PATH | VFSFIX_NOFIXEDGE);
}

void ZapMain(EXECSTRUCT *Z, const TCHAR *command, const TCHAR *exepath, const TCHAR *param, const TCHAR *path)
{
	TCHAR msg[VFPS];
	HANDLE hProcess;

	if ( NULL != (hProcess = PPxShellExecute(Z->hWnd, command, exepath,
			param, path, Z->flags, msg)) ){
		if ( Z->flags & XEO_WAITIDLE ){
			WaitForInputIdle(hProcess, 30*60*1000);
		}
		if ( Z->flags & XEO_SEQUENTIAL ){
			if( WaitJobDialog(Z->hWnd, hProcess, exepath, 0) == FALSE){
				Z->result = ERROR_CANCELLED;
			}
		}
		if ( Z->flags & (XEO_WAITIDLE | XEO_SEQUENTIAL) ){
			CloseHandle(hProcess);
		}
	}else{
		PopupErrorMessage(Z->hWnd, command, msg);
		Z->result = ERROR_CANCELLED;
	}
}

void ComZExecPPx(EXECSTRUCT *Z, const TCHAR *ppxname, const TCHAR *param)
{
	TCHAR buf[CMDLINESIZE], pathbuf[MAX_PATH];
	const TCHAR *ptr = param, *path;
	DWORD attr;

	path = GetZCurDir(Z);
	attr = GetFileAttributesL(path);
	if ( ((attr != BADATTR) && !(attr & FILE_ATTRIBUTE_DIRECTORY)) ||
		 !memcmp(path, T("\\\\.\\"), sizeof(TCHAR) * 4) ){
		MakeTempEntry(MAX_PATH, pathbuf, FILE_ATTRIBUTE_DIRECTORY);
		path = pathbuf;
	}
	if ( (GetOption(&ptr, buf) == '-') && !tstrcmp(buf + 1, T("RUNAS")) ){
		CatPath(buf, DLLpath, ppxname);
		if ( PPxShellExecute(Z->hWnd, T("RUNAS"), buf, ptr, path, 0, pathbuf) == NULL ){
			PopupErrorMessage(Z->hWnd, buf, pathbuf);
			Z->result = ERROR_CANCELLED;
		}
		return;
	}

	wsprintf(buf, T("\"%s\\%s\" %s"), DLLpath, ppxname, param);
	ComExecSelf(Z->hWnd, buf, path, Z->flags, &Z->ExitCode);
}

int USECDECL SureDialog(const TCHAR *message)
{
	return PMessageBox(NULL, message, NilStr, MB_YESNO) == IDYES;
}

void CustCmdSub(EXECSTRUCT *Z, TCHAR *text, TCHAR *textmax, BOOL reload)
{
	TCHAR *log = NULL;
	int result;

	result = PPcustCStore(text, textmax, PPXCUSTMODE_APPEND, &log, SureDialog);
	if ( log ){
		if ( reload ){
			TCHAR *logp = log;

			for ( ;; logp++ ){
				TCHAR c;

				c = *logp;
				if ( (c == ' ') || (c == '\r') || (c == '\n') ) continue;
				break;
			}
			PPxInfoFunc(Z->Info, PPXCMDID_SETPOPLINE, (void *)logp);
		}
		HeapFree(ProcHeap, 0, log);
	}
	if ( reload && ((result & (PPCUSTRESULT_RELOAD | 1)) == 1) ){
		PPxPostMessage(WM_PPXCOMMAND, K_Lcust, GetTickCount());
	}
}

void CustFile(EXECSTRUCT *Z, const TCHAR *filename, BOOL reload)
{
	TCHAR *mem, *text, *textmax;		// カスタマイズ解析位置

	PPxSendMessage(WM_PPXCOMMAND, K_Scust, 0);
										// ファイル読み込み処理 ---------------
	if ( LoadTextImage(filename, &mem, &text, &textmax) != NO_ERROR ){
		PPErrorBox(Z->hWnd, filename, PPERROR_GETLASTERROR);
		return;
	}
	CustCmdSub(Z, text, textmax, reload);
	HeapFree(ProcHeap, 0, mem);
}

void CustLine(EXECSTRUCT *Z, const TCHAR *line, BOOL reload)
{
	TCHAR buf[CMDLINESIZE];
	TCHAR *p, *q, separator;

	PPxSendMessage(WM_PPXCOMMAND, K_Scust, 0);
	p = tstrchr(line, '=');
	q = tstrchr(line, ',');
	if ( p == NULL ){
		if ( q == NULL ) return;
		p = q;
	}else{
		if ( (q != NULL) && (p > q) ) p = q;
	}
	separator = *p;
	*p++ = '\0';
	q = tstrchr(line, ':');
	if ( (q != NULL) && (q < p) ){
		*q = '\0';
		wsprintf(buf, T("%s = {\n%s %c%s\n}"), line, q + 1, separator, p);
	}else{
		wsprintf(buf, T("%s %c%s"), line, separator, p);
	}
	CustCmdSub(Z, buf, buf + tstrlen(buf), reload);
}

BOOL GetSetParams(TCHAR **name, TCHAR **param)
{
	UTCHAR *p;
														// 指定無し
	if ( (UTCHAR)SkipSpace((const TCHAR **)name) < (UTCHAR)' ' ) return FALSE;
	p = (UTCHAR *)*name;
	for ( ; ; ){
		if ( *p <= (UTCHAR)' ' ){
			*p++ = '\0';
			if ( SkipSpace((const TCHAR **)&p) == '=' ){
				p++;
			}
			break;
		}
		if ( *p == '=' ){
			*p++ = '\0';
			break;
		}
		p++;
	}
	if ( (UTCHAR)SkipSpace((const TCHAR **)&p) < (UTCHAR)' ' ){
		*param = NULL;
	}else{
		*param = (TCHAR *)p;
	}
	return TRUE;
}

void ZSetCurrentDir(EXECSTRUCT *Z, TCHAR *olddir)
{
	GetCurrentDirectory(VFPS, olddir);
	SetCurrentDirectory(GetZCurDir(Z));
}

//-----------------------------------------------------------------------------
int USEFASTCALL SaveShiftKeys(const int *keysID, int requirekey)
{
	int oldkey, type = SKEY_BOTH;

	oldkey = GetAsyncKeyState(keysID[SKEY_BOTH]) & KEYSTATE_PUSH;
	if ( oldkey ^ requirekey ){ // シフト状態が異なる
		If_WinNT_Block {
			if ( (GetAsyncKeyState(keysID[SKEY_LEFT]) & KEYSTATE_PUSH) ^ requirekey ){
				type = SKEY_LEFT;
			}else if ( (GetAsyncKeyState(keysID[SKEY_RIGHT]) & KEYSTATE_PUSH) ^ requirekey ){
				type = SKEY_RIGHT;
			}
		}
		keybd_event((BYTE)keysID[type], 0, requirekey ? 0 : KEYEVENTF_KEYUP, 0);
		oldkey |= type | SKEY_FIX;
	}
	return oldkey;
}

void USEFASTCALL RestoreShiftKeys(const int *keysID, int oldkey)
{
	if ( oldkey & SKEY_FIX ){ // シフト状態が異なる
		// 右シフトキーはうまく戻せないので省略する
		if ( (oldkey & (KEYSTATE_PUSH | SKEY_MASK)) == (KEYSTATE_PUSH | SKEY_RIGHT) ){
			return;
		}
		keybd_event((BYTE)keysID[oldkey & SKEY_MASK], 0, (oldkey & KEYSTATE_PUSH) ? 0 : KEYEVENTF_KEYUP, 0);
	}
}

void CmdEmurateKeyInput(EXECSTRUCT *Z, const TCHAR *ptr)
{
	int oldctrl, oldalt, oldshift;
	const TCHAR *p;

	if ( SkipSpace(&ptr) != '\"' ){
		XMessage(Z->hWnd, T("%K"), XM_GrERRld, MES_EPRM);
		Z->result = ERROR_INVALID_PARAMETER;
		return;
	}
	p = ptr + 1;
	for ( ; ; ){
		UTCHAR c;
		BOOL winshift;
		int key, exflag;

		c = SkipSpace(&p);
		if ( c < ' ' ) break;
		if ( c == '\"' ){
			p++;
			break;
		}
		if ( SkipSpace(&p) == '`' ){
			p++;
			winshift = TRUE;
		}else{
			winshift = FALSE;
		}
		key = GetKeyCode(&p);
		if ( key < 0 ){
			XMessage(Z->hWnd, T("%K"), XM_GrERRld, MES_EPRM);
			Z->result = ERROR_INVALID_PARAMETER;
			break;
		}
		if ( key == K_NULL ){
			Sleep(100);
			continue;
		}
		if ( !(key & K_v) && !IsalnumA(key) ){
			int vkey;

			vkey = (int)VkKeyScan((TCHAR)(key & 0xff));
			key = (vkey & 0xff) | (key & 0xff00);
			if ( vkey & B8 ) setflag(key, K_s);
		}
		oldshift = SaveShiftKeys(KeysShift, (key & K_s) << 5);
		oldctrl  = SaveShiftKeys(KeysCtrl , (key & K_c) << 4);
		oldalt   = SaveShiftKeys(KeysAlt  , (key & K_a) << 3);
		if ( IsTrue(winshift) ) keybd_event(VK_RWIN, 0, 0, 0);

		exflag = 0;
		if ( key & K_v ){
			int vkey = key & 0xff;

			if ( ((vkey >= VK_PRIOR) && (vkey <= VK_DOWN)) ||
				 ((vkey >= VK_MULTIPLY) && (vkey <= VK_DIVIDE)) ){
				exflag = KEYEVENTF_EXTENDEDKEY;
			}
		}
		keybd_event((BYTE)key, 0, exflag, 0);
		keybd_event((BYTE)key, 0, exflag | KEYEVENTF_KEYUP, 0);

		if ( IsTrue(winshift) ) keybd_event(VK_RWIN, 0, KEYEVENTF_KEYUP, 0);
		RestoreShiftKeys(KeysAlt  , oldalt);
		RestoreShiftKeys(KeysCtrl , oldctrl);
		RestoreShiftKeys(KeysShift, oldshift);
	}
}

void BreakAction(EXECSTRUCT *Z)
{
	MSG msg;

	Z->result = ERROR_CANCELLED;
	// 先行入力を除去
	while ( PeekMessage(&msg, NULL, WM_KEYFIRST, 0x10f, PM_REMOVE) );
	while ( PeekMessage(&msg, NULL, WM_MOUSEFIRST, 0x20f, PM_REMOVE) );
	PPxInfoFunc(Z->Info, PPXCMDID_SETPOPLINE, (void *)MessageText(MES_BRAK));
}

#define WAIT_STEP_TIME 100
void CmdWaitExtract(EXECSTRUCT *Z, const TCHAR *param)
{
	int waittime, sleepmode;
	MSG msg;

	waittime = GetIntNumber(&param);
	if ( IsTrue(NextParameter(&param)) ){
		sleepmode = GetIntNumber(&param);
		if ( sleepmode ){
			while( waittime >= 0 ){
				Sleep( (sleepmode == 1) || (waittime <= WAIT_STEP_TIME) ?
						waittime : WAIT_STEP_TIME );
				while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ){
					if ( msg.message == WM_QUIT ) break;

					if ( (msg.message == WM_KEYDOWN) &&
						 ((int)msg.wParam == VK_PAUSE) ){
						BreakAction(Z);
						waittime = 0;
					}
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				if ( sleepmode == 1 ) return;
				waittime -= WAIT_STEP_TIME;
			}
			return;
		}
	}
	GetAsyncKeyState(VK_PAUSE); // 読み捨て(最下位bit対策)
	Sleep(waittime);
	if ( GetAsyncKeyState(VK_PAUSE) & KEYSTATE_FULLPUSH ) BreakAction(Z);
	/* PeekMessage を実行すると、SendMessage を受け付けてしまうので実装しない
	if ( PeekMessage(&msg, NULL, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE) ){
		if ( (msg.message == WM_KEYDOWN) && ((int)msg.wParam == VK_PAUSE) ){
			PauseAction(Z);
		}
	}
	*/
}

void CmdShowTip(EXECSTRUCT *Z, const TCHAR *param)
{
	TCHAR text[CMDLINESIZE];
	TOOLINFO ti;

	GetCommandParameter(&param, text, TSIZEOF(text));

	LoadCommonControls(ICC_TAB_CLASSES);
	if ( hTipWnd != NULL ){
		PostMessage(hTipWnd, WM_CLOSE, 0, 0);
		hTipWnd = NULL;
	}
	if ( text[0] == '\0' ) return;

	hTipWnd = CreateWindow(TOOLTIPS_CLASS, NULL, TTS_ALWAYSTIP | TTS_NOPREFIX,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			Z->hWnd, NULL, DLLhInst, NULL);

	ti.cbSize = sizeof(TOOLINFO);
	ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	ti.hwnd = Z->hWnd;
	ti.hinst = DLLhInst;
	ti.uId = (UINT_PTR)Z->hWnd;
	ti.lpszText = text;
	ti.rect.left = 0;
	ti.rect.top = 0;
	ti.rect.right = 10000;
	ti.rect.bottom = 10000;
	SendMessage(hTipWnd, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

void CmdDoForfile(EXECSTRUCT *Z, TCHAR *param, const TCHAR *cmdline)
{
	TCHAR mask[VFPS], cmd[CMDLINESIZE], dir[VFPS], *p;
	HANDLE hFF;
	WIN32_FIND_DATA ff;
	FN_REGEXP fn;

	p = VFSFindLastEntry(param);
	tstrcpy(mask, ((*p == '\\') || (*p == '/')) ? p + 1 : p);
	MakeFN_REGEXP(&fn, mask);

	*p = '\0';
	VFSFullPath(NULL, param, GetZCurDir(Z));
	CatPath(dir, param, WildCard_All);

	tstrcpy(cmd, cmdline);
	p = cmd + tstrlen(cmd);

	hFF = VFSFindFirst(dir, &ff);
	if ( hFF == INVALID_HANDLE_VALUE ) return;
	do{
		if ( IsRelativeDirectory(ff.cFileName) ) continue;
		if ( ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		if( FinddataRegularExpression(&ff, &fn) ){
			VFSFullPath(p, ff.cFileName, param);
			PP_ExtractMacro(Z->hWnd, Z->Info, NULL, cmd, NULL, 0);
		}

	}while( IsTrue(VFSFindNext(hFF, &ff)) );
	VFSFindClose(hFF);
	FreeFN_REGEXP(&fn);
	return;
}

BOOL CheckAndMinimize(EXECSTRUCT *Z, BOOL toggle, HWND hTargetWnd)
{
	HWND hForegroundWnd;

	if ( IsTrue(toggle) ){
		// ↓PPtray は ForegroundWnd を別途取得する
		hForegroundWnd = (HWND)PPxInfoFunc(Z->Info, PPXCMDID_GETFGWND, NULL);
		if ( hForegroundWnd == (HWND)0 ){
			hForegroundWnd = GetForegroundWindow();
		}
		if ( hForegroundWnd == hTargetWnd ){
			ShowWindow(hTargetWnd, SW_MINIMIZE);
			return TRUE;
		}
	}
	return FALSE;
}

void PPcSetForeground(HWND hParentWnd, HWND hPPcWnd, BOOL combo)
{
	if ( combo ?
			((hParentWnd != NULL) && (GetParent(hPPcWnd) != NULL)) :
			(hParentWnd != hPPcWnd) ){
		hParentWnd = GetParent(hPPcWnd);
		SendMessage(hParentWnd, WM_PPXCOMMAND, KCW_setforeground, (LPARAM)hPPcWnd);
		if ( GetForegroundWindow() != hParentWnd ){
			ForceSetForegroundWindow(hParentWnd); // ↑で失敗した時用
			WindowZPosition(hParentWnd, HWND_TOP);
		}
	}else{
		ForceSetForegroundWindow(hPPcWnd);
		SetFocus(hPPcWnd); //一体化対策
		WindowZPosition(hPPcWnd, HWND_TOP);
	}
}

void CmdFocusPPx(EXECSTRUCT *Z, const TCHAR *paramptr) // *focus
{
	TCHAR param[VFPS];
	BOOL toggle = FALSE;
	HWND hPPcWnd;

	if ( SkipSpace(&paramptr) == '!' ){
		toggle = TRUE;
		paramptr++;
	}
	GetCommandParameter(&paramptr, param, TSIZEOF(param));
	// パラメータ指定があり、"C" 以外→通常／PPxウィンドウが対象
	if ( param[0] && !((param[0] == 'C') && (param[1] == '\0')) ){
		HWND hTargetWnd;

		hTargetWnd = GetWindowHandleByText(Z->Info, param);
		if ( hTargetWnd != NULL ){
			if ( IsTrue(CheckAndMinimize(Z, toggle, hTargetWnd)) ) return;
			PPcSetForeground(Sm->ppc.hComboWnd[0], hTargetWnd, TRUE);
			return;
		}
		if ( NextParameter(&paramptr) == FALSE ) return;
		if ( ComExecEx(Z->hWnd, paramptr, GetZCurDir(Z), &Z->useppb, Z->flags, &Z->ExitCode) == FALSE){
			Z->result = ERROR_CANCELLED;
		}
		return;
	}
	// パラメータ指定無し or 「C」→アクティブ PPc が対象
	hPPcWnd = PPcGetWindow(0, CGETW_GETFOCUS);
	if ( hPPcWnd == NULL ){
		ComZExecPPx(Z, PPcExeName, NilStr); // PPc 起動
	}else{
		HWND hParentWnd;

		hParentWnd = GetParent(hPPcWnd);
		if ( hParentWnd == NULL ) hParentWnd = hPPcWnd;
		if ( IsTrue(CheckAndMinimize(Z, toggle, hParentWnd)) ) return;
		PPcSetForeground(hParentWnd, hPPcWnd, FALSE);
	}
}

void CmdSelectPPx(EXECSTRUCT *Z, const TCHAR *param)
{
	HWND hTargetWnd;
	DWORD menuid = 1;
	TCHAR buf[VFPS + 16];
									// 指定あり？
	SkipSpace(&param);
	hTargetWnd = GetPPxhWndFromID(Z->Info, &param, NULL);
	if ( hTargetWnd == BADHWND ){
		Z->result = ERROR_INVALID_PARAMETER;
		return; // 該当無し
	}
	if ( hTargetWnd == NULL ){ // 指定無し...メニュー表示
		HMENU hPopupMenu;
		MENUITEMINFO minfo;

		hPopupMenu = CreatePopupMenu();
		if ( GetPPxList(hPopupMenu, GetPPxList_hWnd, NULL, &menuid) ){
			int index;

			index = TTrackPopupMenu(Z, hPopupMenu, NULL);
			if ( index > 0 ){
				minfo.cbSize = sizeof(minfo);
				minfo.fMask = (WinType < WINTYPE_2000) ?
						(MIIM_STATE | MIIM_TYPE | MIIM_ID | MIIM_DATA) :
						(MIIM_STATE | MIIM_FTYPE | MIIM_STRING | MIIM_ID | MIIM_DATA);
				minfo.cch = VFPS + 8;
				minfo.dwTypeData = buf;
				if ( GetMenuItemInfo(hPopupMenu, index, MF_BYCOMMAND, &minfo) == FALSE ){
					Z->result = ERROR_CANCELLED;
				}else{
					hTargetWnd = (HWND)minfo.dwItemData;
					if ( hTargetWnd == NULL ){ // 一体化PPc
						TCHAR *idptr, *sep;
						idptr = tstrchr(buf, '&');
						if ( idptr != NULL ){
							*idptr = 'C';
							sep = tstrchr(idptr, ':');
							if ( sep != NULL ) *sep = '\0';
							hTargetWnd = (HWND)PPxInfoFunc(Z->Info, PPXCMDID_COMBOGETHWNDEX, idptr);
							if ( hTargetWnd == NULL ){ // 別プロセスPPc
								hTargetWnd = GetPPxhWndFromID(Z->Info, (const TCHAR **)&idptr, NULL);
							}
						}
					}
				}
			}else{
				Z->result = ERROR_CANCELLED;
			}
		}
		DestroyMenu(hPopupMenu);
	}
	if ( (hTargetWnd != NULL) && (hTargetWnd != BADHWND) ){
		PPcSetForeground(Sm->ppc.hComboWnd[0], hTargetWnd, TRUE);
	}
}

BOOL CmdPack_Edit(EXECSTRUCT *Z, struct PACKINFO *pinfo, ThSTRUCT *TH, TCHAR *packname, TCHAR *packcmd)
{
	TINPUT tinput;

	if ( packcmd[0] == '\0' ){
		tstrcpy(packname, StrPackZipFolderTitle);
		tstrcpy(packcmd, StrPackZipFolderCommand);
	}

	ThSetString(TH, T("Edit_PackName"), packname);

	if ( *pinfo->path == '!' ){
		pinfo->path++;
		return TRUE;
	}

	ThSetString(TH, T("Edit_PackCmd"), packcmd);

	tinput.title	= packname;
	tinput.buff		= pinfo->path;
	tinput.size		= VFPS;
	tinput.flag		= TIEX_USEINFO | TIEX_USEOPTBTN | TIEX_INSTRSEL | TIEX_USESELECT;
	tinput.firstC	= EC_LAST;
	tinput.lastC	= EC_LAST;
	if ( ZTinput(Z, &tinput) == FALSE ){
		Z->result = ERROR_CANCELLED;
		return FALSE;
	}

	ThGetString(TH, T("Edit_PackCmd"), packcmd, CMDLINESIZE);
	return TRUE;
}

void CmdClosePPx(const TCHAR *param)
{
	TCHAR text[CMDLINESIZE];
	int i;
	HWND hWnd;
	FN_REGEXP fn;
	const TCHAR *id;

	GetCommandParameter(&param, text, TSIZEOF(text));

	if ( text[0] == '\0' ){
		PPxSendMessage(WM_CLOSE, 0, 0);
		return;
	}
	if ( MakeFN_REGEXP(&fn, text) & REGEXPF_ERROR ) return;

	FixTask();
	for ( i = 0 ; i < X_Mtask ; i++ ){
		if ( Sm->P[i].ID[0] == '\0' ) continue;
		hWnd = Sm->P[i].hWnd;
		if ( (hWnd == NULL) ||
			 (((LONG_PTR)hWnd & (LONG_PTR)HWND_BROADCAST) == (LONG_PTR)HWND_BROADCAST) ){
			continue;
		}
		id = Sm->P[i].ID;
		if ( FilenameRegularExpression(id, &fn) ){
			ClosePPxOne(hWnd);
		}else if ( id[1] == '_' ){
			text[0] = id[0];
			text[1] = id[2];
			text[2] = '\0';
			if ( FilenameRegularExpression(text, &fn) ) ClosePPxOne(hWnd);
		}
	}
	FreeFN_REGEXP(&fn);
}

void CmdHttpGet(EXECSTRUCT *Z, const TCHAR *param)
{
	TCHAR uri[CMDLINESIZE], name[VFPS];
	ThSTRUCT th;
	const char *bottom, *body;
	DWORD size;
	HANDLE hFile;
	int inheader = 0;

	GetCommandParameter(&param, uri, TSIZEOF(uri));
	if ( (*uri == '/') && ((*(uri+1) == 'h') || (*(uri+1) == 'H')) ){
		inheader = 1;
		NextParameter(&param);
		GetCommandParameter(&param, uri, TSIZEOF(uri));
	}
	if ( NextParameter(&param) == FALSE ){
		XMessage(Z->hWnd, T("*httpget"), XM_GrERRld, MES_EPRM);
		Z->result = ERROR_INVALID_PARAMETER;
		return;
	}
	ZGetFilePathParam(Z, &param, name);
								// メモリ上に取得 --------------------
	GetImageByHttp(uri, &th);
	bottom = (char *)th.bottom;
	if ( bottom == NULL ){
		Z->result = ERROR_FILE_NOT_FOUND;
		PPErrorBox(Z->hWnd, T("*httpget"), Z->result);
		return;
	}
	size = th.top - 1;
	body = strstr(bottom, "\r\n\r\n");
	if ( !inheader && (body != NULL) && *(body + 4) ){
		size -= body - bottom + 4;
		bottom = body + 4;
	}
								// 書き込み ---------------------------
	hFile = CreateFileL(name, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hFile != INVALID_HANDLE_VALUE ){
		WriteFile(hFile, bottom, size, &size, NULL);
		CloseHandle(hFile);
	}else{
		Z->result = PPErrorBox(Z->hWnd, T("*httpget save"), PPERROR_GETLASTERROR);
	}
	ThFree(&th);
}

/* ●(UNICODE/64bit版)書庫内文字コードを指定するコマンド(*setarchivecp)
  現在、PPcによる書庫内の一覧表示にしか対応していないため、隠し機能扱い。
  例) *setarchivecp 65001 // codepage をUTF8
  他) 932 SJIS, 20932 EUC-JP
*/
#ifdef UNICODE
void CmdSetArchiveCP(EXECSTRUCT *Z, const TCHAR *param)
{
	TCHAR buf[0x40];

	UnDllCodepage = GetDigitNumber32(&param);
	if ( UnDllCodepage >= 0xffff ) UnDllCodepage = CP_ACP;
	if ( UnDllCodepage == CP_ACP ){
		tstrcpy(buf, T("Codepage:default"));
	}else{
		wsprintf(buf, T("Codepage:%d"), UnDllCodepage);
	}
	SendMessage(Z->hWnd, WM_PPXCOMMAND, TMAKEWPARAM(K_SETPOPMSG, POPMSG_MSG), (LPARAM)buf);
}
#endif

void CmdChopDirectory(EXECSTRUCT *Z, const TCHAR *param)
{
	TCHAR path[VFPS], edir[VFPS], tdir[VFPS], *path_sep;
	WIN32_FIND_DATA ff;
	HANDLE hFile;
	int entries = 0;

	if ( VFSFixPath(path, (TCHAR *)param, GetZCurDir(Z), (VFSFIX_NOBLANK | VFSFIX_SEPARATOR | VFSFIX_FULLPATH | VFSFIX_REALPATH)) == NULL ){
		return;
	}
	CatPath(NULL, path, WildCard_All);
	path_sep = path + tstrlen(path) - 2;
	hFile = FindFirstFileL(path, &ff);
	if ( hFile == INVALID_HANDLE_VALUE ) return;
	tdir[0] = '\0';
	for (;;){
		if ( !IsRelativeDirectory(ff.cFileName) ){
			if ( ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
				TCHAR *p;

				entries++;
				if ( entries >= 2 ) break;

				*path_sep = '\0';
				// 仮dir名が書庫内dir名と被ったら、名前を変えて移動可能にする
				if ( tstricmp(ff.cFileName, FindLastEntryPoint(path)) == 0 ){
					tstrcpy(tdir, path);
					*(path_sep - 1) = '~';
				}
				tstrcpy(edir, path);
				*path_sep = '\\';
				tstrcpy(path_sep + 1, ff.cFileName); // 移動元
				p = FindLastEntryPoint(edir);
				if ( *p == '\0' ){
					entries = 2;
					break;
				}
				tstrcpy(p, ff.cFileName); // 移動先
			}else{
				entries = 0; // fileだった…chop不要
				break;
			}
		}
		if ( FindNextFile(hFile, &ff) == FALSE ) break;
	}
	FindClose(hFile);
	if ( entries != 1 ) return; // dir数が1以外は処理不要
	if ( tdir[0] != '\0' ){
		*path_sep = '\0';
		MoveFileL(tdir, path);
		*path_sep = '\\';
	}
	MoveFileL(path, edir);
	*path_sep = '\0';
	if ( IsTrue(RemoveDirectoryL(path)) ){
		SHChangeNotify(SHCNE_RMDIR, SHCNF_PATH, path, NULL);
	}
}

#ifdef UNICODE
	#ifndef _M_ARM64
		#define UHNAME ValueX3264(T("ppw"), T("ppx64"))
	#else
		#define UHNAME T("ppxarm64")
	#endif
#else
	#define UHNAME T("ppx")
#endif
const TCHAR CheckUpdateURL[] = CHECKVERSIONURL T("?exe=PPx&ver=") T(FileProp_Version) T("&name=") UHNAME;

const TCHAR CheckUpdateErrorMsg[] = MES_XUUE;
const TCHAR CheckUpdateNoupdMsg[] = MES_XUUN;
const TCHAR CheckUpdateQueryMsg[] = MES_XUUQ;
const TCHAR CheckUpdateBeginMsg[] = T("Download address '%s' is trust ?");

#define FTHOUR_L 0x61c46800
#define FTHOUR_H 8

BOOL IntervalUpdateCheck(const TCHAR *ptr)
{
	int itime;
	FILETIME nowtime;
	TCHAR buf[64];
	DWORD timeL, timeH, tmpH, tmpT;

	// 確認間隔を取得
	ptr++;
	itime = GetDigitNumber32(&ptr);
	if ( itime <= 0 ) itime = 7;
	if ( *ptr != 'h' ) itime *= 24; // i123[d] ... 日数→時間

	// 基準時間
	DDmul(itime, FTHOUR_L, &timeL, &timeH);
	DDmul(itime, FTHOUR_H, &tmpH, &tmpT);
	timeH += tmpH;
	// 以前の確認時刻を取得
	buf[0] = '\0';
	GetCustTable(StrCustSetup, T("CheckFT"), buf, sizeof(buf));
	ptr = buf;
	tmpT = GetDigitNumber32(&ptr);
	if ( *ptr != '.' ) return TRUE; // データ内→初めてのチェック
	ptr++;
	tmpH = GetDigitNumber32(&ptr);
	AddDD(timeL, timeH, tmpT, tmpH);
	// 比較
	GetSystemTimeAsFileTime(&nowtime);
	// high
	if ( nowtime.dwHighDateTime > timeH ) return TRUE;
	if ( nowtime.dwHighDateTime < timeH ) return FALSE;
	// low
	return (nowtime.dwLowDateTime >= timeL ) ? TRUE : FALSE;
}

void Update_CheckUpdateInterval(void)
{
	TCHAR buf[64];
	FILETIME nowtime;

	GetSystemTimeAsFileTime(&nowtime);
	wsprintf(buf, T("%u.%u"), nowtime.dwLowDateTime, nowtime.dwHighDateTime);
	SetCustStringTable(StrCustSetup, T("CheckFT"), buf, 0);
}

void CmdCheckUpdateFile(EXECSTRUCT *Z, const char *dataA, const TCHAR *param)
{
	TCHAR ver[VFPS];
	TCHAR url[VFPS];
	TCHAR text[VFPS * 3];
#ifdef UNICODE
	WCHAR bufW[0x400];
	const WCHAR *DATA;

	AnsiToUnicode(dataA, bufW, 0x400);
	bufW[0x400 - 1] = '\0';
	DATA = bufW;
#else
	#define DATA dataA
#endif
	GetLineParam(&DATA, ver);
	GetLineParam(&DATA, url);

	if ( tstrchr(param, 'y') == NULL ){
		wsprintf(text, MessageText(CheckUpdateQueryMsg), ver, T(FileProp_Version));
		if ( PMessageBox(Z->hWnd, text, NULL, MB_YESNO) != IDYES ) return;
	}
	if ( memcmp(url, T("http://toro.d.dooo.jp/"), TSTROFF(22)) != 0 ){
		wsprintf(text, MessageText(CheckUpdateBeginMsg), url);
		if ( PMessageBox(Z->hWnd, text, NULL, MB_YESNO | MB_DEFBUTTON2) != IDYES ){
			return;
		}
	}
	Get_X_save_widthUI(ver);
	CatPath(NULL, ver, tstrrchr(url, '/') + 1);
	{ // 保存できるか確認する
		HANDLE hFile;

		hFile = CreateFileL(ver, GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if ( hFile != INVALID_HANDLE_VALUE ){ // 成功
			CloseHandle(hFile);
		}else{ // 失敗したので temp に保存
			GetTempPath(MAX_PATH, ver);
			CatPath(NULL, ver, tstrrchr(url, '/') + 1);
		}
	}
	wsprintf(text, T("\"%s\",\"%s\""), url, ver);
	CmdHttpGet(Z, text);
	if ( Z->result == NO_ERROR ){
		int VerifyResult = VerifyZipImage(ver);

		if ( VerifyResult != VERIFYZIP_NOSUPPORTVERIFY ){
			if ( VerifyResult != VERIFYZIP_SUCCEDD ){
				XMessage(NULL, NULL, XM_GrERRld, MES_EBSG, ver);
				return;
			}
		}
		wsprintf(text, T("setup /sq \"%s\""), ver);

		url[0] = '\0';
		GetCustTable(StrCustSetup, T("elevate"), url, sizeof(url));
		if ( url[0] == '1' ){ // 昇格
			text[5] = '\0';
			text[8] = ' ';
			PPxShellExecute(Z->hWnd, T("RUNAS"), text, text + 6, DLLpath, XEO_NOUSEPPB, text);
		}else{
		#ifndef _WIN64
			#define COMPATNAME T("__COMPAT_LAYER")
			TCHAR oldvaule[0x2000];

			GetEnvironmentVariable(COMPATNAME, oldvaule, TSIZEOF(oldvaule));
			SetEnvironmentVariable(COMPATNAME, T("RunAsInvoker"));
		#endif
			ComExecSelf(Z->hWnd, text, DLLpath, XEO_NOUSEPPB, NULL);
		#ifndef _WIN64
			SetEnvironmentVariable(COMPATNAME, oldvaule);
		#endif
		}
		// ●とりあえずもっと前の段階で時刻更新
		// if ( tstrchr(param, 'i') != NULL) Update_CheckUpdateInterval();
	}
}

void zipunpack(EXECSTRUCT *Z, const TCHAR *param)
{
	IMAGEGETEXINFO exinfo;
	TCHAR src[VFPS];
	BYTE *mem;
	DWORD type = VFSDT_ZIPFOLDER;
	ERRORCODE result;

	GetLineParam(&param, src);
	VFSFullPath(NULL, src, GetZCurDir(Z));
	GetLineParam(&param, exinfo.dest);

	exinfo.Progress = (LPPROGRESS_ROUTINE)NULL;
	exinfo.lpData = NULL;
	exinfo.Cancel = NULL;
	mem = (BYTE *)&exinfo;

	result = VFSGetArchivefileImage(INVALID_HANDLE_VALUE, NULL, src, NilStr, NULL, &type, NULL, &mem);
	if ( result != MAX32 ){
		Z->result = result;
		PPErrorBox(NULL, T("Unarc"), result);
	}
}

void CmdCheckUpdate(EXECSTRUCT *Z, const TCHAR *param)
{
	ThSTRUCT th;
	const char *bottom;
	const TCHAR *msg = CheckUpdateNoupdMsg, *force, *ptr;

	if ( *param == 'u' ){
		zipunpack(Z, param + 1);
		return;
	}
	ptr = tstrchr(param, 'i');
	if ( ptr != NULL ){ // チェック間隔指定
		if ( IntervalUpdateCheck(ptr) == FALSE ) return;
		Update_CheckUpdateInterval(); // ●とりあえずここに
	}
	GetImageByHttp(CheckUpdateURL, &th);
	force = tstrchr(param, 'f');
	bottom = strstr(th.bottom, "\n>");
	if ( bottom != NULL ){
		MessageBoxA(Z->hWnd, bottom + 2, NULL, MB_ICONINFORMATION | MB_OK);
	}
	bottom = strstr(th.bottom, "\r\n\r\n#");
	if ( bottom == NULL ){
		msg = CheckUpdateErrorMsg;
	}else{ // チェック可能
		const TCHAR *pluscheck;
		const char *plusbottom;

		pluscheck = tstrchr(param, 'p');
		plusbottom = strstr(bottom + 5, "\r\n#");
		if ( (*(bottom + 5) == '+') || // 正式版の更新有り
					// 強制更新有り→
					//  試験公開版指定なしか、指定あり時に試験版がない
			 ((force != NULL) && ((pluscheck == NULL) || (plusbottom == NULL)))
			 ){
			CmdCheckUpdateFile(Z, bottom + 6, param);
			msg = NULL;
		}
		#if VersionP // 試験公開版は、常に試験公開版の更新チェックをする
		else
		#else // 正式版は、'p' 指定があるときのみ更新チェック
		else if ( pluscheck != NULL )
		#endif
		{
			if ( (plusbottom != NULL) &&
				 ((*(plusbottom + 3) == '+') || (force != NULL)) ){ // 試験公開版更新有
				CmdCheckUpdateFile(Z, plusbottom + 4, param);
				msg = NULL;
			}
		}
	}
	ThFree(&th);
	if ( msg != NULL ){
		if ( FALSE == PPxInfoFunc(Z->Info, PPXCMDID_SETPOPLINE, (void *)msg) ){
			XMessage(Z->hWnd, T("PPx ") T(FileProp_Version), XM_RESULTld, msg);
		}
		Z->result = ERROR_CANCELLED;
	}
}

void CmdCheckSignature(EXECSTRUCT *Z, TCHAR *line)
{
	TCHAR param[CMDLINESIZE];
	int VerifyResult;
	BOOL showsuccessresult = TRUE;

	GetModuleFileName(DLLhInst, param, MAX_PATH);
	VerifyResult = VerifyZipImage(param);
	if ( VerifyResult != VERIFYZIP_SUCCEDD ){
		XMessage(NULL, NULL, XM_GrERRld,
			(VerifyResult != VERIFYZIP_NOSUPPORTVERIFY) ? MES_EBSG : MES_EBRV,
			param);
		Z->result = ERROR_INVALID_DATA;
		return;
	}

	if ( *line == '!' ){
		showsuccessresult = FALSE;
		line++;
	}
	ZGetFilePathParam(Z, (const TCHAR **)&line, param);
	if ( param[0] != '\0' ){
		VerifyResult = VerifyZipImage(param);
	}else{
		GetModuleFileName(DLLhInst, param, MAX_PATH);
	}

	if ( VerifyResult != VERIFYZIP_SUCCEDD ){
		XMessage(NULL, NULL, XM_GrERRld, MES_EBSG, param);
		Z->result = ERROR_INVALID_DATA;
	}else{
		if ( showsuccessresult ) XMessage(NULL, NULL, XM_RESULTld, MES_GSIG);
	}
}

/*
			*togglecustword Custname:subname, word,
	又は、  *togglecustword Custname:subname, word:subword,
	"word" が一致すれば、削除・追加、subwordがあってもwordが一致すればよい
*/
void CmdToggleCustWord(const TCHAR *param)
{
	TCHAR parambuf[CMDLINESIZE], *sub, *optname, *optptr, *optnamesub;
	TCHAR custbuf[CMDLINESIZE];

	tstrcpy(parambuf, param);
	sub = tstrchr(parambuf, ':');
	if ( sub == NULL ) return;
	*sub++ = '\0';
	optname = tstrchr(sub, ',');
	if ( optname == NULL ) return;
	*optname++ = '\0';
	optnamesub = tstrchr(optname, ':');
	if ( optnamesub != NULL ) *optnamesub = '\0';
	custbuf[0] = '\0';
	GetCustTable(parambuf, sub, custbuf, sizeof(custbuf));
	optptr = tstrstr(custbuf, optname);
	if ( optptr != NULL ){ // 存在…削除
		TCHAR *ptr = optptr + tstrlen(optname);

		if ( optnamesub != NULL ){ // 別名有り…入れ替え
			int add;

			*optnamesub = ':';
			for (;;){
				if ( *ptr == '\0' ) break;
				if ( *ptr++ == ',' ) break;
			}
			add = (tstrstr(optptr, optname) != optptr);
			memmove(optptr, ptr, TSTRSIZE(ptr));
			if ( add ) tstrcat(custbuf, optname);
		}else{
			memmove(optptr, ptr, TSTRSIZE(ptr));
		}
	}else{ // 追加
		if ( optnamesub != NULL ) *optnamesub = ':';
		tstrcat(custbuf, optname);
	}
	SetCustStringTable(parambuf, sub, custbuf, 0);
}

void CmdMakeDirectory(EXECSTRUCT *Z, const TCHAR *param)
{
	TCHAR path[CMDLINESIZE];
	ERRORCODE result;

	if ( VFSFixPath(path, (TCHAR *)param, GetZCurDir(Z), (VFSFIX_NOBLANK | VFSFIX_SEPARATOR | VFSFIX_FULLPATH | VFSFIX_REALPATH)) == NULL ){
		return;
	}
	result = MakeDirectories(path, NULL);
	if ( (result != NO_ERROR) && (result != ERROR_ALREADY_EXISTS) ){
		Z->result = PPErrorBox(Z->hWnd, T("*makedir"), result);
	}
}

void CmdAddHistory(const TCHAR *param)
{
	TCHAR *ptr, buf[CMDLINESIZE];

	GetCommandParameter(&param, buf, TSIZEOF(buf));
	ptr = tstrchr(HistType, buf[0]);
	if ( (ptr == NULL) || (*ptr == '\0') || (tstrchr(T("pv"), buf[0]) != NULL) ){
		XMessage(NULL, NULL, XM_GrERRld, T("option error"));
		return;
	}

	NextParameter(&param);
	GetCommandParameter(&param, buf, TSIZEOF(buf));

	if ( buf[0] != '\0' ){
		WriteHistory(HistWriteTypeflag[ptr - HistType], buf, 0, NULL);
	}
}

void CmdGoto(EXECSTRUCT *Z, const TCHAR *param)
{
	TCHAR *ptr, label[128];

	GetCommandParameter(&param, label + 2, TSIZEOF(label) - 2);
	if ( label[2] == '\0' ){
		label[0] = '\0';
	}else{
		label[0] = '%';
		label[1] = 'm';
	}
	ptr = tstrstr(Z->src, label);
	if ( ptr != NULL ){
		Z->src = ptr;
	}else{
		XMessage(NULL, NULL, XM_GrERRld, T("*Goto label %s not found."), label);
		Z->result = ERROR_INVALID_PARAMETER;
	}
}

void CmdDeleteHistory(const TCHAR *param)
{
	TCHAR *ptr, buf[CMDLINESIZE];
	WORD htype;

	GetCommandParameter(&param, buf, TSIZEOF(buf));
	ptr = tstrchr(HistType, buf[0]);
	if ( (ptr == NULL) || (*ptr == '\0') || (tstrchr(T("pv"), buf[0]) != NULL) ){
		XMessage(NULL, NULL, XM_GrERRld, T("option error"));
		return;
	}
	htype = HistWriteTypeflag[ptr - HistType];

	NextParameter(&param);
	if ( Isdigit(*param) ){
		int index = GetDigitNumber32(&param);
		const TCHAR *hptr;

		UsePPx();
		hptr = EnumHistory(htype, index++);
		if ( hptr == NULL ) hptr = NilStr;
		tstrcpy(buf, hptr);
		FreePPx();
	}else{
		GetCommandParameter(&param, buf, TSIZEOF(buf));
	}
	if ( buf[0] != '\0' ) DeleteHistory(htype, buf);
}

void CmdMakeFile(EXECSTRUCT *Z, const TCHAR *param)
{
	TCHAR path[CMDLINESIZE];
	HANDLE hFile;

	if ( VFSFixPath(path, (TCHAR *)param, GetZCurDir(Z), (VFSFIX_NOBLANK | VFSFIX_SEPARATOR | VFSFIX_FULLPATH | VFSFIX_REALPATH)) == NULL ){
		return;
	}
	hFile = CreateFileL(path, GENERIC_WRITE, 0, NULL,
				CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ){
		Z->result = PPErrorBox(Z->hWnd, T("*makefile"), PPERROR_GETLASTERROR);
		return;
	}
	CloseHandle(hFile);
}

void CmdDeleteEntry(EXECSTRUCT *Z, const TCHAR *param)
{
	TCHAR path[CMDLINESIZE];
	DWORD attr;
	//BOOL result = FALSE;

	if ( VFSFixPath(path, (TCHAR *)param, GetZCurDir(Z), (VFSFIX_NOBLANK | VFSFIX_SEPARATOR | VFSFIX_FULLPATH | VFSFIX_REALPATH)) == NULL ){
		return;
	}
	attr = GetFileAttributes(path);
	if ( attr != BADATTR ){
		if ( attr & FILE_ATTRIBUTE_DIRECTORY ){
			/*result = */DeleteDirectories(path, TRUE);
		}else{
			/*result = */DeleteFileL(path);
		}
	}
	/*
	if ( result == FALSE ){
		Z->result = PPErrorBox(Z->hWnd, T("*delete"), PPERROR_GETLASTERROR);
	}
	*/
}

void CmdRenameEntry(EXECSTRUCT *Z, const TCHAR *param)
{
	TCHAR path1[CMDLINESIZE], path2[CMDLINESIZE], *curdir;

	GetCommandParameter(&param, path1, TSIZEOF(path1));
	if ( NextParameter(&param) == FALSE ){
		Z->result = PPErrorBox(Z->hWnd, T("*rename"), ERROR_INVALID_PARAMETER);
		return;
	}
	curdir = GetZCurDir(Z);
	if ( VFSFixPath(NULL, path1, curdir, (VFSFIX_SEPARATOR | VFSFIX_FULLPATH | VFSFIX_REALPATH | VFSFIX_NOFIXEDGE)) == NULL ){
		return;
	}
	if ( VFSFixPath(path2, (TCHAR *)param, curdir, (VFSFIX_SEPARATOR | VFSFIX_FULLPATH | VFSFIX_REALPATH)) == NULL ){
		return;
	}
	if ( MoveFileL(path1, path2) == FALSE ){
		Z->result = PPErrorBox(Z->hWnd, T("*rename"), PPERROR_GETLASTERROR);
	}
}


void CmdLaunch(EXECSTRUCT *Z, const TCHAR *param)
{
	TCHAR exepath[VFPS], *curdir = GetZCurDir(Z);
	const TCHAR *pp;

	pp = param;
	if ( GTYPE_ERROR != GetExecType(&pp, exepath, curdir) ){
		*VFSFindLastEntry(exepath) = '\0';
		curdir = exepath;
		if ( *curdir == '\"' ) curdir++; // 末尾の \" は２行前で削除済み
	}
	if ( Z->flags & XEO_CONSOLE ){
		PPXCMD_PPBEXEC ppbe;

		ppbe.name = param;
		ppbe.flag = Z->flags;
		ppbe.path = curdir;
		Z->ExitCode = PPxInfoFunc32u(Z->Info, PPXCMDID_PPBEXEC, &ppbe);
		return;
	}
	if ( ComExecEx(Z->hWnd, param, curdir, &Z->useppb, Z->flags, &Z->ExitCode) == FALSE ){
		Z->result = ERROR_CANCELLED;
	}
}

void CmdStart(EXECSTRUCT *Z, const TCHAR *param)
{
	const TCHAR *ptr = param;
	BOOL launch = FALSE;
	TCHAR cmdbuf[VFPS], exepath[VFPS], pathbuf[VFPS];
	TCHAR *command = NULL, *path;

	while ( GetOption(&ptr, exepath) == '-' ){
		if ( tstrcmp(exepath + 1, T("LAUNCH")) == 0 ){
			launch = TRUE;
		}else{
			tstrcpy(cmdbuf, exepath + 1);
			command = cmdbuf;
		}
		param = ptr;
	}
	path = GetZCurDir(Z);
	GetExecType(&param, exepath, path);

	if ( launch ){
		tstrcpy(pathbuf, exepath);
		*VFSFindLastEntry(pathbuf) = '\0';
		path = pathbuf;
		if ( *path == '\"' ) path++; // 末尾の \" は２行前で削除済み
	}
	ZapMain(Z, command, exepath, param, path);
}

DWORD_PTR USECDECL CmdPackInfo(struct PACKINFO *info, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case '2': // %2 展開先
			tstrcpy(uptr->enums.buffer, info->path);
			break;

		default:
			return info->parent->Function(info->parent, cmdID, uptr);
	}
	return 1;
}

DWORD_PTR USECDECL CmdPackInfoIndiv(struct PACKINFO *info, DWORD cmdID, PPXAPPINFOUNION *uptr)
{
	switch(cmdID){
		case PPXCMDID_STARTENUM:	// 検索開始(マーク無しもあり)
		case PPXCMDID_STARTNOENUM:	// 検索開始(マーク無しはなし)
			uptr->enums.enumID = 1;
			break;

		case PPXCMDID_NEXTENUM:		// 次へ
		case PPXCMDID_ENDENUM:		//列挙終了
			uptr->enums.enumID = -1;
			return 0;

		case '2': // %2 展開先
			tstrcpy(uptr->enums.buffer, info->path);
			break;

		case '3': // %3 書庫対象ファイル(フルパス、zipfldr.dl)
			info->parent->Function(info->parent, '1', uptr);
			VFSFullPath(uptr->enums.buffer, info->files, uptr->enums.buffer);
			break;

		case '4': // %4 書庫対象ファイル
			tstrcpy(uptr->enums.buffer, info->files);
			if ( info->attr & FILE_ATTRIBUTE_DIRECTORY ){
				tstrcat(uptr->enums.buffer, T("\\*"));
			}
			break;

		case PPXCMDID_ENUMATTR:
			*(DWORD *)uptr->enums.buffer = info->attr;
			break;

		default:
			return info->parent->Function(info->parent, cmdID, uptr);
	}
	return 1;
}

void CmdPack(EXECSTRUCT *Z, const TCHAR *param)
{
	struct PACKINFO pinfo;
	TCHAR pathbuf[VFPS], packname[VFPS], packcmd[CMDLINESIZE], packtype[MAX_PATH];
	TCHAR buf[VFPS];
	ThSTRUCT *VarTH;
	BOOL indivmode = FALSE;

	VarTH = (ThSTRUCT *)PPxInfoFunc(Z->Info, PPXCMDID_GETWNDVARIABLESTRUCT, NULL);
	if ( VarTH == NULL ) VarTH = &ProcessStringValue;

	// 1st param : 編集無し"!" + ファイル名
	GetCommandParameter(&param, pathbuf, TSIZEOF(pathbuf));
	pinfo.path = pathbuf;

	// 2nd param : dllname
	if ( NextParameter(&param) == FALSE ){
		packtype[0] = '\0';
	}else{
		GetCommandParameter(&param, packtype, TSIZEOF(packtype));
	}

	// another 2nd param : indiv ?
	if ( tstrcmp(packtype, T("indiv")) == 0 ){
		indivmode = TRUE;
		if ( NextParameter(&param) == FALSE ){
			packtype[0] = '\0';
		}else{
			GetCommandParameter(&param, packtype, TSIZEOF(packtype));
		}
	}

	SetCustTable(StrCustOthers, T("PackIndiv"), indivmode ? Str1 : NilStr , sizeof(Str1));

	// 3rd param : type
	NextParameter(&param);
	GetCommandParameter(&param, packname + 2, TSIZEOF(packname) - 2);

	if ( packtype[0] != '\0' ){ // 圧縮種類を指定
		const TCHAR *tpac;
		TCHAR *pp;

		packname[0] = 'p';
		packname[1] = ':';
		if ( FindPackType(packtype, packname, packcmd) == FALSE ){
			XMessage(Z->hWnd, NULL, XM_GrERRld, T("unmatch name %s - %s"), packtype, packname + 2);
			return;
		}
		ThSetString(&ProcessStringValue, T("Edit_PackMode"), T("t"));
		tpac = MessageText(MES_TPAC);
		tstrcpy(buf, packtype);
		pp = tstrrchr(buf, '.');
		if ( pp != NULL ) *pp = '\0';
		wsprintf(packtype, T("%s %s - %s"), tpac, packname + 2, buf);
		wsprintf(packname, T("*string ,Edit_PackCmd=%%M&M:?packlist %%: *string i,Edit_PackName=%%s\"Menu_Index\" %%: *string i,Edit_PackCmd=%%s\"Edit_PackCmd\" %: *setcaption %s %%s\"Menu_Index\""), tpac);
		ThSetString(VarTH, T("Edit_OptionCmd"), packname);

		if ( FALSE == CmdPack_Edit(Z, &pinfo, VarTH, packtype, packcmd) ) return;

		ThGetString(VarTH, T("Edit_PackName"), packtype, TSIZEOF(packtype));
	}else{ // 圧縮種類の指定なし
		const TCHAR *tpac;

		packname[0] = '\0';
		packcmd[0] = '\0';
		GetCustTable(StrCustOthers, T("PackName"), packname, sizeof(packname));
		GetCustTable(StrCustOthers, T("PackCmd"), packcmd, sizeof(packcmd));

		ThSetString(&ProcessStringValue, T("Edit_PackMode"), T("g"));
		tpac = MessageText(MES_TPAC);
		wsprintf(packtype, T("%s %s"), tpac, packname);
		wsprintf(packname, T("*string ,Edit_PackCmd=%%M&M:?packlist %%: *setcust _others:PackName=%%s\"Menu_Index\" %%: *setcust _others:PackCmd=%%s\"Edit_PackCmd\" %%: *setcaption %s %%s\"Menu_Index\""), tpac);
		ThSetString(VarTH, T("Edit_OptionCmd"), packname);

		if ( FALSE == CmdPack_Edit(Z, &pinfo, VarTH, packtype, packcmd) ) return;

		GetCustTable(StrCustOthers, T("PackName"), packtype, sizeof(packtype));
	}
	packname[0] = '0';
	GetCustTable(StrCustOthers, T("PackIndiv"), packname, sizeof(packname));
	indivmode = packname[0] == '1';

	// オプションがあれば、 <option1, option2...< で展開する
	packname[0] = '<';
	GetCustTable(StrCustOthers, T("PackOption"), packname + 1, sizeof(packname) - sizeof(TCHAR));

	while ( NextParameter(&param) != FALSE ){
		GetCommandParameter(&param, buf, TSIZEOF(buf));
		if ( buf[0] != '\0' ){
			if ( buf[0] == '-' ){ // option 削除
				tstrreplace(packname, buf + 1, NilStr);
			}else{
				TCHAR *ptr = tstrchr(buf, ':');

				if ( ptr != NULL ){ // 種別指定有り
					TCHAR back = *(ptr + 1), *oldptr;

					*(ptr + 1) = '\0';
					oldptr = tstrstr(packname, buf);
					*(ptr + 1) = back;
					if ( oldptr != NULL ){ // 同じ種別があったら除去する
						TCHAR *oldptrlast = oldptr;

						for (;;){
							if ( *oldptrlast == '\0' ) break;
							if ( *oldptrlast++ == ',' ) break;
						}
						memmove(oldptr, oldptrlast, TSTRSIZE(oldptrlast));
					}
				}

				tstrcat(packname, buf);
				tstrcat(packname, T(","));
			}
		}
	}

	if ( packname[1] != '\0' ){
		tstrcat(packname, T("<"));
	}else{
		packname[0] = '\0';
	}

	tstrreplace(packcmd, T(">"), packname);

	tstrreplace(packcmd, T("%2%\\"), T("%2")); // %2にファイル名が入っているから

	pinfo.info = *Z->Info;
	pinfo.parent = Z->Info;
	if ( indivmode == FALSE ){
		pinfo.info.Function = (PPXAPPINFOFUNCTION)CmdPackInfo;
		PP_ExtractMacro(Z->hWnd, &pinfo.info, NULL, packcmd, NULL, XEO_NOEDIT);
	}else{
		TCHAR *pathLast, *lp;
		ThSTRUCT th;

		{ // packtype を作成書庫の拡張子名に加工する
			TCHAR *p;

			p = tstrchr(packtype, ':');
			if ( p != NULL ){
				p++;
				if ( *p == ' ' ) p++;
				memmove(packtype, p, TSTRSIZE(p));
			}
			p = tstrstr(packtype, T(" - "));
			if ( p != NULL ) *p = '\0';
		}

		pinfo.info.Function = (PPXAPPINFOFUNCTION)CmdPackInfoIndiv;
		pinfo.files = buf;
		if ( tstrstr(packcmd, T("%uzipfldr.dll")) != NULL ){
			tstrcpy(packcmd, T("%uzipfldr.dll,A \"%2\" /src:\"%3\""));
		}else{
			tstrreplace(packcmd, T("%@"), T("\"%4\""));
		}
		CatPath(NULL, pinfo.path, NilStr);
		pathLast = pinfo.path + tstrlen(pinfo.path);
		ThInit(&th);
		for ( ; ; ){
			GetValue(Z, 'C', buf);
			if ( buf[0] == '\0' ) break;
			PPxEnumInfoFunc(Z->Info, PPXCMDID_ENUMATTR, (TCHAR *)&pinfo.attr, &Z->EnumInfo);
			tstrcpy(pathLast, buf);
			if ( pinfo.attr & FILE_ATTRIBUTE_DIRECTORY ){
				lp = pathLast + tstrlen(pathLast);
			}else{
				lp = pathLast + FindExtSeparator(pathLast);
			}
			*lp++ = '.';
			tstrcpy(lp, packtype);
			ThSize(&th, CMDLINESIZE);
			ThCatString(&th, T("%u/"));
			PP_ExtractMacro(Z->hWnd, &pinfo.info, NULL, packcmd, (TCHAR *)ThLast(&th), XEO_NOEDIT);
			if ( PPxEnumInfoFunc(Z->Info, PPXCMDID_NEXTENUM, buf, &Z->EnumInfo) == 0 ){
				break;
			}
			th.top += TSTRLENGTH32((TCHAR *)ThLast(&th));
			ThCatString(&th, T("\r\n"));
		}
		if ( th.top > 0 ){
			PP_ExtractMacro(Z->hWnd, Z->Info, NULL, (TCHAR *)th.bottom, NULL, XEO_NOEDIT);
		}
		ThFree(&th);
	}
}

void CmdNextItem(EXECSTRUCT *Z, const TCHAR *param) // *nextitem
{
	int skipcount = 1;
	TCHAR buf[64];

	if ( *param != '\0' ) CalcString(&param, &skipcount);
	while ( skipcount > 0 ){
		if ( PPxEnumInfoFunc(Z->Info, PPXCMDID_NEXTENUM, buf, &Z->EnumInfo) == 0 ){
			XMessage(Z->hWnd, NULL, XM_GrERRld, T("empty next item"));
			Z->result = ERROR_INVALID_PARAMETER;
			break;
		}
		skipcount--;
	}
}

void CmdStop(EXECSTRUCT *Z, const TCHAR *param)
{
	int result = 1;
	BOOL nextitem = FALSE;

	if ( ((param[0] == '-') || (param[0] == '/')) && // nextitem option
		 (param[1] == 'n') &&
		 ((UTCHAR)param[2] <= ' ') ){
		param += 2;
		nextitem = TRUE;
	}

	if ( (*param == '\0') || (CalcString(&param, &result) == CALC_NOERROR) ){
		if ( result ){
			if ( nextitem == FALSE ){ // 実行中止
				Z->result = ERROR_CANCELLED;
			}else{ // 次へ
				Z->src += tstrlen(Z->src);
			}
		}
	}else{
		XMessage(Z->hWnd, NULL, XM_GrERRld, T("stop error"));
		Z->result = ERROR_INVALID_PARAMETER;
	}
}

void CmdIf(EXECSTRUCT *Z, const TCHAR *param)
{
	const TCHAR *paramback;
	int result;

	paramback = param;
	if ( CalcString(&param, &result) != CALC_NOERROR ){
		XMessage(NULL, NULL, XM_GrERRld, T("*if error"), paramback);
		Z->result = ERROR_INVALID_PARAMETER;
		return;
	}

	if ( result ) return;

	for (;;){ // 行末までスキップ
		TCHAR c;

		c = *Z->src;
		if ( (c == '\0') || (c == '\r') || (c == '\n') ) return;
		Z->src++;
	}
}

void CmdIfMatch(EXECSTRUCT *Z, const TCHAR *param)
{
	TCHAR wildcard[CMDLINESIZE], defparam[VFPS];
	DWORD MakeResult;
	int fnresult;
	FN_REGEXP fn;

	GetCommandParameter(&param, wildcard, TSIZEOF(wildcard));
	if ( NextParameter(&param) == FALSE ){
		GetValue(Z, 'C', defparam);
		param = defparam;
	}
	MakeResult = MakeFN_REGEXP(&fn, wildcard);
	if ( MakeResult & REGEXPF_ERROR ){
		XMessage(Z->hWnd, T("*ifmatch"), XM_GrERRld, T("wildcard error"));
		Z->result = ERROR_CANCELLED;
		return;
	}

	if ( MakeResult & (REGEXPF_REQ_ATTR | REGEXPF_REQ_SIZE | REGEXPF_REQ_TIME) ){
		WIN32_FIND_DATA ff;

		if ( (param != defparam) ||
			 (PPxEnumInfoFunc(Z->Info, PPXCMDID_ENUMFINDDATA, (TCHAR *)&ff, &Z->EnumInfo) == 0) ){
			HANDLE hFF;

			VFSFixPath(wildcard, (TCHAR *)param, GetZCurDir(Z), VFSFIX_FULLPATH);
			hFF = FindFirstFileL(wildcard, &ff);
			if ( hFF == INVALID_HANDLE_VALUE ){
				Z->result = GetLastError();
				FreeFN_REGEXP(&fn);
				if ( MakeResult & REGEXPF_SILENTERROR ){
					Z->result = NO_ERROR;
					goto fin;
				}else{
					PPErrorBox(Z->hWnd, T("*ifmatch"), Z->result);
				}
				return;
			}
			FindClose(hFF);
		}
		fnresult = FinddataRegularExpression(&ff, &fn);
	}else{
		if ( param != defparam ){
			if ( SkipSpace(&param) == '\"' ){
				GetQuotedParameter(&param, defparam, defparam + TSIZEOF(defparam));
			}else{
				TCHAR *dst, *dstmax, *dstlast;

				dst = defparam;
				dstmax = defparam + TSIZEOF(defparam) - 1;
				dstlast = dst;
				for (;;){
					TCHAR code;

					code = *param++;
					*dst++ = code;
					if ( (UTCHAR)code > ' ' ) dstlast = dst;
					if ( (code == '\0') || (code == '\r') || (code == '\n') ){
						break;
					}
					if ( dst < dstmax ) continue;
					break;
				}
				*dstlast = '\0';
			}
			param = defparam;
		}
		fnresult = FilenameRegularExpression(param, &fn);
	}
	FreeFN_REGEXP(&fn);
	if ( fnresult ) return; // 一致したのでそのまま実行
fin:
	for (;;){ // 行末までスキップ
		TCHAR c;

		c = *Z->src;
		if ( (c == '\0') || (c == '\r') || (c == '\n') ) return;
		Z->src++;
	}
}

void CmdCust(EXECSTRUCT *Z, TCHAR *line, BOOL reload)
{
	TCHAR param[CMDLINESIZE];

	if ( SkipSpace((const TCHAR **)&line) == '@' ){ // filename mode
		line++;
		ZGetFilePathParam(Z, (const TCHAR **)&line, param);
		CustFile(Z, param, reload);
	}else{
		CustLine(Z, line, reload);
	}
}

void CmdLineCust(EXECSTRUCT *Z, const TCHAR *line)
{
	TCHAR id[MAX_PATH], *idsep;
	TCHAR parambuf[CMDLINESIZE], *makedata, *param, *paramorg;
	TCHAR *custtext, separator, *sub, orgseparator;
	DWORD idsize;
	BOOL toggle = FALSE;
	TCHAR *next, *last = NULL;

	if ( SkipSpace(&line) == '^' ){
		toggle = TRUE;
		line++;
	}
	id[0] = '%';
	id[1] = 'm';
	GetCommandParameter(&line, id + 2, TSIZEOF(id) - 2);
	idsize = TSTRLENGTH32(id);
	NextParameter(&line);

	custtext = tstrchr(line, '=');
	idsep = tstrchr(line, ',');
	if ( custtext == NULL ){
		if ( idsep == NULL ) return;
		custtext = idsep;
	}else{
		if ( (idsep != NULL) && (custtext > idsep) ) custtext = idsep;
	}
	separator = *custtext;
	*custtext++ = '\0';

	sub = tstrchr(line, ':');
	if ( sub != NULL ) *sub++ = '\0';
	param = parambuf;
	PPcustCDumpText(line, sub, &param);
	paramorg = param;

	orgseparator = '\0';
	if ( *param == '\t' ) param++;
	if ( (*param == '=') || (*param == ',') ) orgseparator = *param++;
	if ( *param == ' ' ) param++;
	next = param;
	for (;;){
		TCHAR c, *p1, *p;

		if ( *next == '\0' ) break;
		p = next;
		for(;;){
			c = *next;
			if ( c == '\0' ) break;
			if ( c == '\n' ){
				next++;
				while ( *next == '\n' ) next++;
				break;
			}
			next++;
		}
		p1 = p;
		if ( *p1 == '\t' ) p1++;
		if ( memcmp(p1, id, idsize) == 0 ){
			if ( p > param ) --p;
			*p = '\0';
			last = next;
			if ( toggle ) custtext = NilStrNC; // 既にあるので挿入しない
			break;
		}
	}
	makedata = HeapAlloc(DLLheap, 0, TSTROFF(tstrlen(paramorg) + tstrlen(custtext) + 256));
	if ( makedata == NULL ){
		PPErrorBox(Z->hWnd, T("*linecust"), PPERROR_GETLASTERROR);
	}else{
		TCHAR *destp, *data1st;

		if ( sub != NULL ){
			destp = makedata + wsprintf(makedata, T("%s = {\n%s %c"), line, sub, separator);
		}else{
			destp = makedata + wsprintf(makedata, T("%s %c"), line, separator);
		}
		data1st = destp;
		if ( *param != '\0' ){
			if ( ((*line == 'K') || (*line == 'E')) && (orgseparator == '=') ){
				destp += wsprintf(destp, T("%%K\""));
			}
			destp = tstpcpy(destp, param);
		}
		if ( *custtext != '\0' ){
			if ( *param != '\0' ){
				*destp++ = '\n';
				*destp++ = '\t';
			}
			destp += wsprintf(destp, T("%s "), id);
			destp = tstpcpy(destp, custtext);
		}
		if ( last != NULL ){
			if ( destp != data1st ) *destp++ = '\n';
			destp = tstpcpy(destp, last);
		}
		CustCmdSub(Z, makedata, destp, FALSE);
		HeapFree(DLLheap, 0, makedata);
	}
	if ( paramorg != parambuf ) HeapFree(ProcHeap, 0, paramorg);
}

void CmdDeleteCust(const TCHAR *param)
{
	TCHAR *ptr, key[CMDLINESIZE], name[CMDLINESIZE], first;
	int index = -1;

	first = SkipSpace(&param);
	GetCommandParameter(&param, key, TSIZEOF(key));
	if ( key[0] == '\0' ) return;

	ptr = tstrchr(key, ':');
	if ( ptr != NULL ){ // key:name 形式
		*ptr = '\0';
		param = ptr + 1;
		if ( *param == '\0' ) return;
		tstrcpy(name, param);
	}else if ( NextParameter(&param) == FALSE ){ // "key" のみ
		if ( first != '\"' ) return;
		name[0] = '\0';
	}else if ( Isdigit(*param) ){ // key,index
		index = GetDigitNumber32(&param);
		if ( index < 0 ) return;
	}else if( *param == '\"' ){ // key,"name"
		GetCommandParameter(&param, name, TSIZEOF(name));
		if ( name[0] == '\0' ) return;
	}else{
		return;
	}
	if ( index >= 0 ){
		DeleteCustTable(key, NULL, index);
	}else if ( name[0] != '\0' ){
		DeleteCustTable(key, name, 0);
	}else{
		DeleteCustData(key);
	}
}

void CmdAlias(EXECSTRUCT *Z, TCHAR *param)
{
	TCHAR *data;

	if ( GetSetParams(&param, &data) == FALSE ){
		HMENU hMenu;
		DWORD id = 1;
		int menupos;
		PPXMENUINFO xminfo;

		ThInit(&xminfo.th);
		xminfo.info = Z->Info;
		xminfo.commandID = 0;
		hMenu = PP_AddMenu(Z->Info, Z->hWnd, NULL, &id, T("A_exec"), &xminfo.th);
		PPxSetMenuInfo(hMenu, &xminfo);

		if ( hMenu != NULL ){
			menupos = TTrackPopupMenu(Z, hMenu, &xminfo);
			DestroyMenu(hMenu);
			if ( menupos ){
				const TCHAR *newsrc;

				newsrc = GetMenuDataString(&xminfo.th, menupos - 1);
				PP_ExtractMacro(Z->hWnd, Z->Info, Z->posptr, newsrc, NULL, Z->flags);
			}else{
				Z->result = ERROR_CANCELLED;
			}
		}
		ThFree(&xminfo.th);
	}else{
		if ( data != NULL ){
			SetCustStringTable(T("A_exec"), param, data, 0);
		}else{
			DeleteCustTable(T("A_exec"), param, 0);
		}
	}
}

void CmdPPbSet(EXECSTRUCT *Z, const TCHAR *param, const TCHAR *data)
{
	TCHAR fixbuf[CMDLINESIZE * 2], *butptr = fixbuf, *allocbuf;
	size_t len;

	len = tstrlen(param) + 16;
	if ( data != NULL ) len += tstrlen(data);
	if ( len >= TSIZEOF(fixbuf) ){
		allocbuf = HeapAlloc(DLLheap, 0, TSTROFF(len));
		if ( allocbuf == NULL ) return;
		butptr = allocbuf;
	}

	tstrcpy(butptr, T(">*set "));
	tstrcpy(butptr + 6, param);
	tstrcat(butptr + 6, T("="));
	if ( data != NULL ) tstrcat(butptr + 6, data);
	ComExecEx(Z->hWnd, butptr, GetZCurDir(Z), &Z->useppb, Z->flags, &Z->ExitCode);
#pragma warning(suppress:6001) // if 内で変更
	if ( butptr != fixbuf ) HeapFree(DLLheap, 0, allocbuf);
}

// *set
void CmdSet(EXECSTRUCT *Z, TCHAR *param)
{
	TCHAR *data;

	if ( IsTrue(GetSetParams(&param, &data)) ){
		TCHAR *paramlp, *allocbuf = NULL;

		paramlp = param + tstrlen(param) - 1;
		if ( *paramlp == '+' ){
			TCHAR fixbuf[CMDLINESIZE], *bufptr;
			DWORD esize, buflen, len;

			*paramlp = '\0';
			bufptr = fixbuf;
			esize = GetEnvironmentVariable(param, bufptr, TSIZEOF(fixbuf));
			buflen = esize + ((data != NULL) ? (tstrlen32(data) + 32) : 32);
			if ( buflen >= TSIZEOF(fixbuf) ){
				allocbuf = HeapAlloc(DLLheap, 0, TSTROFF(buflen));
				if ( allocbuf == NULL ){
					PPErrorBox(Z->hWnd, NULL, PPERROR_GETLASTERROR);
					return;
				}
				bufptr = allocbuf;
				esize = GetEnvironmentVariable(param, bufptr, buflen);
			}
			if ( esize && (data != NULL) ){ // 既存有り→追加するか判断
				TCHAR *ptr, *lp;

				len = tstrlen32(data);
				ptr = tstrstr(bufptr, data);
				if ( ptr != NULL ){
					lp = ptr + len;
					if ( !( ((ptr == bufptr) || (*(ptr - 1) == ';') ) &&
							((*lp == '\0') || (*lp == ';')) ) ){ // 未記載？
						ptr = NULL;
					}
				}
				if ( ptr != NULL ){
					if ( allocbuf != NULL ) HeapFree(DLLheap, 0, allocbuf);
					return; // 記載済みのため、処理せず
				}
				 // 未記載なら、先頭に追加
				memmove(bufptr + len + 1, bufptr, TSTROFF(esize + 1));
				memcpy(bufptr, data, TSTROFF(len));
				*(bufptr + len) = ';';
				#ifndef _MSC_VER
					data = bufptr;
				#else // VS2008 の場合、data = pathbuf で、上の memcpy が省略される(data==pathbuf前提に処理する)最適化バグがあるので、回避する。
					if ( SetEnvironmentVariable(param, bufptr) == FALSE ){
						PPErrorBox(Z->hWnd, NULL, PPERROR_GETLASTERROR);
					}
					if ( ((Z->flags & XEO_USEPPB) || (Z->useppb != -1)) &&
						 !(Z->flags & (XEO_NOUSEPPB | XEO_CONSOLE | XEO_USECMD)) ){
						CmdPPbSet(Z, param, bufptr);
					}
					if ( allocbuf != NULL ) HeapFree(DLLheap, 0, allocbuf);
					return;
				#endif
			}
		}
		if ( SetEnvironmentVariable(param, data) == FALSE ){
			PPErrorBox(Z->hWnd, NULL, PPERROR_GETLASTERROR);
		}
		if ( ((Z->flags & XEO_USEPPB) || (Z->useppb != -1)) &&
			 !(Z->flags & (XEO_NOUSEPPB | XEO_CONSOLE | XEO_USECMD)) ){
			CmdPPbSet(Z, param, data);
		}
		if ( allocbuf != NULL ) HeapFree(DLLheap, 0, allocbuf);
	}else{ // 一覧表示
		HMENU hMenu;
		TCHAR *envptr;
		const TCHAR *p;

		hMenu = CreatePopupMenu();
		p = envptr = GetEnvironmentStrings();
		while ( *p != '\0' ){
			AppendMenuString(hMenu, 0, p);
			p += tstrlen(p) + 1;
		}
		FreeEnvironmentStrings(envptr);
		TTrackPopupMenu(Z, hMenu, NULL);
		DestroyMenu(hMenu);
	}
}

void USEFASTCALL CmdCursor(EXECSTRUCT *Z, const TCHAR *param) // *cursor
{
	TCHAR numbuf[100];
	int ibuf[10], *ip;

	ip = ibuf;
	memset(ibuf, 0, sizeof(ibuf));
	for (;;){
		const TCHAR *ww;

		numbuf[0] = '\0';
		GetCommandParameter(&param, numbuf, TSIZEOF(numbuf));
		if ( numbuf[0] == '\0' ) break;

		ww = numbuf;
		*ip++ = GetIntNumber(&ww);
		if ( *param != ',' ) break;
		param++;
	}
	PPxInfoFunc(Z->Info, PPXCMDID_MOVECSR, &ibuf);
}

void USEFASTCALL CmdPPeEdit(EXECSTRUCT *Z, const TCHAR *param)
{
	HWND hEwnd;
	MSG msg;
	PPE_CMDMODEDATA pc;

	pc.dummy = NULL;
	pc.param = param;
	pc.curdir = GetZCurDir(Z);

	hEwnd = PPEui(Z->hWnd, (const TCHAR *)&pc, PPE_TEXT_CMDMODE);

	if ( Z->command == CID_EDIT ){ // *edit は PPe を閉じるまで待つ
		while ( IsWindow(hEwnd) ){
			if( (int)GetMessage(&msg, NULL, 0, 0) <= 0 ) break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

// %I / %Q
void USEFASTCALL CmdMessageBox(EXECSTRUCT *Z, TCHAR *param)
{
	DWORD style = MB_ICONINFORMATION;
	TCHAR *src;

	if ( Z->command == 'Q' ){
		// 繰り返し実行中なら、前回に既に ok を押しているので確認しない
	 	if ( Z->status & ST_EXECLOOP ) return;
		if ( (*param == 'N') || (*param == 'C') ){
			param++;
			style = MB_ICONQUESTION | MB_OKCANCEL | MB_DEFBUTTON2;
		}else{
			if ( (*param == 'Y') || (*param == 'O') ) param++;
			style = MB_ICONQUESTION | MB_OKCANCEL;
		}
	}
	src = param;
//	ZFixParameter(&param);
	GetLfGetParam((const TCHAR **)&param, param, tstrlen32(param) + 1 );
	if ( IDOK != PMessageBox(Z->hWnd, src, ZGetTitleName(Z), style) ){
		Z->result = ERROR_CANCELLED;
	}
}

void USEFASTCALL CmdKeyCommand(EXECSTRUCT *Z, const TCHAR *param) // %K
{
	HWND hWnd;

	hWnd = GetPPxhWndFromID(Z->Info, &param, NULL);
	if ( hWnd == BADHWND ) return; // 該当無し
	if ( hWnd != NULL ){ // 指定あり
		if ( SkipSpace(&param) == ',' ) param++;
	}

	if ( *param != '\"' ){
		XMessage(Z->hWnd, T("%K"), XM_GrERRld, MES_EPRM);
		Z->result = ERROR_INVALID_PARAMETER;
		return;
	}
	param++;

	for ( ; ; ){
		UTCHAR c;
		int key;

		c = SkipSpace(&param);
		if ( c < ' ' ) break;
		if ( c == '\"' ){
			param++;
			break;
		}
		key = GetKeyCode(&param);
		if ( key < 0 ){
			XMessage(Z->hWnd, T("%K"), XM_GrERRld, MES_EPRM);
			Z->result = ERROR_INVALID_PARAMETER;
			break;
		}
		if ( key == K_NULL ){
			Sleep(100);
			continue;
		}
		if ( hWnd == NULL ){ // 自分自身で実行
			ERRORCODE result;

			result = PPxInfoFunc32u(Z->Info, PPXCMDID_PPXCOMMAD, &key);
			if ( result > 1 ){ // NO_ERROR, ERROR_INVALID_FUNCTION 以外
				if ( (result == ERROR_CANCELLED) || !(Z->flags & XEO_IGNOREERR) ){
					Z->result = result;
					break;
				}
			}
		}else{ // 他のPPxに送信
			PostMessage(hWnd, WM_PPXCOMMAND, (WPARAM)key, 0);
		}
	}
}

void CmdPPv(EXECSTRUCT *Z, const TCHAR *paramptr, TCHAR *param) // %v
{
	TCHAR param2[CMDLINESIZE];

	GetCommandParameter(&paramptr, param, VFPS);
	if ( *param != '\0' ){
		VFSFixPath(param2, param, GetZCurDir(Z), VFSFIX_PATH | VFSFIX_NOFIXEDGE);
		PPxView(Z->hWnd, param2, 0);
	}else{
		PPXCMD_F pcmdf;

		pcmdf.source = T("VCDN");
		pcmdf.dest[0] = '\0';
		Get_F_MacroData(Z->Info, &pcmdf, &Z->EnumInfo);
		PPxView(Z->hWnd, pcmdf.dest, 0);
	}
}

void CmdZap(EXECSTRUCT *Z, const TCHAR *paramptr, TCHAR *param)
{
	TCHAR param2[CMDLINESIZE];
	const TCHAR *command;
	POINT pos;

	GetCommandParameter(&paramptr, param, CMDLINESIZE);
	if ( *paramptr == ',' ){
		paramptr++;
		GetCommandParameter(&paramptr, param2, CMDLINESIZE);
		command = *param2 ? param2 : NilStr;
	}else{
		command = NULL;
	}
	if ( *param == '\0' ){	// ファイル名が省略されていた
		GetValue(Z, 'C', param);
	}
	if ( Z->command == 'Z' ){	// %Z
		ZapMain(Z, command, param, NilStr, GetZCurDir(Z));
	}else{	// %z
		GetPopupPoint(Z, &pos);
		VFSSHContextMenu(Z->hWnd, &pos, GetZCurDir(Z), param, command);
	}
}

void CmdExecute(EXECSTRUCT *Z, const TCHAR *param) // *execute
{
	HWND hWnd;
	COPYDATASTRUCT copydata;

	SkipSpace(&param);
	hWnd = GetPPxhWndFromID(Z->Info, &param, NULL);
	if ( hWnd == BADHWND ) return; // 該当無し

	if ( SkipSpace(&param) == ',' ) param++;
	if ( hWnd == NULL ){ // 指定がない→自前実行
		Z->result = PP_ExtractMacro(Z->hWnd, Z->Info, NULL, param, NULL, 0);
	}else{ // 指定有り→指定PPxで実行
		copydata.dwData = 'H';
		copydata.cbData = TSTRSIZE32(param);
		copydata.lpData = (PVOID)param;
		SendMessage(hWnd, WM_COPYDATA, 0, (LPARAM)&copydata);
	}
}

void CmdFile(EXECSTRUCT *Z, TCHAR *olddir, const TCHAR *pptr, TCHAR *param)
{
	VFSFILEOPERATION fileop;
	TCHAR param2[CMDLINESIZE];

	fileop.src		= NULL;
	fileop.dest		= NULL;
	fileop.files	= NULL;
	fileop.option	= NULL;
	fileop.dtype	= VFSDT_UNKNOWN;
	fileop.info		= Z->Info;
	fileop.flags	= VFSFOP_FREEFILES;
	fileop.hReturnWnd = Z->hWnd;
								// 第１パラメータ：種類
	if ( SkipSpace(&pptr) == '!' ){ // autorun
		pptr++;
		setflag(fileop.flags, VFSFOP_AUTOSTART);
	}
	if ( SkipSpace(&pptr) == '\"' ){ // actionname
		GetQuotedParameter(&pptr, param, param + VFPS - 1);
	}else{
		TCHAR *dest, code;

		dest = param;
		for ( ;; ){
			code = *pptr;
			if ( (code == '\0') || (code == ',') ||
				 (code == ' ')  || (code == '\t') ){
				break;
			}
			*dest++ = code;
			pptr++;
		}
		*dest = '\0';
	}
	fileop.action = param;
	if ( SkipSpace(&pptr) == ',' ){	// 旧形式 // 第２パラメータ：コピー元
		pptr++;
		GetCommandParameter(&pptr, param2, VFPS);
		if ( param2[0] != '\0' ){
			ZSetCurrentDir(Z, olddir);
			fileop.src = Z->curdir; // ZSetCurrentDir で取得済み
			fileop.files = MakeFOPlistFromParam(param2, fileop.src);
		}else{
			fileop.files = MakeFOPlistFromPPx(Z->Info);
		}
		if ( fileop.files == NULL ){
			XMessage(Z->hWnd, NULL, XM_FaERRd, T("Alloc error"));
			Z->result = ERROR_INVALID_PARAMETER;
			return;
		}
								// 第３パラメータ：コピー先
		if ( *pptr == ',' ){
			pptr++;
			GetCommandParameter(&pptr, param2, VFPS);
			fileop.dest = param2;
							// 第４パラメータ：オプション
			SkipSpace(&pptr);
			if ( *pptr == ',' ) pptr++;
			if ( *pptr != '\0' ) fileop.option = pptr;
		}
	}else if ( *pptr != '\0' ){			// 新形式
		fileop.option = pptr;
	}
	if ( PPxFileOperation(NULL, &fileop) == FALSE ){
		Z->result = ERROR_CANCELLED;
	}
}

// １コマンド文を実行する -----------------------------------------------------
void ZExec(EXECSTRUCT *Z)
{
	TCHAR *param;
	TCHAR *lp;
	TCHAR linebuf[CMDLINESIZE];
	TCHAR olddir[VFPS];

	olddir[0] = '\0';
//	resetflag(Z->status, ST_MULTIPARAM);
	param = lp = Z->DstBuf;
	if ( Z->ExtendDst.top != 0 ){
		ThCatString(&Z->ExtendDst, Z->DstBuf);
		param = lp = (TCHAR *)Z->ExtendDst.bottom;
		lp += Z->ExtendDst.top / sizeof(TCHAR);
		Z->ExtendDst.top = 0;
	}else{
		lp += tstrlen(lp);
	}
	while ( lp > param ){ // 末尾の空白を除去
		lp--;
		if ( *lp != ' ' ) break;
		*lp = '\0';
	}

	if ( Z->func.off != 0 ){
		XMessage(Z->hWnd, NULL, XM_GrERRld,
				T("%%*%s missing ')'."), param + Z->func.off - 1);
		Z->result = ERROR_INVALID_PARAMETER;
		return;
	}

	SkipSpace((const TCHAR **)&param);
	switch( Z->command ){
		case CID_FILE_EXEC:				// 外部プロセスを実行
			if ( *param == '\0' ) break;
			ZSetCurrentDir(Z, olddir);
			if ( Z->flags & XEO_CONSOLE ){
				ExecOnConsole(Z, param);
				break;
			}
			// Z->curdir ... ZSetCurrentDir で取得済み
			if ( ComExecEx(Z->hWnd, param, Z->curdir, &Z->useppb, Z->flags, &Z->ExitCode) == FALSE ){
				Z->result = GetLastError();
				if ( Z->result == NO_ERROR ) Z->result = ERROR_PATH_NOT_FOUND;
			}
			break;
										// *closeppx 全 PPx 終了
		case CID_CLOSEPPX:
			CmdClosePPx(param);
			break;
										// *screensaver スクリーンセーバを起動
		case CID_SCREENSAVER:
			PPxCommonCommand(Z->hWnd, 0, K_SSav);
			break;
										// *logoff
		case CID_LOGOFF:
			ExitSession(Z->hWnd, EWX_LOGOFF);
			break;
										// *poweroff
		case CID_POWEROFF:
			ExitSession(Z->hWnd, EWX_POWEROFF);
			break;
										// *reboot
		case CID_REBOOT:
			ExitSession(Z->hWnd, EWX_REBOOT);
			break;
										// *shutdown
		case CID_SHUTDOWN:
			ExitSession(Z->hWnd, EWX_SHUTDOWN);
			break;
										// *terminate
		case CID_TERMINATE:
			ExitSession(Z->hWnd, EWX_FORCE);
			break;
										// *suspend
		case CID_SUSPEND:
			ExitSession(Z->hWnd, EWX_EX_SUSPEND);
			break;
										// *hibernate
		case CID_HIBERNATE:
			ExitSession(Z->hWnd, EWX_EX_HIBERNATE);
			break;
										// *lockpc
		case CID_LOCKPC:
			LockPC();
			break;
										// *httpget
		case CID_HTTPGET:
			CmdHttpGet(Z, param);
			break;
										// *cliptext
		case CID_CLIPTEXT:
			ClipTextData(Z->hWnd, param);
			break;
										// *selectppx
		case CID_SELECTPPX:
			CmdSelectPPx(Z, param);
			break;
										// *stop
		case CID_STOP:
			CmdStop(Z, param);
			break;
										// *nextitem
		case CID_NEXTITEM:
			CmdNextItem(Z, param);
			break;
										// *cd
		case CID_CD:
			ZGetFilePathParam(Z, (const TCHAR **)&param, GetZCurDir(Z));
			break;
										// *cursor
		case CID_CURSOR:
			CmdCursor(Z, param);
			break;
										// *ppe / *edit
		case CID_PPE:
		case CID_EDIT:
			CmdPPeEdit(Z, param);
			break;
										// *file
		case CID_FILE:
			CmdFile(Z, olddir, param, linebuf);
			break;
										// *insert, *insertsel
		case CID_INSERT:
		case CID_INSERTSEL:
			GetCommandParameter((const TCHAR **)&param, linebuf, TSIZEOF(linebuf));
			if ( Z->flags & XEO_CONSOLE ){
				Z->result = PPxInfoFunc32u(Z->Info,
						(Z->command == CID_INSERT) ? PPXCMDID_PPBINSERT : PPXCMDID_PPBINSERTSEL,
						 linebuf);
			}else{
				SendMessage(Z->hWnd, WM_PPXCOMMAND,
						(Z->command == CID_INSERT) ? KE_insert : KE_insertsel,
						(LPARAM)linebuf);
			}
			break;
										// *replace
		case CID_REPLACE:
			GetCommandParameter((const TCHAR **)&param, linebuf, TSIZEOF(linebuf));
			if ( Z->flags & XEO_CONSOLE ){
				Z->result = PPxInfoFunc32u(Z->Info, PPXCMDID_PPBREPLACE, linebuf);
			}else{
				SendMessage(Z->hWnd, WM_PPXCOMMAND, KE_replace, (LPARAM)linebuf);
			}
			break;
										// *focus
		case CID_FOCUS:
			CmdFocusPPx(Z, param);
			break;
										// *set
		case CID_SET:
			CmdSet(Z, (TCHAR *)param);
			break;
										// *forfile
		case CID_FORFILE:
			GetCommandParameter((const TCHAR **)&param, linebuf, TSIZEOF(linebuf));
			if ( *param == ',' ) param++;
			CmdDoForfile(Z, linebuf, param);
			break;
										// *alias
		case CID_ALIAS:
			CmdAlias(Z, (TCHAR *)param);
			break;
										// *customize
		case CID_CUSTOMIZE:
			CmdCust(Z, param, TRUE);
			break;
										// *setcust
		case CID_SETCUST:
			CmdCust(Z, param, FALSE);
			break;
										// *linecust
		case CID_LINECUST:
			CmdLineCust(Z, param);
			break;
										// *deletecust
		case CID_DELETECUST:
			CmdDeleteCust(param);
			break;
										// *monitoroff
		case CID_MONITOROFF:
			SendWmSysyemMessage(SC_MONITORPOWER, 2);
			break;
										// *execute
		case CID_EXECUTE:
			CmdExecute(Z, param);
			break;
										// *ppc
		case CID_PPC:
			ComZExecPPx(Z, PPcExeName, param);
			break;
										// *ppv
		case CID_PPV:
			ComZExecPPx(Z, PPvExeName, param);
			break;
										// *ppb
		case CID_PPB:
			ComZExecPPx(Z, T(PPBEXE), param);
			break;
										// *pptray
		case CID_PPTRAY:
			ComZExecPPx(Z, T(PPTRAYEXE), param);
			break;
										// *ppffix
		case CID_PPFFIX:
			ComZExecPPx(Z, T(PPFFIXEXE), param);
			break;
										// *ppcust
		case CID_PPCUST:
			ComZExecPPx(Z, T(PPCUSTEXE), param);
			break;
										// *freediruse
		case CID_FREEDRIVEUSE:
			PPxSendMessage(WM_PPXCOMMAND, K_FREEDRIVEUSE, upper(*param));
			Sleep(500); // 完全に開放されるまで少し待機
			break;
										// *launch
		case CID_LAUNCH:
			CmdLaunch(Z, param);
			break;
										// *start
		case CID_START:
			CmdStart(Z, param);
			break;
										// *job
		case CID_JOB:
			if ( IsTrue(JobListMenu(Z, SkipSpace((const TCHAR **)&param))) ){
				Z->result = ERROR_CANCELLED;
			}
			break;
										// *tip
		case CID_TIP:
			CmdShowTip(Z, param);
			break;
										// *help
		case CID_HELP:
			GetCommandParameter((const TCHAR **)&param, linebuf, VFPS);
			if ( linebuf[0] != '\0' ){
				PPxHelp(Z->hWnd, HELP_KEY, (DWORD_PTR)linebuf);
			}else{
				PPxHelp(Z->hWnd, HELP_FINDER, 0);
			}
			break;
										// *makedir
		case CID_MAKEDIR:
			CmdMakeDirectory(Z, param);
			break;
										// *makefile
		case CID_MAKEFILE:
			CmdMakeFile(Z, param);
			break;
										// *delete
		case CID_DELETE:
			CmdDeleteEntry(Z, param);
			break;
										// *rename
		case CID_RENAME:
			CmdRenameEntry(Z, param);
			break;
										// *linemessage
		case CID_LINEMESSAGE:
			PPxInfoFunc(Z->Info, PPXCMDID_SETPOPLINE, (void *)param);
			break;
										// *commandhash
		case CID_COMMANDHASH: {
			DWORD hash;

			GetCommandParameter((const TCHAR **)&param, linebuf, TSIZEOF(linebuf));
			hash = GetModuleNameHash(linebuf, linebuf);
			wsprintf(linebuf + tstrlen(linebuf), T(" is 0x%x"), hash);
			tInput(Z->hWnd, T("COMMANDHASH"), linebuf, TSIZEOF(linebuf), PPXH_GENERAL, PPXH_GENERAL);
			break;
		}
										// *setarchivecp
		case CID_SETARCHIVECP:
			#ifdef UNICODE
				CmdSetArchiveCP(Z, param);
			#else
				SendMessage(Z->hWnd, WM_PPXCOMMAND, TMAKEWPARAM(K_SETPOPMSG, POPMSG_MSG), (LPARAM)"Codepage change not support");
			#endif
			break;

		case 'I':						//	%I	情報メッセージボックス
		case 'Q':						//	%Q	確認メッセージボックス
			CmdMessageBox(Z, param);
			break;
										// *keycommand
		case CID_KEYCOMMAND:
		// case 'K' へ
		case 'K':						//	%K	内蔵コマンド
			CmdKeyCommand(Z, param);
			break;

		/* *NOIME コマンド
			実行したプロセスでIMEを完全に使用できないようにする。
			ただし、ウィンドウを作成する前に実行しないと機能しないので、
			コマンドの公開はしていない。*/
		case CID_NOIME:{
			DefineWinAPI(BOOL, ImmDisableIme, (DWORD));
			GETDLLPROC(GetModuleHandle(T("IMM32.DLL")), ImmDisableIme);
			if ( DImmDisableIme != NULL ) DImmDisableIme((DWORD)-1);
			break;
		}
		case CID_IME:
			SetIMEStatus(Z->hWnd, GetIntNumber((const TCHAR **)&param) > 0);
			break;
										// *flashwindow
		case CID_FLASHWINDOW: {
			HWND hWnd;

			hWnd = GetWindowHandleByText(Z->Info, param);
			if ( hWnd == NULL ) hWnd = Z->hWnd;
			PPxFlashWindow(hWnd, PPXFLASH_FLASH);
			break;
		}
										// *sound
		case CID_SOUND:
			ZGetFilePathParam(Z, (const TCHAR **)&param, linebuf);
			PlayWave(linebuf);
			break;
										// *wait
		case CID_WAIT:
			CmdWaitExtract(Z, param);
			break;
										// *emulatekey
		case CID_EMULATEKEY:
//		case 'k': へ
		case 'k':						//	%k	キーエミュレート
			CmdEmurateKeyInput(Z, param);
			break;
										//	%j	パスジャンプ
		case 'j':
			GetCommandParameter((const TCHAR **)&param, linebuf, VFPS);
			PPxInfoFunc(Z->Info, PPXCMDID_CHDIR, linebuf);
			break;
										// *string 特殊環境変数
		case CID_STRING:
			ZStringVariable(Z, (const TCHAR **)&param, StringVariable_command);
			break;
										// *chopdir 直下のディレクトリを削除
		case CID_CHOPDIR:
			CmdChopDirectory(Z, param);
			break;
										// *if 条件を満たしたら、この行を実行
		case CID_IF:
			CmdIf(Z, param);
			break;
										// *ifmatch 条件を満たしたら、この行を実行
		case CID_IFMATCH:
			CmdIfMatch(Z, param);
			break;
										// *pack	書庫作成
		case CID_PACK:
			CmdPack(Z, param);
			break;
										// *togglecustword
		case CID_TOGGLECUSTWORD:
			CmdToggleCustWord(param);
			break;
										// *checkupdate
		case CID_CHECKUPDATE:
			CmdCheckUpdate(Z, param);
			break;
										// *checksignature
		case CID_CHECKSIGNATURE:
			CmdCheckSignature(Z, param);
			break;
										// *addhistory
		case CID_ADDHISTORY:
			CmdAddHistory(param);
			break;
										// *deletehistory
		case CID_DELETEHISTORY:
			CmdDeleteHistory(param);
			break;
										// *trimmark
		case CID_TRIMMARK:
			PPxEnumInfoFunc(Z->Info, PPXCMDID_TRIMENUM, Z->DstBuf, &Z->EnumInfo);
			break;
										// *cpu
		case CID_CPU:
			CPUinfo();
			break;
										// *clearauth
		case CID_CLEARAUTH:
			AuthHostCache[0] = '\0';
			break;
										// *return
		case CID_RETURN:
			GetCommandParameter((const TCHAR **)&param, Z->DstBuf, CMDLINESIZE);
			Z->result = ERROR_EX_RETURN;
			Z->dst = Z->DstBuf + tstrlen(Z->dst);
			Z->src += tstrlen(Z->src);
			break;
										// *maxlength
		case CID_MAXLENGTH:
			Z->LongResultLen = GetDigitNumber32u((const TCHAR **)&param);
			setflag(Z->status, ST_LONGRESULT);
			break;
		case CID_GOTO:
			CmdGoto(Z,param);
			break;
										// *jumpentry
		case CID_JUMPENTRY:
//		case 'J': へ
		case 'J':						//	%J	エントリ移動
			GetCommandParameter((const TCHAR **)&param, linebuf, VFPS);
			PPxInfoFunc(Z->Info, PPXCMDID_PATHJUMP, linebuf);
			break;
										//	%u	UnXXX を実行
		case 'u':
			GetCommandParameter((const TCHAR **)&param, linebuf, VFPS);
			if ( *param != ',' ){
				XMessage(Z->hWnd, T("%u"), XM_GrERRld, MES_EPRM);
				Z->result = ERROR_INVALID_PARAMETER;
			}else{
				tstrreplace((TCHAR *)param + 1, T(">"), T(" "));
				ZSetCurrentDir(Z, olddir);
				if ( DoUnarc(Z->Info, linebuf, Z->hWnd, param + 1) != 0 ){
					Z->result = ERROR_BAD_COMMAND;
				}
			}
			break;
										//	%v	PPV で表示
		case 'v':
			CmdPPv(Z, param, linebuf);
			break;

		case 'z':						//	%z	ShellContextMenu で実行
		case 'Z':						//	%Z	ShellExecute で実行
			CmdZap(Z, param, linebuf);
			break;

		default:
			if ( Z->command >= CID_USERNAME ){
				ERRORCODE result;

				// 各PPx固有機能
				if ( NO_ERROR != (result = PPxInfoFunc32u(Z->Info, PPXCMDID_COMMAND, (void *)param)) ){
					Z->result = result ^ 1;
					break;
				}

				// コマンドモジュール
				ZSetCurrentDir(Z, olddir);
				result = CommandModule(Z, param);
				if ( result == (ERRORCODE)PPXMRESULT_STOP ){
					Z->result = ERROR_CANCELLED;
					break;
				}
				if ( result != PPXMRESULT_SKIP ) break;

				// user コマンド
				linebuf[CMDLINESIZE - 1] = '\0';
				if ( NO_ERROR != GetCustTable(StrUserCommand, param, linebuf, sizeof(linebuf)) ){
					XMessage(Z->hWnd, NULL, XM_GrERRld, MES_EUXC, param);
					Z->result = ERROR_BAD_COMMAND;
					break;
				}else if ( linebuf[CMDLINESIZE - 1] != '\0' ){
					TCHAR *longbuf;
					int size = GetCustTableSize(StrUserCommand, param);

					longbuf = HeapAlloc(DLLheap, 0, size);
					if ( longbuf == NULL ){
						Z->result = RPC_S_STRING_TOO_LONG;
						break;
					}
					GetCustTable(StrUserCommand, param, longbuf, size);
					UserCommand(Z, NULL, param, longbuf, NULL);
					HeapFree(DLLheap, 0, longbuf);
					break;
				}else{
					UserCommand(Z, NULL, param, linebuf, NULL);
					break;
				}
			}
			XMessage(Z->hWnd, NULL, XM_GrERRld, T("ZExec:Not command"));
			Z->result = ERROR_CANCELLED;
	}
	if ( olddir[0] != '\0' ) SetCurrentDirectory(olddir);
}
