/*-----------------------------------------------------------------------------
	Paper Plane xUI	 Virtual File System				～ HTTP処理 ～
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H の DLL export 指定
#include "WINAPI.H"
#include <winsock.h>
#include "PPXVER.H"
#include "PPX.H"
#include "PPD_DEF.H"
#include "VFS.H"
#include "VFS_STRU.H"
#include "VFS_FF.H"
#pragma hdrstop

#define MAX_BUF 0x1000

typedef struct impaddrinfo {
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	size_t ai_addrlen;
	char *ai_canonname;
	struct sockaddr *ai_addr;
	struct impaddrinfo *ai_next;
} impADDRINFO;

// Winsock --------------------------

int (WINAPI *Drecv)(SOCKET s, char FAR * buf, int len, int flags);
int (WINAPI *Dsend)(SOCKET s, const char FAR * buf, int len, int flags);
int (WINAPI *DWSAStartup)(WORD wVersionRequested, LPWSADATA lpWSAData);
SOCKET (WINAPI *Dsocket)(int af, int type, int protocol);
unsigned long (WINAPI *Dinet_addr)(const char FAR * cp);
struct hostent FAR *(WINAPI *Dgethostbyname)(const char FAR * name);
//struct servent FAR *(PASCAL FAR *Dgetservbyname)(const char FAR * name, const char FAR * proto);
int (WINAPI *Dconnect)(SOCKET s, const struct sockaddr FAR * name, int namelen);
int (WINAPI *Dclosesocket)(SOCKET s);
int (WINAPI *DWSACleanup)(void);
u_short (WINAPI *Dhtons)(u_short hostshort);
int (WINAPI *DWSAGetLastError)(void);
int (WINAPI *Dshutdown)(SOCKET s, int how);

#ifdef UNICODE
	#define GETADDRINFONAME "getaddrinfo"
	#define FREEADDRINFONAME "freeaddrinfo"
//	#define GETADDRINFONAME "GetAddrInfoW"
//	#define FREEADDRINFONAME "FreeAddrInfoW"
#else
	#define GETADDRINFONAME "getaddrinfo"
	#define FREEADDRINFONAME "freeaddrinfo"
#endif

//DefineWinAPI(int, getaddrinfo, (CTCHAR *pNodeName, CTCHAR *pServiceName, const impADDRINFO *pHints, impADDRINFO **ppResult));
DefineWinAPI(int, getaddrinfo, (const char *pNodeName, const char *pServiceName, const impADDRINFO *pHints, impADDRINFO **ppResult));
DefineWinAPI(void, freeaddrinfo, (impADDRINFO *pAddrInfo));

LOADWINAPISTRUCT WINSOCKDLL[] = {
	LOADWINAPI1(WSAStartup),
	LOADWINAPI1(socket),
	LOADWINAPI1(inet_addr),
	LOADWINAPI1(gethostbyname),
//	LOADWINAPI1(getservbyname),
	LOADWINAPI1(connect),
	LOADWINAPI1(recv),
	LOADWINAPI1(send),
	LOADWINAPI1(closesocket),
	LOADWINAPI1(WSACleanup),
	LOADWINAPI1(htons),
	LOADWINAPI1(WSAGetLastError),
	LOADWINAPI1(shutdown),
	{NULL, NULL}
};

// SSL -------------------------
#define DefineCdeclAPI(retvar, name, param) typedef retvar (CDECL *imp ## name) param; imp ## name D ## name

typedef void SSL;
typedef void SSL_CTX;
typedef void SSL_METHOD;

DefineCdeclAPI(SSL_CTX *, SSL_CTX_new, (SSL_METHOD *meth));
DefineCdeclAPI(void, SSL_CTX_free, (SSL_CTX *));
DefineCdeclAPI(int, SSL_set_fd, (SSL *s, int fd));
DefineCdeclAPI(void, SSL_load_error_strings, (void));
DefineCdeclAPI(SSL *, SSL_new, (SSL_CTX *ctx));
DefineCdeclAPI(void, SSL_free, (SSL *ssl));
DefineCdeclAPI(int, SSL_connect, (SSL *ssl));
DefineCdeclAPI(int, SSL_read, (SSL *ssl, void *buf, int num));
DefineCdeclAPI(int, SSL_peek, (SSL *ssl, void *buf, int num));
DefineCdeclAPI(int, SSL_write, (SSL *ssl, const void *buf, int num));
DefineCdeclAPI(long, SSL_CTX_ctrl, (SSL_CTX *ctx, int cmd, long larg, void *parg));
DefineCdeclAPI(SSL_METHOD *, SSLv23_client_method, (void));
DefineCdeclAPI(int, SSL_shutdown, (SSL *s));
DefineCdeclAPI(int, SSL_library_init, (void));

LOADWINAPISTRUCT SSLEAY32DLL[] = {
	LOADWINAPI1(SSL_CTX_new),
	LOADWINAPI1(SSL_CTX_free),
	LOADWINAPI1(SSL_set_fd),
	LOADWINAPI1(SSL_load_error_strings),
	LOADWINAPI1(SSL_new),
	LOADWINAPI1(SSL_free),
	LOADWINAPI1(SSL_connect),
	LOADWINAPI1(SSL_read),
	LOADWINAPI1(SSL_peek),
	LOADWINAPI1(SSL_write),
	LOADWINAPI1(SSL_CTX_ctrl),
	LOADWINAPI1(SSLv23_client_method),
	LOADWINAPI1(SSL_shutdown),
	LOADWINAPI1(SSL_library_init),
	{NULL, NULL}
};

#define SSL_CTRL_MODE 33
#define SSL_MODE_AUTO_RETRY 0x00000004L
#define DSSL_CTX_set_mode(ctx, op) DSSL_CTX_ctrl((ctx), SSL_CTRL_MODE, (op), NULL)

// -------------------------
typedef struct {
	SOCKET sock;
	SSL *ssl;
} SSOCKET;

const TCHAR RegProxy[] =T("Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
const TCHAR ProxyEnable[] = T("ProxyEnable");
const char ProxyServer[] = "ProxyServer";
const char DefAgent[] = "PaperPlaneXUI/" FileCfg_Version;
const char HttpResult[] = "=Result=\r\n";
const char SSLerrorstr[] = "\r\n\r\nSocks error\r\n";

void SockErrorMsg(ThSTRUCT *th, ERRORCODE code, HMODULE hModule)
{
	char buf[MAX_BUF];
	char *p;

	strcpy(buf, "\r\n\r\nError:");
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE |
			FORMAT_MESSAGE_IGNORE_INSERTS, hModule, code,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT), buf + 10, VFPS, NULL);
	for ( p = buf + 10 ; *p ; p++ ) if ( (BYTE)*p < ' ' ) *p = ' ';
	if ( strlen(buf) < 13 ) wsprintfA(buf + 10, "Unknown(%d)", code);
	ThCatStringA(th, buf);
	return;
}

int GetProxySetting(char *proxyserver)
{
	HKEY HK;
	DWORD size;

	DWORD enable = 0, port = 80;
	char servers[0x800], *p, *q;

	if ( RegOpenKeyEx(HKEY_CURRENT_USER, RegProxy, 0, KEY_READ, &HK) != ERROR_SUCCESS ){
		return 0;
	}
	size = sizeof(DWORD);
	RegQueryValueEx(HK, ProxyEnable, NULL, NULL, (LPBYTE)&enable, &size);
	servers[0] = '\0';
	size = sizeof(servers);
	RegQueryValueExA(HK, ProxyServer, NULL, NULL, (LPBYTE)servers, &size);
	RegCloseKey(HK);

	if ( enable == 0 ) return 0;

	p = strstr(servers, "http=");
	if ( p != NULL ){
		p += 5;
	}else{
		p = strchr(servers, '=');
		if ( p != NULL ) return 0;
		p = servers;
	}
	q = strchr(p, ':');
	if ( q == NULL ){
		q = strchr(p, ';');
		if ( q != NULL ) *q = '\0';
	}else{
		*q++= '\0';
		port = 0;
		while(Isdigit(*q)){
			port = (DWORD)(port * 10 + (BYTE)(*q++ - (BYTE)'0'));
		}
	}
	strcpy(proxyserver, p);
	return port;
}

//１行送信（改行は、内で付加される）
int wsPuts(SSOCKET *ssock, char *str)
{
	char *p;
	int n;
	int len;

	p = str;
	len = strlen32(p);
	while(len){
		if ( ssock->ssl == NULL ){
			n = Dsend(ssock->sock, p, len, 0);
		}else{
			n = DSSL_write(ssock->ssl, p, len);
		}
		if ( n == 0 ) return FALSE;
		if ( n == SOCKET_ERROR ){
			int errorcode;

			errorcode = DWSAGetLastError();
			if ( errorcode == NO_ERROR ) continue;
			return errorcode;
		}
		p += n;
		len -= n;
	};

	p = "\r\n";
	len = 2;
	while(len){
		if ( ssock->ssl == NULL ){
			n = Dsend(ssock->sock, p, len, 0);
		}else{
			n = DSSL_write(ssock->ssl, p, len);
		}
		if ( n == 0 ) return FALSE;
		if ( n == SOCKET_ERROR ){
			int errorcode;

			errorcode = DWSAGetLastError();
			if ( errorcode == NO_ERROR ) continue;
			return errorcode;
		}
		p += n;
		len -= n;
	};

	return NO_ERROR;
}

VFSDLL BOOL PPXAPI GetImageByHttp(const TCHAR *urladr, ThSTRUCT *th)
{
	HANDLE hWSOCK32;
	HANDLE hSsleay32 = NULL;

	SSOCKET ssock;
	impADDRINFO ai_default, *adr_info = NULL, *useadr;
	WSADATA wsad;
	SSL_CTX *ctx;

	SOCKADDR_IN addr_in;

	char url[MAX_BUF];
	char host[VFPS];
	char buf[MAX_BUF];
	const char *Agent = DefAgent;
	char AgentBuf[MAX_BUF];
	char AuthorizationBuf[MAX_BUF];
//	DWORD resultoffset;

	int cache = 0;
	int errorcode;
	int getport = 0, connectport = 80;
	#ifdef UNICODE
	TCHAR	bufW[VFPS];
	#endif
	HWND hFocusWnd;
	DWORD tickcount;

	char *getdir, *urldir;

#ifdef _WIN64
	#define reqwsver 0x202 // 必ず2.2
#else
	WORD reqwsver;
#endif

	ThInit(th);
#ifdef UNICODE
	hWSOCK32 = LoadWinAPI("WS2_32.dll", NULL, WINSOCKDLL, LOADWINAPI_LOAD_ERRMSG);
#else
	hWSOCK32 = LoadWinAPI((WinType >= WINTYPE_2000) ?
		"WS2_32.dll" : "WSOCK32.DLL", NULL, WINSOCKDLL, LOADWINAPI_LOAD_ERRMSG);
#endif
	if ( hWSOCK32 == NULL ) return FALSE;
	Dgetaddrinfo = (impgetaddrinfo)GetProcAddress(hWSOCK32, GETADDRINFONAME);
	Dfreeaddrinfo = (impfreeaddrinfo)GetProcAddress(hWSOCK32, FREEADDRINFONAME);

	if ( urladr[4] == 's' ){ // https://
		if ( hSsleay32 == NULL ){
			hSsleay32 = LoadWinAPI("SSLEAY32.DLL", NULL, SSLEAY32DLL, LOADWINAPI_LOAD_ERRMSG);
			if ( hSsleay32 == NULL ){
				FreeLibrary(hWSOCK32);
				return FALSE;
			}

			DSSL_library_init();
			DSSL_load_error_strings();
		}
		//	RAND_seed(seed, sizeof(seed) - 1);
		//	SSLeay_add_ssl_algorithms();
		ctx = DSSL_CTX_new(DSSLv23_client_method());
		DSSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
	}


#ifndef _WIN64
	if ( WinType >= WINTYPE_XP ){
		reqwsver = 0x202; // XP以降は2.2
	}else{
		reqwsver = 0x101; // 2000以前は1.1
	}
#endif
	if ( DWSAStartup(reqwsver, &wsad) != 0 ) return FALSE;

	AuthorizationBuf[0] = '\0';
	hFocusWnd = GetFocus();

	{
		char domain[MAX_PATH];
		TCHAR portbuf[32];
		char *q, *r;
												// コード変換 -----------------
		#ifdef UNICODE
			UnicodeToAnsi(urladr, url, sizeof(url));
		#else
			strcpy(url, urladr);
		#endif
		q = url;
		for ( ; ; ){
			r = strchr(q, '&');
			if ( r == NULL ) break;
			q = r + 1;
			if ( !memcmp( q, "amp;", 4) ){
				memmove( q, q + 4, strlen(q) - 3);
			}
		}
/*
		q = url;
		for ( ; ; ){
			r = strchr(q, '%');
			if ( r == NULL ) break;
			q = r + 1;
			if ( IsxdigitA(*q) && IsxdigitA(*(q + 1)) ){
				*r = (BYTE)(GetHexCharA(&q) * 16);
				*r |= (BYTE)GetHexCharA(&q);
				memmove( r + 1, q, strlen(q) + 1);
				q = r + 1;
			}
		}
*/
												// ドメイン部分を切り出し -----
		getdir = url + sizeof("http://") - 1;
		if ( url[4] == 's' ) getdir++;
		urldir = strchr(getdir, '/');
		if ( urldir == NULL ){
			urldir = getdir + strlen(getdir);
			strcpy(urldir, "/index.html");
		}
		memcpy(host, getdir, urldir - getdir);
		*(host + (urldir - getdir)) = '\0';
		{										// ポートの指定 ---------------
			char *pt;

			pt = strchr(host, ':');
			if ( pt != NULL ){
				*pt++ = '\0';
#ifdef UNICODE
				getport = (int)GetNumberA((const char **)&pt);
#else
				getport = (int)GetNumber((const char **)&pt);
#endif
			}
			if ( getport == 0 ) getport = (hSsleay32 == NULL) ? 80 : 443;
//			addr_in.sin_port = Dhtons((WORD)port);	//ポートの指定
			#if 0
				struct servent *service;
				service = Dgetservbyname("http", "tcp");
				if ( service ){
					addr_in.sin_port = service->s_port;
				}else{
					addr_in.sin_port = Dhtons((u_short)80);	//ポートの指定
				}
			#endif
		}

		#ifdef UNICODE
			bufW[0] = '\0';
			GetCustData(T("V_proxy"), &bufW, sizeof(bufW));
			UnicodeToAnsi(bufW, domain, sizeof(domain));
		#else
			domain[0] = '\0';
			GetCustData(T("V_proxy"), &domain, sizeof(domain));
		#endif

		if ( domain[0] == '\0' ){
			connectport = GetProxySetting(domain);
		}
		if ( domain[0] != '\0' ){
			getdir = url;
			ThCatStringA(th, "Domain(proxy):");

			portbuf[0] = '\0';
			GetCustData(T("V_http"), &portbuf, sizeof(portbuf));
			if ( portbuf[0] != '\0' ){
				const TCHAR *tp;

				tp = portbuf;
				connectport = (int)GetNumber(&tp);
			}
		}else{
			connectport = getport;
			strcpy(domain, host);
			getdir = urldir;
			ThCatStringA(th, "Domain:");
		}
		ThCatStringA(th, domain);
		ThCatStringA(th, "\r\n");

		if ( hFocusWnd != NULL ){
			if ( SendMessage(hFocusWnd, WM_PPXCOMMAND, K_SETPOPLINENOLOG,
					(LPARAM)T("Resolving")) != 1 ){
				hFocusWnd = NULL;
			}
		}
		memset(&ai_default, 0, sizeof(ai_default));
		ai_default.ai_socktype = SOCK_STREAM;
		ssock.ssl = NULL;
		ssock.sock = INVALID_SOCKET;

		if ( Dgetaddrinfo == NULL ){ // 古い形式
			DWORD ip;

			useadr = &ai_default;
			addr_in.sin_family = AF_INET;
			addr_in.sin_port = Dhtons((u_short)connectport);
			ai_default.ai_addrlen = sizeof(addr_in);
			ai_default.ai_addr = (struct sockaddr *)&addr_in;
									//ドット形式のアドレスをIPアドレスへ変換
			ip = Dinet_addr(domain);
			if ( ip == INADDR_NONE ){ //ドット形式でないとき
				struct hostent *pHost;

				pHost = Dgethostbyname(domain);//ホスト名->IPアドレス変換
				if ( pHost != NULL ) ip = *((DWORD*)(pHost->h_addr));
			}
			if ( ip != INADDR_NONE ){
				addr_in.sin_addr.s_addr = ip;
				ssock.sock = Dsocket(PF_INET, SOCK_STREAM, 0);
			}
		}else{ // IPv6対応版
			wsprintfA(buf, "%d", connectport);
			ai_default.ai_family = PF_UNSPEC;
			if ( Dgetaddrinfo(domain, buf, &ai_default, &adr_info) == 0 ){
				for ( useadr = adr_info ; useadr != NULL ; useadr = useadr->ai_next ){
					ssock.sock = Dsocket(useadr->ai_family, useadr->ai_socktype, useadr->ai_protocol);
					if ( ssock.sock != INVALID_SOCKET ) break;
				}
			}
		}
	}

	AgentBuf[0] = '\0';
	#ifdef UNICODE
	bufW[0] = '\0';
	GetCustData(T("V_httpa"), &bufW, sizeof(bufW));
	UnicodeToAnsi(bufW, AgentBuf, sizeof(AgentBuf));
	#else
	GetCustData(T("V_httpa"), &AgentBuf, sizeof(AgentBuf));
	#endif
	if ( AgentBuf[0] != '\0' ) Agent = AgentBuf;

													//ソケットのオープン
	if ( ssock.sock != INVALID_SOCKET ) for(;;){
		int retry = 3;

		if ( hFocusWnd != NULL ){
			SendMessage(hFocusWnd, WM_PPXCOMMAND,
					K_SETPOPLINENOLOG, (LPARAM)T("Connecting"));
		}
													//接続
		if ( Dconnect(ssock.sock, useadr->ai_addr, useadr->ai_addrlen) ) break;
		if ( hSsleay32 != NULL ){
			if ( getdir == url ){ // proxy
				int getlen;
				wsprintfA(buf, "CONNECT %s:%d HTTP/1.1\r\n", host, getport);
				ThCatStringA(th, buf);
				errorcode = wsPuts(&ssock, buf);
				if ( errorcode != NO_ERROR ){
					SockErrorMsg(th, errorcode, hWSOCK32);
					break;
				}
				getlen = Drecv(ssock.sock, buf, sizeof(buf) - 1, 0);
				if ( getlen < 0 ) break;
				buf[getlen] = '\0';
				ThCatStringA(th, buf);
				if ( strstr(buf, " 200") == NULL ) break; // 200 以外は失敗
				if ( th->top > 2 ) th->top -= 2;
				getdir = urldir;
			}
			ssock.ssl = DSSL_new(ctx);
			DSSL_set_fd(ssock.ssl, (int)ssock.sock);	// 注意!x64! ssock.sock で警告
			if ( DSSL_connect(ssock.ssl) != 1 ){
				ThCatStringA(th, SSLerrorstr);
				break;
			}
		}

		GetCustData(T("V_httpc"), &cache, sizeof(cache));

		wsprintfA(buf,
			"GET %s HTTP/1.0\r\n"
			"Accept: text/html, */*\r\n"
			"Accept-Language: ja-JP\r\n"
			"User-Agent: %s\r\n"
			"Connection: close\r\n"
//			"Authorization: \r\n"
//			"Proxy-Authorization: \r\n"
//			"Referer: \r\n"
//			"From: tester@hogehoge\r\n"
			"Host: %s\r\n"
			, getdir, Agent, host);
		if ( cache != 0 ){
			strcat(buf, "Pragma: no-cache\r\nCache-Control: no-cache\r\n");
		}
		errorcode = wsPuts(&ssock, buf);
		if ( errorcode != NO_ERROR ){
			SockErrorMsg(th, errorcode, hWSOCK32);
			break;
		}
		ThCatStringA(th, buf);
		ThCatStringA(th, HttpResult);
//		resultoffset = th->top;

		if ( hFocusWnd != NULL ){
			SendMessage(hFocusWnd, WM_PPXCOMMAND, K_SETPOPLINENOLOG, (LPARAM)T("Reading"));
			tickcount = GetTickCount();
		}
		for ( ; ; ){
			int getlen;

			if ( (WinType >= WINTYPE_VISTA) && (th->size >= (32 * KB)) ){
				getlen = (th->size / 2) & ~MAX_BUF;
			}else{
				getlen = MAX_BUF;
			}
			ThSize(th, getlen);
			if ( ssock.ssl == NULL ){
				getlen = Drecv(ssock.sock,
						th->bottom + th->top, th->size - th->top - 1, 0);
			}else{
				getlen = DSSL_read(ssock.ssl,
						th->bottom + th->top, th->size - th->top - 1);
			}
			*(th->bottom + th->top + getlen) = '\0';

			if ( getlen == 0 ){
				retry--;
				if ( retry == 0 ) break;
			}else if ( getlen == SOCKET_ERROR ){
				int wsaerrorcode;

				wsaerrorcode = DWSAGetLastError();
				if ( wsaerrorcode == NO_ERROR ) continue;
				SockErrorMsg(th, wsaerrorcode, hWSOCK32);
				break;
			}

			th->top += getlen;
			if ( hFocusWnd != NULL ){
				DWORD newtickcount;

				newtickcount = GetTickCount();
				if ( (newtickcount - tickcount) >= 200 ){
					#ifdef UNICODE
						#define buft bufW
					#else
						#define buft buf
					#endif
					tickcount = newtickcount;
					wsprintf(buft, T("Reading %d"), th->top);
					SendMessage(hFocusWnd, WM_PPXCOMMAND, K_SETPOPLINENOLOG, (LPARAM)buft);
				}
			}
		}

		ThAppend(th, "", 1);
		if ( ssock.ssl != NULL ){
			DSSL_shutdown(ssock.ssl);
			DSSL_free(ssock.ssl);
			DSSL_CTX_free(ctx);
		}
		Dshutdown(ssock.sock, 0);	// recv を無効に
/*
		{
			char *p;

			p = th->bottom + resultoffset;
			p = strchr(p, ' ');
			if ( p != NULL ){
				p++;
				if ( !memcmp(p, "401", 3) ){
				// Authorization: Basic base64==
				}
			}
		}
*/
		break;
	}

	if ( hFocusWnd != NULL ){
		SendMessage(hFocusWnd, WM_PPXCOMMAND, K_SETPOPLINENOLOG, (LPARAM)NULL);
	}

	Dclosesocket(ssock.sock);
	if ( adr_info != NULL ) Dfreeaddrinfo(adr_info);
	DWSACleanup();
	FreeLibrary(hWSOCK32);
	return TRUE;
}

BOOL MakeWebListSub(ThSTRUCT *th, char *p, char *name)
{
	char *q, buf[MAX_PATH];

	while ( (p = strstr(p, name)) != NULL ){
		p += strlen(name);

		while ( (*p == ' ') || (*p == '\t') ) p++;
		if ( *p != '=' ) continue;
		p++;
		while ( (*p == ' ') || (*p == '\t') ) p++;
		if ( *p != '\"' ){
			q = p;
			while ( (BYTE)*q > ' ' ) q++;
		}else{
			p++;
			q = strchr(p, '\"');
			if ( !q ) continue;
		}
		if ( (DWORD)(q - p) >= MAX_PATH ) continue;
		if ( (DWORD)(q - p) == '\0' ) continue;
		if ( *p == '#' ) continue; // アンカーはリスト不要
		memcpy(buf, p, q - p);
		buf[q - p] = '\0';
		p = q;

		// 重複チェック
		q = th->bottom;
		while ( (DWORD)(q - th->bottom) < th->top ){
			if ( strcmp(q, buf) == 0 ) break;
			q += strlen(q) + 1;
		}
		if ( (DWORD)(q - th->bottom) >= th->top ){
			#ifdef UNICODE
				TCHAR nameA[MAX_PATH];

				AnsiToUnicode(buf, nameA, MAX_PATH);
				ThAddString(th, nameA);
			#else
				ThAddString(th, buf);
			#endif
		}
	}
	return TRUE;
}

ERRORCODE MakeWebList(FF_MC *mc, const TCHAR *filename, BOOL file)
{
	ThSTRUCT th;
	char *bottom;

	ThInit(&mc->dirs);
	ThInit(&mc->files);
	mc->d_off = 0;
	mc->f_off = 0;
										// 「\\」==============================
	if ( file == FALSE ){
		char *p;
		char statusF;

		ThAddString(&mc->dirs, T(":"));

		if ( GetImageByHttp(filename, &th) == FALSE ) return ERROR_READ_FAULT;
		bottom = (char *)th.bottom;
		// ステータスを取得する
		p = strstr(bottom, HttpResult);
		if ( p != NULL ){
			p += sizeof(HttpResult) - 1;
			for (;;){
				char c = *p;

				if ( IsalnumA(c) || (c == '/') || (c == '.') ){
					p++;
					continue;
				}
				break;
			}
			for (;;){
				statusF = *p;
				if ( statusF == ' ' ){
					p++;
					continue;
				}
				break;
			}
		}else{
			statusF = '\0';
		}
		if ( statusF != '2' ){
			if ( IsdigitA(statusF) ){
				char *plf;

				plf = strchr(p, '\r');
				if ( plf != NULL ) *plf = '\0';
				if ( strlen(p) > 200 ) *(p + 200) = '\0';

				#ifdef UNICODE
				{
					TCHAR nameA[MAX_PATH];

					AnsiToUnicode(p, nameA, MAX_PATH);
					ThAddString(&mc->files, nameA);
				}
				#else
					ThAddString(&mc->files, p);
				#endif
			}else{
				ThAddString(&mc->files, T("HTTP unknown error"));
			}
			bottom = NULL;
		}else{
			p = strstr(bottom, "\r\n\r\n");
			if ( p && *(p + 4) ){
				bottom = p + 4;
			}
		}
	}else{
		bottom = NULL;
		if ( LoadFileImage(filename, 0x40, &bottom, NULL, NULL) ){
			return GetLastError();
		}

		ThAddString(&mc->dirs, T("."));
		ThAddString(&mc->dirs, T(".."));
	}
	if ( bottom != NULL ){
		MakeWebListSub(&mc->dirs, bottom, "HREF");
		MakeWebListSub(&mc->dirs, bottom, "href");
		MakeWebListSub(&mc->files, bottom, "SRC");
		MakeWebListSub(&mc->files, bottom, "src");
	}
	ThAddString(&mc->dirs, NilStr);
	ThAddString(&mc->files, NilStr);

	if ( file ) HeapFree(DLLheap, 0, bottom);
	return NO_ERROR;
}
