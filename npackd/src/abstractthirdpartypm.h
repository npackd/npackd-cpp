#ifndef ABSTRACTTHIRDPARTYPM_H
#define ABSTRACTTHIRDPARTYPM_H

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
     * all InstalledPackageVersion.detectionInfo created by this package manager
     * start with this prefix. The value can be empty.
     */
    QString detectionPrefix;

    /**
     * @brief -
     */
    AbstractThirdPartyPM();

    virtual ~AbstractThirdPartyPM();

    /**
     * @brief suitable for QRunnable::run. Calls scan()
     */
    void scanRunnable(Job* job, std::vector<InstalledPackageVersion*>* installed,
            Repository* rep);

    /**
     * @brief scans the 3rd party package manager repository
     *
     * @param job job
     * @param installed installed package versions. Please
     *     note that this information is from another package manager. It is
     *     possible that some packages are installed in nested directories
     *     to each other or to one of the package versions defined in Npackd.
     *     Please also note that the directory for an object stored here
     *     may be empty even if a package version is installed.
     *     This means that the directory is not known. The order is important
     *     as the data is later processed in this order. For example,
     *     "com.microsoft.Windows" package should be placed first as its
     *     location is detected precisely and other packages cannot be under
     *     the Windows directory.
     * @param rep packages, package versions and licenses
     *     will be stored here. Package versions with a file named
     *     ".Npackd\Uninstall.bat" will be used to define uninstallers
     */
    virtual void scan(Job* job,
            std::vector<InstalledPackageVersion*>* installed,
            Repository* rep) const = 0;
};

#endif // ABSTRACTTHIRDPARTYPM_H
