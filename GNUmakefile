#=======================================
# Paper Plane xUI Makefile
#
#set CLANG=C-SJIS
#set LANG=ja_JP
#=======================================
#MinGW	= 1	# Wine/MinGW の自動判別ができないときは、この行を有効にする
OldMinGW	= 0	# 古いMinGW(リソースコンパイルに失敗する)なら1にする
ifndef UseUnicode
UseUnicode	= 1	# 1 なら UNICODE版
endif

#=======================================
ifeq ($(strip $(UseUnicode)),0)
PPXLIB	= PPLIB32
else
PPXLIB	= PPLIB32W
UnicodeCopt	= -DUNICODE
W	= w
endif
#=======================================
ifndef MinGW
 ifneq (,$(findstring /bin:,$(PATH)))
   MinGW	= 0
 else
   MinGW	= 1
 endif
endif
ifeq ($(strip $(MinGW)),1)
# MinGW
Copy	= copy
Ccn	= gcc -DMinGW
cc	= @$(Ccn) $(WarnOpt) $(SimpleWarnOpt) $(UnicodeCopt) -mms-bitfields -finput-charset=CP932 -fexec-charset=CP932 -O2 -Os
Rcn	= brc32 -r
Nul	= nul
Delete	= del
NoDll	= 0
  ifeq ($(strip $(OldMinGW)),1)
    linkdll	= @dllwrap --kill-at -s
  else
    linkdll	= @dllwrap -s
  endif
else
# Wine
Copy	= cp
Ccn	= winegcc -DWINEGCC -DNODLL=1
cc	= @$(Ccn) $(WarnOpt) $(SimpleWarnOpt) $(UnicodeCopt) -mms-bitfields -finput-charset=CP932 -fexec-charset=CP932 -O2 -Os
Rcn	= wrc -DWINEGCC -l0x11 --nostdinc -l0x11 -I. -I/usr/include/wine/windows -I/usr/local/include/wine/windows
Nul	= /dev/null
Delete	= rm
NoDll	= 1
endif

SimpleWarnOpt = -Wno-multichar -Wno-uninitialized -Wno-strict-aliasing
WarnOpt = -W -Wunused-value -Wno-unknown-pragmas -Wno-unused-parameter
# -Wall -Wextra
# -Wstrict-aliasing	キャスト使用
# -Wno-multichar	漢字等の文字定数
# -Wuninitialized	条件によって使われる物も警告
# -Wshadow		スコープが違うところで被っている名前を警告
linkcon	= @$(Ccn) -s
linkexe	= @$(Ccn) -s

.SUFFIXES: .coff .mc .rc .mc.rc .res .res.o .spec .spec.o .idl .tlb .h  .ico .RC .c .C .cpp .CPP

ifeq ($(strip $(MinGW)),0)
.c.o:
	@echo [EXE] $<
	$(cc) -x c -c $<
.cpp.o:
	@echo [EXE] $<
	$(cc) -x c++ -c $<
endif

.C.o:
	@echo [EXE] $<
	$(cc) -x c -c $<
.CPP.o:
	@echo [EXE] $<
	$(cc) -x c++ -c $<

.RC.coff:
	@echo $<
        ifeq ($(strip $(OldMinGW)),1)
	  $(Rcn) -fo$(basename $<).res $(UnicodeCopt) $<
	  @windres -i $(basename $<).res -o $@
        else
	  @windres -DWINDRES -i $(basename $<).RC -o $@
        endif

.RC.res:
	@echo $<
	$(Rcn) $< -o $(basename $<).res

.spec.spec.o:
	winebuild -D_REENTRANT -fPIC --as-cmd "as" --dll -o $@ --main-module $(MODULE) --export $<

#------------------------------------------------------------------------------
ifeq ($(strip $(MinGW)),1)
allFiles:	$(PPXLIB).o	$(PPXLIB).DLL	SETUP.EXE	PPCUST$(W).EXE\
		PPB$(W).EXE	PPC$(W).EXE	PPV$(W).EXE	PPTRAY$(W).EXE\
		PPFFIX$(W).EXE	PPX.HLP
else
allFiles:	$(PPXLIB).o	CHRTYPE.o	FATTIME.o\
		ppcust$(W)	ppc$(W)		ppv$(W)		ppb$(W)
endif

PPXH	= PPX.H PPX.RH PPCOMMON.H CHRTYPE.H TOROWIN.H TKEY.H
COMMON	= GNUmakefile PPCOMMON.H VFS.H
#------------------------------------------------------ PPX.OBJ(code体系切換用)
$(PPXLIB).o:
    ifneq ('','$(wildcard *.o)')
	-@$(Delete) *.o 2> $(Nul)
    endif
    ifneq ('','$(wildcard *.res)')
	-@$(Delete) *.res 2> $(Nul)
    endif
	@$(Copy) $(Nul) $(PPXLIB).o > $(Nul)

#--------------------------------------------------------------------- PPCOMMON
PPDH	= $(PPXH) PPCOMMON.RH PPD_DEF.H PPD_GVAR.C
VFSH	= $(PPXH) VFS.H VFS_STRU.H
ifeq ($(strip $(MinGW)),1)
PPDDOBJ	= CHRTYPE.o FATTIME.o
endif
PPDCOBJ	= TMEM.o	PPCOMMON.o	PPD_MDL.o	PPD_CMDC.o\
	PPD_CMD.o	PPD_MENU.o\
	PPD_WILD.o	PPD_STR.o	PPD_CCMD.o	PPD_CMDL.o\
	PPD_CODE.o	PPD_EDL.o	PPD_EDLC.o	PPD_EDLL.o\
	PPD_EDLM.o	PPD_EDLS.o	PPD_EXEC.o	PPD_EXTL.o\
	PPD_FALT.o	PPD_HIST.o	PPD_SUB.o	PPD_TASK.o\
	PPD_CSCD.o	PPD_CUST.o	PPD_PPE.o	PPD_UI.o\
	PPD_DLG.o\
	CALC.o		PPX_64.o	$(PPDDOBJ)\
	VFS_FCHK.o	VFS_MAIN.o	VFS_ARCD.o	VFS_TREE.o\
	VFS_LAPI.o\
	VFS_FF.o	VFS_PATH.o	VFS_SHN.o\
	VFS_ODLG.o	VFS_FOP.o	VFS_OCF.o\
	VFS_OCD.o	VFS_OFLT.o	VFS_OIMG.o	VFS_ODEL.o\
	VFS_OLNK.o	VFS_OSUB.o\
	VFS_FFP.o	VFS_FMY.o	VFS_LF.o	VFS_FFSH.o\
	VFS_HTTP.o	VFS_FTP.o	VFS_CD.o	VFS_FAT.o\
	PPD_CSTD.o	BMP.o
PPDCPPOBJ	= PPD_SUBP.o	VFS_SHNP.o
PPDOBJ	= $(PPDCOBJ) $(PPDCPPOBJ)

$(PPXLIB).DLL:	GNUmakefile	$(PPDOBJ)	PPCOMMON.coff
	-@PPB$(W) /c *closeppx
	@dlltool --output-def PPCOMMON.def $(PPDOBJ)
	@dlltool --dllname $(PPXLIB).dll --def PPCOMMON.def --output-lib libppcommon.a
	$(linkdll) --def ppcommon.def -mwindows -o $@ $(PPDOBJ) PPCOMMON.coff -limm32 -lole32 -luuid -lstdc++ -loleaut32 -lversion

pplib.dll.so: GNUmakefile	$(PPDOBJ)
#	winebuild -D_REENTRANT -fPIC --as-cmd "as" --dll -o pplib32.spec.o --main-module pplib.dll --export pplib32.spec
	@$(cc) -shared pplib32.spec $(PPDOBJ) -o pplib.dll.so -lgdi32 -ladvapi32 -lkernel32 -lole32 -lshell32
#winegcc -B/tools/winebuild -shared imm32.spec imm.spec.o imm.o version.res -o imm32.dll.so -luser32 -lgdi32 -ladvapi32 -lkernel32 libs/port/libwine_port.a
#winebuild -w --def -o libimm32.def --export ./imm32.spec

ifeq ($(strip $(MinGW)),1)
CHRTYPE.o:	CHRTYPE.C	$(PPXH)
PPCOMMON.o:	PPCOMMON.C	$(PPDH)
PPX_64.o:	PPX_64.C	PPX_64.H
PPD_CMD.o:	PPD_CMD.C	$(PPDH) PPXCMDS.C
PPD_CMDC.o:	PPD_CMDC.C	$(PPDH) PPXCMDS.C
PPD_CCMD.o:	PPD_CCMD.C	$(PPDH)
PPD_CMDL.o:	PPD_CMDL.C	$(PPDH)
PPD_CUST.o:	PPD_CUST.C	$(PPDH) PPC_DISP.H
PPD_CSTD.o:	PPD_CSTD.C	$(PPDH)
PPD_CODE.o:	PPD_CODE.C	$(PPDH)
PPD_EDL.o:	PPD_EDL.C	$(PPDH)
PPD_EXEC.o:	PPD_EXEC.C	$(PPDH)
PPD_EXTL.o:	PPD_EXTL.C	$(PPDH)
PPD_FALT.o:	PPD_FALT.C	$(PPDH)
PPD_HIST.o:	PPD_HIST.C	$(PPDH)
PPD_PPE.o:	PPD_PPE.C	$(PPDH)
PPD_STR.o:	PPD_STR.C	$(PPDH)
PPD_SUB.o:	PPD_SUB.C	$(PPDH)
PPD_TASK.o:	PPD_TASK.C	$(PPDH)
PPD_UI.o:	PPD_UI.C	$(PPDH)
PPD_WILD.o:	PPD_WILD.C	$(PPDH)
VFS_FCHK.o:	VFS_FCHK.C	$(VFSH)
VFS_FDEL.o:	VFS_FDEL.C	$(PPCH)
VFS_FF.o:	VFS_FF.C	$(VFSH)
VFS_MAIN.o:	VFS_MAIN.C	$(VFSH)
VFS_PATH.o:	VFS_PATH.C	$(VFSH)
VFS_SHN.o:	VFS_SHN.C	$(VFSH)
VFS_TREE.o:	VFS_TREE.C	$(VFSH)
PPCOMMON.coff:	PPCOMMON.RC	PPX.RH PPCOMMON.RH resource/DEFCUST.CFG resource/TOOLBAR.BMP
else
$(PPDCOBJ): %.o: %.C
	@echo [DLL] $<
	$(cc) -fPIC -x c -c $<
$(PPDCPPOBJ): %.o: %.CPP
	@echo [DLL] $<
	$(cc) -x c++ -c $<
endif
PPXCMDS.C:	ppxcmds.pl
	-@perl ppxcmds.pl

#-------------------------------------------------------------------------- PPC
PPCH	=	$(PPXH) PPCUI.RH PPX_DRAW.H PPX_CV.H PPX_DOCK.H PPC_STRU.H\
		PPC_GVAR.C PPC_FUNC.H PPC_DISP.H
PPCOBJ	=	PPCUI.o		PPC_MAIN.o	PPC_CELL.o	PPC_COM.o\
		PPC_COM2.o	PPC_WIND.o	PPC_DIRA.o	PPC_DIR.o\
		PPC_DISP.o	PPC_PAIN.o	PPX_DRAW.o\
		PPC_SUB.o	PPC_CINF.o	PPC_CR.o	PPC_FINF.o\
		PPC_DRPC.o	PPC_DROP.o	PPC_DRAG.o	IDATAOBJ.o\
		IDROPSRC.o	IENUMFE.o	RENDRDAT.o\
		PPC_SUBP.o	PPC_SUBT.o	PPC_SUBI.o\
		PPC_FSIZ.o	PPC_ADD.o	PPC_DATR.o	PPC_FOP.o\
		PPC_ARCH.o	PPC_DLG.o	FATTIME.o	PPC_CCMP.o\
		PPC_INIT.o	PPC_WHER.o	PPC_INCS.o\
		TM_MEM.o	CHRTYPE.o	MD5.o\
		sha.o		sha224-256.o\
		PPX_CV.o	PPX_DOCK.o	PPCOMBOB.o	PPCOMBOP.o\
		PPCOMBO.o	PPCOMBOS.o

PPC$(W).EXE:	$(COMMON)	$(PPCOBJ)	$(PPDOBJ) PPCUI.coff
	-@WINCLOSE PaperPlaneCombo
	-@WINCLOSE PaperPlaneCUI$(W)
	$(linkexe) $(PPCOBJ) -o $@ -mwindows libppcommon.a PPCUI.coff -lole32 -luuid -lstdc++ -loleaut32 -limm32

ppc$(W):	$(COMMON)	$(PPCOBJ)	$(PPDOBJ)	PPCUI.res
	@$(Ccn) $(PPCOBJ) $(PPDOBJ) PPCUI.res -L./ -o $@  -lstdc++ -loleaut32 -lwine -luser32 -lgdi32 -limm32 -lole32 -luuid -lkernel32 -lshell32
	@cp ppc$(W).exe ppc$(W)

PPCUI.o:	PPCUI.C		$(PPCH)
PPC_MAIN.o:	PPC_MAIN.C	$(PPCH)
PPC_CCMP.o:	PPC_CCMP.C	$(PPCH)
PPC_CELL.o:	PPC_CELL.C	$(PPCH)
PPC_COM.o:	PPC_COM.C	$(PPCH)
PPC_COM2.o:	PPC_COM2.C	$(PPCH)
PPC_CR.o:	PPC_CR.C	$(PPCH)
PPC_DATR.o:	PPC_DATR.C	$(PPCH)
PPC_DD.o:	PPC_DD.CPP	$(PPCH)
PPC_DIR.o:	PPC_DIR.C	$(PPCH)
PPC_DIRA.o:	PPC_DIRA.C	$(PPCH)
PPC_DISP.o:	PPC_DISP.C	$(PPCH)
PPC_DLG.o:	PPC_DLG.C	$(PPCH)
PPC_DRAG.o:	PPC_DRAG.CPP	$(PPCH)
PPC_DROP.o:	PPC_DROP.CPP	$(PPCH)
PPC_DRPC.o:	PPC_DRPC.CPP	$(PPCH)
PPC_FSIZ.o:	PPC_FSIZ.C	$(PPCH)
PPC_INIT.o:	PPC_INIT.C	$(PPCH)
PPC_PAIN.o:	PPC_PAIN.C	$(PPCH)
PPC_SUB.o:	PPC_SUB.C	$(PPCH)
PPC_SUBI.o:	PPC_SUBI.C	$(PPCH)
PPC_SUBP.o:	PPC_SUBP.CPP	$(PPCH)
PPC_SUBT.o:	PPC_SUBT.C	$(PPCH)
PPC_WIND.o:	PPC_WIND.C	$(PPCH)
IDATAOBJ.o:	IDATAOBJ.CPP	$(PPCH)
IDROPSRC.o:	IDROPSRC.CPP	$(PPCH)
IENUMFE.o:	IENUMFE.CPP	$(PPCH)
RENDRDAT.o:	RENDRDAT.CPP	$(PPCH)
TM_MEM.o:	TM_MEM.C	TM_MEM.H
PPX_DRAW.o:	PPX_DRAW.CPP	$(PPCH)
PPX_CV.o:	PPX_CV.C PPX.H VFS.H PPX_DRAW.H PPX_CV.H
PPCUI.coff:	PPXVER.H PPCUI.RC PPCUI.RH PPX.RH
PPC_DISP.H:	ppcdisp.pl
	-@perl ppcdisp.pl

#-------------------------------------------------------------------------- PPV
PPVH	=	$(PPXH) PPVUI.RH PPV_STRU.H PPV_FUNC.H
PPVOBJ	=	PPVUI.o		PPV_COM.o	PPV_IMG.o	PPV_OPEN.o\
		PPV_PAIN.o	PPV_PAIT.o	PPV_TEXT.o	CHRTYPE.o\
		PPV_INIT.o	PPV_PRIN.o	PPV_DOAS.o	PPV_CP.o\
		PPV_CLIP.o	PPV_PSTE.o	PPV_SUB.o\
		TM_MEM.o	PPX_CV.o	PPV_SUBP.o

PPV$(W).EXE:	$(COMMON)	$(PPVOBJ)	PPVUI.coff PPX_64.o
	-@WINCLOSE PaperPlaneVUI$(W)
	$(linkexe) $(PPVOBJ) PPX_64.o -o $@ -mwindows libppcommon.a PPVUI.coff -luuid -lstdc++ -lole32

ppv$(W):	$(COMMON)	$(PPVOBJ)	$(PPDOBJ)	PPVUI.res
	@$(Ccn) $(PPVOBJ) $(PPDOBJ) FATTIME.o PPVUI.res -L./ -o $@  -lstdc++ -loleaut32 -lwine -luser32 -lgdi32 -imm32 -lole2 -lole32 -luuid -lkernel32 -lshell32
	@cp ppv$(W).exe ppv$(W)

PPVUI.o:	PPVUI.C		$(PPXH) $(PPVH) PPV_GVAR.C
PPV_COM.o:	PPV_COM.C	$(PPXH) $(PPVH)
PPV_DOAS.o:	PPV_DOAS.C	$(PPXH) $(PPVH)
PPV_IMG.o:	PPV_IMG.C	$(PPXH) $(PPVH)
PPV_OPEN.o:	PPV_OPEN.C	$(PPXH) $(PPVH)
PPV_PAIN.o:	PPV_PAIN.C	$(PPXH) $(PPVH)
PPV_PRIN.o:	PPV_PRIN.C	$(PPXH) $(PPVH)
PPV_TEXT.o:	PPV_TEXT.C	$(PPXH) $(PPVH)
PPV_INIT.o:	PPV_INIT.C	$(PPXH) $(PPVH)
PPVUI.coff:	PPVUI.RC PPVUI.RH PPX.RH

#-------------------------------------------------------------------------- PPB
PPBOBJ	=	PPBUI.o		PPB_SUB.o	TCONSOLE.o	TCINPUT.o\
		CHRTYPE.o
PPB$(W).EXE:	$(COMMON)	$(PPBOBJ)	PPBUI.coff
	$(linkcon) $(PPBOBJ) -o $@ -mconsole libppcommon.a PPBUI.coff -lole32

ppb$(W):	$(COMMON)	$(PPBOBJ)	$(PPDOBJ)
	@$(Ccn) $(PPBOBJ) $(PPDOBJ) FATTIME.o -L./ -o $@ -lstdc++ -loleaut32 -lwine -luser32 -lgdi32 -imm32 -lole2 -lole32 -luuid -lkernel32 -lshell32 -lcurses
	@cp ppb$(W).exe ppb$(W)

PPBUI.o:	PPBUI.C		$(PPXH)		PPB_SUB.C
PPBUI.coff:	PPBUI.RC	PPX.RH

#----------------------------------------------------------------------- PPCUST
PPCUSTOBJ =	PPCUST.o	PPCST_G.o	PPCST_FI.o	PPCST_GR.o\
		PPCST_TB.o	PPCST_GC.o	PPCST_AO.o	PPCST_ET.o\
		PPCST_ED.o	CHRTYPE.o
PPCUST$(W).EXE:	$(COMMON)	$(PPCUSTOBJ)	PPCUST.coff
	$(linkcon) $(PPCUSTOBJ) -o $@ -mwindows -mconsole libppcommon.a PPCUST.coff -lole32 -lcomctl32

ppcust$(W):	$(COMMON)	$(PPCUSTOBJ)	$(PPDOBJ)	PPCUST.res
	@$(Ccn) $(PPCUSTOBJ) $(PPDOBJ) FATTIME.o PPCUST.res -L./ -o $@ -lstdc++ -loleaut32 -lwine -luser32 -lgdi32 -imm32 -lole2 -lole32 -luuid -lkernel32 -lshell32 -lcomdlg32 -lcomctl32
	@cp ppcust$(W).exe ppcust$(W)

PPCUST.o:	PPCUST.C	$(PPXH)
PPCUST.coff:	PPCUST.RC PPX.RH resource/CUSTLIST.CFG resource/CUSTKEY.CFG

#----------------------------------------------------------------------- PPTRAY
PPTRAY$(W).EXE:	$(COMMON)	PPTRAY.coff	PPTRAY.o
	-@WINCLOSE PPtray$(W)
	$(linkexe) PPTRAY.o -o $@ -mwindows libppcommon.a PPTRAY.coff -lole32
PPTRAY.o:	PPTRAY.C	$(PPXH)
PPTRAY.coff:	PPTRAY.RC
#----------------------------------------------------------------------- PPFFIX
PPFFIX$(W).EXE:	$(COMMON)	PPFFIX.coff	PPFFIX.o
	$(linkexe) PPFFIX.o -o $@ -mwindows libppcommon.a PPFFIX.coff
ppffix$(W):	$(COMMON)	ppffix.o
	@$(Ccn) ppffix.o -o $@ -L./ -lpplib.dll

ppffix.o :	ppffix.C	$(PPXH)
ppffix.coff :	ppffix.RC	PPX.RH
#----------------------------------------------------------------------- SETUP
PPSETUPOBJ =	PPSETUP.o	PPSET_S.o	PPSET_DG.o	PPSET_IN.o\
		PPSET_UN.o	CHRTYPE.o
SETUP.EXE:	$(COMMON)	$(PPSETUPOBJ)	PPSETUP.coff
	$(linkexe) $(PPSETUPOBJ) -o $@ -mwindows libppcommon.a PPSETUP.coff -lole32 -luuid -lcomctl32

PPSETUP.o :	PPSETUP.C	$(PPXH)
PPSETUP.coff :	PPSETUP.RC	PPX.RH
#--------------------------------------------------------------------- PPX.HLP
PPX.HLP:	XHELP.TXT
	-@perl ppxhelp.pl
	-@$(Delete) ppxtemp.rtf
