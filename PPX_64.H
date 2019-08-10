/*-----------------------------------------------------------------------------
	64 bit 整数の操作用関数
-----------------------------------------------------------------------------*/
#ifdef __BORLANDC__
	#define USETASM32	1	// TASM32 が使用できない場合は、0 にする。
#else
	#define USETASM32	0
#endif

#define DWORDPP		4294967296.	// 0x1 0000 0000 (DWORD で扱えない最少数)
#define DWORDTEN	1000000000	// DWORD で桁の途中で溢れることなく扱える
									// 10 数値 + 1

extern void DDwordToDten(DWORD srcl, DWORD srch, DWORD *destl, DWORD *desth);
extern void DDmul(DWORD src1, DWORD src2, DWORD *destl, DWORD *desth);
extern BOOL InitFloat(void);
