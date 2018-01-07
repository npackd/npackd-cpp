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
#include <QIODevice>
#include <QMutex>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QBuffer>

#include <zlib.h>

//#include "msoav2.h"
#include "quazip.h"
#include "quazipfile.h"

#include "packageversion.h"
#include "job.h"
#include "downloader.h"
#include "wpmutils.h"
#include "repository.h"
#include "version.h"
#include "windowsregistry.h"
#include "installedpackages.h"
#include "installedpackageversion.h"
#include "dbrepository.h"
#include "repositoryxmlhandler.h"

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
        // qCDebug(npackd) << "CoCreateInstance succeeded";
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

    if (err.isEmpty() && pExecute) {
        hr = pExecute->SetLocalPath((LPCWSTR) filename.utf16());
        if (!SUCCEEDED(hr)) {
            WPMUtils::formatMessage(hr, &err);
            // err = "2: " + err;
        }
    }

    if (err.isEmpty() && pExecute) {
        pExecute->SetSource((LPCWSTR) url.utf16());
        if (!SUCCEEDED(hr)) {
            WPMUtils::formatMessage(hr, &err);
            // err = "3: " + err;
        }
    }

    if (err.isEmpty() && pExecute) {
        hr = pExecute->Save();
        if (hr != S_OK)
            res = false;
    }

    if (pExecute)
        pExecute->Release();

    // qCDebug(npackd) << err;

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
            qCDebug(npackd) << "av2";
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

PackageVersion::PackageVersion(const QString &package, const Version &version):
version(version)
{
    this->package = package;
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

QString PackageVersion::getPath() const
{
    return InstalledPackages::getDefault()->
            getPath(this->package, this->version);
}

QString PackageVersion::setPath(const QString& path)
{
    //qCDebug(npackd) << "PackageVersion::setPath " << path;
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
        QString newName = QString("%1-%2").arg(d.absolutePath()).arg(
                now.toTime_t());

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

void PackageVersion::deleteShortcutsRunnable(const QString& dir, Job* job,
        bool menu, bool desktop, bool quickLaunch)
{
    QThread::currentThread()->setPriority(QThread::IdlePriority);
    bool b = SetThreadPriority(GetCurrentThread(),
            THREAD_MODE_BACKGROUND_BEGIN);

    CoInitialize(0);
    deleteShortcuts(dir, job, menu, desktop, quickLaunch);
    CoUninitialize();

    if (b)
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
}

void PackageVersion::deleteShortcuts(const QString& dir, Job* job,
        bool menu, bool desktop, bool quickLaunch)
{
    if (menu) {
        Job* sub = job->newSubJob(0.33, QObject::tr("Start menu"));
        QDir d(WPMUtils::getShellDir(CSIDL_STARTMENU));
        WPMUtils::deleteShortcuts(dir, d);

        if (WPMUtils::adminMode)
		{
			QDir d2(WPMUtils::getShellDir(CSIDL_COMMON_STARTMENU));
			WPMUtils::deleteShortcuts(dir, d2);
			sub->completeWithProgress();
		}
    }

    if (desktop) {
        Job* sub = job->newSubJob(0.33, QObject::tr("Desktop"));
        QDir d3(WPMUtils::getShellDir(CSIDL_DESKTOP));
        WPMUtils::deleteShortcuts(dir, d3);

        if (WPMUtils::adminMode)
		{
			QDir d4(WPMUtils::getShellDir(CSIDL_COMMON_DESKTOPDIRECTORY));
			WPMUtils::deleteShortcuts(dir, d4);
			sub->completeWithProgress();
		}
    }

    if (quickLaunch) {
        Job* sub = job->newSubJob(0.33, QObject::tr("Quick launch bar"));
        const char* A = "\\Microsoft\\Internet Explorer\\Quick Launch";
        QDir d3(WPMUtils::getShellDir(CSIDL_APPDATA) + A);
        WPMUtils::deleteShortcuts(dir, d3);

        if (WPMUtils::adminMode)
		{
			QDir d4(WPMUtils::getShellDir(CSIDL_COMMON_APPDATA) + A);
			WPMUtils::deleteShortcuts(dir, d4);
			sub->completeWithProgress();
		}
    }

    job->setProgress(1);
    job->complete();
}

void PackageVersion::uninstall(Job* job, bool printScriptOutput,
        int programCloseType)
{
    if (!installed()) {
        job->setProgress(1);
        job->complete();
        return;
    }

    QString initialTitle = job->getTitle();

    QString where = getPath();

    QDir d(getPath());

    QFuture<void> deleteShortcutsFuture;
    if (job->getErrorMessage().isEmpty()) {
        Job* deleteShortcutsJob = job->newSubJob(0,
                QObject::tr("Deleting shortcuts"), false);
        deleteShortcutsFuture = QtConcurrent::run(this,
                &PackageVersion::deleteShortcutsRunnable,
                d.absolutePath(), deleteShortcutsJob, true, false, false);
    }

    QString uninstallationScript;
    if (job->shouldProceed()) {
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

    if (job->shouldProceed()) {
        if (!uninstallationScript.isEmpty()) {
            job->setTitle(initialTitle + " / " +
                    QObject::tr("Waiting while other (un)installation scripts are running"));

            time_t start = time(NULL);
            while (!job->isCancelled()) {
                uninstallationScriptAcquired = installationScripts.
                        tryAcquire(1, 10000);
                if (uninstallationScriptAcquired) {
                    job->setProgress(0.25);
                    break;
                }

                time_t seconds = time(NULL) - start;
                job->setTitle(initialTitle + " / " + QString(
                        QObject::tr("Waiting while other (un)installation scripts are running (%1 minutes)")).
                        arg(seconds / 60));
            }
        } else {
            job->setProgress(0.25);
        }
    }
    job->setTitle(initialTitle);

    if (job->shouldProceed()) {
        if (!uninstallationScript.isEmpty()) {
            if (!d.exists(".Npackd"))
                d.mkdir(".Npackd");
            Job* sub = job->newSubJob(0.20,
                    QObject::tr("Running the uninstallation script (this may take some time)"),
                    true, true);

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
                qCDebug(npackd) << "file handle is OK";
            }
            */

            this->executeFile2(sub, d.absolutePath(), uninstallationScript,
                    ".Npackd\\Uninstall.log",
                    env, printScriptOutput);

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

    if (job->shouldProceed()) {

        // ignore the error
        QString err;
        this->createExecutableShims("", &err);

        job->setProgress(0.46);
    }

    bool success = false;
    if (job->getErrorMessage().isEmpty()) {
        if (d.exists()) {
            Job* rjob = job->newSubJob(0.53, QObject::tr("Deleting files"));

            // the errors occured while deleting the directory are ignored
            removeDirectory(rjob, d.absolutePath(), programCloseType);

            QString err = setPath("");
            if (!err.isEmpty())
                job->setErrorMessage(err);
            else
                success = true;
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
            job->setTitle(initialTitle + " / " +
                    QObject::tr("Updating NPACKD_CL"));
            DBRepository::getDefault()->updateNpackdCLEnvVar();
        }
        job->setProgress(1);
    }
    job->setTitle(initialTitle);

    deleteShortcutsFuture.waitForFinished();

    if (success)
        WPMUtils::reportEvent(QObject::tr(
                "The package %1 was removed successfully from %2").
                arg(this->toString(true), where));
    else
        WPMUtils::reportEvent(QObject::tr(
                "The removal of the package %1 from %2 failed: %3").
                arg(this->toString(true), where, job->getErrorMessage()),
                EVENTLOG_ERROR_TYPE);

    job->complete();
}

void PackageVersion::removeDirectory(Job* job, const QString& dir,
        int programCloseType)
{
    // for a .dll loaded in another process it is possible to:
    // - rename the .dll file
    // - move the .dll file to another directory
    // - move the directory with the .dll to another directory

    QDir d(dir);

    QTemporaryDir tempDir;

    int n = 0;
    while (job->shouldProceed() && n < 10) {
        d.refresh();
        if (d.exists()) {
            // qCDebug(npackd) << "moving to recycly bin" << d.absolutePath();
            WPMUtils::moveToRecycleBin(d.absolutePath());
        } else {
            break;
        }

        d.refresh();
        if (d.exists() && tempDir.isValid()) {
            // qCDebug(npackd) << "renaming" << d.absolutePath() << " to " <<
            //        tempDir.path() + "\\" + d.dirName();
            d.rename(d.absolutePath(), tempDir.path() + "\\" + d.dirName());
        } else {
            break;
        }

        d.refresh();
        if (d.exists()) {
            Job* sub = job->newSubJob(0.01,
                    QObject::tr("Deleting the directory %1").arg(
                    d.absolutePath().replace('/', '\\')));

            // qCDebug(npackd) << "deleting" << d.absolutePath();
            WPMUtils::removeDirectory(sub, d);
        } else {
            break;
        }

        d.refresh();
        if (d.exists()) {
            WPMUtils::closeProcessesThatUseDirectory(dir, programCloseType);
        } else {
            break;
        }

        // 5 seconds
        Sleep(5000);

        n++;
    }

    job->setProgress(1);

    job->complete();
}

QString PackageVersion::planInstallation(InstalledPackages &installed,
        QList<InstallOperation*>& ops, QList<PackageVersion*>& avoid,
        const QString& where)
{
    QString res;

    avoid.append(this->clone());

    DBRepository* rep = DBRepository::getDefault();

    for (int i = 0; i < this->dependencies.count(); i++) {
        Dependency* d = this->dependencies.at(i);
        bool depok = installed.isInstalled(*d);
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
            QList<PackageVersion*> pvs = rep->findAllMatchesToInstall(
                    *d, avoid, &err);
            if (!err.isEmpty()) {
                res = QString(QObject::tr("Error searching for the dependency matches: %1")).
                           arg(err);
                qDeleteAll(pvs);
                break;
            }
            if (pvs.count() == 0) {
                res = QString(QObject::tr("Unsatisfied dependency: %1")).
                           arg(rep->toString(*d));
                break;
            } else {
                bool found = false;
                for (int j = 0; j < pvs.count(); j++) {
                    PackageVersion* pv = pvs.at(j);
                    InstalledPackages installed2(installed);
                    int opsCount = ops.count();
                    int avoidCount = avoid.count();

                    res = pv->planInstallation(installed2, ops, avoid);
                    if (!res.isEmpty()) {
                        // rollback
                        while (ops.count() > opsCount) {
                            delete ops.takeLast();
                        }
                        while (avoid.count() > avoidCount) {
                            delete avoid.takeLast();
                        }
                    } else {
                        found = true;
                        installed = installed2;
                        break;
                    }
                }
                if (!found) {
                    res = QString(QObject::tr("Unsatisfied dependency: %1")).
                               arg(rep->toString(*d));
                }
            }
            qDeleteAll(pvs);
        }
    }

    if (res.isEmpty()) {
        if (!installed.isInstalled(this->package, this->version)) {
            InstallOperation* io = new InstallOperation();
            io->install = true;
            io->package = this->package;
            io->version = this->version;
            io->where = where;
            ops.append(io);

            QString where2 = where;
            if (where2.isEmpty()) {
                where2 = this->getIdealInstallationDirectory();
                where2 = WPMUtils::findNonExistingFile(where2, "");
            }
            installed.setPackageVersionPath(this->package, this->version,
                    where2, false);
        }
    }

    return res;
}

QString PackageVersion::planUninstallation(InstalledPackages &installed,
        QList<InstallOperation*>& ops)
{
    // qCDebug(npackd) << "PackageVersion::planUninstallation()" << this->toString();
    QString res;

    if (!installed.isInstalled(this->package, this->version))
        return res;

    installed.setPackageVersionPath(this->package, this->version,
            "", false);

    DBRepository* dbr = DBRepository::getDefault();

    // this loop ensures that all the items in "installed" are processed
    // even if changes in the list were done in nested calls to
    // "planUninstallation"
    while (true) {
        QScopedPointer<InstalledPackageVersion> ipv(installed.
                findFirstWithMissingDependency());
        if (ipv.data()) {
            QScopedPointer<PackageVersion> pv(dbr->findPackageVersion_(
                    ipv.data()->package, ipv.data()->version, &res));
            if (!res.isEmpty())
                break;

            res = pv->planUninstallation(installed, ops);
            if (!res.isEmpty())
                break;
        } else {
            break;
        }
    }

    if (res.isEmpty()) {
        InstallOperation* op = new InstallOperation();
        op->install = false;
        op->package = this->package;
        op->version = this->version;
        ops.append(op);
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

void PackageVersion::downloadTo(Job& job, const QString& filename, bool interactive)
{
    QFile* f = new QFile(filename);

    bool downloadOK = false;
    QString dsha1;

    if (job.shouldProceed()) {
        if (!f->open(QIODevice::ReadWrite)) {
            job.setErrorMessage(QString(QObject::tr("Cannot open the file: %0")).
                    arg(f->fileName()));
        } else {
            Job* djob = job.newSubJob(0.8,
                    QObject::tr("Downloading & computing hash sum"));

            Downloader::Request request(this->download);
            request.file = f;
            if (!this->sha1.isEmpty())
                request.hashSum = true;
            request.alg = this->hashSumType;
            request.interactive = interactive;
            Downloader::Response response = Downloader::download(djob, request);
            dsha1 = response.hashSum;
            downloadOK = djob->shouldProceed();
            f->close();
        }
    }

    if (job.shouldProceed()) {
        if (!downloadOK) {
            if (!f->open(QIODevice::ReadWrite)) {
                job.setErrorMessage(QObject::tr("Cannot open the file: %0").
                        arg(f->fileName()));
            } else {
                double rest = 0.9 - job.getProgress();
                Job* djob = job.newSubJob(rest,
                        QObject::tr("Downloading & computing hash sum (2nd try)"));
                Downloader::Request request(this->download);
                request.file = f;
                if (!this->sha1.isEmpty())
                    request.hashSum = true;
                request.alg = this->hashSumType;
                request.interactive = interactive;
                Downloader::Response response =
                        Downloader::download(djob, request);
                dsha1 = response.hashSum;
                if (!djob->getErrorMessage().isEmpty())
                    job.setErrorMessage(QObject::tr("Error downloading %1: %2").
                        arg(this->download.toString()).arg(
                        djob->getErrorMessage()));
                f->close();
            }
        } else {
            job.setProgress(0.9);
        }
    }

    delete f;

    if (job.shouldProceed())
        job.setProgress(1);

    job.complete();
}

QString PackageVersion::downloadAndComputeSHA1(Job* job)
{
    if (!this->download.isValid()) {
        job->setErrorMessage(QObject::tr("No download URL"));
        job->complete();
        return "";
    }

    QString r;

    Job* djob = job->newSubJob(0.95, QObject::tr("Downloading"));
    Downloader::Request request(this->download);
    QTemporaryFile* f = Downloader::downloadToTemporary(djob, request);
    if (!djob->getErrorMessage().isEmpty())
        job->setErrorMessage(QString(QObject::tr("Download failed: %1")).
                arg(djob->getErrorMessage()));

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.05, QObject::tr("Computing SHA1"));
        r = WPMUtils::sha1(f->fileName());
        sub->completeWithProgress();
    }

    if (f)
        delete f;

    job->complete();

    return r;
}

QString PackageVersion::getPackageTitle(
        bool includeFullPackageName) const
{
    DBRepository* rep = DBRepository::getDefault();

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

QList<PackageVersion*> PackageVersion::getAddPackageVersionOptions(const CommandLine& cl,
        QString* err)
{
    QList<PackageVersion*> ret;
    QList<CommandLine::ParsedOption *> pos = cl.getParsedOptions();

    DBRepository* rep = DBRepository::getDefault();

    for (int i = 0; i < pos.size(); i++) {
        if (!err->isEmpty())
            break;

        CommandLine::ParsedOption* po = pos.at(i);
        if (po->opt->nameMathes("package")) {
            CommandLine::ParsedOption* ponext = 0;
            if (i + 1 < pos.size())
                ponext = pos.at(i + 1);

            QString package = po->value;
            if (!Package::isValidName(package)) {
                *err = QObject::tr("Invalid package name: %1").arg(package);
            }

            Package* p = 0;
            if (err->isEmpty()) {
                p = AbstractRepository::findOnePackage(package, err);
                if (err->isEmpty()) {
                    if (!p)
                        *err = QObject::tr("Unknown package: %1").arg(package);
                }
            }

            PackageVersion* pv = 0;
            if (err->isEmpty()) {
                QString version;
                if (ponext != 0 && ponext->opt->nameMathes("version"))
                    version = ponext->value;

                QString versions;
                if (ponext != 0 && ponext->opt->nameMathes("versions"))
                    versions = ponext->value;

                if (!versions.isNull()) {
                    i++;
                    Dependency v;
                    v.package = p->name;
                    if (!v.setVersions(versions)) {
                        *err = QObject::tr("Cannot parse a version range: %1").
                                arg(versions);
                    } else {
                        InstalledPackageVersion* ipv =
                                rep->findHighestInstalledMatch(v);
                        if (ipv) {
                            pv = rep->findPackageVersion_(ipv->package,
                                    ipv->version, err);
                            if (err->isEmpty()) {
                                if (!pv) {
                                    *err = QObject::tr("Package version not found: %1 (%2) %3").
                                            arg(p->title).arg(p->name).
                                            arg(ipv->version.getVersionString());
                                }
                            }
                            delete ipv;
                        } else {
                            pv = rep->findBestMatchToInstall(v,
                                    QList<PackageVersion*>(), err);
                            if (err->isEmpty()) {
                                if (!pv) {
                                    *err = QObject::tr("Package version not found: %1 (%2) %3").
                                            arg(p->title).arg(p->name).
                                            arg(versions);
                                }
                            }
                        }
                    }
                } else if (version.isNull()) {
                    pv = rep->findNewestInstallablePackageVersion_(
                            p->name, err);
                    if (err->isEmpty()) {
                        if (!pv) {
                            *err = QObject::tr("No installable version was found for the package %1 (%2)").
                                    arg(p->title).arg(p->name);
                        }
                    }
                } else {
                    i++;
                    Version v;
                    if (!v.setVersion(version)) {
                        *err = QObject::tr("Cannot parse version: %1").
                                arg(version);
                    } else {
                        pv = rep->findPackageVersion_(p->name, v,
                                err);
                        if (err->isEmpty()) {
                            if (!pv) {
                                *err = QObject::tr("Package version not found: %1 (%2) %3").
                                        arg(p->title).arg(p->name).arg(version);
                            }
                        }
                    }
                }
            }

            if (pv)
                ret.append(pv);

            delete p;
        }
    }

    return ret;
}

QList<PackageVersion*> PackageVersion::getRemovePackageVersionOptions(const CommandLine& cl,
        QString* err)
{
    QList<PackageVersion*> ret;
    QList<CommandLine::ParsedOption *> pos = cl.getParsedOptions();

    DBRepository* rep = DBRepository::getDefault();

    for (int i = 0; i < pos.size(); i++) {
        if (!err->isEmpty())
            break;

        CommandLine::ParsedOption* po = pos.at(i);
        if (po->opt->nameMathes("package")) {
            CommandLine::ParsedOption* ponext = 0;
            if (i + 1 < pos.size())
                ponext = pos.at(i + 1);

            QString package = po->value;
            if (!Package::isValidName(package)) {
                *err = QObject::tr("Invalid package name: %1").arg(package);
            }

            Package* p = 0;
            if (err->isEmpty()) {
                p = AbstractRepository::findOnePackage(package, err);
                if (err->isEmpty()) {
                    if (!p)
                        *err = QObject::tr("Unknown package: %1").arg(package);
                }
            }

            PackageVersion* pv = 0;
            if (err->isEmpty()) {
                QString version;
                if (ponext != 0 && ponext->opt->nameMathes("version"))
                    version = ponext->value;
                if (version.isNull()) {
                    QList<InstalledPackageVersion*> ipvs =
                            InstalledPackages::getDefault()->getByPackage(p->name);
                    if (ipvs.count() == 0) {
                        *err = QObject::tr(
                                "Package %1 (%2) is not installed").
                                arg(p->title).arg(p->name);
                    } else if (ipvs.count() > 1) {
                        QString vns;
                        for (int i = 0; i < ipvs.count(); i++) {
                            InstalledPackageVersion* ipv = ipvs.at(i);
                            if (!vns.isEmpty())
                                vns.append(", ");
                            vns.append(ipv->version.getVersionString());
                        }
                        *err = QObject::tr(
                                "More than one version of the package %1 (%2) "
                                "is installed: %3").arg(p->title).arg(p->name).
                                arg(vns);
                    } else {
                        pv = rep->findPackageVersion_(p->name,
                                ipvs.at(0)->version, err);
                        if (err->isEmpty()) {
                            if (!pv) {
                                *err = QObject::tr("Package version not found: %1 (%2) %3").
                                        arg(p->title).arg(p->name).arg(version);
                            }
                        }
                    }
                    qDeleteAll(ipvs);
                } else {
                    i++;
                    Version v;
                    if (!v.setVersion(version)) {
                        *err = QObject::tr("Cannot parse version: %1").
                                arg(version);
                    } else {
                        pv = rep->findPackageVersion_(p->name, v,
                                err);
                        if (err->isEmpty()) {
                            if (!pv) {
                                *err = QObject::tr("Package version not found: %1 (%2) %3").
                                        arg(p->title).arg(p->name).arg(version);
                            }
                        }
                    }
                }
            }

            if (pv)
                ret.append(pv);

            delete p;
        }
    }

    return ret;
}

QList<InstalledPackageVersion*> PackageVersion::getPathPackageVersionOptions(const CommandLine& cl,
        QString* err)
{
    QList<InstalledPackageVersion*> ret;
    QList<CommandLine::ParsedOption *> pos = cl.getParsedOptions();

    InstalledPackages* ip = InstalledPackages::getDefault();

    DBRepository* rep = DBRepository::getDefault();

    for (int i = 0; i < pos.size(); i++) {
        if (!err->isEmpty())
            break;

        CommandLine::ParsedOption* po = pos.at(i);
        if (po->opt->nameMathes("package")) {
            CommandLine::ParsedOption* ponext = 0;
            if (i + 1 < pos.size())
                ponext = pos.at(i + 1);

            QString package = po->value;
            if (!Package::isValidName(package)) {
                *err = QObject::tr("Invalid package name: %1").arg(package);
            }

            Package* p = 0;
            if (err->isEmpty()) {
                p = AbstractRepository::findOnePackage(package, err);
                if (err->isEmpty()) {
                    if (!p)
                        *err = QObject::tr("Unknown package: %1").arg(package);
                }
            }

            InstalledPackageVersion* ipv = 0;
            if (err->isEmpty()) {
                QString version;
                if (ponext != 0 && ponext->opt->nameMathes("version"))
                    version = ponext->value;

                QString versions;
                if (ponext != 0 && ponext->opt->nameMathes("versions"))
                    versions = ponext->value;

                if (!versions.isNull()) {
                    i++;
                    Dependency v;
                    v.package = p->name;
                    if (!v.setVersions(versions)) {
                        *err = QObject::tr("Cannot parse a version range: %1").
                                arg(versions);
                    } else {
                        ipv = rep->findHighestInstalledMatch(v);
                    }
                } else if (version.isNull()) {
                    ipv = ip->getNewestInstalled(p->name);
                } else {
                    i++;
                    Version v;
                    if (!v.setVersion(version)) {
                        *err = QObject::tr("Cannot parse version: %1").
                                arg(version);
                    } else {
                        ipv = ip->find(p->name, v);
                    }
                }
            }

            if (ipv)
                ret.append(ipv);

            delete p;
        }
    }

    return ret;
}

bool PackageVersion::createExecutableShims(const QString& dir, QString *errMsg)
{
    *errMsg = "";

    if (this->cmdFiles.size() == 0)
        return true;

    QString sourceBasePath = WPMUtils::adminMode
		? WPMUtils::getShellDir(CSIDL_COMMON_APPDATA) + "\\Npackd\\Commands\\"
		: WPMUtils::getShellDir(CSIDL_APPDATA) + "\\Npackd\\Commands\\";

    DBRepository* dbr = DBRepository::getDefault();

    QDir d(dir);

    QString exeProxy = WPMUtils::getExeDir() + "\\exeproxy.exe";
    if (!d.exists(exeProxy)) {
        exeProxy = WPMUtils::getExeDir() + "\\ncl.exe";
        if (!d.exists(exeProxy)) {
            exeProxy = "";
        }
    }

    if (exeProxy.isEmpty()) {
        *errMsg = QObject::tr("Cannot find the EXE Proxy executable.");
    }

    if (errMsg->isEmpty()) {
        for (int i = 0; i < this->cmdFiles.count(); i++) {
            QString cmdFilePath = this->cmdFiles.at(i);
            cmdFilePath.replace('/', '\\');

            QString cmdFileName = getCmdFileName(i);
            QString cmdFileNameLC = cmdFileName.toLower();

            QList<PackageVersion*> pvs = dbr->findPackageVersionsWithCmdFile(
                    cmdFileName, errMsg);
            if (pvs.size() > 0) {
                PackageVersion* last = 0;
                for (int i = pvs.size() - 1; i >= 0; i--) {
                    PackageVersion* pv = pvs.at(i);
                    if (pv->installed()) {
                        last = pv;
                        break;
                    }
                }

                QString sourcePath = sourceBasePath + cmdFileName;
                if (d.exists(sourcePath))
                    d.remove(sourcePath);

                if (last || !dir.isEmpty()) {
                    QString targetPath;
                    if (last) {
                        targetPath = last->getPath() + "\\";
                        for (int j = 0; j < last->cmdFiles.size(); j++) {
                            QString fn = last->getCmdFileName(j);
                            if (fn.toLower() == cmdFileNameLC) {
                                targetPath.append(last->cmdFiles.at(j));
                                break;
                            }
                        }
                    } else {
                        targetPath = dir + "\\" + cmdFilePath;
                        if (!d.exists(targetPath)) {
                            *errMsg = QString(QObject::tr("Command line tool %1 does not exist")).
                                    arg(targetPath);
                            break;
                        }
                    }

                    std::unique_ptr<Job> job(new Job());

                    WPMUtils::executeFile(job.get(), dir, exeProxy,
                            "exeproxy-copy \"" + sourcePath + "\" \"" +
                            targetPath + "\"",
                            0,
                            QStringList());
                }
            }
            qDeleteAll(pvs);
        }
    }

    return errMsg->isEmpty();
}

QString PackageVersion::getCmdFileName(int index)
{
    QString cmdFileName = cmdFiles.at(index);
    int lastIndex = cmdFileName.lastIndexOf('\\');
    if (lastIndex >= 0)
        cmdFileName.remove(0, lastIndex + 1);
    return cmdFileName;
}

bool PackageVersion::createShortcuts(const QString& dir, QString *errMsg)
{
    *errMsg = "";

    QString packageTitle = this->getPackageTitle();

    QDir d(dir);
    Package* p = DBRepository::getDefault()->findPackage_(this->package);
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
        QString commonStartMenu = WPMUtils::adminMode
			? WPMUtils::getShellDir(CSIDL_COMMON_STARTMENU)
			: WPMUtils::getShellDir(CSIDL_STARTMENU);
        simple = commonStartMenu + "\\" + simple;
        withVersion = commonStartMenu + "\\" + withVersion;

        QString from;
        if (QFileInfo(simple).exists())
            from = WPMUtils::findNonExistingFile(withVersion, ".lnk");
        else
            from = simple;

        // qCDebug(npackd) << "createShortcuts " << ifile << " " << p << " " <<
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

QString PackageVersion::getIdealInstallationDirectory()
{
    return WPMUtils::normalizePath(
            WPMUtils::getInstallationDirectory() + "\\" +
            WPMUtils::makeValidFilename(this->getPackageTitle(), '_'),
            false);
}

QString PackageVersion::getSecondaryInstallationDirectory()
{
    return WPMUtils::normalizePath(
            WPMUtils::getInstallationDirectory() + "\\" +
            WPMUtils::makeValidFilename(this->getPackageTitle(), '_') +
            "-" + this->version.getVersionString(),
            false);
}

QString PackageVersion::getPreferredInstallationDirectory()
{
    QString name = WPMUtils::normalizePath(
            WPMUtils::getInstallationDirectory() + "\\" +
            WPMUtils::makeValidFilename(this->getPackageTitle(), '_'),
            false);
    if (!QFileInfo(name).exists())
        return name;
    else
        return WPMUtils::findNonExistingFile(name + "-" +
                this->version.getVersionString(), "");
}

QString PackageVersion::download_(Job* job, const QString& where,
        bool interactive, const QString user, const QString password)
{
    if (!this->download.isValid()) {
        job->setErrorMessage(QObject::tr("No download URL"));
        job->complete();
        return "";
    }

    QString initialTitle = job->getTitle();

    // qCDebug(npackd) << "install.2";
    QDir d(where);
    QString npackdDir = where + "\\.Npackd";

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + " / " +
                QObject::tr("Creating directory"));
        QString s = d.absolutePath();
        if (!d.mkpath(s)) {
            job->setErrorMessage(QObject::tr("Cannot create directory: %0").
                    arg(s));
        } else {
            job->setProgress(0.01);
        }
    }
    job->setTitle(initialTitle);

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + " / " +
                QObject::tr("Creating .Npackd sub-directory"));
        QString s = npackdDir;
        if (!d.mkpath(s)) {
            job->setErrorMessage(QObject::tr("Cannot create directory: %0").
                    arg(s));
        } else {
            job->setProgress(0.02);
        }
    }
    job->setTitle(initialTitle);

    bool httpConnectionAcquired = false;

    if (job->shouldProceed()) {
        job->setTitle(initialTitle + " / " +
                QObject::tr("Waiting for a free HTTP connection"));

        time_t start = time(NULL);
        while (!job->isCancelled()) {
            httpConnectionAcquired = httpConnections.tryAcquire(1, 10000);
            if (httpConnectionAcquired) {
                job->setProgress(0.05);
                break;
            }

            time_t seconds = time(NULL) - start;
            job->setTitle(initialTitle + " / " + QString(
                    QObject::tr("Waiting for a free HTTP connection (%1 minutes)")).
                    arg(seconds / 60));
        }
    }
    job->setTitle(initialTitle);

    // qCDebug(npackd) << "install.3";
    QFile* f = new QFile(npackdDir + "\\__NpackdPackageDownload");

    bool downloadOK = false;
    QString dsha1;

    if (job->shouldProceed()) {
        if (!f->open(QIODevice::ReadWrite)) {
            job->setErrorMessage(QString(QObject::tr("Cannot open the file: %0")).
                    arg(f->fileName()));
        } else {
            Job* djob = job->newSubJob(0.8,
                    QObject::tr("Downloading & computing hash sum"));

            Downloader::Request request(this->download);
            request.file = f;
            if (!this->sha1.isEmpty())
                request.hashSum = true;
            request.user = user;
            request.password = password;
            request.alg = this->hashSumType;
            request.interactive = interactive;
            Downloader::Response response = Downloader::download(djob, request);
            dsha1 = response.hashSum;
            downloadOK = !djob->isCancelled() &&
                    djob->getErrorMessage().isEmpty();
            f->close();
        }
    }

    if (job->shouldProceed()) {
        if (!downloadOK) {
            if (!f->open(QIODevice::ReadWrite)) {
                job->setErrorMessage(QObject::tr("Cannot open the file: %0").
                        arg(f->fileName()));
            } else {
                double rest = 0.9 - job->getProgress();
                Job* djob = job->newSubJob(rest,
                        QObject::tr("Downloading & computing hash sum (2nd try)"));
                Downloader::Request request(this->download);
                request.file = f;
                if (!this->sha1.isEmpty())
                    request.hashSum = true;
                request.user = user;
                request.password = password;
                request.alg = this->hashSumType;
                request.interactive = interactive;
                Downloader::Response response =
                        Downloader::download(djob, request);
                dsha1 = response.hashSum;
                if (!djob->getErrorMessage().isEmpty())
                    job->setErrorMessage(QObject::tr("Error downloading %1: %2").
                        arg(this->download.toString()).arg(
                        djob->getErrorMessage()));
                f->close();
            }
        } else {
            job->setProgress(0.9);
        }
    }

    if (httpConnectionAcquired)
        httpConnections.release();

    if (job->shouldProceed()) {
        if (!this->sha1.isEmpty()) {
            if (dsha1.toLower() != this->sha1.toLower()) {
                job->setErrorMessage(QString(
                        QObject::tr("Hash sum %1 found, but %2 was expected. The file has changed.")).arg(dsha1).
                        arg(this->sha1));
            } else {
                job->setProgress(0.91);
            }
        } else {
            job->setProgress(0.91);
        }
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Checking for viruses"));
        bool safe = isFileSafe(f->fileName(), this->download.toString());
        if (!safe) {
            job->setErrorMessage(QObject::tr("Antivirus check failed. The file is not safe."));
        }
        sub->completeWithProgress();
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
    if (job->shouldProceed()) {
        if (this->type == 0) {
            Job* djob = job->newSubJob(0.06, QObject::tr("Extracting files"));
            WPMUtils::unzip(djob, f->fileName(), d.absolutePath() + "\\");
            if (!djob->getErrorMessage().isEmpty())
                job->setErrorMessage(QString(
                        QObject::tr("Error unzipping file into directory %0: %1")).
                        arg(d.absolutePath()).
                        arg(djob->getErrorMessage()));
            else if (!job->isCancelled())
                job->setProgress(0.98);
        } else {
            job->setTitle(initialTitle + " / " +
                    QObject::tr("Renaming the downloaded file"));
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
                job->setProgress(0.98);
            }
        }
    }
    job->setTitle(initialTitle);

    if (job->shouldProceed()) {
        QString errMsg = this->saveFiles(d);
        if (!errMsg.isEmpty()) {
            job->setErrorMessage(errMsg);
        } else {
            job->setProgress(0.99);
        }
    }

    if (f && f->exists())
        f->remove();

    delete f;

    if (job->shouldProceed()) {
        job->setProgress(1);
    }

    job->complete();

    return binary;
}

void PackageVersion::installWith(Job* job)
{

}

void PackageVersion::install(Job* job, const QString& where,
        const QString& binary,
        bool printScriptOutput, int programCloseType)
{
    if (installed()) {
        job->setProgress(1);
        job->complete();
        return;
    }

    QString initialTitle = job->getTitle();

    // qCDebug(npackd) << "install.2";
    QDir d(where);

    QString installationScript;
    if (job->shouldProceed()) {
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

    if (job->shouldProceed() && this->dependencies.size() > 0) {
        Job* sjob = job->newSubJob(0.2,
                QObject::tr("Running the installation hooks for dependencies"),
                true, true);
        installWith(sjob);
    } else {
        job->setProgress(0.2);
    }

    bool installationScriptAcquired = false;

    if (job->shouldProceed()) {
        if (!installationScript.isEmpty()) {
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
        } else {
            job->setProgress(0.21);
        }
    }
    job->setTitle(initialTitle);

    if (job->shouldProceed()) {
        if (!installationScript.isEmpty()) {
            Job* exec = job->newSubJob(0.7,
                    QObject::tr("Running the installation script (this may take some time)"),
                    true, true);
            if (!d.exists(".Npackd"))
                d.mkdir(".Npackd");

            QStringList env;
            env.append("NPACKD_PACKAGE_BINARY");
            env.append(WPMUtils::normalizePath(where, false) + "\\" + binary);

            QString err = addBasicVars(&env);
            if (!err.isEmpty())
                job->setErrorMessage(err);

            addDependencyVars(&env);

            this->executeFile2(exec, d.absolutePath(), installationScript,
                    ".Npackd\\Install.log", env, printScriptOutput);
            if (exec->getErrorMessage().isEmpty()) {
                QString path = d.absolutePath();
                path.replace('/', '\\');
                QString err = setPath(path);
                if (!err.isEmpty()) {
                    job->setErrorMessage(err);
                }
            }
        } else {
            QString path = d.absolutePath();
            path.replace('/', '\\');
            QString err = setPath(path);
            if (!err.isEmpty())
                job->setErrorMessage(err);
        }

        if (this->package == "com.googlecode.windows-package-manager.NpackdCL" ||
                this->package == "com.googlecode.windows-package-manager.NpackdCL64") {
            job->setTitle(initialTitle + " / " +
                    QObject::tr("Updating NPACKD_CL"));
            DBRepository::getDefault()->updateNpackdCLEnvVar();
        }

        job->setProgress(0.91);
    }
    job->setTitle(initialTitle);

    if (installationScriptAcquired)
        installationScripts.release();

    if (job->shouldProceed()) {
        QString err;
        this->createShortcuts(d.absolutePath(), &err);
        if (err.isEmpty())
            job->setProgress(0.97);
        else
            job->setErrorMessage(err);
    }

    if (job->shouldProceed()) {
        // ignore the error
        QString err;
        this->createExecutableShims(d.absolutePath(), &err);

        job->setProgress(0.98);
    }

    bool success = false;
    if (job->shouldProceed()) {
        QString err = InstalledPackages::getDefault()->setPackageVersionPath(
                this->package, this->version, where);
        //InstalledPackages::getDefault()->notifyInstalled(this->package,
        //        this->version); // integrate in setPackageVersionPath?
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else
            success = true;
    }

    if (job->shouldProceed()) {
        QString err = DBRepository::getDefault()->updateStatus(this->package);
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    if (!job->shouldProceed()) {
        Job* sub = job->newSubJob(0.01,
                QObject::tr("Deleting start menu, desktop and quick launch shortcuts"));
        deleteShortcuts(d.absolutePath(), sub, true, true, true);

        Job* rjob = job->newSubJob(0.01, QObject::tr("Deleting files"));
        removeDirectory(rjob, d.absolutePath(), programCloseType);
    }

    if (success) {
        WPMUtils::reportEvent(QObject::tr(
                "The package %1 was installed successfully in %2").
                arg(this->toString(true), where));
    } else {
        WPMUtils::reportEvent(QObject::tr(
                "The installation of the package %1 in %2 failed: %3").
                arg(this->toString(true), where, job->getErrorMessage()),
                EVENTLOG_ERROR_TYPE);
    }

    if (job->shouldProceed()) {
        job->setProgress(1);
    }

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
    env->append(DBRepository::getDefault()->
            computeNpackdCLEnvVar_(&err));

    return err;
}

void PackageVersion::addDependencyVars(QStringList* vars)
{
    DBRepository* rep = DBRepository::getDefault();
    for (int i = 0; i < this->dependencies.count(); i++) {
        Dependency* d = this->dependencies.at(i);
        if (!d->var.isEmpty()) {
            vars->append(d->var);
            InstalledPackageVersion* ipv = rep->findHighestInstalledMatch(*d);
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
    DBRepository* r = DBRepository::getDefault();
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
        const QStringList& env, bool printScriptOutput)
{
    WPMUtils::executeBatchFile(
            job, where, path, outputFile, env, printScriptOutput);

    if (!job->getErrorMessage().isEmpty()) {
        QString out = where + "\\" + outputFile;

        if (QFile::exists(out)) {
            QString lastOutputLines;
            QFile outf(out);
            if (outf.open(QFile::ReadOnly)) {
                qint64 sz = outf.size();
                qint64 p;

                if (sz > 2048) {
                    // UTF-16: 2 bytes per character, 2040 to not get the BOM
                    p = sz - 2040 - sz % 1;
                } else if (sz >= 2) {
                    // BOM
                    p = 2;
                } else {
                    p = 0;
                }

                outf.seek(p);

                QByteArray ba = outf.read(sz - p);
                lastOutputLines = QString::fromUtf16(
                        (const ushort*) ba.constData(),
                        ba.length() / 2);

                outf.close();
            }

            QTemporaryFile of;
            of.setFileTemplate(QDir::tempPath() + "\\NpackdXXXXXX.log");
            of.setAutoRemove(false);
            QString filename;
            if (of.open()) {
                filename = WPMUtils::normalizePath(of.fileName(), false);

                // qCDebug(npackd) << "haha" << filename;

                of.close();
                if (of.remove()) {
                    // ignoring the error here
                    QFile::copy(out, filename);
                }
            }

            job->setErrorMessage((
                    QObject::tr("%1. Full output was saved in %2") +
                    "\r\n" +
                    QObject::tr("The last lines of the output from the removal script:") +
                    "\r\n...\r\n%3").arg(
                    job->getErrorMessage(), filename, lastOutputLines));
        } else {
            job->setErrorMessage(QString(
                    QObject::tr("%1. No output was generated")).arg(
                    job->getErrorMessage()));
        }
    }
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
    r->cmdFiles = this->cmdFiles;
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

PackageVersion *PackageVersion::parse(const QByteArray &xml, QString *err, bool validate)
{
    PackageVersion* r = 0;

    Repository rep;
    RepositoryXMLHandler handler(&rep, QUrl());
    QXmlSimpleReader reader;
    reader.setContentHandler(&handler);
    reader.setErrorHandler(&handler);
    QXmlInputSource inputSource;
    inputSource.setData(xml);
    handler.enter("root");
    if (!reader.parse(inputSource))
        *err = handler.errorString();
    else {
        if (rep.packageVersions.size() == 1) {
            r = rep.packageVersions.takeAt(0);
            *err = "";
        } else {
            *err = QObject::tr("Expected one package version");
        }
    }
    return r;
}

bool PackageVersion::contains(const QList<PackageVersion *> &list,
        PackageVersion *pv)
{
    return indexOf(list, pv) >= 0;
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
    for (int i = 0; i < this->cmdFiles.count(); i++) {
        w->writeStartElement("cmd-file");
        //qCDebug(npackd) << this->package << this->version.getVersionString() <<
        //    this->cmdFiles.at(i) << "!";
        w->writeAttribute("path", this->cmdFiles.at(i));
        w->writeEndElement();
    }
    for (int i = 0; i < this->files.count(); i++) {
        w->writeStartElement("file");
        w->writeAttribute("path", this->files.at(i)->path);
        w->writeCharacters(files.at(i)->content);
        w->writeEndElement();
    }
    if (this->download.isValid()) {
        w->writeTextElement("url", this->download.toString(
                QUrl::FullyEncoded));
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

void PackageVersion::toJSON(QJsonObject& w) const
{
    w["name"] = this->version.getVersionString();
    w["package"] = this->package;
    if (this->type == 1)
        w["type"] = "one-file";

    if (!importantFiles.isEmpty()) {
        QJsonArray a;
        for (int i = 0; i < this->importantFiles.count(); i++) {
            QJsonObject obj;
            obj["path"] = this->importantFiles.at(i);
            obj["title"] = this->importantFilesTitles.at(i);
            a.append(obj);
        }
        w["importantFiles"] = a;
    }

    if (!cmdFiles.isEmpty()) {
        QJsonArray a;
        for (int i = 0; i < this->cmdFiles.count(); i++) {
            QJsonObject obj;
            obj["path"] = this->cmdFiles.at(i);
            a.append(obj);
        }
        w["cmdFiles"] = a;
    }

    if (!files.isEmpty()) {
        QJsonArray path;
        for (int i = 0; i < this->files.count(); i++) {
            QJsonObject obj;
            obj["path"] = this->files.at(i)->path;
            obj["content"] = files.at(i)->content;
            path.append(obj);
        }
        w["files"] = path;
    }

    if (this->download.isValid()) {
        w["url"] = this->download.toString(QUrl::FullyEncoded);
    }
    if (!this->sha1.isEmpty()) {
        if (this->hashSumType == QCryptographicHash::Sha1)
            w["sha1"] = this->sha1;
        else
            w["hashSum"] = this->sha1;
    }

    if (dependencies.count() > 0) {
        QJsonArray dependency;
        for (int i = 0; i < this->dependencies.count(); i++) {
            Dependency* d = this->dependencies.at(i);
            QJsonObject obj;
            obj["package"] = d->package;
            obj["versions"] = d->versionsToString();
            if (!d->var.isEmpty())
                obj["variable"] = d->var;
            dependency.append(obj);
        }
        w["dependencies"] = dependency;
    }

    if (!this->msiGUID.isEmpty()) {
        w["detectMSI"] = this->msiGUID;
    }

    if (!detectFiles.isEmpty()) {
        QJsonArray detectFile;
        for (int i = 0; i < detectFiles.count(); i++) {
            DetectFile* df = this->detectFiles.at(i);
            QJsonObject obj;
            obj["path"] = df->path;
            obj["sha1"] = df->sha1;
            detectFile.append(obj);
        }
        w["detectFiles"] = detectFile;
    }
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

void PackageVersion::stop(Job* job, int programCloseType,
        bool printScriptOutput)
{
    bool me = false;
    QString myPackage = InstalledPackages::packageName;
    Version myVersion;
    (void) myVersion.setVersion(NPACKD_VERSION);
    if (this->package == myPackage && this->version == myVersion) {
        if (WPMUtils::pathEquals(WPMUtils::getExeDir(), getPath())) {
            me = true;
        }
    }

    QDir d(getPath());
    if (me) {
        job->setProgress(0.5);
    } else if (QFile::exists(d.absolutePath() + "\\.Npackd\\Stop.bat")) {
        Job* exec = job->newSubJob(0.5,
                QObject::tr("Executing the stop script"), true, true);
        if (!d.exists(".Npackd"))
            d.mkdir(".Npackd");

        QStringList env;
        QString err = addBasicVars(&env);
        if (!err.isEmpty())
            job->setErrorMessage(err);

        addDependencyVars(&env);

        this->executeFile2(exec, d.absolutePath(), ".Npackd\\Stop.bat",
                ".Npackd\\Stop.log", env, printScriptOutput);
    } else {
        WPMUtils::closeProcessesThatUseDirectory(getPath(), programCloseType);

        job->setProgress(0.5);
    }

    if (job->getErrorMessage().isEmpty())
        job->setProgress(1);

    job->complete();
}
