#ifndef ABSTRACTREPOSITORY_H
#define ABSTRACTREPOSITORY_H

#include <mutex>

#include "stable.h"

#include "packageversion.h"
#include "package.h"
#include "license.h"
#include "threadpool.h"

class InstallOperation;
class InstalledPackages;

/**
 * @brief basis for repositories
 */
class AbstractRepository
{
private:
    static std::mutex installationScripts;
    QString createDirForInstallation(Job &job,
        const PackageVersion &pv, const InstallOperation &op, const QString &dir);
public:
    /** should be used for installation operations */
    static ThreadPool threadPool;

    /**
     * @brief acquire the lock for executing installation/uninstallation scripts.
     * Only one script is allowed to be executed at any time.
     *
     * @param job job
     * @return true = acquired
     */
    static bool lockInstallationScript(Job* job);

    /**
     * @brief release the lock for executing installation/uninstallation scripts.
     * Only one script is allowed to be executed at any time.
     *
     * @param job job
     */
    static void unlockInstallationScript();

    /**
     * @brief creates a new instance
     */
    AbstractRepository();

    virtual ~AbstractRepository();

    /**
     * @brief checks a value for the installation directory
     * @param dir a directory
     * @return error message or ""
     */
    QString checkInstallationDirectory(const QString& dir) const;

    /**
     * @param package full or short package name
     * @param err error message will be stored here
     * @return [move] found package or 0. The returned value is
     *     only 0 if the error is not empty
     */
    Package *findOnePackage(const QString &package, QString *err) const;

    /**
     * @brief inserts or updates an existing license
     * @param p a license
     * @param replace what to do if an entry already exists:
     *     true = replace, false = ignore
     * @return error message
     */
    virtual QString saveLicense(License* p, bool replace) = 0;

    /**
     * @brief inserts or updates an existing package version
     * @param p a package version
     * @param replace what to do if an entry already exists:
     *     true = replace, false = ignore
     * @return error message
     */
    virtual QString savePackageVersion(PackageVersion *p, bool replace) = 0;

    /**
     * @brief inserts or updates an existing package
     * @param p a package
     * @param replace what to do if an entry already exists:
     *     true = replace, false = ignore
     * @return error message
     */
    virtual QString savePackage(Package *p, bool replace) = 0;

    /**
     * @param name
     * @return package title and name. Example: "AbiWord (com.abiword.AbiWord)"
     */
    QString getPackageTitleAndName(const QString& name) const;

    /**
     * @brief searches for a package with the given short name
     * @param name full package name
     * @return [move] found packages.
     */
    virtual std::vector<Package*> findPackagesByShortName(const QString& name) const = 0;

    /**
     * @brief searches for a package with the given name
     * @param name full package name
     * @return [move] found package or 0.
     */
    virtual Package* findPackage_(const QString& name) const = 0;

    /**
     * Finds all package versions.
     *
     * @param package full package name
     * @param err error message will be stored here
     * @return [move] the list of package versions.
     *     The first returned object has the highest version number.
     */
    virtual std::vector<PackageVersion*> getPackageVersions_(
            const QString& package, QString* err) const = 0;

    /**
     * Find the newest installed package version.
     *
     * @param name name of the package like "org.server.Word"
     * @param err error message will be stored here
     * @return [move] found package version or 0
     */
    PackageVersion *findNewestInstalledPackageVersion_(
            const QString &name, QString* err) const;

    /**
     * Find the newest installable package version.
     *
     * @param package name of the package like "org.server.Word"
     * @param err error message will be stored here
     * @return found package version or 0. The returned object should be
     *     destroyed later.
     */
    PackageVersion* findNewestInstallablePackageVersion_(const QString& package,
                                                         QString *err) const;

    /**
     * @brief processes the given operations
     * @param job job
     * @param install operations that should be performed
     * @param opsDependencies dependencies between installation operations. The
     *     nodes of the graph are indexes in the "ops" vector. An edge from "u"
     *     to "v" means that "u" depends on "v".
     * @param programCloseType how to close running applications
     * @param printScriptOutput true = redirect the script output to the
     *     standard output
     * @param interactive true = allow the interaction with the user
     * @param user user name for the HTTP authentication
     * @param password password for the HTTP authentication
     * @param proxyUser user name for the HTTP proxy authentication
     * @param proxyPassword password for the HTTP proxy authentication
     */
    void process(Job* job, const std::vector<InstallOperation *> &install,
            const DAG &opsDependencies,
            DWORD programCloseType, bool printScriptOutput,
            bool interactive,
            const QString &user, const QString &password,
            const QString &proxyUser, const QString &proxyPassword);

    /**
     * Finds all installed package versions.
     *
     * @param err error message will be stored here
     * @return [move] the list of installed package versions
     */
    std::vector<PackageVersion *> getInstalled_(QString* err);

    /**
     * Plans updates for the given packages.
     *
     * @param installed list of installed packages. This object will be modified
     * @param packages these packages should be updated. No duplicates are
     *     allowed here
     * @param ranges these package versions should be updated. No duplicates are
     *     allowed here. Only the versions in the specified range are considered
     *     for an update. This means that the newest installed version from a
     *     range will be uninstalled and the newest available in the range will
     *     be installed.
     * @param ops installation operations will be appended here
     * @param opsDependencies dependencies between installation operations. The
     *     nodes of the graph are indexes in the "ops" vector. An edge from "u"
     *     to "v" means that "u" depends on "v".
     * @param keepDirectories true = use the same directories for the updated
     *     versions
     * @param install this determines the behaviour if no version is installed.
     *     If true, installs the newest version. If false, an error will be
     *     returned.
     * @param where_ where to install the new version. This parameter will only
     *     be used if only one package should be updated
     * @param exactLocation if keepDirectories=true or "where" is not empty:
     *     true = fail if a package cannot be updated in the exact same location
     *     false = choose a similar location like "Notepad_2.81" instead of
     *     "Notepad"
     * @return error message or ""
     */
    QString planUpdates(InstalledPackages &installed,
            const std::vector<Package *> &packages,
            std::vector<Dependency *> ranges,
            std::vector<InstallOperation *> &ops, DAG &opsDependencies,
            bool keepDirectories=false,
            bool install=false, const QString& where_="",
            bool exactLocation=true);

    /**
     * Plans installation of this package and all the dependencies recursively.
     *
     * @param installed list of installed packages.
     *     This list should be
     *     consulted instead of .installed() and will be updated and contains
     *     all installed package versions after the process
     * @param me package version that will be installed
     * @param op necessary operations will be appended here.
     *     The existing
     *     elements will not be modified in any way.
     * @param opsDependencies dependencies between installation operations. The
     *     nodes of the graph are indexes in the "ops" vector. An edge from "u"
     *     to "v" means that "u" depends on "v".
     * @param avoid list of package versions that cannot be
     *     installed. The list
     *     will be changed by this method. Normally this is an empty list and
     *     objects will be added to it on different recursion levels.
     * @param where target directory for the installation or "" if the
     *     directory should be chosen automatically
     * @return error message or ""
     */
    QString planInstallation(InstalledPackages& installed,
            PackageVersion* me,
            std::vector<InstallOperation*>& ops, DAG &opsDependencies,
            std::vector<PackageVersion*>& avoid,
            const QString &where="");

    /**
     * Plans un-installation of a package version and all the dependent
     * recursively.
     *
     * @param installed list of installed packages. This list should be
     *     consulted instead of .installed() and will be updated and contains
     *     all installed package versions after the process. The list will also
     *     be updated to reflect packages "uninstalled" by this method
     * @param package full package name
     * @param version version number to be uninstalled
     * @param op necessary operations will be added here. The existing
     *     elements will not be modified in any way.
     * @param opsDependencies dependencies between installation operations. The
     *     nodes of the graph are indexes in the "ops" vector. An edge from "u"
     *     to "v" means that "u" depends on "v".
     * @return error message or ""
     */
    QString planUninstallation(InstalledPackages& installed,
            const QString& package, const Version& version,
            std::vector<InstallOperation *> &ops, DAG &opsDependencies);

    /**
     * Find the newest available package version.
     *
     * @param package name of the package like "org.server.Word"
     * @param version package version
     * @param err error message will be stored here
     * @return [move] found package version or 0
     */
    virtual PackageVersion* findPackageVersion_(const QString& package,
            const Version& version, QString* err) const = 0;

    /**
     * Searches for a license by name.
     *
     * @param name name of the license like "org.gnu.GPLv3"
     * @param err error message will be stored here
     * @return [move] found license or 0
     */
    virtual License* findLicense_(const QString& name, QString* err) = 0;

    /**
     * @brief removes all package, version and license definitions
     * @return error message
     */
    virtual QString clear() = 0;

    /**
     * @brief checks whether the given operations include removing the current
     *     running Npackd or NpackdCL instance
     * @param install_ operations
     * @return true = the operation for removing the current running Npackd
     *     or NpackdCL instance is included
     */
    bool includesRemoveItself(const std::vector<InstallOperation *> &install_);

    /**
     * @param dep a dependency
     * @param avoid list of package versions that should be avoided and cannot
     *     be considered to be a match
     * @param err error message will be stored here
     * @return [move] all package versions that matches this
     *     dependency by
     *     being installed. Returned objects should be destroyed later.
     *     The returned objects are sorted by the package version number. The
     *     first returned object has the highest version number.
     */
    std::vector<PackageVersion *> findAllMatchesToInstall(const Dependency& dep,
            const std::vector<PackageVersion *> &avoid, QString *err);

    /**
     * @param dep a dependency
     * @param avoid list of package versions that should be avoided and cannot
     *     be considered to be a match
     * @param err error message will be stored here
     * @return [move] the newest package version that matches this
     *     dependency by
     *     being installed. Returned object should be destroyed later.
     */
    PackageVersion* findBestMatchToInstall(const Dependency& dep,
                                           const std::vector<PackageVersion *> &avoid,
                                           QString *err) const;

    /**
     * @param dep a dependency
     * @param includeFullPackageName true = the full package name will be added
     * @return human readable representation of this dependency
     */
    QString toString(const Dependency& dep, bool includeFullPackageName=false);

    /**
     * @brief exports the specified package versions to a directory
     * @param job job object
     * @param pvs package versions. These objects will be freed.
     * @param where output directory
     * @param def what should be exported: 0..3
     */
    void exportPackagesCoInitializeAndFree(Job *job,
            const std::vector<PackageVersion *> &pvs, const QString &where, int def);

    /**
     * @brief exports the settting for the specified packages to a file
     * @param job job object
     * @param packages packages. These objects will be freed.
     * @param filename output file name
     */
    void exportPackageSettingsCoInitializeAndFree(Job *job,
            const std::vector<Package*> &packages, const QString &filename);

    /**
     * @brief imports the settting from a file
     * @param job job object
     * @param filename input file name
     */
    void importPackageSettingsCoInitializeAndFree(Job *job,
            const QString &filename);

    /**
     * @brief plans adding missing dependencies
     *
     * @param installed list of installed packages. This object will be
     *      modified.
     * @param ops installation operations will be appended here
     * @param opsDependencies dependencies between installation operations. The
     *     nodes of the graph are indexes in the "ops" vector. An edge from "u"
     *     to "v" means that "u" depends on "v".
     * @return error message or ""
     */
    QString planAddMissingDeps(InstalledPackages &installed,
                               std::vector<InstallOperation *> &ops, DAG &opsDependencies);
};

#endif // ABSTRACTREPOSITORY_H

