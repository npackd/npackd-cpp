#include "repositoryxmlhandler.h"

#include <QObject>
#include <QDebug>

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
            if (tag1 == "version")
                r = TAG_VERSION;
            else if (tag1 == "package")
                r = TAG_PACKAGE;
            else if (tag1 == "license")
                r = TAG_LICENSE;
            else if (tag1 == "spec-version")
                r = TAG_SPEC_VERSION;
            break;
        case 3:
            tag1 = tags.at(1);
            tag2 = tags.at(2);
            if (tag1 == "version") {
                if (tag2 == "important-file")
                    r = TAG_VERSION_IMPORTANT_FILE;
                else if (tag2 == "file")
                    r = TAG_VERSION_FILE;
                else if (tag2 == "dependency")
                    r = TAG_VERSION_DEPENDENCY;
                else if (tag2 == "detect-file")
                    r = TAG_VERSION_DETECT_FILE;
                else if (tag2 == "url")
                    r = TAG_VERSION_URL;
                else if (tag2 == "sha1")
                    r = TAG_VERSION_SHA1;
                else if (tag2 == "detect-msi")
                    r = TAG_VERSION_DETECT_MSI;
            } else if (tag1 == "package") {
                if (tag2 == "title")
                    r = TAG_PACKAGE_TITLE;
                else if (tag2 == "url")
                    r = TAG_PACKAGE_URL;
                else if (tag2 == "description")
                    r = TAG_PACKAGE_DESCRIPTION;
                else if (tag2 == "icon")
                    r = TAG_PACKAGE_ICON;
                else if (tag2 == "license")
                    r = TAG_PACKAGE_LICENSE;
                else if (tag2 == "category")
                    r = TAG_PACKAGE_CATEGORY;
            } else if (tag1 == "license") {
                if (tag2 == "title")
                    r = TAG_LICENSE_TITLE;
                else if (tag2 == "url")
                    r = TAG_LICENSE_URL;
                else if (tag2 == "description")
                    r = TAG_LICENSE_DESCRIPTION;
            }
            break;
        case 4:
            tag1 = tags.at(1);
            tag2 = tags.at(2);
            tag3 = tags.at(3);
            if (tag1 == "version") {
                if (tag2 == "dependency") {
                    if (tag3 == "variable")
                        r = TAG_VERSION_DEPENDENCY_VARIABLE;
                } else if (tag2 == "detect-file") {
                    if (tag3 == "path")
                        r = TAG_VERSION_DETECT_FILE_PATH;
                    else if (tag3 == "sha1")
                        r = TAG_VERSION_DETECT_FILE_SHA1;
                }
            }
            break;
    }
    return r;
}

RepositoryXMLHandler::RepositoryXMLHandler(DBRepository *rep) :
        lic(0), p(0), pv(0), pvf(0), dep(0), df(0)
{
    this->rep = rep;
}

RepositoryXMLHandler::~RepositoryXMLHandler()
{
    delete p;
    delete pv;
    delete lic;
}

bool RepositoryXMLHandler::startElement(const QString &namespaceURI,
    const QString &localName,
    const QString &qName,
    const QXmlAttributes &atts)
{
    if (tags.count() == 0)
        tags.append("root");
    else
        tags.append(localName);

    int where = findWhere();

    if (where == TAG_VERSION) {
        pv = new PackageVersion();
        QString packageName = atts.value("package");
        error = WPMUtils::validateFullPackageName(packageName);
        if (!error.isEmpty()) {
            error = QObject::tr("Error in the attribute 'package' in <version>: %1").
                    arg(error);
        } else {
            pv->package = packageName;
        }

        if (error.isEmpty()) {
            QString name = atts.value("name");
            if (name.isEmpty())
                name = "1.0";

            if (pv->version.setVersion(name)) {
                pv->version.normalize();
            } else {
                error = QObject::tr("Not a valid version for %1: %2").
                        arg(pv->package).arg(name);
            }
        }

        if (error.isEmpty()) {
            QString type = atts.value("type");
            if (type.isEmpty())
                type = "zip";
            if (type == "one-file")
                pv->type = 1;
            else if (type == "" || type == "zip")
                pv->type = 0;
            else {
                error = QObject::tr("Wrong value for the attribute 'type' for %1: %3").
                        arg(pv->toString()).arg(type);
            }
        }
    } else if (where == TAG_VERSION_IMPORTANT_FILE) {
        QString p = atts.value("path");
        if (p.isEmpty())
            p = atts.value("name");

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

        QString title = atts.value("title");
        if (error.isEmpty()) {
            if (title.isEmpty()) {
                error = QObject::tr("Empty 'title' attribute value for <important-file> for %1").
                        arg(pv->toString());
            }
        }

        if (error.isEmpty()) {
            pv->importantFilesTitles.append(title);
        }
    } else if (where == TAG_VERSION_FILE) {
        QString path = atts.value("path");
        pvf = new PackageVersionFile(path, "");
        pv->files.append(pvf);
    } else if (where == TAG_VERSION_DEPENDENCY) {
        QString package = atts.value("package");
        QString versions = atts.value("versions");
        dep = new Dependency();
        pv->dependencies.append(dep);
        dep->package = package;
        if (!dep->setVersions(versions))
            error = QObject::tr("Error in attribute 'versions' in <dependency> in %1").
                    arg(pv->toString());
    } else if (where == TAG_VERSION_DETECT_FILE) {
        qDebug() << pv->toString();
        df = new DetectFile();
        pv->detectFiles.append(df);
    } else if (where == TAG_PACKAGE) {
        QString name = atts.value("name");
        p = new Package(name, name);

        error = WPMUtils::validateFullPackageName(name);
        if (!error.isEmpty()) {
            error.prepend(QObject::tr("Error in attribute 'name' in <package>: "));
        }
    } else if (where == TAG_LICENSE) {
        QString name = atts.value("name");
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
        pv = 0;
    } else if (where == TAG_VERSION_FILE) {
        pvf->content = chars;
        pvf = 0;
    } else if (where == TAG_VERSION_URL) {
        QString url = chars.trimmed();
        if (!url.isEmpty()) {
            pv->download.setUrl(url, QUrl::StrictMode);
            QUrl d = pv->download;
            if (!d.isValid() || d.isRelative() ||
                    (d.scheme() != "http" && d.scheme() != "https")) {
                error = QObject::tr("Not a valid download URL for %1: %2").
                        arg(pv->package).arg(url);
            }
        }
    } else if (where == TAG_VERSION_SHA1) {
        pv->sha1 = chars.trimmed().toLower();
        if (!pv->sha1.isEmpty()) {
            error = WPMUtils::validateSHA1(pv->sha1);
            if (!error.isEmpty()) {
                error = QObject::tr("Invalid SHA1 for %1: %2").
                        arg(pv->toString()).arg(error);
            }
        }
    } else if (where == TAG_VERSION_DETECT_MSI) {
        pv->msiGUID = chars.trimmed().toLower();
        if (!pv->msiGUID.isEmpty()) {
            error = WPMUtils::validateGUID(pv->msiGUID);
            if (!error.isEmpty())
                error = QObject::tr("Wrong MSI GUID for %1: %2 (%3)").
                        arg(pv->toString()).arg(pv->msiGUID).arg(error);
        }
    } else if (where == TAG_VERSION_DEPENDENCY_VARIABLE) {
        dep->var = chars.trimmed();
    } else if (where == TAG_VERSION_DETECT_FILE_PATH) {
        df->path = chars.trimmed();
        df->path.replace('/', '\\');
        if (df->path.isEmpty()) {
            error = QObject::tr("Empty tag <path> under <detect-file>");
        }
    } else if (where == TAG_VERSION_DETECT_FILE_SHA1) {
        df->sha1 = chars.trimmed();
        error = WPMUtils::validateSHA1(df->sha1);
        if (!error.isEmpty()) {
            error = QObject::tr("Wrong SHA1 in <detect-file>: ").arg(error);
        }
    } else if (where == TAG_PACKAGE) {
        error = rep->savePackage(p, false);

        if (!error.isEmpty())
            error = QObject::tr("Error saving the package %1: %2").
                    arg(p->title).arg(error);
        delete p;
        p = 0;
    } else if (where == TAG_PACKAGE_TITLE) {
        p->title = chars.trimmed();
    } else if (where == TAG_PACKAGE_URL) {
        p->url = chars.trimmed();
    } else if (where == TAG_PACKAGE_DESCRIPTION) {
        p->description = chars.trimmed();
    } else if (where == TAG_PACKAGE_ICON) {
        p->icon = chars.trimmed();
        if (!p->icon.isEmpty()) {
            QUrl u(p->icon);
            if (!u.isValid() || u.isRelative() ||
                    !(u.scheme() == "http" || u.scheme() == "https")) {
                error = QString(
                        QObject::tr("Invalid icon URL for %1: %2")).
                        arg(p->title).arg(p->icon);
            }
        }
    } else if (where == TAG_PACKAGE_LICENSE) {
        p->license = chars.trimmed();
    } else if (where == TAG_PACKAGE_CATEGORY) {
        QString err;
        QString c = Repository::checkCategory(chars.trimmed(), &err);
        if (!err.isEmpty()) {
            err = QObject::tr("Error in category tag for %1: %2").
                    arg(p->title).arg(err);
        } else if (p->categories.contains(c)) {
            err = QObject::tr("More than one <category> %1").arg(c);
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
        lic = 0;
    } else if (where == TAG_LICENSE_TITLE) {
        lic->title = chars.trimmed();
    } else if (where == TAG_LICENSE_URL) {
        lic->url = chars.trimmed();
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
