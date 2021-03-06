/*-----------------------------------------------------------------------------
	64 bit 整数の操作用関数
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX_64.H"
#pragma hdrstop

#ifndef _WIN64
/*-----------------------------------------------
	DOUBLE DWORD を 1G 未満で２つの DWORD に分ける
-----------------------------------------------*/
void DDwordToDten(DWORD srcl, DWORD srch, DWORD *destl, DWORD *desth)
{
#ifdef _WIN64
	unsigned long long int a;

	if (srch >= DWORDTEN){
		*destl = *desth = DWORDTEN - 1;
		return;
	}
	a = ((unsigned long long int)srch << 32) + (unsigned long long int)srcl;
	*desth = (DWORD)(a / 1000000000uLL);
	*destl = (DWORD)(a - (unsigned long long int)*desth * 1000000000uLL);
#else
 #if USETASM32
	if (srch >= DWORDTEN){
		*destl = *desth = DWORDTEN - 1;
		return;
	}

	asm {
		mov edx, srch
		mov eax, srcl
		mov ebx, DWORDTEN
		div ebx
		mov ebx, desth
		mov [ebx], eax
		mov ebx, destl
		mov [ebx], edx
	}
 #else
	double a;

	if (srch >= DWORDTEN){
		*destl = *desth = DWORDTEN - 1;
		return;
	}
	a = (double)srch * DWORDPP + (double)srcl;
	*desth = (DWORD)(a / 1e9);
	*destl = (DWORD)(a - (double)*desth * 1e9);
 #endif
#endif
}
#endif
/*-----------------------------------------------
	２つの DWORD を掛算し、DOUBLE DWORD を作成する
-----------------------------------------------*/
void DDmul(DWORD src1, DWORD src2, DWORD *destl, DWORD *desth)
{
#ifdef _WIN64
	unsigned long long int a;

	a = (unsigned long long int)src1 * (unsigned long long int)src2;
	*desth = (DWORD)(a >> 32);
	*destl = (DWORD)a;
#else
 #if USETASM32
	asm {
		mov eax, src1
		mul src2
		mov ecx, destl
		mov [ecx], eax
		mov eax, desth
		mov [eax], edx
	}
 #else
	double a;

	a = (double)src1 * (double)src2;
	*desth = (DWORD)(a / DWORDPP);
	*destl = (DWORD)(a - (double)*desth * DWORDPP);
 #endif
#endif
}

/*-----------------------------------------------
 FPU の初期化をする
-----------------------------------------------*/
BOOL InitFloat(void)
{
#if !defined(_WIN64) && USETASM32
	static BOOL float_init = FALSE;

	if ( float_init != FALSE ) return FALSE;
	asm {
		finit
	}
	float_init = TRUE;
	return TRUE;
#else
	return FALSE;
#endif
}
/*-----------------------------------------------
 migemo の読み出し規約違いを検出・対処する
-----------------------------------------------*/
#if !defined(_WIN64) && USETASM32
typedef struct _migemo{int dummy;} migemo;
migemo *(__stdcall * migemo_open)(const char *dict) = NULL;
// 1.2 以前の旧版
void (__cdecl * migemo_close_cdecl)(migemo *object);
char *(__cdecl * migemo_query_cdecl)(migemo *object, const char *query);
void (__cdecl * migemo_release_cdecl)(migemo *object, char *string);
// 1.3 以降のの新版
void (__stdcall * migemo_close)(migemo *object);
char *(__stdcall * migemo_query)(migemo *object, const char *query);
void (__stdcall * migemo_release)(migemo *object, char *string);

extern migemo * migemo_open_fix(const char *dict);

void __stdcall migemo_close_fix(migemo *object)
{
	migemo_close_cdecl(object);
}
char * __stdcall migemo_query_fix(migemo *object, const char *query)
{
	return migemo_query_cdecl(object, query);
}
void __stdcall migemo_release_fix(migemo *object, char *string)
{
	migemo_release_cdecl(object, string);
}

migemo * migemo_open_fix(const char *dict)
{
	migemo *result;
	DWORD IsCDECL;

	// migemo_open の cdecl() / stdcall() 両対応 & 検出コード
	asm {
		push ebx
		mov  ebx, esp
		push dict
		call migemo_open
		mov  result, eax
		sub  ebx, esp
		mov  IsCDECL, ebx
		add  esp, ebx
		pop  ebx
	};
	if ( IsCDECL > 0 ){ // 旧版向けに用意
		migemo_close_cdecl = (void (__cdecl *)(migemo *))migemo_close;
		migemo_close = migemo_close_fix;

		migemo_query_cdecl = (char *(__cdecl *)(migemo *, const char *))migemo_query;
		migemo_query = migemo_query_fix;

		migemo_release_cdecl = (void (__cdecl *)(migemo *, char *))migemo_release;
		migemo_release = migemo_release_fix;
	}

	return result;
}
#endif
