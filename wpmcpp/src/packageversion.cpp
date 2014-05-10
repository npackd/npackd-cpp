// IE 6 SP2
#define _WIN32_IE 0x0603

#include <shobjidl.h>
#include <windows.h>
#include <initguid.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wininet.h>
#include <stdlib.h>
#include <time.h>
#include <ole2.h>
#include <comcat.h>

#include <QUrl>
#include <QDebug>
#include <QIODevice>
#include <QProcess>
#include <QDomElement>
#include <QDomDocument>
#include <QMutex>
#include <zlib.h>

//#include "msoav2.h"
#include "quazip.h"
#include "quazipfile.h"

#include "repository.h"
#include "packageversion.h"
#include "job.h"
#include "downloader.h"
#include "wpmutils.h"
#include "repository.h"
#include "version.h"
#include "windowsregistry.h"
#include "xmlutils.h"
#include "installedpackages.h"
#include "installedpackageversion.h"
#include "dbrepository.h"

QSemaphore PackageVersion::httpConnections(3);
QSemaphore PackageVersion::installationScripts(1);
QSet<QString> PackageVersion::lockedPackageVersions;
QMutex PackageVersion::lockedPackageVersionsMutex(QMutex::Recursive);

/**
 * Creates an instance of IAttachmentExecute
 */
HRESULT CreateAttachmentServices(IAttachmentExecute **ppae)
{
    // Assume that CoInitialize has already been called for this thread.
    HRESULT hr = CoCreateInstance(CLSID_AttachmentServices,
                                  NULL,
                                  CLSCTX_ALL,
                                  IID_IAttachmentExecute,
                                  (void**)ppae);

    if (SUCCEEDED(hr))
    {
        // qDebug() << "CoCreateInstance succeeded";
        // Set the client's GUID.

        // UUID_ClientID should be created using uuidgen.exe and
        // defined internally.
        (*ppae)->SetClientGuid(UUID_ClientID);

        // You also could call SetClientTitle at this point, but it is
        // not required.
    }
    return hr;
}

bool isFileSafe(const QString& filename, const QString& url)
{
    bool res = true;

    IAttachmentExecute *pExecute;

    QString err;

    // MS Security Essentials does not support the interface
    HRESULT hr = CreateAttachmentServices(&pExecute);
    if (!SUCCEEDED(hr)) {
        WPMUtils::formatMessage(hr, &err);
        // err = "1: " + err;
        pExecute = 0;
    }

    if (err.isEmpty()) {
        hr = pExecute->SetLocalPath((LPCWSTR) filename.utf16());
        if (!SUCCEEDED(hr)) {
            WPMUtils::formatMessage(hr, &err);
            // err = "2: " + err;
        }
    }

    if (err.isEmpty()) {
        pExecute->SetSource((LPCWSTR) url.utf16());
        if (!SUCCEEDED(hr)) {
            WPMUtils::formatMessage(hr, &err);
            // err = "3: " + err;
        }
    }

    if (err.isEmpty()) {
        hr = pExecute->Save();
        if (hr != S_OK)
            res = false;
    }

    if (pExecute)
        pExecute->Release();

    // qDebug() << err;

    return res;
}

/* this should actually be used by MS Office. MS Essentials and
 * Avira Free Antivirus do not use it
bool isFileSafeOfficeAntiVirus(const QString& filename, QString* err)
{
    bool res = true;

    *err = "";

    ICatInformation *pInfo = 0;
    if (err->isEmpty()) {
        HRESULT hr = CoCreateInstance(CLSID_StdComponentCategoriesMgr, NULL,
                CLSCTX_ALL, IID_ICatInformation, (void**)&pInfo);
        if (!SUCCEEDED(hr))
            WPMUtils::formatMessage(hr, err);
    }

    IEnumCLSID* pEnumCLSID = 0;
    if (err->isEmpty()) {
        ULONG cIDs = 1;
        CATID IDs[1];
        IDs[0] = CATID_MSOfficeAntiVirus;
        HRESULT hr = pInfo->EnumClassesOfCategories(cIDs, IDs, 0, NULL,
                &pEnumCLSID);
        if (!SUCCEEDED(hr))
            WPMUtils::formatMessage(hr, err);
    }

    if (err->isEmpty()) {
        CLSID clsid;

        while (pEnumCLSID->Next(1, &clsid, NULL) == S_OK) {
            qDebug() << "av2";
            IOfficeAntiVirus* m_pOfficeAntiVirus;
            HRESULT hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER,
                    IID_IOfficeAntiVirus, (void**)&m_pOfficeAntiVirus);
            if (SUCCEEDED(hr)) {
                MSOAVINFO msoavinfo;
                ZeroMemory(&msoavinfo, sizeof(MSOAVINFO));
                msoavinfo.cbsize = sizeof(MSOAVINFO);
                msoavinfo.hwnd = ::GetActiveWindow();
                msoavinfo.fPath = 1;
                msoavinfo.fReadOnlyRequest = 0;
                msoavinfo.fInstalled = 0;
                msoavinfo.fHttpDownload = 0;
                msoavinfo.u.pwzFullPath = (WCHAR*) filename.utf16();
                msoavinfo.pwzHostName = (WCHAR*) L"Npackd";
                msoavinfo.pwzOrigURL = 0; // (WCHAR*) L"http://www.test.de";

                hr = m_pOfficeAntiVirus->Scan(&msoavinfo);
                if (hr == S_OK)
                    res = true; // no virus found
                else {
                    res = false;
                    m_pOfficeAntiVirus->Release();
                    break;
                }

                m_pOfficeAntiVirus->Release();
            }
        }
    }

    if (pEnumCLSID) {
        pEnumCLSID->Release();
        pEnumCLSID = 0;
    }

    if (pInfo) {
        pInfo->Release();
        pInfo = 0;
    }

    if (!res)
        *err = "";

    return res;
}
*/

int PackageVersion::indexOf(const QList<PackageVersion*> pvs, PackageVersion* f)
{
    int r = -1;
    for (int i = 0; i < pvs.count(); i++) {
        PackageVersion* pv = pvs.at(i);
        if (pv->package == f->package && pv->version == f->version) {
            r = i;
            break;
        }
    }
    return r;
}

PackageVersion* PackageVersion::findLockedPackageVersion(QString *err)
{
    *err = "";

    PackageVersion* r = 0;

    lockedPackageVersionsMutex.lock();
    QSetIterator<QString> i(lockedPackageVersions);
    QString key;
    if (i.hasNext()) {
        key = i.next();
    }
    lockedPackageVersionsMutex.unlock();

    if (!key.isEmpty()) {
        QStringList parts = key.split('/');
        if (parts.count() == 2) {
            QString package = parts.at(0);
            Version version;
            if (version.setVersion(parts.at(1))) {
                DBRepository* rep = DBRepository::getDefault();
                r = rep->findPackageVersion_(package, version, err);
            }
        }
    }
    return r;
}

PackageVersion::PackageVersion(const QString& package)
{
    this->package = package;
    this->type = 0;
    this->hashSumType = QCryptographicHash::Sha1;
}

PackageVersion::PackageVersion(const QString &package, const Version &version)
{
    this->package = package;
    this->version = version;
    this->type = 0;
    this->hashSumType = QCryptographicHash::Sha1;
}

PackageVersion::PackageVersion()
{
    this->package = "unknown";
    this->type = 0;
    this->hashSumType = QCryptographicHash::Sha1;
}

void PackageVersion::emitStatusChanged()
{
    InstalledPackages::getDefault()->fireStatusChanged(this->package,
            this->version);
}

void PackageVersion::lock()
{
    QString key = getStringId();

    bool changed = false;
    lockedPackageVersionsMutex.lock();
    if (!lockedPackageVersions.contains(key)) {
        lockedPackageVersions.insert(key);
        changed = true;
    }
    lockedPackageVersionsMutex.unlock();

    if (changed)
        emitStatusChanged();
}

void PackageVersion::unlock()
{
    QString key = getStringId();

    bool changed = false;
    lockedPackageVersionsMutex.lock();
    if (lockedPackageVersions.contains(key)) {
        lockedPackageVersions.remove(key);
        changed = true;
    }
    lockedPackageVersionsMutex.unlock();

    if (changed)
        emitStatusChanged();
}

bool PackageVersion::isLocked() const
{
    bool r;
    lockedPackageVersionsMutex.lock();
    r = lockedPackageVersions.contains(getStringId());
    lockedPackageVersionsMutex.unlock();
    return r;
}

QString PackageVersion::getStringId() const
{
    return getStringId(this->package, this->version);
}

void PackageVersion::fillFrom(PackageVersion *pv)
{
    this->package = pv->package;
    this->version = pv->version;
    this->importantFiles = pv->importantFiles;
    this->importantFilesTitles = pv->importantFilesTitles;
    this->type = pv->type;
    this->sha1 = pv->sha1;
    this->hashSumType = pv->hashSumType;
    this->download = pv->download;
    this->msiGUID = pv->msiGUID;

    qDeleteAll(this->files);
    this->files.clear();
    for (int i = 0; i < pv->files.count(); i++) {
        this->files.append(pv->files.at(i)->clone());
    }

    qDeleteAll(this->detectFiles);
    this->detectFiles.clear();
    for (int i = 0; i < pv->detectFiles.count(); i++) {
        this->detectFiles.append(pv->detectFiles.at(i)->clone());
    }

    qDeleteAll(this->dependencies);
    this->dependencies.clear();
    for (int i = 0; i < pv->dependencies.count(); i++) {
        this->dependencies.append(pv->dependencies.at(i)->clone());
    }
}

QString PackageVersion::getPath() const
{
    return InstalledPackages::getDefault()->
            getPath(this->package, this->version);
}

QString PackageVersion::setPath(const QString& path)
{
    //qDebug() << "PackageVersion::setPath " << path;
    InstalledPackages* ip = InstalledPackages::getDefault();
    QString err = ip->setPackageVersionPath(this->package, this->version, path);
    if (!err.isEmpty()) {
        err = QString(QObject::tr("Error storing the information about an installed package version in the Windows registry: %1")).arg(err);
    }
    return err;
}

bool PackageVersion::isDirectoryLocked()
{
    if (installed()) {
        QDir d(getPath());
        QDateTime now = QDateTime::currentDateTime();
        QString newName = QString("%1-%2").arg(d.absolutePath()).arg(now.toTime_t());

        if (!d.rename(d.absolutePath(), newName)) {
            return true;
        }

        if (!d.rename(newName, d.absolutePath())) {
            return true;
        }
    }

    return false;
}


QString PackageVersion::toString(bool includeFullPackageName)
{
    QString r = this->getPackageTitle(includeFullPackageName) + " " +
            this->version.getVersionString();
    return r;
}

QString PackageVersion::getShortPackageName()
{
    QStringList sl = this->package.split(".");
    return sl.last();
}

PackageVersion::~PackageVersion()
{
    qDeleteAll(this->detectFiles);
    qDeleteAll(this->files);
    qDeleteAll(this->dependencies);
}

bool PackageVersion::installed() const
{
    return InstalledPackages::getDefault()->isInstalled(this->package,
            this->version);
}

void PackageVersion::deleteShortcuts(const QString& dir, Job* job,
        bool menu, bool desktop, bool quickLaunch)
{
    if (menu) {
        job->setHint(QObject::tr("Start menu"));
        QDir d(WPMUtils::getShellDir(CSIDL_STARTMENU));
        WPMUtils::deleteShortcuts(dir, d);

        QDir d2(WPMUtils::getShellDir(CSIDL_COMMON_STARTMENU));
        WPMUtils::deleteShortcuts(dir, d2);
    }
    job->setProgress(0.33);

    if (desktop) {
        job->setHint(QObject::tr("Desktop"));
        QDir d3(WPMUtils::getShellDir(CSIDL_DESKTOP));
        WPMUtils::deleteShortcuts(dir, d3);

        QDir d4(WPMUtils::getShellDir(CSIDL_COMMON_DESKTOPDIRECTORY));
        WPMUtils::deleteShortcuts(dir, d4);
    }
    job->setProgress(0.66);

    if (quickLaunch) {
        job->setHint(QObject::tr("Quick launch bar"));
        const char* A = "\\Microsoft\\Internet Explorer\\Quick Launch";
        QDir d3(WPMUtils::getShellDir(CSIDL_APPDATA) + A);
        WPMUtils::deleteShortcuts(dir, d3);

        QDir d4(WPMUtils::getShellDir(CSIDL_COMMON_APPDATA) + A);
        WPMUtils::deleteShortcuts(dir, d4);
    }
    job->setProgress(1);

    job->complete();
}

void PackageVersion::uninstall(Job* job, int programCloseType)
{
    if (!installed()) {
        job->setProgress(1);
        job->complete();
        return;
    }

    QDir d(getPath());

    if (job->shouldProceed(QObject::tr("Closing running processes"))) {
        WPMUtils::closeProcessesThatUseDirectory(this->getPath(),
                programCloseType);
        if (isDirectoryLocked()) {
            QString exe = WPMUtils::findFirstExeLockingDirectory(
                    this->getPath());
            if (exe.isEmpty())
                job->setErrorMessage(QObject::tr("Directory %0 is locked").arg(
                        this->getPath()));
            else
                job->setErrorMessage(
                        QObject::tr("Directory %0 is locked by %1").arg(
                        this->getPath()).arg(exe));
        } else
            job->setProgress(0.05);
    }

    if (job->getErrorMessage().isEmpty()) {
        job->setHint(QObject::tr("Deleting shortcuts"));
        Job* sub = job->newSubJob(0.15);
        deleteShortcuts(d.absolutePath(), sub, true, false, false);
        delete sub;
    }

    QString uninstallationScript;
    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        uninstallationScript = ".Npackd\\Uninstall.bat";
        if (!QFile::exists(d.absolutePath() +
                "\\" + uninstallationScript)) {
            uninstallationScript = ".WPM\\Uninstall.bat";
            if (!QFile::exists(d.absolutePath() +
                    "\\" + uninstallationScript)) {
                uninstallationScript = "";
            }
        }
    }

    bool uninstallationScriptAcquired = false;

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        if (!uninstallationScript.isEmpty()) {
            job->setHint(QObject::tr("Waiting while other (un)installation scripts are running"));

            time_t start = time(NULL);
            while (!job->isCancelled()) {
                uninstallationScriptAcquired = installationScripts.
                        tryAcquire(1, 10000);
                if (uninstallationScriptAcquired) {
                    job->setProgress(0.25);
                    break;
                }

                time_t seconds = time(NULL) - start;
                job->setHint(QString(
                        QObject::tr("Waiting while other (un)installation scripts are running (%1 minutes)")).
                        arg(seconds / 60));
            }
        } else {
            job->setProgress(0.25);
        }
    }

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        if (!uninstallationScript.isEmpty()) {
            job->setHint(QObject::tr("Running the uninstallation script (this may take some time)"));
            if (!d.exists(".Npackd"))
                d.mkdir(".Npackd");
            Job* sub = job->newSubJob(0.20);

            // prepare the environment variables

            QStringList env;
            QString err = addBasicVars(&env);
            if (!err.isEmpty())
                job->setErrorMessage(err);

            addDependencyVars(&env);

            // some uninstallers delete all files in the directory including
            // the un-installation script. cmd.exe returns an error code in
            // this case and stops the script execution. We try to prevent
            // this by locking the Uninstall.bat script
            QString usfn = d.absolutePath() + "\\" + uninstallationScript;
            HANDLE ush = CreateFile((LPCWSTR) usfn.utf16(),
                    GENERIC_READ,
                    FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

            /* debugging
            if (ush != INVALID_HANDLE_VALUE) {
                qDebug() << "file handle is OK";
            }
            */

            this->executeFile2(sub, d.absolutePath(), uninstallationScript,
                    ".Npackd\\Uninstall.log", "NpackdUninstallXXXXXX.log",
                    env);

            if (!sub->getErrorMessage().isEmpty())
                job->setErrorMessage(sub->getErrorMessage());

            delete sub;

            if (ush != INVALID_HANDLE_VALUE) {
                CloseHandle(ush);
            }
        }
        job->setProgress(0.45);
    }

    if (uninstallationScriptAcquired)
        installationScripts.release();

    // Uninstall.bat may have deleted some files
    d.refresh();

    if (job->getErrorMessage().isEmpty()) {
        if (d.exists()) {
            job->setHint(QObject::tr("Deleting files"));
            Job* rjob = job->newSubJob(0.54);
            removeDirectory(rjob, d.absolutePath());
            if (!rjob->getErrorMessage().isEmpty())
                job->setErrorMessage(rjob->getErrorMessage());
            else {
                QString err = setPath("");
                if (!err.isEmpty())
                    job->setErrorMessage(err);
            }
            delete rjob;
        }
    }

    if (job->shouldProceed()) {
        QString err = DBRepository::getDefault()->updateStatus(this->package);
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        if (this->package == "com.googlecode.windows-package-manager.NpackdCL" ||
                this->package == "com.googlecode.windows-package-manager.NpackdCL64") {
            job->setHint("Updating NPACKD_CL");
            AbstractRepository::getDefault_()->updateNpackdCLEnvVar();
        }
        job->setProgress(1);
    }

    job->complete();
}

void PackageVersion::removeDirectory(Job* job, const QString& dir)
{
    QDir d(dir);

    WPMUtils::moveToRecycleBin(d.absolutePath());
    job->setProgress(0.3);

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        d.refresh();
        if (d.exists()) {
            Sleep(5000); // 5 Seconds
            WPMUtils::moveToRecycleBin(d.absolutePath());
        }
        job->setProgress(0.6);
    }

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        d.refresh();
        if (d.exists()) {
            Sleep(5000); // 5 Seconds
            QString oldName = d.dirName();
            if (!d.cdUp())
                job->setErrorMessage(QObject::tr("Cannot change directory to %1").arg(
                        d.absolutePath() + "\\.."));
            else {
                QString trash = ".NpackdTrash";
                if (!d.exists(trash)) {
                    if (!d.mkdir(trash))
                        job->setErrorMessage(QString(
                                QObject::tr("Cannot create directory %0\%1")).
                                arg(d.absolutePath()).arg(trash));
                }
                if (d.exists(trash)) {
                    QString nn = trash + "\\" + oldName + "_%1";
                    int i = 0;
                    while (true) {
                        QString newName = nn.arg(i);
                        if (!d.exists(newName)) {
                            if (!d.rename(oldName, newName)) {
                                job->setErrorMessage(QString(
                                        QObject::tr("Cannot rename %1 to %2 in %3")).
                                        arg(oldName).arg(newName).
                                        arg(d.absolutePath()));
                            }
                            break;
                        } else {
                            i++;
                        }
                    }
                }
            }
        }
        job->setProgress(1);
    }

    job->complete();
}

QString PackageVersion::planInstallation(QList<PackageVersion*>& installed,
        QList<InstallOperation*>& ops, QList<PackageVersion*>& avoid)
{
    QString res;

    avoid.append(this->clone());

    for (int i = 0; i < this->dependencies.count(); i++) {
        Dependency* d = this->dependencies.at(i);
        bool depok = false;
        for (int j = 0; j < installed.size(); j++) {
            PackageVersion* pv = installed.at(j);
            if ((pv->package != this->package || pv->version != this->version) &&
                    pv->package == d->package &&
                    d->test(pv->version)) {
                depok = true;
                break;
            }
        }
        if (!depok) {
            // we cannot just use Dependency->findBestMatchToInstall here as
            // it is possible that the highest match cannot be installed because
            // of unsatisfied dependencies. Example: the newest version depends
            // on Windows Vista, but the current operating system is XP.

            /* old code:
            QString err;
            QScopedPointer<PackageVersion> pv(d->findBestMatchToInstall(avoid,
                    &err));
            if (!err.isEmpty()) {
                res = QString(QObject::tr("Error searching for the best dependency match: %1")).
                           arg(err);
                break;
            }

            if (!pv) {
                res = QString(QObject::tr("Unsatisfied dependency: %1")).
                           arg(d->toString());
                break;
            } else {
                res = pv->planInstallation(installed, ops, avoid);
                if (!res.isEmpty())
                    break;
            }
            */

            QString err;
            QList<PackageVersion*> pvs = d->findAllMatchesToInstall(avoid, &err);
            if (!err.isEmpty()) {
                res = QString(QObject::tr("Error searching for the dependency matches: %1")).
                           arg(err);
                qDeleteAll(pvs);
                break;
            }
            if (pvs.count() == 0) {
                res = QString(QObject::tr("Unsatisfied dependency: %1")).
                           arg(d->toString());
                break;
            } else {
                bool found = false;
                for (int j = 0; j < pvs.count(); j++) {
                    PackageVersion* pv = pvs.at(j);
                    int installedCount = installed.count();
                    int opsCount = ops.count();
                    int avoidCount = avoid.count();

                    res = pv->planInstallation(installed, ops, avoid);
                    if (!res.isEmpty()) {
                        // rollback
                        while (installed.count() > installedCount) {
                            delete installed.takeLast();
                        }
                        while (ops.count() > opsCount) {
                            delete ops.takeLast();
                        }
                        while (avoid.count() > avoidCount) {
                            delete avoid.takeLast();
                        }
                    } else {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    res = QString(QObject::tr("Unsatisfied dependency: %1")).
                               arg(d->toString());
                }
            }
            qDeleteAll(pvs);
        }
    }

    if (res.isEmpty()) {
        InstallOperation* io = new InstallOperation();
        io->install = true;
        io->package = this->package;
        io->version = this->version;
        ops.append(io);
        installed.append(this->clone());
    }

    return res;
}

QString PackageVersion::planUninstallation(QList<PackageVersion*>& installed,
        QList<InstallOperation*>& ops)
{
    // qDebug() << "PackageVersion::planUninstallation()" << this->toString();
    QString res;

    if (!PackageVersion::contains(installed, this))
        return res;

    // this loop ensures that all the items in "installed" are processed
    // even if changes in the list were done in nested calls to
    // "planUninstallation"
    while (true) {
        int oldCount = installed.count();
        for (int i = 0; i < installed.count(); i++) {
            PackageVersion* pv = installed.at(i);
            if (pv->package != this->package || pv->version != this->version) {
                for (int j = 0; j < pv->dependencies.count(); j++) {
                    Dependency* d = pv->dependencies.at(j);
                    if (d->package == this->package && d->test(this->version)) {
                        int n = 0;
                        for (int k = 0; k < installed.count(); k++) {
                            PackageVersion* pv2 = installed.at(k);
                            if (d->package == pv2->package &&
                                    d->test(pv2->version)) {
                                n++;
                            }
                            if (n > 1)
                                break;
                        }
                        if (n <= 1) {
                            res = pv->planUninstallation(installed, ops);
                            if (!res.isEmpty())
                                break;
                        }
                    }
                }
                if (!res.isEmpty())
                    break;
            }
        }

        if (oldCount == installed.count() || !res.isEmpty())
            break;
    }

    if (res.isEmpty()) {
        InstallOperation* op = new InstallOperation();
        op->install = false;
        op->package = this->package;
        op->version = this->version;
        ops.append(op);
        installed.removeAt(PackageVersion::indexOf(installed, this));
    }

    return res;
}

QString PackageVersion::getFileExtension()
{
    if (this->download.isValid()) {
        QString fn = this->download.path();
        QStringList parts = fn.split('/');
        QString file = parts.at(parts.count() - 1);
        int index = file.lastIndexOf('.');
        if (index > 0)
            return file.right(file.length() - index);
        else
            return ".bin";
    } else {
        return ".bin";
    }
}

QString PackageVersion::downloadAndComputeSHA1(Job* job)
{
    if (!this->download.isValid()) {
        job->setErrorMessage(QObject::tr("No download URL"));
        job->complete();
        return "";
    }

    QString r;

    job->setHint(QObject::tr("Downloading"));
    QTemporaryFile* f = 0;
    Job* djob = job->newSubJob(0.95);
    f = Downloader::download(djob, this->download);
    if (!djob->getErrorMessage().isEmpty())
        job->setErrorMessage(QString(QObject::tr("Download failed: %1")).
                arg(djob->getErrorMessage()));
    delete djob;

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        job->setHint(QObject::tr("Computing SHA1"));
        r = WPMUtils::sha1(f->fileName());
        job->setProgress(1);
    }

    if (f)
        delete f;

    job->complete();

    return r;
}

QString PackageVersion::getPackageTitle(
        bool includeFullPackageName) const
{
    AbstractRepository* rep = AbstractRepository::getDefault_();

    QString pn;
    Package* package = rep->findPackage_(this->package);
    if (package)
        pn = package->title;
    else
        pn = this->package;
    delete package;

    if (includeFullPackageName)
        pn += " (" + this->package + ")";

    return pn;
}

bool PackageVersion::createShortcuts(const QString& dir, QString *errMsg)
{
    *errMsg = "";

    QString packageTitle = this->getPackageTitle();

    QDir d(dir);
    Package* p = AbstractRepository::getDefault_()->findPackage_(this->package);
    for (int i = 0; i < this->importantFiles.count(); i++) {
        QString ifile = this->importantFiles.at(i);
        QString ift = this->importantFilesTitles.at(i);

        QString path(ifile);
        path.prepend("\\");
        path.prepend(d.absolutePath());
        path.replace('/' , '\\');

        if (!d.exists(path)) {
            *errMsg = QString(QObject::tr("Shortcut target %1 does not exist")).
                    arg(path);
            break;
        }

        QString workingDir = path;
        int pos = workingDir.lastIndexOf('\\');
        workingDir.chop(workingDir.length() - pos);

        QString simple = ift;
        QString withVersion = simple + " " + this->version.getVersionString();
        if (packageTitle != ift) {
            QString suffix = " (" + packageTitle + ")";
            simple.append(suffix);
            withVersion.append(suffix);
        }

        simple = WPMUtils::makeValidFilename(simple, ' ') + ".lnk";
        withVersion = WPMUtils::makeValidFilename(withVersion, ' ');
        QString commonStartMenu = WPMUtils::getShellDir(CSIDL_COMMON_STARTMENU);
        simple = commonStartMenu + "\\" + simple;
        withVersion = commonStartMenu + "\\" + withVersion;

        QString from;
        if (QFileInfo(simple).exists())
            from = WPMUtils::findNonExistingFile(withVersion, ".lnk");
        else
            from = simple;

        // qDebug() << "createShortcuts " << ifile << " " << p << " " <<
        //         from;

        QString desc;
        if (p)
            desc = p->description;
        if (desc.isEmpty())
            desc = this->package;

        desc = WPMUtils::getFirstLine(desc);
        if (desc.length() > 258)
            desc = desc.left(255) + "...";

        QString r = WPMUtils::createLink(
                (WCHAR*) path.replace('/', '\\').utf16(),
                (WCHAR*) from.utf16(),
                (WCHAR*) desc.utf16(),
                (WCHAR*) workingDir.utf16());

        if (!r.isEmpty()) {
            *errMsg = QString(QObject::tr("Shortcut creation from %1 to %2 failed: %3")).
                    arg(from).arg(path).arg(r);
            break;
        }
    }
    delete p;

    return errMsg->isEmpty();
}

QString PackageVersion::getPreferredInstallationDirectory()
{
    QString name = WPMUtils::getInstallationDirectory() + "\\" +
            WPMUtils::makeValidFilename(this->getPackageTitle(), '_');
    if (!QFileInfo(name).exists())
        return name;
    else
        return WPMUtils::findNonExistingFile(name + "-" +
                this->version.getVersionString(), "");
}

void PackageVersion::install(Job* job, const QString& where)
{
    job->setHint(QObject::tr("Preparing"));

    if (installed()) {
        job->setProgress(1);
        job->complete();
        return;
    }

    if (!this->download.isValid()) {
        job->setErrorMessage(QObject::tr("No download URL"));
        job->complete();
        return;
    }

    // qDebug() << "install.2";
    QDir d(where);
    QString npackdDir = where + "\\.Npackd";

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        job->setHint(QObject::tr("Creating directory"));
        QString s = d.absolutePath();
        if (!d.mkpath(s)) {
            job->setErrorMessage(QString(QObject::tr("Cannot create directory: %0")).
                    arg(s));
        } else {
            job->setProgress(0.01);
        }
    }

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        job->setHint(QObject::tr("Creating .Npackd sub-directory"));
        QString s = npackdDir;
        if (!d.mkpath(s)) {
            job->setErrorMessage(QString(QObject::tr("Cannot create directory: %0")).
                    arg(s));
        } else {
            job->setProgress(0.02);
        }
    }

    bool httpConnectionAcquired = false;

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        job->setHint(QObject::tr("Waiting for a free HTTP connection"));

        time_t start = time(NULL);
        while (!job->isCancelled()) {
            httpConnectionAcquired = httpConnections.tryAcquire(1, 10000);
            if (httpConnectionAcquired) {
                job->setProgress(0.05);
                break;
            }

            time_t seconds = time(NULL) - start;
            job->setHint(QString(
                    QObject::tr("Waiting for a free HTTP connection (%1 minutes)")).
                    arg(seconds / 60));
        }
    }

    // qDebug() << "install.3";
    QFile* f = new QFile(npackdDir + "\\__NpackdPackageDownload");

    bool downloadOK = false;
    QString dsha1;

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        job->setHint(QObject::tr("Downloading & computing hash sum"));
        if (!f->open(QIODevice::ReadWrite)) {
            job->setErrorMessage(QString(QObject::tr("Cannot open the file: %0")).
                    arg(f->fileName()));
        } else {
            Job* djob = job->newSubJob(0.58);
            Downloader::download(djob, this->download, f,
                    this->sha1.isEmpty() ? 0 : &dsha1, this->hashSumType);
            downloadOK = !djob->isCancelled() &&
                    djob->getErrorMessage().isEmpty();
            f->close();
            delete djob;
        }
    }

    if (httpConnectionAcquired)
        httpConnections.release();

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        if (!downloadOK) {
            if (!f->open(QIODevice::ReadWrite)) {
                job->setErrorMessage(QString(QObject::tr("Cannot open the file: %0")).
                        arg(f->fileName()));
            } else {
                job->setHint(QObject::tr("Downloading & computing hash sum (2nd try)"));
                double rest = 0.63 - job->getProgress();
                Job* djob = job->newSubJob(rest);
                Downloader::download(djob, this->download, f,
                        this->sha1.isEmpty() ? 0 : &dsha1, this->hashSumType);
                if (!djob->getErrorMessage().isEmpty())
                    job->setErrorMessage(QString(QObject::tr("Error downloading %1: %2")).
                        arg(this->download.toString()).arg(
                        djob->getErrorMessage()));
                f->close();
                delete djob;
            }
        } else {
            job->setProgress(0.63);
        }
    }

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        if (!this->sha1.isEmpty()) {
            if (dsha1.toLower() != this->sha1.toLower()) {
                job->setErrorMessage(QString(
                        QObject::tr("Hash sum %1 found, but %2 was expected. The file has changed.")).arg(dsha1).
                        arg(this->sha1));
            } else {
                job->setProgress(0.64);
            }
        } else {
            job->setProgress(0.64);
        }
    }

    if (job->shouldProceed(QObject::tr("Checking for viruses"))) {
        bool safe = isFileSafe(f->fileName(), this->download.toString());
        if (!safe) {
            job->setErrorMessage(QObject::tr("Antivirus check failed. The file is not safe."));
        }
        job->setProgress(0.69);
    }

    /* this should actually be used by MS Office. MS Essentials and
     * Avira Free Antivirus do not use it
    if (job->shouldProceed(QObject::tr("Checking for viruses 2"))) {
        QString err;
        bool safe = isFileSafeOfficeAntiVirus(f->fileName(), &err);
        if (!err.isEmpty()) {
            job->setErrorMessage(QObject::tr("Antivirus check 2 failed: %1").
                    arg(err));
        } else if (!safe) {
            job->setErrorMessage(QObject::tr("Antivirus check 2 failed. The file is not safe."));
        }
        job->setProgress(0.69);
    }
    */

    QString binary;
    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        if (this->type == 0) {
            job->setHint(QObject::tr("Extracting files"));
            Job* djob = job->newSubJob(0.06);
            unzip(djob, f->fileName(), d.absolutePath() + "\\");
            if (!djob->getErrorMessage().isEmpty())
                job->setErrorMessage(QString(
                        QObject::tr("Error unzipping file into directory %0: %1")).
                        arg(d.absolutePath()).
                        arg(djob->getErrorMessage()));
            else if (!job->isCancelled())
                job->setProgress(0.75);
            delete djob;
        } else {
            job->setHint(QObject::tr("Renaming the downloaded file"));
            QString t = d.absolutePath();
            t.append("\\");
            QString fn = this->download.path();
            QStringList parts = fn.split('/');
            t.append(parts.at(parts.count() - 1));
            t.replace('/', '\\');

            binary = t;

            if (!QFile::rename(f->fileName(), t)) {
                job->setErrorMessage(QString(QObject::tr("Cannot rename %0 to %1")).
                        arg(f->fileName()).arg(t));
            } else {
                job->setProgress(0.75);
            }
        }
    }

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        QString errMsg = this->saveFiles(d);
        if (!errMsg.isEmpty()) {
            job->setErrorMessage(errMsg);
        } else {
            job->setProgress(0.85);
        }
    }

    QString installationScript;
    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        installationScript = ".Npackd\\Install.bat";
        if (!QFile::exists(d.absolutePath() +
                "\\" + installationScript)) {
            installationScript = ".WPM\\Install.bat";
            if (!QFile::exists(d.absolutePath() +
                    "\\" + installationScript)) {
                installationScript = "";
            }
        }
    }

    bool installationScriptAcquired = false;

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        if (!installationScript.isEmpty()) {
            job->setHint(QObject::tr("Waiting while other (un)installation scripts are running"));

            time_t start = time(NULL);
            while (!job->isCancelled()) {
                installationScriptAcquired = installationScripts.
                        tryAcquire(1, 10000);
                if (installationScriptAcquired) {
                    job->setProgress(0.86);
                    break;
                }

                time_t seconds = time(NULL) - start;
                job->setHint(QString(
                        QObject::tr("Waiting while other (un)installation scripts are running (%1 minutes)")).
                        arg(seconds / 60));
            }
        } else {
            job->setProgress(0.86);
        }
    }

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        if (!installationScript.isEmpty()) {
            job->setHint(QObject::tr("Running the installation script (this may take some time)"));
            Job* exec = job->newSubJob(0.09);
            if (!d.exists(".Npackd"))
                d.mkdir(".Npackd");

            QStringList env;
            env.append("NPACKD_PACKAGE_BINARY");
            env.append(binary);
            QString err = addBasicVars(&env);
            if (!err.isEmpty())
                job->setErrorMessage(err);

            addDependencyVars(&env);

            this->executeFile2(exec, d.absolutePath(), installationScript,
                    ".Npackd\\Install.log", "NpackdInstallXXXXXX.log", env);
            if (!exec->getErrorMessage().isEmpty())
                job->setErrorMessage(exec->getErrorMessage());
            else {
                QString path = d.absolutePath();
                path.replace('/', '\\');
                QString err = setPath(path);
                if (!err.isEmpty()) {
                    job->setErrorMessage(err);
                }
            }
            delete exec;
        } else {
            QString path = d.absolutePath();
            path.replace('/', '\\');
            QString err = setPath(path);
            if (!err.isEmpty())
                job->setErrorMessage(err);
        }

        if (this->package == "com.googlecode.windows-package-manager.NpackdCL" ||
                this->package == "com.googlecode.windows-package-manager.NpackdCL64") {
            job->setHint("Updating NPACKD_CL");
            AbstractRepository::getDefault_()->updateNpackdCLEnvVar();
        }

        job->setProgress(0.95);
    }

    if (installationScriptAcquired)
        installationScripts.release();

    if (job->shouldProceed()) {
        QString err = DBRepository::getDefault()->updateStatus(this->package);
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        QString err;
        this->createShortcuts(d.absolutePath(), &err);
        if (err.isEmpty())
            job->setProgress(1);
        else
            job->setErrorMessage(err);
    }

    if (!job->getErrorMessage().isEmpty() || job->isCancelled()) {
        job->setHint(QString(
                QObject::tr("Deleting start menu, desktop and quick launch shortcuts")));
        Job* sub = new Job();
        deleteShortcuts(d.absolutePath(), sub, true, true, true);
        delete sub;

        job->setHint(QString(QObject::tr("Deleting files")));
        Job* rjob = new Job();
        removeDirectory(rjob, d.absolutePath());
        delete rjob;
    }

    if (f && f->exists())
        f->remove();

    delete f;

    job->complete();
}

QString PackageVersion::addBasicVars(QStringList* env)
{
    QString err;
    env->append("NPACKD_PACKAGE_NAME");
    env->append(this->package);
    env->append("NPACKD_PACKAGE_VERSION");
    env->append(this->version.getVersionString());
    env->append("NPACKD_CL");
    env->append(AbstractRepository::getDefault_()->
            computeNpackdCLEnvVar_(&err));

    return err;
}

void PackageVersion::addDependencyVars(QStringList* vars)
{
    for (int i = 0; i < this->dependencies.count(); i++) {
        Dependency* d = this->dependencies.at(i);
        if (!d->var.isEmpty()) {
            vars->append(d->var);
            InstalledPackageVersion* ipv = d->findHighestInstalledMatch();
            if (ipv) {
                vars->append(ipv->getDirectory());
                delete ipv;
            } else {
                // this could happen if a package was un-installed manually
                // without Npackd or the repository has changed after this
                // package was installed
                vars->append("");
            }
        }
    }
}

void PackageVersion::unzip(Job* job, QString zipfile, QString outputdir)
{
    job->setHint(QObject::tr("Opening ZIP file"));
    QuaZip zip(zipfile);
    if (!zip.open(QuaZip::mdUnzip)) {
        job->setErrorMessage(QString(QObject::tr("Cannot open the ZIP file %1: %2")).
                       arg(zipfile).arg(zip.getZipError()));
    } else {
        job->setProgress(0.01);
    }

    if (!job->isCancelled() && job->getErrorMessage().isEmpty()) {
        job->setHint(QObject::tr("Extracting"));
        QuaZipFile file(&zip);
        int n = zip.getEntriesCount();
        int blockSize = 1024 * 1024;
        char* block = new char[blockSize];
        int i = 0;
        for (bool more = zip.goToFirstFile(); more; more = zip.goToNextFile()) {
            QString name = zip.getCurrentFileName();
            if (!file.open(QIODevice::ReadOnly)) {
                job->setErrorMessage(QString(
                        QObject::tr("Error unzipping the file %1: Error %2 in %3")).
                        arg(zipfile).arg(file.getZipError()).
                        arg(name));
                break;
            }
            name.prepend(outputdir);
            QFile meminfo(name);
            QFileInfo infofile(meminfo);
            QDir dira(infofile.absolutePath());
            if (dira.mkpath(infofile.absolutePath())) {
                if (meminfo.open(QIODevice::ReadWrite)) {
                    while (true) {
                        qint64 read = file.read(block, blockSize);
                        if (read <= 0)
                            break;
                        meminfo.write(block, read);
                    }
                    meminfo.close();
                }
            } else {
                job->setErrorMessage(QString(QObject::tr("Cannot create directory %1")).arg(
                        infofile.absolutePath()));
                file.close();
            }
            file.close(); // do not forget to close!
            i++;
            job->setProgress(0.01 + 0.99 * i / n);
            if (i % 100 == 0)
                job->setHint(QString(QObject::tr("%L1 files")).arg(i));

            if (job->isCancelled() || !job->getErrorMessage().isEmpty())
                break;
        }
        zip.close();

        delete[] block;
    }

    job->complete();
}

QString PackageVersion::saveFiles(const QDir& d)
{
    QString res;
    for (int i = 0; i < this->files.count(); i++) {
        PackageVersionFile* f = this->files.at(i);
        QString fullPath = d.absolutePath() + "\\" + f->path;
        QString fullDir = WPMUtils::parentDirectory(fullPath);
        if (d.mkpath(fullDir)) {
            QFile file(fullPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QTextStream stream(&file);
                stream.setCodec("UTF-8");
                stream << f->content;
                file.close();
            } else {
                res = QString(QObject::tr("Could not create file %1")).arg(
                        fullPath);
                break;
            }
        } else {
            res = QString(QObject::tr("Could not create directory %1")).arg(
                    fullDir);
            break;
        }
    }
    return res;
}

QString PackageVersion::getStatus() const
{
    QString status;
    bool installed = this->installed();
    AbstractRepository* r = AbstractRepository::getDefault_();
    QString err;
    PackageVersion* newest = r->findNewestInstallablePackageVersion_(
            this->package, &err);
    if (installed) {
        status = QObject::tr("installed");
    }
    if (installed && newest != 0 && version.compare(newest->version) < 0) {
        if (!newest->installed())
            status += ", " + QObject::tr("updateable");
        else
            status += ", " + QObject::tr("obsolete");
    }
    if (!err.isEmpty())
        status += ", " + err;

    if (isLocked()) {
        if (!status.isEmpty())
            status = ", " + status;
        status = QObject::tr("locked") + status;
    }

    delete newest;

    return status;
}

void PackageVersion::executeFile2(Job* job, const QString& where,
        const QString& path,
        const QString& outputFile,
        const QString& tempFileTemplate,
        const QStringList& env)
{
    QByteArray output = this->executeFile(job, where,
            path, outputFile, env);
    output.insert(0, WPMUtils::UCS2LE_BOM);
    if (!job->getErrorMessage().isEmpty()) {
        QTemporaryFile of(QDir::tempPath() +
                          "\\" + tempFileTemplate);
        of.setAutoRemove(false);
        if (of.open()) {
            of.write(output);
        }

        job->setErrorMessage(QString(
                QObject::tr("%1. Full output was saved in %2")).arg(
                job->getErrorMessage()).
                arg(WPMUtils::normalizePath(of.fileName())));
    }
}

QByteArray PackageVersion::executeFile(Job* job, const QString& where,
        const QString& path,
        const QString& outputFile, const QStringList& env)
{
    QByteArray ret;

    QDir d(where);
    QProcess p(0);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.setWorkingDirectory(d.absolutePath());

    QString exe = WPMUtils::findCmdExe();
    QString file = d.absolutePath() + "\\" + path;
    file.replace('/', '\\');
    QStringList args;
    /*args.append("/U");
    args.append("/E:ON");
    args.append("/V:OFF");
    args.append("/C");
    args.append(file);
    */
    p.setNativeArguments("/U /E:ON /V:OFF /C \"\"" + file + "\"\"");
    // qDebug() << p.nativeArguments();
    QProcessEnvironment pe = QProcessEnvironment::systemEnvironment();
    for (int i = 0; i < env.count(); i += 2) {
        pe.insert(env.at(i), env.at(i + 1));
    }
    p.setProcessEnvironment(pe);
    p.start(exe, args);

    time_t start = time(NULL);
    while (true) {
        if (job->isCancelled()) {
            if (p.state() == QProcess::Running) {
                p.terminate();
                if (p.waitForFinished(10000))
                    break;
                p.kill();
            }
        }
        if (p.waitForFinished(5000) || p.state() == QProcess::NotRunning) {
            job->setProgress(1);
            if (p.exitCode() != 0) {
                job->setErrorMessage(
                        QString(QObject::tr("Process %1 exited with the code %2")).
                        arg(
                        exe).arg(p.exitCode()));
            }
            QFile f(d.absolutePath() + "\\" + outputFile);
            if (f.open(QIODevice::WriteOnly)) {
                f.write("\xff\xfe");
                ret = p.readAll();
                f.write(ret);
                f.close();
            }
            break;
        }
        time_t seconds = time(NULL) - start;
        double percents = ((double) seconds) / 300; // 5 Minutes
        if (percents > 0.9)
            percents = 0.9;
        job->setProgress(percents);
        job->setHint(QString(QObject::tr("%1 minutes")).
                arg(seconds / 60));
    }
    job->complete();

    return ret;
}

bool PackageVersion::isInWindowsDir() const
{
    QString dir = WPMUtils::getWindowsDir();
    return this->installed() && (WPMUtils::pathEquals(getPath(), dir) ||
            WPMUtils::isUnder(getPath(), dir));
}

PackageVersion* PackageVersion::clone() const
{
    PackageVersion* r = new PackageVersion(this->package, this->version);
    r->importantFiles = this->importantFiles;
    r->importantFilesTitles = this->importantFilesTitles;
    for (int i = 0; i < this->files.count(); i++) {
        PackageVersionFile* f = this->files.at(i);
        r->files.append(f->clone());
    }
    for (int i = 0; i < this->detectFiles.count(); i++) {
        DetectFile* f = this->detectFiles.at(i);
        r->detectFiles.append(f->clone());
    }
    for (int i = 0; i < dependencies.count(); i++) {
        Dependency* d = this->dependencies.at(i);
        r->dependencies.append(d->clone());
    }

    r->type = this->type;
    r->sha1 = this->sha1;
    r->hashSumType = this->hashSumType;
    r->download = this->download;
    r->msiGUID = this->msiGUID;

    return r;
}

PackageVersionFile* PackageVersion::createPackageVersionFile(QDomElement* e,
        QString* err)
{
    *err = "";

    QString path = e->attribute("path");
    QString content = e->firstChild().nodeValue();
    PackageVersionFile* a = new PackageVersionFile(path, content);

    return a;
}

DetectFile* PackageVersion::createDetectFile(QDomElement* e, QString* err)
{
    *err = "";

    DetectFile* a = new DetectFile();
    a->path = XMLUtils::getTagContent(*e, "path").trimmed();
    a->path.replace('/', '\\');
    if (a->path.isEmpty()) {
        err->append(QObject::tr("Empty tag <path> under <detect-file>"));
    }

    if (err->isEmpty()) {
        a->sha1 = XMLUtils::getTagContent(*e, "sha1").trimmed().toLower();
        *err = WPMUtils::validateSHA1(a->sha1);
        if (!err->isEmpty()) {
            err->prepend(QObject::tr("Wrong SHA1 in <detect-file>: "));
        }
    }

    if (err->isEmpty())
        return a;
    else {
        delete a;
        return 0;
    }
}

Dependency* PackageVersion::createDependency(QDomElement* e)
{
    // qDebug() << "Repository::createDependency";

    QString package = e->attribute("package").trimmed();

    Dependency* d = new Dependency();
    d->package = package;

    d->var = XMLUtils::getTagContent(*e, "variable");

    if (d->setVersions(e->attribute("versions")))
        return d;
    else {
        delete d;
        return 0;
    }
}

PackageVersion* PackageVersion::parse(QDomElement* e, QString* err,
        bool validate)
{
    *err = "";

    // qDebug() << "Repository::createPackageVersion.1" << e->attribute("package");

    QString packageName = e->attribute("package").trimmed();
    if (validate) {
        *err = WPMUtils::validateFullPackageName(packageName);
        if (!err->isEmpty()) {
            err->prepend(QObject::tr("Error in the attribute 'package' in <version>: "));
        }
    }

    PackageVersion* a = new PackageVersion(packageName);

    if (err->isEmpty()) {
        QString url = XMLUtils::getTagContent(*e, "url").trimmed();
        if (!url.isEmpty()) {
            a->download.setUrl(url);
            if (validate) {
                QUrl d = a->download;
                if (!d.isValid() || d.isRelative() ||
                        (d.scheme() != "http" && d.scheme() != "https")) {
                    err->append(QString(QObject::tr("Not a valid download URL for %1: %2")).
                            arg(a->package).arg(url));
                }
            }
        }
    }

    if (err->isEmpty()) {
        QString name = e->attribute("name", "1.0").trimmed();
        if (a->version.setVersion(name)) {
            a->version.normalize();
        } else {
            err->append(QString(QObject::tr("Not a valid version for %1: %2")).
                    arg(a->package).arg(name));
        }
    }

    if (err->isEmpty()) {
        a->sha1 = XMLUtils::getTagContent(*e, "sha1").trimmed().toLower();
        a->hashSumType = QCryptographicHash::Sha1;
        if (validate) {
            if (!a->sha1.isEmpty()) {
                *err = WPMUtils::validateSHA1(a->sha1);
                if (!err->isEmpty()) {
                    err->prepend(QString(QObject::tr("Invalid SHA1 for %1: ")).
                            arg(a->toString()));
                }
            }
        }
    }

    QString hashSum;
    QString hashSumType;
    if (err->isEmpty()) {
        hashSum = XMLUtils::getTagContent(*e, "hash-sum").trimmed().toLower();
        hashSumType = e->attribute("type", "SHA-256").trimmed();
        if (!a->sha1.isEmpty() && !hashSum.isEmpty()) {
            *err = QObject::tr(
                    "SHA-1 and SHA-256 cannot be defined both for the same package version %1").
                    arg(a->toString());
        }
    }

    QCryptographicHash::Algorithm alg = QCryptographicHash::Sha1;

    if (err->isEmpty() && !hashSum.isEmpty()) {
        if (hashSumType == "SHA-1")
            alg = QCryptographicHash::Sha1;
        else if (hashSumType == "SHA-256" || hashSumType.isEmpty())
            alg = QCryptographicHash::Sha256;
        else
            *err = QObject::tr(
                    "Unknown hash sum type %1 for %2").arg(hashSumType).
                    arg(a->toString());
    }

    if (err->isEmpty() && !hashSum.isEmpty()) {
        a->sha1 = hashSum;
        a->hashSumType = alg;
        if (validate) {
            if (!a->sha1.isEmpty()) {
                *err = WPMUtils::validateSHA256(a->sha1);
                if (!err->isEmpty()) {
                    err->prepend(QString(QObject::tr("Invalid SHA-256 for %1: ")).
                            arg(a->toString()));
                }
            }
        }
    }

    if (err->isEmpty()) {
        QString type = e->attribute("type", "zip").trimmed();
        if (type == "one-file")
            a->type = 1;
        else if (type == "" || type == "zip")
            a->type = 0;
        else {
            err->append(QString(
                    QObject::tr("Wrong value for the attribute 'type' for %1: %3")).
                    arg(a->toString()).arg(type));
        }
    }

    if (err->isEmpty()) {
        QDomNodeList ifiles = e->elementsByTagName("important-file");
        for (int i = 0; i < ifiles.count(); i++) {
            QDomElement e = ifiles.at(i).toElement();
            QString p = e.attribute("path").trimmed();
            if (p.isEmpty())
                p = e.attribute("name").trimmed();

            if (p.isEmpty()) {
                err->append(QString(
                        QObject::tr("Empty 'path' attribute value for <important-file> for %1")).
                        arg(a->toString()));
                break;
            }

            if (a->importantFiles.contains(p)) {
                err->append(QString(
                        QObject::tr("More than one <important-file> with the same 'path' attribute %1 for %2")).
                        arg(p).arg(a->toString()));
                break;
            }

            a->importantFiles.append(p);

            QString title = e.attribute("title").trimmed();
            if (title.isEmpty()) {
                err->append(QString(
                        QObject::tr("Empty 'title' attribute value for <important-file> for %1")).
                        arg(a->toString()));
                break;
            }

            a->importantFilesTitles.append(title);
        }
    }

    if (err->isEmpty()) {
        QDomNodeList files = e->elementsByTagName("file");
        for (int i = 0; i < files.count(); i++) {
            QDomElement e = files.at(i).toElement();
            PackageVersionFile* pvf = createPackageVersionFile(&e, err);
            if (pvf) {
                a->files.append(pvf);
            } else {
                break;
            }
        }
    }

    if (err->isEmpty()) {
        for (int i = 0; i < a->files.count() - 1; i++) {
            PackageVersionFile* fi = a->files.at(i);
            for (int j = i + 1; j < a->files.count(); j++) {
                PackageVersionFile* fj = a->files.at(j);
                if (fi->path == fj->path) {
                    err->append(QString(
                        QObject::tr("Duplicate <file> entry for %1 in %2")).
                            arg(fi->path).arg(a->toString()));
                    goto out;
                }
            }
        }
    out:;
    }

    if (err->isEmpty()) {
        QDomNodeList detectFiles = e->elementsByTagName("detect-file");
        for (int i = 0; i < detectFiles.count(); i++) {
            QDomElement e = detectFiles.at(i).toElement();
            DetectFile* df = createDetectFile(&e, err);
            if (df) {
                a->detectFiles.append(df);
            } else {
                err->prepend(QString(
                        QObject::tr("Invalid <detect-file> for %1: ")).
                        arg(a->toString()));
                break;
            }
        }
    }

    if (err->isEmpty()) {
        if (validate) {
            for (int i = 0; i < a->detectFiles.count() - 1; i++) {
                DetectFile* fi = a->detectFiles.at(i);
                for (int j = i + 1; j < a->detectFiles.count(); j++) {
                    DetectFile* fj = a->detectFiles.at(j);
                    if (fi->path == fj->path) {
                        err->append(QString(
                                QObject::tr("Duplicate <detect-file> entry for %1 in %2")).
                                arg(fi->path).arg(a->toString()));
                        goto out2;
                    }
                }
            }
        }
    out2:;
    }

    if (err->isEmpty()) {
        QDomNodeList deps = e->elementsByTagName("dependency");
        for (int i = 0; i < deps.count(); i++) {
            QDomElement e = deps.at(i).toElement();
            Dependency* d = createDependency(&e);
            if (d)
                a->dependencies.append(d);
        }
    }

    if (err->isEmpty()) {
        if (validate) {
            for (int i = 0; i < a->dependencies.count() - 1; i++) {
                Dependency* fi = a->dependencies.at(i);
                for (int j = i + 1; j < a->dependencies.count(); j++) {
                    Dependency* fj = a->dependencies.at(j);
                    if (fi->autoFulfilledIf(*fj) ||
                            fj->autoFulfilledIf(*fi)) {
                        err->append(QString(
                                QObject::tr("Duplicate <dependency> for %1 in %2")).
                                arg(fi->package).arg(a->toString()));
                        goto out3;
                    }
                }
            }
        }
    out3:;
    }

    if (err->isEmpty()) {
        a->msiGUID = XMLUtils::getTagContent(*e, "detect-msi").trimmed().
                toLower();
        if (validate) {
            if (!a->msiGUID.isEmpty()) {
                *err = WPMUtils::validateGUID(a->msiGUID);
                if (!err->isEmpty())
                    *err = QString(
                            QObject::tr("Wrong MSI GUID for %1: %2")).
                            arg(a->toString()).arg(a->msiGUID);
            }
        }
    }

    if (err->isEmpty())
        return a;
    else {
        delete a;
        return 0;
    }
}

bool PackageVersion::contains(const QList<PackageVersion *> &list,
        PackageVersion *pv)
{
    return indexOf(list, pv) >= 0;
}

void PackageVersion::toXML(QDomElement* version) const {
    QDomDocument doc = version->ownerDocument();
    version->setAttribute("name", this->version.getVersionString());
    version->setAttribute("package", this->package);
    if (this->type == 1)
        version->setAttribute("type", "one-file");
    for (int i = 0; i < this->importantFiles.count(); i++) {
        QDomElement importantFile = doc.createElement("important-file");
        version->appendChild(importantFile);
        importantFile.setAttribute("path", this->importantFiles.at(i));
        importantFile.setAttribute("title", this->importantFilesTitles.at(i));
    }
    for (int i = 0; i < this->files.count(); i++) {
        QDomElement file = doc.createElement("file");
        version->appendChild(file);
        file.setAttribute("path", this->files.at(i)->path);
        file.appendChild(doc.createTextNode(files.at(i)->content));
    }
    if (this->download.isValid()) {
        XMLUtils::addTextTag(*version, "url", this->download.toString());
    }
    if (!this->sha1.isEmpty()) {
        if (this->hashSumType == QCryptographicHash::Sha1)
            XMLUtils::addTextTag(*version, "sha1", this->sha1);
        else
            XMLUtils::addTextTag(*version, "hash-sum", this->sha1);
    }
    for (int i = 0; i < this->dependencies.count(); i++) {
        Dependency* d = this->dependencies.at(i);
        QDomElement dependency = doc.createElement("dependency");
        version->appendChild(dependency);
        dependency.setAttribute("package", d->package);
        dependency.setAttribute("versions", d->versionsToString());
        if (!d->var.isEmpty())
            XMLUtils::addTextTag(dependency, "variable", d->var);
    }
    if (!this->msiGUID.isEmpty()) {
        XMLUtils::addTextTag(*version, "detect-msi", this->msiGUID);
    }
    for (int i = 0; i < detectFiles.count(); i++) {
        DetectFile* df = this->detectFiles.at(i);
        QDomElement detectFile = doc.createElement("detect-file");
        version->appendChild(detectFile);
        XMLUtils::addTextTag(detectFile, "path", df->path);
        XMLUtils::addTextTag(detectFile, "sha1", df->sha1);
    }
}

void PackageVersion::toXML(QXmlStreamWriter *w) const
{
    w->writeStartElement("version");
    w->writeAttribute("name", this->version.getVersionString());
    w->writeAttribute("package", this->package);
    if (this->type == 1)
        w->writeAttribute("type", "one-file");
    for (int i = 0; i < this->importantFiles.count(); i++) {
        w->writeStartElement("important-file");
        w->writeAttribute("path", this->importantFiles.at(i));
        w->writeAttribute("title", this->importantFilesTitles.at(i));
        w->writeEndElement();
    }
    for (int i = 0; i < this->files.count(); i++) {
        w->writeStartElement("file");
        w->writeAttribute("path", this->files.at(i)->path);
        w->writeCharacters(files.at(i)->content);
        w->writeEndElement();
    }
    if (this->download.isValid()) {
        w->writeTextElement("url", this->download.toString());
    }
    if (!this->sha1.isEmpty()) {
        if (this->hashSumType == QCryptographicHash::Sha1)
            w->writeTextElement("sha1", this->sha1);
        else
            w->writeTextElement("hash-sum", this->sha1);
    }
    for (int i = 0; i < this->dependencies.count(); i++) {
        Dependency* d = this->dependencies.at(i);
        w->writeStartElement("dependency");
        w->writeAttribute("package", d->package);
        w->writeAttribute("versions", d->versionsToString());
        if (!d->var.isEmpty())
            w->writeTextElement("variable", d->var);
        w->writeEndElement();
    }
    if (!this->msiGUID.isEmpty()) {
        w->writeTextElement("detect-msi", this->msiGUID);
    }
    for (int i = 0; i < detectFiles.count(); i++) {
        DetectFile* df = this->detectFiles.at(i);
        w->writeStartElement("detect-file");
        w->writeTextElement("path", df->path);
        w->writeTextElement("sha1", df->sha1);
        w->writeEndElement();
    }
    w->writeEndElement();
}

QString PackageVersion::serialize() const
{
    QDomDocument doc;
    QDomElement version = doc.createElement("version");
    doc.appendChild(version);
    toXML(&version);

    return doc.toString(4);
}

PackageVersionFile* PackageVersion::findFile(const QString& path) const
{
    PackageVersionFile* r = 0;
    QString lowerPath = path.toLower();
    for (int i = 0; i < this->files.count(); i++) {
        PackageVersionFile* pvf = this->files.at(i);
        if (pvf->path.toLower() == lowerPath) {
            r = pvf;
            break;
        }
    }
    return r;
}

void PackageVersion::stop(Job* job, int programCloseType)
{
    bool me = false;
    QString myPackage = InstalledPackages::getDefault()->packageName;
    Version myVersion(NPACKD_VERSION);
    if (this->package == myPackage && this->version == myVersion) {
        if (WPMUtils::pathEquals(WPMUtils::getExeDir(), getPath())) {
            me = true;
        }
    }

    QDir d(getPath());
    if (me) {
        job->setProgress(0.5);
    } else if (QFile::exists(d.absolutePath() + "\\.Npackd\\Stop.bat")) {
        Job* exec = job->newSubJob(0.5);
        if (!d.exists(".Npackd"))
            d.mkdir(".Npackd");

        QStringList env;
        QString err = addBasicVars(&env);
        if (!err.isEmpty())
            job->setErrorMessage(err);

        addDependencyVars(&env);

        this->executeFile2(exec, d.absolutePath(), ".Npackd\\Stop.bat",
                ".Npackd\\Stop.log", "NpackdStopXXXXXX.log", env);
        if (!exec->getErrorMessage().isEmpty()) {
            job->setErrorMessage(exec->getErrorMessage());
        } else {
            QString path = d.absolutePath();
            path.replace('/', '\\');
            QString err = setPath(path);
            if (!err.isEmpty()) {
                job->setErrorMessage(err);
            }
        }
        delete exec;

        if (this->isDirectoryLocked())
            WPMUtils::closeProcessesThatUseDirectory(getPath(),
                    programCloseType);
    } else {
        job->setProgress(0.5);

        if (this->isDirectoryLocked())
            WPMUtils::closeProcessesThatUseDirectory(getPath(),
                    programCloseType);
    }

    if (job->getErrorMessage().isEmpty())
        job->setProgress(1);

    job->complete();
}
