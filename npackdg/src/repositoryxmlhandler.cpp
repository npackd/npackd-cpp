#include "repositoryxmlhandler.h"

#include <QObject>

#include "repository.h"
#include "wpmutils.h"
#include "packageversionfile.h"
#include "packageutils.h"

RepositoryXMLHandler::RepositoryXMLHandler(AbstractRepository *rep,
        const QUrl &url, QXmlStreamReader *reader) :
        rep(rep), reader(reader),
        url(url)
{

}

QString RepositoryXMLHandler::formatError()
{
    QString err = reader->errorString();
    if (err.isEmpty())
        return QString();
    else
        return QObject::tr("XML parsing error at line %1, column %2: %3").
                arg(reader->lineNumber()).arg(reader->columnNumber()).
                arg(err);
}

RepositoryXMLHandler::~RepositoryXMLHandler()
{
}

void RepositoryXMLHandler::parseVersion()
{
    PackageVersion* pv = new PackageVersion();
    QString packageName = reader->attributes().value(QStringLiteral("package")).toString();
    QString error = PackageUtils::validateFullPackageName(packageName);
    if (!error.isEmpty()) {
        error = QObject::tr("Error in the attribute 'package' in <version>: %1").
                arg(error);
    } else {
        pv->package = packageName;
    }

    if (error.isEmpty()) {
        QString name = reader->attributes().value(QStringLiteral("name")).toString();
        if (name.isEmpty())
            name = QStringLiteral("1.0");

        if (pv->version.setVersion(name)) {
            pv->version.normalize();
        } else {
            error = QObject::tr("Not a valid version for %1: %2").
                    arg(pv->package, name);
        }
    }

    if (error.isEmpty()) {
        QString type = reader->attributes().value(QStringLiteral("type")).toString();
        if (type.isEmpty())
            type = QStringLiteral("zip");
        if (type == QStringLiteral("one-file"))
            pv->type = PackageVersion::Type::ONE_FILE;
        else if (type == QStringLiteral("inno-setup"))
            pv->type = PackageVersion::Type::INNO_SETUP;
        else if (type == QStringLiteral("nsis"))
            pv->type = PackageVersion::Type::NSIS;
        else if (type.isEmpty() || type == QStringLiteral("zip"))
            pv->type = PackageVersion::Type::ZIP;
        else {
            error = QObject::tr("Wrong value for the attribute 'type' for %1: %3").
                    arg(pv->toString(), type);
        }
    }

    while (reader->readNextStartElement()){
        if (reader->name() == "important-file") {
            QString p = reader->attributes().value(QStringLiteral("path")).toString();
            if (p.isEmpty())
                p = reader->attributes().value(QStringLiteral("name")).toString();

            if (p.isEmpty()) {
                error = QObject::tr("Empty 'path' attribute value for <important-file> for %1").
                        arg(pv->toString());
            }

            if (error.isEmpty()) {
                if (std::find(pv->importantFiles.begin(), pv->importantFiles.end(), p) != pv->importantFiles.end()) {
                    error = QObject::tr("More than one <important-file> with the same 'path' attribute %1 for %2").
                            arg(p, pv->toString());
                }
            }

            if (error.isEmpty()) {
                pv->importantFiles.push_back(p);
            }

            QString title = reader->attributes().value(QStringLiteral("title")).toString();
            if (error.isEmpty()) {
                if (title.isEmpty()) {
                    error = QObject::tr("Empty 'title' attribute value for <important-file> for %1").
                            arg(pv->toString());
                }
            }

            if (error.isEmpty()) {
                pv->importantFilesTitles.push_back(title);
            }

            if (error.isEmpty())
                reader->skipCurrentElement();
        } else if (reader->name() == "sha1") {
            pv->sha1 = reader->readElementText().trimmed().toLower();
            pv->hashSumType = QCryptographicHash::Sha1;
            if (!pv->sha1.isEmpty()) {
                error = WPMUtils::validateSHA1(pv->sha1);
                if (!error.isEmpty()) {
                    error = QObject::tr("Invalid SHA1 for %1: %2").
                            arg(pv->toString(), error);
                }
            }
        } else if (reader->name() == "cmd-file") {
            QString p = reader->attributes().value(QStringLiteral("path")).toString();

            if (p.isEmpty()) {
                error = QObject::tr("Empty 'path' attribute value for <cmd-file> for %1").
                        arg(pv->toString());
            }

            if (error.isEmpty()) {
                if (std::find(pv->cmdFiles.begin(), pv->cmdFiles.end(), p) != pv->cmdFiles.end()) {
                    error = QObject::tr("More than one <cmd-file> with the same 'path' attribute %1 for %2").
                            arg(p, pv->toString());
                }
            }

            if (error.isEmpty()) {
                pv->cmdFiles.push_back(WPMUtils::normalizePath(p));
                //qCDebug(npackd) << pv->package << pv->version.getVersionString() <<
                //        p << "??";
            }

            if (error.isEmpty())
                reader->skipCurrentElement();
        } else if (reader->name() == "url") {
            QString url = reader->readElementText();
            error = WPMUtils::checkURL(this->url, &url, true);

            if (error.isEmpty()) {
                pv->download.setUrl(url);
            }
        } else if (reader->name() == "file") {
            QString path = reader->attributes().value(QStringLiteral("path")).toString();
            PackageVersionFile* pvf = new PackageVersionFile(path, QStringLiteral(""));
            pv->files.push_back(pvf);

            pvf->content = reader->readElementText();
        } else if (reader->name() == "hash-sum") {
            QString type = reader->attributes().value(QStringLiteral("type")).trimmed().toString();
            if (type.isEmpty() || type == QStringLiteral("SHA-256"))
                pv->hashSumType = QCryptographicHash::Sha256;
            else if (type == QStringLiteral("SHA-1"))
                pv->hashSumType = QCryptographicHash::Sha1;
            else
                error = QObject::tr("Error in attribute 'type' in <hash-sum> in %1").
                        arg(pv->toString());

            pv->sha1 = reader->readElementText().trimmed().toLower();
            if (!pv->sha1.isEmpty()) {
                error = WPMUtils::validateSHA256(pv->sha1);
                if (!error.isEmpty()) {
                    error = QObject::tr("Invalid SHA-256 for %1: %2").
                            arg(pv->toString(), error);
                }
            }
        } else if (reader->name() == "dependency") {
            QString package = reader->attributes().value(QStringLiteral("package")).toString();
            QString versions = reader->attributes().value(QStringLiteral("versions")).toString();
            Dependency* dep = new Dependency();
            pv->dependencies.push_back(dep);
            dep->package = package;
            if (!dep->setVersions(versions))
                error = QObject::tr("Error in attribute 'versions' in <dependency> in %1").
                        arg(pv->toString());

            while (reader->readNextStartElement()){
                if (reader->name() == "variable") {
                    dep->var = reader->readElementText().trimmed();
                } else
                    reader->skipCurrentElement();
            }
        } else
            reader->skipCurrentElement();
    }

    if (error.isEmpty()) {
        error = rep->savePackageVersion(pv, false);
    }

    if (!error.isEmpty())
        error = QObject::tr("Error saving the package version %1 %2: %3").
                arg(pv->package).arg(pv->version.getVersionString(),
                error);
    delete pv;

    if (!error.isEmpty())
        reader->raiseError(error);
}

void RepositoryXMLHandler::parsePackage()
{
    QString name = reader->attributes().value(QStringLiteral("name")).toString();
    Package* p = new Package(name, name);

    QString error = PackageUtils::validateFullPackageName(name);
    if (!error.isEmpty()) {
        error.prepend(QObject::tr("Error in attribute 'name' in <package>: "));
    }

    while (reader->readNextStartElement()){
        if (reader->name() == "title") {
            p->title = reader->readElementText();
        } else if (reader->name() == "url") {
            QString url = reader->readElementText();
            error = WPMUtils::checkURL(this->url, &url, true);

            if (error.isEmpty()) {
                p->url = url;
            }
        } else if (reader->name() == "description") {
            p->description = reader->readElementText().trimmed();
        } else if (reader->name() == "icon") {
            QString url = reader->readElementText();
            error = WPMUtils::checkURL(this->url, &url, true);

            if (error.isEmpty()) {
                p->setIcon(url);
            }
        } else if (reader->name() == "license") {
            p->license = reader->readElementText().trimmed();
        } else if (reader->name() == "category") {
            QString err;
            QString c = Repository::checkCategory(reader->readElementText().trimmed(), &err);
            if (!err.isEmpty()) {
                error = QObject::tr("Error in category tag for %1: %2").
                        arg(p->title, err);
            } else if (std::find(p->categories.begin(), p->categories.end(), c) != p->categories.end()) {
                error = QObject::tr("More than one <category> %1").arg(c);
            } else {
                p->categories.push_back(c);
            }
        } else if (reader->name() == "tag") {
            QString c = reader->readElementText().trimmed();
            QString err = PackageUtils::validateFullPackageName(c);
            if (!err.isEmpty()) {
                error = QObject::tr("Error in <tag> for %1: %2").
                        arg(p->title ,err);
            } else if (std::find(p->tags.begin(), p->tags.end(), c) != p->tags.end()) {
                error = QObject::tr("More than one <tag> %1").arg(c);
            } else {
                p->tags.push_back(c);
            }
        } else if (reader->name() == "stars") {
            QString c = reader->readElementText().trimmed();
            bool ok;
            int stars = c.toInt(&ok);
            if (!ok) {
                error = QObject::tr("Error in <stars> for %1: not a number").
                        arg(p->title);
            } else {
                p->stars = stars;
            }
        } else if (reader->name() == "link") {
            QString rel = reader->attributes().value(QStringLiteral("rel")).trimmed().toString();
            QString href = reader->attributes().value(QStringLiteral("href")).trimmed().toString();

            if (rel.isEmpty()) {
                error = QObject::tr("Empty 'rel' attribute value for <link> for %1").
                        arg(p->name);
            }

            if (error.isEmpty()) {
                error = WPMUtils::checkURL(this->url, &href, false);
            }

            if (error.isEmpty())
                p->links.insert({rel, href});

            if (error.isEmpty())
                reader->skipCurrentElement();
        } else
            reader->skipCurrentElement();
    }

    if (error.isEmpty()) {
        error = rep->savePackage(p, false);
    }

    if (!error.isEmpty())
        error = QObject::tr("Error saving the package %1: %2").
                arg(p->title, error);
    delete p;

    if (!error.isEmpty())
        reader->raiseError(error);
}

void RepositoryXMLHandler::parseLicense()
{
    QString name = reader->attributes().value(QStringLiteral("name")).toString();
    License* lic = new License(name, name);

    QString error = PackageUtils::validateFullPackageName(name);
    if (!error.isEmpty()) {
        error.prepend(QObject::tr("Error in attribute 'name' in <package>: "));
    }

    while (reader->readNextStartElement()){
        if (reader->name() == "title") {
            lic->title = reader->readElementText();
        } else if (reader->name() == "url") {
            QString url = reader->readElementText();
            error = WPMUtils::checkURL(this->url, &url, true);

            if (error.isEmpty()) {
                lic->url = url;
            }
        } else if (reader->name() == "description") {
            lic->description = reader->readElementText().trimmed();
        } else
            reader->skipCurrentElement();
    }

    if (error.isEmpty()) {
        error = rep->saveLicense(lic, false);
    }

    if (!error.isEmpty())
        error = QObject::tr("Error saving the license %1: %2").
                arg(lic->title, error);
    delete lic;

    if (!error.isEmpty())
        reader->raiseError(error);
}

QString RepositoryXMLHandler::parse()
{
    // readNextStartElement() always descends the tree!
    if (reader->readNextStartElement()) {
        if (reader->name() == "root") {
            parseRoot();
        } else
            reader->raiseError(QObject::tr("<root> expected"));
    }

    return formatError();
}

QString RepositoryXMLHandler::parseTopLevelVersion()
{
    if (reader->readNextStartElement()){
        if (reader->name() == "version") {
            parseVersion();
        } else
            reader->raiseError(QObject::tr("<version> expected"));
    }

    return formatError();
}

QString RepositoryXMLHandler::parseRoot()
{
    while (reader->readNextStartElement()){
        if (reader->name() == "version") {
            parseVersion();
        } else if (reader->name() == "package") {
            parsePackage();
        } else if (reader->name() == "license") {
            parseLicense();
        } else if (reader->name() == "spec-version") {
            QString error = Repository::checkSpecVersion(reader->readElementText().trimmed());
            if (!error.isEmpty())
                reader->raiseError(error);
        } else {
            //qCWarning(npackd) << "skipping2" << reader->name() << "tag";
            reader->skipCurrentElement();
        }
    }

    return formatError();
}
