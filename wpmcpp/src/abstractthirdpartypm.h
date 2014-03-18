#ifndef ABSTRACTTHIRDPARTYPM_H
#define ABSTRACTTHIRDPARTYPM_H

#include <QList>

#include "installedpackageversion.h"
#include "package.h"
#include "packageversion.h"
#include "installedpackageversion.h"
#include "repository.h"

/**
 * @brief 3rd party package manager
 */
class AbstractThirdPartyPM
{
public:
    /**
     * @brief -
     */
    AbstractThirdPartyPM();

    virtual ~AbstractThirdPartyPM();

    /**
     * @brief scans the 3rd party package manager repository
     *
     * @param job job
     * @param installed [ownership:caller] installed package versions. Please
     *     note that this information is from another package manager. It is
     *     possible that some packages are installed in nested directories
     *     to each other or to one of the package versions defined in Npackd.
     *     Please also note that the directory for an object stored here
     *     may be empty even if a package
     *     version is installed. This means that the directory is not known.
     * @param rep [ownership:caller] packages, package versions and licenses
     *     will be stored here. Package versions with a file named
     *     ".Npackd\Uninstall.bat" will be used to define uninstallers
     */
    virtual void scan(Job* job, QList<InstalledPackageVersion*>* installed,
            Repository* rep) const = 0;
};

#endif // ABSTRACTTHIRDPARTYPM_H
