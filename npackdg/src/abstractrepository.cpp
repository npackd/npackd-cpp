#include <future>

#include "QLoggingCategory"
#include "QTemporaryDir"
#include "QDirIterator"

#include "JlCompress.h"

#include "abstractrepository.h"
#include "wpmutils.h"
#include "windowsregistry.h"
#include "installedpackages.h"
#include "downloader.h"
#include "packageutils.h"
#include "threadpool.h"
#include "deptask.h"

std::mutex AbstractRepository::installationScripts;
ThreadPool AbstractRepository::threadPool(10, THREAD_PRIORITY_LOWEST);

bool AbstractRepository::includesRemoveItself(
        const std::vector<InstallOperation *> &install_)
{
    bool res = false;

    QString exeDir = WPMUtils::getExeDir();
    for (auto op: install_) {
        if (!op->install) {
            QString err;
            PackageVersion* pv = this->findPackageVersion_(
                    op->package, op->version, &err);
            if (err.isEmpty() && pv) {
                QString path = pv->getPath();
                delete pv;

                if (WPMUtils::pathEquals(exeDir, path) ||
                        WPMUtils::isUnder(exeDir, path)) {
                    res = true;
                    break;
                }
            }
        }
    }

    return res;
}

void AbstractRepository::exportPackagesCoInitializeAndFree(Job *job,
        const std::vector<PackageVersion *> &pvs, const QString& where,
        int def)
{
    QDir d;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);

    CoInitialize(nullptr);

    if (job->shouldProceed()) {
        if (!d.mkpath(where)) {
            job->setErrorMessage(QObject::tr("Cannot create directory: %0").
                    arg(where));
        } else {
            job->setProgress(0.01);
        }
    }

    if (def == 3) {
        for (auto pv: pvs) {
            if (!job->shouldProceed())
                break;

            Job* djob = job->newSubJob(0.9 / pvs.size(),
                    QObject::tr("Downloading & computing hash sum for %1").
                    arg(pv->getPackageTitle()), true, true);

            QString fn = pv->download.path();
            std::vector<QString> parts = WPMUtils::split(fn, '/');
            fn = where + "\\";

            QFileInfo fi(parts.at(parts.size() - 1));
            fn = fn + fi.baseName();
            QString suffix = fi.completeSuffix();
            if (!suffix.isEmpty())
                suffix.prepend('.');

            fn = WPMUtils::findNonExistingFile(fn, suffix);

            pv->downloadTo(*djob, fn, true);

            pv->download.setUrl(QFileInfo(fn).fileName());
        }
    }

    if (job->shouldProceed()) {
        Repository* rep = new Repository();

        if (def == 0 || def == 2 || def == 3) {
            std::unique_ptr<Package> super(new Package(
                    WPMUtils::getHostName() + ".super",
                    QObject::tr("List of packages")));
            rep->savePackage(super.get(), true);

            std::unique_ptr<PackageVersion> superv(
                    new PackageVersion(super.get()->name));
            if (!superv->version.setVersion(QDateTime::currentDateTime().toString(
                    "yyyy.M.d.h.m.s"))) {
                job->setErrorMessage("Internal error: unexpected version syntax");
            }
            superv->type = PackageVersion::Type::ONE_FILE;
            superv->download.setUrl("Rep.xml");
            for (auto pv: pvs) {
                Dependency* dep = new Dependency();
                dep->package = pv->package;
                dep->setVersions("[0,2000000000)");
                superv.get()->dependencies.push_back(dep);
            }
            rep->savePackageVersion(superv.get(), true);
        }

        if (def == 1 || def == 2 || def == 3) {
            for (auto pv: pvs) {
                if (!job->shouldProceed())
                    break;

                QString err = rep->savePackageVersion(pv, true);
                if (!err.isEmpty()) {
                    job->setErrorMessage(err);
                    break;
                }

                std::unique_ptr<Package> p(findPackage_(pv->package));
                if (p) {
                    err = rep->savePackage(p.get(), false);
                    if (!err.isEmpty()) {
                        job->setErrorMessage(err);
                        break;
                    }
                    if (!p->license.isEmpty()) {
                        std::unique_ptr<License> lic(findLicense_(p->license, &err));
                        if (lic) {
                            err = rep->saveLicense(lic.get(), false);
                            if (!err.isEmpty()) {
                                job->setErrorMessage(err);
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (job->shouldProceed()) {
            QString xml = where + "\\Rep.xml";
            QFile f(xml);
            if (f.open(QFile::WriteOnly)) {
                QXmlStreamWriter w(&f);
                w.setAutoFormatting(true);
                rep->toXML(w);
                if (f.error() != QFile::NoError)
                    job->setErrorMessage(f.errorString());
            } else {
                job->setErrorMessage(QObject::tr("Cannot open %1 for writing").
                        arg(xml));
            }
        }

        delete rep;
    }

    job->complete();

    CoUninitialize();

    qDeleteAll(pvs);
}

void AbstractRepository::exportPackageSettingsCoInitializeAndFree(
    Job *job, const std::vector<Package *> &packages, const QString &filename)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);

    CoInitialize(nullptr);

    QTemporaryDir dir;

    if (!dir.isValid()) {
        job->setErrorMessage(QObject::tr("Cannot create a temporary directory"));
    }

    // TODO: what about locked package versions or running installation scripts?
    if (job->shouldProceed()) {
        QDir qd;
        for (Package* p: packages) {
            if (!job->shouldProceed())
                break;

            InstalledPackageVersion* ipv = InstalledPackages::getDefault()->
                    getNewestInstalled(p->name);

            if (ipv && ipv->installed()) {
                // TODO: Package title instead of name
                Job* sub = job->newSubJob(1 / (packages.size() + 1),
                    QObject::tr("Exporting settings for %1").arg(p->name),
                    true, true);

                QFileInfo fi(ipv->getDirectory() + "\\.Npackd\\ExportUserSettings.bat");
                if (fi.exists()) {
                    QString d = dir.path() + "\\" + p->name;
                    if (!qd.mkdir(d)) {
                        job->setErrorMessage(QObject::tr("Cannot create the directory: %1").arg(d));
                        break;
                    }

                    WPMUtils::executeBatchFile(sub, d, fi.absoluteFilePath(), "",
                                                std::vector<QString>(), false, true);
                } else {
                    sub->setProgress(1);
                    sub->complete();
                }
            }
        }
    }

    if (job->shouldProceed()) {
        if (!JlCompress::compressDir(filename, dir.path(), true))
            job->setErrorMessage(QObject::tr(
                "Cannot zip the directory \"%1\" to \"%2\"").arg(dir.path(), filename));
    }

    if (job->shouldProceed())
        job->setProgress(1);

    job->complete();

    CoUninitialize();

    qDeleteAll(packages);
}

void AbstractRepository::importPackageSettingsCoInitializeAndFree(
    Job *job, const QString &filename)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);

    CoInitialize(nullptr);

    QTemporaryDir dir;

    if (!dir.isValid()) {
        job->setErrorMessage(QObject::tr("Cannot create a temporary directory"));
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.1, QObject::tr("Unzip settings file"), true, true);
        WPMUtils::unzip(sub, filename, dir.path());
    }

    if (job->shouldProceed()) {
        QDir qd(dir.path());
        QStringList entries = qd.entryList(QDir::Dirs | QDir::PermissionMask | QDir::AccessMask |
                QDir::CaseSensitive | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        for (const QString& packageName: entries) {
            Package* p = findPackage_(packageName);
            if (p) {
                InstalledPackageVersion* ipv = InstalledPackages::getDefault()->
                        getNewestInstalled(p->name);

                if (ipv && ipv->installed()) {
                    // TODO: Package title instead of name
                    Job* sub = job->newSubJob(0.9 / (entries.size() + 1),
                        QObject::tr("Importing settings for %1").arg(p->name),
                        true, true);

                    QFileInfo fi(ipv->getDirectory() + "\\.Npackd\\ImportUserSettings.bat");
                    if (fi.exists()) {
                        WPMUtils::executeBatchFile(sub, dir.path() + "\\" + packageName,
                            fi.absoluteFilePath(), "",
                            std::vector<QString>(), false, true);
                    } else {
                        sub->setProgress(1);
                        sub->complete();
                    }
                }
            }
        }
    }

    job->complete();

    CoUninitialize();
}

std::vector<PackageVersion *> AbstractRepository::findAllMatchesToInstall(
        const Dependency &dep, const std::vector<PackageVersion *> &avoid,
        QString *err)
{
    std::vector<PackageVersion*> res;

    std::vector<PackageVersion*> pvs = getPackageVersions_(dep.package, err);
    if (err->isEmpty()) {
        for (auto pv: pvs) {
            if (dep.test(pv->version) &&
                    pv->download.isValid() &&
                    PackageVersion::indexOf(avoid, pv) < 0) {
                res.push_back(pv->clone());
            }
        }
    }
    qDeleteAll(pvs);

    return res;
}

PackageVersion *AbstractRepository::findBestMatchToInstall(
        const Dependency &dep, const std::vector<PackageVersion *> &avoid,
        QString *err) const
{
    PackageVersion* res = nullptr;

    std::vector<PackageVersion*> pvs = getPackageVersions_(dep.package, err);
    if (err->isEmpty()) {
        for (auto pv: pvs) {
            if (dep.test(pv->version) &&
                    pv->download.isValid() &&
                    PackageVersion::indexOf(avoid, pv) < 0) {
                if (res == nullptr || pv->version.compare(res->version) > 0)
                    res = pv;
            }
        }
    }

    if (res) {
        res = res->clone();
    }
    qDeleteAll(pvs);

    return res;
}

QString AbstractRepository::toString(const Dependency &dep,
        bool includeFullPackageName)
{
    QString res;

    Package* p = findPackage_(dep.package);
    if (p)
        res.append(p->title);
    else
        res.append(dep.package);
    delete p;

    if (includeFullPackageName) {
        res.append(" (").append(dep.package).append(")");
    }

    res.append(" ");

    if (dep.minIncluded)
        res.append('[');
    else
        res.append('(');

    res.append(dep.min.getVersionString());

    res.append(", ");

    res.append(dep.max.getVersionString());

    if (dep.maxIncluded)
        res.append(']');
    else
        res.append(')');

    return res;
}

bool AbstractRepository::lockInstallationScript(Job* job)
{
    bool r = false;

    QString initialTitle = job->getTitle();
    time_t start = time(nullptr);
    while (!job->isCancelled()) {
        bool installationScriptAcquired = installationScripts.try_lock();
        if (installationScriptAcquired) {
            job->setProgress(1);
            r = true;
            break;
        }

        time_t seconds = time(nullptr) - start;
        job->setTitle(initialTitle + " / " + QObject::tr("%1 minutes").
                arg(seconds / 60));
    }
    job->setTitle(initialTitle);

    job->complete();

    return r;
}

void AbstractRepository::unlockInstallationScript()
{
    installationScripts.unlock();
}

QString AbstractRepository::createDirForInstallation(Job& job,
        const PackageVersion& pv,
        const InstallOperation& op, const QString& dir) {
    QString ret;
    QDir d;

    if (op.where.isEmpty()) {
        // if we are not forced to install in a particular
        // directory, we try to use the ideal location
        QString try_ = pv.getIdealInstallationDirectory();
        if (WPMUtils::pathEquals(try_, dir) ||
                (!d.exists(try_) && d.rename(dir, try_))) {
            ret = try_;
        } else {
            qCWarning(npackdImportant()).noquote() << QObject::tr(
                    "The preferred installation directory \"%1\" is not available").arg(try_);

            try_ = pv.getSecondaryInstallationDirectory();
            if (WPMUtils::pathEquals(try_, dir) ||
                    (!d.exists(try_) && d.rename(dir, try_))) {
                ret = try_;
            } else {
                try_ = WPMUtils::findNonExistingFile(try_, "");
                if (WPMUtils::pathEquals(try_, dir) ||
                        (!d.exists(try_) && d.rename(dir, try_))) {
                    ret = try_;
                }
            }
        }
    } else {
        if (d.exists(op.where)) {
            if (!WPMUtils::pathEquals(op.where, dir) &&
                    op.exactLocation) {
                job.setErrorMessage(QObject::tr(
                        "Cannot install %1 into %2. The directory already exists.").
                        arg(pv.toString(true), op.where));
            } else {
                ret = dir;
            }
        } else {
            ret = op.where;

            Job* sub = job.newSubJob(0.99, QObject::tr("Rename %1 to %2").
                arg(dir, op.where), true, false);
            WPMUtils::renameDirectory(&job, dir, op.where);

            if (!sub->getErrorMessage().isEmpty()) {
                 if (op.exactLocation) {
                    job.setErrorMessage(sub->getErrorMessage());
                 } else {
                    // we were not able to rename the directory, but an exact
                    // location is not obligatory
                    ret = dir;
                }
            }
        }
    }

    job.complete();

    return ret;
}

void AbstractRepository::process(Job *job,
        const std::vector<InstallOperation *> &install_,
        const DAG& opsDependencies_, DWORD programCloseType,
        bool printScriptOutput, bool interactive,
        const QString& user, const QString& password,
        const QString& proxyUser, const QString& proxyPassword)
{
    DAG opsDependencies(opsDependencies_);

    if (npackd().isDebugEnabled()) {
        qCDebug(npackd) << "AbstractRepository::process: " <<
                install_.size() << " operations";
        for (int i = 0; i < static_cast<int>(install_.size()); i++) {
            auto op = install_.at(i);
            qCDebug(npackd) << op->package << ": " <<
                    op->version.getVersionString() <<
                    (op->install ? "install" : "remove") << " in " << op->where;

            if (static_cast<int>(opsDependencies.size()) > i) {
                auto edges = opsDependencies.getEdges(i);
                for (auto edge: edges) {
                    qCDebug(npackd) << "depends on" << edge;
                }
            }
        }
    }

    QDir d;

    std::vector<InstallOperation *> install = install_;

    // reoder the operations if a package is updated. In this case it is better
    // to uninstall the old first and then install the new one.
    if (install.size() == 2) {
        InstallOperation* first = install.at(0);
        InstallOperation* second = install.at(1);
        if (first->package == second->package &&
                first->install && !second->install) {
            install.insert(install.begin(), second);
            install.erase(install.begin() + 2);
            opsDependencies.swapNodes(0, 1);
        }
    }

    // search for PackageVersion objects
    std::vector<PackageVersion*> pvs;
    for (auto op: install) {
        QString err;
        PackageVersion* pv = op->findPackageVersion(&err);
        if (!err.isEmpty()) {
            job->setErrorMessage(QString(
                    QObject::tr("Cannot find the package version %1 %2: %3")).
                    arg(op->package, op->version.getVersionString(), err));
            break;
        }
        if (!pv) {
            job->setErrorMessage(QString(
                    QObject::tr("Cannot find the package version %1 %2")).
                    arg(op->package, op->version.getVersionString()));
            break;
        }
        pvs.push_back(pv);
    }

    if (job->shouldProceed()) {
        for (auto pv: pvs) {
            pv->lock();
        }
    }

    int n = install.size();

    // where the binary was downloaded
    std::vector<QString> dirs;

    // names of the binaries relative to the directory
    std::vector<QString> binaries;

    // 70% for downloading the binaries
    if (job->shouldProceed()) {
        // QString here is the full path to the downloaded file
        // or an empty string if not applicable
        std::vector<std::future<QString>> downloadFutures;

        // downloading packages
        for (int i = 0; i < static_cast<int>(install.size()); i++) {
            InstallOperation* op = install.at(i);
            PackageVersion* pv = pvs.at(i);
            if (op->install) {
                QString txt = QObject::tr("Downloading %1").arg(
                        pv->toString());

                Job* sub = job->newSubJob(0.7 / n, txt, true, true);

                // dir is not the final installation directory. It can be
                // changed later during the installation.
                QString dir = op->where;
                if (dir.isEmpty()) {
                    dir = pv->getIdealInstallationDirectory();
                }
                dir = WPMUtils::findNonExistingFile(dir, "");

                if (!d.mkpath(dir)) {
                    sub->setErrorMessage(QObject::tr("Cannot create the directory %1").
                            arg(dir));
                } else {
                    dirs.push_back(dir);

                    std::future<QString> future = std::async(
                        std::launch::async,
                        [pv, interactive, sub, dir, user, password, proxyUser, proxyPassword](){
                            return pv->download_(sub, dir, interactive,
                                    user, password, proxyUser, proxyPassword);
                        });
                    downloadFutures.push_back(std::move(future));

                    if (sub->isCancelled())
                        job->cancel();
                }
            } else {
                dirs.push_back("");
                std::future<QString> future = std::async([](){ return QString(); });
                downloadFutures.push_back(std::move(future));
                job->setProgress(job->getProgress() + 0.7 / n);
            }

            if (!job->shouldProceed())
                break;
        }

        // waiting for all downloads to finish
        if (job->shouldProceed()) {
            for (int i = 0; i < static_cast<int>(downloadFutures.size()); i++) {
                binaries.push_back(QFileInfo(downloadFutures.at(i).get()).fileName());
                if (!job->shouldProceed())
                    break;
            }
        }
    }

    std::vector<QString> stoppedServices;
    std::mutex stoppedServicesLock;

    // 10% for stopping the packages
    if (job->shouldProceed()) {
        for (int i = 0; i < static_cast<int>(install.size()); i++) {
            InstallOperation* op = install.at(i);
            PackageVersion* pv = pvs.at(i);
            if (!op->install) {
                Job* sub = job->newSubJob(0.1 / n,
                        QObject::tr("Stopping the package %1").
                        arg(pv->toString(false)));
                pv->stop(sub, programCloseType, printScriptOutput,
                        &stoppedServices);
                if (!sub->getErrorMessage().isEmpty()) {
                    job->setErrorMessage(sub->getErrorMessage());
                    break;
                }
            } else {
                job->setProgress(job->getProgress() + 0.1 / n);
            }
        }
    }

    int processed = 0;

    // 19% for removing/installing the packages
    if (job->shouldProceed()) {
        std::vector<std::function<void(Job*)>> tasks;

        Job* prep = job->newSubJob(0.01, QObject::tr("Prepare install operations"), true, true);

        // installing/removing packages
        for (int i = 0; i < static_cast<int>(install.size()); i++) {
            InstallOperation* op = install.at(i);
            PackageVersion* pv = pvs.at(i);
            QString txt;
            if (op->install)
                txt = QString(QObject::tr("Installing %1")).arg(
                        pv->toString());
            else
                txt = QString(QObject::tr("Uninstalling %1")).arg(
                        pv->toString());

            if (op->install) {
                QString dir = dirs.at(i);
                QString binary = binaries.at(i);

                tasks.push_back([this, pv, dir, op, binary, printScriptOutput, programCloseType,
                    &stoppedServicesLock, &stoppedServices](Job* job){
                    Job* sub = job->newSubJob(0.01,
                        QObject::tr("Create package directory"), true, true);
                    QString newDir = createDirForInstallation(*sub, *pv, *op, dir);

                    std::vector<QString> svc;
                    if (job->shouldProceed()) {
                        sub = job->newSubJob(0.99,
                            QObject::tr("Install package"), true, true);
                        pv->install(sub, newDir, binary, printScriptOutput,
                            programCloseType, &svc);
                    }

                    stoppedServicesLock.lock();
                    stoppedServices.insert(stoppedServices.end(), svc.begin(),
                        svc.end());
                    stoppedServicesLock.unlock();

                    job->complete();
                });
            } else {
                tasks.push_back([pv, printScriptOutput, programCloseType,
                    &stoppedServicesLock, &stoppedServices](Job* job){
                    std::vector<QString> svc;
                    pv->uninstall(job, printScriptOutput, programCloseType, &svc);

                    stoppedServicesLock.lock();
                    stoppedServices.insert(stoppedServices.end(), svc.begin(),
                        svc.end());
                    stoppedServicesLock.unlock();
                });
            }

            if (!job->shouldProceed())
                break;

            prep->setProgress((i + 1.0) / n);

            processed = i + 1;
        }

        if (prep->shouldProceed())
            prep->setProgress(1);
        prep->complete();

        if (job->shouldProceed()) {
            Job* sub = job->newSubJob(0.17, QObject::tr("Install operations"), true, true);
            DepTask dt(sub, tasks, opsDependencies, threadPool);
            dt();
        }
    }

    // removing the binaries if we should not proceed
    if (!job->shouldProceed()) {
        for (int i = processed; i < static_cast<int>(dirs.size()); i++) {
            QString dir = dirs.at(i);
            if (!dir.isEmpty()) {
                QString txt = QObject::tr("Deleting %1").arg(dir);

                Job* sub = job->newSubJob(0.01 / dirs.size(), txt, true, false);
                WPMUtils::removeDirectory(sub, dir);
            } else {
                job->setProgress(job->getProgress() + 0.01 / dirs.size());
            }
        }
    }

    for (auto pv: pvs) {
        pv->unlock();
    }

    qDeleteAll(pvs);

    if (job->shouldProceed()) {
        QString err;

        SC_HANDLE schSCManager = OpenSCManager(
                nullptr,                    // local computer
                nullptr,                    // ServicesActive database
                SC_MANAGER_ALL_ACCESS);  // full access rights

        if (nullptr == schSCManager) {
            WPMUtils::formatMessage(GetLastError(), &err);
            err = QObject::tr("OpenSCManager failed: %0").arg(err);

            if (!err.isEmpty())
                qCDebug(npackd) << "AbstractRepository::process: " << err;
        }

        if (err.isEmpty()) {
            for (auto& s: stoppedServices) {
                err = WPMUtils::startService(schSCManager, s);

                if (!err.isEmpty())
                    qCDebug(npackd) << "AbstractRepository::process: " << err;
            }
        }

        if (schSCManager)
            CloseServiceHandle(schSCManager);
    }

    if (job->shouldProceed())
        job->setProgress(1);

    job->complete();
}

std::vector<PackageVersion*> AbstractRepository::getInstalled_(QString *err)
{
    *err = "";

    std::vector<PackageVersion*> ret;
    std::vector<InstalledPackageVersion*> ipvs =
            InstalledPackages::getDefault()->getAll();
    for (auto ipv: ipvs) {
        PackageVersion* pv = this->findPackageVersion_(ipv->package,
                ipv->version, err);
        if (!err->isEmpty())
            break;
        if (pv) {
            ret.push_back(pv);
        }
    }
    qDeleteAll(ipvs);

    return ret;
}

QString AbstractRepository::planAddMissingDeps(InstalledPackages &installed,
        std::vector<InstallOperation*>& ops, DAG& opsDependencies)
{
    QString err;

    std::vector<PackageVersion*> avoid;
    std::vector<InstalledPackageVersion*> all = installed.getAll();
    for (auto ipv: all) {
        PackageVersion* pv = this->findPackageVersion_(ipv->package,
                ipv->version, &err);
        if (!err.isEmpty())
            break;

        if (pv) {
            qDeleteAll(avoid);
            avoid.clear();
            err = planInstallation(installed, pv,
                    ops, opsDependencies, avoid);
            delete pv;
            if (!err.isEmpty())
                break;
        }
    }
    qDeleteAll(all);
    qDeleteAll(avoid);

    return err;
}

QString AbstractRepository::planUpdates(InstalledPackages& installed,
        const std::vector<Package*>& packages,
        std::vector<Dependency*> ranges,
        std::vector<InstallOperation*>& ops,
        DAG& opsDependencies, bool keepDirectories,
        bool install, const QString &where_, bool exactLocation)
{
    QString err;

    std::vector<PackageVersion*> newest, newesti;
    std::vector<bool> used;

    // packages first
    if (err.isEmpty()) {
        for (auto p: packages) {
            PackageVersion* a = findNewestInstallablePackageVersion_(p->name,
                    &err);
            if (!err.isEmpty())
                break;

            // packages that cannot be installed are ignored
            if (a == nullptr) {
                continue;
            }

            InstalledPackageVersion* ib = installed.getNewestInstalled(p->name);
            PackageVersion* b = nullptr;
            if (ib) {
                b = this->findPackageVersion_(p->name, ib->version, &err);
                delete ib;
            }

            if (!err.isEmpty()) {
                err = QString(QObject::tr("Cannot find the newest installed version for %1: %2")).
                        arg(p->title, err);
                break;
            }

            if (b == nullptr) {
                if (!install) {
                    err = QString(QObject::tr("No installed version found for the package %1")).
                            arg(p->title);
                    break;
                }
            }

            qCDebug(npackd) << "planUpdates" <<
                    (b ? b->version.getVersionString() : "not installed") <<
                    "to" <<
                    a->version.getVersionString();

            if (b == nullptr || a->version.compare(b->version) > 0) {
                newest.push_back(a);
                newesti.push_back(b);
                used.push_back(false);
            }
        }
    }

    // version ranges second
    if (err.isEmpty()) {
        for (auto d: ranges) {
            std::unique_ptr<Package> p(findPackage_(d->package));
            if (!p.get()) {
                err = QString(QObject::tr("Cannot find the package %1")).
                        arg(d->package);
                break;
            }

            PackageVersion* a = findBestMatchToInstall(*d,
                    std::vector<PackageVersion*>(), &err);
            if (!err.isEmpty())
                break;

            // packages that cannot be installed are ignored
            if (a == nullptr) {
                continue;
            }

            InstalledPackageVersion* ipv = installed.findHighestInstalledMatch(*d);
            PackageVersion* b = nullptr;
            if (ipv) {
                b = findPackageVersion_(ipv->package, ipv->version, &err);
                if (!err.isEmpty()) {
                    err = QString(QObject::tr("Cannot find the newest installed version for %1: %2")).
                            arg(p->title, err);
                    break;
                }
            }

            if (b == nullptr) {
                if (!install) {
                    err = QString(QObject::tr("No installed version found for the package %1")).
                            arg(p->title);
                    break;
                }
            }

            qCDebug(npackd) << "planUpdates" <<
                    (b ? b->version.getVersionString() : "not installed") <<
                    "to" <<
                    a->version.getVersionString();

            if (b == nullptr || a->version.compare(b->version) > 0) {
                newest.push_back(a);
                newesti.push_back(b);
                used.push_back(false);
            }
        }
    }

    if (err.isEmpty()) {
        qCDebug(npackd) << "planUpdates: searching install/uninstall pairs";

        // many packages cannot be installed side-by-side and overwrite for
        // example
        // the shortcuts of the old version in the start menu. We try to find
        // those packages where the old version can be uninstalled first and
        // then
        // the new version installed. This is the reversed order for an update.
        // If this is possible and does not affect other packages, we do this
        // first.
        for (int i = 0; i < static_cast<int>(newest.size()); i++) {
            std::vector<PackageVersion*> avoid;
            std::vector<InstallOperation*> ops2;
            InstalledPackages installedCopy(installed);
            DAG ops2Dependencies;

            PackageVersion* b = newesti.at(i);
            if (b) {
                QString err = planUninstallation(
                        installedCopy, b->package, b->version, ops2,
                        ops2Dependencies);

                qCDebug(npackd) << "planUpdates: 1st uninstall" <<
                        b->package << "resulted in" << ops2.size() <<
                        "operations with result:" << err;

                if (err.isEmpty()) {
                    QString where;
                    if (i == 0 && !where_.isEmpty())
                        where = where_;
                    else if (keepDirectories)
                        where = b->getPath();

                    err = planInstallation(installedCopy, newest.at(i),
                            ops2, ops2Dependencies, avoid, where);

                    qCDebug(npackd) << "planUpdates: 1st install and uninstall" <<
                            b->package << "resulted in" << ops2.size() <<
                            "operations with result:" << err;

                    if (err.isEmpty()) {
                        if (ops2.size() == 2) {
                            used[i] = true;
                            installed = installedCopy;
                            ops.push_back(ops2[0]);
                            ops.push_back(ops2[1]);

                            // the installation depends on the un-installation
                            opsDependencies.addEdge(ops.size() - 1,
                                    ops.size() - 2);

                            ops2.clear();
                        }
                    }
                }
            }

            qDeleteAll(ops2);
        }
    }

    if (err.isEmpty()) {
        qCDebug(npackd) << "planUpdates:" << newest.size() << "packages";

        for (int i = 0; i < static_cast<int>(newest.size()); i++) {
            if (!used[i]) {
                bool undo = false;

                // another package should not be completely uninstalled
                InstalledPackages installedCopy(installed);

                int oldSize = ops.size();

                QString where;
                PackageVersion* a = newesti.at(i);
                if (keepDirectories && a)
                    where = a->getPath();

                std::vector<PackageVersion*> avoid;
                err = planInstallation(installedCopy, newest.at(i),
                        ops, opsDependencies, avoid, where);
                if (!err.isEmpty())
                    break;

                int sizeAfterInstallation = ops.size();

                PackageVersion* b = newesti.at(i);
                if (b) {
                    err = planUninstallation(installedCopy, b->package,
                            b->version, ops, opsDependencies);

                    qCDebug(npackd) << "planUpdates: 2nd uninstall" <<
                            b->package << "resulted in" << ops.size() <<
                            "operations with result:" << err;

                    if (!err.isEmpty())
                        break;
                }

                if (undo) {
                    while (static_cast<int>(ops.size()) > oldSize) {
                        delete ops.back();
                        ops.pop_back();
                    }
                } else {
                    installed = installedCopy;

                    // all un-installation operations depend on all installation
                    // operations
                    for (int i_ = sizeAfterInstallation;
                            i_ < static_cast<int>(ops.size()); i_++) {
                        for (int j = oldSize; j < sizeAfterInstallation; j++) {
                            opsDependencies.addEdge(i_, j);
                        }
                    }
                }
            }
        }
    }

    if (err.isEmpty()) {
        qCDebug(npackd) << "planUpdates: simplifying" << ops.size() <<
                "operations";

        InstallOperation::simplify(ops);
    }

    for (auto op: ops) {
        if (op->install && !op->where.isEmpty())
            op->exactLocation = exactLocation;
    }

    qCDebug(npackd) << "planUpdates: results in" << ops.size() <<
            "operations with result" << err;
    for (int i = 0; i < static_cast<int>(ops.size()); i++) {
        qCDebug(npackd) << "planUpdates:" << i << ops.at(i)->toString();
    }

    qDeleteAll(newest);
    qDeleteAll(newesti);

    return err;
}

QString AbstractRepository::planInstallation(InstalledPackages &installed,
    PackageVersion *me,
    std::vector<InstallOperation *> &ops, DAG &opsDependencies,
    std::vector<PackageVersion *> &avoid, const QString &where)
{
    int initialOpsSize = ops.size();

    QString res;

    avoid.push_back(me->clone());

    for (auto d: me->dependencies) {
        if (installed.isInstalled(*d))
            continue;

        // we cannot just use Dependency->findBestMatchToInstall here as
        // it is possible that the highest match cannot be installed because
        // of unsatisfied dependencies. Example: the newest version depends
        // on Windows Vista, but the current operating system is XP.

        QString err;
        std::vector<PackageVersion*> pvs = findAllMatchesToInstall(
                *d, avoid, &err);
        if (!err.isEmpty()) {
            res = QString(QObject::tr("Error searching for the dependency matches: %1")).
                       arg(err);
            qDeleteAll(pvs);
            break;
        }

        if (pvs.size() == 0) {
            res = QString(QObject::tr("Unsatisfied dependency: %1")).
                       arg(toString(*d));
            break;
        }

        bool found = false;
        for (auto pv: pvs) {
            InstalledPackages installed2(installed);
            auto opsDependencies2 = opsDependencies;
            int opsCount = ops.size();
            int avoidCount = avoid.size();

            res = planInstallation(installed2, pv, ops,
                    opsDependencies2, avoid);
            if (!res.isEmpty()) {
                // rollback
                while (static_cast<int>(ops.size()) > opsCount) {
                    delete ops.back();
                    ops.pop_back();
                }
                while (static_cast<int>(avoid.size()) > avoidCount) {
                    delete avoid.back();
                    avoid.pop_back();
                }
            } else {
                found = true;
                installed = installed2;
                opsDependencies = opsDependencies2;
                break;
            }
        }
        if (!found) {
            res = QString(QObject::tr("Unsatisfied dependency: %1")).
                       arg(toString(*d));
        }
        qDeleteAll(pvs);
    }

    if (res.isEmpty()) {
        if (!installed.isInstalled(me->package, me->version)) {
            InstallOperation* io = new InstallOperation(me->package,
                    me->version, true);
            io->where = where;
            ops.push_back(io);

            QString where2 = where;
            if (where2.isEmpty()) {
                where2 = me->getIdealInstallationDirectory();
                where2 = WPMUtils::findNonExistingFile(where2, "");
            }
            installed.setPackageVersionPath(me->package, me->version,
                    where2, false);

            int me = ops.size() - 1;
            for (int i = initialOpsSize; i < me; ++i) {
                opsDependencies.addEdge(me, i);
            }
        }
    }

    return res;
}

QString AbstractRepository::planUninstallation(InstalledPackages &installed,
        const QString &package, const Version &version,
        std::vector<InstallOperation *> &ops,
        DAG& opsDependencies)
{
    // qCDebug(npackd) << "PackageVersion::planUninstallation()" << this->toString();
    QString res;

    int initialOpsSize = ops.size();

    if (!installed.isInstalled(package, version))
        return res;

    installed.setPackageVersionPath(package, version, "", false);

    // this loop ensures that all the items in "installed" are processed
    // even if changes in the list were done in nested calls to
    // "planUninstallation"
    while (true) {
        std::unique_ptr<InstalledPackageVersion> ipv(installed.
                findFirstWithMissingDependency());
        if (ipv.get()) {
            res = planUninstallation(installed, ipv->package, ipv->version, ops,
                    opsDependencies);
            if (!res.isEmpty())
                break;
        } else {
            break;
        }
    }

    if (res.isEmpty()) {
        InstallOperation* op = new InstallOperation(package, version, false);
        ops.push_back(op);

        int me = ops.size() - 1;
        for (int i = initialOpsSize; i < me; ++i) {
            opsDependencies.addEdge(me, i);
        }
    }

    return res;
}

PackageVersion* AbstractRepository::findNewestInstalledPackageVersion_(
        const QString &name, QString *err) const
{
    *err = "";

    PackageVersion* r = nullptr;

    InstalledPackageVersion* ipv = InstalledPackages::getDefault()->
            getNewestInstalled(name);

    if (ipv) {
        r = this->findPackageVersion_(name, ipv->version, err);
    }

    delete ipv;

    return r;
}

PackageVersion* AbstractRepository::findNewestInstallablePackageVersion_(
        const QString &package, QString* err) const
{
    PackageVersion* r = nullptr;

    std::vector<PackageVersion*> pvs = this->getPackageVersions_(package, err);
    if (err->isEmpty()) {
        for (auto p: pvs) {
            if (r == nullptr || p->version.compare(r->version) > 0) {
                if (p->download.isValid())
                    r = p;
            }
        }
    }

    if (r)
        r = r->clone();

    qDeleteAll(pvs);

    return r;
}

AbstractRepository::AbstractRepository()
{
}

AbstractRepository::~AbstractRepository()
{
}

QString AbstractRepository::getPackageTitleAndName(const QString &name) const
{
    QString res;
    Package* p = findPackage_(name);
    if (p)
        res = p->title + " (" + name + ")";
    else
        res = name;
    delete p;

    return res;
}

Package* AbstractRepository::findOnePackage(
        const QString& package, QString* err) const
{
    *err = "";

    Package* p = findPackage_(package);

    if (!p) {
        if (!package.contains('.')) {
            std::vector<Package*> packages = findPackagesByShortName(package);

            if (packages.size() == 0) {
                *err = QObject::tr("Unknown package: %1").arg(package);
            } else if (packages.size() > 1) {
                QString names;
                for (auto pi: packages) {
                    if (!names.isEmpty())
                        names.append(", ");
                    names.append(pi->title).append(" (").append(pi->name).
                            append(")");
                }
                *err = QObject::tr("More than one package was found: %1").
                        arg(names);
                qDeleteAll(packages);
            } else {
                p = packages.at(0);
            }
        } else {
            *err = QObject::tr("Unknown package: %1").arg(package);
        }
    }

    return p;
}

QString AbstractRepository::checkInstallationDirectory(const QString &dir) const
{
    QString err;
    if (err.isEmpty() && dir.isEmpty())
        err = QObject::tr("The installation directory cannot be empty");

    if (err.isEmpty() && !QDir(dir).exists())
        err = QObject::tr("The installation directory does not exist");

    if (err.isEmpty()) {
        InstalledPackages* ip = InstalledPackages::getDefault();
        InstalledPackageVersion* ipv = ip->findOwner(dir);
        if (ipv) {
            err = QObject::tr("Cannot change the installation directory to %1. %2 %3 is installed there").
                    arg(dir, getPackageTitleAndName(ipv->package),
                    ipv->version.getVersionString());
            delete ipv;
        }
    }
    return err;
}

