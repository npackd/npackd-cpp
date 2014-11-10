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
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>

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
    insertPackageQuery = 0;
    replacePackageQuery = 0;
    selectCategoryQuery = 0;
}

DBRepository::~DBRepository()
{
    delete selectCategoryQuery;
    delete insertPackageQuery;
    delete replacePackageQuery;
    delete savePackageVersionQuery;
}

DBRepository* DBRepository::getDefault()
{
    return &def;
}

QString DBRepository::exec(const QString& sql)
{
    MySQLQuery q(db);
    q.exec(sql);
    return toString(q.lastError());
}

int DBRepository::count(const QString& sql, QString* err)
{
    int n = 0;

    *err = "";

    MySQLQuery q(db);
    if (!q.exec(sql))
        *err = toString(q.lastError());

    if (err->isEmpty()) {
        if (!q.next())
            *err = QObject::tr("No records found");
    }

    if (err->isEmpty()) {
        bool ok;
        n = q.value(0).toInt(&ok);
        if (!ok)
            *err = QObject::tr("Not a number");
    }

    return n;
}

QString DBRepository::saveLicense(License* p, bool replace)
{
    QString err;

    MySQLQuery q(db);

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

    MySQLQuery q(*db);
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

    MySQLQuery q(*db);
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

    MySQLQuery q(db);
    if (!q.prepare("SELECT NAME, TITLE, URL, ICON, "
            "DESCRIPTION, LICENSE, CATEGORY0, CATEGORY1, CATEGORY2, "
            "CATEGORY3, CATEGORY4, CHANGELOG, SCREENSHOTS "
            "FROM PACKAGE2 WHERE NAME = :NAME"))
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
        p->changelog = q.value(11).toString();
        QString screenshots = q.value(12).toString().trimmed();
        if (!screenshots.isEmpty())
            p->screenshots = screenshots.split("\n");

        r = p;
    }

    return r;
}

QString DBRepository::findCategory(int cat) const
{
    return categories.value(cat);
}

PackageVersion* DBRepository::findPackageVersion_(
        const QString& package, const Version& version, QString* err) const
{
    *err = "";

    Version v = version;
    v.normalize();
    QString version_ = v.getVersionString();
    PackageVersion* r = 0;

    MySQLQuery q(db);
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

    MySQLQuery q(db);
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

    MySQLQuery q(db);
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
            pv->download.setUrl(url);
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

    MySQLQuery q(db);
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
        MySQLQuery q(db);

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

    if (!query.trimmed().isEmpty()) {
        where += "FULLTEXT MATCH :FULLTEXT";
        params.append(query);
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

    MySQLQuery q(db);

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
            "PACKAGE_FULLTEXT LEFT JOIN CATEGORY ON PACKAGE2.CATEGORY") +
            QString::number(level) +
            " = CATEGORY.ID " +
            where + " GROUP BY CATEGORY.ID, CATEGORY.NAME "
            "ORDER BY CATEGORY.NAME";

    MySQLQuery q(db);

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

    MySQLQuery q(db);
    QString sql = "SELECT NAME, TITLE, URL, ICON, "
            "DESCRIPTION, LICENSE, "
            "CATEGORY0, CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4, "
            "CHANGELOG, SCREENSHOTS "
            "FROM PACKAGE_FULLTEXT";

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
            p->changelog = q.value(11).toString();
            QString screenshots = q.value(12).toString().trimmed();
            if (!screenshots.isEmpty())
                p->screenshots = screenshots.split("\n");

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
        selectCategoryQuery = new MySQLQuery(db);

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
            MySQLQuery q(db);
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

    if (!insertPackageQuery) {
        insertPackageQuery = new MySQLQuery(db);
        replacePackageQuery = new MySQLQuery(db);

        QString insertSQL = "INSERT OR IGNORE";
        QString replaceSQL = "INSERT OR REPLACE";

        QString add = " INTO PACKAGE2 "
                "(NAME, TITLE, URL, ICON, DESCRIPTION, LICENSE, FULLTEXT, "
                "STATUS, SHORT_NAME, CATEGORY0, CATEGORY1, CATEGORY2, CATEGORY3,"
                " CATEGORY4, CHANGELOG, SCREENSHOTS)"
                "VALUES(:NAME, :TITLE, :URL, :ICON, :DESCRIPTION, :LICENSE, "
                ":FULLTEXT, :STATUS, :SHORT_NAME, "
                ":CATEGORY0, :CATEGORY1, :CATEGORY2, :CATEGORY3, :CATEGORY4, "
                ":CHANGELOG, :SCREENSHOTS)";

        insertSQL += add;
        replaceSQL += add;

        if (!insertPackageQuery->prepare(insertSQL)) {
            err = toString(insertPackageQuery->lastError());
            delete insertPackageQuery;
            delete replacePackageQuery;
        }

        if (err.isEmpty()) {
            if (!replacePackageQuery->prepare(replaceSQL)) {
                err = toString(replacePackageQuery->lastError());
                delete insertPackageQuery;
                delete replacePackageQuery;
            }
        }
    }

    MySQLQuery* savePackageQuery;
    if (replace)
        savePackageQuery = replacePackageQuery;
    else
        savePackageQuery = insertPackageQuery;
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
        savePackageQuery->bindValue(":CHANGELOG", p->changelog);
        savePackageQuery->bindValue(":SCREENSHOTS", p->screenshots.join('\n'));
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

    MySQLQuery q(db);
    if (!q.prepare("SELECT NAME, TITLE, URL, ICON, "
            "DESCRIPTION, LICENSE, CATEGORY0, "
            "CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4, CHANGELOG, SCREENSHOTS "
            "FROM PACKAGE2 WHERE SHORT_NAME = :SHORT_NAME"))
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

        p->changelog = q.value(11).toString();
        QString screenshots = q.value(12).toString().trimmed();
        if (!screenshots.isEmpty())
            p->screenshots = screenshots.split('\n');

        r.append(p);
    }

    return r;
}

QString DBRepository::savePackageVersion(PackageVersion *p, bool replace)
{
    QString err;

    if (!savePackageVersionQuery) {
        savePackageVersionQuery = new MySQLQuery(db);
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

    MySQLQuery q(db);
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

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05,
                QObject::tr("Clearing the packages table"));
        QString err = exec("DELETE FROM PACKAGE");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05,
                QObject::tr("Clearing the packages table 2"));
        QString err = exec("DELETE FROM PACKAGE2");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.055,
                QObject::tr("Clearing the packages search table"));
        QString err = exec("DELETE FROM PACKAGE_FULLTEXT");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.6,
                QObject::tr("Clearing the package versions table"));
        QString err = exec("DELETE FROM PACKAGE_VERSION");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.26,
                QObject::tr("Clearing the licenses table"));
        QString err = exec("DELETE FROM LICENSE");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.04,
                QObject::tr("Clearing the categories table"));
        QString err = exec("DELETE FROM CATEGORY");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            sub->completeWithProgress();
            job->setProgress(1);
        }
    }

    job->complete();

    delete job;

    return "";
}

void DBRepository::load(Job* job, bool useCache)
{
    QString err;
    QList<QUrl*> urls = AbstractRepository::getRepositoryURLs(&err);
    if (urls.count() > 0) {
        QStringList reps;
        for (int i = 0; i < urls.size(); i++) {
            reps.append(urls.at(i)->toString());
        }

        err = saveRepositories(reps);
        if (!err.isEmpty())
            job->setErrorMessage(
                    QObject::tr("Error saving the list of repositories in the database: %1").arg(
                    err));

        QList<QFuture<QTemporaryFile*> > files;
        for (int i = 0; i < urls.count(); i++) {
            QUrl* url = urls.at(i);
            Job* s = job->newSubJob(0.1,
                    QObject::tr("Downloading %1").
                    arg(url->toDisplayString()), false, true);
            QFuture<QTemporaryFile*> future = QtConcurrent::run(
                    Downloader::download, s, *url, (QString*) 0,
                    QCryptographicHash::Sha1,
                    useCache);
            files.append(future);
        }

        for (int i = 0; i < urls.count(); i++) {
            if (!job->shouldProceed())
                break;

            files[i].waitForFinished();

            job->setProgress((i + 1.0) / urls.count() * 0.5);
        }

        for (int i = 0; i < urls.count(); i++) {
            if (!job->shouldProceed())
                break;

            QTemporaryFile* tf = files.at(i).result();
            Job* s = job->newSubJob(1.0 / urls.count(), QString(
                    QObject::tr("Repository %1 of %2")).arg(i + 1).
                    arg(urls.count()));
            loadOne(s, tf);
            if (!s->getErrorMessage().isEmpty()) {
                job->setErrorMessage(QString(
                        QObject::tr("Error loading the repository %1: %2")).arg(
                        urls.at(i)->toString()).arg(
                        s->getErrorMessage()));
                break;
            }
        }

        for (int i = 0; i < urls.count(); i++) {
            if (!job->shouldProceed())
                break;

            QTemporaryFile* tf = files.at(i).result();
            delete tf;
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

void DBRepository::loadOne(Job* job, QFile* f) {
    QTemporaryDir* dir = 0;
    if (job->shouldProceed()) {
        if (f->open(QFile::ReadOnly) &&
                f->seek(0) && f->read(4) == QByteArray::fromRawData(
                "PK\x03\x04", 4)) {
            f->close();

            dir = new QTemporaryDir();
            if (dir->isValid()) {
                Job* sub = job->newSubJob(0.1, QObject::tr("Extracting"));
                WPMUtils::unzip(sub, f->fileName(), dir->path() + "\\");
                if (!sub->getErrorMessage().isEmpty()) {
                    job->setErrorMessage(QString(
                            QObject::tr("Unzipping the repository failed: %1")).
                            arg(f->fileName()).
                            arg(sub->getErrorMessage()));
                } else {
                    QString repfn = dir->path() + "\\Rep.xml";
                    if (QFile::exists(repfn)) {
                        f = new QFile(repfn);
                    } else {
                        job->setErrorMessage(QObject::tr(
                                "Rep.xml is missing in a repository in ZIP format"));
                    }
                }
            }
        }
        f->close();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.9, QObject::tr("Parsing XML"));
        RepositoryXMLHandler handler(this);
        QXmlSimpleReader reader;
        reader.setContentHandler(&handler);
        reader.setErrorHandler(&handler);
        QXmlInputSource inputSource(f);
        if (!reader.parse(inputSource))
            job->setErrorMessage(handler.errorString());
        else {
            sub->completeWithProgress();
            job->setProgress(1);
        }
    }

    delete dir;

    job->complete();
}

void DBRepository::updateF5(Job* job)
{
    DBRepository temp;
    QTemporaryFile tempFile;
    bool tempDatabaseOpen = false;
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Creating a temporary file"));
        if (!tempFile.open()) {
            job->setErrorMessage(QObject::tr("Error creating a temporary file"));
        } else {
            tempFile.close();
            sub->completeWithProgress();
        }
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Creating a temporary database"));
        QString err = temp.open("tempdb", tempFile.fileName());
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            tempDatabaseOpen = true;
            sub->completeWithProgress();
        }
    }

    bool transactionStarted = false;
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Starting an SQL transaction (tempdb)"));
        QString err = temp.exec("BEGIN TRANSACTION");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            sub->completeWithProgress();
            transactionStarted = true;
        }
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.27,
                QObject::tr("Downloading the remote repositories and filling the local database (tempdb)"));
        temp.load(sub, true);
        if (!sub->getErrorMessage().isEmpty())
            job->setErrorMessage(sub->getErrorMessage());
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05,
                QObject::tr("Commiting the SQL transaction (tempdb)"));
        QString err = temp.exec("COMMIT");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    } else {
        if (transactionStarted)
            temp.exec("ROLLBACK");
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.4,
                QObject::tr("Refreshing the installation status (tempdb)"));
        InstalledPackages::getDefault()->refresh(&temp, sub);
        if (!sub->getErrorMessage().isEmpty())
            job->setErrorMessage(sub->getErrorMessage());
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05,
                QObject::tr("Updating the status for installed packages in the database (tempdb)"));
        temp.updateStatusForInstalled(sub);
        if (!sub->getErrorMessage().isEmpty())
            job->setErrorMessage(sub->getErrorMessage());
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.04,
                QObject::tr("Removing packages without versions"));
        QString err = temp.exec("DELETE FROM PACKAGE WHERE NOT EXISTS "
                "(SELECT * FROM PACKAGE_VERSION "
                "WHERE PACKAGE = PACKAGE.NAME)");
        if (err.isEmpty())
            sub->completeWithProgress();
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Removing packages without versions 2"));
        QString err = temp.exec("DELETE FROM PACKAGE2 WHERE NOT EXISTS "
                "(SELECT * FROM PACKAGE_VERSION "
                "WHERE PACKAGE = PACKAGE2.NAME)");
        if (err.isEmpty())
            sub->completeWithProgress();
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05,
                QObject::tr("Attaching the temporary database"));
        QString err = exec("ATTACH '" + tempFile.fileName() + "' as tempdb");
        if (err.isEmpty())
            sub->completeWithProgress();
        else
            job->setErrorMessage(err);
    }

    /*QString error;
    //tempFile.setAutoRemove(false);
    qDebug() << "packages in tempdb" << count("SELECT COUNT(*) FROM tempdb.PACKAGE", &error);
    qDebug() << error;
    qDebug() << "package versions in tempdb" << count("SELECT COUNT(*) FROM tempdb.PACKAGE_VERSION", &error);
    qDebug() << error;
    qDebug() << tempFile.fileName();
    */

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Starting an SQL transaction"));
        QString err = exec("BEGIN TRANSACTION");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            sub->completeWithProgress();
            transactionStarted = true;
        }
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Clearing the database"));
        QString err = clear();
        if (err.isEmpty())
            sub->completeWithProgress();
        else
            job->setErrorMessage(err);
    }

    // do not copy full tables here, specify columns names instead to be
    // forward compatible if a new column will be added
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.03,
                QObject::tr("Transferring the data from the temporary database"));

        // exec("DROP INDEX PACKAGE_VERSION_PACKAGE_NAME");
        QString err = exec("INSERT INTO PACKAGE(NAME, TITLE, URL, "
                "ICON, DESCRIPTION, LICENSE, FULLTEXT, STATUS, "
                "SHORT_NAME, REPOSITORY, CATEGORY0, "
                "CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4) "
                "SELECT NAME, TITLE, URL, "
                "ICON, DESCRIPTION, LICENSE, FULLTEXT, STATUS, "
                "SHORT_NAME, REPOSITORY, CATEGORY0, "
                "CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4 "
                "FROM tempdb.PACKAGE2");
        if (err.isEmpty())
            err = exec("INSERT INTO PACKAGE2(NAME, TITLE, URL, "
                    "ICON, DESCRIPTION, LICENSE, FULLTEXT, STATUS, "
                    "SHORT_NAME, REPOSITORY, CATEGORY0, "
                    "CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4, CHANGELOG, "
                    "SCREENSHOTS) "
                    "SELECT NAME, TITLE, URL, "
                    "ICON, DESCRIPTION, LICENSE, FULLTEXT, STATUS, "
                    "SHORT_NAME, REPOSITORY, CATEGORY0, "
                    "CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4, CHANGELOG, "
                    "SCREENSHOTS "
                    "FROM tempdb.PACKAGE2");
        if (err.isEmpty())
            err = exec("INSERT INTO PACKAGE_VERSION ("
                    "NAME, PACKAGE, URL, "
                    "CONTENT, MSIGUID, DETECT_FILE_COUNT) "
                    "SELECT "
                    "NAME, PACKAGE, URL, "
                    "CONTENT, MSIGUID, DETECT_FILE_COUNT "
                    "FROM tempdb.PACKAGE_VERSION");
        if (err.isEmpty())
            err = exec("INSERT INTO LICENSE (NAME, "
                    "TITLE, "
                    "DESCRIPTION, "
                    "URL) "
                    "SELECT NAME, "
                    "TITLE, "
                    "DESCRIPTION, "
                    "URL FROM tempdb.LICENSE");
        if (err.isEmpty())
            err = exec("INSERT INTO CATEGORY (ID, "
                    "NAME, PARENT, LEVEL) "
                    "SELECT ID, "
                    "NAME, PARENT, LEVEL FROM tempdb.CATEGORY");
        if (err.isEmpty())
            err = exec("INSERT INTO PACKAGE_FULLTEXT(NAME, TITLE, URL, "
                    "ICON, DESCRIPTION, LICENSE, FULLTEXT, STATUS, "
                    "SHORT_NAME, REPOSITORY, CATEGORY0, "
                    "CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4, CHANGELOG, "
                    "SCREENSHOTS) "
                    "SELECT NAME, TITLE, URL, "
                    "ICON, DESCRIPTION, LICENSE, FULLTEXT, STATUS, "
                    "SHORT_NAME, REPOSITORY, CATEGORY0, "
                    "CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4, CHANGELOG, "
                    "SCREENSHOTS "
                    "FROM PACKAGE2");
        if (err.isEmpty())
            sub->completeWithProgress();
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.03,
                QObject::tr("Commiting the SQL transaction"));
        QString err = exec("COMMIT");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    } else {
        if (transactionStarted)
            exec("ROLLBACK");
    }

    /*
    qDebug() << "packages in db" << count("SELECT COUNT(*) FROM PACKAGE", &error);
    qDebug() << error;
    qDebug() << "package versions in db" << count("SELECT COUNT(*) FROM PACKAGE_VERSION", &error);
    qDebug() << error;
    qDebug() << tempFile.fileName();
    */

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Detaching the temporary database"));

        QString err;
        for (int i = 0; i < 10; i++) {
            err = exec("DETACH tempdb");
            if (err.isEmpty())
                break;
            else
                Sleep(1000);
        }

        if (err.isEmpty())
            sub->completeWithProgress();
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Reading categories"));
        QString err = readCategories();
        if (err.isEmpty()) {
            sub->completeWithProgress();
            job->setProgress(1);
        } else
            job->setErrorMessage(err);
    }

    if (tempDatabaseOpen)
        temp.db.close();

    job->complete();
}

void DBRepository::updateF5Runnable(Job *job)
{
    QThread::currentThread()->setPriority(QThread::LowestPriority);

    /*
    makes the process too slow
    bool b = SetThreadPriority(GetCurrentThread(),
            THREAD_MODE_BACKGROUND_BEGIN);
    */

    CoInitialize(0);
    updateF5(job);
    CoUninitialize();

    /*
    if (b)
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    */
}

void DBRepository::saveAll(Job* job, Repository* r, bool replace)
{
    bool transactionStarted = false;
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Starting an SQL transaction"));
        QString err = exec("BEGIN TRANSACTION");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            sub->completeWithProgress();
            transactionStarted = true;
        }
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05,
                QObject::tr("Inserting data in the packages table"));
        QString err = savePackages(r, replace);
        if (err.isEmpty())
            sub->completeWithProgress();
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.89,
                QObject::tr("Inserting data in the package versions table"));
        QString err = savePackageVersions(r, replace);
        if (err.isEmpty())
            sub->completeWithProgress();
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.04,
                QObject::tr("Inserting data in the licenses table"));
        QString err = saveLicenses(r, replace);
        if (err.isEmpty())
            sub->completeWithProgress();
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.02,
                QObject::tr("Commiting the SQL transaction"));
        QString err = exec("COMMIT");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            sub->completeWithProgress();
            job->setProgress(1);
        }
    } else {
        if (transactionStarted)
            exec("ROLLBACK");
    }

    job->complete();
}

void DBRepository::updateStatusForInstalled(Job* job)
{
    QString initialTitle = job->getTitle();

    bool transactionStarted = false;
    if (job->shouldProceed()) {
        job->setTitle(initialTitle + " / " +
                QObject::tr("Starting an SQL transaction"));
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

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + " / " +
                QObject::tr("Updating statuses"));
        QList<QString> packages_ = packages.values();
        for (int i = 0; i < packages_.count(); i++) {
            QString package = packages_.at(i);
            updateStatus(package);

            if (!job->shouldProceed())
                break;

            job->setProgress(0.02 + 0.9 * (i + 1) / packages_.count());
        }
    }

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + " / " +
                QObject::tr("Commiting the SQL transaction"));
        QString err = exec("COMMIT");
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(1);
    } else {
        if (transactionStarted)
            exec("ROLLBACK");
    }

    job->setTitle(initialTitle);

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

    MySQLQuery q(db);

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

QStringList DBRepository::readRepositories(QString* err)
{
    QStringList r;

    *err = "";

    QString sql = "SELECT ID, URL FROM REPOSITORY ORDER BY ID";

    MySQLQuery q(db);

    if (!q.prepare(sql))
        *err = DBRepository::toString(q.lastError());

    if (err->isEmpty()) {
        if (!q.exec())
            *err = toString(q.lastError());
        else {
            while (q.next()) {
                r.append(q.value(1).toString());
            }
        }
    }

    return r;
}

QString DBRepository::saveRepositories(const QStringList &reps)
{
    QString err = exec("DELETE FROM REPOSITORY");

    MySQLQuery q(db);

    if (err.isEmpty()) {
        QString sql = "INSERT INTO REPOSITORY "
                "(ID, URL)"
                "VALUES(:ID, :URL)";
        if (!q.prepare(sql))
            err = toString(q.lastError());
    }

    if (err.isEmpty()) {
        for (int i = 0; i < reps.size(); i++) {
            q.bindValue(":ID", i + 1);
            q.bindValue(":URL", reps.at(i));
            if (!q.exec())
                err = toString(q.lastError());
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

        MySQLQuery q(db);
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

        if (err.isEmpty()) {
            if (!q.prepare("UPDATE PACKAGE2 "
                    "SET STATUS=:STATUS "
                    "WHERE NAME=:NAME"))
                err = toString(q.lastError());
        }

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

QString DBRepository::openDefault()
{
    QString dir = WPMUtils::getShellDir(CSIDL_COMMON_APPDATA) + "\\Npackd";
    QDir d;
    if (!d.exists(dir))
        d.mkpath(dir);

    QString path = dir + "\\Data.db";

    path = QDir::toNativeSeparators(path);

    QString err = open("default", path);

    return err;
}

QString DBRepository::open(const QString& connectionName, const QString& file)
{
    QString err;

    QSqlDatabase::removeDatabase(connectionName);
    db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(file);
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
                    "CATEGORY4 INTEGER "
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

    // PACKAGE_FULLTEXT is new in Npackd 1.20
    // cannot handle new columns
    if (err.isEmpty()) {
        e = tableExists(&db, "PACKAGE_FULLTEXT", &err);
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE VIRTUAL TABLE PACKAGE_FULLTEXT USING fts4("
                    "NAME TEXT, "
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
                    "CATEGORY4 INTEGER, "
                    "CHANGELOG TEXT, "
                    "SCREENSHOTS TEXT"
                    ")");
            err = toString(db.lastError());
        }
    }

    // PACKAGE2 is new in Npackd 1.20 and replaces PACKAGE as Npackd 1.19
    // cannot handle new columns
    if (err.isEmpty()) {
        e = tableExists(&db, "PACKAGE2", &err);
    }

    if (err.isEmpty()) {
        if (!e) {
            // NULL should be stored in CATEGORYx if a package is not
            // categorized
            // qDebug() << "creating package2";
            db.exec("CREATE TABLE PACKAGE2(NAME TEXT, "
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
                    "CATEGORY4 INTEGER, "
                    "CHANGELOG TEXT, "
                    "SCREENSHOTS TEXT"
                    ")");
            err = toString(db.lastError());
        }
    }

/*
    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE INDEX PACKAGE2_FULLTEXT ON PACKAGE2(FULLTEXT)");
            err = toString(db.lastError());
        }
    }
    */

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE UNIQUE INDEX PACKAGE2_NAME ON PACKAGE2(NAME)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE INDEX PACKAGE2_SHORT_NAME ON PACKAGE2(SHORT_NAME)");
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        e = tableExists(&db, "REPOSITORY", &err);
    }

    // REPOSITORY is new in Npackd 1.19
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
            // PACKAGE_VERSION.URL is new in 1.18.4
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

