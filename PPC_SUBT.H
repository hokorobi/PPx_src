/*-----------------------------------------------------------------------------
	Paper Plane cUI										SubThread
-----------------------------------------------------------------------------*/

#ifdef DEBUG // BC5.0x の場合は CodeGuard のバグのためアイコン表示しない
	#ifdef __BORLANDC__
		#if __BORLANDC__ <= 0x509
//			#define DEBUGICON
		#endif
	#endif
#endif

typedef struct {
	RESTARTTHREADSTRUCT threeadinfo;
	OverlayClassTable *LayPtr;
} SubThreadData;

extern void GetInfoIcon(PPC_APPINFO *cinfo, HICON *hInfoIcon, SubThreadData *threaddata);
extern void USEFASTCALL GetCellIcon(PPC_APPINFO *cinfo, SubThreadData *threeaddata);
extern void FreeOverlayClass(OverlayClassTable *oc);
