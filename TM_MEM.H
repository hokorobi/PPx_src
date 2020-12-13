#ifdef __cplusplus
extern "C" {
#endif

#define TM_step 0x10000					// 確保単位

typedef struct {							// メモリ管理構造体
	void *p;	// ポインタ
	ULONG_PTR s;	// 大きさ
	HGLOBAL h;	// ハンドル
} TM_struct;

typedef struct {							// 文字列群管理構造体
	TM_struct tm;	// メモリ管理用
	LONG_PTR p;	// 次回の書き込み位置(offset)
} TMS_struct;

extern void TM_off(TM_struct *TM);
extern void TM_kill(TM_struct *TM);
extern BOOL TM_check(TM_struct *TM, ULONG_PTR size);

#define TMS_off(TMS)	(TM_off(TMS.tm));
#define TMS_kill(TMS)	(TM_kill(TMS.tm));
extern void TMS_reset(TMS_struct *TMS);
extern char *TMS_setA(TMS_struct *TMS, const char *string);
#ifdef UNICODE
extern char *TMS_setW(TMS_struct *TMS, const WCHAR *string);
#define TMS_set TMS_setW
#else
#define TMS_set TMS_setA
#endif

#ifdef __cplusplus
}
#endif
