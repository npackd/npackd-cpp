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
     * @brief scan scans the 3rd party package manager repository
     *
     * @param job job
     * @param installed [ownership:caller] installed package versions. Please
     *     note that this information is from another package manager. It is
     *     possible that some packages are installed in nested directories
     *     to each other or to one of the package versions defined in Npackd.
     *     Please also note that the directory for an object stored here
     *     may be empty even if a package
     *     version is installed. This means that the directory is not known.
     *     If the directory is not empty, but is under "C:\Windows" or another
     *     package, it will be handled as if the value would be empty.
     *
     *     These 4 cases exist:
     *     case 1: directory is empty, "Uninstall.bat" is not available.
     *         A directory under "NpackdDetected" will be created and a simple
     *         "Uninstall.bat" that generates an error will be stored there.
     *     case 2: directory is empty, "Uninstall.bat" is available.
     *         A directory under "NpackdDetected" will be created and the
     *         "Uninstall.bat" will be placed there
     *     case 3: directory is not empty, "Uninstall.bat" is not available.
     *         The package removal would just delete the directory.
     *     case 4: directory is not empty, "Uninstall.bat" is available. The
     *         "Uninstall.bat" will be stored in the package directory, if
     *         it does not already exist.
     * @param rep [ownership:caller] packages, package versions and licenses
     *     will be stored here. Package versions with a file named
     *     ".Npackd\Uninstall.bat" will be used to define uninstallers
     */
    virtual void scan(Job* job, QList<InstalledPackageVersion*>* installed,
            Repository* rep) const = 0;
};

#endif // ABSTRACTTHIRDPARTYPM_H
