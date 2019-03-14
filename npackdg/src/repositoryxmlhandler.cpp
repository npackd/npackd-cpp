#include "repositoryxmlhandler.h"

#include <QObject>

#include "repository.h"
#include "wpmutils.h"
#include "packageversionfile.h"

int RepositoryXMLHandler::findWhere()
{
    int r = -1;
    QString tag1, tag2, tag3;
    switch (tags.count()) {
        case 2:
            tag1 = tags.at(1);
            if (tag1 == QStringLiteral("version"))
                r = TAG_VERSION;
            else if (tag1 == QStringLiteral("package"))
                r = TAG_PACKAGE;
            else if (tag1 == QStringLiteral("license"))
                r = TAG_LICENSE;
            else if (tag1 == QStringLiteral("spec-version"))
                r = TAG_SPEC_VERSION;
            break;
        case 3:
            tag1 = tags.at(1);
            tag2 = tags.at(2);
            if (tag1 == QStringLiteral("version")) {
                if (tag2 == QStringLiteral("important-file"))
                    r = TAG_VERSION_IMPORTANT_FILE;
                else if (tag2 == QStringLiteral("cmd-file"))
                    r = TAG_VERSION_CMD_FILE;
                else if (tag2 == QStringLiteral("file"))
                    r = TAG_VERSION_FILE;
                else if (tag2 == QStringLiteral("dependency"))
                    r = TAG_VERSION_DEPENDENCY;
                else if (tag2 == QStringLiteral("detect-file"))
                    r = TAG_VERSION_DETECT_FILE;
                else if (tag2 == QStringLiteral("url"))
                    r = TAG_VERSION_URL;
                else if (tag2 == QStringLiteral("sha1"))
                    r = TAG_VERSION_SHA1;
                else if (tag2 == QStringLiteral("hash-sum"))
                    r = TAG_VERSION_HASH_SUM;
                else if (tag2 == QStringLiteral("detect-msi"))
                    r = TAG_VERSION_DETECT_MSI;
            } else if (tag1 == QStringLiteral("package")) {
                if (tag2 == QStringLiteral("title"))
                    r = TAG_PACKAGE_TITLE;
                else if (tag2 == QStringLiteral("url"))
                    r = TAG_PACKAGE_URL;
                else if (tag2 == QStringLiteral("description"))
                    r = TAG_PACKAGE_DESCRIPTION;
                else if (tag2 == QStringLiteral("icon"))
                    r = TAG_PACKAGE_ICON;
                else if (tag2 == QStringLiteral("license"))
                    r = TAG_PACKAGE_LICENSE;
                else if (tag2 == QStringLiteral("category"))
                    r = TAG_PACKAGE_CATEGORY;
                else if (tag2 == QStringLiteral("link"))
                    r = TAG_PACKAGE_LINK;
            } else if (tag1 == QStringLiteral("license")) {
                if (tag2 == QStringLiteral("title"))
                    r = TAG_LICENSE_TITLE;
                else if (tag2 == QStringLiteral("url"))
                    r = TAG_LICENSE_URL;
                else if (tag2 == QStringLiteral("description"))
                    r = TAG_LICENSE_DESCRIPTION;
            }
            break;
        case 4:
            tag1 = tags.at(1);
            tag2 = tags.at(2);
            tag3 = tags.at(3);
            if (tag1 == QStringLiteral("version")) {
                if (tag2 == QStringLiteral("dependency")) {
                    if (tag3 == QStringLiteral("variable"))
                        r = TAG_VERSION_DEPENDENCY_VARIABLE;
                } else if (tag2 == QStringLiteral("detect-file")) {
                    if (tag3 == QStringLiteral("path"))
                        r = TAG_VERSION_DETECT_FILE_PATH;
                    else if (tag3 == QStringLiteral("sha1"))
                        r = TAG_VERSION_DETECT_FILE_SHA1;
                }
            }
            break;
    }
    return r;
}

RepositoryXMLHandler::RepositoryXMLHandler(AbstractRepository *rep,
        const QUrl &url) :
        rep(rep), lic(nullptr), p(nullptr), pv(nullptr), pvf(nullptr), dep(nullptr), url(url)
{
}

RepositoryXMLHandler::~RepositoryXMLHandler()
{
    delete p;
    delete pv;
    delete lic;
}

void RepositoryXMLHandler::enter(const QString &qName)
{
    tags.append(qName);
}

bool RepositoryXMLHandler::startElement(const QString &namespaceURI,
    const QString &localName,
    const QString &qName,
    const QXmlAttributes &atts)
{
    chars.clear();
    if (tags.count() == 0)
        tags.append(QStringLiteral("root"));
    else
        tags.append(localName);

    int where = findWhere();

    if (where == TAG_VERSION) {
        pv = new PackageVersion();
        QString packageName = atts.value(QStringLiteral("package"));
        error = WPMUtils::validateFullPackageName(packageName);
        if (!error.isEmpty()) {
            error = QObject::tr("Error in the attribute 'package' in <version>: %1").
                    arg(error);
        } else {
            pv->package = packageName;
        }

        if (error.isEmpty()) {
            QString name = atts.value(QStringLiteral("name"));
            if (name.isEmpty())
                name = QStringLiteral("1.0");

            if (pv->version.setVersion(name)) {
                pv->version.normalize();
            } else {
                error = QObject::tr("Not a valid version for %1: %2").
                        arg(pv->package).arg(name);
            }
        }

        if (error.isEmpty()) {
            QString type = atts.value(QStringLiteral("type"));
            if (type.isEmpty())
                type = QStringLiteral("zip");
            if (type == QStringLiteral("one-file"))
                pv->type = 1;
            else if (type.isEmpty() || type == QStringLiteral("zip"))
                pv->type = 0;
            else {
                error = QObject::tr("Wrong value for the attribute 'type' for %1: %3").
                        arg(pv->toString()).arg(type);
            }
        }
    } else if (where == TAG_VERSION_IMPORTANT_FILE) {
        QString p = atts.value(QStringLiteral("path"));
        if (p.isEmpty())
            p = atts.value(QStringLiteral("name"));

        if (p.isEmpty()) {
            error = QObject::tr("Empty 'path' attribute value for <important-file> for %1").
                    arg(pv->toString());
        }

        if (error.isEmpty()) {
            if (pv->importantFiles.contains(p)) {
                error = QObject::tr("More than one <important-file> with the same 'path' attribute %1 for %2").
                        arg(p).arg(pv->toString());
            }
        }

        if (error.isEmpty()) {
            pv->importantFiles.append(p);
        }

        QString title = atts.value(QStringLiteral("title"));
        if (error.isEmpty()) {
            if (title.isEmpty()) {
                error = QObject::tr("Empty 'title' attribute value for <important-file> for %1").
                        arg(pv->toString());
            }
        }

        if (error.isEmpty()) {
            pv->importantFilesTitles.append(title);
        }
    } else if (where == TAG_VERSION_CMD_FILE) {
        QString p = atts.value(QStringLiteral("path"));

        if (p.isEmpty()) {
            error = QObject::tr("Empty 'path' attribute value for <cmd-file> for %1").
                    arg(pv->toString());
        }

        if (error.isEmpty()) {
            if (pv->cmdFiles.contains(p)) {
                error = QObject::tr("More than one <cmd-file> with the same 'path' attribute %1 for %2").
                        arg(p).arg(pv->toString());
            }
        }

        if (error.isEmpty()) {
            pv->cmdFiles.append(WPMUtils::normalizePath(p));
            //qCDebug(npackd) << pv->package << pv->version.getVersionString() <<
            //        p << "??";
        }
    } else if (where == TAG_VERSION_FILE) {
        QString path = atts.value(QStringLiteral("path"));
        pvf = new PackageVersionFile(path, QStringLiteral(""));
        pv->files.append(pvf);
    } else if (where == TAG_VERSION_HASH_SUM) {
        QString type = atts.value(QStringLiteral("type")).trimmed();
        if (type.isEmpty() || type == QStringLiteral("SHA-256"))
            pv->hashSumType = QCryptographicHash::Sha256;
        else if (type == QStringLiteral("SHA-1"))
            pv->hashSumType = QCryptographicHash::Sha1;
        else
            error = QObject::tr("Error in attribute 'type' in <hash-sum> in %1").
                    arg(pv->toString());
    } else if (where == TAG_VERSION_DEPENDENCY) {
        QString package = atts.value(QStringLiteral("package"));
        QString versions = atts.value(QStringLiteral("versions"));
        dep = new Dependency();
        pv->dependencies.append(dep);
        dep->package = package;
        if (!dep->setVersions(versions))
            error = QObject::tr("Error in attribute 'versions' in <dependency> in %1").
                    arg(pv->toString());
    } else if (where == TAG_PACKAGE) {
        QString name = atts.value(QStringLiteral("name"));
        p = new Package(name, name);

        error = WPMUtils::validateFullPackageName(name);
        if (!error.isEmpty()) {
            error.prepend(QObject::tr("Error in attribute 'name' in <package>: "));
        }
    } else if (where == TAG_PACKAGE_LINK) {
        QString rel = atts.value(QStringLiteral("rel")).trimmed();
        QString href = atts.value(QStringLiteral("href")).trimmed();

        if (rel.isEmpty()) {
            error = QObject::tr("Empty 'rel' attribute value for <link> for %1").
                    arg(p->name);
        }

        if (error.isEmpty()) {
            error = WPMUtils::checkURL(this->url, &href, false);
        }

        if (error.isEmpty())
            p->links.insert(rel, href);
    } else if (where == TAG_LICENSE) {
        QString name = atts.value(QStringLiteral("name"));
        lic = new License(name, name);

        error = WPMUtils::validateFullPackageName(name);
        if (!error.isEmpty()) {
            error.prepend(QObject::tr("Error in attribute 'name' in <package>: "));
        }
    }

    return error.isEmpty();
}

bool RepositoryXMLHandler::endElement(const QString &namespaceURI,
    const QString &localName, const QString &qName)
{
    int where = findWhere();
    if (where == TAG_VERSION) {
        error = rep->savePackageVersion(pv, false);

        if (!error.isEmpty())
            error = QObject::tr("Error saving the package version %1 %2: %3").
                    arg(pv->package).arg(pv->version.getVersionString()).
                    arg(error);
        delete pv;
        pv = nullptr;
    } else if (where == TAG_VERSION_FILE) {
        pvf->content = chars;
        pvf = nullptr;
    } else if (where == TAG_VERSION_URL) {
        QString url = chars;
        error = WPMUtils::checkURL(this->url, &url, true);

        if (error.isEmpty()) {
            pv->download.setUrl(url);
        }
    } else if (where == TAG_VERSION_SHA1) {
        pv->sha1 = chars.trimmed().toLower();
        pv->hashSumType = QCryptographicHash::Sha1;
        if (!pv->sha1.isEmpty()) {
            error = WPMUtils::validateSHA1(pv->sha1);
            if (!error.isEmpty()) {
                error = QObject::tr("Invalid SHA1 for %1: %2").
                        arg(pv->toString()).arg(error);
            }
        }
    } else if (where == TAG_VERSION_HASH_SUM) {
        pv->sha1 = chars.trimmed().toLower();
        if (!pv->sha1.isEmpty()) {
            error = WPMUtils::validateSHA256(pv->sha1);
            if (!error.isEmpty()) {
                error = QObject::tr("Invalid SHA-256 for %1: %2").
                        arg(pv->toString()).arg(error);
            }
        }
    } else if (where == TAG_VERSION_DEPENDENCY_VARIABLE) {
        dep->var = chars.trimmed();
    } else if (where == TAG_PACKAGE) {
        error = rep->savePackage(p, false);

        if (!error.isEmpty())
            error = QObject::tr("Error saving the package %1: %2").
                    arg(p->title).arg(error);
        delete p;
        p = nullptr;
    } else if (where == TAG_PACKAGE_TITLE) {
        p->title = chars.trimmed();
    } else if (where == TAG_PACKAGE_URL) {
        QString url = chars;
        error = WPMUtils::checkURL(this->url, &url, true);

        if (error.isEmpty()) {
            p->url = url;
        }
    } else if (where == TAG_PACKAGE_DESCRIPTION) {
        p->description = chars.trimmed();
    } else if (where == TAG_PACKAGE_ICON) {
        QString url = chars;
        error = WPMUtils::checkURL(this->url, &url, true);

        if (error.isEmpty()) {
            p->setIcon(url);
        }
    } else if (where == TAG_PACKAGE_LICENSE) {
        p->license = chars.trimmed();
    } else if (where == TAG_PACKAGE_CATEGORY) {
        QString err;
        QString c = Repository::checkCategory(chars.trimmed(), &err);
        if (!err.isEmpty()) {
            error = QObject::tr("Error in category tag for %1: %2").
                    arg(p->title).arg(err);
        } else if (p->categories.contains(c)) {
            error = QObject::tr("More than one <category> %1").arg(c);
        } else {
            p->categories.append(c);
        }
    } else if (where == TAG_LICENSE) {
        error = rep->saveLicense(lic, false);

        if (!error.isEmpty())
            error = QObject::tr("Error saving the license %1: %2").
                    arg(lic->title).
                    arg(error);
        delete lic;
        lic = nullptr;
    } else if (where == TAG_LICENSE_TITLE) {
        lic->title = chars.trimmed();
    } else if (where == TAG_LICENSE_URL) {
        QString url = chars;
        error = WPMUtils::checkURL(this->url, &url, true);

        if (error.isEmpty()) {
            lic->url = url;
        }
    } else if (where == TAG_LICENSE_DESCRIPTION) {
        lic->description = chars.trimmed();
    } else if (where == TAG_SPEC_VERSION) {
        this->error = Repository::checkSpecVersion(chars.trimmed());
    }
    tags.removeLast();
    chars.clear();

    return this->error.isEmpty();
}

bool RepositoryXMLHandler::characters(const QString &ch)
{
    chars.append(ch);
    return true;
}

bool RepositoryXMLHandler::fatalError(const QXmlParseException &exception)
{
    this->error = QObject::tr("XML parsing error at line %1, column %2: %3").
            arg(exception.lineNumber()).arg(exception.columnNumber()).
            arg(exception.message());
    return false;
}

QString RepositoryXMLHandler::errorString() const
{
    return error;
}
