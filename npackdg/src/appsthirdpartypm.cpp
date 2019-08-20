#include <roapi.h>

#include "appsthirdpartypm.h"

AppsThirdPartyPM::AppsThirdPartyPM()
{

}

void AppsThirdPartyPM::scan(Job* job,
        QList<InstalledPackageVersion *> *installed,
        Repository *rep) const
{
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (!FAILED(hr)) {
        LPCTSTR str = "Windows.Management.Deployment.PackageManager";
        HSTRING hstr;
        hr = WindowsCreateString(str, _tcslen(str), &hstr);
        IPackageManager* pIPackageManager;
        hr = RoActivateInstance(hstr, (IInspectable**)(&pIPackageManager));
        WindowsDeleteString(hstr);
        if (FAILED(hr)) {
        Finalize();
    }

/*
FOREACH p IN PackageManager.FindPackagesForUserWithPackageTypes(null,
PackageType_Main|PackageType_Optional)
{
    PACKAGE_INFO_REFERENCE pir
    OpenPackageInfoByFullName(p.Id.FullName, 0, &pir)
    UINT32 n=0
    GetPackageApplicationIds(pir, &n, null, null)
    BYTE* buffer = new BYTE[n]
    UINT32 count=0
    GetPackageApplicationIds(pir, &n, buffer, &count)
    ClosePackageInfo(pir)
    PCWSTR * applicationUserModelIds = reinterpret_cast<PCWSTR*>(buffer);
    FOR (i=0; i<count; ++i)
    {
        PCWSTR applicationUserModelId = applicationUserModelIds[i]
    }
    delete [] buffer
} */
    job->setProgress(1);
    job->complete();
}
