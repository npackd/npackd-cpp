#ifndef CBSTHIRDPARTYPM_H
#define CBSTHIRDPARTYPM_H

#include "abstractthirdpartypm.h"
#include "installedpackageversion.h"
#include "repository.h"

/**
 * @brief 3rd party package manager for "Component Based Servicing"
 * http://technet.microsoft.com/en-us/library/cc756291(v=ws.10).aspx
 */
class CBSThirdPartyPM: public AbstractThirdPartyPM
{
    void detectOneCBSPackage(QList<InstalledPackageVersion*>* installed,
            Repository* rep,
            const QString &registryPath,
            WindowsRegistry &k, const QString &keyName) const;
    void detectCBSPackagesFrom(
            QList<InstalledPackageVersion*>* installed,
            Repository* rep,
            HKEY root, const QString &path,
            bool useWoWNode) const;

    // Example: HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\Packages\Microsoft-Windows-IIS-WebServer-AddOn-2-Package~31bf3856ad364e35~amd64~~6.1.7600.16385\Updates
    void detectOneCBSPackageUpdate(const QString &superPackage,
            QList<InstalledPackageVersion *> *installed,
            Repository *rep, const QString &registryPath,
            WindowsRegistry &k, bool superPackageInstalled) const;
public:
    void scan(Job *job, QList<InstalledPackageVersion*>* installed,
              Repository* rep) const;
};

#endif // CBSTHIRDPARTYPM_H
