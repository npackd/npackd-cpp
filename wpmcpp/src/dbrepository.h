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
#include "mysqlquery.h"

/**
 * @brief A repository stored in an SQLite database.
 */
class DBRepository: public AbstractRepository
{
private:
    static DBRepository def;
    static bool tableExists(QSqlDatabase* db,
            const QString& table, QString* err);
    static bool columnExists(QSqlDatabase *db, const QString &table,
            const QString &column, QString *err);
    static QString toString(const QSqlError& e);

    QCache<QString, License> licenses;

    QMap<int, QString> categories;

    MySQLQuery* savePackageVersionQuery;
    MySQLQuery* insertPackageQuery;
    MySQLQuery* replacePackageQuery;
    MySQLQuery* selectCategoryQuery;

    QSqlDatabase db;

    QString readCategories();
    QString getCategoryPath(int c0, int c1, int c2, int c3, int c4) const;
    int insertCategory(int parent, int level,
            const QString &category, QString *err);
    QString findCategory(int cat) const;

    /**
     * @brief reads all stored package versions from the database
     * @param package full package name
     * @param err error will be stored here
     * @return *unsorted* versions. Only the version number and the URL are
     *     valid.
     */
    QList<PackageVersion*> getPackageVersions2(const QString &package,
            QString *err) const;

    QList<Package*> findPackagesWhere(const QString &where,
            const QList<QVariant> &params, QString *err) const;

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

    /**
     * Loads the content from the URLs. None of the packages has the information
     * about installation path after this method was called.
     *
     * @param job job for this method
     * @param useCache true = cache will be used
     */
    void load(Job *job, bool useCache);

    void loadOne(QUrl *url, Job *job, bool useCache);

    int count(const QString &sql, QString *err);
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

    /**
     * @brief inserts or updates an existing license
     * @param p a license
     * @param replace what to do if an entry already exists:
     *     true = replace, false = ignore
     * @return error message
     */
    QString saveLicense(License* p, bool replace);

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

    virtual ~DBRepository();

    /**
     * @brief opens the default database as "default"
     * @return error
     */
    QString openDefault();

    /**
     * @brief opens the database
     * @param connectionName name for the database connection
     * @param file database file
     * @return error
     */
    QString open(const QString &connectionName, const QString &file);

    /**
     * @brief update the status for the specified package
     *     (see Package::Status)
     *
     * @param package full package name
     * @return error message
     */
    QString updateStatus(const QString &package);

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
     * @param job job
     */
    void updateStatusForInstalled(Job *job);

    Package* findPackage_(const QString& name);

    QList<PackageVersion*> getPackageVersions_(const QString& package,
            QString *err) const;

    /**
     * @brief returns all package versions with at least one <detect-file>
     *     entry
     * @param err error message will be stored here
     * @return [owner:caller] list of package versions sorted by full package
     *     name and version
     */
    QList<PackageVersion*> getPackageVersionsWithDetectFiles(
            QString *err) const;

    /**
     * @brief searches for packages that match the specified keywords
     * @param status filter for the package status if filterByStatus is true
     * @param statusInclude true = only return packages with the given status,
     *     false = return all packages with the status not equal to the given
     * @param query search query (keywords)
     * @param cat0 filter for the level 0 of categories. -1 means "All",
     *     0 means "Uncategorized"
     * @param cat1 filter for the level 1 of categories. -1 means "All",
     *     0 means "Uncategorized"
     * @param err error message will be stored here
     * @return [ownership:caller] found packages
     */
    QList<Package*> findPackages(Package::Status status, bool filterByStatus,
            const QString &query, int cat0, int cat1, QString* err) const;

    /**
     * @brief loads does all the necessary updates when F5 is pressed. The
     *    repositories from the Internet are loaded and the MSI database and
     *    "Software" control panel data will be scanned.
     * @param job job
     */
    void updateF5(Job *job);

    QString savePackage(Package* p);

    QString savePackageVersion(PackageVersion* p);

    QString saveLicense(License* p);

    PackageVersion* findPackageVersionByMSIGUID_(
            const QString& guid, QString *err) const;

    PackageVersion* findPackageVersion_(const QString& package,
            const Version& version, QString *err);

    License* findLicense_(const QString& name, QString* err);

    QString clear();

    QList<Package*> findPackagesByShortName(const QString &name);

    /**
     * @brief searches for packages that match the specified keywords
     * @param status filter for the package status if filterByStatus is true
     * @param statusInclude true = only return packages with the given status,
     *     false = return all packages with the status not equal to the given
     * @param query search query (keywords)
     * @param level level for the categories (0, 1, ...)
     * @param cat0 filter for the level 0 of categories. -1 means "All",
     *     0 means "Uncategorized"
     * @param cat1 filter for the level 1 of categories. -1 means "All",
     *     0 means "Uncategorized"
     * @param err error message will be stored here
     * @return categories for found packages: ID, COUNT, NAME. One category may
     *     have all values empty showing all un-categorized packages.
     */
    QList<QStringList> findCategories(Package::Status status,
            bool filterByStatus, const QString &query, int level, int cat0, int cat1, QString *err) const;

    /**
     * @brief converts category IDs in titles
     * @param ids CATEGORY.ID
     * @param err error message will be stored here
     * @return category titles
     */
    QStringList getCategories(const QStringList &ids, QString *err);

    /**
     * @brief reads the list of repositories
     * @param err error message will be stored here
     * @return list of URLs
     */
    QStringList readRepositories(QString *err);

    /**
     * @brief saves the list of given repository URLs. The repositories will
     *     get the IDs 1, 2, 3, ...
     * @param reps URLs
     * @return error message
     */
    QString saveRepositories(const QStringList& reps);
};

#endif // DBREPOSITORY_H
