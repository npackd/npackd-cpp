#ifndef DBREPOSITORY_H
#define DBREPOSITORY_H

#include <QString>
#include <QSqlError>
#include <QSqlDatabase>
#include <QSharedPointer>
#include <QMap>
#include <QWeakPointer>
#include <QMultiMap>
#include <QCache>

#include "package.h"
#include "repository.h"
#include "packageversion.h"
#include "license.h"
#include "abstractrepository.h"

/**
 * @brief A repository stored in an SQLite database.
 */
class DBRepository: public AbstractRepository
{
private:
    static DBRepository def;
    static bool tableExists(QSqlDatabase* db,
            const QString& table, QString* err);
    static QString toString(const QSqlError& e);

    QCache<QString, License> licenses;

    /**
     * @brief inserts or updates an existing license
     * @param p a license
     * @param replace what to do if an entry already exists:
     *     true = replace, false = ignore
     * @return error message
     */
    QString saveLicense(License* p, bool replace);

    /**
     * @brief inserts or updates existing packages
     * @param r repository with packages
     * @param replace what to do if an entry already exists:
     *     true = replace, false = ignore
     * @return error message
     */
    QString savePackages(Repository* r, bool replace);

    /**
     * @brief inserts or updates existing package versions
     * @param r repository with package versions
     * @param replace what to do if an entry already exists:
     *     true = replace, false = ignore
     * @return error message
     */
    QString savePackageVersions(Repository* r, bool replace);

    /**
     * @brief inserts or updates existing licenses
     * @param r repository with licenses
     * @param replace what to do if an entry already exists:
     *     true = replace, false = ignore
     * @return error message
     */
    QString saveLicenses(Repository* r, bool replace);

    QString exec(const QString& sql);

    QString updateStatus(const QString &package);

    /**
     * @brief inserts or updates an existing package version
     * @param p a package version
     * @param replace what to do if an entry already exists:
     *     true = replace, false = ignore
     * @return error message
     */
    QString savePackageVersion(PackageVersion *p, bool replace);

    /**
     * @brief inserts or updates an existing package
     * @param p a package
     * @param replace what to do if an entry already exists:
     *     true = replace, false = ignore
     * @return error message
     */
    QString savePackage(Package *p, bool replace);
public:
    /**
     * @return default repository
     * @threadsafe
     */
    static DBRepository* getDefault();

    /**
     * @brief -
     */
    DBRepository();

    virtual ~DBRepository();

    /**
     * @brief opens the database
     * @return error
     */
    QString open();

    /**
     * @brief inserts the data from the given repository
     * @param job job
     * @param r the repository
     * @param replace what to to if an entry already exists:
     *     true = replace, false = ignore
     * @return error message
     */
    void saveAll(Job* job, Repository* r, bool replace=false);

    /**
     * @brief updates the status for currently installed packages in
     *     PACKAGE.STATUS
     */
    void updateStatusForInstalled();

    Package* findPackage_(const QString& name);

    QList<PackageVersion*> getPackageVersions_(const QString& package,
            QString *err) const;

    /**
     * @brief searches for packages that match the specified keywords
     * @param status filter for the package status if filterByStatus is true
     * @param statusInclude true = only return packages with the given status,
     *     false = return all packages with the status not equal to the given
     * @param query search query (keywords)
     * @param err error message will be stored here
     * @return [ownership:caller] found packages
     */
    QList<Package*> findPackages(Package::Status status, bool filterByStatus,
            const QString &query, QString* err) const;

    /**
     * @brief loads does all the necessary updates when F5 is pressed. The
     *    repositories from the Internet are loaded and the MSI database and
     *    "Software" control panel data will be scanned.
     * @param job job
     */
    void updateF5(Job *job);

    QString savePackage(Package* p);

    QString savePackageVersion(PackageVersion* p);

    PackageVersion* findPackageVersionByMSIGUID_(
            const QString& guid, QString *err) const;

    PackageVersion* findPackageVersion_(const QString& package,
            const Version& version, QString *err);

    License* findLicense_(const QString& name, QString* err);

    QString clear();

    QList<Package*> findPackagesByShortName(const QString &name);
};

#endif // DBREPOSITORY_H
