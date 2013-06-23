#include <QApplication>

#include "abstractrepository.h"
#include "wpmutils.h"
#include "windowsregistry.h"
#include "installedpackages.h"

AbstractRepository* AbstractRepository::def = 0;

AbstractRepository *AbstractRepository::getDefault_()
{
    return def;
}

QStringList AbstractRepository::getRepositoryURLs(HKEY hk, const QString& path,
        QString* err)
{
    WindowsRegistry wr;
    *err = wr.open(hk, path, false, KEY_READ);
    QStringList urls;
    if (err->isEmpty()) {
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

void AbstractRepository::setDefault_(AbstractRepository* d)
{
    delete def;
    def = d;
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
            if (WPMUtils::setSystemEnvVar("NPACKD_CL", v).isEmpty())
                WPMUtils::fireEnvChanged();
        }
    }

    return err;
}

void AbstractRepository::process(Job *job,
        const QList<InstallOperation *> &install)
{
    QList<PackageVersion*> pvs;
    for (int i = 0; i < install.size(); i++) {
        InstallOperation* op = install.at(i);

        QString err;
        PackageVersion* pv = op->findPackageVersion(&err);
        if (!err.isEmpty()) {
            job->setErrorMessage(QString(
                    QApplication::tr("Cannot find the package version %1 %2: %3")).
                    arg(op->package).
                    arg(op->version.getVersionString()).
                    arg(err));
            break;
        }
        if (!pv) {
            job->setErrorMessage(QString(
                    QApplication::tr("Cannot find the package version %1 %2")).
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

    if (job->shouldProceed()) {
        for (int i = 0; i < install.count(); i++) {
            InstallOperation* op = install.at(i);
            PackageVersion* pv = pvs.at(i);
            if (op->install)
                job->setHint(QString(QApplication::tr("Installing %1")).arg(
                        pv->toString()));
            else
                job->setHint(QString(QApplication::tr("Uninstalling %1")).arg(
                        pv->toString()));
            Job* sub = job->newSubJob(1.0 / n);
            if (op->install)
                pv->install(sub, pv->getPreferredInstallationDirectory());
            else
                pv->uninstall(sub);
            if (!sub->getErrorMessage().isEmpty())
                job->setErrorMessage(sub->getErrorMessage());
            delete sub;

            if (!job->getErrorMessage().isEmpty())
                break;
        }
    }

    for (int j = 0; j < pvs.size(); j++) {
        PackageVersion* pv = pvs.at(j);
        pv->unlock();
    }

    qDeleteAll(pvs);

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
        QList<InstallOperation*>& ops)
{
    QString err;

    QList<PackageVersion*> installed = getInstalled_(&err);
    QList<PackageVersion*> newest, newesti;
    QList<bool> used;

    if (err.isEmpty()) {
        for (int i = 0; i < packages.count(); i++) {
            Package* p = packages.at(i);

            PackageVersion* a = findNewestInstallablePackageVersion_(p->name);
            if (a == 0) {
                err = QString(QApplication::tr("No installable version found for the package %1")).
                        arg(p->title);
                break;
            }

            PackageVersion* b = findNewestInstalledPackageVersion_(p->name, &err);
            if (!err.isEmpty()) {
                err = QString(QApplication::tr("Cannot find the newest installed version for %1: %2")).
                        arg(p->title).arg(err);
                break;
            }

            if (b == 0) {
                err = QString(QApplication::tr("No installed version found for the package %1")).
                        arg(p->title);
                break;
            }

            if (a->version.compare(b->version) <= 0) {
                err = QString(QApplication::tr("The newest version (%1) for the package %2 is already installed")).
                        arg(b->version.getVersionString()).arg(p->title);
                break;
            }

            newest.append(a);
            newesti.append(b);
            used.append(false);
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
            QList<PackageVersion*> installedCopy = installed;

            QString err = newesti.at(i)->planUninstallation(
                    installedCopy, ops2);
            if (err.isEmpty()) {
                err = newest.at(i)->planInstallation(installedCopy, ops2, avoid);
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

            qDeleteAll(ops2);
        }
    }

    if (err.isEmpty()) {
        for (int i = 0; i < newest.count(); i++) {
            if (!used[i]) {
                QList<PackageVersion*> avoid;
                err = newest.at(i)->planInstallation(installed, ops, avoid);
                if (!err.isEmpty())
                    break;
            }
        }
    }

    if (err.isEmpty()) {
        for (int i = 0; i < newesti.count(); i++) {
            if (!used[i]) {
                err = newesti.at(i)->planUninstallation(installed, ops);
                if (!err.isEmpty())
                    break;
            }
        }
    }

    if (err.isEmpty()) {
        InstallOperation::simplify(ops);
    }

    qDeleteAll(installed);
    qDeleteAll(newest);
    qDeleteAll(newesti);

    return err;
}

QList<QUrl*> AbstractRepository::getRepositoryURLs(QString* err)
{
    // the most errors in this method are ignored so that we get the URLs even
    // if something cannot be done
    QString e;

    QStringList urls = getRepositoryURLs(HKEY_LOCAL_MACHINE,
            "Software\\Npackd\\Npackd\\Reps", &e);

    bool save = false;

    // compatibility for Npackd < 1.17
    if (urls.isEmpty()) {
        urls = getRepositoryURLs(HKEY_CURRENT_USER,
                "Software\\Npackd\\Npackd\\repositories", &e);
        if (urls.isEmpty())
            urls = getRepositoryURLs(HKEY_CURRENT_USER,
                    "Software\\WPM\\Windows Package Manager\\repositories",
                    &e);

        if (urls.isEmpty()) {
            urls.append(
                    "https://windows-package-manager.googlecode.com/hg/repository/Rep.xml");
            if (WPMUtils::is64BitWindows())
                urls.append(
                        "https://windows-package-manager.googlecode.com/hg/repository/Rep64.xml");
        }
        save = true;
    }

    QList<QUrl*> r;
    for (int i = 0; i < urls.count(); i++) {
        r.append(new QUrl(urls.at(i)));
    }

    if (save)
        setRepositoryURLs(r, &e);

    return r;
}

void AbstractRepository::setRepositoryURLs(QList<QUrl*>& urls, QString* err)
{
    WindowsRegistry wr;
    *err = wr.open(HKEY_LOCAL_MACHINE, "",
            false, KEY_CREATE_SUB_KEY);
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
                    r.set("repository", urls.at(i)->toString());
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

    QList<PackageVersion*> pvs = this->getPackageVersions_(name, err);
    if (err->isEmpty()) {
        for (int i = 0; i < pvs.count(); i++) {
            PackageVersion* p = pvs.at(i);
            if (p->installed()) {
                if (r == 0 || p->version.compare(r->version) > 0) {
                    r = p;
                }
            }
        }

        if (r)
            r = r->clone();
    }

    qDeleteAll(pvs);

    return r;
}

QString AbstractRepository::computeNpackdCLEnvVar_(QString* err) const
{
    *err = "";

    QString v;
    PackageVersion* pv;
    if (WPMUtils::is64BitWindows())
        pv = findNewestInstalledPackageVersion_(
            "com.googlecode.windows-package-manager.NpackdCL64", err);
    else
        pv = 0;

    if (err->isEmpty()) {
        if (!pv)
            pv = findNewestInstalledPackageVersion_(
                "com.googlecode.windows-package-manager.NpackdCL", err);
    }

    if (pv)
        v = pv->getPath();

    delete pv;

    return v;
}

PackageVersion* AbstractRepository::findNewestInstallablePackageVersion_(
        const QString &package) const
{
    PackageVersion* r = 0;

    QString err; // TODO: the error is not handled
    QList<PackageVersion*> pvs = this->getPackageVersions_(package, &err);
    for (int i = 0; i < pvs.count(); i++) {
        PackageVersion* p = pvs.at(i);
        if (r == 0 || p->version.compare(r->version) > 0) {
            if (p->download.isValid())
                r = p;
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
