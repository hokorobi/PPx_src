/*-----------------------------------------------------------------------------
		ctype.h ŒİŠ·ƒwƒbƒ_	Version 1.12					(c)TORO 1995-2011

	isXXX Œn‚Í^‚Ì‚ª‚OˆÈŠO‚Å‚ ‚Á‚Ä‚P‚Å‚Í‚È‚¢
-----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

#define T_IS_KNJ 2			/* Š¿š     */
#define T_IS_DIG 4			/* ‚P‚Oi” */
#define T_IS_HEX 8			/* ‚P‚Ui” */
#define T_IS_UPP 16			/* ‰p‘å•¶š */
#define T_IS_LOW 32			/* ‰p¬•¶š */
#define T_IS_CTL 64			/* §Œä•¶š */

extern char T_CHRTYPE[256];

										/* ctype.h ŒİŠ·ŠÖ”ŒQ */
/* ANSI */
#define IsalnumA(c) (T_CHRTYPE[(unsigned char)(c)] & (T_IS_DIG | T_IS_UPP | T_IS_LOW))
#define IsalphaA(c) (T_CHRTYPE[(unsigned char)(c)] & (T_IS_UPP | T_IS_LOW))
#define IsasciiA(c) ((unsigned char)(c) < 128)
#define IscntrlA(c) (T_CHRTYPE[(unsigned char)(c)] & T_IS_CTL)
#define IsdigitA(c) (T_CHRTYPE[(unsigned char)(c)] & T_IS_DIG)
#define IsgraphA(c) ((unsigned char)(c) >= 0x21 && (unsigned char)(c) <= 0x7e)
#define IslowerA(c) (T_CHRTYPE[(unsigned char)(c)] & T_IS_LOW)
#define IsprintA(c) ((unsigned char)(c) >= 0x20 && (unsigned char)(c) <= 0x7e)
#define IspunctA(c) (T_CHRTYPE[(unsigned char)(c)] & T_IS_PUN)
#define IsupperA(c) (T_CHRTYPE[(unsigned char)(c)] & T_IS_UPP)
#define IsxdigitA(c) (T_CHRTYPE[(unsigned char)(c)] & (T_IS_DIG | T_IS_HEX))

/* UNICODE */
#define IsalnumW(c) (((c) > 'z') ? 0 : (T_CHRTYPE[(unsigned char)(c)] & (T_IS_DIG | T_IS_UPP | T_IS_LOW)))
#define IsalphaW(c) (((c) > 'z') ? 0 : (T_CHRTYPE[(unsigned char)(c)] & (T_IS_UPP | T_IS_LOW)))
#define IsasciiW(c) ((c) < 128)
#define IscntrlW(c) (((c) > 0x80) ? 0 : (T_CHRTYPE[(unsigned char)(c)] & T_IS_CTL))
#define IsdigitW(c) ((c) >= '0' && (c) <= '9')
#define IsgraphW(c) ((c) >= 0x21 && (c) <= 0x7e)
#define IslowerW(c) ((c) >= 'a' && (c) <= 'z')
#define IsprintW(c) ((c) >= 0x20 && (c) <= 0x7e)
#define IspunctW(c) (T_CHRTYPE[(unsigned char)(c)] & T_IS_PUN)
#define IsupperW(c) ((c) >= 'A' && (c) <= 'Z')
#define IsxdigitW(c) (((c) > 'z') ? 0 : (T_CHRTYPE[(unsigned char)(c)] & (T_IS_DIG | T_IS_HEX)))

#ifdef UNICODE
	#define Isalnum IsalnumW
	#define Isalpha IsalphaW
	#define Isascii IsasciiW
	#define Iscntrl IscntrlW
	#define Isdigit IsdigitW
	#define Isgraph IsgraphW
	#define Islower IslowerW
	#define Isprint IsprintW
	#define Ispunct IspunctW
	#define Isupper IsupperW
	#define Isxdigit IsxdigitW
#else
	#define Isalnum IsalnumA
	#define Isalpha IsalphaA
	#define Isascii IsasciiA
	#define Iscntrl IscntrlA
	#define Isdigit IsdigitA
	#define Isgraph IsgraphA
	#define Islower IslowerA
	#define Isprint IsprintA
	#define Ispunct IspunctA
	#define Isupper IsupperA
	#define Isxdigit IsxdigitA
#endif
										/* ’Ç‰Á‚³‚ê‚½ŠÖ” */
/* Š¿š‚Ì‚PƒoƒCƒg–Ú‚©H(ANSIê—p) */
#define IskanjiA(c)	(T_CHRTYPE[(unsigned char)(c)] & T_IS_KNJ)
/* •¶š‚ÌƒoƒCƒg”‚ğ“¾‚é(NULL=0, ANSIê—p) */
#define ChrlenA(c)	(T_CHRTYPE[(unsigned char)(c)] & 3)

#define IsmultiA IskanjiA
#define IsmultiW(c) (((c) >= 0xd800) && ((c) <= 0xdfff))

#ifdef UNICODE
	#define Ismulti IsmultiW
#else
	#define Ismulti IsmultiA
	#define Iskanji IskanjiA
	#define Chrlen ChrlenA
#endif

#ifdef __cplusplus
}
#endif // __cplusplus
