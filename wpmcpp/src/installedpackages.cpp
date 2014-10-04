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
#include "cbsthirdpartypm.h"

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

InstalledPackages::InstalledPackages() : mutex(QMutex::Recursive)
{
}

InstalledPackages::~InstalledPackages()
{
    this->mutex.lock();
    qDeleteAll(this->data);
    this->data.clear();
    this->mutex.unlock();
}

InstalledPackageVersion* InstalledPackages::findNoCopy(const QString& package,
        const Version& version) const
{
    // internal method, mutex is not used

    InstalledPackageVersion* ipv = this->data.value(
            PackageVersion::getStringId(package, version));

    return ipv;
}

InstalledPackageVersion* InstalledPackages::find(const QString& package,
        const Version& version) const
{
    this->mutex.lock();

    InstalledPackageVersion* ipv = this->data.value(
            PackageVersion::getStringId(package, version));
    if (ipv)
        ipv = ipv->clone();

    this->mutex.unlock();

    return ipv;
}

QString InstalledPackages::detect3rdParty(DBRepository* r,
        AbstractThirdPartyPM *pm,
        bool replace, const QString& detectionInfoPrefix)
{
    // this method does not manipulate "date" directly => no locking

    HRTimer timer(5);
    timer.time(0);

    QString err;

    // qDebug() << "detect3rdParty 1";

    Repository rep;
    QList<InstalledPackageVersion*> installed;

    if (err.isEmpty()) {
        Job* job = new Job();
        pm->scan(job, &installed, &rep);
        if (!job->getErrorMessage().isEmpty())
            err = job->getErrorMessage();
        delete job;
    }

    timer.time(1);

    // qDebug() << "detect3rdParty 3";

    // remove packages and versions that are not installed
    // we assume that one 3rd party package manager does not create package
    // or package version objects for another
    if (err.isEmpty()) {
        QSet<QString> packages;
        for (int i = 0; i < installed.size();i++) {
            InstalledPackageVersion* ipv = installed.at(i);
            packages.insert(ipv->package);
        }

        for (int i = 0; i < rep.packages.size(); ) {
            Package* p = rep.packages.at(i);
            if (!packages.contains(p->name)) {
                rep.packages.removeAt(i);
                rep.package2versions.remove(p->name);
                delete p;
            } else
                i++;
        }

        for (int i = 0; i < rep.packageVersions.size(); ) {
            PackageVersion* pv = rep.packageVersions.at(i);
            if (!packages.contains(pv->package)) {
                rep.packageVersions.removeAt(i);
                delete pv;
            } else
                i++;
        }
    }

    // save all detected packages and versions
    if (err.isEmpty()) {
        Job* job = new Job();
        r->saveAll(job, &rep, replace);
        if (!job->getErrorMessage().isEmpty())
            err = job->getErrorMessage();
        delete job;
    }

    timer.time(2);

    // remove all package versions that are not detected as installed anymore
    if (err.isEmpty() && !detectionInfoPrefix.isEmpty()) {
        QSet<QString> installedPVs;
        for (int i = 0; i < installed.count(); i++) {
            InstalledPackageVersion* ipv = installed.at(i);
            installedPVs.insert(ipv->package + "@" +
                    ipv->version.getVersionString());
        }

        // remove uninstalled packages
        QList<InstalledPackageVersion*> ipvs = getAll();
        for (int i = 0; i < ipvs.count(); i++) {
            InstalledPackageVersion* ipv = ipvs.at(i);
            bool same3rdPartyPM = ipv->detectionInfo.indexOf(
                    detectionInfoPrefix) == 0 ||
                    (detectionInfoPrefix == "msi:" &&
                    ipv->package.startsWith("msi.")) ||
                    (detectionInfoPrefix == "control-panel:" &&
                    ipv->package.startsWith("control-panel."));
            if (same3rdPartyPM &&
                    ipv->installed() && !installedPVs.contains(
                    ipv->package + "@" +
                    ipv->version.getVersionString())) {
                this->setPackageVersionPath(ipv->package, ipv->version, "");
            }
        }
        qDeleteAll(ipvs);
    }

    QStringList packagePaths = this->getAllInstalledPackagePaths();
    for (int i = 0; i < packagePaths.size(); i++) {
        packagePaths[i] = WPMUtils::normalizePath(packagePaths.at(i));
    }

    // qDebug() << "InstalledPackages::detect3rdParty.0";

    if (err.isEmpty()) {
        for (int i = 0; i < installed.count(); i++) {
            InstalledPackageVersion* ipv = installed.at(i);

            // qDebug() << ipv->package << ipv->version.getVersionString();

            // if the package version is already installed, we skip it
            InstalledPackageVersion* existing = find(ipv->package,
                    ipv->version);
            if (existing && existing->installed()) {
                // qDebug() << "existing: " << existing->toString();
                delete existing;
                continue;
            }

            delete existing;

            // qDebug() << "    0.1";

            // we cannot handle nested directories
            QString path = ipv->directory;
            if (!path.isEmpty()) {
                path = WPMUtils::normalizePath(path);
                if (WPMUtils::isUnderOrEquals(path, packagePaths))
                    continue;
            }

            // qDebug() << "    0.2";

            processOneInstalled3rdParty(r, ipv);
        }
    }

    // qDebug() << "detect3rdParty 5";

    timer.time(3);

    qDeleteAll(installed);

    timer.time(4);

    // qDebug() << detectionInfoPrefix;

    // timer.dump();

    return err;
}

void InstalledPackages::processOneInstalled3rdParty(DBRepository *r,
        InstalledPackageVersion* ipv)
{
    if (!ipv->directory.isEmpty()) {
        QDir d;
        if (!d.exists(ipv->directory))
            ipv->directory = "";
    }

    QString err;

    QDir d;

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

    if (err.isEmpty() && d.exists(path)) {
        err = pv->saveFiles(QDir(path));
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
    // internal method, mutex is not used

    *err = "";

    QString key = PackageVersion::getStringId(package, version);
    InstalledPackageVersion* r = this->data.value(key);
    if (!r) {
        r = new InstalledPackageVersion(package, version, "");
        this->data.insert(key, r);
    }

    return r;
}

QString InstalledPackages::setPackageVersionPath(const QString& package,
        const Version& version,
        const QString& directory)
{
    this->mutex.lock();

    QString err;

    InstalledPackageVersion* ipv = this->findNoCopy(package, version);
    if (!ipv) {
        ipv = new InstalledPackageVersion(package, version, directory);
        this->data.insert(package + "/" + version.getVersionString(), ipv);
        err = saveToRegistry(ipv);
    } else {
        ipv->setPath(directory);
        err = saveToRegistry(ipv);
    }

    this->mutex.unlock();

    fireStatusChanged(package, version);

    return err;
}

InstalledPackageVersion *InstalledPackages::findOwner(
        const QString &filePath) const
{
    this->mutex.lock();

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

    this->mutex.unlock();

    return f;
}

QList<InstalledPackageVersion*> InstalledPackages::getAll() const
{
    this->mutex.lock();

    QList<InstalledPackageVersion*> all = this->data.values();
    QList<InstalledPackageVersion*> r;
    for (int i = 0; i < all.count(); i++) {
        InstalledPackageVersion* ipv = all.at(i);
        if (ipv->installed())
            r.append(ipv->clone());
    }

    this->mutex.unlock();

    return r;
}

QList<InstalledPackageVersion *> InstalledPackages::getByPackage(
        const QString &package) const
{
    this->mutex.lock();

    QList<InstalledPackageVersion*> all = this->data.values();
    QList<InstalledPackageVersion*> r;
    for (int i = 0; i < all.count(); i++) {
        InstalledPackageVersion* ipv = all.at(i);
        if (ipv->installed() && ipv->package == package)
            r.append(ipv->clone());
    }

    this->mutex.unlock();

    return r;
}

InstalledPackageVersion* InstalledPackages::getNewestInstalled(
        const QString &package) const
{
    this->mutex.lock();

    QList<InstalledPackageVersion*> all = this->data.values();
    InstalledPackageVersion* r = 0;
    for (int i = 0; i < all.count(); i++) {
        InstalledPackageVersion* ipv = all.at(i);
        if (ipv->package == package && ipv->installed()) {
            if (!r || r->version < ipv->version)
                r = ipv;
        }
    }

    if (r)
        r = r->clone();

    this->mutex.unlock();

    return r;
}

QStringList InstalledPackages::getAllInstalledPackagePaths() const
{
    this->mutex.lock();

    QStringList r;
    QList<InstalledPackageVersion*> ipvs = this->data.values();
    for (int i = 0; i < ipvs.count(); i++) {
        InstalledPackageVersion* ipv = ipvs.at(i);
        if (ipv->installed())
            r.append(ipv->getDirectory());
    }

    this->mutex.unlock();

    return r;
}

void InstalledPackages::refresh(DBRepository *rep, Job *job)
{
    // no direct usage of "data" here => no mutex

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
        Job* sub = job->newSubJob(0.2,
                QObject::tr("Detecting directories deleted externally"));

        QList<InstalledPackageVersion*> ipvs = getAll();
        for (int i = 0; i < ipvs.count(); i++) {
            InstalledPackageVersion* ipv = ipvs.at(i);
            if (ipv->installed()) {
                QDir d(ipv->getDirectory());
                d.refresh();
                if (!d.exists()) {
                    QString err = this->setPackageVersionPath(
                            ipv->package, ipv->version, "");
                    if (!err.isEmpty())
                        job->setErrorMessage(err);
                }
            }
        }
        qDeleteAll(ipvs);

        sub->completeWithProgress();
    }

    timer.time(1);

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        Job* sub = job->newSubJob(0.6,
                QObject::tr("Reading registry package database"));
        QString err = readRegistryDatabase();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        sub->completeWithProgress();
    }

    timer.time(2);

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.02,
                QObject::tr(
            "Correcting installation paths created by previous versions of Npackd"));
        QString windowsDir = WPMUtils::normalizePath(WPMUtils::getWindowsDir());
        QList<InstalledPackageVersion*> ipvs = this->getAll();
        for (int i = 0; i < ipvs.count(); i++) {
            InstalledPackageVersion* ipv = ipvs.at(i);
            if (ipv->installed()) {
                QString d = WPMUtils::normalizePath(ipv->directory);
                // qDebug() << ipv->package <<ipv->directory;
                if ((WPMUtils::isUnder(d, windowsDir) ||
                        WPMUtils::pathEquals(d, windowsDir)) &&
                        ipv->package != packageName) {
                    this->setPackageVersionPath(ipv->package, ipv->version, "");
                }
            }
        }
        qDeleteAll(ipvs);
        sub->completeWithProgress();
    }

    // adding well-known packages should happen before adding packages
    // determined from the list of installed packages to get better
    // package descriptions for com.microsoft.Windows64 and similar packages
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.03,
                QObject::tr("Adding well-known packages"));
        AbstractThirdPartyPM* pm = new WellKnownProgramsThirdPartyPM(
                this->packageName);
        job->setErrorMessage(detect3rdParty(rep, pm, false));
        delete pm;

        if (job->getErrorMessage().isEmpty())
            sub->completeWithProgress();
    }

    timer.time(3);

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.02,
                QObject::tr("Setting the NPACKD_CL environment variable"));
        QString err = rep->updateNpackdCLEnvVar();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

    timer.time(4);

    // qDebug() << "InstalledPackages::refresh.2";

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05,
                QObject::tr("Detecting MSI packages"));
        // MSI package detection should happen before the detection for
        // control panel programs
        AbstractThirdPartyPM* pm = new MSIThirdPartyPM();
        job->setErrorMessage(detect3rdParty(rep, pm, true, "msi:"));
        delete pm;

        if (job->getErrorMessage().isEmpty())
            sub->completeWithProgress();
    }

     // qDebug() << "InstalledPackages::refresh.2.1";

    /*
    if (job->shouldProceed(
            QObject::tr("Detecting Component Based Servicing packages"))) {
        AbstractThirdPartyPM* pm = new CBSThirdPartyPM();
        job->setErrorMessage(detect3rdParty(pm, true, "cbs:"));
        delete pm;

        if (job->getErrorMessage().isEmpty())
            job->setProgress(0.69);
    }
    */

    timer.time(5);

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Reading the list of packages installed by Npackd"));

        // detecting from the list of installed packages should happen first
        // as all other packages consult the list of installed packages
        AbstractThirdPartyPM* pm = new InstalledPackagesThirdPartyPM();
        job->setErrorMessage(detect3rdParty(rep, pm, false));
        delete pm;

        if (job->getErrorMessage().isEmpty())
            sub->completeWithProgress();
    }

    timer.time(6);

    // qDebug() << "InstalledPackages::refresh.3";

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.02,
                QObject::tr("Detecting software control panel packages"));

        AbstractThirdPartyPM* pm = new ControlPanelThirdPartyPM();
        job->setErrorMessage(detect3rdParty(rep, pm, true, "control-panel:"));
        delete pm;

        if (job->getErrorMessage().isEmpty())
            sub->completeWithProgress();
    }

    timer.time(7);

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05,
                QObject::tr("Clearing information about installed package versions in nested directories"));
        QString err = clearPackagesInNestedDirectories();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            sub->completeWithProgress();
            job->setProgress(1);
        }
    }

    timer.time(8);

    // timer.dump();

    // this->mutex.unlock();

    job->complete();
}

QString InstalledPackages::getPath(const QString &package,
        const Version &version) const
{
    this->mutex.lock();

    QString r;
    InstalledPackageVersion* ipv = findNoCopy(package, version);
    if (ipv)
        r = ipv->getDirectory();

    this->mutex.unlock();

    return r;
}

bool InstalledPackages::isInstalled(const QString &package,
        const Version &version) const
{
    this->mutex.lock();

    InstalledPackageVersion* ipv = findNoCopy(package, version);
    bool r = ipv && ipv->installed();

    this->mutex.unlock();

    return r;
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

    for (int j = 0; j < pvs.count(); j++) {
        InstalledPackageVersion* pv = pvs.at(j);
        QString pvdir = paths.at(j);
        if (pv->installed()) {
            for (int i = j + 1; i < pvs.count(); i++) {
                InstalledPackageVersion* pv2 = pvs.at(i);
                QString pv2dir = paths.at(i);
                if (pv2->installed()) {
                    if (pv2dir.startsWith(pvdir)) {
                        err = setPackageVersionPath(pv2->package,
                                pv2->version, "");
                        if (!err.isEmpty())
                            goto out;
                    }
                }
            }
        }
    }
out:

    qDeleteAll(pvs);
    pvs.clear();

    return err;
}

QString InstalledPackages::readRegistryDatabase()
{
    // qDebug() << "start reading registry database";

    // "data" is only used at the bottom of this method

    QString err;

    WindowsRegistry packagesWR;
    LONG e;
    err = packagesWR.open(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Npackd\\Npackd\\Packages", false, KEY_READ, &e);

    QList<InstalledPackageVersion*> ipvs;
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
                ipvs.append(ipv);
            } else {
                delete ipv;
            }
        }
    }

    this->mutex.lock();
    qDeleteAll(this->data);
    this->data.clear();
    for (int i = 0; i < ipvs.count(); i++) {
        InstalledPackageVersion* ipv = ipvs.at(i);
        this->data.insert(PackageVersion::getStringId(ipv->package,
                ipv->version), ipv->clone());
    }
    this->mutex.unlock();

    for (int i = 0; i < ipvs.count(); i++) {
        InstalledPackageVersion* ipv = ipvs.at(i);
        fireStatusChanged(ipv->package, ipv->version);
    }
    qDeleteAll(ipvs);

    // qDebug() << "stop reading";

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

