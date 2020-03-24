#ifndef DBREPOSITORY_H
#define DBREPOSITORY_H

#include <memory>

#include <QString>
#include <QSqlError>
#include <QSqlDatabase>
#include <QSharedPointer>
#include <QMap>
#include <QWeakPointer>
#include <QMultiMap>
#include <QCache>
#include <QList>
#include <QMutex>

#include "package.h"
#include "repository.h"
#include "packageversion.h"
#include "license.h"
#include "abstractrepository.h"
#include "mysqlquery.h"
#include "installedpackageversion.h"
#include "urlinfo.h"

/**
 * @brief A repository stored in an SQLite database.
 */
class DBRepository: public AbstractRepository
{
private:
    class PackageVersionList {
    public:
        QList<PackageVersion*> data;

        virtual ~PackageVersionList();
    };

    static DBRepository def;

    bool tableExists(QSqlDatabase* db,
            const QString& table, QString* err);
    bool columnExists(QSqlDatabase *db, const QString &table,
            const QString &column, QString *err);
    static QString toString(const QSqlError& e);
    static QString getErrorString(const MySQLQuery& q);

    mutable QMutex mutex;

    QCache<QString, License> licenses;
    mutable QCache<QString, PackageVersionList> packageVersions;
    mutable QCache<QString, Package> packages;

    QMap<int, QString> categories;

    MySQLQuery* replacePackageVersionQuery;
    MySQLQuery* insertPackageVersionQuery;
    std::unique_ptr<MySQLQuery> insertCmdFileQuery;
    MySQLQuery* insertPackageQuery;
    MySQLQuery* replacePackageQuery;
    MySQLQuery* selectCategoryQuery;
    MySQLQuery* insertLinkQuery;
    MySQLQuery* deleteLinkQuery;
    std::unique_ptr<MySQLQuery> insertTagQuery;
    std::unique_ptr<MySQLQuery> deleteTagQuery;
    std::unique_ptr<MySQLQuery> deleteCmdFilesQuery;
    MySQLQuery* insertInstalledQuery;
    MySQLQuery* insertURLSizeQuery;

    QStringList stopWords;

    QSqlDatabase db;

    QString readCategories();
    QString getCategoryPath(int c0, int c1, int c2, int c3, int c4) const;
    int insertCategory(int parent, int level,
            const QString &category, QString *err);
    QString findCategory(int cat) const;

    /**
     * @brief findPackagesWhere
     * @param sql "SELECT NAME FROM PACKAGE ORDER BY TITLE"
     * @param params
     * @param err
     * @return
     */
    QStringList findPackagesWhere(const QString &sql,
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
     * @param repositories URLs for the repositories
     * @param useCache true = cache will be used
     * @param interactive true = allow the interaction with the user
     * @param user user name for the HTTP authentication or ""
     * @param password password for the HTTP authentication or ""
     * @param user user name for the HTTP proxy authentication or ""
     * @param password password for the HTTP proxy authentication or ""
     */
    void load(Job *job, const QList<QUrl *>& repositories, bool useCache, bool interactive, const QString& user,
            const QString& password,
            const QString& proxyUser, const QString& proxyPassword);

    /**
     * @brief loadOne
     * @param job
     * @param f
     * @param url URL of the repository. This value will be used for resolving
     *     relative URLs.
     */
    void loadOne(Job *job, QFile *f, const QUrl &url);

    int count(const QString &sql, QString *err);
    QString getRepositorySHA1(const QString &url, QString *err);
    void setRepositorySHA1(const QString &url, const QString &sha1, QString *err);
    QString clearRepository(int id);
    QString saveLinks(Package *p);
    QString readLinks(Package *p) const;
    QString deleteLinks(const QString &name);
    QString updateDatabase();
    void transferFrom(Job *job, const QString &databaseFilename);
    QString deleteCmdFiles(const QString &name, const Version &version);
    QStringList tokenizeTitle(const QString &title);
    QString deleteTags(const QString &name);
    QString saveTags(Package *p);
    QString readTags(Package *p) const;
    QString createQuery(Package::Status minStatus, Package::Status maxStatus,
            const QString &query, int cat0, int cat1, QList<QVariant> &params) const;
public:
    /** index of the current repository used for saving the packages */
    int currentRepository;

    /**
     * @return default repository. This repository should only be used form the
     *     main UI thread or from the main thread of the command line
     *     application.
     */
    static DBRepository* getDefault();

    using AbstractRepository::toString;

    /**
     * @brief -
     */
    DBRepository();

    virtual ~DBRepository();

    /**
     * @brief replaces the list of installed package versions with the specified
     * @param installed new list of installed versions
     * @return error message
     */
    QString saveInstalled(const QList<InstalledPackageVersion*>& installed);

    /**
     * @brief saves the download size for an URL
     * @param url URL
     * @param size size of the URL or -1 if unknown or -2 if an error occured
     * @return error message
     */
    QString saveURLSize(const QString& url, int64_t size);

    QString saveLicense(License* p, bool replace) override;

    QString savePackageVersion(PackageVersion *p, bool replace) override;

    QString savePackage(Package *p, bool replace) override;

    /**
     * @brief opens the default database
     * @param databaseName name for the database
     * @param readOnly true = open in read-only mode
     * @return error
     */
    QString openDefault(const QString &databaseName="default",
            bool readOnly=false);

    /**
     * @brief opens the database
     * @param connectionName name for the database connection
     * @param file database file
     * @param readOnly true = open in read-only mode
     * @return error
     */
    QString open(const QString &connectionName, const QString &file,
            bool readOnly=false);

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

    /**
     * @brief reads the download sizes for URLs
     * @param err error message will be stored here
     * @return URL -> info
     */
    QMap<QString, URLInfo*> findURLInfos(QString* err);

    Package* findPackage_(const QString& name) const override;

    QList<PackageVersion*> getPackageVersions_(const QString& package,
            QString *err) const override;

    /**
     * @brief returns all package versions with a <cmd-file> entry with the
     *     specified path
     * @param name name of the command line tool without \ or /
     * @param err error message will be stored here
     * @return [move] list of package versions sorted by full package
     *     name and version
     */
    QList<PackageVersion*> findPackageVersionsWithCmdFile(const QString& name,
            QString *err) const;

    /**
     * @brief searches for packages that match the specified keywords. No filter
     *     by the status will be applied if minStatus >= maxStatus
     * @param minStatus filter for the package status >=
     * @param maxStatus filter for the package status <
     * @param query search query (keywords)
     * @param cat0 filter for the level 0 of categories. -1 means "All",
     *     0 means "Uncategorized"
     * @param cat1 filter for the level 1 of categories. -1 means "All",
     *     0 means "Uncategorized"
     * @param err error message will be stored here
     * @return found packages
     */
    QStringList findPackages(Package::Status minStatus,
            Package::Status maxStatus,
            const QString &query, int cat0, int cat1, QString* err) const;

    /**
     * @brief loads does all the necessary updates when F5 is pressed. The
     *    repositories from the Internet are loaded and the MSI database and
     *    "Software" control panel data will be scanned.
     *
     * @param job job
     * @param repositories URLs for the repositories
     * @param interactive true = allow the interaction with the user
     * @param user user name for the HTTP authentication or ""
     * @param password password for the HTTP authentication or ""
     * @param proxyUser user name for the HTTP proxy authentication or ""
     * @param proxyPassword password for the HTTP proxy authentication or ""
     * @param useCache true = use the HTTP cache
     * @param detect true = detect software
     */
    void clearAndDownloadRepositories(Job *job,
            const QList<QUrl*>& repositories, bool interactive, const QString& user,
            const QString& password,
            const QString& proxyUser, const QString& proxyPassword,
            bool useCache, bool detect=true);

    /**
     * @brief updateF5() that can be used with QtConcurrent::Run
     * @param job job
     * @param useCache true = cache will be used
     */
    void updateF5Runnable(Job* job, bool useCache);

    PackageVersion* findPackageVersion_(const QString& package,
            const Version& version, QString *err) const override;

    License* findLicense_(const QString& name, QString* err) override;

    QString clear() override;

    /**
     * @brief clears the cache
     */
    void clearCache();

    QList<Package*> findPackagesByShortName(const QString &name) const override;

    /**
     * @brief searches for packages that match the specified keywords. No filter
     *     for the status will be applied if minStatus >= maxStatus
     * @param minStatus filter for the package status >=
     * @param maxStatus filter for the package status <
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
    QList<QStringList> findCategories(Package::Status minStatus,
            Package::Status maxStatus, const QString &query,
            int level, int cat0, int cat1, QString *err) const;

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

    /**
     * @brief searches for packages
     * @param names names for the packages
     * @return list of found packages. The order of returned packages does *NOT*
     *     correspond the order in names.
     */
    QList<Package*> findPackages(const QStringList &names);

    /**
     * @brief searches for better packages for detection
     * @param title title of a package
     * @param err error message will be stored here
     * @return list of found packages.
     */
    QStringList findBetterPackages(const QString &title, QString *err);

    /**
     * @return maximum number of stars for a package
     * @param err error message will be stored here
     */
    int getMaxStars(QString *err);
};

#endif // DBREPOSITORY_H
