#ifndef INSTALLOPERATION_H
#define INSTALLOPERATION_H

#include "qlist.h"

#include "packageversion.h"

/**
 * Installation operation.
 */
class InstallOperation
{
public:
    /** true = install, false = uninstall */
    bool install;

    /** full package name. This package will be modified. */
    QString package;

    /** package version */
    Version version;

    /**
     * if "where" is not empty:
     * true = exact location is necessary, fail otherwise
     * false = choose a similar installation directory, e.g. "Notepad_2.81" instead of "Notepad"
     */
    bool exactLocation = true;

    /**
     * the directory where the package version should be installed on "" if the
     * the installation directory should be determined automatically.
     */
    QString where;

    InstallOperation();

    /**
     * @brief finds the corresponding package version
     * @param err error message will be stored here
     * @return [move] found package version or 0
     */
    PackageVersion* findPackageVersion(QString *err) const;

    /**
     * @return [move] copy of this object
     */
    InstallOperation* clone() const;

    /**
     * @return debug representation
     */
    QString toString() const;

    /**
     * Simplifies a list of operations.
     *
     * @param ops a list of operations. The list will be modified and
     *     unnecessary operations removed and the objects destroyed.
     */
    static void simplify(QList<InstallOperation*> ops);
};

#endif // INSTALLOPERATION_H
