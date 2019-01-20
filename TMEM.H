//-----------------------------------------------------------------------------
//	共有メモリ関係
//-----------------------------------------------------------------------------
#define TMEM_H
//------------------------------------- 共有メモリ管理構造体
typedef struct {
	void *ptr;		// 共有メモリへのポインタ
	HANDLE fileH;	// ファイルハンドル
	HANDLE mapH;	// 共有メモリハンドル
} FM_H;


// 戻り値
// 0 既に作成したものを利用
// -1 失敗
#define FMAP_CREATEMAP		B0	// マップを新規作成
#define FMAP_CREATEFILE		B1	// ファイルを新規作成
#define FMAP_FILEOPENERROR	B2	// ファイルを開くことができなかった
extern int FileMappingOn(FM_H *fm,const TCHAR *fname,const TCHAR *mapname,DWORD size,DWORD atr);
extern void FileMappingOff(FM_H *fm);
