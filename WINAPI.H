#define STRICT
#undef WINVER
#undef _WIN32_WINNT
#ifndef _WIN64 // 32bit : Win95/NT4.0
  #define WINVER 0x0400
  #define _WIN32_WINNT 0x0400
#else // 64bit : WinXP x64 / 2003
  #define WINVER 0x0502
  #define _WIN32_WINNT 0x0502
#endif
#include <windows.h>

#ifndef CREATE_PROTECTED_PROCESS
#define CREATE_PROTECTED_PROCESS 0x40000 // 保護モードで実行(Vista以降, DRMを扱うソフト用)
#endif

#ifndef FILE_ATTRIBUTE_INTEGRITY_STREAM
#define FILE_ATTRIBUTE_INTEGRITY_STREAM 0x8000
#endif
#ifndef FILE_ATTRIBUTE_NO_SCRUB_DATA
#define FILE_ATTRIBUTE_NO_SCRUB_DATA 0x20000
#endif
#ifndef FILE_ATTRIBUTE_PINNED
#define FILE_ATTRIBUTE_PINNED   0x080000
#define FILE_ATTRIBUTE_UNPINNED 0x100000
#endif
#ifndef FILE_ATTRIBUTE_RECALL_ON_OPEN
#define FILE_ATTRIBUTE_RECALL_ON_OPEN        0x040000
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x400000
#endif

#ifndef WM_GETOBJECT
#define WM_GETOBJECT 0x003D
#define OBJID_CLIENT ((LONG)0xFFFFFFFC)
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#ifndef EVENT_OBJECT_FOCUS
#define EVENT_OBJECT_FOCUS 0x8005
#endif

#ifndef ERROR_ELEVATION_REQUIRED
 #define ERROR_ELEVATION_REQUIRED 740
#endif
#ifndef ERROR_CANT_ACCESS_FILE
 #define ERROR_CANT_ACCESS_FILE 1920
#endif

#ifndef GetCompressedFileSize // WINVER > 500 になっている SDK がある
 WINBASEAPI DWORD WINAPI GetCompressedFileSizeA(LPCSTR, LPDWORD);
 WINBASEAPI DWORD WINAPI GetCompressedFileSizeW(LPCWSTR, LPDWORD);
 #ifdef UNICODE
  #define GetCompressedFileSize  GetCompressedFileSizeW
 #else
  #define GetCompressedFileSize  GetCompressedFileSizeA
 #endif
#endif

#ifndef MIIM_STRING
#define MIIM_STRING 0x40
#define MIIM_FTYPE 0x100
#endif
