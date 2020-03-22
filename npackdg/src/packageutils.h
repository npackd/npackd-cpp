#ifndef PACKAGEUTILS_H
#define PACKAGEUTILS_H

#include "QList"
#include "QString"

#include "packageversion.h"
#include "dbrepository.h"
#include "commandline.h"
#include "dependency.h"

class PackageUtils
{
private:
    PackageUtils();
public:
    /** true = install programs globally, false = locally */
    static bool adminMode;

    /**
     * @brief parses the command line and returns the list of chosen package
     *     versions
     * @param dbr DB-Repository
     * @param cl command line
     * @param err errors will be stored here
     * @return [owner:caller] list of package versions
     */
    static QList<PackageVersion *> getAddPackageVersionOptions(const DBRepository& dbr, const CommandLine &cl, QString *err);

    /**
     * @brief parses the command line and returns the list of chosen package
     *     versions
     * @param cl command line
     * @param err errors will be stored here
     * @return [owner:caller] list of package versions ranges
     */
    static QList<Dependency *> getPackageVersionOptions(const CommandLine &cl,
            QString *err);

    /**
     * Validates a full package name.
     *
     * @param n a package name
     * @return an error message or an empty string if n is a valid package name.
     */
    static QString validateFullPackageName(const QString& n);

    /**
     * @param name invalid full package name
     * @return valid package name
     */
    static QString makeValidFullPackageName(const QString& name);

    /**
     * @return directory where the packages will be installed. Typically
     *     c:\program files\Npackd
     */
    static QString getInstallationDirectory();

    /**
     * see getInstallationDirectory()
     *
     * @return error message or ""
     */
    static QString setInstallationDirectory(const QString& dir);

    /**
     * @return how the programs should be closed
     */
    static DWORD getCloseProcessType();

    /**
     * @brief changes how the programs should be closed
     * @param cpt new value
     */
    static void setCloseProcessType(DWORD cpt);
};

#endif // PACKAGEUTILS_H
