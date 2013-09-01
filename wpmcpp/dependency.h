#ifndef DEPENDENCY_H
#define DEPENDENCY_H

#include <QString>

#include "version.h"
#include "installedpackageversion.h"

class PackageVersion;

/**
 * A dependency from another package.
 */
class Dependency
{
public:
    /** dependency on this package */
    QString package;

    /** ( or [ */
    bool minIncluded;

    /** lower bound */
    Version min;

    /** ) or ] */
    bool maxIncluded;

    /** high bound */
    Version max;

    /** variable name or an empty string */
    QString var;

    /**
     * [0, 1)
     */
    Dependency();

    /**
     * @param v a version
     * @return true if the version can be used for this dependency
     */
    bool test(const Version& v) const;

    /**
     * @return [ownership:caller] all package versions that match
     *     this dependency and are installed
     */
    QList<InstalledPackageVersion*> findAllInstalledMatches() const;

    /**
     * @param avoid list of package versions that should be avoided and cannot
     *     be considered to be a match
     * @param err error message will be stored here
     * @return [ownership:caller] the newest package version that matches this
     *     dependency by
     *     being installed. Returned object should be destroyed later.
     */
    PackageVersion* findBestMatchToInstall(const QList<PackageVersion*>& avoid,
            QString *err);

    /**
     * @param avoid list of package versions that should be avoided and cannot
     *     be considered to be a match
     * @param err error message will be stored here
     * @return [ownership:caller] all package versions that matches this
     *     dependency by
     *     being installed. Returned objects should be destroyed later.
     *     The returned objects are sorted by the package version number. The
     *     first returned object has the highest version number.
     */
    QList<PackageVersion *> findAllMatchesToInstall(
            const QList<PackageVersion *> &avoid, QString *err);

    /**
     * Changes the versions.
     *
     * @param versions something like "[2.12, 3.4)"
     */
    bool setVersions(const QString versions);

    /**
     * Checks whether this dependency is automatically fulfilled if the
     * specified dependency is already fulfilled.
     *
     * @param dep another dependency
     * @return true if this dependency is automatically fulfilled
     */
    bool autoFulfilledIf(const Dependency& dep);

    /**
     * @return [ownership:callser] the newest package version that matches this
     *     dependency and are installed
     */
    InstalledPackageVersion* findHighestInstalledMatch() const;

    /**
     * @return true if a package, that satisfies this dependency, is installed
     */
    bool isInstalled();

    /**
     * @param includeFullPackageName true = the full package name will be added
     * @return human readable representation of this dependency
     */
    QString toString(bool includeFullPackageName=false);

    /**
     * @return [min, max]
     */
    QString versionsToString() const;

    /**
     * @return [ownership:caller] copy of this object
     */
    Dependency* clone() const;
};

#endif // DEPENDENCY_H
