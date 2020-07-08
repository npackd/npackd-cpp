#include <windows.h>
#include <shlobj.h>
#include <msi.h>

#include <QTemporaryFile>
#include <QXmlSimpleReader>
#include <QXmlInputSource>

#include "downloader.h"
#include "repository.h"
#include "downloader.h"
#include "packageversionfile.h"
#include "wpmutils.h"
#include "version.h"
#include "windowsregistry.h"
#include "wpmutils.h"
#include "installedpackages.h"
#include "dbrepository.h"
#include "repositoryxmlhandler.h"

Repository Repository::def;
QMutex Repository::mutex;

Repository::Repository(): AbstractRepository()
{
}

static bool packageVersionLessThan2(const PackageVersion* a,
        const PackageVersion* b) {
    int r = a->package.compare(b->package);
    if (r == 0) {
        r = a->version.compare(b->version);
    }

    return r < 0;
}

QList<PackageVersion*> Repository::getPackageVersions(const QString& package)
        const
{
    QList<PackageVersion*> ret = this->package2versions.values(package);

    std::sort(ret.begin(), ret.end(), packageVersionLessThan2);

    return ret;
}

QList<PackageVersion*> Repository::getPackageVersions_(const QString& package,
        QString *err) const
{
    *err = "";

    QList<PackageVersion*> ret = this->package2versions.values(package);

    std::sort(ret.begin(), ret.end(), packageVersionLessThan2);

    for (int i = 0; i < ret.count(); i++) {
        ret[i] = ret.at(i)->clone();
    }

    return ret;
}

Repository::~Repository()
{
    qDeleteAll(this->packages);
    qDeleteAll(this->packageVersions);
    qDeleteAll(this->licenses);
}

PackageVersion* Repository::findNewestInstallablePackageVersion(
        const QString &package)
{
    PackageVersion* r = nullptr;

    QList<PackageVersion*> pvs = this->getPackageVersions(package);
    for (int i = 0; i < pvs.count(); i++) {
        PackageVersion* p = pvs.at(i);
        if (r == nullptr || p->version.compare(r->version) > 0) {
            if (p->download.isValid())
                r = p;
        }
    }
    return r;
}

License* Repository::findLicense(const QString& name)
{
    for (int i = 0; i < this->licenses.count(); i++) {
        if (this->licenses.at(i)->name == name)
            return this->licenses.at(i);
    }
    return nullptr;
}

Package* Repository::findPackage(const QString& name) const
{
    for (int i = 0; i < this->packages.count(); i++) {
        if (this->packages.at(i)->name == name)
            return this->packages.at(i);
    }
    return nullptr;
}

/*
QString Repository::writeTo(const QString& filename) const
{
    QString r;

    QDomDocument doc;
    QDomElement root = doc.createElement("root");
    doc.appendChild(root);

    XMLUtils::addTextTag(root, "spec-version", "3.3");

    for (int i = 0; i < this->packages.count(); i++) {
        Package* p = packages.at(i);
        QDomElement package = doc.createElement("package");
        p->saveTo(package);
        root.appendChild(package);
    }

    for (int i = 0; i < this->packageVersions.count(); i++) {
        PackageVersion* pv = this->packageVersions.at(i);
        QDomElement version = doc.createElement("version");
        root.appendChild(version);
        pv->toXML(&version);
    }

    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream s(&file);
        doc.save(s, 4);
    } else {
        r = QString(QObject::tr("Cannot open %1 for writing")).arg(filename);
    }

    return "";
}
*/

PackageVersion* Repository::findPackageVersion(const QString& package,
        const Version& version) const
{
    PackageVersion* r = nullptr;

    QList<PackageVersion*> pvs = this->getPackageVersions(package);
    for (int i = 0; i < pvs.count(); i++) {
        PackageVersion* pv = pvs.at(i);
        if (pv->version.compare(version) == 0) {
            r = pv;
            break;
        }
    }
    return r;
}

QString Repository::checkSpecVersion(const QString &specVersion)
{
    QString err;

    Version specVersion_;
    if (!specVersion_.setVersion(specVersion)) {
        err = QString(
                QObject::tr("Invalid repository specification version: %1")).
                arg(specVersion);
    } else {
        if (specVersion_.compare(Version(4, 0)) >= 0)
            err = QString(
                    QObject::tr("Incompatible repository specification version: %1.") + " \r\n" +
                    QObject::tr("Please download a newer version of Npackd from https://github.com/tim-lebedkov/npackd/wiki/Downloads")).
                    arg(specVersion);
    }

    return err;
}

QString Repository::checkCategory(const QString &category, QString *err)
{
    *err = "";

    QString c = category.trimmed();

    if (c.isEmpty()) {
        *err = QObject::tr("Empty category tag");
    }

    QStringList parts;
    if (err->isEmpty()) {
        parts = c.split('/', Qt::KeepEmptyParts);
        if (parts.count() == 0) {
            *err = QObject::tr("Empty category tag");
        }
    }

    if (err->isEmpty()) {
        for (int j = 0; j < parts.count(); j++) {
            QString part = parts.at(j).trimmed();
            if (part.isEmpty()) {
                *err = QObject::tr("Empty sub-category");
                goto out;
            }
            parts[j] = part;
        }
out:;
    }

    return parts.join("/");
}

Package *Repository::findPackage_(const QString &name) const
{
    Package* p = findPackage(name);
    if (p)
        p = new Package(*p);
    return p;
}

void Repository::toXML(QXmlStreamWriter &w) const
{
    w.writeStartElement("repository");

    for (int i = 0; i < this->licenses.size(); i++) {
        License* lic = this->licenses.at(i);
        lic->toXML(w);
    }

    for (int i = 0; i < this->packages.size(); i++) {
        Package* p = this->packages.at(i);
        p->toXML(&w);
    }

    for (int i = 0; i < this->packageVersions.size(); i++) {
        PackageVersion* pv = this->packageVersions.at(i);
        pv->toXML(&w);
    }

    w.writeEndElement();
}

QString Repository::saveLicense(License *p, bool replace)
{
    License* fp = findLicense(p->name);
    if (!fp || replace) {
        if (!fp) {
            fp = new License(p->name, p->title);
            this->licenses.append(fp);
        }
        fp->title = p->title;
        fp->url = p->url;
        fp->description = p->description;
    }

    return "";
}

QString Repository::savePackage(Package *p, bool replace)
{
    Package* fp = findPackage(p->name);
    if (!fp || replace) {
        if (!fp) {
            fp = new Package(p->name, p->title);
            this->packages.append(fp);
        }
        fp->title = p->title;
        fp->url = p->url;
        fp->setIcon(p->getIcon());
        fp->description = p->description;
        fp->license = p->license;
        fp->categories = p->categories;
    }

    return "";
}

QString Repository::savePackageVersion(PackageVersion *p, bool replace)
{
    PackageVersion* fp = findPackageVersion(p->package, p->version);
    if (!fp) {
        fp = p->clone();
        this->packageVersions.append(fp);
        this->package2versions.insert(p->package, fp);
    } else {
        if (replace) {
            this->packageVersions.removeOne(fp);
            this->package2versions.remove(p->package, fp);
            delete fp;

            fp = p->clone();
            this->packageVersions.append(fp);
            this->package2versions.insert(p->package, fp);
        }
    }

    return "";
}

PackageVersion *Repository::findPackageVersion_(const QString &package,
        const Version &version, QString *err) const
{
    *err = "";

    PackageVersion* pv = findPackageVersion(package, version);
    if (pv)
        pv = pv->clone();
    return pv;
}

License *Repository::findLicense_(const QString &name, QString* err)
{
    *err = "";
    License* r = findLicense(name);
    if (r)
        r = r->clone();
    return r;
}

QString Repository::clear()
{
    qDeleteAll(this->packages);
    this->packages.clear();

    qDeleteAll(this->packageVersions);
    this->packageVersions.clear();

    this->package2versions.clear();

    qDeleteAll(this->licenses);
    this->licenses.clear();

    return "";
}

QList<Package*> Repository::findPackagesByShortName(const QString &name) const
{
    QString suffix = "." + name;
    QList<Package*> r;
    for (int i = 0; i < this->packages.count(); i++) {
        QString n = this->packages.at(i)->name;
        if (n.endsWith(suffix) || n == name) {
            r.append(new Package(*this->packages.at(i)));
        }
    }

    return r;
}

