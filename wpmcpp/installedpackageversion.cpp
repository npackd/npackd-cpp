#include "installedpackageversion.h"

#include <QDir>
#include <QDebug>

#include "windowsregistry.h"
#include "repository.h"
#include "installedpackages.h"

InstalledPackageVersion::InstalledPackageVersion(const QString &package,
        const Version &version, const QString &directory)
{
    this->package = package;
    this->version = version;
    this->directory = directory;

    //qDebug() << "InstalledPackageVersion::InstalledPackageVersion " <<
    //        package << " " << directory;
}

QString InstalledPackageVersion::getDirectory() const
{
    return this->directory;
}

InstalledPackageVersion *InstalledPackageVersion::clone() const
{
    InstalledPackageVersion* r = new InstalledPackageVersion(this->package,
            this->version, this->directory);
    r->detectionInfo = this->detectionInfo;
    return r;
}

QString InstalledPackageVersion::toString() const
{
    return this->package + " " + this->version.getVersionString() + " " +
            this->directory;
}

QString InstalledPackageVersion::getDetectionInfo() const
{
    return this->detectionInfo;
}

bool InstalledPackageVersion::installed() const
{
    return !this->getDirectory().isEmpty();
}

void InstalledPackageVersion::setPath(const QString& path)
{
    this->directory = path;
}


