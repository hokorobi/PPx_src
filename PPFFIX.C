/*-----------------------------------------------------------------------------
	Paper Plane file extension fixer
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include "PPX.H"
#include "VFS.H"
#pragma hdrstop

#define CHECKSIZE	(1 * MB)	// í≤ç∏Ç…ópÇ¢ÇÈëÂÇ´Ç≥

const TCHAR StrHelp[] = MES_FIXH;
const TCHAR StrUnknown[] = MES_FIXU;
const TCHAR StrModify[] = MES_FIXM;
const TCHAR StrEqual[] = MES_FIXE;

/*=============================================================================
	WinMain
=============================================================================*/
#pragma argsused
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	TCHAR fname[VFPS];
	int CaseLower = FALSE;
#ifdef UNICODE
	#define CMD cmdptr
	const TCHAR *cmdptr;
	UnUsedParam(hInstance);UnUsedParam(hPrevInstance);UnUsedParam(lpCmdLine);UnUsedParam(nShowCmd);

	cmdptr = GetCommandLine();
	GetLineParam(&CMD,fname);
#else
	#define CMD lpCmdLine
	UnUsedParam(hInstance);UnUsedParam(hPrevInstance);UnUsedParam(nShowCmd);
#endif

	if ( GetLineParam((const TCHAR **)&CMD,fname) == '\0' ){
		xmessage(XM_INFOld,StrHelp);
		return 0;
	}
	GetCustData(T("X_ffxl"),&CaseLower,sizeof(CaseLower));
	do{
		char *image = NULL;
		DWORD imgsize;

		if ( LoadFileImage(fname,CHECKSIZE,&image,&imgsize,LFI_ALWAYSLIMIT) ){
			PPErrorBox(NULL,fname,PPERROR_GETLASTERROR);
		}else{
			ERRORCODE err;
			VFSFILETYPE vft;

			vft.flags = VFSFT_TYPETEXT | VFSFT_EXT;
			err = VFSGetFileType(fname,image,imgsize,&vft);
			HeapFree(GetProcessHeap(),0,image);
			if ( err == ERROR_NO_DATA_DETECTED ){
				XMessage(NULL,fname,XM_RESULTld,StrUnknown);
			}else if ( err != NO_ERROR ){
				PPErrorBox(NULL,fname,err);
			}else{
				TCHAR mes[0x1000];
				TCHAR newname[VFPS],*namep,*extp,*extnp;
				const TCHAR *mesp;
				UINT type;

				tstrcpy(newname,fname);
				namep = VFSFindLastEntry(newname);
				if ( *namep == '\\' ) namep++;
				extnp = extp = namep + FindExtSeparator(namep);
				if ( *extnp == '.' ) extnp++;
				if ( tstricmp(extnp,vft.ext) ){ // ïœçXóLÇË
					mesp = StrModify;
					type = MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION;
				}else{
					mesp = StrEqual;
					type = MB_OK | MB_DEFBUTTON1 | MB_ICONINFORMATION;
				}
				if ( IsTrue(CaseLower) ) Strlwr(vft.ext);
				wsprintf(mes,MessageText(mesp),vft.typetext,vft.ext);
				if ( PMessageBox(NULL,mes,namep,type) == IDYES ){
					if ( *extp != '.' ) *extp = '.';
					tstrcpy(extp + 1,vft.ext);
					if ( MoveFile(fname,newname) == FALSE ){
						PPErrorBox(NULL,NULL,err);
					}
				}
			}
		}
	}while( GetLineParam((const TCHAR **)&CMD,fname) );
	return 0;
}
