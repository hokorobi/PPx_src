@echo off
rem XVER: 123 or 123p1
set XVER=171
set RETAIL=1
set DIR64=PPx64
rem *** set value ***
set readme=PPX.TXT
if %RETAIL%==0 set readme=PPX%XVER%.TXT
set help=
if %RETAIL%==1 set help=PPX.CNT PPX.HLP
set etcfile=
if %RETAIL%==1 set etcfile=PPXF*.TXT
set ppsetup=
if %RETAIL%==1 set ppsetup=SETUP.EXE
set ppc=
set ppv=
set ppb=
set ppcust=
set pptray=
set ppffix=
set ppcommon=

rem *** main ***
if "%1"=="x64" goto :x64main
if "%1"=="arm64" goto :arm64main
WINCLOSE PaperPlaneCUI
WINCLOSE PaperPlaneCUIW
WINCLOSE PaperPlaneVUI
WINCLOSE PaperPlaneVUIW
pptray /c *closeppx
pptrayw /c *closeppx
perl ppxver.pl %XVER%
del /q *.zip 2> NUL
del /q *.map 2> NUL
del /q %DIR64%\*.* 2> NUL
del /q %DIR64%ARM\*.* 2> NUL
set arcbit=32
set arcpath=
set tail=W
set arcx=ppw
call :makes
set tail=
set arcx=ppx
call :makes

rem *** Source Archive ***
if %RETAIL%==0 goto :x64call
del PPXDEF.CFG 2> NUL
for %%i in (*) do CT %readme% %%i
for /R %%i in (*.sln) do CT %readme% %%i
for /R %%i in (*.vc*) do CT %readme% %%i
for %%i in (resource\*) do CT %readme% %%i
rem 2015/3Œ»ÝAunlha32 ‚Åƒhƒ‰ƒCƒu–¼Žw’è‚µ‚Ä‚¢‚È‚¢‘ŒÉ–¼‚ª‚ ‚é‚ÆˆÙíI—¹‚·‚é
ppb /c %%u/UNLHA32.DLL,a %~dp0\PPX%XVER%SR.LZH -z -n -r2x MAKE*. GNUmake*. *.BAT *.C *.CPP *.H *.HPJ index.html *.pl *.RC *.RH X*.TXT PPX_TEC.TXT PPX_REC.TXT *.XLS resource\* VS2008\PPx.sln VS2008\PPb\*.vcproj VS2008\PPc\*.vcproj VS2008\PPcommon\*.vcproj VS2008\PPcust\*.vcproj VS2008\PPffix\*.vcproj VS2008\PPtray\*.vcproj VS2008\PPv\*.vcproj VS2008\SETUP\*.vcproj VS2015\PPx.sln VS2015\PPb\*.vcxproj VS2015\PPc\*.vcxproj VS2015\PPcommon\*.vcxproj VS2015\PPcust\*.vcxproj VS2015\PPffix\*.vcxproj VS2015\PPtray\*.vcxproj VS2015\PPv\*.vcxproj VS2015\SETUP\*.vcxproj
CT %readme% PPX%XVER%SR.LZH
ppb /c %%u/7-ZIP32.DLL,a -tzip -hide -mx=7 -r ppx%XVER%s.zip PPXSRC.TXT PPX%XVER%SR.LZH
tfilesign s ppx%XVER%s.zip ppx%XVER%s.zip
CT %readme% ppx%XVER%s.zip
del /q PPX%XVER%SR.LZH 2> NUL
ppb /c %%u/7-ZIP32.DLL,a -tzip -hide -mx=7 ppxhelp.zip %help%
tfilesign s ppxhelp.zip ppxhelp.zip
CT %readme% ppxhelp.zip

rem *** x64/arm64 ***
:x64call
md %DIR64%
md %DIR64%\resource
copy *.* %DIR64% > NUL
del %DIR64%\*.map > NUL
copy resource\*.* %DIR64%\resource > NUL
cmd /c archive.bat x64 %DIR64% %readme%

md %DIR64%ARM
md %DIR64%ARM\resource
copy *.* %DIR64%ARM > NUL
del %DIR64%ARM\*.map > NUL
copy resource\*.* %DIR64%ARM\resource > NUL
cmd /c archive.bat arm64 %DIR64%ARM %readme%


rem *** final ***
perl final.pl
ppb /c %%u/7-ZIP32.DLL,a -tzip -hide -mx=7 -r ppx%XVER%map.zip *.map %DIR64%\*.map %DIR64%ARM\*.map
goto :EOF

rem *** arm64 main ***
:arm64main
CD %2
rem vcvarsall.bat x64_arm64
call VCARM64.bat
copy ..\64FILESARM\*.* .
del *.obj 2> NUL
del *.zip 2> NUL
make -DARM -DX64=1 -DRELEASE
for %%i in (*.dll) do CT %3 %%i
for %%i in (*.exe) do CT %3 %%i
del ..\ppxarm64%XVER%.zip 2> NUL
set arcpath=..\
set arcx=ppxarm64
set arcbit=64
set tail=W
if %RETAIL%==1 set etcfile=%etcfile% iftwic.spha UNBYPASS.DLL UNBYPASS.EXE PPXTEXT64A.DLL PPXSCR64A.DLL
call :archive
if %RETAIL%==1 CT PPX.TXT ..\ppxarm64%XVER%.zip
del *.c 2> NUL
del *.obj 2> NUL
goto :EOF

rem *** x64 main ***
:x64main
CD %2
rem vcvarsall.bat amd64
call VC64.BAT
copy ..\64FILES\*.* .
del *.obj 2> NUL
del *.zip 2> NUL
make -DX64=1 -DRELEASE
for %%i in (*.dll) do CT %3 %%i
for %%i in (*.exe) do CT %3 %%i
del ..\ppx64%XVER%.zip 2> NUL
set arcpath=..\
set arcx=ppx64
set arcbit=64
set tail=W
if %RETAIL%==1 set etcfile=%etcfile% iftwic.sph UNBYPASS.DLL UNBYPASS.EXE
call :archive
if %RETAIL%==1 CT PPX.TXT ..\ppx64%XVER%.zip
del *.c 2> NUL
del *.obj 2> NUL
goto :EOF

rem *** make ***
:makes
del /q *.obj 2> NUL
del /q *.res 2> NUL
rem make / make -DUseUnicode=1
call make%tail% -DRELEASE | perl makeflit.pl

rem ** archive **
:archive
set ppcommon=PPLIB%arcbit%%tail%.DLL
set ppc=PPC%tail%.EXE
if %RETAIL%==1 set ppv=PPV%tail%.EXE
set ppb=PPB%tail%.EXE
set ppcust=PPCUST%tail%.EXE
if %RETAIL%==1 set pptray=PPTRAY%tail%.EXE
if %RETAIL%==1 set ppffix=PPFFIX%tail%.EXE

tfilesign sp %ppcommon% %ppcommon%
if not "%ppc%"=="" tfilesign sp %ppc% %ppc%
if not "%ppv%"=="" tfilesign sp %ppv% %ppv%
if not "%ppb%"=="" tfilesign sp %ppb% %ppb%
if not "%ppcust%"=="" tfilesign sp %ppcust% %ppcust%
if not "%pptray%"=="" tfilesign sp %pptray% %pptray%
if not "%ppffix%"=="" tfilesign sp %ppffix% %ppffix%
if not "%ppsetup%"=="" tfilesign sp %ppsetup% %ppsetup%

for %%i in (*.exe) do CT %readme% %%i
if %RETAIL%==1 for %%i in (*.html) do CT %readme% %%i
if %RETAIL%==1 for %%i in (PPXF*.TXT) do CT %readme% %%i
CT %readme% PPX.CNT
CT %readme% PPX.HLP
CT %readme% %ppcommon%

del %arcpath%%arcx%%XVER%.zip 2> NUL
ppb /c %%u/7-ZIP32.DLL,a -tzip -hide -mx=7 %arcpath%%arcx%%XVER%.zip %readme% %ppsetup% %ppc% %ppv% %ppb% %ppcust% %pptray% %ppffix% %ppcommon% %etcfile%
tfilesign s %arcpath%%arcx%%XVER%.zip %arcpath%%arcx%%XVER%.zip
CT %readme% %arcpath%%arcx%%XVER%.zip

if %RETAIL%==1 echo [30;47mDirectWrite”Å[0m
