#include "packageutils.h"

#include "shlobj.h"

#include "installedpackages.h"
#include "wpmutils.h"

bool PackageUtils::globalMode = true;

PackageUtils::PackageUtils()
{

}

std::tuple<QStringList, QStringList, bool, QString> PackageUtils::getRepositoryURLs(
        HKEY hk, const QString &path)
{
    QString err;
    bool keyExists = false;
    QStringList comments;

    WindowsRegistry wr;
    err = wr.open(hk, path, false, KEY_READ);
    QStringList urls;
    if (err.isEmpty()) {
        keyExists = true;
        DWORD size = wr.getDWORD("size", &err);
        if (err.isEmpty()) {
            for (int i = 1; i <= static_cast<int>(size); i++) {
                WindowsRegistry er;
                err = er.open(wr, QString("%1").arg(i), KEY_READ);
                if (err.isEmpty()) {
                    QString url = er.get("repository", &err);
                    if (err.isEmpty()) {
                        urls.append(url);
                        comments.append(er.get("comment", &err));
                    }
                }
            }

            // ignore any errors while reading the entries
            err = "";
        }
    }

    return std::make_tuple(urls, comments, keyExists, err);
}

QList<PackageVersion*> PackageUtils::getAddPackageVersionOptions(
        const DBRepository& dbr, const CommandLine& cl,
        QString* err)
{
    QList<PackageVersion*> ret;
    QList<CommandLine::ParsedOption *> pos = cl.getParsedOptions();

    InstalledPackages* ip = InstalledPackages::getDefault();

    for (int i = 0; i < pos.size(); i++) {
        if (!err->isEmpty())
            break;

        CommandLine::ParsedOption* po = pos.at(i);
        if (po->opt && po->opt->nameMatches("package")) {
            CommandLine::ParsedOption* ponext = nullptr;
            if (i + 1 < pos.size())
                ponext = pos.at(i + 1);

            QString package = po->value;
            if (!Package::isValidName(package)) {
                *err = QObject::tr("Invalid package name: %1").arg(package);
            }

            Package* p = nullptr;
            if (err->isEmpty()) {
                p = dbr.findOnePackage(package, err);
                if (err->isEmpty()) {
                    if (!p)
                        *err = QObject::tr("Unknown package: %1").arg(package);
                }
            }

            PackageVersion* pv = nullptr;
            if (err->isEmpty()) {
                QString version;
                if (ponext != nullptr && ponext->opt && ponext->opt->nameMatches("version"))
                    version = ponext->value;

                QString versions;
                if (ponext != nullptr && ponext->opt && ponext->opt->nameMatches("versions"))
                    versions = ponext->value;

                if (!versions.isNull()) {
                    i++;
                    Dependency v;
                    v.package = p->name;
                    if (!v.setVersions(versions)) {
                        *err = QObject::tr("Cannot parse a version range: %1").
                                arg(versions);
                    } else {
                        InstalledPackageVersion* ipv =
                                ip->findHighestInstalledMatch(v);
                        if (ipv) {
                            pv = dbr.findPackageVersion_(ipv->package,
                                    ipv->version, err);
                            if (err->isEmpty()) {
                                if (!pv) {
                                    *err = QObject::tr("Package version not found: %1 (%2) %3").
                                            arg(p->title).arg(p->name).
                                            arg(ipv->version.getVersionString());
                                }
                            }
                            delete ipv;
                        } else {
                            pv = dbr.findBestMatchToInstall(v,
                                    QList<PackageVersion*>(), err);
                            if (err->isEmpty()) {
                                if (!pv) {
                                    *err = QObject::tr("Package version not found: %1 (%2) %3").
                                            arg(p->title).arg(p->name).
                                            arg(versions);
                                }
                            }
                        }
                    }
                } else if (version.isNull()) {
                    pv = dbr.findNewestInstallablePackageVersion_(
                            p->name, err);
                    if (err->isEmpty()) {
                        if (!pv) {
                            *err = QObject::tr("No installable version was found for the package %1 (%2)").
                                    arg(p->title).arg(p->name);
                        }
                    }
                } else {
                    i++;
                    Version v;
                    if (!v.setVersion(version)) {
                        *err = QObject::tr("Cannot parse version: %1").
                                arg(version);
                    } else {
                        pv = dbr.findPackageVersion_(p->name, v,
                                err);
                        if (err->isEmpty()) {
                            if (!pv) {
                                *err = QObject::tr("Package version not found: %1 (%2) %3").
                                        arg(p->title).arg(p->name).arg(version);
                            }
                        }
                    }
                }
            }

            if (pv)
                ret.append(pv);

            delete p;
        }
    }

    return ret;
}

QList<Dependency *> PackageUtils::getPackageVersionOptions(const CommandLine &cl, QString *err)
{
    DBRepository* dbr = DBRepository::getDefault();

    QList<Dependency*> ret;
    QList<CommandLine::ParsedOption *> pos = cl.getParsedOptions();

    for (int i = 0; i < pos.size(); i++) {
        if (!err->isEmpty())
            break;

        CommandLine::ParsedOption* po = pos.at(i);
        if (po->opt && po->opt->nameMatches("package")) {
            CommandLine::ParsedOption* ponext = nullptr;
            if (i + 1 < pos.size())
                ponext = pos.at(i + 1);

            QString package = po->value;
            if (!Package::isValidName(package)) {
                *err = QObject::tr("Invalid package name: %1").arg(package);
            }

            Package* p = nullptr;
            if (err->isEmpty()) {
                p = dbr->findOnePackage(package, err);
                if (err->isEmpty()) {
                    if (!p)
                        *err = QObject::tr("Unknown package: %1").arg(package);
                }
            }

            if (err->isEmpty()) {
                QString version;
                if (ponext != nullptr && ponext->opt && ponext->opt->nameMatches("version"))
                    version = ponext->value;

                QString versions;
                if (ponext != nullptr && ponext->opt && ponext->opt->nameMatches("versions"))
                    versions = ponext->value;

                Dependency* dep = nullptr;
                if (!versions.isNull()) {
                    i++;
                    dep = new Dependency();
                    dep->package = p->name;
                    if (!dep->setVersions(versions)) {
                        *err = QObject::tr("Cannot parse a version range: %1").
                                arg(versions);
                        delete dep;
                        dep = nullptr;
                    }
                } else if (version.isNull()) {
                    dep = new Dependency();
                    dep->package = p->name;
                    dep->setUnboundedVersions();
                } else {
                    i++;
                    Version v;
                    if (!v.setVersion(version)) {
                        *err = QObject::tr("Cannot parse version: %1").
                                arg(version);
                    } else {
                        dep = new Dependency();
                        dep->package = p->name;
                        dep->setExactVersion(v);
                    }
                }
                if (dep)
                    ret.append(dep);
            }

            delete p;
        }
    }

    return ret;
}

QString PackageUtils::validateFullPackageName(const QString &n)
{
    if (n.length() == 0) {
        return QObject::tr("Empty ID");
    } else {
        int pos = n.indexOf(QStringLiteral(".."));
        if (pos >= 0)
            return QString(QObject::tr("Empty segment at position %1 in %2")).
                    arg(pos + 1).arg(n);

        pos = n.indexOf(QStringLiteral("--"));
        if (pos >= 0)
            return QString(QObject::tr("-- at position %1 in %2")).
                    arg(pos + 1).arg(n);

        QStringList parts = n.split('.', Qt::SkipEmptyParts);
        for (int j = 0; j < parts.count(); j++) {
            QString part = parts.at(j);

            int pos = part.indexOf(QStringLiteral("--"));
            if (pos >= 0)
                return QString(QObject::tr("-- at position %1 in %2")).
                        arg(pos + 1).arg(part);

            if (!part.isEmpty()) {
                QChar c = part.at(0);
                if (!((c >= '0' && c <= '9') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c == '_') ||
                        (c >= 'a' && c <= 'z') ||
                        c.isLetter()))
                    return QString(QObject::tr("Wrong character at position 1 in %1")).
                            arg(part);
            }

            for (int i = 1; i < part.length() - 1; i++) {
                QChar c = part.at(i);
                if (!((c >= '0' && c <= '9') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c == '_') ||
                        (c == '-') ||
                        (c >= 'a' && c <= 'z') ||
                        c.isLetter()))
                    return QString(QObject::tr("Wrong character at position %1 in %2")).
                            arg(i + 1).arg(part);
            }

            if (!part.isEmpty()) {
                QChar c = part.at(part.length() - 1);
                if (!((c >= '0' && c <= '9') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c == '_') ||
                        (c >= 'a' && c <= 'z') ||
                        c.isLetter()))
                    return QString(QObject::tr("Wrong character at position %1 in %2")).
                            arg(part.length()).arg(part);
            }
        }
    }

    return QStringLiteral("");
}

QString PackageUtils::makeValidFullPackageName(const QString &name)
{
    QString r(name);
    QStringList parts = r.split('.', Qt::SkipEmptyParts);
    for (int j = 0; j < parts.count(); ) {
        QString part = parts.at(j);

        if (!part.isEmpty()) {
            QChar c = part.at(0);
            if (!((c >= '0' && c <= '9') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c == '_') ||
                    (c >= 'a' && c <= 'z') ||
                    c.isLetter()))
                part[0] = '_';
        }

        for (int i = 1; i < part.length() - 1; i++) {
            QChar c = part.at(i);
            if (!((c >= '0' && c <= '9') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c == '_') ||
                    (c == '-') ||
                    (c >= 'a' && c <= 'z') ||
                    c.isLetter()))
                part[i] = '_';
        }

        if (!part.isEmpty()) {
            QChar c = part.at(part.length() - 1);
            if (!((c >= '0' && c <= '9') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c == '_') ||
                    (c >= 'a' && c <= 'z') ||
                    c.isLetter()))
                part[part.length() - 1] = '_';
        }

        if (part.isEmpty())
            parts.removeAt(j);
        else {
            parts.replace(j, part);
            j++;
        }
    }
    r = parts.join(".");
    if (r.isEmpty())
        r = '_';
    return r;
}

QString PackageUtils::getInstallationDirectory()
{
    QString v;

    WindowsRegistry npackd;
    QString err = npackd.open(HKEY_LOCAL_MACHINE,
        QStringLiteral("SOFTWARE\\Policies\\Npackd"), false, KEY_READ);
    if (err.isEmpty()) {
        v = npackd.get(QStringLiteral("path"), &err);
    }

    if (v.isEmpty()) {
        err = npackd.open(
            PackageUtils::globalMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            QStringLiteral("Software\\Npackd\\Npackd"), false, KEY_READ);
        if (err.isEmpty()) {
            v = npackd.get(QStringLiteral("path"), &err);
        }
    }

    if (v.isEmpty()) {
        err = npackd.open(HKEY_LOCAL_MACHINE,
                QStringLiteral("Software\\WPM\\Windows Package Manager"),
                false,
                KEY_READ);
        if (err.isEmpty()) {
            v = npackd.get(QStringLiteral("path"), &err);
        }
    }

    if (v.isEmpty())
    {
        if (PackageUtils::globalMode)
            v = WPMUtils::getProgramFilesDir();
        else
        {
            v = WPMUtils::getShellDir(CSIDL_APPDATA) +
                QStringLiteral("\\Npackd\\Installation");
            QDir dir(v);
            if (!dir.exists()) dir.mkpath(v);
        }
    }

    return v;
}

QString PackageUtils::setInstallationDirectory(const QString &dir)
{
    WindowsRegistry m(
            PackageUtils::globalMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            false, KEY_ALL_ACCESS);
    QString err;
    WindowsRegistry npackd = m.createSubKey(
            QStringLiteral("Software\\Npackd\\Npackd"), &err,
            KEY_ALL_ACCESS);
    if (err.isEmpty()) {
        npackd.set("path", dir);
    }

    return err;
}

DWORD PackageUtils::getCloseProcessType()
{
    WindowsRegistry npackd;
    QString err = npackd.open(
            HKEY_LOCAL_MACHINE,
            QStringLiteral("SOFTWARE\\Policies\\Npackd"), false, KEY_READ);
    if (err.isEmpty()) {
        DWORD v = npackd.getDWORD(QStringLiteral("closeProcessType"), &err);
        if (err.isEmpty())
            return v;
    }
    err = npackd.open(
            PackageUtils::globalMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            QStringLiteral("Software\\Npackd\\Npackd"), false, KEY_READ);
    if (err.isEmpty()) {
        DWORD v = npackd.getDWORD(QStringLiteral("closeProcessType"), &err);
        if (err.isEmpty())
            return v;
    }

    return WPMUtils::CLOSE_WINDOW;
}

void PackageUtils::setCloseProcessType(DWORD cpt)
{
    WindowsRegistry m(
            PackageUtils::globalMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            false, KEY_ALL_ACCESS);
    QString err;
    WindowsRegistry npackd = m.createSubKey(
            QStringLiteral("Software\\Npackd\\Npackd"), &err,
            KEY_ALL_ACCESS);
    if (err.isEmpty()) {
        npackd.setDWORD(QStringLiteral("closeProcessType"), cpt);
    }
}

QList<QUrl *> PackageUtils::getRepositoryURLs(QString *err)
{
    QStringList reps, comments;
    std::tie(reps, comments, *err) = getRepositoryURLsAndComments();

    QList<QUrl*> r;
    if (err->isEmpty()) {
        for (int i = 0; i < reps.count(); i++) {
            QUrl* url = new QUrl(reps.at(i));
            if (url->scheme() == "file")
                *url = QUrl::fromLocalFile(url->toLocalFile().replace('\\', '/'));
            r.append(url);
        }
    }

    return r;
}

std::tuple<QStringList, QStringList, QString> PackageUtils::getRepositoryURLsAndComments(bool used)
{
    QString suffix = used ? "Reps" : "UnusedReps";

    QStringList comments;
    QString err;

    // the most errors in this method are ignored so that we get the URLs even
    // if something cannot be done
    QString e;

    bool keyExists;
    QStringList urls;
    std::tie(urls, comments, keyExists, e) = getRepositoryURLs(
            HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Npackd\\" + suffix);

    if (!keyExists) {
        std::tie(urls, comments, keyExists, e) = getRepositoryURLs(
            PackageUtils::globalMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            "Software\\Npackd\\Npackd\\" + suffix);
    }

    bool save = false;

    // compatibility for Npackd < 1.17
    if (!keyExists && used) {
        std::tie(urls, comments, keyExists, e) = getRepositoryURLs(HKEY_CURRENT_USER,
                "Software\\Npackd\\Npackd\\repositories");
        if (urls.isEmpty())
            std::tie(urls, comments, keyExists, e) = getRepositoryURLs(HKEY_CURRENT_USER,
                    "Software\\WPM\\Windows Package Manager\\repositories");

        if (urls.isEmpty()) {
            urls.append(
                    "https://www.npackd.org/rep/zip?tag=stable");
            if (WPMUtils::is64BitWindows())
                urls.append(
                        "https://www.npackd.org/rep/zip?tag=stable64");
        }
        save = true;
    }

    QList<QUrl*> r;
    for (int i = 0; i < urls.count(); i++) {
        QUrl* url = new QUrl(urls.at(i));
        if (url->scheme() == "file")
            *url = QUrl::fromLocalFile(url->toLocalFile().replace('\\', '/'));
        r.append(url);
    }

    if (save)
        setRepositoryURLs(r, &e);

    qDeleteAll(r);
    r.clear();

    return std::make_tuple(urls, comments, err);
}

void PackageUtils::setRepositoryURLs(QList<QUrl *> &urls, QString *err)
{
    QStringList reps;
    QStringList comments;
    for (int i = 0; i < urls.size(); i++) {
        reps.append(urls.at(i)->toString(QUrl::FullyEncoded));
        comments.append(QString());
    }
    *err = setRepositoryURLsAndComments(reps, comments);
}

QString PackageUtils::setRepositoryURLsAndComments(const QStringList &urls, const QStringList &comments, bool used)
{
    QString suffix = used ? "Reps" : "UnusedReps";

    QString err;

    WindowsRegistry wr;
    err = wr.open(
            PackageUtils::globalMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            "", false, KEY_CREATE_SUB_KEY);
    if (err.isEmpty()) {
        WindowsRegistry wrr = wr.createSubKey(
                "Software\\Npackd\\Npackd\\" + suffix, &err, KEY_ALL_ACCESS);
        if (err.isEmpty()) {
            wrr.setDWORD("size", static_cast<DWORD>(urls.count()));
            for (int i = 0; i < urls.count(); i++) {
                WindowsRegistry r = wrr.createSubKey(QString("%1").arg(i + 1),
                        &err, KEY_ALL_ACCESS);
                if (err.isEmpty()) {
                    r.set("repository", urls.at(i));
                    r.set("comment", comments.at(i));
                }
            }
        }
    }

    return err;
}









