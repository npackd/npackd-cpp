#include "installedpackages.h"

#include <windows.h>
#include <msi.h>

#include <QDebug>

#include "windowsregistry.h"
#include "package.h"
#include "version.h"
#include "packageversion.h"
#include "repository.h"
#include "wpmutils.h"
#include "controlpanelthirdpartypm.h"
#include "msithirdpartypm.h"
#include "wellknownprogramsthirdpartypm.h"
#include "hrtimer.h"
#include "installedpackagesthirdpartypm.h"
#include "dbrepository.h"

InstalledPackages InstalledPackages::def;

static bool installedPackageVersionLessThan(const InstalledPackageVersion* a,
        const InstalledPackageVersion* b)
{
    int r = a->package.compare(b->package);
    if (r == 0) {
        r = a->version.compare(b->version);
    }

    return r > 0;
}

InstalledPackages* InstalledPackages::getDefault()
{
    return &def;
}

InstalledPackages::InstalledPackages()
{
}

InstalledPackageVersion* InstalledPackages::find(const QString& package,
        const Version& version) const
{
    return this->data.value(PackageVersion::getStringId(package, version));
}

QString InstalledPackages::detect3rdParty(AbstractThirdPartyPM *pm,
        bool replace, const QString& detectionInfoPrefix)
{
    HRTimer timer(5);
    timer.time(0);

    QString err;

    Repository rep;
    QList<InstalledPackageVersion*> installed;

    Job* job;
    if (err.isEmpty()) {
        job = new Job();
        pm->scan(job, &installed, &rep);
        if (!job->getErrorMessage().isEmpty())
            err = job->getErrorMessage();
        delete job;
    }

    timer.time(1);

    if (err.isEmpty()) {
        job = new Job();
        DBRepository::getDefault()->saveAll(job, &rep, replace);
        if (!job->getErrorMessage().isEmpty())
            err = job->getErrorMessage();
        delete job;
    }

    timer.time(2);

    QStringList packagePaths = this->getAllInstalledPackagePaths();

    // qDebug() << "InstalledPackages::detect3rdParty.0";

    if (err.isEmpty()) {
        QString windowsDir = WPMUtils::getWindowsDir();
        for (int i = 0; i < installed.count(); i++) {
            InstalledPackageVersion* ipv = installed.at(i);

            // qDebug() << ipv->package << ipv->version.getVersionString();

            // if the package version is already installed, we skip it
            InstalledPackageVersion* existing = find(ipv->package,
                    ipv->version);
            if (existing && existing->installed()) {
                // qDebug() << "existing: " << existing->toString();
                continue;
            }

            // qDebug() << "    0.1";

            // we cannot handle nested directories or paths under C:\Windows
            QString path = ipv->directory;
            if (!path.isEmpty()) {
                path = WPMUtils::normalizePath(path);
                if (WPMUtils::isUnderOrEquals(path, packagePaths) ||
                        WPMUtils::pathEquals(path, windowsDir) ||
                        WPMUtils::isUnder(path, windowsDir))
                    ipv->directory = "";
            }

            // qDebug() << "    0.2";

            processOneInstalled3rdParty(ipv);
        }
    }

    timer.time(3);

    // qDebug() << "InstalledPackages::detect3rdParty.1";

    if (!detectionInfoPrefix.isEmpty()) {
        QSet<QString> foundDetectionInfos;
        for (int i = 0; i < installed.count(); i++) {
            InstalledPackageVersion* ipv = installed.at(i);
            foundDetectionInfos.insert(ipv->detectionInfo);
        }

        // remove uninstalled packages
        QMapIterator<QString, InstalledPackageVersion*> i(data);
        while (i.hasNext()) {
            i.next();
            InstalledPackageVersion* ipv = i.value();
            if (ipv->detectionInfo.indexOf(detectionInfoPrefix) == 0 &&
                    ipv->installed() &&
                    !foundDetectionInfos.contains(ipv->detectionInfo)) {
                //qDebug() << detectionInfoPrefix <<
                //        " package removed: " << ipv->package;
                ipv->setPath("");
            }
        }
    }

    qDeleteAll(installed);

    timer.time(4);

    // qDebug() << detectionInfoPrefix;

    // timer.dump();

    return err;
}

void InstalledPackages::processOneInstalled3rdParty(
        InstalledPackageVersion* ipv)
{
    QString err;

    QDir d;

    AbstractRepository* r = AbstractRepository::getDefault_();
    QScopedPointer<PackageVersion> pv(
            r->findPackageVersion_(ipv->package, ipv->version, &err));

    if (err.isEmpty()) {
        if (!pv) {
            err = "Cannot find the package version " + ipv->package + " " +
                    ipv->version.getVersionString();
        }
    }

    PackageVersionFile* u = 0;
    if (err.isEmpty()) {
        // qDebug() << "    1";

        u = pv->findFile(".Npackd\\Uninstall.bat");
    }

    QString path = ipv->directory;

    // special case: we don't know where the package is installed and we
    // don't know how to remove it
    if (err.isEmpty() && ipv->directory.isEmpty() && u == 0 && pv) {
        u = new PackageVersionFile(".Npackd\\Uninstall.bat",
                "echo no removal procedure for this package is available"
                "\r\n"
                "exit 1"  "\r\n");
        pv->files.append(u);
    }

    if (err.isEmpty() && path.isEmpty()) {
        // qDebug() << "    2";

        Package* p = r->findPackage_(ipv->package);

        path = WPMUtils::normalizePath(WPMUtils::getInstallationDirectory()) +
                "\\NpackdDetected\\" +
                WPMUtils::makeValidFilename(p ? p->title : ipv->package, '_');
        if (d.exists(path)) {
            path = WPMUtils::findNonExistingFile(path + "-" +
                    ipv->version.getVersionString(), "");
        }
        d.mkpath(path);
        delete p;
    }

    if (err.isEmpty() && u != 0 && d.exists(path)) {
        // qDebug() << "    3";
        if (d.mkpath(path + "\\.Npackd")) {
            QFile file(path + "\\.Npackd\\Uninstall.bat");
            if (file.open(QIODevice::WriteOnly |
                    QIODevice::Truncate)) {
                QTextStream stream(&file);
                stream.setCodec("UTF-8");
                stream << u->content;
                file.close();
            }
        }
    }

    InstalledPackageVersion* ipv2 = 0;
    if (err.isEmpty()) {
        // qDebug() << "    4";
        ipv2 = this->findOrCreate(ipv->package, ipv->version, &err);
    }

    if (err.isEmpty()) {
        // qDebug() << "    5";
        ipv2->detectionInfo = ipv->detectionInfo;
        ipv2->setPath(path);
        this->saveToRegistry(ipv2);
    }
}

InstalledPackageVersion* InstalledPackages::findOrCreate(const QString& package,
        const Version& version, QString* err)
{
    *err = "";

    QString key = PackageVersion::getStringId(package, version);
    InstalledPackageVersion* r = this->data.value(key);
    if (!r) {
        r = new InstalledPackageVersion(package, version, "");
        this->data.insert(key, r);

        // *err = saveToRegistry(r);
        fireStatusChanged(package, version);
    }
    return r;
}

QString InstalledPackages::setPackageVersionPath(const QString& package,
        const Version& version,
        const QString& directory)
{
    QString err;

    InstalledPackageVersion* ipv = this->find(package, version);
    if (!ipv) {
        ipv = new InstalledPackageVersion(package, version, directory);
        this->data.insert(package + "/" + version.getVersionString(), ipv);
        err = saveToRegistry(ipv);
        fireStatusChanged(package, version);
    } else {
        ipv->setPath(directory);
        err = saveToRegistry(ipv);
        fireStatusChanged(package, version);
    }

    return err;
}

InstalledPackageVersion *InstalledPackages::findOwner(const QString &filePath) const
{
    InstalledPackageVersion* f = 0;
    QList<InstalledPackageVersion*> ipvs = this->data.values();
    for (int i = 0; i < ipvs.count(); ++i) {
        InstalledPackageVersion* ipv = ipvs.at(i);
        QString dir = ipv->getDirectory();
        if (!dir.isEmpty() && (WPMUtils::pathEquals(filePath, dir) ||
                WPMUtils::isUnder(filePath, dir))) {
            f = ipv;
            break;
        }
    }

    if (f)
        f = f->clone();

    return f;
}

QList<InstalledPackageVersion*> InstalledPackages::getAll() const
{
    QList<InstalledPackageVersion*> all = this->data.values();
    QList<InstalledPackageVersion*> r;
    for (int i = 0; i < all.count(); i++) {
        InstalledPackageVersion* ipv = all.at(i);
        if (ipv->installed())
            r.append(ipv->clone());
    }
    return r;
}

QList<InstalledPackageVersion*> InstalledPackages::getAllNoCopy() const
{
    QList<InstalledPackageVersion*> all = this->data.values();
    QList<InstalledPackageVersion*> r;
    for (int i = 0; i < all.count(); i++) {
        InstalledPackageVersion* ipv = all.at(i);
        if (ipv->installed())
            r.append(ipv);
    }
    return r;
}

QList<InstalledPackageVersion *> InstalledPackages::getByPackage(
        const QString &package) const
{
    QList<InstalledPackageVersion*> all = this->data.values();
    QList<InstalledPackageVersion*> r;
    for (int i = 0; i < all.count(); i++) {
        InstalledPackageVersion* ipv = all.at(i);
        if (ipv->installed() && ipv->package == package)
            r.append(ipv->clone());
    }
    return r;
}

QStringList InstalledPackages::getAllInstalledPackagePaths() const
{
    QStringList r;
    QList<InstalledPackageVersion*> ipvs = this->data.values();
    for (int i = 0; i < ipvs.count(); i++) {
        InstalledPackageVersion* ipv = ipvs.at(i);
        if (ipv->installed())
            r.append(ipv->getDirectory());
    }
    return r;
}

void InstalledPackages::refresh(Job *job)
{
    /* Example:
0 :  0  ms
1 :  0  ms
2 :  143  ms
3 :  31  ms
4 :  5  ms
5 :  3365  ms
6 :  199  ms
7 :  378  ms
8 :  644  ms
     */
    HRTimer timer(9);
    timer.time(0);

    // qDebug() << "InstalledPackages::refresh.0";

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        job->setHint(QObject::tr("Detecting directories deleted externally"));
        QList<InstalledPackageVersion*> ipvs = this->data.values();
        for (int i = 0; i < ipvs.count(); i++) {
            InstalledPackageVersion* ipv = ipvs.at(i);
            if (ipv->installed()) {
                QDir d(ipv->getDirectory());
                d.refresh();
                if (!d.exists()) {
                    ipv->directory = "";
                    QString err = saveToRegistry(ipv);
                    if (!err.isEmpty())
                        job->setErrorMessage(err);
                    fireStatusChanged(ipv->package, ipv->version);
                }
            }
        }
        job->setProgress(0.2);
    }

    timer.time(1);

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        job->setHint(QObject::tr("Reading registry package database"));
        QString err = readRegistryDatabase();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        job->setProgress(0.5);
    }

    timer.time(2);

    // adding well-known packages should happen before adding packages
    // determined from the list of installed packages to get better
    // package descriptions for com.microsoft.Windows64 and similar packages
    if (job->shouldProceed(QObject::tr("Adding well-known packages"))) {
        AbstractThirdPartyPM* pm = new WellKnownProgramsThirdPartyPM(
                this->packageName);
        job->setErrorMessage(detect3rdParty(pm, false));
        delete pm;

        if (job->getErrorMessage().isEmpty())
            job->setProgress(0.55);
    }

    timer.time(3);

    if (job->shouldProceed(QObject::tr("Setting the NPACKD_CL environment variable"))) {
        job->setHint("Updating NPACKD_CL");
        AbstractRepository* rep = AbstractRepository::getDefault_();
        QString err = rep->updateNpackdCLEnvVar();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(0.57);
    }

    timer.time(4);

    // qDebug() << "InstalledPackages::refresh.2";

    if (job->shouldProceed(QObject::tr("Detecting MSI packages"))) {
        // MSI package detection should happen before the detection for
        // control panel programs
        AbstractThirdPartyPM* pm = new MSIThirdPartyPM();
        job->setErrorMessage(detect3rdParty(pm, true, "msi:"));
        delete pm;

        if (job->getErrorMessage().isEmpty())
            job->setProgress(0.65);
    }

    timer.time(5);

    if (job->shouldProceed(QObject::tr("Reading the list of packages installed by Npackd"))) {
        // detecting from the list of installed packages should happen first
        // as all other packages consult the list of installed packages
        AbstractThirdPartyPM* pm = new InstalledPackagesThirdPartyPM();
        job->setErrorMessage(detect3rdParty(pm, false));
        delete pm;

        if (job->getErrorMessage().isEmpty())
            job->setProgress(0.7);
    }

    timer.time(6);

    // qDebug() << "InstalledPackages::refresh.3";

    if (job->shouldProceed(
            QObject::tr("Detecting software control panel packages"))) {
        AbstractThirdPartyPM* pm = new ControlPanelThirdPartyPM();
        job->setErrorMessage(detect3rdParty(pm, true, "control-panel:"));
        delete pm;

        if (job->getErrorMessage().isEmpty())
            job->setProgress(0.75);
    }

    timer.time(7);

    if (job->shouldProceed(
            QObject::tr("Clearing information about installed package versions in nested directories"))) {
        QString err = clearPackagesInNestedDirectories();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            job->setProgress(1);
    }

    timer.time(8);

    // timer.dump();

    job->complete();
}

QString InstalledPackages::getPath(const QString &package,
        const Version &version) const
{
    QString r;
    InstalledPackageVersion* ipv = find(package, version);
    if (ipv)
        r = ipv->getDirectory();

    return r;
}

bool InstalledPackages::isInstalled(const QString &package,
        const Version &version) const
{
    InstalledPackageVersion* ipv = find(package, version);
    return ipv && ipv->installed();
}

void InstalledPackages::fireStatusChanged(const QString &package,
        const Version &version)
{
    emit statusChanged(package, version);
}

QString InstalledPackages::clearPackagesInNestedDirectories() {
    QString err;

    QList<InstalledPackageVersion*> pvs = this->getAll();
    qSort(pvs.begin(), pvs.end(), installedPackageVersionLessThan);

    // performance improvement: normalize the paths first
    QStringList paths;
    for (int i = 0; i < pvs.count(); i++) {
        InstalledPackageVersion* pv = pvs.at(i);
        QString p = WPMUtils::normalizePath(pv->getDirectory()) + '\\';
        paths.append(p);
    }

    QString windowsDir = WPMUtils::normalizePath(WPMUtils::getWindowsDir()) +
            '\\';
    for (int j = 0; j < pvs.count(); j++) {
        InstalledPackageVersion* pv = pvs.at(j);
        QString pvdir = paths.at(j);
        if (pv->installed() && pvdir != windowsDir) {
            for (int i = j + 1; i < pvs.count(); i++) {
                InstalledPackageVersion* pv2 = pvs.at(i);
                QString pv2dir = paths.at(i);
                if (pv2->installed() && pv2dir != windowsDir) {
                    if (pv2dir.startsWith(pvdir)) {
                        pv2->setPath("");

                        err = saveToRegistry(pv2);
                        if (!err.isEmpty())
                            goto out;
                        fireStatusChanged(pv2->package, pv2->version);
                    }
                }
            }
        }
    }
out:
    return err;
}

QString InstalledPackages::readRegistryDatabase()
{
    QString err;

    this->data.clear();

    WindowsRegistry packagesWR;
    LONG e;
    err = packagesWR.open(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Npackd\\Npackd\\Packages", false, KEY_READ, &e);

    if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) {
        err = "";
    } else if (err.isEmpty()) {
        QStringList entries = packagesWR.list(&err);
        for (int i = 0; i < entries.count(); ++i) {
            QString name = entries.at(i);
            int pos = name.lastIndexOf("-");
            if (pos <= 0)
                continue;

            QString packageName = name.left(pos);
            if (!Package::isValidName(packageName))
                continue;

            QString versionName = name.right(name.length() - pos - 1);
            Version version;
            if (!version.setVersion(versionName))
                continue;

            WindowsRegistry entryWR;
            err = entryWR.open(packagesWR, name, KEY_READ);
            if (!err.isEmpty())
                continue;

            QString p = entryWR.get("Path", &err).trimmed();
            if (!err.isEmpty())
                continue;

            QString dir;
            if (p.isEmpty())
                dir = "";
            else {
                QDir d(p);
                if (d.exists()) {
                    dir = p;
                } else {
                    dir = "";
                }
            }

            if (dir.isEmpty())
                continue;

            InstalledPackageVersion* ipv = new InstalledPackageVersion(
                    packageName, version, dir);
            ipv->detectionInfo = entryWR.get("DetectionInfo", &err);
            if (!err.isEmpty()) {
                // ignore
                ipv->detectionInfo = "";
                err = "";
            }

            if (!ipv->directory.isEmpty()) {
                /*
                qDebug() << "adding " << ipv->package <<
                        ipv->version.getVersionString() << "in" <<
                        ipv->directory;*/
                this->data.insert(PackageVersion::getStringId(
                        packageName, version), ipv);

                fireStatusChanged(packageName, version);
            } else {
                delete ipv;
            }
        }
    }

    return err;
}

QString InstalledPackages::findPath_npackdcl(const Dependency& dep)
{
    QString ret;

    QString err;
    WindowsRegistry packagesWR;
    LONG e;
    err = packagesWR.open(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Npackd\\Npackd\\Packages", false, KEY_READ, &e);

    if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) {
        err = "";
    } else if (err.isEmpty()) {
        Version found = Version::EMPTY;

        QStringList entries = packagesWR.list(&err);
        for (int i = 0; i < entries.count(); ++i) {
            QString name = entries.at(i);
            int pos = name.lastIndexOf("-");
            if (pos <= 0)
                continue;

            QString packageName = name.left(pos);
            if (packageName != dep.package)
                continue;

            QString versionName = name.right(name.length() - pos - 1);
            Version version;
            if (!version.setVersion(versionName))
                continue;

            if (!dep.test(version))
                continue;

            if (found != Version::EMPTY) {
                if (version.compare(found) < 0)
                    continue;
            }

            WindowsRegistry entryWR;
            err = entryWR.open(packagesWR, name, KEY_READ);
            if (!err.isEmpty())
                continue;

            QString p = entryWR.get("Path", &err).trimmed();
            if (!err.isEmpty())
                continue;

            QString dir;
            if (p.isEmpty())
                dir = "";
            else {
                QDir d(p);
                if (d.exists()) {
                    dir = p;
                } else {
                    dir = "";
                }
            }

            if (dir.isEmpty())
                continue;

            found = version;
            ret = dir;
        }
    }

    return ret;
}

QString InstalledPackages::saveToRegistry(InstalledPackageVersion *ipv)
{
    WindowsRegistry machineWR(HKEY_LOCAL_MACHINE, false);
    QString r;
    QString keyName = "SOFTWARE\\Npackd\\Npackd\\Packages";
    Version v = ipv->version;
    v.normalize();
    QString pn = ipv->package + "-" + v.getVersionString();

    /*WPMUtils::outputTextConsole(
            "InstalledPackages::saveToRegistry " + ipv->directory + " " +
            ipv->package + " " + ipv->version.getVersionString() + " " +
            ipv->detectionInfo + "\n");*/

    if (!ipv->directory.isEmpty()) {
        WindowsRegistry wr = machineWR.createSubKey(keyName + "\\" + pn, &r);
        if (r.isEmpty()) {
            wr.set("DetectionInfo", ipv->detectionInfo);

            // for compatibility with Npackd 1.16 and earlier. They
            // see all package versions by default as "externally installed"
            wr.setDWORD("External", 0);

            r = wr.set("Path", ipv->directory);
        }
        // qDebug() << "saveToRegistry 1 " << r;
    } else {
        // qDebug() << "deleting " << pn;
        WindowsRegistry packages;
        r = packages.open(machineWR, keyName, KEY_ALL_ACCESS);
        if (r.isEmpty()) {
            r = packages.remove(pn);
        }
        // qDebug() << "saveToRegistry 2 " << r;
    }
    //qDebug() << "InstalledPackageVersion::save " << pn << " " <<
    //        this->directory;

    // qDebug() << "saveToRegistry returns " << r;

    return r;
}
