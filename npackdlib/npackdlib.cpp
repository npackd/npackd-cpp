#include "npackdlib.h"


BOOL GetInstalledInfo(PDWORD ninfo,
        LPINSTALLED_PACKAGE_VERSION* info,
        PROGRESSCHANGEPROC pf,
        LPCWSTR* err)
{
    return FALSE;
}

BOOL Process(PDWORD nops, LPINSTALL_OPERATION* ops,
        PROGRESSCHANGEPROC pf, LPCWSTR* err)
{
    return FALSE;
}

BOOL Plan(BOOL install, LPCWSTR package,
        LPCWSTR version, PDWORD nops, LPINSTALL_OPERATION* ops,
        PROGRESSCHANGEPROC pf, LPCWSTR* err)
{
    return FALSE;
}

BOOL Find(LPCWSTR text, BOOL finstalled,
        DWORD offset, DWORD max,
        PDWORD npackages,
        LPPACKAGE* packages, PROGRESSCHANGEPROC pf, LPCWSTR* err)
{
    return FALSE;
}

BOOL GetVersions(LPCWSTR package,
        PDWORD nversions,
        LPPACKAGE_VERSION* versions, PROGRESSCHANGEPROC pf, LPCWSTR* err)
{
    return FALSE;
}

BOOL GetRepositories(
        PDWORD nreps,
        LPREPOSITORY* reps, LPCWSTR* err)
{
    return FALSE;
}

BOOL SetRepositories(
        DWORD nreps,
        LPREPOSITORY reps, LPCWSTR* err)
{
    return FALSE;
}

