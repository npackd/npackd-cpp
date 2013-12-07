#include "dbrepository.h"

#include <shlobj.h>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDir>
#include <QVariant>
#include <QDomDocument>
#include <QDomElement>
#include <QTextStream>
#include <QByteArray>
#include <QDebug>
#include <QXmlStreamWriter>
#include <QSqlRecord>

#include "package.h"
#include "repository.h"
#include "packageversion.h"
#include "wpmutils.h"
#include "installedpackages.h"
#include "hrtimer.h"
#include "mysqlquery.h"
#include "repositoryxmlhandler.h"
#include "downloader.h"

static bool packageVersionLessThan3(const PackageVersion* a,
        const PackageVersion* b)
{
    int r = a->package.compare(b->package);
    if (r == 0) {
        r = a->version.compare(b->version);
    }

    return r > 0;
}

DBRepository DBRepository::def;

DBRepository::DBRepository()
{
    savePackageVersionQuery = 0;
    savePackageQuery = 0;
    selectCategoryQuery = 0;
}

DBRepository::~DBRepository()
{
    delete selectCategoryQuery;
    delete savePackageQuery;
    delete savePackageVersionQuery;
}

DBRepository* DBRepository::getDefault()
{
    return &def;
}

QString DBRepository::exec(const QString& sql)
{
    MySQLQuery q;
    q.exec(sql);
    return toString(q.lastError());
}

QString DBRepository::saveLicense(License* p, bool replace)
{
    QString err;

    MySQLQuery q;

    QString sql = "INSERT OR ";
    if (replace)
        sql += "REPLACE";
    else
        sql += "IGNORE";
    sql += " INTO LICENSE "
            "(NAME, TITLE, DESCRIPTION, URL)"
            "VALUES(:NAME, :TITLE, :DESCRIPTION, :URL)";
    if (!q.prepare(sql))
        err = toString(q.lastError());

    if (err.isEmpty()) {
        q.bindValue(":NAME", p->name);
        q.bindValue(":TITLE", p->title);
        q.bindValue(":DESCRIPTION", p->description);
        q.bindValue(":URL", p->url);
        if (!q.exec())
            err = toString(q.lastError());
    }

    return err;
}

bool DBRepository::tableExists(QSqlDatabase* db,
        const QString& table, QString* err)
{
    *err = "";

    MySQLQuery q;
    if (!q.prepare("SELECT name FROM sqlite_master WHERE "
            "type='table' AND name=:NAME"))
        *err = toString(q.lastError());

    if (err->isEmpty()) {
        q.bindValue(":NAME", table);
        if (!q.exec())
            *err = toString(q.lastError());
    }

    bool e = false;
    if (err->isEmpty()) {
        e = q.next();
    }

    return e;
}

bool DBRepository::columnExists(QSqlDatabase* db,
        const QString& table, const QString& column, QString* err)
{
    *err = "";

    MySQLQuery q;
    if (!q.exec("PRAGMA table_info(" + table + ")"))
        *err = toString(q.lastError());

    bool e = false;
    if (err->isEmpty()) {
        int index = q.record().indexOf("name");
        while (q.next()) {
            QString n = q.value(index).toString();
            if (QString::compare(n, column, Qt::CaseInsensitive) == 0) {
                e = true;
                break;
            }
        }
    }

    return e;
}

Package *DBRepository::findPackage_(const QString &name)
{
    QString err;

    Package* r = 0;

    MySQLQuery q;
    if (!q.prepare("SELECT NAME, TITLE, URL, ICON, "
            "DESCRIPTION, LICENSE, CATEGORY0, CATEGORY1, CATEGORY2, "
            "CATEGORY3, CATEGORY4 "
            "FROM PACKAGE WHERE NAME = :NAME"))
        err = toString(q.lastError());

    if (err.isEmpty()) {
        q.bindValue(":NAME", name);
        if (!q.exec())
            err = toString(q.lastError());
    }

    if (err.isEmpty() && q.next()) {
        Package* p = new Package(q.value(0).toString(), q.value(1).toString());
        p->url = q.value(2).toString();
        p->icon = q.value(3).toString();
        p->description = q.value(4).toString();
        p->license = q.value(5).toString();
        QString path = getCategoryPath(q.value(6).toInt(),
                q.value(7).toInt(),
                q.value(8).toInt(),
                q.value(9).toInt(),
                q.value(10).toInt());
        if (!path.isEmpty())
            p->categories.append(path);

        r = p;
    }

    return r;
}

QString DBRepository::findCategory(int cat) const
{
    return categories.value(cat);
}

PackageVersion* DBRepository::findPackageVersion_(
        const QString& package, const Version& version, QString* err)
{
    *err = "";

    Version v = version;
    v.normalize();
    QString version_ = v.getVersionString();
    PackageVersion* r = 0;

    MySQLQuery q;
    if (!q.prepare("SELECT NAME, "
            "PACKAGE, CONTENT, MSIGUID FROM PACKAGE_VERSION "
            "WHERE NAME = :NAME AND PACKAGE = :PACKAGE"))
        *err = toString(q.lastError());

    if (err->isEmpty()) {
        q.bindValue(":NAME", version_);
        q.bindValue(":PACKAGE", package);
        if (!q.exec())
            *err = toString(q.lastError());
    }

    if (err->isEmpty() && q.next()) {
        QDomDocument doc;
        int errorLine, errorColumn;
        if (!doc.setContent(q.value(2).toByteArray(),
                err, &errorLine, &errorColumn))
            *err = QString(
                    QObject::tr("XML parsing failed at line %1, column %2: %3")).
                    arg(errorLine).arg(errorColumn).arg(*err);

        if (err->isEmpty()) {
            QDomElement root = doc.documentElement();
            PackageVersion* p = PackageVersion::parse(&root, err);

            if (err->isEmpty()) {
                r = p;
            }
        }
    }

    return r;
}

QList<PackageVersion*> DBRepository::getPackageVersions_(const QString& package,
        QString *err) const
{
    *err = "";

    QList<PackageVersion*> r;

    MySQLQuery q;
    if (!q.prepare("SELECT CONTENT FROM PACKAGE_VERSION "
            "WHERE PACKAGE = :PACKAGE"))
        *err = toString(q.lastError());

    if (err->isEmpty()) {
        q.bindValue(":PACKAGE", package);
        if (!q.exec()) {
            *err = toString(q.lastError());
        }
    }

    while (err->isEmpty() && q.next()) {
        QDomDocument doc;
        int errorLine, errorColumn;
        if (!doc.setContent(q.value(0).toByteArray(),
                err, &errorLine, &errorColumn)) {
            *err = QString(
                    QObject::tr("XML parsing failed at line %1, column %2: %3")).
                    arg(errorLine).arg(errorColumn).arg(*err);
        }

        QDomElement root = doc.documentElement();

        if (err->isEmpty()) {
            PackageVersion* pv = PackageVersion::parse(&root, err, false);
            if (err->isEmpty())
                r.append(pv);
        }
    }

    // qDebug() << vs.count();

    qSort(r.begin(), r.end(), packageVersionLessThan3);

    return r;
}

QList<PackageVersion *> DBRepository::getPackageVersions2(const QString& package,
        QString *err) const
{
    *err = "";

    QList<PackageVersion*> r;

    MySQLQuery q;
    if (!q.prepare("SELECT NAME, URL FROM PACKAGE_VERSION "
            "WHERE PACKAGE = :PACKAGE"))
        *err = toString(q.lastError());

    if (err->isEmpty()) {
        q.bindValue(":PACKAGE", package);
        if (!q.exec()) {
            *err = toString(q.lastError());
        }
    }

    while (err->isEmpty() && q.next()) {
        QString v = q.value(0).toString();
        QString url = q.value(1).toString();
        Version version;
        if (!version.setVersion(v)) {
            *err = QObject::tr("Read invalid package version from the database: %1").
                    arg(v);
            break;
        } else {
            PackageVersion* pv = new PackageVersion(package, version);
            pv->download.setUrl(url, QUrl::StrictMode);
            r.append(pv);
        }
    }

    return r;
}

QList<PackageVersion *> DBRepository::getPackageVersionsWithDetectFiles(
        QString *err) const
{
    *err = "";

    QList<PackageVersion*> r;

    MySQLQuery q;
    if (!q.prepare("SELECT CONTENT FROM PACKAGE_VERSION "
            "WHERE DETECT_FILE_COUNT > 0"))
        *err = toString(q.lastError());

    if (err->isEmpty()) {
        if (!q.exec()) {
            *err = toString(q.lastError());
        }
    }

    while (err->isEmpty() && q.next()) {
        QDomDocument doc;
        int errorLine, errorColumn;
        if (!doc.setContent(q.value(0).toByteArray(),
                err, &errorLine, &errorColumn)) {
            *err = QString(
                    QObject::tr("XML parsing failed at line %1, column %2: %3")).
                    arg(errorLine).arg(errorColumn).arg(*err);
        }

        QDomElement root = doc.documentElement();

        if (err->isEmpty()) {
            PackageVersion* pv = PackageVersion::parse(&root, err);
            if (err->isEmpty())
                r.append(pv);
        }
    }

    // qDebug() << vs.count();

    qSort(r.begin(), r.end(), packageVersionLessThan3);

    return r;
}

License *DBRepository::findLicense_(const QString& name, QString *err)
{
    *err = "";

    License* r = 0;
    License* cached = this->licenses.object(name);
    if (!cached) {
        MySQLQuery q;

        if (!q.prepare("SELECT NAME, TITLE, DESCRIPTION, URL "
                "FROM LICENSE "
                "WHERE NAME = :NAME"))
            *err = toString(q.lastError());

        if (err->isEmpty()) {
            q.bindValue(":NAME", name);
            if (!q.exec())
                *err = toString(q.lastError());
        }

        if (err->isEmpty()) {
            if (q.next()) {
                cached = new License(name, q.value(1).toString());
                cached->description = q.value(2).toString();
                cached->url = q.value(3).toString();
                r = cached->clone();
                this->licenses.insert(name, cached);
            }
        }
    } else {
        r = cached->clone();
    }

    return r;
}

QList<Package*> DBRepository::findPackages(Package::Status status,
        bool filterByStatus,
        const QString& query, int cat0, int cat1, QString *err) const
{
    // qDebug() << "DBRepository::findPackages.0";

    QString where;
    QList<QVariant> params;

    QStringList keywords = query.toLower().simplified().split(" ",
            QString::SkipEmptyParts);

    for (int i = 0; i < keywords.count(); i++) {
        if (!where.isEmpty())
            where += " AND ";
        where += "FULLTEXT LIKE :FULLTEXT" + QString::number(i);
        params.append(QString("%" + keywords.at(i).toLower() + "%"));
    }
    if (filterByStatus) {
        if (!where.isEmpty())
            where += " AND ";
        if (status == Package::INSTALLED)
            where += "STATUS >= :STATUS";
        else
            where += "STATUS = :STATUS";
        params.append(QVariant((int) status));
    }

    if (cat0 == 0) {
        if (!where.isEmpty())
            where += " AND ";
        where += "CATEGORY0 IS NULL";
    } else if (cat0 > 0) {
        if (!where.isEmpty())
            where += " AND ";
        where += "CATEGORY0 = :CATEGORY0";
        params.append(QVariant((int) cat0));
    }

    if (cat1 == 0) {
        if (!where.isEmpty())
            where += " AND ";
        where += "CATEGORY1 IS NULL";
    } else if (cat1 > 0) {
        if (!where.isEmpty())
            where += " AND ";
        where += "CATEGORY1 = :CATEGORY1";
        params.append(QVariant((int) cat1));
    }

    if (!where.isEmpty())
        where = "WHERE " + where;

    where += " ORDER BY TITLE";

    // qDebug() << "DBRepository::findPackages.1";

    return findPackagesWhere(where, params, err);
}

QStringList DBRepository::getCategories(const QStringList& ids, QString* err)
{
    *err = "";

    QString sql = "SELECT NAME FROM CATEGORY WHERE ID IN (" +
            ids.join(", ") + ")";

    MySQLQuery q;

    if (!q.prepare(sql))
        *err = DBRepository::toString(q.lastError());

    QStringList r;
    if (err->isEmpty()) {
        if (!q.exec())
            *err = toString(q.lastError());
        else {
            while (q.next()) {
                r.append(q.value(0).toString());
            }
        }
    }

    return r;
}

QList<QStringList> DBRepository::findCategories(Package::Status status,
        bool filterByStatus,
        const QString& query, int level, int cat0, int cat1, QString *err) const
{
    // qDebug() << "DBRepository::findPackages.0";

    QString where;
    QList<QVariant> params;

    QStringList keywords = query.toLower().simplified().split(" ",
            QString::SkipEmptyParts);

    for (int i = 0; i < keywords.count(); i++) {
        if (!where.isEmpty())
            where += " AND ";
        where += "FULLTEXT LIKE :FULLTEXT" + QString::number(i);
        params.append(QString("%" + keywords.at(i).toLower() + "%"));
    }
    if (filterByStatus) {
        if (!where.isEmpty())
            where += " AND ";
        if (status == Package::INSTALLED)
            where += "STATUS >= :STATUS";
        else
            where += "STATUS = :STATUS";
        params.append(QVariant((int) status));
    }

    if (cat0 == 0) {
        if (!where.isEmpty())
            where += " AND ";
        where += "CATEGORY0 IS NULL";
    } else if (cat0 > 0) {
        if (!where.isEmpty())
            where += " AND ";
        where += "CATEGORY0 = :CATEGORY0";
        params.append(QVariant((int) cat0));
    }

    if (cat1 == 0) {
        if (!where.isEmpty())
            where += " AND ";
        where += "CATEGORY1 IS NULL";
    } else if (cat1 > 0) {
        if (!where.isEmpty())
            where += " AND ";
        where += "CATEGORY1 = :CATEGORY1";
        params.append(QVariant((int) cat1));
    }

    if (!where.isEmpty())
        where = "WHERE " + where;

    QString sql = QString("SELECT CATEGORY.ID, COUNT(*), CATEGORY.NAME FROM "
            "PACKAGE LEFT JOIN CATEGORY ON PACKAGE.CATEGORY") +
            QString::number(level) +
            " = CATEGORY.ID " +
            where + " GROUP BY CATEGORY.ID, CATEGORY.NAME "
            "ORDER BY CATEGORY.NAME";

    MySQLQuery q;

    if (!q.prepare(sql))
        *err = DBRepository::toString(q.lastError());

    if (err->isEmpty()) {
        for (int i = 0; i < params.count(); i++) {
            q.bindValue(i, params.at(i));
        }
    }

    QList<QStringList> r;
    if (err->isEmpty()) {
        if (!q.exec())
            *err = toString(q.lastError());

        while (q.next()) {
            QStringList sl;
            sl.append(q.value(0).toString());
            sl.append(q.value(1).toString());
            sl.append(q.value(2).toString());
            r.append(sl);
        }
    }

    return r;
}

QList<Package*> DBRepository::findPackagesWhere(const QString& where,
        const QList<QVariant>& params,
        QString *err) const
{
    *err = "";

    QList<Package*> r;

    MySQLQuery q;
    QString sql = "SELECT NAME, TITLE, URL, ICON, "
            "DESCRIPTION, LICENSE, "
            "CATEGORY0, CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4 "
            "FROM PACKAGE";

    if (!where.isEmpty())
        sql += " " + where;

    if (!q.prepare(sql))
        *err = DBRepository::toString(q.lastError());

    if (err->isEmpty()) {
        for (int i = 0; i < params.count(); i++) {
            q.bindValue(i, params.at(i));
        }
    }

    if (err->isEmpty()) {
        if (!q.exec())
            *err = toString(q.lastError());

        while (q.next()) {
            Package* p = new Package(q.value(0).toString(), q.value(1).toString());
            p->url = q.value(2).toString();
            p->icon = q.value(3).toString();
            p->description = q.value(4).toString();
            p->license = q.value(5).toString();
            QString path = getCategoryPath(q.value(6).toInt(),
                    q.value(7).toInt(),
                    q.value(8).toInt(),
                    q.value(9).toInt(),
                    q.value(10).toInt());
            if (!path.isEmpty()) {
                p->categories.append(path);
            }

            r.append(p);
        }
    }

    return r;
}

int DBRepository::insertCategory(int parent, int level,
        const QString& category, QString* err)
{
    *err = "";

    if (!selectCategoryQuery) {
        selectCategoryQuery = new MySQLQuery();

        QString sql = "SELECT ID FROM CATEGORY WHERE PARENT = :PARENT AND "
                "LEVEL = :LEVEL AND NAME = :NAME";

        if (!selectCategoryQuery->prepare(sql)) {
            *err = toString(selectCategoryQuery->lastError());
            delete selectCategoryQuery;
        }
    }

    if (err->isEmpty()) {
        selectCategoryQuery->bindValue(":NAME", category);
        selectCategoryQuery->bindValue(":PARENT", parent);
        selectCategoryQuery->bindValue(":LEVEL", level);
        if (!selectCategoryQuery->exec())
            *err = toString(selectCategoryQuery->lastError());
    }

    int id = -1;
    if (err->isEmpty()) {
        if (selectCategoryQuery->next())
            id = selectCategoryQuery->value(0).toInt();
        else {
            MySQLQuery q;
            QString sql = "INSERT INTO CATEGORY "
                    "(ID, NAME, PARENT, LEVEL) "
                    "VALUES (NULL, :NAME, :PARENT, :LEVEL)";
            if (!q.prepare(sql))
                *err = toString(q.lastError());

            if (err->isEmpty()) {
                q.bindValue(":NAME", category);
                q.bindValue(":PARENT", parent);
                q.bindValue(":LEVEL", level);
                if (!q.exec())
                    *err = toString(q.lastError());
                else
                    id = q.lastInsertId().toInt();
            }
        }
    }

    selectCategoryQuery->finish();

    return id;
}

QString DBRepository::savePackage(Package *p, bool replace)
{
    QString err;

    /*
    if (p->name == "com.microsoft.Windows64")
        qDebug() << p->name << "->" << p->description;
        */

    int cat0 = 0;
    int cat1 = 0;
    int cat2 = 0;
    int cat3 = 0;
    int cat4 = 0;

    if (p->categories.count() > 0) {
        QString category = p->categories.at(0);
        QStringList cats = category.split('/');
        for (int i = 0; i < cats.length(); i++) {
            cats[i] = cats.at(i).trimmed();
        }

        QString c;
        if (cats.count() > 0) {
            c = cats.at(0);
            cat0 = insertCategory(0, 0, c, &err);
        }
        if (cats.count() > 1) {
            c = cats.at(1);
            cat1 = insertCategory(cat0, 1, c, &err);
        }
        if (cats.count() > 2) {
            c = cats.at(2);
            cat2 = insertCategory(cat1, 2, c, &err);
        }
        if (cats.count() > 3) {
            c = cats.at(3);
            cat3 = insertCategory(cat2, 3, c, &err);
        }
        if (cats.count() > 4) {
            c = cats.at(4);
            cat4 = insertCategory(cat3, 4, c, &err);
        }
    }

    if (!savePackageQuery) {
        savePackageQuery = new MySQLQuery();
        QString sql = "INSERT OR ";
        if (replace)
            sql += "REPLACE";
        else
            sql += "IGNORE";
        sql += " INTO PACKAGE "
                "(NAME, TITLE, URL, ICON, DESCRIPTION, LICENSE, FULLTEXT, "
                "STATUS, SHORT_NAME, CATEGORY0, CATEGORY1, CATEGORY2, CATEGORY3,"
                " CATEGORY4)"
                "VALUES(:NAME, :TITLE, :URL, :ICON, :DESCRIPTION, :LICENSE, "
                ":FULLTEXT, :STATUS, :SHORT_NAME, "
                ":CATEGORY0, :CATEGORY1, :CATEGORY2, :CATEGORY3, :CATEGORY4)";
        if (!savePackageQuery->prepare(sql)) {
            err = toString(savePackageQuery->lastError());
            delete savePackageQuery;
        }
    }

    if (err.isEmpty()) {
        savePackageQuery->bindValue(":NAME", p->name);
        savePackageQuery->bindValue(":TITLE", p->title);
        savePackageQuery->bindValue(":URL", p->url);
        savePackageQuery->bindValue(":ICON", p->icon);
        savePackageQuery->bindValue(":DESCRIPTION", p->description);
        savePackageQuery->bindValue(":LICENSE", p->license);
        savePackageQuery->bindValue(":FULLTEXT", (p->title + " " + p->description + " " +
                p->name).toLower());
        savePackageQuery->bindValue(":STATUS", 0);
        savePackageQuery->bindValue(":SHORT_NAME", p->getShortName());
        if (cat0 == 0)
            savePackageQuery->bindValue(":CATEGORY0", QVariant(QVariant::Int));
        else
            savePackageQuery->bindValue(":CATEGORY0", cat0);
        if (cat1 == 0)
            savePackageQuery->bindValue(":CATEGORY1", QVariant(QVariant::Int));
        else
            savePackageQuery->bindValue(":CATEGORY1", cat1);
        if (cat2 == 0)
            savePackageQuery->bindValue(":CATEGORY2", QVariant(QVariant::Int));
        else
            savePackageQuery->bindValue(":CATEGORY2", cat2);
        if (cat3 == 0)
            savePackageQuery->bindValue(":CATEGORY3", QVariant(QVariant::Int));
        else
            savePackageQuery->bindValue(":CATEGORY3", cat3);
        if (cat4 == 0)
            savePackageQuery->bindValue(":CATEGORY4", QVariant(QVariant::Int));
        else
            savePackageQuery->bindValue(":CATEGORY4", cat4);
        if (!savePackageQuery->exec())
            err = toString(savePackageQuery->lastError());

    }

    savePackageQuery->finish();

    return err;
}

QString DBRepository::savePackage(Package *p)
{
    return savePackage(p, true);
}

QString DBRepository::savePackageVersion(PackageVersion *p)
{
    return savePackageVersion(p, true);
}

QString DBRepository::saveLicense(License *p)
{
    return saveLicense(p, true);
}

QList<Package*> DBRepository::findPackagesByShortName(const QString &name)
{
    QString err;

    QList<Package*> r;

    MySQLQuery q;
    if (!q.prepare("SELECT NAME, TITLE, URL, ICON, "
            "DESCRIPTION, LICENSE, CATEGORY0, "
            "CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4 "
            "FROM PACKAGE WHERE SHORT_NAME = :SHORT_NAME"))
        err = toString(q.lastError());

    if (err.isEmpty()) {
        q.bindValue(":SHORT_NAME", name);
        if (!q.exec())
            err = toString(q.lastError());
    }

    while (err.isEmpty() && q.next()) {
        Package* p = new Package(q.value(0).toString(), q.value(1).toString());
        p->url = q.value(2).toString();
        p->icon = q.value(3).toString();
        p->description = q.value(4).toString();
        p->license = q.value(5).toString();

        QString path = getCategoryPath(
                q.value(6).toInt(),
                q.value(7).toInt(),
                q.value(8).toInt(),
                q.value(9).toInt(),
                q.value(10).toInt());
        if (!path.isEmpty())
            p->categories.append(path);

        r.append(p);
    }

    return r;
}

QString DBRepository::savePackageVersion(PackageVersion *p, bool replace)
{
    QString err;

    if (!savePackageVersionQuery) {
        savePackageVersionQuery = new MySQLQuery();
        QString sql = "INSERT OR ";
        if (replace)
            sql += "REPLACE";
        else
            sql += "IGNORE";
        sql += " INTO PACKAGE_VERSION "
                "(NAME, PACKAGE, URL, CONTENT, MSIGUID, DETECT_FILE_COUNT)"
                "VALUES(:NAME, :PACKAGE, :URL, :CONTENT, :MSIGUID, "
                ":DETECT_FILE_COUNT)";
        if (!savePackageVersionQuery->prepare(sql)) {
            err = toString(savePackageVersionQuery->lastError());
            delete savePackageVersionQuery;
        }
    }

    if (err.isEmpty()) {
        Version v = p->version;
        v.normalize();
        savePackageVersionQuery->bindValue(":NAME",
                v.getVersionString());
        savePackageVersionQuery->bindValue(":PACKAGE", p->package);
        savePackageVersionQuery->bindValue(":URL", p->download.toString());
        savePackageVersionQuery->bindValue(":MSIGUID", p->msiGUID);
        savePackageVersionQuery->bindValue(":DETECT_FILE_COUNT",
                p->detectFiles.count());
        QByteArray file;
        file.reserve(1024);
        QXmlStreamWriter w(&file);
        p->toXML(&w);

        savePackageVersionQuery->bindValue(":CONTENT", QVariant(file));
        if (!savePackageVersionQuery->exec())
            err = toString(savePackageVersionQuery->lastError());
    }

    savePackageVersionQuery->finish();

    return err;
}

PackageVersion *DBRepository::findPackageVersionByMSIGUID_(
        const QString &guid, QString* err) const
{
    *err = "";

    PackageVersion* r = 0;

    MySQLQuery q;
    if (!q.prepare("SELECT NAME, "
            "PACKAGE, CONTENT FROM PACKAGE_VERSION "
            "WHERE MSIGUID = :MSIGUID"))
        *err = toString(q.lastError());

    if (err->isEmpty()) {
        q.bindValue(":MSIGUID", guid);
        if (!q.exec())
            *err = toString(q.lastError());
    }

    if (err->isEmpty()) {
        if (q.next()) {
            QDomDocument doc;
            int errorLine, errorColumn;
            if (!doc.setContent(q.value(2).toByteArray(),
                    err, &errorLine, &errorColumn))
                *err = QString(
                        QObject::tr("XML parsing failed at line %1, column %2: %3")).
                        arg(errorLine).arg(errorColumn).arg(*err);

            if (err->isEmpty()) {
                QDomElement root = doc.documentElement();
                PackageVersion* p = PackageVersion::parse(&root, err);

                if (err->isEmpty()) {
                    r = p;
                }
            }
        }
    }

    return r;
}

QString DBRepository::clear()
{
    Job* job = new Job();

    this->categories.clear();

    bool transactionStarted = false;
    if (job->shouldProceed(QObject::tr("Starting an SQL transaction"))) {
        QString err = exec("BEGIN TRANSACTION");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            job->setProgress(0.01);
            transactionStarted = true;
        }
    }

    if (job->shouldProceed(QObject::tr("Clearing the packages table"))) {
        QString err = exec("DELETE FROM PACKAGE");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(0.1);
    }

    if (job->shouldProceed(QObject::tr("Clearing the package versions table"))) {
        QString err = exec("DELETE FROM PACKAGE_VERSION");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(0.7);
    }

    if (job->shouldProceed(QObject::tr("Clearing the licenses table"))) {
        QString err = exec("DELETE FROM LICENSE");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(0.96);
    }

    if (job->shouldProceed(QObject::tr("Clearing the categories table"))) {
        QString err = exec("DELETE FROM CATEGORY");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(0.97);
    }

    if (job->shouldProceed(QObject::tr("Commiting the SQL transaction"))) {
        QString err = exec("COMMIT");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(1);
    } else {
        if (transactionStarted)
            exec("ROLLBACK");
    }

    job->complete();

    return "";
}

void DBRepository::load(Job* job, bool useCache)
{
    QString err;
    QList<QUrl*> urls = AbstractRepository::getRepositoryURLs(&err);
    if (urls.count() > 0) {
        for (int i = 0; i < urls.count(); i++) {
            job->setHint(QString(
                    QObject::tr("Repository %1 of %2")).arg(i + 1).
                         arg(urls.count()));
            Job* s = job->newSubJob(1.0 / urls.count());
            loadOne(urls.at(i), s, useCache);
            if (!s->getErrorMessage().isEmpty()) {
                job->setErrorMessage(QString(
                        QObject::tr("Error loading the repository %1: %2")).arg(
                        urls.at(i)->toString()).arg(
                        s->getErrorMessage()));
                delete s;
                break;
            }
            delete s;

            if (job->isCancelled())
                break;
        }
    } else {
        job->setErrorMessage(QObject::tr("No repositories defined"));
        job->setProgress(1);
    }

    // qDebug() << "Repository::load.3";

    qDeleteAll(urls);
    urls.clear();

    job->complete();
}

void DBRepository::loadOne(QUrl* url, Job* job, bool useCache) {
    job->setHint(QObject::tr("Downloading"));

    QTemporaryFile* f = 0;
    if (job->getErrorMessage().isEmpty() && !job->isCancelled()) {
        Job* djob = job->newSubJob(0.90);
        f = Downloader::download(djob, *url, 0, useCache);
        if (!djob->getErrorMessage().isEmpty())
            job->setErrorMessage(QString(
                    QObject::tr("Download failed: %2")).
                    arg(djob->getErrorMessage()));
        delete djob;
    }

    if (job->shouldProceed(QObject::tr("Parsing XML"))) {
        RepositoryXMLHandler handler(this);
        QXmlSimpleReader reader;
        reader.setContentHandler(&handler);
        reader.setErrorHandler(&handler);
        QXmlInputSource inputSource(f);
        if (!reader.parse(inputSource))
            job->setErrorMessage(handler.errorString());
        else
            job->setProgress(1);
    }

    delete f;

    job->complete();
}

void DBRepository::updateF5(Job* job)
{
    HRTimer timer(9);

    /*
     * Example:
    0 :  0  ms
    1 :  43  ms
    2 :  3343  ms
    3 :  1366  ms
    4 :  2809  ms
    5 :  5683  ms
    6 :  0  ms
    7 :  848  ms
     */
    timer.time(0);

    if (job->shouldProceed(QObject::tr("Clearing the database"))) {
        QString err = clear();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(0.01);
    }

    bool transactionStarted = false;
    if (job->shouldProceed(QObject::tr("Starting an SQL transaction"))) {
        QString err = exec("BEGIN TRANSACTION");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            job->setProgress(0.02);
            transactionStarted = true;
        }
    }

    timer.time(1);
    if (job->shouldProceed(
            QObject::tr("Downloading the remote repositories and filling the local database"))) {
        Job* sub = job->newSubJob(0.3);
        load(sub, true);
        if (!sub->getErrorMessage().isEmpty())
            job->setErrorMessage(sub->getErrorMessage());
        delete sub;
    }

    timer.time(2);

    if (job->shouldProceed(QObject::tr("Commiting the SQL transaction"))) {
        QString err = exec("COMMIT");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(0.35);
    } else {
        if (transactionStarted)
            exec("ROLLBACK");
    }

    timer.time(3);

    timer.time(4);

    if (job->shouldProceed(QObject::tr("Refreshing the installation status"))) {
        Job* sub = job->newSubJob(0.4);
        InstalledPackages::getDefault()->refresh(sub);
        if (!sub->getErrorMessage().isEmpty())
            job->setErrorMessage(sub->getErrorMessage());
        delete sub;
    }

    timer.time(5);

    if (job->shouldProceed(
            QObject::tr("Updating the status for installed packages in the database"))) {
        Job* sub = job->newSubJob(0.05);
        updateStatusForInstalled(sub);
        if (!sub->getErrorMessage().isEmpty())
            job->setErrorMessage(sub->getErrorMessage());
        delete sub;
    }

    timer.time(6);
    if (job->shouldProceed(QObject::tr("Reading categories"))) {
        QString err = readCategories();
        if (err.isEmpty())
            job->setProgress(0.99);
        else
            job->setErrorMessage(err);
    }

    // qDebug() << "updateF5.2";

    timer.time(7);
    if (job->shouldProceed(
            QObject::tr("Removing packages without versions"))) {
        QString err = exec("DELETE FROM PACKAGE WHERE NOT EXISTS "
                "(SELECT * FROM PACKAGE_VERSION "
                "WHERE PACKAGE = PACKAGE.NAME)");
        if (err.isEmpty())
            job->setProgress(0.995);
        else
            job->setErrorMessage(err);
    }

    timer.time(8);

    // timer.dump();

    job->complete();
}

void DBRepository::saveAll(Job* job, Repository* r, bool replace)
{
    bool transactionStarted = false;
    if (job->shouldProceed(QObject::tr("Starting an SQL transaction"))) {
        QString err = exec("BEGIN TRANSACTION");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            job->setProgress(0.01);
            transactionStarted = true;
        }
    }

    if (job->shouldProceed(QObject::tr("Inserting data in the packages table"))) {
        QString err = savePackages(r, replace);
        if (err.isEmpty())
            job->setProgress(0.6);
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed(QObject::tr("Inserting data in the package versions table"))) {
        QString err = savePackageVersions(r, replace);
        if (err.isEmpty())
            job->setProgress(0.95);
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed(QObject::tr("Inserting data in the licenses table"))) {
        QString err = saveLicenses(r, replace);
        if (err.isEmpty())
            job->setProgress(0.98);
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed(QObject::tr("Commiting the SQL transaction"))) {
        QString err = exec("COMMIT");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(1);
    } else {
        if (transactionStarted)
            exec("ROLLBACK");
    }

    job->complete();
}

void DBRepository::updateStatusForInstalled(Job* job)
{
    bool transactionStarted = false;
    if (job->shouldProceed(QObject::tr("Starting an SQL transaction"))) {
        QString err = exec("BEGIN TRANSACTION");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            job->setProgress(0.01);
            transactionStarted = true;
        }
    }

    QSet<QString> packages;
    if (job->shouldProceed()) {
        QList<InstalledPackageVersion*> pvs = InstalledPackages::getDefault()->getAll();
        for (int i = 0; i < pvs.count(); i++) {
            InstalledPackageVersion* pv = pvs.at(i);
            packages.insert(pv->package);
        }
        qDeleteAll(pvs);
        pvs.clear();
        job->setProgress(0.02);
    }

    if (job->shouldProceed(QObject::tr("Updating statuses"))) {
        QList<QString> packages_ = packages.values();
        for (int i = 0; i < packages_.count(); i++) {
            QString package = packages_.at(i);
            updateStatus(package);

            if (!job->shouldProceed())
                break;

            job->setProgress(0.02 + 0.9 * (i + 1) / packages_.count());
        }
    }

    if (job->shouldProceed(QObject::tr("Commiting the SQL transaction"))) {
        QString err = exec("COMMIT");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(1);
    } else {
        if (transactionStarted)
            exec("ROLLBACK");
    }

    job->complete();
}

QString DBRepository::savePackages(Repository* r, bool replace)
{
    QString err;
    for (int i = 0; i < r->packages.count(); i++) {
        Package* p = r->packages.at(i);
        err = savePackage(p, replace);
        if (!err.isEmpty())
            break;
    }

    return err;
}

QString DBRepository::saveLicenses(Repository* r, bool replace)
{
    QString err;
    for (int i = 0; i < r->licenses.count(); i++) {
        License* p = r->licenses.at(i);
        err = saveLicense(p, replace);
        if (!err.isEmpty())
            break;
    }

    return err;
}

QString DBRepository::savePackageVersions(Repository* r, bool replace)
{
    QString err;
    for (int i = 0; i < r->packageVersions.count(); i++) {
        PackageVersion* p = r->packageVersions.at(i);
        err = savePackageVersion(p, replace);
        if (!err.isEmpty())
            break;
    }

    return err;
}

QString DBRepository::toString(const QSqlError& e)
{
    return e.type() == QSqlError::NoError ? "" : e.text();
}

QString DBRepository::readCategories()
{
    QString err;

    this->categories.clear();

    QString sql = "SELECT ID, NAME FROM CATEGORY";

    MySQLQuery q;

    if (!q.prepare(sql))
        err = DBRepository::toString(q.lastError());

    if (err.isEmpty()) {
        if (!q.exec())
            err = toString(q.lastError());
        else {
            while (q.next()) {
                categories.insert(q.value(0).toInt(),
                        q.value(1).toString());
            }
        }
    }

    return err;
}

QString DBRepository::getCategoryPath(int c0, int c1, int c2, int c3,
        int c4) const
{
    QString r;

    QString cat0 = findCategory(c0);
    QString cat1 = findCategory(c1);
    QString cat2 = findCategory(c2);
    QString cat3 = findCategory(c3);
    QString cat4 = findCategory(c4);

    r = cat0;
    if (!cat1.isEmpty())
        r.append('/').append(cat1);
    if (!cat2.isEmpty())
        r.append('/').append(cat2);
    if (!cat3.isEmpty())
        r.append('/').append(cat3);
    if (!cat4.isEmpty())
        r.append('/').append(cat4);

    return r;
}

QString DBRepository::updateStatus(const QString& package)
{
    QString err;

    QList<PackageVersion*> pvs = getPackageVersions_(package, &err);
    PackageVersion* newestInstallable = 0;
    PackageVersion* newestInstalled = 0;
    if (err.isEmpty()) {
        for (int j = 0; j < pvs.count(); j++) {
            PackageVersion* pv = pvs.at(j);
            if (pv->installed()) {
                if (!newestInstalled ||
                        newestInstalled->version.compare(pv->version) < 0)
                    newestInstalled = pv;
            }

            // qDebug() << pv->download.toString();

            if (pv->download.isValid()) {
                if (!newestInstallable ||
                        newestInstallable->version.compare(pv->version) < 0)
                    newestInstallable = pv;
            }
        }
    }

    if (err.isEmpty()) {
        Package::Status status;
        if (newestInstalled) {
            bool up2date = !(newestInstalled && newestInstallable &&
                    newestInstallable->version.compare(
                    newestInstalled->version) > 0);
            if (up2date)
                status = Package::INSTALLED;
            else
                status = Package::UPDATEABLE;
        } else {
            status = Package::NOT_INSTALLED;
        }

        MySQLQuery q;
        if (!q.prepare("UPDATE PACKAGE "
                "SET STATUS=:STATUS "
                "WHERE NAME=:NAME"))
            err = toString(q.lastError());

        if (err.isEmpty()) {
            q.bindValue(":STATUS", status);
            q.bindValue(":NAME", package);
            if (!q.exec())
                err = toString(q.lastError());
        }
    }
    qDeleteAll(pvs);

    return err;
}

QString DBRepository::open()
{
    QString err;

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    QString dir = WPMUtils::getShellDir(CSIDL_COMMON_APPDATA) + "\\Npackd";
    QDir d;
    if (!d.exists(dir))
        d.mkpath(dir);

    QString path = dir + "\\Data.db";

    path = QDir::toNativeSeparators(path);
    db.setDatabaseName(path);
    db.open();
    err = toString(db.lastError());

    if (err.isEmpty())
        err = exec("PRAGMA busy_timeout = 30000");

    bool e = false;
    if (err.isEmpty()) {
        e = tableExists(&db, "PACKAGE", &err);
    }

    if (err.isEmpty()) {
        if (!e) {
            // NULL should be stored in CATEGORYx if a package is not
            // categorized
            db.exec("CREATE TABLE PACKAGE(NAME TEXT, "
                    "TITLE TEXT, "
                    "URL TEXT, "
                    "ICON TEXT, "
                    "DESCRIPTION TEXT, "
                    "LICENSE TEXT, "
                    "FULLTEXT TEXT, "
                    "STATUS INTEGER, "
                    "SHORT_NAME TEXT, "
                    "REPOSITORY INTEGER, "
                    "CATEGORY0 INTEGER, "
                    "CATEGORY1 INTEGER, "
                    "CATEGORY2 INTEGER, "
                    "CATEGORY3 INTEGER, "
                    "CATEGORY4 INTEGER"
                    ")");
            err = toString(db.lastError());
        }
    }

/*
    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE INDEX PACKAGE_FULLTEXT ON PACKAGE(FULLTEXT)");
            err = toString(db.lastError());
        }
    }
    */

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE UNIQUE INDEX PACKAGE_NAME ON PACKAGE(NAME)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE INDEX PACKAGE_SHORT_NAME ON PACKAGE(SHORT_NAME)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        e = tableExists(&db, "CATEGORY", &err);
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE TABLE CATEGORY(ID INTEGER PRIMARY KEY ASC, "
                    "NAME TEXT, PARENT INTEGER, LEVEL INTEGER)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE UNIQUE INDEX CATEGORY_ID ON CATEGORY(ID)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        e = tableExists(&db, "PACKAGE_VERSION", &err);
    }

    if (err.isEmpty()) {
        if (e) {
            // PACKAGE_VERSION.URL is new in 1.19.4
            if (!columnExists(&db, "PACKAGE_VERSION", "URL", &err)) {
                exec("DROP TABLE PACKAGE_VERSION");
                e = false;
            }
        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE TABLE PACKAGE_VERSION(NAME TEXT, "
                    "PACKAGE TEXT, URL TEXT, "
                    "CONTENT BLOB, MSIGUID TEXT, DETECT_FILE_COUNT INTEGER)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE INDEX PACKAGE_VERSION_PACKAGE ON PACKAGE_VERSION("
                    "PACKAGE)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE UNIQUE INDEX PACKAGE_VERSION_PACKAGE_NAME ON "
                    "PACKAGE_VERSION("
                    "PACKAGE, NAME)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        db.exec("CREATE INDEX IF NOT EXISTS PACKAGE_VERSION_MSIGUID ON "
                "PACKAGE_VERSION(MSIGUID)");
        err = toString(db.lastError());
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE INDEX PACKAGE_VERSION_DETECT_FILE_COUNT ON PACKAGE_VERSION("
                    "DETECT_FILE_COUNT)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        e = tableExists(&db, "LICENSE", &err);
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE TABLE LICENSE(NAME TEXT, "
                    "TITLE TEXT, "
                    "DESCRIPTION TEXT, "
                    "URL TEXT"
                    ")");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE UNIQUE INDEX LICENSE_NAME ON LICENSE(NAME)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        e = tableExists(&db, "REPOSITORY", &err);
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE TABLE REPOSITORY(ID INTEGER PRIMARY KEY ASC, "
                    "URL TEXT)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE UNIQUE INDEX REPOSITORY_ID ON REPOSITORY(ID)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        err = readCategories();
    }

    return err;
}

