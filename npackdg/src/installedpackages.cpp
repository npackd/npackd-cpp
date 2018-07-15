#include "installedpackages.h"

#include <windows.h>
#include <msi.h>
#include <memory>
#include <shlobj.h>

#include <QtGlobal>
#if defined(_MSC_VER) && (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#include <QtConcurrent/QtConcurrent>
#include <QLoggingCategory>

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
    qCDebug(npackd) << "Setting installed packages";
    this->dump();

    other.dump();

    QList<InstalledPackageVersion*> myInfos = getAll();
    for (int i = 0; i < myInfos.size(); i++) {
        InstalledPackageVersion* myIpv = myInfos.at(i);
        InstalledPackageVersion* otherIpv = other.find(
                myIpv->package, myIpv->version);

        if (!otherIpv) {
            setPackageVersionPath(myIpv->package,
                    myIpv->version, "", false);
        } else if (*myIpv != *otherIpv) {
            setOne(*otherIpv);
        }

        delete otherIpv;
    }
    qDeleteAll(myInfos);
    myInfos.clear();

    QList<InstalledPackageVersion*> otherInfos = other.getAll();
    for (int i = 0; i < otherInfos.size(); i++) {
        InstalledPackageVersion* otherIpv = otherInfos.at(i);
        InstalledPackageVersion* myIpv = find(
                otherIpv->package, otherIpv->version);

        if (!myIpv) {
            setOne(*otherIpv);
        }

        delete myIpv;
    }
    qDeleteAll(otherInfos);
    otherInfos.clear();

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
        const QList<InstalledPackageVersion*>& installed,
        const QString& detectionInfoPrefix)
{
    // this method does not manipulate "data" directly => no locking

    if (job->shouldProceed()) {
        for (int i = 0; i < installed.count(); i++) {
            InstalledPackageVersion* ipv = installed.at(i);

            processOneInstalled3rdParty(r, ipv, detectionInfoPrefix);

            job->setProgress((i + 1.0) / installed.size());
        }
    }

    job->complete();
}

void InstalledPackages::addPackages(Job* job, DBRepository* r,
        Repository* rep,
        const QList<InstalledPackageVersion*>& installed,
        bool replace, const QString& detectionInfoPrefix)
{
    // this method does not manipulate "data" directly => no locking

    // qCDebug(npackd) << "detect3rdParty 3";

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
        r->saveAll(job, rep, replace);
    }

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
            QString err;

            qCDebug(npackd) << "trying to replace" << p->name << p->title;

            QStringList found = r->findBetterPackages(p->title, &err);

            if (err.isEmpty() && found.size() == 1) {
                QList<Package*> replacements = r->findPackages(found);
                result = replacements.at(0)->name;
                qDeleteAll(replacements);
            }

            delete p;
        }
    }

    return result;
}

void InstalledPackages::processOneInstalled3rdParty(DBRepository *r,
        InstalledPackageVersion* ipv, const QString& detectionInfoPrefix)
{
    QDir qd;

    QString d = ipv->directory;

    if (ipv->package.startsWith("msi.") ||
            ipv->package.startsWith("control-panel.")) {
        InstalledPackageVersion* orig = InstalledPackages::getDefault()->
                find(ipv->package, ipv->version);
        if (orig) {
            d = orig->getDirectory();
            delete orig;
        }
    }

    //qCDebug(npackd) << "0" << ipv->package << ipv->version.getVersionString() <<
    //        ipv->directory << ipv->detectionInfo << detectionInfoPrefix;

    if (!d.isEmpty()) {
        if (!qd.exists(d))
            d = "";
    }

    QString windowsDir = WPMUtils::getWindowsDir();

    // ancestor of the Windows directory
    if (!d.isEmpty() && WPMUtils::isUnder(windowsDir, d)) {
        d = "";
    }

    // child of the Windows directory
    if (!d.isEmpty() && WPMUtils::isUnder(d,
            windowsDir)) {
        d = "";
    }

    // Windows directory
    if (!d.isEmpty() && WPMUtils::pathEquals(d,
            windowsDir)) {
        if (ipv->package != "com.microsoft.Windows" &&
                ipv->package != "com.microsoft.Windows32" &&
                ipv->package != "com.microsoft.Windows64") {
            d = "";
        }
    }

    QString programFilesDir = WPMUtils::getProgramFilesDir();

    // ancestor of "C:\Program Files"
    if (!d.isEmpty() && (WPMUtils::isUnder(programFilesDir, d) ||
            WPMUtils::pathEquals(d, programFilesDir))) {
        d = "";
    }

    QString programFilesX86Dir = WPMUtils::getShellDir(CSIDL_PROGRAM_FILESX86);

    // ancestor of "C:\Program Files (x86)"
    if (!d.isEmpty() && WPMUtils::is64BitWindows() &&
            (WPMUtils::isUnder(programFilesX86Dir, d) ||
            WPMUtils::pathEquals(d,
            programFilesX86Dir))) {
        d = "";
    }

    // qCDebug(npackd) << "    0.1";

    // we cannot handle nested directories
    if (!d.isEmpty()) {
        QStringList packagePaths = this->getAllInstalledPackagePaths();
        for (int i = 0; i < packagePaths.size(); i++) {
            packagePaths[i] = packagePaths.at(i);
        }

        bool ignore = false;
        for (int i = 0; i < packagePaths.size(); i++) {
            // e.g. an MSI package and a package from the Control Panel
            // "Software" have the same path
            if (WPMUtils::isUnderOrEquals(d, packagePaths.at(i))) {
                ignore = true;
                break;
            }

            if (WPMUtils::isUnderOrEquals(packagePaths.at(i), d)) {
                d = "";
                break;
            }
        }

        if (ignore)
            return;

        //qCDebug(npackd) << "not ignoring" << d;
    }

    QString betterPackageName = findBetterPackageName(r, ipv);
    if (!betterPackageName.isEmpty()) {
        ipv->package = betterPackageName;
    }

    // if the package version is already installed, we skip it
    InstalledPackageVersion* existing = find(ipv->package,
            ipv->version);
    if (existing && existing->installed()) {
        delete existing;
        return;
    }
    delete existing;

    QString err;

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
        // qCDebug(npackd) << "    1";

        u = pv->findFile(".Npackd\\Uninstall.bat");
    }

    // special case: we don't know where the package is installed and we
    // don't know how to remove it
    if (err.isEmpty() && d.isEmpty() && u == 0 && pv) {
        u = new PackageVersionFile(".Npackd\\Uninstall.bat",
                "echo no removal procedure for this package is available"
                "\r\n"
                "exit 1"  "\r\n");
        pv->files.append(u);
    }

    if (err.isEmpty() && d.isEmpty()) {
        // qCDebug(npackd) << "    2";

        Package* p = r->findPackage_(ipv->package);

        d = WPMUtils::normalizePath(
                WPMUtils::getProgramFilesDir(),
                false) +
                "\\NpackdDetected\\" +
                WPMUtils::makeValidFilename(p ? p->title : ipv->package, '_');
        if (qd.exists(d)) {
            d = WPMUtils::findNonExistingFile(
                    d + "-" +
                    ipv->version.getVersionString(), "");
        }
        qd.mkpath(d);
        delete p;
    }

    if (err.isEmpty() && qd.exists(d)) {
        err = pv->saveFiles(QDir(d));
    }

    InstalledPackageVersion* ipv2 = 0;
    if (err.isEmpty()) {
        // qCDebug(npackd) << "    4";
        ipv2 = this->findOrCreate(ipv->package, ipv->version, &err);
    }

    if (err.isEmpty()) {
        // qCDebug(npackd) << "    5";
        ipv2->detectionInfo = ipv->detectionInfo;
        ipv2->setPath(d);

        //qCDebug(npackd) << ipv2->package << ipv2->version.getVersionString() <<
        //        ipv2->directory << ipv2->detectionInfo;
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
    bool changed = false;

    this->mutex.lock();

    QString err;

    InstalledPackageVersion* ipv = this->findNoCopy(package, version);
    if (!ipv) {
        ipv = new InstalledPackageVersion(package, version, directory);
        this->data.insert(package + "/" + version.getVersionString(), ipv);
        changed = true;
    } else {
        if (ipv->getDirectory() != directory) {
            ipv->setPath(directory);
            changed = true;
        }
    }
    if (updateRegistry)
        err = saveToRegistry(ipv);

    this->mutex.unlock();

    if (changed)
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
            //    qCDebug(npackd) << "cannot find" << ipv->package << ipv->version.getVersionString();
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
            // qCDebug(npackd) << file;
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

void InstalledPackages::refresh(DBRepository *rep, Job *job)
{
    rep->currentRepository = 10000;

    // no direct usage of "data" here => no mutex

    if (job->shouldProceed()) {
        clear();
        job->setProgress(0.2);
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
        QList<bool> replace;
        QStringList jobTitles;
        QStringList prefixes;

        jobTitles.append(QObject::tr("Reading the list of packages installed by Npackd"));
        tpms.append(new InstalledPackagesThirdPartyPM());
        replace.append(false);
        prefixes.append("");

        if (WPMUtils::adminMode)
		{
			jobTitles.append(QObject::tr("Adding well-known packages"));
			tpms.append(new WellKnownProgramsThirdPartyPM(
				InstalledPackages::packageName));
            replace.append(false);
            prefixes.append("");

            jobTitles.append(QObject::tr("Detecting MSI packages"));
			tpms.append(new MSIThirdPartyPM());
            replace.append(true);
            prefixes.append("msi:");

			jobTitles.append(QObject::tr("Detecting software control panel packages"));
			tpms.append(new ControlPanelThirdPartyPM());
            replace.append(true);
            prefixes.append("control-panel:");
		}

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

            Job* sub = job->newSubJob(0.1,
                    QObject::tr("Saving detected packages %1").arg(i),
                    false, true);
            addPackages(sub, rep, repositories.at(i),
                    *installeds.at(i),
                    replace.at(i),
                    prefixes.at(i));

            job->setProgress(0.2 + (i + 1.0) / futures.count() * 0.4);
        }

        for (int i = 0; i < futures.count(); i++) {
            Job* sub = job->newSubJob(0.1,
                    QObject::tr("Adding detected versions %1").arg(i),
                    false, true);
            detect3rdParty(sub, rep,
                    *installeds.at(i),
                    prefixes.at(i));
            qDeleteAll(*installeds.at(i));

            job->setProgress(0.6 + (i + 1.0) / futures.count() * 0.2);
        }

        qDeleteAll(repositories);
        qDeleteAll(installeds);
        qDeleteAll(tpms);
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.2,
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

/* WUA
#include "stdafx.h"
#include <wuapi.h>
#include <iostream>
#include <ATLComTime.h>
#include <wuerror.h>

using namespace std;


int _tmain(int argc, _TCHAR* argv[])
{
    HRESULT hr;
    hr = CoInitialize(NULL);

    IUpdateSession* iUpdate;
    IUpdateSearcher* searcher;
    ISearchResult* results;
    BSTR criteria = SysAllocString(L"IsInstalled=1 or IsHidden=1 or IsPresent=1");

    hr = CoCreateInstance(CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER, IID_IUpdateSession, (LPVOID*)&iUpdate);
    hr = iUpdate->CreateUpdateSearcher(&searcher);

    wcout << L"Searching for updates ..."<<endl;
    hr = searcher->Search(criteria, &results);
    SysFreeString(criteria);

    switch(hr)
    {
    case S_OK:
        wcout<<L"List of applicable items on the machine:"<<endl;
        break;
    case WU_E_LEGACYSERVER:
        wcout<<L"No server selection enabled"<<endl;
        return 0;
    case WU_E_INVALID_CRITERIA:
        wcout<<L"Invalid search criteria"<<endl;
        return 0;
    }

    IUpdateCollection *updateList;
    IUpdate *updateItem;
    LONG updateSize;
    BSTR updateName;
    DATE retdate;

    results->get_Updates(&updateList);
    updateList->get_Count(&updateSize);

    if (updateSize == 0)
    {
        wcout << L"No updates found"<<endl;
    }

    for (LONG i = 0; i < updateSize; i++)
    {
        updateList->get_Item(i,&updateItem);
        updateItem->get_Title(&updateName);
        updateItem->get_LastDeploymentChangeTime(&retdate);
        COleDateTime odt;
        odt.m_dt=retdate;
        wcout<<i+1<<" - "<<updateName<<"  Release Date "<< (LPCTSTR)odt.Format(_T("%A, %B %d, %Y"))<<endl;
    }
    ::CoUninitialize();
    wcin.get();
    return 0;
} */
    job->complete();
}

void InstalledPackages::dump() const
{
    qCDebug(npackd) << "Installed packages:";
    QList<InstalledPackageVersion*> myInfos = getAll();
    for (int i = 0; i < myInfos.size(); i++) {
        InstalledPackageVersion* myIpv = myInfos.at(i);
        qCDebug(npackd) << myIpv->package << myIpv->version.getVersionString() <<
                myIpv->directory;
    }
    qDeleteAll(myInfos);
}

QString InstalledPackages::save()
{
    qCDebug(npackd) << "Saving installed packages";
    this->dump();

    InstalledPackages other;

    QString err = other.readRegistryDatabase();

    if (err.isEmpty()) {
        other.dump();

        QList<InstalledPackageVersion*> myInfos = getAll();
        for (int i = 0; i < myInfos.size(); i++) {
            InstalledPackageVersion* myIpv = myInfos.at(i);
            InstalledPackageVersion* otherIpv = other.find(
                    myIpv->package, myIpv->version);

            if (!otherIpv || !(*myIpv == *otherIpv)) {
                err = saveToRegistry(myIpv);
            }

            delete otherIpv;

            if (!err.isEmpty())
                break;
        }
        qDeleteAll(myInfos);
        myInfos.clear();

        QList<InstalledPackageVersion*> otherInfos = other.getAll();
        for (int i = 0; i < otherInfos.size(); i++) {
            InstalledPackageVersion* otherIpv = otherInfos.at(i);
            InstalledPackageVersion* myIpv = find(
                    otherIpv->package, otherIpv->version);

            if (!myIpv) {
                err = setPackageVersionPath(otherIpv->package,
                        otherIpv->version, "", true);
            }

            delete myIpv;

            if (!err.isEmpty())
                break;
        }
        qDeleteAll(otherInfos);
        otherInfos.clear();
    }

    return err;
}

QString InstalledPackages::setOne(const InstalledPackageVersion& other)
{
    QString err;
    bool changed = false;

    this->mutex.lock();
    InstalledPackageVersion* ipv =
            this->findOrCreate(other.package, other.version, &err);
    if (*ipv != other) {
        *ipv = other;
        changed = true;
    }
    this->mutex.unlock();

    if (changed)
        fireStatusChanged(other.package, other.version);

    return err;
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
    // qCDebug(npackd) << "start reading registry database";

    // "data" is only used at the bottom of this method

    QString err;

    WindowsRegistry packagesWR;
    LONG e;
    err = packagesWR.open(
        WPMUtils::adminMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
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
                    qCDebug(npackd) << "adding " << ipv->package <<
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

    // qCDebug(npackd) << "stop reading";

    return err;
}

void InstalledPackages::clear()
{
    this->mutex.lock();
    qDeleteAll(this->data);
    this->data.clear();
    this->mutex.unlock();
}

QString InstalledPackages::findPath_npackdcl(const Dependency& dep)
{
    QString ret;

    QString err;
    WindowsRegistry packagesWR;
    LONG e;
    err = packagesWR.open(
        WPMUtils::adminMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
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
    WindowsRegistry machineWR(
            WPMUtils::adminMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
			false);
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
        // qCDebug(npackd) << "saveToRegistry 1 " << r;
    } else {
        // qCDebug(npackd) << "deleting " << pn;
        WindowsRegistry packages;
        r = packages.open(machineWR, keyName, KEY_ALL_ACCESS);
        if (r.isEmpty()) {
            r = packages.remove(pn);
        }
        // qCDebug(npackd) << "saveToRegistry 2 " << r;
    }
    //qCDebug(npackd) << "InstalledPackageVersion::save " << pn << " " <<
    //        this->directory;

    // qCDebug(npackd) << "saveToRegistry returns " << r;

    return r;
}

