/*-----------------------------------------------------------------------------
	数式演算処理

	param	文字列へのポインタ、正しく解釈できたところまで移動する
	result	結果
	lv0		内部で使用(再帰処理)。0 に指定する
	para0	内部で使用(第１項が取得済のときに使う)。0 に指定する
-----------------------------------------------------------------------------*/
enum {					// CalcString の戻り値
	CALC_NOERROR,	// 演算成功
	CALC_NUMERROR,	// 数値エラー
	CALC_PREERROR,	// １項演算子エラー
	CALC_OPEERROR,	// ２項演算子エラー
	CALC_BRAERROR,	// 括弧エラー
	CALC_DIVERROR	// 0割エラー
};

extern int Calc_String(const TCHAR **param,int *result,int lv0,int para0);
#define CalcString(param,result) Calc_String(param,result,0,0)
