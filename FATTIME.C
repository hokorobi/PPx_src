/*-----------------------------------------------------------------------------
	éûçèî‰ärä÷êî	(c)TORO 2006
-----------------------------------------------------------------------------*/
#define ONVFSDLL		// VFS.H ÇÃ DLL export éwíË
#include "WINAPI.H"
#include <windowsx.h>
#include "TOROWIN.H"
#include "FATTIME.H"
#pragma hdrstop

tFUZZYCOMPAREFILETIME FuzzyCompareFileTime = FuzzyCompareFileTime0;

// éûçèî‰ärä÷êî - åµñßî‰är
int USEFASTCALL FuzzyCompareFileTime0(const FILETIME *time1,const FILETIME *time2)
{
	DWORD num1,num2;

	num1 = time1->dwHighDateTime;
	num2 = time2->dwHighDateTime;
	if ( num1 == num2 ){
		num1 = time1->dwLowDateTime;
		num2 = time2->dwLowDateTime;
	}
	if ( num1 < num2 ) return -1;
	return ( num1 == num2 ) ? 0 : 1;
}

// éûçèî‰ärä÷êî - Å}2ïbÇ‹Ç≈ÇãñóeÇ∑ÇÈ
int USEFASTCALL FuzzyCompareFileTime2(const FILETIME *time1,const FILETIME *time2)
{
	DWORD numL,numH;

	numL = time1->dwLowDateTime;
	numH = time1->dwHighDateTime;
	numL -= time2->dwLowDateTime;
	if ( numL > time1->dwLowDateTime ) numH--;
	numH -= time2->dwHighDateTime;
	if ( numH < 0x80000000 ){ // ê≥
		if ( numH ) return 1;
		return (numL <= 2*10000000) ? 0 : 1;
	} // ïâ
	if ( numH != MAX32 ) return -1;
	return (numL >= (MAX32 - 2*10000000 + 1)) ? 0 : -1;
}
