#ifndef WELLKNOWNPROGRAMSTHIRDPARTYPM_H
#define WELLKNOWNPROGRAMSTHIRDPARTYPM_H

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

    void scanDotNet(std::vector<InstalledPackageVersion*>* installed,
            Repository* rep) const;
    void detectOneDotNet(
            std::vector<InstalledPackageVersion *> *installed,
            Repository *rep,
            const WindowsRegistry& wr,
            const QString& keyName) const;

    /**
     * @brief detects MS XML libraries
     * @param installed information about installed packages will be stored here
     * @param rep information about packages and versions will be stored here
     * @return error message
     */
    QString detectMSXML(std::vector<InstalledPackageVersion *> *installed,
            Repository *rep) const;

    void detectWindows(std::vector<InstalledPackageVersion *> *installed,
            Repository *rep) const;
    void detectJRE(std::vector<InstalledPackageVersion *> *installed,
            Repository *rep, bool w64bit) const;
    void detectJDK(std::vector<InstalledPackageVersion *> *installed,
            Repository *rep, bool w64bit) const;

    void detectPython(std::vector<InstalledPackageVersion *> *installed,
            Repository *rep, bool w64bit) const;

    /**
     * @brief detects MSI
     * @param installed information about installed packages will be stored here
     * @param rep information about packages and versions will be stored here
     * @return error message
     */
    QString detectMicrosoftInstaller(std::vector<InstalledPackageVersion *> *installed,
            Repository *rep) const;
public:
    WellKnownProgramsThirdPartyPM(const QString& packageName);

    void scan(Job *job, std::vector<InstalledPackageVersion*>* installed,
              Repository* rep) const;
};

#endif // WELLKNOWNPROGRAMSTHIRDPARTYPM_H
