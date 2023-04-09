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
     * Changes the versions.
     *
     * @param versions something like "[2.12, 3.4)"
     */
    bool setVersions(const QString& versions);

    /**
     * @brief changes the range so that any version is allowed
     */
    void setUnboundedVersions();

    /**
     * @brief changes the range so that only the specified version is allowed
     */
    void setExactVersion(const Version& version);

    /**
     * Checks whether this dependency is automatically fulfilled if the
     * specified dependency is already fulfilled.
     *
     * @param dep another dependency
     * @return true if this dependency is automatically fulfilled
     */
    bool autoFulfilledIf(const Dependency& dep);

    /**
     * @return [min, max]
     */
    QString versionsToString() const;

    /**
     * @return [move] copy of this object
     */
    Dependency* clone() const;
};

#endif // DEPENDENCY_H
