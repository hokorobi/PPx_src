/*-----------------------------------------------------------------------------
	Paper Plane cUI										ファイル情報
-----------------------------------------------------------------------------*/
#include "WINAPI.H"
#include <winioctl.h>
#include "PPX.H"
#include "VFS.H"
#include "PPC_STRU.H"
#include "PPC_FUNC.H"
#pragma hdrstop

#define ATTR_FLAGS_COUNT 24
static const TCHAR *AttrsLabels[ATTR_FLAGS_COUNT] = {
	T("Read only"),T("Hidden"),T("System"),T("Label"),		// 0x
	T("Directory"),T("Archive"),T("Device"),T("Normal"),	// x0
	T("Temporary"),T("Sparse file"),T("Reparse point"),T("Compressed"), // x00
	T("Offline"),T("Not content indexed"),T("Encrypted"),T("Integrity stream"), // x000
	T("Virtual"),T("No scrub data"),T("Recall on open"),T("Pinned"), //  x 0000
	T("Unpinned"),T("B21"),T("Recall on data access"),T("B23")       // x0 0000
//  T("B24"),T("B24"),T("B24"),T("B24"),	//  x00 0000
//  T("B28"),T("Strictly sequential"),T("B30"),T("B31"),	// x000 0000
};
static const TCHAR *status[] = {
	T("System Message"),T("Deleted"),T("Normal"),T("Gray"),
	T("Changed"),T("Added"),NilStr,NilStr
};

#pragma pack(push,1)
struct LXSSEA {
	BYTE ud1[4];	// 00 00 00 00
	WORD namelen;		// 00 07
	WORD EAlen;			// 38 00
	BYTE name[8];		// LXATTRB\0

	BYTE ud2[4];	// 00 00 01 00
	DWORD attr;			// 6d 81 00 00
	DWORD gp1,gp2;	// e8 03 00 00 e8 03 00 00

	DWORD ud3,ud4,ud5,ud6;	// 00 00 00 00 80 91 d4 14  58 01 c3 15 58 01 c3 15

	FILETIME date1,date2,date3;
};
#pragma pack(pop)

/*
void ConvEAtime(FILETIME *ftime,TCHAR **dstptr,const TCHAR *label)
{
	FILETIME sttime;
	FILETIME lTime;
	SYSTEMTIME sTime;

	DDmul(ftime->dwLowDateTime,10000000,&sttime.dwLowDateTime,&sttime.dwHighDateTime);
	sttime.dwHighDateTime += (ftime->dwHighDateTime >> 32) * 10000000;

	AddDD(sttime.dwLowDateTime,sttime.dwHighDateTime,0xd53e8000,0x19db1de);

	FileTimeToLocalFileTime(&sttime,&lTime);
	FileTimeToSystemTime(&lTime,&sTime);
	*dstptr += wsprintf(*dstptr,T("  %s : %04d-%02d-%02d%3d:%02d:%02d.%03d\r\n"),
			label,
			sTime.wYear % 10000,sTime.wMonth,sTime.wDay,
			sTime.wHour,sTime.wMinute,sTime.wSecond,sTime.wMilliseconds);
}
*/

void GetLxssEA(HANDLE hF,LPVOID *context,TCHAR **dstptr)
{
	struct LXSSEA eadata;
	DWORD reads;

	*dstptr += wsprintf(*dstptr,T("lxss EA(72)\r\n"));

	BackupRead(hF,(LPBYTE)&eadata,72,&reads,FALSE,FALSE,context);
	if ( reads != 72 ) return;
	if ( memcmp(eadata.name,"LXATTRB",8) != 0 ) return;
	*dstptr += wsprintf(*dstptr,T("   Attr: "));
	MakeStatString(*dstptr,eadata.attr,ECS_NORMAL,15);
	*dstptr += tstrlen(*dstptr);
/*
	ConvEAtime(&eadata.date1,&dstptr,T("time1"));
	ConvEAtime(&eadata.date2,&dstptr,T("time2"));
	ConvEAtime(&eadata.date3,&dstptr,T("time3"));
*/
}

void LoadSummary(TCHAR *name,TCHAR *type,TCHAR *text)
{
	TCHAR filename[VFPS];
	char *Summary = NULL;
	DWORD sizeL,fileL;		// ファイルの大きさ
	DWORD off = 0x40;

	wsprintf(filename,T("%s:\5%s"),name,type);
	if( !LoadFileImage(filename,0x40,&Summary,&sizeL,&fileL) ){
		while( off < fileL ){
			if ( *(DWORD *)(Summary + off) == 0x1e ){
				DWORD size;

				size = *(DWORD *)(Summary + off + 4);
				if (size){
					tstrcat(text,T("\r\nSummary       :"));
					text += tstrlen(text);
					#ifdef UNICODE
						AnsiToUnicode((Summary + off + 8),text,200);
					#else
						strcpy(text,(Summary + off + 8));
					#endif
				}
				off += size + 1 + 4;
			}
			if ( *(DWORD *)(Summary + off) == 0x1f ){
				DWORD size;

				size = *(DWORD *)(Summary + off + 4);
				if (size){
					tstrcat(text,T("\r\nSummaryU      :"));
					text += tstrlen(text);
					#ifdef UNICODE
						tstrcpy(text,(TCHAR *)(Summary + off + 8));
					#else
						UnicodeToAnsi((WCHAR *)(Summary + off + 8),text,200);
					#endif
				}
				off += (size * 2) + 4;
			}
			off = (off + 4) & ~(4 - 1);
		}
		HeapFree( hProcessHeap,0,Summary);
	}
}

DWORD DumpSID(TCHAR *dst,PSID psid,TCHAR *mes,const TCHAR *name)
{
	TCHAR fname[VFPS],oname[0x400],domain[0x400],*db = NULL;
	DWORD namesize,domainsize;
	SID_NAME_USE snu;

	namesize = TSIZEOF(oname);
	domainsize = TSIZEOF(domain);

	if ( name[0] == '\\' ){
		tstrcpy(fname,name);
		db = FindPathSeparator(fname + 2);
		if ( db != NULL ) *db = '\0';
		db = fname;
	}

	if ( IsTrue(LookupAccountSid(db,psid,
			oname,&namesize,domain,&domainsize,&snu)) ){
		return wsprintf(dst,T("\r\n%s:%s@%s"),mes,oname,domain);
	}else{
		return wsprintf(dst,T("\r\n%s:(error)"),mes);
	}
}

struct ACCESSNAMES {
	DWORD flag;
	TCHAR *name;
};
struct ACCESSNAMES fileaccess[] = {
						 //FILE_LIST_DIRECTORY
	{ FILE_READ_DATA, T("Read/List,") },
						// FILE_ADD_FILE
	{ FILE_WRITE_DATA, T("Write/AddFile,")},
						// FILE_ADD_SUBDIRECTORY,FILE_CREATE_PIPE_INSTANCE
	{ FILE_APPEND_DATA,T("Add,")},
	{ FILE_READ_EA,T("ReadEA,")},
	{ FILE_WRITE_EA,T("WriteEA,")},
	{ FILE_EXECUTE,T("Execute/Traverse,")}, //FILE_TRAVERSE
	{ FILE_DELETE_CHILD,T("DeleteChild,")},
	{ FILE_READ_ATTRIBUTES,T("ReadAttributes,")},
	{ FILE_WRITE_ATTRIBUTES,T("WriteAttributs,")},

	{ DELETE,		T("Delete,") },
	{ READ_CONTROL,	T("ReadControl,")},
	{ WRITE_DAC,	T("WriteDAC,")},
	{ WRITE_OWNER,	T("WriteOwner,")},
	{ SYNCHRONIZE,	T("Synchronize,")},
	{ ACCESS_SYSTEM_SECURITY,T("SystemACL,")},
	{ MAXIMUM_ALLOWED,T("AllAccess,")},
	{ GENERIC_ALL,	T("GenericAll,")},
	{ GENERIC_EXECUTE,T("GenericExecute,")},
	{ GENERIC_WRITE,T("GenericWrite,")},
	{ GENERIC_READ,	T("GenericRead,")},
	{ 0,NULL }
};

DWORD DumpAMask(TCHAR *dst,ACCESS_MASK am)
{
	int size,i;

	size = i = wsprintf(dst,T("\r\n (%04x),"),am);
	dst += i;

	if ( am == FILE_ALL_ACCESS ){
		size += wsprintf(dst,T("(All)"));
	}else if ( am == FILE_GENERIC_READ ){
		size += wsprintf(dst,T("(Read)"));
	}else if ( am == FILE_GENERIC_WRITE ){
		size += wsprintf(dst,T("(Write)"));
	}else if ( am == FILE_GENERIC_EXECUTE ){
		size += wsprintf(dst,T("(Execute)"));
	}else if ( am == (FILE_GENERIC_READ | FILE_GENERIC_EXECUTE) ){
		size += wsprintf(dst,T("(Read & Execute)"));
	}else if ( am == 0x1301bf ){
		size += wsprintf(dst,T("(Change)"));
	}else{
		struct ACCESSNAMES *an;

		an = fileaccess;
		while ( an->flag ){
			if ( am & an->flag ){
				size += i = wsprintf(dst,an->name);
				dst += i;
			}
			an++;
		}
	}
	return size;
}

#ifndef IO_REPARSE_TAG_HSM
#define IO_REPARSE_TAG_HSM	0xC0000004
#define IO_REPARSE_TAG_SIS	0x80000007 // シングル インスタンス ストレージ
#endif

#ifndef IO_REPARSE_TAG_DFS
#define IO_REPARSE_TAG_DFS	0x8000000A
#endif

#ifndef IO_REPARSE_TAG_HSM2
#define IO_REPARSE_TAG_HSM2	0x80000006 //  HD と Tape/MO との組み合わせ
#define IO_REPARSE_TAG_WIM	0x80000008
#define IO_REPARSE_TAG_CSV	0x80000009
#endif

#ifndef IO_REPARSE_TAG_DFSR
#define IO_REPARSE_TAG_DFSR	0x80000012
#endif

#ifndef IO_REPARSE_TAG_DEDUP
#define IO_REPARSE_TAG_DEDUP	0x80000013
#define IO_REPARSE_TAG_NFS		0x80000014
#endif

DWORD DumpReparsePoint(TCHAR *dst,LPCTSTR lpFileName)
{
	TCHAR rpath[VFPS];
	const TCHAR *tagname;
	DWORD tag = GetReparsePath(lpFileName,rpath);

	switch ( tag ){
		case IO_REPARSE_TAG_MOUNT_POINT:
			tagname = T("Junction/Volume Mount Point");
			break;

		case IO_REPARSE_TAG_HSM:
		case IO_REPARSE_TAG_HSM2:
			tagname = T("Hierarchical Storage Management");
			break;

		case IO_REPARSE_TAG_SIS:
			tagname = T("Single Instance Storage");
			break;

		case IO_REPARSE_TAG_WIM:
			tagname = T("Windows Imaging Format");
			break;

		case IO_REPARSE_TAG_CSV:
			tagname = T("Cluster Shared Volumes");
			break;

		case IO_REPARSE_TAG_DFS:
			tagname = T("Distributed File System");
			break;

		case IO_REPARSE_TAG_DFSR:
			tagname = T("DFS Replication");
			break;

		case IO_REPARSE_TAG_NFS:
			tagname = T("Network File System");
			break;

		case IO_REPARSE_TAG_DEDUP:
			tagname = T("DEDUPlication");
			break;

		case IO_REPARSE_TAG_SYMLINK:
			tagname = T("Symbolic link");
			break;

		case 0x80000021: // IO_REPARSE_TAG_ONEDRIVE
			tagname = T("onedrive");
			break;

		case 0x80000023: // IO_REPARSE_TAG_AF_UNIX
			tagname = T("AF_UNIX");
			break;

		default:
			tagname = T("unknown");
			break;
	}
	return wsprintf(dst,
			T("\r\nReparseTag    :%08x (%s)\r\n")
			T("ReparsePath   :%s"),
			tag,tagname,
			rpath);
}

void MakeFileInformation(PPC_APPINFO *cinfo,ThSTRUCT *text,ENTRYCELL *cell)
{
	TCHAR name[VFPS],ext[VFPS],sizebuf[100];
	VFSFILETYPE vft;
	ERRORCODE err;
	TCHAR *dst;

	ThInit(text);

	VFSFullPath(name,(TCHAR *)GetCellFileName(cinfo,cell,name),
			(cinfo->RealPath[0] != '?') ? cinfo->RealPath : cinfo->path);
	if ( cinfo->e.Dtype.mode == VFSDT_SHN ){
		if ( IsTrue(VFSGetRealPath(cinfo->info.hWnd,ext,name)) ){
			tstrcpy(name,ext);
		}
	}
	ThSize(text,0x2000);
	dst = (TCHAR *)text->bottom;

	FormatNumber(sizebuf,XFN_SEPARATOR,26,
			cell->f.nFileSizeLow,cell->f.nFileSizeHigh);
	dst += wsprintf(dst,
			T("Path          :%s\r\n")
			T("Long  name    :%s\r\n")
			T("Short name    :%s\r\n")
			T("Full path name:%s\r\n")
			T("Size          :%s\r\n")
			T("Internal Type :"),
			cinfo->path,
			cell->f.cFileName,
			GetCellSfnName(cinfo,cell,FALSE),
			name,
			sizebuf);

	vft.flags = VFSFT_TYPE | VFSFT_TYPETEXT | VFSFT_EXT | VFSFT_INFO;
	err = VFSGetFileType(name,NULL,0,&vft);
	if ( err == ERROR_NO_DATA_DETECTED ){
		tstrcpy(dst,T("Unknown File Type"));
	}else if ( err != NO_ERROR ){
		PPErrorMsg(dst,err);
	}else{
		dst += wsprintf(dst,T("%s(%s,*.%s)"),
						vft.typetext,vft.type,vft.ext);
		if ( vft.info != NULL ){
			dst += wsprintf(dst,T("\r\n"));
			if ( tstrlen(vft.info) > 0x1000 ){
				tstrcpy(dst,T("*info too large*"));
			}else{
				text->top = (char *)dst - text->bottom;
				ThCatString(text,vft.info);
				dst = (TCHAR *)ThLast(text);
			}
			HeapFree(hProcessHeap,0,vft.info);
		}
	}
	dst += tstrlen(dst);
	text->top = (char *)dst - text->bottom;

	ThSize(text,0x2000);
	dst = (TCHAR *)ThLast(text);

	if ( VFSCheckFileByExt(name,ext) == FALSE ){
		tstrcpy(ext,T("Not registration"));
	}
	{
		DWORD state;

		state = cell->state & ECS_STATEMASK;
		if (cell->attr & ECA_GRAY) state = ECS_GRAY;
		dst += wsprintf(dst,
			T("\r\n")
			T("Extension type:%s\r\n")
			T("Attributes    :(%x),%s\r\n "),
			ext,
			cell->f.dwFileAttributes,status[state]);
		if ( cell->f.dwFileAttributes == BADATTR ){
			dst += wsprintf(dst,T("attribute error"));
		}else{
			int i,usesep = 0;

			for ( i = 0 ; i < ATTR_FLAGS_COUNT ; i++){
				if ( cell->f.dwFileAttributes & ( 1 << i ) ){
					if ( usesep != 0 ){
						tstrcat(dst,T(","));
					}else{
						usesep = 1;
					}
					tstrcat(dst,AttrsLabels[i]);
				}
			}
			dst += tstrlen(dst);

			if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED ){
				DWORD h,l;

				l = GetCompressedFileSize(name,&h);
				FormatNumber(sizebuf,XFN_SEPARATOR,26,l,h);
				dst += wsprintf(dst,T("\r\nCompressedSize:%s"),sizebuf);
			}
		}
		if ( cell->f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ){
			dst += DumpReparsePoint(dst,name);
		}
	}

	tstrcpy(dst,T("\r\nCreate time   :"));
	dst += tstrlen(dst);
	dst += CnvDateTime(dst,NULL,NULL,&cell->f.ftCreationTime);
	tstrcpy(dst,T("\r\nLast Write    :"));
	dst += tstrlen(dst);
	dst += CnvDateTime(dst,NULL,NULL,&cell->f.ftLastWriteTime);
	tstrcpy(dst,T("\r\nLast Access   :"));
	dst += tstrlen(dst);
	dst += CnvDateTime(dst,NULL,NULL,&cell->f.ftLastAccessTime);
	{
		HANDLE hFile;
		BY_HANDLE_FILE_INFORMATION fi;

		hFile = CreateFileL(name, GENERIC_READ, FILE_SHARE_READ, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if ( hFile != INVALID_HANDLE_VALUE ){
			LPVOID context = NULL;

			if ( IsTrue(GetFileInformationByHandle(hFile, &fi)) ){
				DefineWinAPI(HANDLE, FindFirstFileNameW, (LPCWSTR lpFileName, DWORD dwFlags, LPDWORD StringLength, PWCHAR LinkName));
				DefineWinAPI(BOOL, FindNextFileNameW, (HANDLE hFindStream, LPDWORD StringLength, PWCHAR LinkName));
				HMODULE hKernel32 = GetModuleHandle(StrKernel32DLL);

				dst += wsprintf(dst,T("\r\nHard Links    :%d"),
						fi.nNumberOfLinks);
				if ( fi.nNumberOfLinks >= 2){
					GETDLLPROC(hKernel32, FindFirstFileNameW);
					GETDLLPROC(hKernel32, FindNextFileNameW);
					if ( DFindFirstFileNameW != NULL ){
						HANDLE hFFFN;
						WCHAR linknameW[VFPS];
						DWORD len;
						#ifdef UNICODE
							#define nameW name
						#else
							char nameT[VFPS];
							WCHAR nameW[VFPS];

							AnsiToUnicode(name,nameW,VFPS);
						#endif
						len = VFPS;
						hFFFN = DFindFirstFileNameW(nameW,0,&len,linknameW);
						if ( hFFFN != INVALID_HANDLE_VALUE ) for(;;){
							#ifdef UNICODE
								#define nameT linknameW
							#else
								UnicodeToAnsi(linknameW,nameT,VFPS);
							#endif
							dst += wsprintf(dst,T("\r\n*HardLink name:%s"),nameT);
							len = VFPS;
							if ( FALSE == DFindNextFileNameW(hFFFN,&len,linknameW) ){
								FindClose(hFFFN);
								break;
							}
						}
					}
				}
			}
			{
				WIN32_STREAM_ID stid;
				DWORD size,reads;
				WCHAR wname[VFPS];
				TCHAR *baseptr;

				for ( ; ; ){
					size = (LPBYTE)&stid.cStreamName - (LPBYTE)&stid;
					if ( FALSE == BackupRead(hFile, (LPBYTE)&stid,
							size, &reads, FALSE, FALSE, &context) ){
						break;
					}
					if ( reads < size ) break;
					if ( FALSE == BackupRead(hFile, (LPBYTE)&wname,
							stid.dwStreamNameSize,
							&reads, FALSE, FALSE, &context) ){
						break;
					}
					if ( reads < stid.dwStreamNameSize ) break;
					baseptr = dst;
					tstrcpy(dst,T("\r\n*Stream Name  :"));
					dst += tstrlen(dst);
					if ( stid.dwStreamNameSize ){
						wname[stid.dwStreamNameSize >> 1] = 0;
						#ifdef UNICODE
							tstrcpy(dst,wname);
						#else
							UnicodeToAnsi(wname,dst,VFPS);
						#endif
					}else{
						tstrcpy(dst,T("(none)"));
					}
					dst += tstrlen(dst);
					tstrcpy(dst,T("\r\n  Stream Type :"));
					dst += tstrlen(dst);
					switch ( stid.dwStreamId ){
						case BACKUP_DATA: // $DATA
							dst = baseptr;
							*dst = '\0';
							break;
						case BACKUP_EA_DATA: // $EA
							if ( (*(DWORD *)(&stid.Size) == 0x48) && (*((DWORD *)(&stid.Size) + 1) == 0) ){
								GetLxssEA(hFile,&context,&dst);
								continue;
							}
							tstrcpy(dst,T("Enhanced attributes"));
							break;
						case BACKUP_SECURITY_DATA: // $SECURITY_DESCRIPTOR
							tstrcpy(dst,T("Securities"));
							break;
						case BACKUP_ALTERNATE_DATA:
							tstrcpy(dst,T("Alternate data"));
							break;
						case BACKUP_LINK: // $FILE_NAME
							tstrcpy(dst,T("Hard Link"));
							break;
						case BACKUP_PROPERTY: // BACKUP_PROPERTY_DATA
							tstrcpy(dst,T("Properties"));
							break;
						case BACKUP_OBJECT_ID: // $OBJECT_ID
							tstrcpy(dst,T("Object ID"));
							break;
						case BACKUP_REPARSE_DATA: // $REPARSE_POINT
							tstrcpy(dst,T("Reparse DATA"));
							break;
						case BACKUP_SPARSE_BLOCK:
							tstrcpy(dst,T("Sparse blocks"));
							break;
						#ifndef BACKUP_TXFS_DATA
						  #define BACKUP_TXFS_DATA 0xa
						#endif
						case BACKUP_TXFS_DATA: // $TXF_DATA
							tstrcpy(dst,T("Transactional NTFS (TxF)"));
							break;
						#ifndef BACKUP_GHOSTED_FILE_EXTENTS
						  #define BACKUP_GHOSTED_FILE_EXTENTS 0xb
						#endif
						case BACKUP_GHOSTED_FILE_EXTENTS:
							tstrcpy(dst,T("Ghosted file extent"));
							break;
						default:
							wsprintf(dst,T("Unknown(%x)"),stid.dwStreamId);
							break;
					}
					if ( *dst != '\0' ){
						dst += tstrlen(dst);

						tstrcpy(dst,T("\r\n  Stream Size :"));
						dst += tstrlen(dst);
// ※ LowPart/HighPart が使えないため、直接指定している
						FormatNumber(dst,XFN_SEPARATOR,26,
							*(DWORD *)(&stid.Size),*((DWORD *)(&stid.Size) + 1));
						dst += tstrlen(dst);
					}

					if ( FALSE == BackupSeek(hFile,
								*(DWORD *)(&stid.Size),
								*((DWORD *)(&stid.Size) + 1),
								&reads,&reads,&context) ){
						break;
					}
				}
				if ( context != NULL ){
					BackupRead(hFile,(LPBYTE)&stid,0,&reads,TRUE,FALSE,&context);
				}
			}
			CloseHandle(hFile);
		}
	}
	if ( cell->comment != EC_NOCOMMENT ){
		tstrcpy(dst,T("\r\nComment       :"));
		dst += tstrlen(dst);
		tstrcpy(dst,ThPointerT(&cinfo->EntryComments,cell->comment));
		dst += tstrlen(dst);
	}
	if ( cell->cellcolumn >= 0 ){
		ENTRYEXTDATASTRUCT eeds;
		int CommentID;

		for ( CommentID = 1 ; CommentID <= 10 ; CommentID++ ){
			eeds.id = (DWORD)(DFC_COMMENTEX_MAX - (CommentID - 1) );
			eeds.size = VFPS * sizeof(TCHAR);
			eeds.data = (BYTE *)ext;
			if ( IsTrue(EntryExtData_GetDATA(cinfo,&eeds,cell)) ){
				dst += wsprintf(dst,T("\r\nComment%d      :%s"),CommentID,ext);
			}
		}
	}
	{
		SECURITY_DESCRIPTOR *sd;
		BYTE sdbuf[0x400];
		DWORD size;

		sd = (SECURITY_DESCRIPTOR *)sdbuf;
		if ( FALSE != GetFileSecurity(
				name,OWNER_SECURITY_INFORMATION,sd,sizeof sdbuf,&size) ){
			PSID psid;
			BOOL ownflag;

			GetSecurityDescriptorOwner(sd,&psid,&ownflag);

			if ( IsTrue(ownflag) ){
				dst += wsprintf(dst,T("\r\nOwner         :<inheritance>"));
			}else{
				dst += DumpSID(dst,psid,T("Owner         "),name);
			}
		}
	}
	{
		SECURITY_DESCRIPTOR *sd;
		BYTE sdbuf[0x400];
		DWORD size;

		sd = (SECURITY_DESCRIPTOR *)sdbuf;
		if ( FALSE != GetFileSecurity(name,DACL_SECURITY_INFORMATION,
				sd,sizeof sdbuf,&size) ){
			PACL pacl;
			BOOL have,def;

			GetSecurityDescriptorDacl(sd,&have,&pacl,&def);
			if ( IsTrue(have) ){
				int index = 0;
				ACE_HEADER *ace;

				if ( pacl == NULL ){
					dst += wsprintf(dst,T("\r\n*Access All"));
				}else while( IsTrue(GetAce(pacl,index++,(void **)&ace)) ){
					switch ( ace->AceType ){
						case ACCESS_ALLOWED_ACE_TYPE:
							dst += DumpSID(dst,
								(PSID)&((ACCESS_ALLOWED_ACE *)ace)->SidStart,
								T("*Access Allow."),name);
							dst += DumpAMask(dst,
										((ACCESS_ALLOWED_ACE *)ace)->Mask);
							break;
						case ACCESS_DENIED_ACE_TYPE:
							dst += DumpSID(dst,
								(PSID)&((ACCESS_DENIED_ACE *)ace)->SidStart,
								T("*Access Denied"),name);
							dst += DumpAMask(dst,
										((ACCESS_ALLOWED_ACE *)ace)->Mask);
							break;
						case SYSTEM_AUDIT_ACE_TYPE:
							dst += DumpSID(dst,
								(PSID)&((SYSTEM_AUDIT_ACE *)ace)->SidStart,
								T("*Audit        "),name);
							dst += DumpAMask(dst,
										((ACCESS_ALLOWED_ACE *)ace)->Mask);
							break;
						default:
							dst += wsprintf(dst,T("\r\n*Access Unknown:"));
					}
				}
			}
		}
	}
	dst = GetColumnExtTextInfo(cinfo,cell->cellcolumn,dst);
	LoadSummary(name,T("SummaryInformation"),dst);
	LoadSummary(name,T("DocumentSummaryInformation"),dst);
	LoadSummary(name,T("SebiesnrMkudrfcoIaamtykdDa"),dst);

	dst += tstrlen(dst);
	*dst++ = '\r';
	*dst++ = '\n';
	*dst = '\0';
	text->top = (char *)dst - text->bottom;
	VistaProperties(name,text);
}
