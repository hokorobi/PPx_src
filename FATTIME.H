/*-----------------------------------------------------------------------------
	éûçèî‰ärä÷êî	(c)TORO 2006
-----------------------------------------------------------------------------*/
typedef int (USEFASTCALL * tFUZZYCOMPAREFILETIME)(const FILETIME *time1,const FILETIME *time2);
extern tFUZZYCOMPAREFILETIME FuzzyCompareFileTime;
extern int USEFASTCALL FuzzyCompareFileTime0(const FILETIME *time1,const FILETIME *time2);
extern int USEFASTCALL FuzzyCompareFileTime2(const FILETIME *time1,const FILETIME *time2);
