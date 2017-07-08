#include "installedpackages.h"

#include <windows.h>
#include <msi.h>
#include <memory>
#include <shlobj.h>

#include <QDebug>
#include <QtConcurrent/QtConcurrent>

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
//#include "cbsthirdpartypm.h"

InstalledPackages InstalledPackages::def;

QString InstalledPackages::packageName;

InstalledPackages* InstalledPackages::getDefault()
{
    return &def;
}

InstalledPackages::InstalledPackages() : mutex(QMutex::Recursive)
{
}

InstalledPackages::InstalledPackages(const InstalledPackages &other) :
        QObject(), mutex(QMutex::Recursive)
{
    *this = other;
}

InstalledPackages &InstalledPackages::operator=(const InstalledPackages &other)
{
    this->mutex.lock();
    qDeleteAll(this->data);
    this->data.clear();
    QList<InstalledPackageVersion*> ipvs = other.getAll();
    for (int i = 0; i < ipvs.size(); i++) {
        InstalledPackageVersion* ipv = ipvs.at(i);
        this->data.insert(ipv->package + "/" +
                ipv->version.getVersionString(), ipv);
    }
    this->mutex.unlock();
    
    return *this;
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

void InstalledPackages::detect3rdParty(Job* job, DBRepository* r,
        Repository* rep,
        const QList<InstalledPackageVersion*>& installed,
        bool replace, const QString& detectionInfoPrefix)
{
    // this method does not manipulate "date" directly => no locking

    HRTimer timer(5);
    timer.time(0);

    timer.time(1);

    // qDebug() << "detect3rdParty 3";

    // remove packages and versions that are not installed
    // we assume that one 3rd party package manager does not create package
    // or package version objects for another
    if (job->shouldProceed()) {
        QSet<QString> packages;
        for (int i = 0; i < installed.size();i++) {
            InstalledPackageVersion* ipv = installed.at(i);
            packages.insert(ipv->package);
        }

        for (int i = 0; i < rep->packages.size(); ) {
            Package* p = rep->packages.at(i);
            if (!packages.contains(p->name)) {
                rep->packages.removeAt(i);
                rep->package2versions.remove(p->name);
                delete p;
            } else
                i++;
        }

        for (int i = 0; i < rep->packageVersions.size(); ) {
            PackageVersion* pv = rep->packageVersions.at(i);
            if (!packages.contains(pv->package)) {
                rep->packageVersions.removeAt(i);
                delete pv;
            } else
                i++;
        }
    }

    // save all detected packages and versions
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.2, QObject::tr("Saving"), true, true);
        r->saveAll(sub, rep, replace);
    }

    timer.time(2);

    // remove all package versions that are not detected as installed anymore
    if (job->shouldProceed() && !detectionInfoPrefix.isEmpty()) {
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

    // filter the package versions out that are either already installed or
    // point to directories under an already installed version
    if (job->shouldProceed()) {
        QStringList programRoots;
        programRoots.append(WPMUtils::getProgramFilesDir());
        if (WPMUtils::is64BitWindows())
            programRoots.append(WPMUtils::getShellDir(CSIDL_PROGRAM_FILESX86));

        for (int i = 0; i < installed.count(); i++) {
            InstalledPackageVersion* ipv = installed.at(i);

            // qDebug() << ipv->package << ipv->version.getVersionString();

            QString betterPackageName = findBetterPackageName(r, ipv);

            // if the package version is already installed, we skip it
            InstalledPackageVersion* existing = find(ipv->package,
                    ipv->version);
            if (existing && existing->installed()) {
                // remove an existing detected and installed package version
                // if a better package name was found
                if (!existing->detectionInfo.isEmpty() &&
                        !betterPackageName.isEmpty()) {
                    // WPMUtils::moveToRecycleBin(existing->directory);
                    setPackageVersionPath(ipv->package, ipv->version, "");
                }

                // qDebug() << "existing: " << existing->toString();
                delete existing;
                continue;
            }

            delete existing;

            if (!betterPackageName.isEmpty()) {
                ipv->package = betterPackageName;
            }

            // qDebug() << "    0.1";

            // we cannot handle nested directories
            QString path = ipv->directory;
            if (!path.isEmpty()) {
                path = WPMUtils::normalizePath(path);

                bool cont = false;
                for (int i = 0; i < packagePaths.size(); i++) {
                    QString ppi = packagePaths.at(i);

                    // e.g. an MSI package and a package from the Control Panel
                    // "Software" have the same path
                    if (WPMUtils::pathEquals(path, ppi)) {
                        cont = true;
                        break;
                    }

                    if (WPMUtils::isUnder(path, ppi) ||
                            WPMUtils::isUnder(ppi, path) ||
                            WPMUtils::isOverOrEquals(path, programRoots)) {
                        ipv->directory = "";
                        break;
                    }
                }

                if (cont)
                    continue;
            }

            processOneInstalled3rdParty(r, ipv);
        }
    }

    // qDebug() << "detect3rdParty 5";

    timer.time(3);

    timer.time(4);

    // qDebug() << detectionInfoPrefix;

    // timer.dump();

    job->complete();
}

QString InstalledPackages::findBetterPackageName(DBRepository *r,
        InstalledPackageVersion* ipv)
{
    QString result;

    if (ipv->package.startsWith("msi.") ||
            ipv->package.startsWith("control-panel.")) {
        // find another package with the same title
        Package* p = r->findPackage_(ipv->package);
        if (p) {
            QString txt = p->title.toLower();
            txt.replace('(', ' ');
            txt.replace(')', ' ');
            txt.replace('[', ' ');
            txt.replace(']', ' ');
            txt.replace('{', ' ');
            txt.replace('}', ' ');
            txt.replace('-', ' ');
            txt.replace('/', ' ');
            QStringList parts = txt.split(' ', QString::SkipEmptyParts);
            QStringList stopWords = QString("version build edition x86 remove only").
                    split(' ');
            const QString versionString = ipv->version.getVersionString();
            for (int i = 0; i < parts.size(); i++) {
                const QString p = parts.at(i);
                if (p == "x64") {
                    parts[i] = "64";
                } else if (p == versionString || stopWords.contains(p)) {
                    parts[i] = "";
                }
            }

            // qDebug() << "searching for" << parts.join(' ');

            QString err;
            QStringList found = r->findPackages(
                    Package::NOT_INSTALLED,
                    Package::NOT_INSTALLED_NOT_AVAILABLE,
                    parts.join(' '), -1, -1, &err);
            if (err.isEmpty() && found.size() > 0 && found.size() < 4) {
                QList<Package*> replacements = r->findPackages(found);

                Package* replacement = 0;
                for (int i = 0; i < replacements.size(); i++) {
                    Package* p2 = replacements.at(i);
                    if (!p2->name.startsWith("msi.") &&
                            !p2->name.startsWith("control-panel.")) {
                        replacement = p2->clone();
                        break;
                    }
                }
                qDeleteAll(replacements);

                if (replacement) {
                    // qDebug() << "replacing" << ipv->package << replacement->name;

                    result = replacement->name;

                    delete replacement;
                }
            }

            delete p;
        }
    }

    return result;
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

        path = WPMUtils::normalizePath(WPMUtils::getProgramFilesDir(),
                false) +
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
        const QString& directory, bool updateRegistry)
{
    this->mutex.lock();

    QString err;

    InstalledPackageVersion* ipv = this->findNoCopy(package, version);
    if (!ipv) {
        ipv = new InstalledPackageVersion(package, version, directory);
        this->data.insert(package + "/" + version.getVersionString(), ipv);
        if (updateRegistry)
            err = saveToRegistry(ipv);
    } else {
        ipv->setPath(directory);
        if (updateRegistry)
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

bool InstalledPackages::isInstalled(const Dependency& dep) const
{
    QList<InstalledPackageVersion*> installed = getAll();
    bool res = false;
    for (int i = 0; i < installed.count(); i++) {
        InstalledPackageVersion* ipv = installed.at(i);
        if (ipv->package == dep.package &&
                dep.test(ipv->version)) {
            res = true;
            break;
        }
    }
    qDeleteAll(installed);
    return res;
}

QSet<QString> InstalledPackages::getPackages() const
{
    this->mutex.lock();

    QList<InstalledPackageVersion*> all = this->data.values();
    QSet<QString> r;
    for (int i = 0; i < all.count(); i++) {
        InstalledPackageVersion* ipv = all.at(i);
        if (ipv->installed())
            r.insert(ipv->package);
    }

    this->mutex.unlock();

    return r;
}

InstalledPackageVersion*
        InstalledPackages::findFirstWithMissingDependency() const
{
    InstalledPackageVersion* r = 0;

    this->mutex.lock();

    DBRepository* dbr = DBRepository::getDefault();
    QList<InstalledPackageVersion*> all = this->data.values();
    for (int i = 0; i < all.count(); i++) {
        InstalledPackageVersion* ipv = all.at(i);
        if (ipv->installed()) {
            QString err;
            QScopedPointer<PackageVersion> pv(dbr->findPackageVersion_(
                    ipv->package, ipv->version, &err));

            //if (!pv.data()) {
            //    qDebug() << "cannot find" << ipv->package << ipv->version.getVersionString();
            //}

            if (err.isEmpty() && pv.data()) {
                for (int j = 0; j < pv->dependencies.size(); j++) {
                    if (!isInstalled(*pv->dependencies.at(j))) {
                        r = ipv->clone();
                        break;
                    }
                }
            }
        }

        if (r)
            break;
    }

    this->mutex.unlock();

    return r;
}

QString InstalledPackages::notifyInstalled(const QString &package,
        const Version &version, bool success) const
{
    QString err;

    QStringList paths = getAllInstalledPackagePaths();

    QStringList env;
    env.append("NPACKD_PACKAGE_NAME");
    env.append(package);
    env.append("NPACKD_PACKAGE_VERSION");
    env.append(version.getVersionString());
    env.append("NPACKD_CL");
    env.append(DBRepository::getDefault()->
            computeNpackdCLEnvVar_(&err));
    env.append("NPACKD_SUCCESS");
    env.append(success ? "1" : "0");
    err = ""; // ignore the error

    // searching for a file in all installed package versions may take up to 5
    // seconds if the data is not in the cache and only 20 milliseconds
    // otherwise
    for (int i = 0; i < paths.size(); i++) {
        QString path = paths.at(i);
        QString file = path + "\\.Npackd\\InstallHook.bat";
        QFileInfo fi(file);
        if (fi.exists()) {
            // qDebug() << file;
            Job* job = new Job("Notification");
            WPMUtils::executeBatchFile(
                    job, path, ".Npackd\\InstallHook.bat",
                    ".Npackd\\InstallHook.log", env, true);

            // ignore the possible errors

            delete job;
        }
    }

    return "";
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

void InstalledPackages::refresh(DBRepository *rep, Job *job, bool detectMSI)
{
    rep->currentRepository = 10000;

    // no direct usage of "data" here => no mutex

    if (job->shouldProceed()) {
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

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.6,
                QObject::tr("Reading registry package database"));
        QString err = readRegistryDatabase();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        sub->completeWithProgress();
    }

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
                        ipv->package != InstalledPackages::packageName) {
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

    // detecting from the list of installed packages should happen first
    // as all other packages consult the list of installed packages. Secondly,
    // MSI or the programs from the control panel may be installed in strange
    // locations like "C:\" which "uninstalls" all packages installed by Npackd

    // MSI package detection should happen before the detection for
    // control panel programs
    if (job->shouldProceed()) {
        QList<AbstractThirdPartyPM*> tpms;
        tpms.append(new WellKnownProgramsThirdPartyPM(
                InstalledPackages::packageName));
        tpms.append(new InstalledPackagesThirdPartyPM());
        tpms.append(new MSIThirdPartyPM()); // true, msi:
        tpms.append(new ControlPanelThirdPartyPM()); // true, control-panel:

        QStringList jobTitles;
        jobTitles.append(QObject::tr("Adding well-known packages"));
        jobTitles.append(QObject::tr("Reading the list of packages installed by Npackd"));
        jobTitles.append(QObject::tr("Detecting MSI packages"));
        jobTitles.append(QObject::tr("Detecting software control panel packages"));

        QStringList prefixes;
        prefixes.append("");
        prefixes.append("");
        prefixes.append("msi:");
        prefixes.append("control-panel:");

        QList<Repository*> repositories;
        QList<QList<InstalledPackageVersion*>* > installeds;
        for (int i = 0; i < tpms.count(); i++) {
            repositories.append(new Repository());
            installeds.append(new QList<InstalledPackageVersion*>());
        }

        QList<QFuture<void> > futures;
        for (int i = 0; i < tpms.count(); i++) {
            AbstractThirdPartyPM* tpm = tpms.at(i);
            Job* s = job->newSubJob(0.1,
                    jobTitles.at(i), false, true);

            QFuture<void> future = QtConcurrent::run(
                    tpm,
                    &AbstractThirdPartyPM::scan, s,
                    installeds.at(i), repositories.at(i));
            futures.append(future);
        }

        for (int i = 0; i < futures.count(); i++) {
            futures[i].waitForFinished();

            job->setProgress(0.82 + (i + 1.0) / futures.count() * 0.05);
        }


        for (int i = 0; i < futures.count(); i++) {
            Job* sub = job->newSubJob(0.1,
                    QObject::tr("Detecting %1").arg(i), false, true);
            detect3rdParty(sub, rep, repositories.at(i),
                    *installeds.at(i),
                    i == 2 || i == 3,
                    prefixes.at(i));
            qDeleteAll(*installeds.at(i));

            job->setProgress(0.87 + (i + 1.0) / futures.count() * 0.05);
        }

        qDeleteAll(repositories);
        qDeleteAll(installeds);
        qDeleteAll(tpms);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.08,
                QObject::tr("Setting the NPACKD_CL environment variable"));
        QString err = rep->updateNpackdCLEnvVar();
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            sub->completeWithProgress();
    }

/*
 * use DISM API instead
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Detecting Component Based Servicing packages"),
                true, true);

        AbstractThirdPartyPM* pm = new CBSThirdPartyPM();
        detect3rdParty(sub, rep, pm, true, "cbs:");
        delete pm;
    }
 */

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

            if (dir.isEmpty()) {
                packagesWR.remove(name);
            } else {
                dir = WPMUtils::normalizePath(dir, false);

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
            ipv->detectionInfo + "\r\n");*/

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

