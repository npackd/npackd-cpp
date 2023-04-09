#ifndef PACKAGEUTILS_H
#define PACKAGEUTILS_H

#include <tuple>

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
     * @return list of repositories in the specified registry key, list of comments,
     * error message, whether the registry key exists
     */
    static std::tuple<std::vector<QString>, std::vector<QString>, bool, QString> getRepositoryURLs(
            HKEY hk, const QString &path);
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
    static std::vector<PackageVersion *> getAddPackageVersionOptions(const DBRepository& dbr, const CommandLine &cl, QString *err);

    /**
     * @brief parses the command line and returns the list of chosen package
     *     versions
     * @param cl command line
     * @param err errors will be stored here
     * @return [move] list of package versions ranges
     */
    static std::vector<Dependency *> getPackageVersionOptions(const CommandLine &cl,
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
     * @return [move] newly created list of repositories
     */
    static std::vector<QUrl*> getRepositoryURLs(QString *err);

    /**
     * @param err error message will be stored here
     * @param used true = used repositories, false = unused repositories
     * @return repositories, comments and an error message.
     * Repositories and comments list have the same length.
     */
    static std::tuple<std::vector<QString>, std::vector<QString>, QString> getRepositoryURLsAndComments(bool used=true);

    /*
     * Changes the default repository url.
     *
     * @param urls new URLs
     * @param err error message will be stored here
     */
    static void setRepositoryURLs(std::vector<QUrl *> &urls, QString *err);

    /*
     * Changes the default repository url.
     *
     * @param urls new URLs
     * @param comments corresponding comments
     * @return error message
     */
    static QString setRepositoryURLsAndComments(const std::vector<QString>& urls, const std::vector<QString>& comments, bool used=true);
};

#endif // PACKAGEUTILS_H
