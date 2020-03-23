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

    /**
     * @param hk root key
     * @param path registry path
     * @param err error message will be stored here
     * @param keyExists true will be stored here if the registry key exists
     * @return list of repositories in the specified registry key
     */
    static QStringList getRepositoryURLs(HKEY hk, const QString &path,
                                         QString *err, bool* keyExists);
public:
    /** true = install programs globally, false = locally */
    static bool globalMode;

    /**
     * @brief parses the command line and returns the list of chosen package
     *     versions
     * @param dbr DB-Repository
     * @param cl command line
     * @param err errors will be stored here
     * @return [move] list of package versions
     */
    static QList<PackageVersion *> getAddPackageVersionOptions(const DBRepository& dbr, const CommandLine &cl, QString *err);

    /**
     * @brief parses the command line and returns the list of chosen package
     *     versions
     * @param cl command line
     * @param err errors will be stored here
     * @return [move] list of package versions ranges
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

    /**
     * @param err error message will be stored here
     * @return newly created list of repositories
     */
    static QList<QUrl*> getRepositoryURLs(QString *err);

    /*
     * Changes the default repository url.
     *
     * @param urls new URLs
     * @param err error message will be stored here
     */
    static void setRepositoryURLs(QList<QUrl*>& urls, QString *err);
};

#endif // PACKAGEUTILS_H
