/*-----------------------------------------------------------------------------
	64 bit 整数の操作用関数
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX_64.H"
#pragma  hdrstop

#ifndef _WIN64
/*-----------------------------------------------
	DOUBLE DWORD を 1G 未満で２つの DWORD に分ける
-----------------------------------------------*/
void DDwordToDten(DWORD srcl,DWORD srch,DWORD *destl,DWORD *desth)
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
		mov edx,srch
		mov eax,srcl
		mov ebx,DWORDTEN
		div ebx
		mov ebx,desth
		mov [ebx],eax
		mov ebx,destl
		mov [ebx],edx
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
void DDmul(DWORD src1,DWORD src2,DWORD *destl,DWORD *desth)
{
#ifdef _WIN64
	unsigned long long int a;

	a = (unsigned long long int)src1 * (unsigned long long int)src2;
	*desth = (DWORD)(a >> 32);
	*destl = (DWORD)a;
#else
 #if USETASM32
	asm {
		mov eax,src1
		mul src2
		mov ecx,destl
		mov [ecx],eax
		mov eax,desth
		mov [eax],edx
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
migemo *(_cdecl * migemo_open)(const char *dict) = NULL;
void (_cdecl * migemo_close)(migemo *object);
char *(_cdecl * migemo_query)(migemo *object,const char *query);
void (_cdecl * migemo_release)(migemo *object,char *string);

void (WINAPI * migemo_close_std)(migemo *object);
char *(WINAPI * migemo_query_std)(migemo *object,const char *query);
void (WINAPI * migemo_release_std)(migemo *object,char *string);

extern migemo * migemo_open_fix(const char *dict);

void _cdecl migemo_close_fix(migemo *object)
{
	migemo_close_std(object);
}
char * _cdecl migemo_query_fix(migemo *object,const char *query)
{
	return migemo_query_std(object,query);
}
void _cdecl migemo_release_fix(migemo *object,char *string)
{
	migemo_release_std(object,string);
}

migemo * migemo_open_fix(const char *dict)
{
	migemo *result;
	DWORD IsCDECL;

	// migemo_open の cdecl / stdcall 両対応 & 検出コード
	asm {
		push ebx
		mov  ebx,esp
		push dict
		call migemo_open
		mov  result,eax
		sub  ebx,esp
		mov  IsCDECL,ebx
		add  esp,ebx
		pop  ebx
	};
	if ( IsCDECL == 0 ){
		migemo_close_std = (void (WINAPI *)(migemo *))migemo_close;
		migemo_close = migemo_close_fix;

		migemo_query_std = (char *(WINAPI *)(migemo *,const char *))migemo_query;
		migemo_query = migemo_query_fix;

		migemo_release_std = (void (WINAPI *)(migemo *,char *))migemo_release;
		migemo_release = migemo_release_fix;
	}

	return result;
}
#endif
