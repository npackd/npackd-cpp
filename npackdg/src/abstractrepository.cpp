#include "QLoggingCategory"

#include "abstractrepository.h"
#include "wpmutils.h"
#include "windowsregistry.h"
#include "installedpackages.h"
#include "downloader.h"

QSemaphore AbstractRepository::installationScripts(1);

QStringList AbstractRepository::getRepositoryURLs(HKEY hk, const QString& path,
        QString* err, bool* keyExists)
{
    *keyExists = false;

    WindowsRegistry wr;
    *err = wr.open(hk, path, false, KEY_READ);
    QStringList urls;
    if (err->isEmpty()) {
        *keyExists = true;
        int size = wr.getDWORD("size", err);
        if (err->isEmpty()) {
            for (int i = 1; i <= size; i++) {
                WindowsRegistry er;
                *err = er.open(wr, QString("%1").arg(i), KEY_READ);
                if (err->isEmpty()) {
                    QString url = er.get("repository", err);
                    if (err->isEmpty())
                        urls.append(url);
                }
            }

            // ignore any errors while reading the entries
            *err = "";
        }
    }

    return urls;
}

QString AbstractRepository::updateNpackdCLEnvVar()
{
    QString err;
    QString v = computeNpackdCLEnvVar_(&err);

    if (err.isEmpty()) {
        // ignore the error for the case NPACKD_CL does not yet exist
        QString e;
        QString cur = WPMUtils::getSystemEnvVar("NPACKD_CL", &e);

        if (v != cur) {
            if (WPMUtils::setSystemEnvVar("NPACKD_CL", v).isEmpty()) {
                /* this is slow WPMUtils::fireEnvChanged() */;
            }
        }
    }

    return err;
}

bool AbstractRepository::includesRemoveItself(
        const QList<InstallOperation *> &install_)
{
    bool res = false;

    QString exeDir = WPMUtils::getExeDir();
    for (int i = 0; i < install_.count(); i++) {
        InstallOperation* op = install_.at(i);
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

void AbstractRepository::processWithCoInitializeAndFree(Job *job,
        const QList<InstallOperation *> &install_, DWORD programCloseType)
{
    QThread::currentThread()->setPriority(QThread::LowestPriority);

    /*
    makes the process too slow
    bool b = SetThreadPriority(GetCurrentThread(),
            THREAD_MODE_BACKGROUND_BEGIN);
    */

    CoInitialize(NULL);
    process(job, install_, programCloseType, false, true, "", "", "", "");
    CoUninitialize();

    qDeleteAll(install_);

    /*
    if (b)
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    */
}

void AbstractRepository::exportPackagesCoInitializeAndFree(Job *job,
        const QList<PackageVersion *> &pvs, const QString& where,
        int def)
{
    QDir d;

    QThread::currentThread()->setPriority(QThread::LowestPriority);

    CoInitialize(NULL);

    if (job->shouldProceed()) {
        if (!d.mkpath(where)) {
            job->setErrorMessage(QObject::tr("Cannot create directory: %0").
                    arg(where));
        } else {
            job->setProgress(0.01);
        }
    }

    if (def == 3) {
        for (int i = 0; i < pvs.size(); i++) {
            if (!job->shouldProceed())
                break;

            PackageVersion* pv = pvs.at(i);

            Job* djob = job->newSubJob(0.9 / pvs.size(),
                    QObject::tr("Downloading & computing hash sum for %1").
                    arg(pv->getPackageTitle()), true, true);

            QString fn = pv->download.path();
            QStringList parts = fn.split('/');
            fn = where + "\\";

            QFileInfo fi(parts.at(parts.count() - 1));
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
            QScopedPointer<Package> super(new Package(
                    WPMUtils::getHostName() + ".super",
                    QObject::tr("List of packages")));
            rep->savePackage(super.data(), true);

            QScopedPointer<PackageVersion> superv(
                    new PackageVersion(super.data()->name));
            if (!superv->version.setVersion(QDateTime::currentDateTime().toString(
                    "yyyy.M.d.h.m.s"))) {
                job->setErrorMessage("Internal error: unexpected version syntax");
            }
            superv->type = 1;
            superv->download.setUrl("Rep.xml");
            for (int i = 0; i < pvs.size(); i++) {
                PackageVersion* pv = pvs.at(i);
                Dependency* d = new Dependency();
                d->package = pv->package;
                d->setVersions("[0,2000000000)");
                superv.data()->dependencies.append(d);
            }
            rep->savePackageVersion(superv.data(), true);
        }

        if (def == 1 || def == 2 || def == 3) {
            for (int i = 0; i < pvs.size(); i++) {
                if (!job->shouldProceed())
                    break;

                PackageVersion* pv = pvs.at(i);
                QString err = rep->savePackageVersion(pv, true);
                if (!err.isEmpty()) {
                    job->setErrorMessage(err);
                    break;
                }

                QScopedPointer<Package> p(findPackage_(pv->package));
                if (p) {
                    err = rep->savePackage(p.data(), false);
                    if (!err.isEmpty()) {
                        job->setErrorMessage(err);
                        break;
                    }
                    if (!p->license.isEmpty()) {
                        QScopedPointer<License> lic(findLicense_(p->license, &err));
                        if (lic) {
                            err = rep->saveLicense(lic.data(), false);
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

QList<PackageVersion *> AbstractRepository::findAllMatchesToInstall(
        const Dependency &dep, const QList<PackageVersion *> &avoid,
        QString *err)
{
    QList<PackageVersion*> res;

    QList<PackageVersion*> pvs = getPackageVersions_(dep.package, err);
    if (err->isEmpty()) {
        for (int i = 0; i < pvs.count(); i++) {
            PackageVersion* pv = pvs.at(i);
            if (dep.test(pv->version) &&
                    pv->download.isValid() &&
                    PackageVersion::indexOf(avoid, pv) < 0) {
                res.append(pv->clone());
            }
        }
    }
    qDeleteAll(pvs);

    return res;
}

PackageVersion *AbstractRepository::findBestMatchToInstall(
        const Dependency &dep, const QList<PackageVersion *> &avoid,
        QString *err)
{
    PackageVersion* res = 0;

    QList<PackageVersion*> pvs = getPackageVersions_(dep.package, err);
    if (err->isEmpty()) {
        for (int i = 0; i < pvs.count(); i++) {
            PackageVersion* pv = pvs.at(i);
            if (dep.test(pv->version) &&
                    pv->download.isValid() &&
                    PackageVersion::indexOf(avoid, pv) < 0) {
                if (res == 0 || pv->version.compare(res->version) > 0)
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

InstalledPackageVersion *AbstractRepository::findHighestInstalledMatch(
        const Dependency &dep) const
{
    QList<InstalledPackageVersion*> list = findAllInstalledMatches(dep);
    InstalledPackageVersion* res = 0;
    for (int i = 0; i < list.count(); i++) {
        InstalledPackageVersion* ipv = list.at(i);
        if (res == 0 || ipv->version.compare(res->version) > 0)
            res = ipv;
    }
    if (res)
        res = res->clone();
    qDeleteAll(list);

    return res;
}

QList<InstalledPackageVersion *> AbstractRepository::findAllInstalledMatches(
        const Dependency &dep) const
{
    QList<InstalledPackageVersion*> r;
    InstalledPackages* ip = InstalledPackages::getDefault();
    QList<InstalledPackageVersion*> installed = ip->getAll();
    for (int i = 0; i < installed.count(); i++) {
        InstalledPackageVersion* ipv = installed.at(i);
        if (ipv->package == dep.package &&
                dep.test(ipv->version)) {
            r.append(ipv->clone());
        }
    }
    qDeleteAll(installed);
    return r;
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

void AbstractRepository::process(Job *job,
        const QList<InstallOperation *> &install_, DWORD programCloseType,
        bool printScriptOutput, bool interactive,
        const QString user, const QString password,
        const QString proxyUser, const QString proxyPassword)
{
    QString initialTitle = job->getTitle();

    if (npackd().isDebugEnabled()) {
        qCDebug(npackd) << "AbstractRepository::process: " <<
                install_.size() << " operations";
        for (int i = 0; i < install_.size(); i++) {
            InstallOperation* op = install_.at(i);
            qCDebug(npackd) << op->package << ": " <<
                    op->version.getVersionString() <<
                    op->install << " in " << op->where;
        }
    }

    QDir d;

    QList<InstallOperation *> install = install_;

    // reoder the operations if a package is updated. In this case it is better
    // to uninstall the old first and then install the new one.
    if (install.size() == 2) {
        InstallOperation* first = install.at(0);
        InstallOperation* second = install.at(1);
        if (first->package == second->package &&
                first->install && !second->install) {
            install.insert(0, second);
            install.removeAt(2);
        }
    }

    // search for PackageVersion objects
    QList<PackageVersion*> pvs;
    for (int i = 0; i < install.size(); i++) {
        InstallOperation* op = install.at(i);

        QString err;
        PackageVersion* pv = op->findPackageVersion(&err);
        if (!err.isEmpty()) {
            job->setErrorMessage(QString(
                    QObject::tr("Cannot find the package version %1 %2: %3")).
                    arg(op->package).
                    arg(op->version.getVersionString()).
                    arg(err));
            break;
        }
        if (!pv) {
            job->setErrorMessage(QString(
                    QObject::tr("Cannot find the package version %1 %2")).
                    arg(op->package).
                    arg(op->version.getVersionString()));
            break;
        }
        pvs.append(pv);
    }

    if (job->shouldProceed()) {
        for (int j = 0; j < pvs.size(); j++) {
            PackageVersion* pv = pvs.at(j);
            pv->lock();
        }
    }

    int n = install.count();

    // where the binary was downloaded
    QStringList dirs;

    // names of the binaries relative to the directory
    QStringList binaries;

    // 70% for downloading the binaries
    if (job->shouldProceed()) {
        // downloading packages
        for (int i = 0; i < install.count(); i++) {
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

                if (d.exists(dir)) {
                    sub->setErrorMessage(
                            QObject::tr("Directory %1 already exists").
                            arg(dir));
                    dirs.append("");
                    binaries.append("");
                } else {
                    dirs.append(dir);

                    QString binary = pv->download_(sub, dir, interactive,
                            user, password, proxyUser, proxyPassword);
                    binaries.append(QFileInfo(binary).fileName());
                    if (sub->isCancelled())
                        job->cancel();
                }
            } else {
                dirs.append("");
                binaries.append("");
                job->setProgress(job->getProgress() + 0.7 / n);
            }

            if (!job->shouldProceed())
                break;
        }
    }

    bool installationScriptAcquired = false;

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + " / " +
                QObject::tr("Waiting while other (un)installation scripts are running"));

        time_t start = time(NULL);
        while (!job->isCancelled()) {
            installationScriptAcquired = installationScripts.
                    tryAcquire(1, 10000);
            if (installationScriptAcquired) {
                job->setProgress(0.21);
                break;
            }

            time_t seconds = time(NULL) - start;
            job->setTitle(initialTitle + " / " + QString(
                    QObject::tr("Waiting while other (un)installation scripts are running (%1 minutes)")).
                    arg(seconds / 60));
        }
    }
    job->setTitle(initialTitle);

    QStringList stoppedServices;

    // 10% for stopping the packages
    if (job->shouldProceed()) {
        for (int i = 0; i < install.count(); i++) {
            InstallOperation* op = install.at(i);
            PackageVersion* pv = pvs.at(i);
            if (!op->install) {
                Job* sub = job->newSubJob(0.1 / n,
                        QObject::tr("Stopping the package %1 of %2").
                        arg(i + 1).arg(n));
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
        // installing/removing packages
        for (int i = 0; i < install.count(); i++) {
            InstallOperation* op = install.at(i);
            PackageVersion* pv = pvs.at(i);
            QString txt;
            if (op->install)
                txt = QString(QObject::tr("Installing %1")).arg(
                        pv->toString());
            else
                txt = QString(QObject::tr("Uninstalling %1")).arg(
                        pv->toString());

            Job* sub = job->newSubJob(0.19 / n, txt, true, true);
            if (op->install) {
                QString dir = dirs.at(i);
                QString binary = binaries.at(i);

                if (op->where.isEmpty()) {
                    // if we are not forced to install in a particular
                    // directory, we try to use the ideal location
                    QString try_ = pv->getIdealInstallationDirectory();
                    if (WPMUtils::pathEquals(try_, dir) ||
                            (!d.exists(try_) && d.rename(dir, try_))) {
                        dir = try_;
                    } else {
                        try_ = pv->getSecondaryInstallationDirectory();
                        if (WPMUtils::pathEquals(try_, dir) ||
                                (!d.exists(try_) && d.rename(dir, try_))) {
                            dir = try_;
                        } else {
                            try_ = WPMUtils::findNonExistingFile(try_, "");
                            if (WPMUtils::pathEquals(try_, dir) ||
                                    (!d.exists(try_) && d.rename(dir, try_))) {
                                dir = try_;
                            }
                        }
                    }
                } else {
                    if (d.exists(op->where)) {
                        if (!WPMUtils::pathEquals(op->where, dir)) {
                            // we should install in a particular directory, but it
                            // exists.
                            Job* djob = sub->newSubJob(1,
                                    QObject::tr("Deleting temporary directory %1").
                                    arg(dir));
                            WPMUtils::removeDirectory(djob, dir);
                            job->setErrorMessage(QObject::tr(
                                    "Cannot install %1 into %2. The directory already exists.").
                                    arg(pv->toString(true)).arg(op->where));
                            break;
                        }
                    } else {
                        if (d.rename(dir, op->where))
                            dir = op->where;
                        else {
                            // we should install in a particular directory, but it
                            // exists.
                            Job* djob = sub->newSubJob(1,
                                    QObject::tr("Deleting temporary directory %1").
                                    arg(dir));
                            WPMUtils::removeDirectory(djob, dir);
                            job->setErrorMessage(QObject::tr(
                                    "Cannot install %1 into %2. Cannot rename %3.").
                                    arg(pv->toString(true), op->where, dir));
                            break;
                        }
                    }
                }

                pv->install(sub, dir, binary, printScriptOutput,
                        programCloseType, &stoppedServices);
            } else
                pv->uninstall(sub, printScriptOutput, programCloseType,
                        &stoppedServices);

            if (!job->shouldProceed())
                break;

            processed = i + 1;
        }
    }

    // removing the binaries if we should not proceed
    if (!job->shouldProceed()) {
        for (int i = processed; i < dirs.count(); i++) {
            QString dir = dirs.at(i);
            if (!dir.isEmpty()) {
                QString txt = QObject::tr("Deleting %1").arg(dir);

                Job* sub = job->newSubJob(0.01 / dirs.count(), txt, true, false);
                WPMUtils::removeDirectory(sub, dir);
            } else {
                job->setProgress(job->getProgress() + 0.01 / dirs.count());
            }
        }
    }

    for (int j = 0; j < pvs.size(); j++) {
        PackageVersion* pv = pvs.at(j);
        pv->unlock();
    }

    qDeleteAll(pvs);

    if (job->shouldProceed()) {
        QString err;

        SC_HANDLE schSCManager = OpenSCManager(
                NULL,                    // local computer
                NULL,                    // ServicesActive database
                SC_MANAGER_ALL_ACCESS);  // full access rights

        if (NULL == schSCManager) {
            WPMUtils::formatMessage(GetLastError(), &err);
            err = QObject::tr("OpenSCManager failed: %0").arg(err);

            if (!err.isEmpty())
                qCDebug(npackd) << "AbstractRepository::process: " << err;
        }

        if (err.isEmpty()) {
            for (int i = 0; i < stoppedServices.size(); i++) {
                err = WPMUtils::startService(schSCManager, stoppedServices.at(i));

                if (!err.isEmpty())
                    qCDebug(npackd) << "AbstractRepository::process: " << err;
            }
        }

        if (schSCManager)
            CloseServiceHandle(schSCManager);
    }

    if (installationScriptAcquired)
        installationScripts.release();

    if (job->shouldProceed())
        job->setProgress(1);

    job->complete();
}

QList<PackageVersion*> AbstractRepository::getInstalled_(QString *err)
{
    *err = "";

    QList<PackageVersion*> ret;
    QList<InstalledPackageVersion*> ipvs =
            InstalledPackages::getDefault()->getAll();
    for (int i = 0; i < ipvs.count(); i++) {
        InstalledPackageVersion* ipv = ipvs.at(i);
        PackageVersion* pv = this->findPackageVersion_(ipv->package,
                ipv->version, err);
        if (!err->isEmpty())
            break;
        if (pv) {
            ret.append(pv);
        }
    }
    qDeleteAll(ipvs);

    return ret;
}

QString AbstractRepository::planUpdates(const QList<Package*> packages,
        QList<Dependency*> ranges,
        QList<InstallOperation*>& ops, bool keepDirectories,
        bool install, const QString &where_, bool safe)
{
    QString err;

    InstalledPackages installed(*InstalledPackages::getDefault());
    QList<PackageVersion*> newest, newesti;
    QList<bool> used;

    // get the list of installed packages
    QSet<QString> before;
    if (safe) {
        before = installed.getPackages();
    }

    // packages first
    if (err.isEmpty()) {
        for (int i = 0; i < packages.count(); i++) {
            Package* p = packages.at(i);

            PackageVersion* a = findNewestInstallablePackageVersion_(p->name,
                    &err);
            if (!err.isEmpty())
                break;

            if (a == 0) {
                err = QString(QObject::tr("No installable version found for the package %1")).
                        arg(p->title);
                break;
            }

            PackageVersion* b = findNewestInstalledPackageVersion_(p->name, &err);
            if (!err.isEmpty()) {
                err = QString(QObject::tr("Cannot find the newest installed version for %1: %2")).
                        arg(p->title).arg(err);
                break;
            }

            if (b == 0) {
                if (!install) {
                    err = QString(QObject::tr("No installed version found for the package %1")).
                            arg(p->title);
                    break;
                }
            }

            if (b == 0 || a->version.compare(b->version) > 0) {
                newest.append(a);
                newesti.append(b);
                used.append(false);
            }
        }
    }

    // version ranges second
    if (err.isEmpty()) {
        for (int i = 0; i < ranges.count(); i++) {
            Dependency* d = ranges.at(i);
            QScopedPointer<Package> p(findPackage_(d->package));
            if (!p.data()) {
                err = QString(QObject::tr("Cannot find the package %1")).
                        arg(d->package);
                break;
            }

            PackageVersion* a = findBestMatchToInstall(*d,
                    QList<PackageVersion*>(), &err);
            if (!err.isEmpty())
                break;

            if (a == 0) {
                err = QString(QObject::tr("No installable version found for the package %1")).
                        arg(p->title);
                break;
            }

            InstalledPackageVersion* ipv = findHighestInstalledMatch(*d);
            PackageVersion* b = 0;
            if (ipv) {
                b = findPackageVersion_(ipv->package, ipv->version, &err);
                if (!err.isEmpty()) {
                    err = QString(QObject::tr("Cannot find the newest installed version for %1: %2")).
                            arg(p->title).arg(err);
                    break;
                }
            }

            if (b == 0) {
                if (!install) {
                    err = QString(QObject::tr("No installed version found for the package %1")).
                            arg(p->title);
                    break;
                }
            }

            if (b == 0 || a->version.compare(b->version) > 0) {
                newest.append(a);
                newesti.append(b);
                used.append(false);
            }
        }
    }

    if (err.isEmpty()) {
        // many packages cannot be installed side-by-side and overwrite for
        // example
        // the shortcuts of the old version in the start menu. We try to find
        // those packages where the old version can be uninstalled first and
        // then
        // the new version installed. This is the reversed order for an update.
        // If this is possible and does not affect other packages, we do this
        // first.
        for (int i = 0; i < newest.count(); i++) {
            QList<PackageVersion*> avoid;
            QList<InstallOperation*> ops2;
            InstalledPackages installedCopy(installed);

            PackageVersion* b = newesti.at(i);
            if (b) {
                QString err = b->planUninstallation(
                        installedCopy, ops2);
                if (err.isEmpty()) {
                    QString where;
                    if (i == 0 && !where_.isEmpty())
                        where = where_;
                    else if (keepDirectories)
                        where = b->getPath();

                    err = newest.at(i)->planInstallation(installedCopy, ops2,
                            avoid, where);
                    if (err.isEmpty()) {
                        if (ops2.count() == 2) {
                            used[i] = true;
                            installed = installedCopy;
                            ops.append(ops2[0]);
                            ops.append(ops2[1]);
                            ops2.clear();
                        }
                    }
                }
            }

            qDeleteAll(ops2);
        }
    }

    if (err.isEmpty()) {
        for (int i = 0; i < newest.count(); i++) {
            if (!used[i]) {
                bool undo = false;

                // another package should not be completly uninstalled
                InstalledPackages installedCopy(installed);

                int oldSize = ops.size();

                QString where;
                PackageVersion* a = newesti.at(i);
                if (keepDirectories && a)
                    where = a->getPath();

                QList<PackageVersion*> avoid;
                err = newest.at(i)->planInstallation(installedCopy, ops, avoid,
                        where);
                if (!err.isEmpty())
                    break;

                PackageVersion* b = newesti.at(i);
                if (b) {
                    if (safe) {
                        err = b->planUninstallation(installedCopy, ops);
                        if (!err.isEmpty())
                            break;

                        if ((ops.size() - oldSize) > 1) {
                            QSet<QString> after = before;
                            after.subtract(installedCopy.getPackages());

                            if (!after.isEmpty()) {
                                undo = true;
                            }
                        }
                    } else {
                        err = b->planUninstallation(installedCopy, ops);

                        if (!err.isEmpty())
                            break;
                    }
                }

                if (undo) {
                    while (ops.size() > oldSize)
                        delete ops.takeLast();
                } else {
                    installed = installedCopy;
                }
            }
        }
    }

    if (err.isEmpty()) {
        InstallOperation::simplify(ops);
    }

    qDeleteAll(newest);
    qDeleteAll(newesti);

    return err;
}

QList<QUrl*> AbstractRepository::getRepositoryURLs(QString* err)
{
    // the most errors in this method are ignored so that we get the URLs even
    // if something cannot be done
    QString e;

    bool keyExists;
	QStringList urls = getRepositoryURLs(
		HKEY_LOCAL_MACHINE,
		"SOFTWARE\\Policies\\Npackd\\Reps", &e, &keyExists);

	if (!keyExists) {
		urls = getRepositoryURLs(
            WPMUtils::adminMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
			"Software\\Npackd\\Npackd\\Reps", &e, &keyExists);
	}

    bool save = false;

    // compatibility for Npackd < 1.17
    if (!keyExists) {
        urls = getRepositoryURLs(HKEY_CURRENT_USER,
                "Software\\Npackd\\Npackd\\repositories", &e, &keyExists);
        if (urls.isEmpty())
            urls = getRepositoryURLs(HKEY_CURRENT_USER,
                    "Software\\WPM\\Windows Package Manager\\repositories",
                    &e, &keyExists);

        if (urls.isEmpty()) {
            urls.append(
                    "https://www.npackd.org/rep/xml?tag=stable");
            if (WPMUtils::is64BitWindows())
                urls.append(
                        "https://www.npackd.org/rep/xml?tag=stable64");
        }
        save = true;
    }

    QList<QUrl*> r;
    for (int i = 0; i < urls.count(); i++) {
        QUrl* url = new QUrl(urls.at(i));
        if (url->scheme() == "file")
            *url = QUrl::fromLocalFile(url->toLocalFile().replace('\\', '/'));
        r.append(url);
    }

    if (save)
        setRepositoryURLs(r, &e);

    return r;
}

void AbstractRepository::setRepositoryURLs(QList<QUrl*>& urls, QString* err)
{
    WindowsRegistry wr;
    *err = wr.open(
            WPMUtils::adminMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
			"", false, KEY_CREATE_SUB_KEY);
    if (err->isEmpty()) {
        WindowsRegistry wrr = wr.createSubKey(
                "Software\\Npackd\\Npackd\\Reps", err,
                KEY_ALL_ACCESS);
        if (err->isEmpty()) {
            wrr.setDWORD("size", urls.count());
            for (int i = 0; i < urls.count(); i++) {
                WindowsRegistry r = wrr.createSubKey(QString("%1").arg(i + 1),
                        err, KEY_ALL_ACCESS);
                if (err->isEmpty()) {
                    r.set("repository", urls.at(i)->toString(
                            QUrl::FullyEncoded));
                }
            }
        }
    }
}

PackageVersion* AbstractRepository::findNewestInstalledPackageVersion_(
        const QString &name, QString *err) const
{
    *err = "";

    PackageVersion* r = 0;

    InstalledPackageVersion* ipv = InstalledPackages::getDefault()->
            getNewestInstalled(name);

    if (ipv) {
        r = this->findPackageVersion_(name, ipv->version, err);
    }

    delete ipv;

    return r;
}

QString AbstractRepository::computeNpackdCLEnvVar_(QString* err) const
{
    QString v;
    InstalledPackageVersion* ipv;
    if (WPMUtils::is64BitWindows()) {
        ipv = InstalledPackages::getDefault()->getNewestInstalled(
                "com.googlecode.windows-package-manager.NpackdCL64");
    } else
        ipv = 0;

    if (!ipv)
        ipv = InstalledPackages::getDefault()->getNewestInstalled(
            "com.googlecode.windows-package-manager.NpackdCL");

    if (ipv)
        v = ipv->getDirectory();

    delete ipv;

    // qCDebug(npackd) << "computed NPACKD_CL" << v;

    return v;
}

PackageVersion* AbstractRepository::findNewestInstallablePackageVersion_(
        const QString &package, QString* err) const
{
    PackageVersion* r = 0;

    QList<PackageVersion*> pvs = this->getPackageVersions_(package, err);
    if (err->isEmpty()) {
        for (int i = 0; i < pvs.count(); i++) {
            PackageVersion* p = pvs.at(i);
            if (r == 0 || p->version.compare(r->version) > 0) {
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

QString AbstractRepository::getPackageTitleAndName(const QString &name)
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
        const QString& package, QString* err)
{
    DBRepository* rep = DBRepository::getDefault();
    Package* p = rep->findPackage_(package);

    if (!p && !package.contains('.')) {
        QList<Package*> packages = rep->findPackagesByShortName(package);

        if (packages.count() == 0) {
            *err = QObject::tr("Unknown package: %1").arg(package);
        } else if (packages.count() > 1) {
            QString names;
            for (int i = 0; i < packages.count(); ++i) {
                if (i != 0)
                    names.append(", ");
                Package* pi = packages.at(i);
                names.append(pi->title).append(" (").append(pi->name).
                        append(")");
            }
            *err = QObject::tr("More than one package was found: %1").
                    arg(names);
            qDeleteAll(packages);
        } else {
            p = packages.at(0);
        }
    }

    return p;
}

QString AbstractRepository::checkInstallationDirectory(const QString &dir)
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
            DBRepository* r = DBRepository::getDefault();
            err = QObject::tr("Cannot change the installation directory to %1. %2 %3 is installed there").
                    arg(dir).
                    arg(r->getPackageTitleAndName(ipv->package)).
                    arg(ipv->version.getVersionString());
            delete ipv;
        }
    }
    return err;
}

