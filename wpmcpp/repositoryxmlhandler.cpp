#include "repositoryxmlhandler.h"

#include <QObject>
#include <QDebug>

#include "repository.h"
#include "wpmutils.h"
#include "packageversionfile.h"

bool RepositoryXMLHandler::isIn(const QString &first, const QString &second) const
{
    return tags.count() == 2 && tags.at(0) == first && tags.at(1) == second;
}

bool RepositoryXMLHandler::isIn(const QString &first, const QString &second,
        const QString &third) const
{
    return tags.count() == 3 && tags.at(0) == first && tags.at(1) == second &&
            tags.at(2) == third;
}

bool RepositoryXMLHandler::isIn(const QString &first, const QString &second,
        const QString &third, const QString &fourth) const
{
    return tags.count() == 4 && tags.at(0) == first && tags.at(1) == second &&
            tags.at(2) == third && tags.at(3) == fourth;
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

    if (isIn("root", "version")) {
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
    } else if (isIn("root", "version", "important-file")) {
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
    } else if (isIn("root", "version", "file")) {
        QString path = atts.value("path");
        pvf = new PackageVersionFile(path, "");
        pv->files.append(pvf);
    } else if (isIn("root", "version", "dependency")) {
        QString package = atts.value("package");
        QString versions = atts.value("versions");
        dep = new Dependency();
        pv->dependencies.append(dep);
        dep->package = package;
        if (!dep->setVersions(versions))
            error = QObject::tr("Error in attribute 'versions' in <dependency> in %1").
                    arg(pv->toString());
    } else if (isIn("root", "version", "detect-file")) {
        qDebug() << pv->toString();
        df = new DetectFile();
        pv->detectFiles.append(df);
    } else if (isIn("root", "package")) {
        QString name = atts.value("name");
        p = new Package(name, name);

        error = WPMUtils::validateFullPackageName(name);
        if (!error.isEmpty()) {
            error.prepend(QObject::tr("Error in attribute 'name' in <package>: "));
        }
    } else if (isIn("root", "license")) {
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
    if (isIn("root", "version")) {
        error = rep->savePackageVersion(pv, false);

        if (!error.isEmpty())
            error = QObject::tr("Error saving the package version %1 %2: %3").
                    arg(pv->package).arg(pv->version.getVersionString()).
                    arg(error);
        delete pv;
        pv = 0;
    } else if (isIn("root", "version", "file")) {
        pvf->content = chars;
        pvf = 0;
    } else if (isIn("root", "version", "url")) {
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
    } else if (isIn("root", "version", "sha1")) {
        pv->sha1 = chars.trimmed().toLower();
        if (!pv->sha1.isEmpty()) {
            error = WPMUtils::validateSHA1(pv->sha1);
            if (!error.isEmpty()) {
                error = QObject::tr("Invalid SHA1 for %1: %2").
                        arg(pv->toString()).arg(error);
            }
        }
    } else if (isIn("root", "version", "detect-msi")) {
        pv->msiGUID = chars.trimmed().toLower();
        if (!pv->msiGUID.isEmpty()) {
            error = WPMUtils::validateGUID(pv->msiGUID);
            if (!error.isEmpty())
                error = QObject::tr("Wrong MSI GUID for %1: %2 (%3)").
                        arg(pv->toString()).arg(pv->msiGUID).arg(error);
        }
    } else if (isIn("root", "version", "dependency", "variable")) {
        dep->var = chars.trimmed();
    } else if (isIn("root", "version", "detect-file", "path")) {
        df->path = chars.trimmed();
        df->path.replace('/', '\\');
        if (df->path.isEmpty()) {
            error = QObject::tr("Empty tag <path> under <detect-file>");
        }
    } else if (isIn("root", "version", "detect-file", "sha1")) {
        df->sha1 = chars.trimmed();
        error = WPMUtils::validateSHA1(df->sha1);
        if (!error.isEmpty()) {
            error = QObject::tr("Wrong SHA1 in <detect-file>: ").arg(error);
        }
    } else if (isIn("root", "package")) {
        error = rep->savePackage(p, false);

        if (!error.isEmpty())
            error = QObject::tr("Error saving the package %1: %2").
                    arg(p->title).arg(error);
        delete p;
        p = 0;
    } else if (isIn("root", "package", "title")) {
        p->title = chars.trimmed();
    } else if (isIn("root", "package", "title")) {
        p->title = chars.trimmed();
    } else if (isIn("root", "package", "url")) {
        p->url = chars.trimmed();
    } else if (isIn("root", "package", "description")) {
        p->description = chars.trimmed();
    } else if (isIn("root", "package", "icon")) {
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
    } else if (isIn("root", "package", "license")) {
        p->license = chars.trimmed();
    } else if (isIn("root", "package", "category")) {
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
    } else if (isIn("root", "license")) {
        error = rep->saveLicense(lic, false);

        if (!error.isEmpty())
            error = QObject::tr("Error saving the license %1: %2").
                    arg(lic->title).
                    arg(error);
        delete lic;
        lic = 0;
    } else if (isIn("root", "license", "title")) {
        lic->title = chars.trimmed();
    } else if (isIn("root", "license", "url")) {
        lic->url = chars.trimmed();
    } else if (isIn("root", "license", "description")) {
        lic->description = chars.trimmed();
    } else if (isIn("root", "spec-version")) {
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
