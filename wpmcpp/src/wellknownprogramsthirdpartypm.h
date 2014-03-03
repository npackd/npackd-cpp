#ifndef WELLKNOWNPROGRAMSTHIRDPARTYPM_H
#define WELLKNOWNPROGRAMSTHIRDPARTYPM_H

#include <QList>

#include "abstractthirdpartypm.h"
#include "installedpackageversion.h"
#include "repository.h"
#include "windowsregistry.h"

/**
 * @brief detects some well-known programs like .NET or Java
 */
class WellKnownProgramsThirdPartyPM: public AbstractThirdPartyPM
{
    QString packageName;

    void scanDotNet(QList<InstalledPackageVersion*>* installed,
            Repository* rep) const;
    void detectOneDotNet(
            QList<InstalledPackageVersion *> *installed,
            Repository *rep,
            const WindowsRegistry& wr,
            const QString& keyName) const;

    /**
     * @brief detects MS XML libraries
     * @param installed information about installed packages will be stored here
     * @param rep information about packages and versions will be stored here
     * @return error message
     */
    QString detectMSXML(QList<InstalledPackageVersion *> *installed,
            Repository *rep) const;

    void detectWindows(QList<InstalledPackageVersion *> *installed,
            Repository *rep) const;
    void detectJRE(QList<InstalledPackageVersion *> *installed,
            Repository *rep, bool w64bit) const;
    void detectJDK(QList<InstalledPackageVersion *> *installed,
            Repository *rep, bool w64bit) const;

    /**
     * @brief detects MSI
     * @param installed information about installed packages will be stored here
     * @param rep information about packages and versions will be stored here
     * @return error message
     */
    QString detectMicrosoftInstaller(QList<InstalledPackageVersion *> *installed,
            Repository *rep) const;
public:
    WellKnownProgramsThirdPartyPM(const QString& packageName);

    void scan(Job *job, QList<InstalledPackageVersion*>* installed,
              Repository* rep) const;
};

#endif // WELLKNOWNPROGRAMSTHIRDPARTYPM_H
