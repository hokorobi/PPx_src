/*-----------------------------------------------------------------------------
	数式演算処理	(c)TORO
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "TOROWIN.H"
#include "CALC.H"

#define ONPPXDLL		// PPCOMMON.H の DLL 定義指定
#include "PPCOMMON.H"

/* Calc_String(BYTE **param, int *result, int lv0, int para0) の lv0 設定値

	lv	優先順位保存
			LOWORD	２項間の優先順位、大きいほど低順位
				0		最初(括弧内も含む)の呼び出し時
				1	()	[]	->	::	.							左から右へ
				2	!	~	+	-	++	--	&	*	sizeof	new	delete
																右から左へ
				3	.* 	->*										左から右へ
				4	*	/	%									左から右へ
				5	+	-										左から右へ
				6	<<	>>										左から右へ
				7	<	<=	>	>=								左から右へ
				8	==	!=										左から右へ
				9	&											左から右へ
				10	^											左から右へ
				11	|											左から右へ
				12	&&											左から右へ
				13	||											左から右へ
				14	?:											右から左へ
				15	=	*=	/=	%=	+=	-=	&=	^=	|=	<<=	>>=	右から左へ */
#define CALC_BRACKET	B30		// 括弧内の式
#define CALC_USEPARAM0	B29		// para0 が有効
//-----------------------------------------------------------------------------
typedef enum {
	OPENONE, OPEMUL, OPEDIV, OPEPAR, OPEPLUS, OPEMINUS,
	OPESHL, OPESHR, OPELT, OPELE, OPEGT, OPEGE, OPEEQ, OPENE,
	OPEAND, OPEXOR, OPEOR, OPELAND, OPELOR
} OPTYPES;

struct opelevel {
	TCHAR	ope[2];	// 演算子文字列
	int		level;	// 優先順位
	OPTYPES type;	// 演算方法
} levels[] = {			// ２項演算子の一覧
	{ {'<', '<'},	6,	OPESHL},
	{ {'<', '='},	7,	OPELE},
	{ {'>', '>'},	6,	OPESHR},
	{ {'>', '='},	7,	OPEGE},
	{ {'=', '='},	8,	OPEEQ},
	{ {'!', '='},	8,	OPENE},
	{ {'&', '&'},	12,	OPELAND},
	{ {'|', '|'},	13,	OPELOR},
	{ {'*', '\0'},	4,	OPEMUL},
	{ {'/', '\0'},	4,	OPEDIV},
	{ {'%', '\0'},	4,	OPEPAR},
	{ {'+', '\0'},	5,	OPEPLUS},
	{ {'-', '\0'},	5,	OPEMINUS},
	{ {'<', '\0'},	7,	OPELT},
	{ {'>', '\0'},	7,	OPEGT},
	{ {'=', '\0'},	8,	OPEEQ},
	{ {'&', '\0'},	9,	OPEAND},
	{ {'^', '\0'},	10,	OPEXOR},
	{ {'|', '\0'},	11,	OPEOR},
	{ {'\0', '\0'},	0,	OPENONE}
};
/*-----------------------------------------------------------------------------
	２項演算子の優先順位を調べる
-----------------------------------------------------------------------------*/
struct opelevel *GetCalcLevel(const TCHAR **ptr)
{
	struct opelevel *level;
	UTCHAR c0, c1;

	c0 = *(*ptr);
	c1 = *(*ptr + 1);
	for ( level = levels ; level->level ; level++ ){
		if ( c0 != level->ope[0] ) continue;
		if ( level->ope[1] != '\0' ){
			if ( c1 != level->ope[1] ) continue;
			(*ptr) += 2;
		}else{
			(*ptr)++;
		}
		break;
	}
	return level;
}
/*-----------------------------------------------------------------------------
	１項を得る
-----------------------------------------------------------------------------*/
int GetCalcItem(const TCHAR **param, int *result)
{
	const TCHAR *para;
	int status = CALC_NOERROR;

	para = *param;
	SkipSPC(para);
	switch ( *para ){
		case '(':
			para++;
			status = Calc_String(&para, result, CALC_BRACKET, 0);
			break;
		case '!':
			para++;
			status = GetCalcItem(&para, result);
			*result = !(*result);
			break;
		case '~':
			para++;
			status = GetCalcItem(&para, result);
			*result = ~(*result);
			break;
		case '+':
			para++;
			status = GetCalcItem(&para, result);
			break;
		case '-':
			para++;
			status = GetCalcItem(&para, result);
			*result = -(*result);
			break;

		default: {
			const TCHAR *p;
			p = para;
			*result = GetIntNumber(&para);
			if ( para == p ) status = CALC_NUMERROR;
		}
	}
	*param = para;
	return status;
}
/*-----------------------------------------------------------------------------
	電卓機能
-----------------------------------------------------------------------------*/
int Calc_String(const TCHAR **param, int *result, int lv0, int para0)
{
	struct opelevel *lv1, *lv2;
	int n; // 第１項
	int m; // 第２項
	int status = CALC_NOERROR;
	const TCHAR *para, *p;

	para = *param;
	SkipSPC(para);
										// 第１項の取得 -----------------------
	if ( lv0 & CALC_USEPARAM0 ){	// 既に取得済み
		n = para0;
		resetflag(lv0, CALC_USEPARAM0);
	}else{
		status = GetCalcItem(&para, &n);
	}
										// ２項以降 ---------------------------
	while ( status == CALC_NOERROR ){
		SkipSPC(para);
		if ( *para == '\0' ) break;	// １項のみ

		if ( HIWORD(lv0) && (*para == ')') ){
			para++;
			break;
		}
										// 演算子の取得 -----------------------
		p = para;
		lv1 = GetCalcLevel(&para);
		if ( lv1->level == 0 ){
			status = CALC_OPEERROR;
			break;
		}
		if ( LOWORD(lv0) && (LOWORD(lv0) <= lv1->level) ){
			para = p;
			break;
		}
										// 第２項の取得 -----------------------
		status = GetCalcItem(&para, &m);
		if ( status != 0 ) break;
										// 優先順位の判別 ---------------------
		p = para;
		lv2 = GetCalcLevel(&p);
		if ( lv2->level && (lv1->level > lv2->level) ){	// 右の方が優先だった
			status = Calc_String(&para, &m,
					(lv0 & CALC_BRACKET) | lv1->level | CALC_USEPARAM0, m);
			if ( status != 0 ) break;
		}
										// 演算 -------------------------------
		switch (lv1->type){
			case OPEMUL:
				n *= m;
				continue;
			case OPEDIV:
				if ( m ){
					n /= m;
					continue;
				}else{
					status = CALC_DIVERROR;
					break;
				}
			case OPEPAR:
				if ( m ){
					n %= m;
					continue;
				}else{
					status = CALC_DIVERROR;
					break;
				}
			case OPEPLUS:
				n += m;
				continue;
			case OPEMINUS:
				n -= m;
				continue;
			case OPESHL:
				n = n << m;
				continue;
			case OPESHR:
				n = n >> m;
				continue;
			case OPELT:
				n = n < m;
				continue;
			case OPELE:
				n = n <= m;
				continue;
			case OPEGT:
				n = n > m;
				continue;
			case OPEGE:
				n = n >= m;
				continue;
			case OPEEQ:
				n = n == m;
				continue;
			case OPENE:
				n = n != m;
				continue;
			case OPEAND:
				n &= m;
				continue;
			case OPEXOR:
				n ^= m;
				continue;
			case OPEOR:
				n |= m;
				continue;
			case OPELAND:
				n = n && m;
				continue;
			case OPELOR:
				n = n || m;
				continue;
			default:
				status = CALC_OPEERROR;
		}
	}
	*param = para;
	*result = n;
	return status;
}
