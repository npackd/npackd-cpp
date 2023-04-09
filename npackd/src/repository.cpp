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

std::vector<PackageVersion*> Repository::getPackageVersions(const QString& package)
        const
{
    auto it = this->package2versions.equal_range(package);

    std::vector<PackageVersion*> ret;
    for (auto i = it.first; i != it.second; ++i) {
        ret.push_back(i->second);
    }
    std::sort(ret.begin(), ret.end(), packageVersionLessThan2);

    return ret;
}

std::vector<PackageVersion*> Repository::getPackageVersions_(const QString& package,
        QString *err) const
{
    *err = "";

    auto it = this->package2versions.equal_range(package);

    std::vector<PackageVersion*> ret;
    for (auto i = it.first; i != it.second; ++i) {
        ret.push_back(i->second);
    }
    std::sort(ret.begin(), ret.end(), packageVersionLessThan2);

    for (int i = 0; i < static_cast<int>(ret.size()); i++) {
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

    std::vector<PackageVersion*> pvs = this->getPackageVersions(package);
    for (auto p: pvs) {
        if (r == nullptr || p->version.compare(r->version) > 0) {
            if (p->download.isValid())
                r = p;
        }
    }
    return r;
}

License* Repository::findLicense(const QString& name)
{
    for (auto license: licenses) {
        if (license->name == name)
            return license;
    }
    return nullptr;
}

Package* Repository::findPackage(const QString& name) const
{
    for (auto p: this->packages) {
        if (p->name == name)
            return p;
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

    std::vector<PackageVersion*> pvs = this->getPackageVersions(package);
    for (auto pv: pvs) {
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

    std::vector<QString> parts;
    if (err->isEmpty()) {
        parts = WPMUtils::split(c, '/', Qt::KeepEmptyParts);
        if (parts.size() == 0) {
            *err = QObject::tr("Empty category tag");
        }
    }

    if (err->isEmpty()) {
        for (int j = 0; j < static_cast<int>(parts.size()); j++) {
            QString part = parts.at(j).trimmed();
            if (part.isEmpty()) {
                *err = QObject::tr("Empty sub-category");
                goto out;
            }
            parts[j] = part;
        }
out:;
    }

    return WPMUtils::join(parts, "/");
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

    for (auto lic: this->licenses) {
        lic->toXML(w);
    }

    for (auto p: this->packages) {
        p->toXML(&w);
    }

    for (auto pv: this->packageVersions) {
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
            this->licenses.push_back(fp);
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
            this->packages.push_back(fp);
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
        this->packageVersions.push_back(fp);
        this->package2versions.insert({p->package, fp});
    } else {
        if (replace) {
            auto it = this->package2versions.equal_range(p->package);
            for (auto i = it.first; i != it.second; ++i) {
                if (i->second == fp) {
                    delete i->second;
                    auto next = ++i;
                    this->package2versions.erase(i, next);
                    break;
                }
            }

            auto f = std::find(this->packageVersions.begin(), this->packageVersions.end(), fp);
            if (f != this->packageVersions.end())
                this->packageVersions.erase(f);

            fp = p->clone();
            this->packageVersions.push_back(fp);
            this->package2versions.insert({p->package, fp});
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

std::vector<Package*> Repository::findPackagesByShortName(const QString &name) const
{
    QString suffix = "." + name;
    std::vector<Package*> r;
    for (auto p: packages) {
        QString n = p->name;
        if (n.endsWith(suffix) || n == name) {
            r.push_back(new Package(*p));
        }
    }

    return r;
}

