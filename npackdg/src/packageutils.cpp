#include "packageutils.h"

#include "shlobj.h"

#include "installedpackages.h"
#include "wpmutils.h"

bool PackageUtils::globalMode = true;

PackageUtils::PackageUtils()
{

}

std::tuple<std::vector<QString>, std::vector<QString>, bool, QString> PackageUtils::getRepositoryURLs(
        HKEY hk, const QString &path)
{
    QString err;
    bool keyExists = false;
    std::vector<QString> comments;

    WindowsRegistry wr;
    err = wr.open(hk, path, false, KEY_READ);
    std::vector<QString> urls;
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
                        urls.push_back(url);
                        comments.push_back(er.get("comment", &err));
                    }
                }
            }

            // ignore any errors while reading the entries
            err = "";
        }
    }

    return std::make_tuple(urls, comments, keyExists, err);
}

std::vector<PackageVersion*> PackageUtils::getAddPackageVersionOptions(
        const DBRepository& dbr, const CommandLine& cl,
        QString* err)
{
    std::vector<PackageVersion*> ret;
    std::vector<CommandLine::ParsedOption *> pos = cl.getParsedOptions();

    InstalledPackages* ip = InstalledPackages::getDefault();

    for (int i = 0; i < static_cast<int>(pos.size()); i++) {
        if (!err->isEmpty())
            break;

        CommandLine::ParsedOption* po = pos.at(i);
        if (po->opt && po->opt->nameMatches("package")) {
            CommandLine::ParsedOption* ponext = nullptr;
            if (i + 1 < static_cast<int>(pos.size()))
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
                                            arg(p->title, p->name,
                                            ipv->version.getVersionString());
                                }
                            }
                            delete ipv;
                        } else {
                            pv = dbr.findBestMatchToInstall(v,
                                    std::vector<PackageVersion*>(), err);
                            if (err->isEmpty()) {
                                if (!pv) {
                                    *err = QObject::tr("Package version not found: %1 (%2) %3").
                                            arg(p->title, p->name,
                                            versions);
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
                                    arg(p->title, p->name);
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
                                        arg(p->title, p->name, version);
                            }
                        }
                    }
                }
            }

            if (pv)
                ret.push_back(pv);

            delete p;
        }
    }

    return ret;
}

std::vector<Dependency *> PackageUtils::getPackageVersionOptions(const CommandLine &cl, QString *err)
{
    DBRepository* dbr = DBRepository::getDefault();

    std::vector<Dependency*> ret;
    std::vector<CommandLine::ParsedOption *> pos = cl.getParsedOptions();

    for (int i = 0; i < static_cast<int>(pos.size()); i++) {
        if (!err->isEmpty())
            break;

        CommandLine::ParsedOption* po = pos.at(i);
        if (po->opt && po->opt->nameMatches("package")) {
            CommandLine::ParsedOption* ponext = nullptr;
            if (i + 1 < static_cast<int>(pos.size()))
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
                    ret.push_back(dep);
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

        std::vector<QString> parts = WPMUtils::split(n, '.', Qt::SkipEmptyParts);
        for (auto& part: parts) {
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
    std::vector<QString> parts = WPMUtils::split(r, '.', Qt::SkipEmptyParts);
    for (int j = 0; j < static_cast<int>(parts.size()); ) {
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
            parts.erase(parts.begin() + j);
        else {
            parts.at(j) = part;
            j++;
        }
    }
    r = WPMUtils::join(parts, ".");
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
            v = WPMUtils::getShellDir(FOLDERID_RoamingAppData) +
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

std::vector<QUrl *> PackageUtils::getRepositoryURLs(QString *err)
{
    std::vector<QString> reps, comments;
    std::tie(reps, comments, *err) = getRepositoryURLsAndComments();

    std::vector<QUrl*> r;
    if (err->isEmpty()) {
        for (auto& rep: reps) {
            QUrl* url = new QUrl(rep);
            if (url->scheme() == "file")
                *url = QUrl::fromLocalFile(url->toLocalFile().replace('\\', '/'));
            r.push_back(url);
        }
    }

    return r;
}

std::tuple<std::vector<QString>, std::vector<QString>, QString> PackageUtils::getRepositoryURLsAndComments(bool used)
{
    QString suffix = used ? "Reps" : "UnusedReps";

    std::vector<QString> comments;
    QString err;

    // the most errors in this method are ignored so that we get the URLs even
    // if something cannot be done
    QString e;

    bool keyExists;
    std::vector<QString> urls;
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
        if (urls.size() == 0)
            std::tie(urls, comments, keyExists, e) = getRepositoryURLs(HKEY_CURRENT_USER,
                    "Software\\WPM\\Windows Package Manager\\repositories");

        if (urls.size() == 0) {
            urls.push_back(
                    "https://www.npackd.org/rep/zip?tag=stable");
            if (WPMUtils::is64BitWindows())
                urls.push_back(
                        "https://www.npackd.org/rep/zip?tag=stable64");
        }
        save = true;
    }

    std::vector<QUrl*> r;
    for (auto& u: urls) {
        QUrl* url = new QUrl(u);
        if (url->scheme() == "file")
            *url = QUrl::fromLocalFile(url->toLocalFile().replace('\\', '/'));
        r.push_back(url);
    }

    if (save)
        setRepositoryURLs(r, &e);

    qDeleteAll(r);
    r.clear();

    return std::make_tuple(urls, comments, err);
}

void PackageUtils::setRepositoryURLs(std::vector<QUrl *> &urls, QString *err)
{
    std::vector<QString> reps;
    std::vector<QString> comments;
    for (auto& url: urls) {
        reps.push_back(url->toString(QUrl::FullyEncoded));
        comments.push_back(QString());
    }
    *err = setRepositoryURLsAndComments(reps, comments);
}

QString PackageUtils::setRepositoryURLsAndComments(const std::vector<QString> &urls, const std::vector<QString> &comments, bool used)
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
            wrr.setDWORD("size", static_cast<DWORD>(urls.size()));
            for (int i = 0; i < static_cast<int>(urls.size()); i++) {
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









