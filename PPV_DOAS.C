/*-----------------------------------------------------------------------------
	Paper Plane vUI											` Text(OASYS) `
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#include "PPV_STRU.H"
#include "PPV_FUNC.H"
#pragma hdrstop

const unsigned char OasysCode[4][0xbd * 2 + 1] = {
/* ec40 */
"beN??????„¢‡š‡‡‘??????ƒ„«ª©¨ ™¤¢ž??????mmcmkmc2m2"
"k2c3m3mgkgccdl‚ŒklmsusnspsKBMBGBðHPñHzmb??ml??‡b‡_‡a‡`km‡ckg??"
"‡d‡e‡f‡j‡k‡l‡i‡hÎÝmbHz‡napbrmskohi??????‡ƒ‡Š‡„‡„§§‡‚c)????R)??"
"??????????????0.1.2.3.4.5.6.7.8.9.??1)2)3)4)5)6)7)8)9)1011121314"
"151617181920??‡@‡A‡B‡C‡D‡E‡F‡G‡H‡I‡J‡K‡L‡M‡N‡O‡P‡Q‡R‡S21‡T‡U‡V‡W"
"‡X‡Y‡Z‡[‡\‡]?????? 1 2 3 4 5 6 7 8 91022232425262728293031",
/* ed40*/
"a)b)c)d)e)f)g)h)i)j)k)l)m)n)o)p)q)r)s)t)u)v)w)x)y)z)????????????"
"????????³Þ??????ŽŠŠé‡Œ‹¦–¼˜J‡‹ŽÐŠÄŽ©Ž‘à‡Š“ÁŠwÕŒÄ??????–éŠéˆã??"
"‹¦–¼@˜JŠw‡‹‡ŠŽÐŠÄŽ‘à????????„¡„¨„¢„¦„¤„¦„£„¨„¢„¥„£„§„¡„§„¤„¥??"
"????????????????????????????????????????????????????????????????"
"??????????????????????????????????????????????????????????????„ª"
"„¬„­„±„²„¶„±„»„±„±„±„±„±„·„²„¼„²„²„²„²„²„¹„´„´„´„´„´„¶„·„¹",
/* ee40 */
"„¡„¢„¤„£„Ÿ„ „¨„¥„¦„§„©„Ÿ„Ÿ„ „Ÿ„ „Ÿ„¨„½„¥„º„¦„»„§„¼„©„¾„°„°„³„´„´"
"„¬„­„¯„®„ª„«„³„°„±„²„´„ª„ª„«„ª„«„ª„¸„¸„µ„µ„¶„¶„·„·„¹„¹„²„²„³„´??"
"„´„¡„¢„¤„£„Ÿ„ „¨„¥„¦„§„©„Ÿ„Ÿ„ „Ÿ„ „Ÿ„¨„½„¥„º„¦„»„§„¼„©„¾„±„±„´??"
"????????????????????????????????????????????????????????????????"
"??????????????????????????????????????????????????????????????„«"
"„¯„®„³„°„¸„³„½„³„³„³„³„³„µ„°„º„°„°„°„°„°„¾„´„´„´„´„´„¸„µ„´",
/* ef40 */
"RBCDPQ[\]`abcdefghijklmnopqrstuv"
"wxyz‚Ÿ‚¡‚£‚¥‚§‚Á‚á‚ã‚å‚ìƒ@ƒBƒDƒFƒHƒbƒƒƒ…ƒ‡ƒŽƒ•ƒ–‚©‚¯????????"
"????[][]cdcc[][][][][]¨©ª«inin/2/4‹­Á------„ª[][][][]??"
"ƒ EE‹æ‰æ•”¡Ìƒ‚Æƒ????‘å‚P•W¬Ìžc‰¡‹Ï‹Ï’’žžƒS–¾‹Ï‚Q"
"‚RŽÎs”½”½üüdŽ~‚éƒ‹‰p‚S‚TŠgŠg‹Ï‹³ŠgFŠgŠgŠg‘‘‚Ó‚Ó“]–ÑŠÛ„H"
"[]uv•\•\•‘±|x‡•[  [ [] õõ??????üü><~•M–ÑAL‘¾‘¾ƒ|ƒS" };

void Bufwrite(BYTE **buf,char *data,int *cnt)
{
	WORD size;

	size = (WORD)strlen(data);
	if ( size <= *cnt ){
		strcpy((char *)*buf,data);
		*buf += size;
		*cnt -= size;
	}
}
/*-------------------------------------
	Oasys -> HTML
-------------------------------------*/
BYTE *VD_oasys_mdt(MAKETEXTINFO *mti,VT_TABLE *tbl)
{
	BYTE *text;		// “Ç‚Ýž‚ÝˆÊ’u
	int cnt;
	WORD code,backup = 0;
	BYTE *dest = mti->destbuf, *dmax = mti->destbuf + mti->destsize;
	BYTE *srcmax = mti->srcmax;

	text = tbl->ptr;
	tbl->type = 3;

	*dest++ = 1;
	while( text < srcmax ){
		cnt = dmax - dest;
		if ( cnt <= 4 ) break;
		if ( backup == 0 ){
			code = *text++;
		}else{
			code = backup;
			backup = 0;
		}
		if ( code == 0xd ){
			if ( *text == 0xa ) text++;
			break;
		}
		if ( code == 0x1a ) break;
		if ( code < ' ' ) continue;
		if ( ((code >= 0x80)&&(code < 0xa0)) ||
			 ((code >= 0xe0)&&(code < 0x100)) ){
			 if ( (*text == 0xf0) && (*(text+1) < ' ') ) text += 4;
			 code = (WORD)((code << 8) + *text++);
		}
		switch(code){
			case 0xefa0:	/* LF */
				if ( (code == 0xefa0) && convert ){
					*dest++ = 0;
					*dest++ = VCODE_RETURN;
					*dest++ = 1;
					*dest++ = VCODE_ASCII;
					continue;
				}
				*dest++ = 0;
				*dest++ = VCODE_TAB;
				*dest++ = VCODE_ASCII;
				continue;
			case 0xefaf:	/* •\§Œä? */
				text += 3;
				continue;
			case 0xefc0:	/* §Œä */
				text++;
				break;
			case 0xf004:
				text += 2;
				continue;
			case 0xfcc7:
				Bufwrite(&dest," ",&cnt);
				continue;
			case 0xefa2:
			case 0xefa3:
				if ( convert ){
					text += 2;
					continue;
				}
			default:
				if ( (code >= 0xec40) && (code < 0xefbd )){
					int i,j;

					i = (((unsigned)code & 0xff) - 0x40) * 2;
					if ( i < (0xbd * 2) ){
						j = (code - 0xec00) >> 8;
						code = (WORD)((OasysCode[j][i] << 8) + (OasysCode[j][i+1] ));
					}
				}
				if ( code >= 0xf000 ) code = 0x8140;	/* •s–¾ƒR[ƒh‚Í‹ó”’‚É*/
				if ( code > 0xff ){
					char tmp[3];
					tmp[0] = (BYTE)(code >> 8);
					tmp[1] = (BYTE)(code & 0xff);
					tmp[2] = 0;
					Bufwrite(&dest,tmp,&cnt);
				}else{
					char tmp[2];
					tmp[0] = (char)code;
					tmp[1] = 0;
					Bufwrite(&dest,tmp,&cnt);
				}
		}
	}
	cnt = (WORD)(dest - mti->destbuf);
	if ( cnt > VOi->width ) VOi->width = cnt;

	*dest++ = 0;
	*dest = 0;

	if ( mti->writetbl ){
		(tbl + 1)->Fclr	= tbl->Fclr;
		(tbl + 1)->Bclr	= tbl->Bclr;
		tbl++;
		tbl->ptr	= text;
		tbl->attrs	= 0;
		tbl->line	= (tbl - 1)->line + ((cnt < VOi->width) ? 1 : 0);
	}
	return text;
}
