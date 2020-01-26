/*=============================================================================
	動的メモリ管理												(c)TORO 1997

--------------------------------------- 汎用版
・ポインタを参照・代入する時に
	BOOL	TM_check(TM_struct *TM, DWORD size)
		TM_struct *TM		メモリ管理構造体（通常は p のみでいいはず）
		DWORD size			必要なメモリ

・一時的に使わない時
	void	TM_off(TM_struct *TM)
		TM_struct *TM		メモリ管理構造体（通常は p のみでいいはず）

・必要がなくなった時
	void	TM_kill(TM_struct *TM)
		TM_struct *TM		メモリ管理構造体（通常は p のみでいいはず）

※ TM_struct を確保する時は「TM_struct	test = { NULL, 0, NULL };」とする

--------------------------------------- 文字列専用
・空にする
	void	TMS_reset(TMS_struct *TMS)
		TMS_struct *TMS		メモリ管理構造体

・保存
	char	*TMS_set(TMS_struct *TMS, char *string)
		TMS_struct *TMS		メモリ管理構造体
		char *string		保存する文字列

・一時的に使わない時
	void	TMS_off(TMS_struct *TMS)
		TMS_struct *TMS		メモリ管理構造体

・必要がなくなった時
	void	TMS_kill(TMS_struct *TMS)
		TMS_struct *TMS		メモリ管理構造体

※ TMS_struct を確保する時は「TMS_struct test = {{NULL, 0, NULL}, 0};」とする

=============================================================================*/
#include "WINAPI.H"
#include "PPX.H"
#include "TM_MEM.H"
//-----------------------------------------------------------------------------
void TM_off(TM_struct *TM)
{
	if (TM->p != NULL){
		if ( IsTrue(GlobalUnlock(TM->h)) ){
			PPErrorBox(NULL, T("TM_off"), PPERROR_GETLASTERROR);
		}
		TM->p = NULL;
	}
}

//-----------------------------------------------------------------------------
void TM_kill(TM_struct *TM)
{
	if ( TM->s != 0 ){
		TM_off(TM);
		if (GlobalFree(TM->h) != NULL){
			PPErrorBox(NULL, T("TM_kill"), PPERROR_GETLASTERROR);
		}
		TM->s = 0;
	}
}
//-----------------------------------------------------------------------------
BOOL TM_check(TM_struct *TM, DWORD size)
{
	if ( TM->s == 0 ){						// まったく確保していない場合
		TM->h = GlobalAlloc(GMEM_MOVEABLE, TM_step);
		if (TM->h == NULL){
			PPErrorBox(NULL, T("Alloc error"), PPERROR_GETLASTERROR);
			return FALSE;
		}else{
			TM->s = TM_step;
		}
		TM->p = NULL;
	}
	if ( TM->s < size ){					// 確保済みのメモリで足りない場合
		HGLOBAL hTmp;
		DWORD addsize;

		TM_off(TM);
		#define ADDPAR 8
		#define SMALLADD 32

		// 増量サイズは、要求サイズの1/addpar、ただしTM_step 以上
		addsize = size < (TM_step * SMALLADD) ? TM_step : size / ADDPAR;
		size = (size + addsize - 1) & ~(TM_step - 1);
		hTmp = GlobalReAlloc(TM->h, size, GMEM_MOVEABLE);
		if ( hTmp == NULL ){
			PPErrorBox(NULL, T("Alloc error"), PPERROR_GETLASTERROR);
			#pragma warning(suppress:6001) // 失敗時なので生きている
			TM->p = GlobalLock(TM->h);
			return FALSE;
		}else{
			TM->h = hTmp;
			TM->s = size;
		}
	}
	if (TM->p == NULL) TM->p = GlobalLock(TM->h);
	return TRUE;
}
//-----------------------------------------------------------------------------
void TMS_reset(TMS_struct *TMS)
{
	TMS->p = 0;
	TM_off(&TMS->tm);
}

char *TMS_setA(TMS_struct *TMS, const char *string)
{
	SIZE32_T size;

	size = strlen32(string) + 1;
	TM_check(&TMS->tm, (TMS->p) + size);
	memcpy((char *)TMS->tm.p + TMS->p, string, size);
	TMS->p += size;
	return 0;
}
#ifdef UNICODE
char *TMS_setW(TMS_struct *TMS, const WCHAR *string)
{
	SIZE32_T size;

	size = (strlenW32(string) + 1) * sizeof(WCHAR);
	TM_check(&TMS->tm, (TMS->p) + size);
	memcpy((char *)TMS->tm.p + TMS->p, string, size);
	TMS->p += size;
	return 0;
}
#endif
