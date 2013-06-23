#ifndef INSTALLEDPACKAGESTHIRDPARTYPM_H
#define INSTALLEDPACKAGESTHIRDPARTYPM_H

#include <QList>

#include "abstractthirdpartypm.h"
#include "installedpackageversion.h"
#include "repository.h"

/**
 * Third party package manager that uses the information about installed
 * packages to find existing package names and versions.
 */
class InstalledPackagesThirdPartyPM : public AbstractThirdPartyPM
{
public:
    /**
     * -
     */
    InstalledPackagesThirdPartyPM();

    void scan(Job *job, QList<InstalledPackageVersion*>* installed,
            Repository* rep) const;
};

#endif // INSTALLEDPACKAGESTHIRDPARTYPM_H
