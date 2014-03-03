/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the w64 mingw-runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error This stub requires an updated version of <rpcndr.h>
#endif

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif

//#ifndef _MSOAV2_H
//#define _MSOAV2_H

#ifndef __IOfficeAntiVirus_FWD_DEFINED__
#define __IOfficeAntiVirus_FWD_DEFINED__
typedef struct IOfficeAntiVirus IOfficeAntiVirus;
#endif

#include "oaidl.h"
#include "oleidl.h"
#include <guiddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _msoavinfo {
  int cbsize;
  struct {
    ULONG fPath:1;
    ULONG fReadOnlyRequest:1;
    ULONG fInstalled:1;
    ULONG fHttpDownload:1;
  };
  HWND hwnd;
  union {
    WCHAR *pwzFullPath;
    LPSTORAGE lpstg;
  } u;
  WCHAR *pwzHostName;
  WCHAR *pwzOrigURL;
} MSOAVINFO;

DEFINE_GUID(IID_IOfficeAntiVirus,0x56ffcc30,0xd398,0x11d0,0xb2,0xae,0x0,0xa0,0xc9,0x8,0xfa,0x49);
DEFINE_GUID(CATID_MSOfficeAntiVirus,0x56ffcc30,0xd398,0x11d0,0xb2,0xae,0x0,0xa0,0xc9,0x8,0xfa,0x49);

#ifndef __IOfficeAntiVirus_INTERFACE_DEFINED__
#define __IOfficeAntiVirus_INTERFACE_DEFINED__
  EXTERN_C const IID IID_IOfficeAntiVirus;
#if defined(__cplusplus) && !defined(CINTERFACE)
struct IOfficeAntiVirus : public IUnknown {
  public:
    virtual HRESULT WINAPI Scan(MSOAVINFO *pmsoavinfo) = 0;
  };
#endif
#endif

#ifndef AVVENDOR
//MSOAPI_(WINBOOL) MsoFAnyAntiVirus(HMSOINST hmsoinst);
//MSOAPI_(WINBOOL) MsoFDoAntiVirusScan(HMSOINST hmsoinst,MSOAVINFO *msoavinfo);
//MSOAPI_(void) MsoFreeMsoavStuff(HMSOINST hmsoinst);
//MSOAPI_(WINBOOL) MsoFDoSecurityLevelDlg(HMSOINST hmsoinst,DWORD msorid,int *pSecurityLevel,WINBOOL *pfTrustInstalled,HWND hwndParent,WINBOOL fShowVirusCheckers,WCHAR *wzHelpFile,DWORD dwHelpId);

#define msoedmEnable 1
#define msoedmDisable 2
#define msoedmDontOpen 3

// MSOAPI_(int) MsoMsoedmDialog(HMSOINST hmsoinst,WINBOOL fAppIsActive,WINBOOL fHasVBMacros,WINBOOL fHasXLMMacros,void *pvDigSigStore,void *pvMacro,int nAppID,HWND hwnd,const WCHAR *pwtzPath,int iClient,int iSecurityLevel,int *pmsodsv,WCHAR *wzHelpFile,DWORD dwHelpId,HANDLE hFileDLL,WINBOOL fUserControl);

#define msoslUndefined 0
#define msoslNone 1
#define msoslMedium 2
#define msoslHigh 3

//MSOAPI_(int) MsoMsoslGetSL(HMSOINST hmsoinst);
//MSOAPI_(int) MsoMsoslSetSL(DWORD msorid,HMSOINST hmsoinst);

#define msodsvNoMacros 0
#define msodsvUnsigned 1

#define msodsvPassedTrusted 2
#define msodsvFailed 3
#define msodsvLowSecurityLevel 4
#define msodsvPassedTrustedCert 5
#endif

#ifdef __cplusplus
}
#endif

//#endif
