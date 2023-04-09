#include "dbrepository.h"

#include <shlobj.h>
#include <ctime>
#include <unordered_set>
#include <future>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDir>
#include <QVariant>
#include <QTextStream>
#include <QByteArray>
#include <QLoggingCategory>
#include <QXmlStreamWriter>
#include <QSqlRecord>
#include <QTemporaryDir>
#include <QSqlResult>
#include <QtPlugin>

#include "package.h"
#include "repository.h"
#include "packageversion.h"
#include "wpmutils.h"
#include "installedpackages.h"
#include "hrtimer.h"
#include "mysqlquery.h"
#include "repositoryxmlhandler.h"
#include "downloader.h"
#include "packageutils.h"
#include "sqlutils.h"

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
    currentRepository = -1;
    replacePackageVersionQuery = nullptr;
    insertPackageVersionQuery = nullptr;
    insertPackageQuery = nullptr;
    insertLinkQuery = nullptr;
    deleteLinkQuery = nullptr;
    replacePackageQuery = nullptr;
    selectCategoryQuery = nullptr;
    insertInstalledQuery = nullptr;

    // please note that words shorter than 3 characters are removed later anyway
    stopWords = WPMUtils::split(QString("version build edition remove only "
            "bit sp1 sp2 sp3 deu enu update microsoft corporation mozilla "
            "setup package "
            "and are but for into not such that the their then there these "
            "they this was will with windows"), ' ');
}

DBRepository::~DBRepository()
{
    delete insertInstalledQuery;
    delete selectCategoryQuery;
    delete deleteLinkQuery;
    delete insertLinkQuery;
    delete insertPackageQuery;
    delete replacePackageQuery;
    delete replacePackageVersionQuery;
    delete insertPackageVersionQuery;
}

QString DBRepository::saveInstalled(const std::vector<InstalledPackageVersion *> &installed)
{
    QString err;

    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    if (!insertInstalledQuery) {
        insertInstalledQuery = new MySQLQuery(db);

        QString insertSQL = QStringLiteral("INSERT INTO INSTALLED "
                "(PACKAGE, VERSION, CVERSION, WHEN_, WHERE_, DETECTION_INFO) "
                "VALUES(:PACKAGE, :VERSION, :CVERSION, :WHEN_, :WHERE_, :DETECTION_INFO)");

        if (!insertInstalledQuery->prepare(insertSQL)) {
            err = SQLUtils::getErrorString(*insertInstalledQuery);
            delete insertInstalledQuery;
        }
    }

    //qCDebug(npackd) << "saveInstalled";
    if (err.isEmpty()) {
        for (auto ipv: installed) {
            if (!err.isEmpty())
                break;

            //qCDebug(npackd) << "saveInstalled" << ipv->package << ipv->version.getVersionString();
            if (ipv->installed()) {
                insertInstalledQuery->bindValue(QStringLiteral(":PACKAGE"),
                        ipv->package);
                insertInstalledQuery->bindValue(QStringLiteral(":VERSION"),
                        ipv->version.getVersionString());
                insertInstalledQuery->bindValue(QStringLiteral(":CVERSION"),
                        ipv->version.toComparableString());
                insertInstalledQuery->bindValue(QStringLiteral(":WHEN_"), 0);
                insertInstalledQuery->bindValue(QStringLiteral(":WHERE_"),
                        ipv->directory);
                insertInstalledQuery->bindValue(QStringLiteral(":DETECTION_INFO"),
                        ipv->detectionInfo);
                if (!insertInstalledQuery->exec())
                    err = SQLUtils::getErrorString(*insertInstalledQuery);
            }
        }

        insertInstalledQuery->finish();
    }

    return err;
}

DBRepository* DBRepository::getDefault()
{
    return &def;
}

QString DBRepository::exec(const QString& sql)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    MySQLQuery q(db);
    q.exec(sql);
    QString err = SQLUtils::getErrorString(q);

    return err;
}

int DBRepository::count(const QString& sql, QString* err)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    int n = 0;

    *err = QStringLiteral("");

    MySQLQuery q(db);
    if (!q.exec(sql))
        *err = SQLUtils::getErrorString(q);

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
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    MySQLQuery q(db);

    QString sql = QStringLiteral("INSERT OR ");
    if (replace)
        sql += QStringLiteral("REPLACE");
    else
        sql += QStringLiteral("IGNORE");
    sql += QStringLiteral(" INTO LICENSE "
            "(NAME, TITLE, DESCRIPTION, URL)"
            "VALUES(:NAME, :TITLE, :DESCRIPTION, :URL)");
    if (!q.prepare(sql))
        err = SQLUtils::getErrorString(q);

    if (err.isEmpty()) {
        q.bindValue(QStringLiteral(":REPOSITORY"), this->currentRepository);
        q.bindValue(QStringLiteral(":NAME"), p->name);
        q.bindValue(QStringLiteral(":TITLE"), p->title);
        q.bindValue(QStringLiteral(":DESCRIPTION"), p->description);
        q.bindValue(QStringLiteral(":URL"), p->url);
        if (!q.exec())
            err = SQLUtils::getErrorString(q);
    }

    licenses.clear();

    return err;
}

Package *DBRepository::findPackage_(const QString &name) const
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    Package* r = packages.object(name);

    if (r) {
        r = new Package(*r);
    } else {
        MySQLQuery q(db);
        if (!q.prepare(QStringLiteral(
                "SELECT TITLE, URL, ICON, DESCRIPTION, LICENSE, "
                "CATEGORY0, CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4, STARS "
                "FROM PACKAGE WHERE NAME = :NAME LIMIT 1")))
            err = SQLUtils::getErrorString(q);

        if (npackd().isDebugEnabled()) {
            qCDebug(npackd) << name;
        }

        if (err.isEmpty()) {
            q.bindValue(QStringLiteral(":NAME"), name);
            if (!q.exec())
                err = SQLUtils::getErrorString(q);
        }

        if (err.isEmpty() && q.next()) {
            r = new Package(name, name);
            r->title = q.value(0).toString();
            r->url = q.value(1).toString();
            r->setIcon(q.value(2).toString());
            r->description = q.value(3).toString();
            r->license = q.value(4).toString();
            int cat0 = q.value(5).toInt();
            int cat1 = q.value(6).toInt();
            int cat2 = q.value(7).toInt();
            int cat3 = q.value(8).toInt();
            int cat4 = q.value(9).toInt();
            r->stars = q.value(10).toInt();
            QString c;
            if (cat0 > 0) {
                c.append(findCategory(cat0));
                if (cat1 > 0) {
                    c.append('/').append(findCategory(cat1));
                    if (cat2 > 0) {
                        c.append('/').append(findCategory(cat2));
                        if (cat3 > 0) {
                            c.append('/').append(findCategory(cat3));
                            if (cat4 > 0)
                                c.append('/').append(findCategory(cat4));
                        }
                    }
                }
            }
            r->categories.push_back(c);

            if (err.isEmpty())
                err = readLinks(r);

            if (err.isEmpty())
                err = readTags(r);

            if (!err.isEmpty()) {
                delete r;
                r = nullptr;
            }
        }

        if (r)
            packages.insert(name, new Package(*r));
    }

    return r;
}

std::vector<Package*> DBRepository::findPackages(const std::vector<QString>& names)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    std::vector<Package*> ret;
    QString err;

    int start = 0;
    int c = names.size();
    const int block = 10;

    QString sql = QStringLiteral(
            "SELECT NAME, TITLE, URL, ICON, DESCRIPTION, LICENSE, STARS "
            "FROM PACKAGE WHERE NAME IN (:NAME0");
    for (int i = 1; i < block; i++) {
        sql = sql + QStringLiteral(", :NAME") + QString::number(i);
    }
    sql += QStringLiteral(")");

    while (start < c) {
        MySQLQuery q(db);
        if (!q.prepare(sql))
            err = SQLUtils::getErrorString(q);

        if (!err.isEmpty())
            break;

		for (int i = start; i < std::min<int>(start + block, c); i++) {
            q.bindValue(QStringLiteral(":NAME") +
                    QString::number(i - start), names.at(i));
        }

        if (!q.exec())
            err = SQLUtils::getErrorString(q);

        if (!err.isEmpty())
            break;

        std::vector<Package*> list;
        while (q.next()) {
            QString name = q.value(0).toString();
            Package* r = new Package(name, name);
            r->title = q.value(1).toString();
            r->url = q.value(2).toString();
            r->setIcon(q.value(3).toString());
            r->description = q.value(4).toString();
            r->license = q.value(5).toString();
            r->stars = q.value(6).toInt();

            err = readLinks(r);

            if (!err.isEmpty())
                break;

            err = readTags(r);

            if (!err.isEmpty())
                break;

            list.push_back(r);
        }

        for (int i = start; i < std::min<int>(start + block, c); i++) {
            QString find = names.at(i);
            for (int j = 0; j < static_cast<int>(list.size()); j++) {
                Package* p = list.at(j);
                if (p->name == find) {
                    list.erase(list.begin() + j);
                    ret.push_back(p);
                    break;
                }
            }
        }

        qDeleteAll(list);

        if (!err.isEmpty())
            break;

        start += block;
    }

    return ret;
}

QString DBRepository::findCategory(int cat) const
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    auto it = categories.find(cat);
    QString r;
    if (it != categories.end()) {
        r = it->second;
    }

    return r;
}

PackageVersion* DBRepository::findPackageVersion_(
        const QString& package, const Version& version, QString* err) const
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    *err = "";

    Version v = version;
    v.normalize();
    QString version_ = v.getVersionString();
    PackageVersion* r = nullptr;

    MySQLQuery q(db);
    if (!q.prepare(QStringLiteral("SELECT NAME, "
            "PACKAGE, CONTENT, MSIGUID FROM PACKAGE_VERSION "
            "WHERE NAME = :NAME AND PACKAGE = :PACKAGE")))
        *err = SQLUtils::getErrorString(q);

    if (err->isEmpty()) {
        q.bindValue(QStringLiteral(":NAME"), version_);
        q.bindValue(QStringLiteral(":PACKAGE"), package);
        if (!q.exec())
            *err = SQLUtils::getErrorString(q);
    }

    if (err->isEmpty() && q.next()) {
        QByteArray ba = q.value(2).toByteArray();
        r = PackageVersion::parse(ba, err);
    }

    return r;
}

std::vector<PackageVersion*> DBRepository::getPackageVersions_(const QString& package,
        QString *err) const
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    *err = "";

    std::vector<PackageVersion*> r;

    PackageVersionList* pvl = packageVersions.object(package);
    if (pvl) {
        r.reserve(pvl->data.size());
        for (auto pv: pvl->data) {
            r.push_back(pv->clone());
        }
    } else {
        MySQLQuery q(db);
        if (!q.prepare(QStringLiteral("SELECT CONTENT FROM PACKAGE_VERSION "
                "WHERE PACKAGE = :PACKAGE")))
            *err = SQLUtils::getErrorString(q);

        if (err->isEmpty()) {
            q.bindValue(QStringLiteral(":PACKAGE"), package);
            if (!q.exec()) {
                *err = SQLUtils::getErrorString(q);
            }
        }

        while (err->isEmpty() && q.next()) {
            QByteArray ba = q.value(0).toByteArray();
            PackageVersion* pv = PackageVersion::parse(ba,
                    err, false);
            if (err->isEmpty())
                r.push_back(pv);
        }

        // qCDebug(npackd) << vs.count();

        std::sort(r.begin(), r.end(), packageVersionLessThan3);

        if (err->isEmpty()) {
            pvl = new PackageVersionList();
            pvl->data.reserve(r.size());
            for (auto pv: r) {
                pvl->data.push_back(pv->clone());
            }
            this->packageVersions.insert(package, pvl);
        }
    }

    return r;
}

std::vector<PackageVersion *> DBRepository::findPackageVersionsWithCmdFile(
        const QString &name, QString *err) const
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    *err = "";

    std::vector<PackageVersion*> r;

    MySQLQuery q(db);
    if (!q.prepare(QStringLiteral("SELECT CONTENT FROM PACKAGE_VERSION PV "
            "WHERE EXISTS (SELECT 1 FROM CMD_FILE WHERE "
            "PACKAGE = PV.PACKAGE AND "
            "VERSION = PV.NAME AND "
            "NAME = :NAME)")))
        *err = SQLUtils::getErrorString(q);

    if (err->isEmpty()) {
        q.bindValue(QStringLiteral(":NAME"), name.toLower());
        if (!q.exec()) {
            *err = SQLUtils::getErrorString(q);
        }
    }

    while (err->isEmpty() && q.next()) {
        QByteArray ba = q.value(0).toByteArray();
        PackageVersion* pv = PackageVersion::parse(ba,
                err);
        if (err->isEmpty())
            r.push_back(pv);
    }

    // qCDebug(npackd) << vs.count();

    std::sort(r.begin(), r.end(), packageVersionLessThan3);

    return r;
}

License *DBRepository::findLicense_(const QString& name, QString *err)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    *err = QStringLiteral("");

    License* r = nullptr;
    License* cached = this->licenses.object(name);
    if (!cached) {
        MySQLQuery q(db);

        if (!q.prepare(QStringLiteral("SELECT NAME, TITLE, DESCRIPTION, URL "
                "FROM LICENSE "
                "WHERE NAME = :NAME")))
            *err = SQLUtils::getErrorString(q);

        if (err->isEmpty()) {
            q.bindValue(QStringLiteral(":NAME"), name);
            if (!q.exec())
                *err = SQLUtils::getErrorString(q);
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

std::vector<QString> DBRepository::tokenizeTitle(const QString& title)
{
    QString txt = title.toLower();
    txt.replace('(', ' ');
    txt.replace(')', ' ');
    txt.replace('[', ' ');
    txt.replace(']', ' ');
    txt.replace('{', ' ');
    txt.replace('}', ' ');
    txt.replace('-', ' ');
    txt.replace('_', ' ');
    txt.replace('/', ' ');
    txt.replace('@', ' ');
    txt = txt.simplified();

    std::vector<QString> keywords = WPMUtils::split(txt, ' ', Qt::SkipEmptyParts);

    // synonyms
    for (int i = 0; i < static_cast<int>(keywords.size()); i++) {
        const QString& p = keywords.at(i);
        if (p == QStringLiteral("x64") || p == QStringLiteral("amd64") ) {
            keywords[i] = QStringLiteral("x86_64");
        } else if (p == QStringLiteral("x86")) {
            keywords[i] = QStringLiteral("i686");
        }
    }

    // "32 bit" and "64 bit"
    for (int i = 0; i < static_cast<int>(keywords.size()) - 1; i++) {
        const QString& p = keywords.at(i);
        const QString& p2 = keywords.at(i + 1).toLower();
        if (p == QStringLiteral("32") && p2 == QStringLiteral("bit")) {
            keywords[i] = QStringLiteral("i686");
            keywords.erase(keywords.begin() + i + 1);
        } else if (p == QStringLiteral("64") && p2 == QStringLiteral("bit")) {
            keywords[i] = QStringLiteral("x86_64");
            keywords.erase(keywords.begin() + i + 1);
        }
    }

    bool bits = false;
    for (auto& k: keywords) {
        if (k == QStringLiteral("x86_64") || k == QStringLiteral("i686")) {
            bits = true;
            break;
        }
    }
    if (!bits)
        keywords.push_back(QStringLiteral("i686"));

    // remove (X == a digit)
    //  - stop words
    //  - numbers
    //  - KBXXXXXX
    //  - KBXXXXXXX
    //  - all words shorter than 3 characters
    //  - vX...
    for (int i = 0; i < static_cast<int>(keywords.size()); ) {
        const QString& p = keywords.at(i);
        if (std::find(stopWords.begin(), stopWords.end(), p) != stopWords.end()) {
            keywords.erase(keywords.begin() + i);
        } else if (p.length() < 3) {
            keywords.erase(keywords.begin() + i);
        } else if (p.length() > 0 && p.at(0).isDigit()) {
            keywords.erase(keywords.begin() + i);
        } else if (p.length() >= 2 && (p.at(0).toUpper() == 'V') &&
               p.at(1).isDigit()) {
            keywords.erase(keywords.begin() + i);
        } else if (p.length() == 8 && (p.at(0).toUpper() == 'K') &&
                (p.at(1).toUpper() == 'B') && p.at(2).isDigit() &&
                p.at(3).isDigit() && p.at(4).isDigit() &&
                p.at(5).isDigit() && p.at(6).isDigit() && p.at(7).isDigit()) {
            keywords.erase(keywords.begin() + i);
        } else if (p.length() == 9 && (p.at(0).toUpper() == 'K') &&
                (p.at(1).toUpper() == 'B') && p.at(2).isDigit() &&
                p.at(3).isDigit() && p.at(4).isDigit() &&
                p.at(5).isDigit() && p.at(6).isDigit() && p.at(7).isDigit() &&
                p.at(8).isDigit()) {
            keywords.erase(keywords.begin() + i);
        } else {
            i++;
        }
    }

    return keywords;
}

std::vector<QString> DBRepository::findBetterPackages(const QString& title, QString* err)
{
    std::vector<QString> packages;

    std::vector<QString> keywords = tokenizeTitle(title);

    if (keywords.size() > 0) {
        QString where = QStringLiteral("select name from package "
                "where name not like 'msi.%' "
                "and name not like 'control-panel.%'");
        std::vector<QVariant> params;

        for (int i = 0; i < static_cast<int>(keywords.size()); i++) {
            QString kw = keywords.at(i);
            if (kw.length() > 1) {
                if (!where.isEmpty())
                    where += QStringLiteral(" AND ");
                where += QStringLiteral("TITLE_FULLTEXT LIKE :TITLE_FULLTEXT") +
                        QString::number(i);
                params.push_back(QStringLiteral("% ") + kw +
                        QStringLiteral(" %"));
            }
        }

        where.append(QStringLiteral(" LIMIT 2"));

        packages = findPackagesWhere(where, params, err);

        QString what;
        if (packages.size() == 1)
            what = packages.at(0);
        else
            what = QString("%1 packages").arg(packages.size());

        qCDebug(npackd) << "searching for" << WPMUtils::join(keywords, ' ') <<
                "found" << what;
    }

    return packages;
}

int DBRepository::getMaxStars(QString* err)
{
    return count("SELECT MAX(STARS) FROM PACKAGE", err);
}

QString DBRepository::createQuery(Package::Status minStatus,
      Package::Status maxStatus,
      const QString& query, int cat0, int cat1, std::vector<QVariant>& params) const
{
    QString where;

    // simplified() returns single spaces between words and none
    // at the beginning or at the end
    std::vector<QString> keywords = WPMUtils::split(query.toLower().simplified(),
            QStringLiteral(" "),
            Qt::SkipEmptyParts);

    for (int i = 0; i < static_cast<int>(keywords.size()); i++) {
        QString kw = keywords.at(i);

        if (kw.length() <= 1)
            continue;

        if (kw.length() == 2 && kw.at(0) == '-')
            continue;

        if (!where.isEmpty())
            where += QStringLiteral(" AND ");
        if (kw.startsWith('-')) {
            where += QStringLiteral("FULLTEXT NOT LIKE :FULLTEXT") +
                    QString::number(i);
            kw.remove(0, 1);
        } else {
            where += QStringLiteral("FULLTEXT LIKE :FULLTEXT") +
                    QString::number(i);
        }
        params.push_back(QStringLiteral("%") + kw.toLower() +
                QStringLiteral("%"));
    }
    if (minStatus < maxStatus) {
        if (!where.isEmpty())
            where += QStringLiteral(" AND ");

        where += QStringLiteral("STATUS >= :MINSTATUS AND STATUS < :MAXSTATUS");

        params.push_back(QVariant(static_cast<int>(minStatus)));
        params.push_back(QVariant(static_cast<int>(maxStatus)));
    }

    if (cat0 == 0) {
        if (!where.isEmpty())
            where += QStringLiteral(" AND ");
        where += QStringLiteral("CATEGORY0 IS NULL");
    } else if (cat0 > 0) {
        if (!where.isEmpty())
            where += QStringLiteral(" AND ");
        where += QStringLiteral("CATEGORY0 = :CATEGORY0");
        params.push_back(QVariant(cat0));
    }

    if (cat1 == 0) {
        if (!where.isEmpty())
            where += QStringLiteral(" AND ");
        where += QStringLiteral("CATEGORY1 IS NULL");
    } else if (cat1 > 0) {
        if (!where.isEmpty())
            where += QStringLiteral(" AND ");
        where += QStringLiteral("CATEGORY1 = :CATEGORY1");
        params.push_back(QVariant(cat1));
    }

    return where;
}

std::vector<QString> DBRepository::findPackages(Package::Status minStatus,
        Package::Status maxStatus,
        const QString& query, int cat0, int cat1, QString *err) const
{
    // qCDebug(npackd) << "DBRepository::findPackages.0";

    std::vector<QVariant> params;
    QString where = createQuery(minStatus, maxStatus, query, cat0, cat1, params);

    if (!where.isEmpty())
        where = QStringLiteral("WHERE ") + where;

    // qCDebug(npackd) << "DBRepository::findPackages.1";

    return findPackagesWhere(QStringLiteral("SELECT NAME FROM PACKAGE ") +
            where + QStringLiteral(" ORDER BY TITLE"), params, err);
}

std::vector<QString> DBRepository::getCategories(const std::vector<QString>& ids, QString* err)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    *err = "";

    QString sql = QStringLiteral("SELECT NAME FROM CATEGORY WHERE ID IN (") +
            WPMUtils::join(ids, QStringLiteral(", ")) + QStringLiteral(")");

    MySQLQuery q(db);

    if (!q.prepare(sql))
        *err = SQLUtils::getErrorString(q);

    std::vector<QString> r;
    if (err->isEmpty()) {
        if (!q.exec())
            *err = SQLUtils::getErrorString(q);
        else {
            while (q.next()) {
                r.push_back(q.value(0).toString());
            }
        }
    }

    return r;
}

std::vector<std::vector<QString>> DBRepository::findCategories(Package::Status minStatus,
        Package::Status maxStatus,
        const QString& query, int level, int cat0, int cat1, QString *err) const
{
    // qCDebug(npackd) << "DBRepository::findPackages.0";

    std::vector<QVariant> params;
    QString where = createQuery(minStatus, maxStatus, query, cat0, cat1, params);

    if (!where.isEmpty())
        where = QStringLiteral("WHERE ") + where;

    QString sql = QStringLiteral(
            "SELECT CATEGORY.ID, COUNT(*), CATEGORY.NAME FROM "
            "PACKAGE LEFT JOIN CATEGORY ON PACKAGE.CATEGORY") +
            QString::number(level) +
            QStringLiteral(" = CATEGORY.ID ") +
            where + QStringLiteral(" GROUP BY CATEGORY.ID, CATEGORY.NAME "
            "ORDER BY CATEGORY.NAME");

    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    MySQLQuery q(db);

    if (!q.prepare(sql))
        *err = SQLUtils::getErrorString(q);

    if (err->isEmpty()) {
        for (int i = 0; i < static_cast<int>(params.size()); i++) {
            q.bindValue(i, params.at(i));
        }
    }

    std::vector<std::vector<QString>> r;
    if (err->isEmpty()) {
        if (!q.exec())
            *err = SQLUtils::getErrorString(q);

        while (q.next()) {
            std::vector<QString> sl;
            sl.push_back(q.value(0).toString());
            sl.push_back(q.value(1).toString());
            sl.push_back(q.value(2).toString());
            r.push_back(sl);
        }
    }

    return r;
}

std::vector<QString> DBRepository::findPackagesWhere(const QString& sql,
        const std::vector<QVariant>& params,
        QString *err) const
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    *err = QStringLiteral("");

    std::vector<QString> r;
    MySQLQuery q(db);

    if (!q.prepare(sql))
        *err = SQLUtils::getErrorString(q);

    if (err->isEmpty()) {
        for (int i = 0; i < static_cast<int>(params.size()); i++) {
            q.bindValue(i, params.at(i));
        }
    }

    if (err->isEmpty()) {
        if (!q.exec())
            *err = SQLUtils::getErrorString(q);

        while (q.next()) {
            r.push_back(q.value(0).toString());
        }
    }

    return r;
}

int DBRepository::insertCategory(int parent, int level,
        const QString& category, QString* err)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    int id = -1;

    *err = QStringLiteral("");

    if (!selectCategoryQuery) {
        selectCategoryQuery = new MySQLQuery(db);

        QString sql = QStringLiteral(
                "SELECT ID FROM CATEGORY WHERE PARENT = :PARENT AND "
                "LEVEL = :LEVEL AND NAME = :NAME");

        if (!selectCategoryQuery->prepare(sql)) {
            *err = SQLUtils::getErrorString(*selectCategoryQuery);
            delete selectCategoryQuery;
        }
    }

    if (err->isEmpty()) {
        selectCategoryQuery->bindValue(QStringLiteral(":NAME"), category);
        selectCategoryQuery->bindValue(QStringLiteral(":PARENT"), parent);
        selectCategoryQuery->bindValue(QStringLiteral(":LEVEL"), level);
        if (!selectCategoryQuery->exec())
            *err = SQLUtils::getErrorString(*selectCategoryQuery);
    }

    if (err->isEmpty()) {
        if (selectCategoryQuery->next())
            id = selectCategoryQuery->value(0).toInt();
        else {
            MySQLQuery q(db);
            QString sql = QStringLiteral("INSERT INTO CATEGORY "
                    "(ID, NAME, PARENT, LEVEL) "
                    "VALUES (NULL, :NAME, :PARENT, :LEVEL)");
            if (!q.prepare(sql))
                *err = SQLUtils::getErrorString(q);

            if (err->isEmpty()) {
                q.bindValue(QStringLiteral(":NAME"), category);
                q.bindValue(QStringLiteral(":PARENT"), parent);
                q.bindValue(QStringLiteral(":LEVEL"), level);
                if (!q.exec())
                    *err = SQLUtils::getErrorString(q);
                else
                    id = q.lastInsertId().toInt();
            }
        }
    }

    selectCategoryQuery->finish();

    return id;
}

QString DBRepository::deleteLinks(const QString& name)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    if (!deleteLinkQuery) {
        deleteLinkQuery = new MySQLQuery(db);
        if (!deleteLinkQuery->prepare(
                QStringLiteral("DELETE FROM LINK WHERE PACKAGE=:PACKAGE"))) {
            err = SQLUtils::getErrorString(*deleteLinkQuery);
            delete deleteLinkQuery;
        }
    }

    if (err.isEmpty()) {
        deleteLinkQuery->bindValue(QStringLiteral(":PACKAGE"), name);
        if (!deleteLinkQuery->exec())
            err = SQLUtils::getErrorString(*deleteLinkQuery);
        deleteLinkQuery->finish();
    }

    return err;
}

QString DBRepository::deleteTags(const QString& name)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    if (!deleteTagQuery) {
        deleteTagQuery.reset(new MySQLQuery(db));
        if (!deleteTagQuery->prepare(
                QStringLiteral("DELETE FROM TAG WHERE PACKAGE=:PACKAGE"))) {
            err = SQLUtils::getErrorString(*deleteTagQuery);
            deleteTagQuery->clear();
        }
    }

    if (err.isEmpty()) {
        deleteTagQuery->bindValue(QStringLiteral(":PACKAGE"), name);
        if (!deleteTagQuery->exec())
            err = SQLUtils::getErrorString(*deleteTagQuery);
        deleteTagQuery->finish();
    }

    return err;
}

QString DBRepository::deleteCmdFiles(const QString& name, const Version& version)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    if (!deleteCmdFilesQuery) {
        deleteCmdFilesQuery.reset(new MySQLQuery(db));
        if (!deleteCmdFilesQuery->prepare(QStringLiteral(
                "DELETE FROM CMD_FILE WHERE PACKAGE=:PACKAGE AND VERSION=:VERSION"))) {
            err = SQLUtils::getErrorString(*deleteCmdFilesQuery);
            deleteCmdFilesQuery.reset(nullptr);
        }
    }

    if (err.isEmpty()) {
        deleteCmdFilesQuery->bindValue(QStringLiteral(":PACKAGE"), name);
        Version v(version);
        v.normalize();
        deleteCmdFilesQuery->bindValue(QStringLiteral(":VERSION"),
                v.getVersionString());
        if (!deleteCmdFilesQuery->exec())
            err = SQLUtils::getErrorString(*deleteCmdFilesQuery);
        deleteCmdFilesQuery->finish();
    }

    return err;
}

QString DBRepository::saveLinks(Package* p)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    if (!insertLinkQuery) {
        insertLinkQuery = new MySQLQuery(db);

        QString insertSQL = QStringLiteral("INSERT INTO LINK "
                "(PACKAGE, INDEX_, REL, HREF) "
                "VALUES(:PACKAGE, :INDEX_, :REL, :HREF)");

        if (!insertLinkQuery->prepare(insertSQL)) {
            err = SQLUtils::getErrorString(*insertLinkQuery);
            delete insertLinkQuery;
        }
    }

    if (err.isEmpty()) {
        int index = 1;
        for (auto&& i: p->links) {
            if (!err.isEmpty())
                break;

            if (!i.first.isEmpty() && !i.second.isEmpty()) {
                insertLinkQuery->bindValue(QStringLiteral(":PACKAGE"), p->name);
                insertLinkQuery->bindValue(QStringLiteral(":INDEX_"), index);
                insertLinkQuery->bindValue(QStringLiteral(":REL"), i.first);
                insertLinkQuery->bindValue(QStringLiteral(":HREF"), i.second);
                if (!insertLinkQuery->exec())
                    err = SQLUtils::getErrorString(*insertLinkQuery);

                index++;
            }
        }
        insertLinkQuery->finish();
    }

    return err;
}

QString DBRepository::saveTags(Package* p)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    if (!insertTagQuery) {
        insertTagQuery.reset(new MySQLQuery(db));

        QString insertSQL = QStringLiteral("INSERT INTO TAG "
                "(PACKAGE, VALUE) "
                "VALUES(:PACKAGE, :VALUE)");

        if (!insertTagQuery->prepare(insertSQL)) {
            err = SQLUtils::getErrorString(*insertTagQuery);
            insertTagQuery->clear();
        }
    }

    if (err.isEmpty()) {
        for (auto& value: p->tags) {
            if (!err.isEmpty())
                break;

            if (!value.isEmpty()) {
                insertTagQuery->bindValue(QStringLiteral(":PACKAGE"), p->name);
                insertTagQuery->bindValue(QStringLiteral(":VALUE"), value);
                if (!insertTagQuery->exec())
                    err = SQLUtils::getErrorString(*insertTagQuery);
            }
        }
        insertTagQuery->finish();
    }

    return err;
}

QString DBRepository::savePackage(Package *p, bool replace)
{
    QString err;

    /*
    if (p->name == "com.microsoft.Windows64")
        qCDebug(npackd) << p->name << "->" << p->description;
        */

    int cat0 = 0;
    int cat1 = 0;
    int cat2 = 0;
    int cat3 = 0;
    int cat4 = 0;

    if (p->categories.size() > 0) {
        QString category = p->categories.front();
        std::vector<QString> cats = WPMUtils::split(category, '/');
        for (int i = 0; i < static_cast<int>(cats.size()); i++) {
            cats[i] = cats.at(i).trimmed();
        }

        QString c;
        if (cats.size() > 0) {
            c = cats.at(0);
            cat0 = insertCategory(0, 0, c, &err);
        }
        if (cats.size() > 1) {
            c = cats.at(1);
            cat1 = insertCategory(cat0, 1, c, &err);
        }
        if (cats.size() > 2) {
            c = cats.at(2);
            cat2 = insertCategory(cat1, 2, c, &err);
        }
        if (cats.size() > 3) {
            c = cats.at(3);
            cat3 = insertCategory(cat2, 3, c, &err);
        }
        if (cats.size() > 4) {
            c = cats.at(4);
            cat4 = insertCategory(cat3, 4, c, &err);
        }
    }

    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    if (!insertPackageQuery) {
        insertPackageQuery = new MySQLQuery(db);
        replacePackageQuery = new MySQLQuery(db);

        QString insertSQL = QStringLiteral("INSERT OR IGNORE");
        QString replaceSQL = QStringLiteral("INSERT OR REPLACE");

        QString add = QStringLiteral(" INTO PACKAGE "
                "(REPOSITORY, NAME, TITLE, URL, ICON, "
                "DESCRIPTION, LICENSE, FULLTEXT, "
                "STATUS, SHORT_NAME, CATEGORY0, CATEGORY1, CATEGORY2, CATEGORY3,"
                " CATEGORY4, TITLE_FULLTEXT, STARS)"
                "VALUES(:REPOSITORY, :NAME, :TITLE, :URL, "
                ":ICON, :DESCRIPTION, :LICENSE, "
                ":FULLTEXT, :STATUS, :SHORT_NAME, "
                ":CATEGORY0, :CATEGORY1, :CATEGORY2, :CATEGORY3, :CATEGORY4, "
                ":TITLE_FULLTEXT, :STARS)");

        insertSQL += add;
        replaceSQL += add;

        if (!insertPackageQuery->prepare(insertSQL)) {
            err = SQLUtils::getErrorString(*insertPackageQuery);
            delete insertPackageQuery;
            delete replacePackageQuery;
        }

        if (err.isEmpty()) {
            if (!replacePackageQuery->prepare(replaceSQL)) {
                err = SQLUtils::getErrorString(*replacePackageQuery);
                delete insertPackageQuery;
                delete replacePackageQuery;
                return err;
            }
        }
    }

    int affected = 0;

    if (err.isEmpty()) {
        MySQLQuery* savePackageQuery;
        if (replace)
            savePackageQuery = replacePackageQuery;
        else
            savePackageQuery = insertPackageQuery;

        if (err.isEmpty()) {
            savePackageQuery->bindValue(QStringLiteral(":REPOSITORY"),
                    this->currentRepository);
            savePackageQuery->bindValue(QStringLiteral(":NAME"), p->name);
            savePackageQuery->bindValue(QStringLiteral(":TITLE"), p->title);
            savePackageQuery->bindValue(QStringLiteral(":URL"), p->url);
            savePackageQuery->bindValue(QStringLiteral(":ICON"), p->getIcon());
            savePackageQuery->bindValue(QStringLiteral(":DESCRIPTION"),
                    p->description);
            savePackageQuery->bindValue(QStringLiteral(":LICENSE"), p->license);
            savePackageQuery->bindValue(QStringLiteral(":FULLTEXT"),
                    (p->title + QStringLiteral(" ") + p->description +
                    QStringLiteral(" ") +
                    p->name + QStringLiteral(" ") +
                    WPMUtils::join(p->categories, ' ') + QStringLiteral(" ") +
                    WPMUtils::join(p->tags, ' ')).toLower());
            savePackageQuery->bindValue(QStringLiteral(":STATUS"), 0);
            savePackageQuery->bindValue(QStringLiteral(":SHORT_NAME"),
                    p->getShortName());
            if (cat0 == 0)
                savePackageQuery->bindValue(QStringLiteral(":CATEGORY0"),
                        QVariant(QVariant::Int));
            else
                savePackageQuery->bindValue(QStringLiteral(":CATEGORY0"), cat0);
            if (cat1 == 0)
                savePackageQuery->bindValue(QStringLiteral(":CATEGORY1"),
                        QVariant(QVariant::Int));
            else
                savePackageQuery->bindValue(QStringLiteral(":CATEGORY1"), cat1);
            if (cat2 == 0)
                savePackageQuery->bindValue(QStringLiteral(":CATEGORY2"),
                        QVariant(QVariant::Int));
            else
                savePackageQuery->bindValue(QStringLiteral(":CATEGORY2"), cat2);
            if (cat3 == 0)
                savePackageQuery->bindValue(QStringLiteral(":CATEGORY3"),
                        QVariant(QVariant::Int));
            else
                savePackageQuery->bindValue(QStringLiteral(":CATEGORY3"), cat3);
            if (cat4 == 0)
                savePackageQuery->bindValue(QStringLiteral(":CATEGORY4"),
                        QVariant(QVariant::Int));
            else
                savePackageQuery->bindValue(QStringLiteral(":CATEGORY4"), cat4);
            savePackageQuery->bindValue(QStringLiteral(":TITLE_FULLTEXT"),
                    ' ' + WPMUtils::join(tokenizeTitle(p->title), ' ') + ' ');
            savePackageQuery->bindValue(QStringLiteral(":STARS"), p->stars);

            if (!savePackageQuery->exec())
                err = SQLUtils::getErrorString(*savePackageQuery);
            else
                affected = savePackageQuery->numRowsAffected();
        }

        savePackageQuery->finish();
    }

    bool exists = affected == 0;

    if (err.isEmpty()) {
        if (!exists)
            err = deleteLinks(p->name);
    }

    if (err.isEmpty()) {
        if (!exists)
            err = deleteTags(p->name);
    }

    if (err.isEmpty()) {
        if (!exists)
            err = saveLinks(p);
    }

    if (err.isEmpty()) {
        if (!exists)
            err = saveTags(p);
    }

    packages.clear();

    return err;
}

std::vector<Package*> DBRepository::findPackagesByShortName(const QString &name) const
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    std::vector<Package*> r;

    MySQLQuery q(db);
    if (!q.prepare(QStringLiteral("SELECT NAME, TITLE, URL, ICON, "
            "DESCRIPTION, LICENSE, CATEGORY0, "
            "CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4, STARS "
            "FROM PACKAGE WHERE SHORT_NAME = :SHORT_NAME "
            "LIMIT 2")))
        err = SQLUtils::getErrorString(q);

    if (err.isEmpty()) {
        q.bindValue(QStringLiteral(":SHORT_NAME"), name);
        if (!q.exec())
            err = SQLUtils::getErrorString(q);
    }

    while (err.isEmpty() && q.next()) {
        Package* p = new Package(q.value(0).toString(), q.value(1).toString());
        p->url = q.value(2).toString();
        p->setIcon(q.value(3).toString());
        p->description = q.value(4).toString();
        p->license = q.value(5).toString();
        p->stars = q.value(11).toInt();

        QString path = getCategoryPath(
                q.value(6).toInt(),
                q.value(7).toInt(),
                q.value(8).toInt(),
                q.value(9).toInt(),
                q.value(10).toInt());
        if (!path.isEmpty())
            p->categories.push_back(path);

        if (err.isEmpty())
            err = readLinks(p);

        r.push_back(p);
    }

    return r;
}

QString DBRepository::readLinks(Package* p) const
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    std::vector<Package*> r;

    MySQLQuery q(db);
    if (!q.prepare(QStringLiteral("SELECT REL, HREF "
            "FROM LINK WHERE PACKAGE = :PACKAGE "
            "ORDER BY INDEX_")))
        err = SQLUtils::getErrorString(q);

    if (err.isEmpty()) {
        q.bindValue(QStringLiteral(":PACKAGE"), p->name);
        if (!q.exec())
            err = SQLUtils::getErrorString(q);
    }

    while (err.isEmpty() && q.next()) {
        p->links.insert({q.value(0).toString(), q.value(1).toString()});
    }

    return err;
}

QString DBRepository::readTags(Package* p) const
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    std::vector<Package*> r;

    MySQLQuery q(db);
    if (!q.prepare(QStringLiteral("SELECT VALUE "
            "FROM TAG WHERE PACKAGE = :PACKAGE "
            "ORDER BY VALUE")))
        err = SQLUtils::getErrorString(q);

    if (err.isEmpty()) {
        q.bindValue(QStringLiteral(":PACKAGE"), p->name);
        if (!q.exec())
            err = SQLUtils::getErrorString(q);
    }

    while (err.isEmpty() && q.next()) {
        p->tags.push_back(q.value(0).toString());
    }

    return err;
}

QString DBRepository::savePackageVersion(PackageVersion *p, bool replace)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    if (!replacePackageVersionQuery) {
        replacePackageVersionQuery = new MySQLQuery(db);
        insertPackageVersionQuery = new MySQLQuery(db);
        insertCmdFileQuery.reset(new MySQLQuery(db));

        QString sql = QStringLiteral(" INTO PACKAGE_VERSION "
                "(NAME, PACKAGE, URL, "
                "CONTENT, DETECT_FILE_COUNT)"
                "VALUES(:NAME, :PACKAGE, "
                ":URL, :CONTENT, "
                ":DETECT_FILE_COUNT)");

        if (!replacePackageVersionQuery->prepare(
                QStringLiteral("INSERT OR REPLACE ") + sql)) {
            err = SQLUtils::getErrorString(*replacePackageVersionQuery);
            delete replacePackageVersionQuery;
        }
        if (err.isEmpty() && !insertPackageVersionQuery->prepare(
                QStringLiteral("INSERT OR IGNORE ") + sql)) {
            err = SQLUtils::getErrorString(*insertPackageVersionQuery);
            delete insertPackageVersionQuery;
        }
        if (err.isEmpty() && !insertCmdFileQuery->prepare(QStringLiteral(
                "INSERT INTO CMD_FILE("
                "PACKAGE, VERSION, PATH, NAME) "
                "VALUES (:PACKAGE, :VERSION, :PATH, :NAME)"))) {
            err = SQLUtils::getErrorString(*insertCmdFileQuery);
            insertCmdFileQuery = nullptr;
        }
    }

    bool modified = false;
    if (err.isEmpty()) {
        MySQLQuery* q;
        if (replace)
            q = replacePackageVersionQuery;
        else
            q = insertPackageVersionQuery;

        Version v = p->version;
        v.normalize();
        q->bindValue(QStringLiteral(":REPOSITORY"),
                this->currentRepository);
        q->bindValue(QStringLiteral(":NAME"),
                v.getVersionString());
        q->bindValue(QStringLiteral(":PACKAGE"), p->package);
        q->bindValue(QStringLiteral(":URL"), p->download.toString());
        q->bindValue(QStringLiteral(":DETECT_FILE_COUNT"), 0);

        QByteArray file;
        file.reserve(1024);
        QXmlStreamWriter w(&file);
        p->toXML(&w);
        q->bindValue(QStringLiteral(":CONTENT"), QVariant(file));
        if (!q->exec())
            err = SQLUtils::getErrorString(*q);
        modified = q->numRowsAffected() > 0;
        q->finish();
    }

    // save <cmd-file> entries
    if (err.isEmpty()) {
        if (modified)
            deleteCmdFiles(p->package, p->version);

        MySQLQuery* q = insertCmdFileQuery.get();

        for (int i = 0; i < static_cast<int>(p->cmdFiles.size()); i++) {
            // qCDebug(npackd) << p->package << p->version.getVersionString() << p->cmdFiles.at(i);
            q->bindValue(QStringLiteral(":PACKAGE"), p->package);
            Version v = p->version;
            v.normalize();
            q->bindValue(QStringLiteral(":VERSION"), v.getVersionString());

            QString path = p->cmdFiles.at(i);
            q->bindValue(QStringLiteral(":PATH"),
                    WPMUtils::normalizePath(path));

            q->bindValue(QStringLiteral(":NAME"),
                    p->getCmdFileName(i).toLower());
            if (!q->exec())
                err = SQLUtils::getErrorString(*q);

            if (!err.isEmpty())
                break;
        }

        q->finish();
    }

    packageVersions.clear();

    return err;
}

QString DBRepository::clear()
{
    Job* job = new Job(QObject::tr("Clearing the repository database"));

    clearCache();

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.1,
                QObject::tr("Clearing the packages table"));
        QString err = exec(QStringLiteral("DELETE FROM PACKAGE"));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.6,
                QObject::tr("Clearing the package versions table"));
        QString err = exec(QStringLiteral("DELETE FROM PACKAGE_VERSION"));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.13,
                QObject::tr("Clearing the licenses table"));
        QString err = exec(QStringLiteral("DELETE FROM LICENSE"));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05,
                QObject::tr("Clearing the links table"));
        QString err = exec(QStringLiteral("DELETE FROM LINK"));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05,
                QObject::tr("Clearing the tags table"));
        QString err = exec(QStringLiteral("DELETE FROM TAG"));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.03,
                QObject::tr("Clearing the command line tool definitions"));
        QString err = exec(QStringLiteral("DELETE FROM CMD_FILE"));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.02,
                QObject::tr("Clearing the categories table"));
        QString err = exec(QStringLiteral("DELETE FROM CATEGORY"));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            sub->completeWithProgress();
        }
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.02,
                QObject::tr("Clearing the table with information about installed packages"));
        QString err = exec(QStringLiteral("DELETE FROM INSTALLED"));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            sub->completeWithProgress();
        }
    }

    job->complete();

    delete job;

    return QStringLiteral("");
}

void DBRepository::clearCache()
{
    this->mutex.lock();
    this->categories.clear();
    this->licenses.clear();
    this->packageVersions.clear();
    this->packages.clear();
    this->mutex.unlock();

    readCategories();
}

void DBRepository::load(Job* job, const std::vector<QUrl *> &repositories, bool useCache, bool interactive,
        const QString &user, const QString &password,
        const QString &proxyUser, const QString &proxyPassword)
{
    QString err;
    if (repositories.size() > 0) {
        std::vector<QString> reps;
        for (auto r: repositories) {
            reps.push_back(r->toString(QUrl::FullyEncoded));
        }

        err = saveRepositories(reps);
        if (!err.isEmpty())
            job->setErrorMessage(
                    QObject::tr("Error saving the list of repositories in the database: %1").arg(
                    err));

        std::vector<std::future<QTemporaryFile*> > files;
        for (auto url: repositories) {
            Job* s = job->newSubJob(0.1,
                    QObject::tr("Downloading %1").
                    arg(url->toDisplayString()), false, true);

            Downloader::Request request(*url);
            request.user = user;
            request.password = password;
            request.proxyUser = proxyUser;
            request.proxyPassword = proxyPassword;
            request.useCache = useCache;
            request.interactive = interactive;
            std::future<QTemporaryFile*> future = std::async(
                std::launch::async,
                [s, request](){
                    return Downloader::downloadToTemporary(s, request);
                });
            files.push_back(std::move(future));
        }

        for (int i = 0; i < static_cast<int>(repositories.size()); i++) {
            files[i].wait();

            job->setProgress((i + 1.0) / repositories.size() * 0.5);
        }

        for (int i = 0; i < static_cast<int>(repositories.size()); i++) {
            if (!job->shouldProceed())
                break;

            // TODO: if the job is cancelled, not all temporary files are
            // deleted
            QTemporaryFile* tf = files.at(i).get();
            Job* s = job->newSubJob(0.49 / repositories.size(), QString(
                    QObject::tr("Repository %1 of %2")).arg(i + 1).
                    arg(repositories.size()));
            this->currentRepository = i;
            // this is currently unnecessary clearRepository(i);
            loadOne(s, tf, *repositories.at(i));
            delete tf;

            if (!s->getErrorMessage().isEmpty()) {
                job->setErrorMessage(QString(
                        QObject::tr("Error loading the repository %1: %2")).arg(
                        repositories.at(i)->toString(),
                        s->getErrorMessage()));
                break;
            }
        }
    } else {
        job->setErrorMessage(QObject::tr("No repositories defined"));
        job->setProgress(1);
    }

    // qCDebug(npackd) << "Repository::load.3";

    job->complete();
}

void DBRepository::loadOne(Job* job, QFile* f, const QUrl& url) {
    QTemporaryDir* dir = nullptr;
    QFile* xmlInZIP = nullptr;
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
                    job->setErrorMessage(
                            QObject::tr("Unzipping the repository %1 failed: %2").
                            arg(f->fileName(),
                            sub->getErrorMessage()));
                } else {
                    QString repfn = dir->path() + QStringLiteral("\\Rep.xml");
                    if (QFile::exists(repfn)) {
                        xmlInZIP = new QFile(repfn);
                        f = xmlInZIP;
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
        f->open(QFile::ReadOnly);
        Job* sub = job->newSubJob(0.9, QObject::tr("Parsing XML"));
        QXmlStreamReader reader(f);
        RepositoryXMLHandler handler(this, url, &reader);
        QString err = handler.parse();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            sub->completeWithProgress();
            job->setProgress(1);
        }
    }

    delete xmlInZIP;
    delete dir;

    job->complete();
}

void DBRepository::clearAndDownloadRepositories(Job* job,
        const std::vector<QUrl *> &repositories,
        bool interactive, const QString &user,
        const QString &password, const QString &proxyUser,
        const QString &proxyPassword, bool useCache, bool detect)
{
    bool transactionStarted = false;
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Starting an SQL transaction (tempdb)"));
        QString err = exec(QStringLiteral("BEGIN TRANSACTION"));
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

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.27,
                QObject::tr("Downloading the remote repositories and filling the local database (tempdb)"));
        load(sub, repositories, useCache, interactive, user, password, proxyUser, proxyPassword);
        if (!sub->getErrorMessage().isEmpty())
            job->setErrorMessage(sub->getErrorMessage());
    }

    if (job->shouldProceed()) {
        InstalledPackages* def = InstalledPackages::getDefault();
        if (detect) {
            Job* sub = job->newSubJob(0.4,
                    QObject::tr("Refreshing the installation status (tempdb)"));
            InstalledPackages ip;
            ip.refresh(this, sub);
            if (!sub->getErrorMessage().isEmpty())
                job->setErrorMessage(sub->getErrorMessage());

            if (job->shouldProceed()) {
                *def = ip;
                job->setErrorMessage(def->save());
            }
        } else {
            QString err = def->readRegistryDatabase();
            if (!err.isEmpty())
                job->setErrorMessage(err);
            else
                job->setProgress(job->getProgress() + 0.4);
        }
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.06,
                QObject::tr("Updating the status for installed packages in the database (tempdb)"));
        updateStatusForInstalled(sub);
        if (!sub->getErrorMessage().isEmpty())
            job->setErrorMessage(sub->getErrorMessage());
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05,
                QObject::tr("Removing packages without versions"));
        QString err = exec(QStringLiteral(
                "DELETE FROM PACKAGE WHERE STATUS=0 AND NOT EXISTS "
                "(SELECT 1 FROM PACKAGE_VERSION "
                "WHERE PACKAGE = PACKAGE.NAME AND URL <>'')"));
        if (err.isEmpty())
            sub->completeWithProgress();
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        std::vector<InstalledPackageVersion*> installed =
                InstalledPackages::getDefault()->getAll();
        QString err = saveInstalled(installed);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(0.85);

        qDeleteAll(installed);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05,
                QObject::tr("Commiting the SQL transaction (tempdb)"));
        QString err = exec(QStringLiteral("COMMIT"));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    } else {
        if (transactionStarted)
            exec(QStringLiteral("ROLLBACK"));
    }

    /*QString error;
    //tempFile.setAutoRemove(false);
    qCDebug(npackd) << "packages in tempdb" << count("SELECT COUNT(*) FROM tempdb.PACKAGE", &error);
    qCDebug(npackd) << error;
    qCDebug(npackd) << "package versions in tempdb" << count("SELECT COUNT(*) FROM tempdb.PACKAGE_VERSION", &error);
    qCDebug(npackd) << error;
    qCDebug(npackd) << tempFile.fileName();
    */

    /*
    qCDebug(npackd) << "packages in db" << count("SELECT COUNT(*) FROM PACKAGE", &error);
    qCDebug(npackd) << error;
    qCDebug(npackd) << "package versions in db" << count("SELECT COUNT(*) FROM PACKAGE_VERSION", &error);
    qCDebug(npackd) << error;
    qCDebug(npackd) << tempFile.fileName();
    */

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.1,
                QObject::tr("Reading categories"));
        QString err = readCategories();
        if (err.isEmpty()) {
            sub->completeWithProgress();
            job->setProgress(1);
        } else
            job->setErrorMessage(err);
    }

    job->complete();
}

void DBRepository::updateF5Runnable(Job *job, bool useCache)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);

    /*
    makes the process too slow
    bool b = SetThreadPriority(GetCurrentThread(),
            THREAD_MODE_BACKGROUND_BEGIN);
    */

    DBRepository tempdb;

    QTemporaryFile tempFile;
    bool tempDatabaseOpen = false;
    if (job->shouldProceed()) {
        if (!tempFile.open()) {
            job->setErrorMessage(QObject::tr("Error creating a temporary file"));
        } else {
            tempFile.close();
            job->setProgress(0.01);
        }
    }

    if (job->shouldProceed()) {
        QString err = tempdb.open(QStringLiteral("tempdb"),
                tempFile.fileName());
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            tempDatabaseOpen = true;
            job->setProgress(0.02);
        }
    }

    std::vector<QUrl*> urls;
    if (job->shouldProceed()) {
        QString err;
        urls = PackageUtils::getRepositoryURLs(&err);
        if (!err.isEmpty())
            job->setErrorMessage(QObject::tr("Cannot load the list of repositories: %1").arg(err));
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.77,
                QObject::tr("Updating the temporary database"), true, true);
        CoInitialize(nullptr);
        tempdb.clearAndDownloadRepositories(sub, urls, true, "", "", "", "", useCache);
        CoUninitialize();
    }

    if (tempDatabaseOpen)
        tempdb.db.close();

    // as this runs in a separate thread, we cannot use "this", instead
    // we create another connection to the same default database
    DBRepository dbr;

    if (job->shouldProceed()) {
        QString err = dbr.openDefault(QStringLiteral("recognize"));
        if (!err.isEmpty()) {
            job->setErrorMessage(QObject::tr("Error opening the database: %1").
                    arg(err));
        } else {
            job->setProgress(0.8);
        }
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.2,
                QObject::tr("Transferring the data from the temporary database"),
                true, true);
        dbr.transferFrom(sub, tempFile.fileName());
    }

    if (job->shouldProceed()) {
        job->setProgress(1);
    }

    qDeleteAll(urls);
    urls.clear();

    job->complete();

    /*
    if (b)
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    */
}

void DBRepository::saveAll(Job* job, Repository* r, bool replace)
{
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.07,
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

    job->complete();
}

void DBRepository::updateStatusForInstalled(Job* job)
{
    QString initialTitle = job->getTitle();

    std::unordered_set<QString> packages;
    if (job->shouldProceed()) {
        packages = InstalledPackages::getDefault()->getPackages();
        job->setProgress(0.1);
    }

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + QStringLiteral(" / ") +
                QObject::tr("Updating statuses"));
        int i = 0;
        for (auto&& it: packages) {
            updateStatus(it);

            if (!job->shouldProceed())
                break;

            i++;
            job->setProgress(0.1 + 0.9 * i / packages.size());
        }
    }

    job->setTitle(initialTitle);

    job->complete();
}

QString DBRepository::savePackages(Repository* r, bool replace)
{
    QString err;
    for (auto p: r->packages) {
        err = savePackage(p, replace);
        if (!err.isEmpty())
            break;
    }

    return err;
}

QString DBRepository::saveLicenses(Repository* r, bool replace)
{
    QString err;
    for (auto p: r->licenses) {
        err = saveLicense(p, replace);
        if (!err.isEmpty())
            break;
    }

    return err;
}

QString DBRepository::savePackageVersions(Repository* r, bool replace)
{
    QString err;
    for (auto p: r->packageVersions) {
        err = savePackageVersion(p, replace);
        if (!err.isEmpty())
            break;
    }

    return err;
}

QString DBRepository::toString(const QSqlError& e)
{
    return e.type() == QSqlError::NoError ? QStringLiteral("") : e.text();
}

QString DBRepository::readCategories()
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    this->categories.clear();

    QString sql = QStringLiteral("SELECT ID, NAME FROM CATEGORY");

    MySQLQuery q(db);

    if (!q.prepare(sql))
        err = SQLUtils::getErrorString(q);

    if (err.isEmpty()) {
        if (!q.exec())
            err = SQLUtils::getErrorString(q);
        else {
            while (q.next()) {
                categories.insert({q.value(0).toInt(),
                        q.value(1).toString()});
            }
        }
    }

    return err;
}

std::vector<QString> DBRepository::readRepositories(QString* err)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    std::vector<QString> r;

    *err = QStringLiteral("");

    QString sql = QStringLiteral("SELECT ID, URL FROM REPOSITORY ORDER BY ID");

    MySQLQuery q(db);

    if (!q.prepare(sql))
        *err = SQLUtils::getErrorString(q);

    if (err->isEmpty()) {
        if (!q.exec())
            *err = SQLUtils::getErrorString(q);
        else {
            while (q.next()) {
                r.push_back(q.value(1).toString());
            }
        }
    }

    return r;
}

QString DBRepository::getRepositorySHA1(const QString& url, QString* err)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString r;

    QString sql = QStringLiteral("SELECT SHA1 FROM REPOSITORY WHERE URL=:URL");

    MySQLQuery q(db);

    if (!q.prepare(sql))
        *err = SQLUtils::getErrorString(q);

    if (err->isEmpty()) {
        q.bindValue(QStringLiteral(":URL"), url);

        if (!q.exec())
            *err = SQLUtils::getErrorString(q);
        else {
            if (q.next()) {
                r = q.value(1).toString();
            }
        }
    }

    return r;
}

void DBRepository::setRepositorySHA1(const QString& url, const QString& sha1,
        QString* err)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    MySQLQuery q(db);

    QString sql = QStringLiteral(
            "UPDATE REPOSITORY SET SHA1=:SHA1 WHERE URL=:URL");
    if (!q.prepare(sql))
        *err = SQLUtils::getErrorString(q);

    if (err->isEmpty()) {
        q.bindValue(QStringLiteral(":SHA1"), sha1);
        q.bindValue(QStringLiteral(":URL"), url);
        if (!q.exec())
            *err = SQLUtils::getErrorString(q);
    }
}


QString DBRepository::saveRepositories(const std::vector<QString> &reps)
{
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err = exec(QStringLiteral("DELETE FROM REPOSITORY"));

    MySQLQuery q(db);

    if (err.isEmpty()) {
        QString sql = QStringLiteral("INSERT INTO REPOSITORY "
                "(ID, URL)"
                "VALUES(:ID, :URL)");
        if (!q.prepare(sql))
            err = SQLUtils::getErrorString(q);
    }

    if (err.isEmpty()) {
        for (int i = 0; i < static_cast<int>(reps.size()); i++) {
            q.bindValue(QStringLiteral(":ID"), i + 1);
            q.bindValue(QStringLiteral(":URL"), reps.at(i));
            if (!q.exec())
                err = SQLUtils::getErrorString(q);
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
    std::lock_guard<std::recursive_mutex> ml(this->mutex);

    QString err;

    std::vector<PackageVersion*> pvs = getPackageVersions_(package, &err);
    PackageVersion* newestInstallable = nullptr;
    PackageVersion* newestInstalled = nullptr;
    if (err.isEmpty()) {
        for (auto pv: pvs) {
            if (pv->installed()) {
                if (!newestInstalled ||
                        newestInstalled->version.compare(pv->version) < 0)
                    newestInstalled = pv;
            }

            // qCDebug(npackd) << pv->download.toString();

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
            if (newestInstallable)
                status = Package::NOT_INSTALLED;
            else
                status = Package::NOT_INSTALLED_NOT_AVAILABLE;
        }

        MySQLQuery q(db);
        if (!q.prepare(QStringLiteral("UPDATE PACKAGE "
                "SET STATUS=:STATUS "
                "WHERE NAME=:NAME")))
            err = SQLUtils::getErrorString(q);

        if (err.isEmpty()) {
            q.bindValue(QStringLiteral(":STATUS"), status);
            q.bindValue(QStringLiteral(":NAME"), package);
            if (!q.exec())
                err = SQLUtils::getErrorString(q);
        }
    }
    qDeleteAll(pvs);

    return err;
}

void DBRepository::transferFrom(Job* job, const QString& databaseFilename)
{
    bool transactionStarted = false;

    QString initialTitle = job->getTitle();

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + QStringLiteral(" / ") +
            QObject::tr("Attaching the temporary database"));
        QString err = exec(QStringLiteral("ATTACH '") + databaseFilename +
                QStringLiteral("' as tempdb"));
        if (err.isEmpty())
            job->setProgress(0.10);
        else
            job->setErrorMessage(err);
    }

    /*QString error;
    //tempFile.setAutoRemove(false);
    qCDebug(npackd) << "packages in tempdb" << count("SELECT COUNT(*) FROM tempdb.PACKAGE", &error);
    qCDebug(npackd) << error;
    qCDebug(npackd) << "package versions in tempdb" << count("SELECT COUNT(*) FROM tempdb.PACKAGE_VERSION", &error);
    qCDebug(npackd) << error;
    qCDebug(npackd) << tempFile.fileName();
    */

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + QStringLiteral(" / ") +
                QObject::tr("Starting an SQL transaction"));
        QString err = exec(QStringLiteral("BEGIN TRANSACTION"));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            job->setProgress(0.11);
            transactionStarted = true;
        }
    }

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + QStringLiteral(" / ") +
                QObject::tr("Clearing the database"));
        QString err = clear();
        if (err.isEmpty())
            job->setProgress(0.20);
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + QStringLiteral(" / ") +
                QObject::tr("Transferring the data from the temporary database"));

        // exec("DROP INDEX PACKAGE_VERSION_PACKAGE_NAME");
        QString err = exec(QStringLiteral(
                "INSERT INTO PACKAGE(NAME, TITLE, URL, ICON, "
                "DESCRIPTION, LICENSE, FULLTEXT, STATUS, SHORT_NAME, "
                "REPOSITORY, CATEGORY0, CATEGORY1, CATEGORY2, CATEGORY3, "
                "CATEGORY4, TITLE_FULLTEXT, STARS) "
                "SELECT NAME, TITLE, URL, ICON, DESCRIPTION, "
                "LICENSE, FULLTEXT, STATUS, SHORT_NAME, REPOSITORY, "
                "CATEGORY0, CATEGORY1, CATEGORY2, CATEGORY3, CATEGORY4, "
                "TITLE_FULLTEXT, STARS "
                "FROM tempdb.PACKAGE"));
        if (err.isEmpty())
            err = exec(QStringLiteral(
                    "INSERT INTO PACKAGE_VERSION(NAME, PACKAGE, URL, "
                    "CONTENT, MSIGUID, DETECT_FILE_COUNT) SELECT NAME, "
                    "PACKAGE, URL, CONTENT, MSIGUID, DETECT_FILE_COUNT "
                    "FROM tempdb.PACKAGE_VERSION"));
        if (err.isEmpty())
            err = exec(QStringLiteral(
                    "INSERT INTO LICENSE(NAME, TITLE, DESCRIPTION, URL) "
                    "SELECT NAME, TITLE, DESCRIPTION, URL FROM tempdb.LICENSE"));
        if (err.isEmpty())
            err = exec(QStringLiteral(
                    "INSERT INTO CATEGORY(ID, NAME, PARENT, LEVEL) "
                    "SELECT ID, NAME, PARENT, LEVEL FROM tempdb.CATEGORY"));
        if (err.isEmpty())
            err = exec(QStringLiteral(
                    "INSERT INTO LINK(PACKAGE, INDEX_, REL, HREF) "
                    "SELECT PACKAGE, INDEX_, REL, HREF FROM tempdb.LINK"));
        if (err.isEmpty())
            err = exec(QStringLiteral(
                    "INSERT INTO CMD_FILE(PACKAGE, VERSION, PATH, NAME) "
                    "SELECT PACKAGE, VERSION, PATH, NAME FROM tempdb.CMD_FILE"));
        if (err.isEmpty())
            err = exec(QStringLiteral(
                    "INSERT INTO INSTALLED(PACKAGE, VERSION, CVERSION, "
                    "WHEN_, WHERE_, DETECTION_INFO) "
                    "SELECT PACKAGE, VERSION, CVERSION, WHEN_, WHERE_, "
                    "DETECTION_INFO FROM tempdb.INSTALLED"));
        if (err.isEmpty())
            err = exec(QStringLiteral(
                    "INSERT INTO TAG(PACKAGE, VALUE) "
                    "SELECT PACKAGE, VALUE FROM tempdb.TAG"));
        if (err.isEmpty())
            job->setProgress(0.90);
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + QStringLiteral(" / ") +
                QObject::tr("Commiting the SQL transaction"));
        QString err = exec(QStringLiteral("COMMIT"));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(0.95);
    } else {
        if (transactionStarted)
            exec(QStringLiteral("ROLLBACK"));
    }

    /*
    qCDebug(npackd) << "packages in db" << count("SELECT COUNT(*) FROM PACKAGE", &error);
    qCDebug(npackd) << error;
    qCDebug(npackd) << "package versions in db" << count("SELECT COUNT(*) FROM PACKAGE_VERSION", &error);
    qCDebug(npackd) << error;
    qCDebug(npackd) << tempFile.fileName();
    */

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + QStringLiteral(" / ") +
                QObject::tr("Detaching the temporary database"));
        QString err;
        for (int i = 0; i < 10; i++) {
            err = exec(QStringLiteral("DETACH tempdb"));
            if (err.isEmpty())
                break;
            else
                Sleep(1000);
        }

        if (err.isEmpty())
            job->setProgress(0.99);
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + QStringLiteral(" / ") +
                QObject::tr("Reorganizing the database"));
        QString err = exec(QStringLiteral("VACUUM"));
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(1);
    }

    job->setTitle(initialTitle);

    job->complete();
}

QString DBRepository::openDefault(const QString& databaseName, bool readOnly)
{
    QString dir = WPMUtils::getShellDir(PackageUtils::globalMode ?
            FOLDERID_ProgramData : FOLDERID_RoamingAppData) +
            QStringLiteral("\\Npackd");
    QDir d;
    if (!d.exists(dir))
        d.mkpath(dir);

    QString path = dir + QStringLiteral("\\Data.db");

    path = QDir::toNativeSeparators(path);

    QString err = open(databaseName, path, readOnly);

    return err;
}

QString DBRepository::updateDatabase()
{
    QString err;

    bool e = false;

    // PACKAGE
    if (err.isEmpty()) {
        e = SQLUtils::tableExists(&db, QStringLiteral("PACKAGE"), &err);
    }
    if (err.isEmpty()) {
        if (e) {
            // PACKAGE.TITLE_FULLTEXT is new in 1.25
            if (!SQLUtils::columnExists(&db, "PACKAGE",
                    "TITLE_FULLTEXT", &err)) {
                exec(QStringLiteral("DROP TABLE PACKAGE"));
                e = false;
            }
        }
    }
    if (err.isEmpty()) {
        if (e) {
            // PACKAGE.STARS is new in 1.26
            if (!SQLUtils::columnExists(&db, "PACKAGE",
                    "STARS", &err)) {
                exec(QStringLiteral("DROP TABLE PACKAGE"));
                e = false;
            }
        }
    }
    if (err.isEmpty()) {
        if (!e) {
            // NULL should be stored in CATEGORYx if a package is not
            // categorized
            db.exec(QStringLiteral("CREATE TABLE PACKAGE(NAME TEXT, "
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
                    "TITLE_FULLTEXT TEXT, "
                    "STARS INTEGER"
                    ")"));
            err = toString(db.lastError());
        }
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral(
                    "CREATE UNIQUE INDEX PACKAGE_NAME ON PACKAGE(NAME)"));
            err = toString(db.lastError());
        }
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral(
                    "CREATE INDEX PACKAGE_SHORT_NAME ON PACKAGE(SHORT_NAME)"));
            err = toString(db.lastError());
        }
    }

    // REPOSITORY
    if (err.isEmpty()) {
        e = SQLUtils::tableExists(&db, QStringLiteral("REPOSITORY"), &err);
    }

    // CATEGORY
    if (err.isEmpty()) {
        e = SQLUtils::tableExists(&db, QStringLiteral("CATEGORY"), &err);
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral(
                    "CREATE TABLE CATEGORY(ID INTEGER PRIMARY KEY ASC, "
                    "NAME TEXT, PARENT INTEGER, LEVEL INTEGER)"));
            err = toString(db.lastError());
        }
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral(
                    "CREATE UNIQUE INDEX CATEGORY_ID ON CATEGORY(ID)"));
            err = toString(db.lastError());
        }
    }

    // INSTALLED is new in 1.22
    if (err.isEmpty()) {
        e = SQLUtils::tableExists(&db, QStringLiteral("INSTALLED"), &err);
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral("CREATE TABLE INSTALLED("
                    "PACKAGE TEXT, VERSION TEXT, CVERSION TEXT, WHEN_ INTEGER, "
                    "WHERE_ TEXT, DETECTION_INFO TEXT)"));
            err = toString(db.lastError());
        }
    }

    // PACKAGE_VERSION
    if (err.isEmpty()) {
        e = SQLUtils::tableExists(&db, QStringLiteral("PACKAGE_VERSION"), &err);
    }

    if (err.isEmpty()) {
        if (e) {
            // PACKAGE_VERSION.URL is new in 1.18.4
            if (!SQLUtils::columnExists(&db, QStringLiteral("PACKAGE_VERSION"),
                    QStringLiteral("URL"), &err)) {
                exec(QStringLiteral("DROP TABLE PACKAGE_VERSION"));
                e = false;
            }
        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral(
                    "CREATE TABLE PACKAGE_VERSION(NAME TEXT, "
                    "PACKAGE TEXT, URL TEXT, "
                    "CONTENT BLOB, MSIGUID TEXT, DETECT_FILE_COUNT INTEGER)"));
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral(
                    "CREATE INDEX PACKAGE_VERSION_PACKAGE ON PACKAGE_VERSION("
                    "PACKAGE)"));
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral(
                    "CREATE UNIQUE INDEX PACKAGE_VERSION_PACKAGE_NAME ON "
                    "PACKAGE_VERSION("
                    "PACKAGE, NAME)"));
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        db.exec(QStringLiteral(
                "CREATE INDEX IF NOT EXISTS PACKAGE_VERSION_MSIGUID ON "
                "PACKAGE_VERSION(MSIGUID)"));
        err = toString(db.lastError());
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral(
                    "CREATE INDEX PACKAGE_VERSION_DETECT_FILE_COUNT ON PACKAGE_VERSION("
                    "DETECT_FILE_COUNT)"));
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        e = SQLUtils::tableExists(&db, QStringLiteral("LICENSE"), &err);
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral("CREATE TABLE LICENSE(NAME TEXT, "
                    "TITLE TEXT, "
                    "DESCRIPTION TEXT, "
                    "URL TEXT"
                    ")"));
            err = toString(db.lastError());
        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral(
                    "CREATE UNIQUE INDEX LICENSE_NAME ON LICENSE(NAME)"));
            err = toString(db.lastError());
        }
    }

    // REPOSITORY
    if (err.isEmpty()) {
        e = SQLUtils::tableExists(&db, QStringLiteral("REPOSITORY"), &err);
    }
    bool ce = false;
    if (err.isEmpty()) {
        ce = SQLUtils::columnExists(&db, QStringLiteral("REPOSITORY"),
                QStringLiteral("SHA1"), &err);
    }
    if (err.isEmpty()) {
        if (e && !ce) {
            db.exec(QStringLiteral("DROP TABLE REPOSITORY"));
            err = toString(db.lastError());
            e = false;
        }
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral(
                    "CREATE TABLE REPOSITORY(ID INTEGER PRIMARY KEY ASC, "
                    "URL TEXT, SHA1 TEXT)"));
            err = toString(db.lastError());
        }
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec(QStringLiteral(
                    "CREATE UNIQUE INDEX REPOSITORY_ID ON REPOSITORY(ID)"));
            err = toString(db.lastError());
        }
    }

    // LINK. This table is new in Npackd 1.20.
    if (err.isEmpty()) {
        e = SQLUtils::tableExists(&db, QStringLiteral("LINK"), &err);
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE TABLE LINK("
                    "PACKAGE TEXT NOT NULL, INDEX_ INTEGER NOT NULL, "
                    "REL TEXT NOT NULL, "
                    "HREF TEXT NOT NULL)");
            err = toString(db.lastError());
        }
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE INDEX LINK_PACKAGE ON LINK("
                    "PACKAGE)");
            err = toString(db.lastError());
        }
    }

    // CMD_FILE. This table is new in Npackd 1.22.
    if (err.isEmpty()) {
        e = SQLUtils::tableExists(&db, "CMD_FILE", &err);
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE TABLE CMD_FILE("
                    "PACKAGE TEXT NOT NULL, "
                    "VERSION TEXT NOT NULL, "
                    "PATH TEXT NOT NULL, "
                    "NAME TEXT NOT NULL)");
            err = toString(db.lastError());
        }
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE INDEX CMD_FILE_PACKAGE_VERSION ON CMD_FILE("
                    "PACKAGE, VERSION)");
            err = toString(db.lastError());
        }
    }

    // TAG is new in 1.26
    if (err.isEmpty()) {
        e = SQLUtils::tableExists(&db, "TAG", &err);
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE TABLE TAG("
                    "PACKAGE TEXT NOT NULL, "
                    "VALUE TEXT NOT NULL)");
            err = toString(db.lastError());
        }
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE INDEX TAG_PACKAGE ON TAG(PACKAGE)");
            err = toString(db.lastError());
        }
    }

    return err;
}

QString DBRepository::open(const QString& connectionName, const QString& file,
        bool readOnly)
{
    QString err;

    // if we cannot write the file, we still try to open in read-only mode.
    // Opening a not writable file in r/w mode is slow.
    if (!readOnly) {
        QFile f(file);

        //qCDebug(npackd) << "database file writable" << f.isWritable();

        // QFile.isWritable will always return false if a file is closed
        if (!f.open(QFile::ReadWrite)) {
            readOnly = true;
            f.close();
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
    db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(file);
    if (readOnly)
        db.setConnectOptions("QSQLITE_OPEN_READONLY");

    // it takes Sqlite about 2 seconds to open the database for non-admins
    // It seems to depend on Sqlite not being able to write to the file.
    qCDebug(npackd) << "before opening the database" << readOnly;
    db.open();
    qCDebug(npackd) << "after opening the database";

    err = toString(db.lastError());

    if (err.isEmpty())
        err = exec(QStringLiteral("PRAGMA busy_timeout = 30000"));

    if (err.isEmpty()) {
        if (!readOnly)
            err = exec(QStringLiteral("PRAGMA journal_mode = DELETE"));
    }

    if (err.isEmpty()) {
        err = exec(QStringLiteral("PRAGMA case_sensitive_like = on"));
    }

    if (err.isEmpty()) {
        if (!readOnly)
            err = updateDatabase();
    }

    if (err.isEmpty()) {
        err = readCategories();
    }

    return err;
}

DBRepository::PackageVersionList::~PackageVersionList()
{
    qDeleteAll(data);
}
