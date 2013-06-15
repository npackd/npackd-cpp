#ifndef ABSTRACTREPOSITORY_H
#define ABSTRACTREPOSITORY_H

#include <windows.h>

#include <QList>
#include <QString>

#include "packageversion.h"
#include "package.h"
#include "license.h"
#include "installoperation.h"

/**
 * @brief basis for repositories
 */
class AbstractRepository
{
private:
    static AbstractRepository* def;

    /**
     * @param hk root key
     * @param path registry path
     * @param err error message will be stored here
     * @return list of repositories in the specified registry key
     */
    static QStringList getRepositoryURLs(HKEY hk, const QString &path,
            QString *err);

    /**
     * All paths should be in lower case
     * and separated with \ and not / and cannot end with \.
     *
     * @param path directory
     * @param ignore ignored directories
     * @threadsafe
     */
    void scan(const QString& path, Job* job, int level, QStringList& ignore);
public:
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

    /**
     * @return default repository
     */
    static AbstractRepository* getDefault_();

    /**
     * @param d default repository
     */
    static void setDefault_(AbstractRepository* d);

    /**
     * @brief creates a new instance
     */
    AbstractRepository();

    virtual ~AbstractRepository();

    /**
     * @brief searches for a package with the given short name
     * @param name full package name
     * @return [ownership:caller] found packages.
     */
    virtual QList<Package*> findPackagesByShortName(const QString& name) = 0;

    /**
     * @brief searches for a package with the given name
     * @param name full package name
     * @return found package or 0. The returned object should be destroyed later.
     */
    virtual Package* findPackage_(const QString& name) = 0;

    /**
     * Finds all package versions.
     *
     * @param package full package name
     * @param err error message will be stored here
     * @return [ownership:caller] the list of package versions.
     *     The first returned object has the highest version number.
     */
    virtual QList<PackageVersion*> getPackageVersions_(
            const QString& package, QString* err) const = 0;

    /**
     * Find the newest installed package version.
     *
     * @param name name of the package like "org.server.Word"
     * @param err error message will be stored here
     * @return [ownership:caller] found package version or 0
     */
    PackageVersion *findNewestInstalledPackageVersion_(
            const QString &name, QString* err) const;

    /**
     * Find the newest installable package version.
     *
     * @param package name of the package like "org.server.Word"
     * @return found package version or 0. The returned object should be
     *     destroyed later.
     */
    PackageVersion* findNewestInstallablePackageVersion_(
            const QString& package) const;

    /**
     * @return new NPACKD_CL value
     */
    QString computeNpackdCLEnvVar_() const;

    /**
     * Changes the value of the system-wide NPACKD_CL variable to point to the
     * newest installed version of NpackdCL.
     */
    void updateNpackdCLEnvVar();

    /**
     * @brief processes the given operatios
     * @param job job
     * @param install operations that should be performed
     *
     * TODO: this method is not thread-safe
     */
    void process(Job* job, const QList<InstallOperation*> &install);

    /**
     * Scans the hard drive for existing applications.
     *
     * @param job job for this method
     * @threadsafe
     */
    void scanHardDrive(Job* job);

    /**
     * Finds all installed package versions.
     *
     * @param err error message will be stored here
     * @return [ownership:caller] the list of installed package versions
     */
    QList<PackageVersion*> getInstalled_(QString* err);

    /**
     * Plans updates for the given packages.
     *
     * @param packages these packages should be updated. No duplicates are
     *     allowed here
     * @param ops installation operations will be appended here
     * @return error message or ""
     */
    QString planUpdates(const QList<Package*> packages,
            QList<InstallOperation*>& ops);

    /**
     * @brief saves (creates or updates) the data about a package
     * @param p [ownership:caller] package
     * @return error message
     */
    virtual QString savePackage(Package* p) = 0;

    /**
     * @brief saves (creates or updates) the data about a package
     * @param p [ownership:caller] package version
     * @return error message
     */
    virtual QString savePackageVersion(PackageVersion* p) = 0;

    /**
     * @brief searches for a package version by the associated MSI GUID
     * @param guid MSI package GUID
     * @param err error message will be stored here
     * @return [ownership:new] found package version or 0
     */
    virtual PackageVersion* findPackageVersionByMSIGUID_(
            const QString& guid, QString* err) const = 0;

    /**
     * Find the newest available package version.
     *
     * @param package name of the package like "org.server.Word"
     * @param version package version
     * @param err error message will be stored here
     * @return [ownership:caller] found package version or 0
     * TODO: this function now returns an error
     */
    virtual PackageVersion* findPackageVersion_(const QString& package,
            const Version& version, QString* err) = 0;

    /**
     * Searches for a license by name.
     *
     * @param name name of the license like "org.gnu.GPLv3"
     * @param err error message will be stored here
     * @return [ownership:caller] found license or 0
     */
    virtual License* findLicense_(const QString& name, QString* err) = 0;

    /**
     * @brief removes all package, version and license definitions
     * @return error message
     */
    virtual QString clear() = 0;
};

#endif // ABSTRACTREPOSITORY_H
