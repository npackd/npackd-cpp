#include <shobjidl.h>
#include <windows.h>
#include <knownfolders.h>
#include <initguid.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wininet.h>
#include <stdlib.h>
#include <time.h>
#include <ole2.h>
#include <comcat.h>
#include <future>

#include <QUrl>
#include <QIODevice>
#include <QFuture>
#include <QFutureWatcher>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QBuffer>
#include <QDataStream>

#include <zlib.h>

//#include "msoav2.h"

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
#include "packageutils.h"

QSemaphore PackageVersion::httpConnections(3);
std::unordered_set<QString> PackageVersion::lockedPackageVersions;
std::recursive_mutex PackageVersion::lockedPackageVersionsMutex;

/**
 * Creates an instance of IAttachmentExecute
 */
HRESULT CreateAttachmentServices(IAttachmentExecute **ppae)
{
    // Assume that CoInitialize has already been called for this thread.
    HRESULT hr = CoCreateInstance(CLSID_AttachmentServices,
                                  nullptr,
                                  CLSCTX_ALL,
                                  IID_IAttachmentExecute,
                                  reinterpret_cast<void**>(ppae));

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
        WPMUtils::formatMessage(static_cast<DWORD>(hr), &err);
        // err = "1: " + err;
        pExecute = nullptr;
    }

    if (err.isEmpty() && pExecute) {
        hr = pExecute->SetLocalPath(WPMUtils::toLPWSTR(filename));
        if (!SUCCEEDED(hr)) {
            WPMUtils::formatMessage(static_cast<DWORD>(hr), &err);
            // err = "2: " + err;
        }
    }

    if (err.isEmpty() && pExecute) {
        pExecute->SetSource(WPMUtils::toLPWSTR(url));
        if (!SUCCEEDED(hr)) {
            WPMUtils::formatMessage(static_cast<DWORD>(hr), &err);
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

int PackageVersion::indexOf(const std::vector<PackageVersion *> &pvs, PackageVersion* f)
{
    int r = -1;
    for (int i = 0; i < static_cast<int>(pvs.size()); i++) {
        PackageVersion* pv = pvs.at(i);
        if (pv->package == f->package && pv->version == f->version) {
            r = i;
            break;
        }
    }
    return r;
}

void PackageVersion::findLockedPackageVersion(QString* package, Version* version)
{
    *package = "";

    lockedPackageVersionsMutex.lock();
    auto i = lockedPackageVersions.begin();
    QString key;
    if (i != lockedPackageVersions.end()) {
        key = *i;
    }
    lockedPackageVersionsMutex.unlock();

    if (!key.isEmpty()) {
        std::vector<QString> parts = WPMUtils::split(key, '/');
        if (parts.size() == 2) {
            *package = parts.at(0);
            if (!version->setVersion(parts.at(1))) {
                package->clear();
            }
        }
    }
}

PackageVersion::PackageVersion(const QString& package)
{
    this->package = package;
    this->type = PackageVersion::Type::ZIP;
    this->hashSumType = QCryptographicHash::Sha1;
}

PackageVersion::PackageVersion(const QString &package, const Version &version):
version(version)
{
    this->package = package;
    this->type = PackageVersion::Type::ZIP;
    this->hashSumType = QCryptographicHash::Sha1;
}

PackageVersion::PackageVersion()
{
    this->package = "unknown";
    this->type = PackageVersion::Type::ZIP;
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
    if (lockedPackageVersions.count(key) == 0) {
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
    if (lockedPackageVersions.count(key) > 0) {
        lockedPackageVersions.erase(key);
        changed = true;
    }
    lockedPackageVersionsMutex.unlock();

    if (changed)
        emitStatusChanged();
}

bool PackageVersion::isLocked() const
{
    return isLocked(this->package, this->version);
}

bool PackageVersion::isLocked(const QString& package, const Version& version)
{
    bool r;
    lockedPackageVersionsMutex.lock();
    r = lockedPackageVersions.count(getStringId(package, version)) > 0;
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


QString PackageVersion::toString(bool includeFullPackageName) const
{
    QString r = this->getPackageTitle(includeFullPackageName) + " " +
            this->version.getVersionString();
    return r;
}

QString PackageVersion::getShortPackageName()
{
    std::vector<QString> sl = WPMUtils::split(this->package, ".");
    return sl.back();
}

PackageVersion::~PackageVersion()
{
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
    bool b = SetThreadPriority(GetCurrentThread(),
            THREAD_MODE_BACKGROUND_BEGIN | THREAD_PRIORITY_IDLE);

    CoInitialize(nullptr);
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
        QDir d(WPMUtils::getShellDir(FOLDERID_StartMenu));
        WPMUtils::deleteShortcuts(dir, d);

        if (PackageUtils::globalMode)
		{
            QDir d2(WPMUtils::getShellDir(FOLDERID_CommonStartMenu));
			WPMUtils::deleteShortcuts(dir, d2);
			sub->completeWithProgress();
		}
    }

    if (desktop) {
        Job* sub = job->newSubJob(0.33, QObject::tr("Desktop"));
        QDir d3(WPMUtils::getShellDir(FOLDERID_Desktop));
        WPMUtils::deleteShortcuts(dir, d3);

        if (PackageUtils::globalMode)
		{
            QDir d4(WPMUtils::getShellDir(FOLDERID_PublicDesktop));
			WPMUtils::deleteShortcuts(dir, d4);
			sub->completeWithProgress();
		}
    }

    if (quickLaunch) {
        Job* sub = job->newSubJob(0.33, QObject::tr("Quick launch bar"));
        const char* A = "\\Microsoft\\Internet Explorer\\Quick Launch";
        QDir d3(WPMUtils::getShellDir(FOLDERID_RoamingAppData) + A);
        WPMUtils::deleteShortcuts(dir, d3);

        if (PackageUtils::globalMode)
		{
            QDir d4(WPMUtils::getShellDir(FOLDERID_ProgramData) + A);
			WPMUtils::deleteShortcuts(dir, d4);
			sub->completeWithProgress();
		}
    }

    job->setProgress(1);
    job->complete();
}

void PackageVersion::uninstall(Job* job, bool printScriptOutput,
        DWORD programCloseType, std::vector<QString>* stoppedServices)
{
    if (!installed()) {
        job->setProgress(1);
        job->complete();
        return;
    }

    QString initialTitle = job->getTitle();

    QString where = getPath();

    QDir d(getPath());

    std::future<void> deleteShortcutsFuture;
    if (job->getErrorMessage().isEmpty()) {
        Job* deleteShortcutsJob = job->newSubJob(0,
                QObject::tr("Deleting shortcuts"), false);
        deleteShortcutsFuture = std::async(std::launch::async,
            [this, d, deleteShortcutsJob](){
                this->deleteShortcutsRunnable(
                    d.absolutePath(), deleteShortcutsJob, true, false, false);
            });
    }

    // Inno Setup, NSIS
    if (job->shouldProceed() && this->type == PackageVersion::Type::INNO_SETUP) {
        Job* exec = job->newSubJob(0.29,
                QObject::tr("Running the Inno Setup removal (this may take some time)"),
                true, true);
        if (!d.exists(".Npackd"))
            d.mkdir(".Npackd");

        std::vector<QString> env;

        QString err = addBasicVars(&env);
        if (!err.isEmpty())
            job->setErrorMessage(err);

        addDependencyVars(&env);

        // Inno Setup log file
        QString innoSetupLog = WPMUtils::createEmptyTempFile("NpackdXXXXXX.log");

        // run the removal
        QString dir = WPMUtils::normalizePath(d.absolutePath());
        QString fullBinary = WPMUtils::normalizePath(where, false) + "\\unins000.exe";


        Job* sub = job->newSubJob(0.01, QObject::tr("Acquire installation script lock"), true, true);
        bool acquired = AbstractRepository::lockInstallationScript(sub);
        WPMUtils::executeFile(exec, dir,
                fullBinary,
                "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /LOG=\"" + innoSetupLog + "\"",
                QString(), env, false, printScriptOutput, false);
        if (acquired)
            AbstractRepository::unlockInstallationScript();

        // copy the Inno Setup log
        // ignore the errors. The log is not as important as the package removal.
        WPMUtils::appendFile(innoSetupLog, WPMUtils::getMessagesLog());

        if (!job->getErrorMessage().isEmpty()) {
            QString text, error;
            std::tie(text, error) = WPMUtils::readLastLines(innoSetupLog);

            if (error.isEmpty()) {
                job->setErrorMessage((QObject::tr("%1. Full output was saved in %2") +
                        "\r\n" +
                        QObject::tr("The last lines of the output from the Inno Setup log file:") +
                        "\r\n...\r\n%3").arg(
                        job->getErrorMessage(), WPMUtils::getMessagesLog(), text));
            }
        }

        QFile::remove(innoSetupLog);
    } else if (job->shouldProceed() && this->type == PackageVersion::Type::NSIS) {
        Job* exec = job->newSubJob(0.29,
                QObject::tr("Running the NSIS removal (this may take some time)"),
                true, true);
        if (!d.exists(".Npackd"))
            d.mkdir(".Npackd");

        std::vector<QString> env;

        QString err = addBasicVars(&env);
        if (!err.isEmpty())
            job->setErrorMessage(err);

        addDependencyVars(&env);

        // run the removal
        QString dir = WPMUtils::normalizePath(d.absolutePath());
        QString fullBinary = WPMUtils::normalizePath(where, false) + "\\uninstall.exe";

        Job* sub = job->newSubJob(0.01, QObject::tr("Acquire installation script lock"), true, true);
        bool acquired = AbstractRepository::lockInstallationScript(sub);
        WPMUtils::executeFile(exec, dir,
                fullBinary, "/S _?=" + dir,
                QString(), env, false, printScriptOutput, false);
        if (acquired)
            AbstractRepository::unlockInstallationScript();

        if (!job->getErrorMessage().isEmpty()) {
            job->setErrorMessage((QObject::tr("%1. Full output was saved in %2") +
                    "\r\n").arg(
                    job->getErrorMessage(), WPMUtils::getMessagesLog()));
        }
    } else {
        job->setProgress(0.3);
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

    if (job->shouldProceed()) {
        if (!uninstallationScript.isEmpty()) {
            if (!d.exists(".Npackd"))
                d.mkdir(".Npackd");
            Job* sub = job->newSubJob(0.44,
                    QObject::tr("Running the uninstallation script (this may take some time)"),
                    true, true);

            // prepare the environment variables

            std::vector<QString> env;
            QString err = addBasicVars(&env);
            if (!err.isEmpty())
                job->setErrorMessage(err);

            addDependencyVars(&env);

            // some uninstallers delete all files in the directory including
            // the un-installation script. cmd.exe returns an error code in
            // this case and stops the script execution. We try to prevent
            // this by locking the Uninstall.bat script
            QString usfn = d.absolutePath() + "\\" + uninstallationScript;
            HANDLE ush = CreateFile(WPMUtils::toLPWSTR(usfn),
                    GENERIC_READ,
                    FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);

            /* debugging
            if (ush != INVALID_HANDLE_VALUE) {
                qCDebug(npackd) << "file handle is OK";
            }
            */

            Job* sub2 = job->newSubJob(0.01, QObject::tr("Acquire installation script lock"), true, true);
            bool acquired = AbstractRepository::lockInstallationScript(sub2);
            this->executeFile2(sub, d.absolutePath(), usfn,
                    env, printScriptOutput, true);
            if (acquired)
                AbstractRepository::unlockInstallationScript();

            if (ush != INVALID_HANDLE_VALUE) {
                CloseHandle(ush);
            }
        }
        job->setProgress(0.45);
    }

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
            removeDirectory(rjob, d.absolutePath(), programCloseType,
                    stoppedServices);

            d.refresh();
            if (d.exists()) {
                qCWarning(npackd).noquote() << QObject::tr(
                        "Failed to delete the package directory \"%1\": %2").
                        arg(d.absolutePath(), rjob->getErrorMessage());
            }

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
            InstalledPackages::getDefault()->updateNpackdCLEnvVar();
        }
        job->setProgress(1);
    }
    job->setTitle(initialTitle);

    deleteShortcutsFuture.wait();

    if (success)
        qCInfo(npackd).noquote() << QObject::tr(
                "The package %1 was removed successfully from \"%2\"").
                arg(this->toString(true), where);
    else
        qCCritical(npackd).noquote() << QObject::tr(
                "The removal of the package %1 from \"%2\" failed: %3").
                arg(this->toString(true), where, job->getErrorMessage());

    job->complete();
}

void PackageVersion::removeDirectory(Job* job, const QString& dir,
        DWORD programCloseType, std::vector<QString>* stoppedServices)
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
            WPMUtils::removeDirectory(sub, d.absolutePath());
        } else {
            break;
        }

        d.refresh();
        if (d.exists()) {
            WPMUtils::closeProcessesThatUseDirectory(dir, programCloseType,
                    stoppedServices);
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

QString PackageVersion::getFileExtension()
{
    if (this->download.isValid()) {
        QString fn = this->download.path();
        std::vector<QString> parts = WPMUtils::split(fn, '/');
        QString file = parts.back();
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
                        arg(this->download.toString(),
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

std::vector<PackageVersion*> PackageVersion::getRemovePackageVersionOptions(const CommandLine& cl,
        QString* err)
{
    std::vector<PackageVersion*> ret;
    std::vector<CommandLine::ParsedOption *> pos = cl.getParsedOptions();

    DBRepository* rep = DBRepository::getDefault();

    for (int i = 0; i < static_cast<int>(pos.size()); i++) {
        if (!err->isEmpty())
            break;

        CommandLine::ParsedOption* po = pos.at(i);
        if (po->opt && po->opt->nameMatches("package")) {
            CommandLine::ParsedOption* ponext = nullptr;
            if (i + 1 < static_cast<int>(pos.size()))
                ponext = pos.at(i + 1);

            QString package = po->value;
            if (!Package::isValidName(package)) {
                *err = QObject::tr("Invalid package name: %1").arg(package);
            }

            Package* p = nullptr;
            if (err->isEmpty()) {
                p = rep->findOnePackage(package, err);
                if (err->isEmpty()) {
                    if (!p)
                        *err = QObject::tr("Unknown package: %1").arg(package);
                }
            }

            PackageVersion* pv = nullptr;
            if (err->isEmpty()) {
                QString version;
                if (ponext != nullptr && ponext->opt && ponext->opt->nameMatches("version"))
                    version = ponext->value;
                if (version.isNull()) {
                    std::vector<InstalledPackageVersion*> ipvs =
                            InstalledPackages::getDefault()->getByPackage(p->name);
                    if (ipvs.size() == 0) {
                        *err = QObject::tr(
                                "Package %1 (%2) is not installed").
                                arg(p->title, p->name);
                    } else if (ipvs.size() > 1) {
                        QString vns;
                        for (auto ipv: ipvs) {
                            if (!vns.isEmpty())
                                vns.append(", ");
                            vns.append(ipv->version.getVersionString());
                        }
                        *err = QObject::tr(
                                "More than one version of the package %1 (%2) "
                                "is installed: %3").arg(p->title, p->name,
                                vns);
                    } else {
                        pv = rep->findPackageVersion_(p->name,
                                ipvs.at(0)->version, err);
                        if (err->isEmpty()) {
                            if (!pv) {
                                *err = QObject::tr("Package version not found: %1 (%2) %3").
                                        arg(p->title, p->name, version);
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
                ret.push_back(pv);

            delete p;
        }
    }

    return ret;
}

std::vector<InstalledPackageVersion*> PackageVersion::getPathPackageVersionOptions(const CommandLine& cl,
        QString* err)
{
    std::vector<InstalledPackageVersion*> ret;
    std::vector<CommandLine::ParsedOption *> pos = cl.getParsedOptions();

    InstalledPackages* ip = InstalledPackages::getDefault();
    DBRepository* dbr = DBRepository::getDefault();

    for (int i = 0; i < static_cast<int>(pos.size()); i++) {
        if (!err->isEmpty())
            break;

        CommandLine::ParsedOption* po = pos.at(i);
        if (po->opt && po->opt->nameMatches("package")) {
            CommandLine::ParsedOption* ponext = nullptr;
            if (i + 1 < static_cast<int>(pos.size()))
                ponext = pos.at(i + 1);

            QString package = po->value;
            if (!Package::isValidName(package)) {
                *err = QObject::tr("Invalid package name: %1").arg(package);
            }

            Package* p = nullptr;
            if (err->isEmpty()) {
                p = dbr->findOnePackage(package, err);
                if (err->isEmpty()) {
                    if (!p)
                        *err = QObject::tr("Unknown package: %1").arg(package);
                }
            }

            InstalledPackageVersion* ipv = nullptr;
            if (err->isEmpty()) {
                QString version;
                if (ponext != nullptr && ponext->opt && ponext->opt->nameMatches("version"))
                    version = ponext->value;

                QString versions;
                if (ponext != nullptr && ponext->opt && ponext->opt->nameMatches("versions"))
                    versions = ponext->value;

                if (!versions.isNull()) {
                    i++;
                    Dependency v;
                    v.package = p->name;
                    if (!v.setVersions(versions)) {
                        *err = QObject::tr("Cannot parse a version range: %1").
                                arg(versions);
                    } else {
                        ipv = ip->findHighestInstalledMatch(v);
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
                ret.push_back(ipv);

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

    QString sourceBasePath = PackageUtils::globalMode
        ? WPMUtils::getShellDir(FOLDERID_ProgramData) + "\\Npackd\\Commands\\"
        : WPMUtils::getShellDir(FOLDERID_RoamingAppData) + "\\Npackd\\Commands\\";

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

    if (!d.exists(sourceBasePath))
        d.mkpath(sourceBasePath);

    if (errMsg->isEmpty()) {
        if (!d.exists(sourceBasePath))
            d.mkpath(sourceBasePath);

        for (int i = 0; i < static_cast<int>(this->cmdFiles.size()); i++) {
            QString cmdFilePath = this->cmdFiles.at(i);
            cmdFilePath.replace('/', '\\');

            QString cmdFileName = getCmdFileName(i);
            QString cmdFileNameLC = cmdFileName.toLower();

            std::vector<PackageVersion*> pvs = dbr->findPackageVersionsWithCmdFile(
                    cmdFileName, errMsg);
            if (pvs.size() > 0) {
                PackageVersion* last = nullptr;
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
                        for (int j = 0; j < static_cast<int>(last->cmdFiles.size()); j++) {
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
                            nullptr,
                            std::vector<QString>());
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
    for (int i = 0; i < static_cast<int>(this->importantFiles.size()); i++) {
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
        QString commonStartMenu = PackageUtils::globalMode
            ? WPMUtils::getShellDir(FOLDERID_CommonStartMenu)
            : WPMUtils::getShellDir(FOLDERID_StartMenu);
        simple = commonStartMenu + "\\" + simple;
        withVersion = commonStartMenu + "\\" + withVersion;

        QString from;
        if (QFileInfo::exists(simple))
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
                WPMUtils::toLPWSTR(path.replace('/', '\\')),
                WPMUtils::toLPWSTR(from),
                WPMUtils::toLPWSTR(desc),
                WPMUtils::toLPWSTR(workingDir));

        if (!r.isEmpty()) {
            *errMsg = QString(QObject::tr("Shortcut creation from %1 to %2 failed: %3")).
                    arg(from, path, r);
            break;
        }
    }
    delete p;

    return errMsg->isEmpty();
}

QString PackageVersion::getIdealInstallationDirectory() const
{
    return WPMUtils::normalizePath(
            PackageUtils::getInstallationDirectory() + "\\" +
            WPMUtils::makeValidFilename(this->getPackageTitle(), '_'),
            false);
}

QString PackageVersion::getSecondaryInstallationDirectory() const
{
    return WPMUtils::normalizePath(
            PackageUtils::getInstallationDirectory() + "\\" +
            WPMUtils::makeValidFilename(this->getPackageTitle(), '_') +
            "-" + this->version.getVersionString(),
            false);
}

QString PackageVersion::getPreferredInstallationDirectory()
{
    QString name = WPMUtils::normalizePath(
            PackageUtils::getInstallationDirectory() + "\\" +
            WPMUtils::makeValidFilename(this->getPackageTitle(), '_'),
            false);
    if (!QFileInfo::exists(name))
        return name;
    else
        return WPMUtils::findNonExistingFile(name + "-" +
                this->version.getVersionString(), "");
}

QString PackageVersion::download_(Job* job, const QString& where,
        bool interactive, const QString &user, const QString &password,
        const QString &proxyUser, const QString &proxyPassword)
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

        time_t start = time(nullptr);
        while (!job->isCancelled()) {
            httpConnectionAcquired = httpConnections.tryAcquire(1, 10000);
            if (httpConnectionAcquired) {
                job->setProgress(0.05);
                break;
            }

            time_t seconds = time(nullptr) - start;
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
            request.proxyUser = proxyUser;
            request.proxyPassword = proxyPassword;
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
                request.proxyUser = proxyUser;
                request.proxyPassword = proxyPassword;
                request.alg = this->hashSumType;
                request.interactive = interactive;
                Downloader::Response response =
                        Downloader::download(djob, request);
                dsha1 = response.hashSum;
                if (!djob->getErrorMessage().isEmpty())
                    job->setErrorMessage(QObject::tr("Error downloading %1: %2").
                        arg(this->download.toString(),
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
                        QObject::tr("Hash sum %1 found, but %2 was expected. The file has changed.")).arg(dsha1,
                        this->sha1));
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

    QString binary;
    if (job->shouldProceed()) {
        if (this->type == PackageVersion::Type::ZIP) {
            Job* djob = job->newSubJob(0.06, QObject::tr("Extracting files"));
            WPMUtils::unzip(djob, f->fileName(), d.absolutePath() + "\\");
            if (!djob->getErrorMessage().isEmpty())
                job->setErrorMessage(QString(
                        QObject::tr("Error unzipping file into directory %0: %1")).
                        arg(d.absolutePath(),
                        djob->getErrorMessage()));
            else if (!job->isCancelled())
                job->setProgress(0.98);
        } else {
            job->setTitle(initialTitle + " / " +
                    QObject::tr("Renaming the downloaded file"));
            QString t = d.absolutePath();
            t.append("\\");
            QString fn = this->download.path();
            std::vector<QString> parts = WPMUtils::split(fn, '/');
            if (parts.size() > 0 && parts.back().length() > 0)
                t.append(parts.back());
            else
                t.append("data");
            t.replace('/', '\\');

            binary = t;

            if (!QFile::rename(f->fileName(), t)) {
                job->setErrorMessage(QString(QObject::tr("Cannot rename %0 to %1")).
                        arg(f->fileName(), t));
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

void PackageVersion::installWith(Job* /*job*/)
{

}

void PackageVersion::install(Job* job, const QString& where,
        const QString& binary,
        bool printScriptOutput, DWORD programCloseType,
        std::vector<QString>* stoppedServices)
{
    if (installed()) {
        qCInfo(npackd).noquote() << QObject::tr(
                "The package %1 is already installed in \"%2\"").
                arg(this->toString(true), getPath());
        job->setProgress(1);
        job->complete();
        return;
    }

    InstalledPackages* ip = InstalledPackages::getDefault();

    QString initialTitle = job->getTitle();

    // qCDebug(npackd) << "install.2";
    QDir d(where);

    if (job->shouldProceed() && this->dependencies.size() > 0) {
        Job* sjob = job->newSubJob(0.2,
                QObject::tr("Running the installation hooks for dependencies"),
                true, true);
        installWith(sjob);
    } else {
        job->setProgress(0.2);
    }

    // Inno Setup, NSIS
    if (job->shouldProceed() && this->type == PackageVersion::Type::INNO_SETUP) {
        Job* exec = job->newSubJob(0.29,
                QObject::tr("Running the Inno Setup installation (this may take some time)"),
                true, true);
        if (!d.exists(".Npackd"))
            d.mkdir(".Npackd");

        std::vector<QString> env;
        env.push_back("NPACKD_PACKAGE_BINARY");
        env.push_back(WPMUtils::normalizePath(where, false) + "\\" + binary);

        QString err = addBasicVars(&env);
        if (!err.isEmpty())
            job->setErrorMessage(err);

        addDependencyVars(&env);

        // Inno Setup log file
        QString innoSetupLog = WPMUtils::createEmptyTempFile("NpackdXXXXXX.log");

        // run the installation
        QString dir = WPMUtils::normalizePath(d.absolutePath());
        QString fullBinary = WPMUtils::normalizePath(where, false) + "\\" + binary;

        Job* sub = job->newSubJob(0.01, QObject::tr("Acquire installation script lock"), true, true);
        bool acquired = AbstractRepository::lockInstallationScript(sub);
        WPMUtils::executeFile(exec, dir,
                fullBinary,
                "/SP- /VERYSILENT /SUPPRESSMSGBOXES /NOCANCEL /NORESTART /DIR=\"" + dir +
                "\" /SAVEINF=\".Npackd\\InnoSetupInfo.ini\" /LOG=\"" + innoSetupLog + "\"",
                QString(), env, false, printScriptOutput, false);
        if (acquired)
            AbstractRepository::unlockInstallationScript();

        if (job->shouldProceed()) {
            // ignore the errors here
            QFile::remove(fullBinary);
        }

        // copy the Inno Setup log
        // ignore the errors. The log is not as important as the package installation.
        WPMUtils::appendFile(innoSetupLog, WPMUtils::getMessagesLog());

        if (!job->getErrorMessage().isEmpty()) {
            QString text, error;
            std::tie(text, error) = WPMUtils::readLastLines(innoSetupLog);

            if (error.isEmpty()) {
                job->setErrorMessage((QObject::tr("%1. Full output was saved in %2") +
                        "\r\n" +
                        QObject::tr("The last lines of the output from the Inno Setup log file:") +
                        "\r\n...\r\n%3").arg(
                        job->getErrorMessage(), WPMUtils::getMessagesLog(), text));
            }
        }

        QFile::remove(innoSetupLog);
    } else if (job->shouldProceed() && this->type == PackageVersion::Type::NSIS) {
        Job* exec = job->newSubJob(0.29,
                QObject::tr("Running the NSIS installation (this may take some time)"),
                true, true);
        if (!d.exists(".Npackd"))
            d.mkdir(".Npackd");

        std::vector<QString> env;
        env.push_back("NPACKD_PACKAGE_BINARY");
        env.push_back(WPMUtils::normalizePath(where, false) + "\\" + binary);

        QString err = addBasicVars(&env);
        if (!err.isEmpty())
            job->setErrorMessage(err);

        addDependencyVars(&env);

        // run the installation
        QString dir = WPMUtils::normalizePath(d.absolutePath());
        QString fullBinary = WPMUtils::normalizePath(where, false) + "\\" + binary;

        Job* sub = job->newSubJob(0.01, QObject::tr("Acquire installation script lock"), true, true);
        bool acquired = AbstractRepository::lockInstallationScript(sub);
        WPMUtils::executeFile(exec, dir,
                fullBinary, "/S /D=" + dir,
                QString(), env, false, printScriptOutput, false);
        if (acquired)
            AbstractRepository::unlockInstallationScript();

        if (job->shouldProceed()) {
            // ignore the errors here
            QFile::remove(fullBinary);
        }

        if (!job->getErrorMessage().isEmpty()) {
            job->setErrorMessage((QObject::tr("%1. Full output was saved in %2") +
                    "\r\n").arg(
                    job->getErrorMessage(), WPMUtils::getMessagesLog()));
        }
    } else {
        job->setProgress(0.3);
    }

    if (job->shouldProceed()) {
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

        if (!installationScript.isEmpty()) {
            Job* exec = job->newSubJob(0.59,
                    QObject::tr("Running the installation script (this may take some time)"),
                    true, true);
            if (!d.exists(".Npackd"))
                d.mkdir(".Npackd");

            std::vector<QString> env;
            env.push_back("NPACKD_PACKAGE_BINARY");
            env.push_back(WPMUtils::normalizePath(where, false) + "\\" + binary);

            QString err = addBasicVars(&env);
            if (!err.isEmpty())
                job->setErrorMessage(err);

            addDependencyVars(&env);

            Job* sub = job->newSubJob(0.01, QObject::tr("Acquire installation script lock"), true, true);
            bool acquired = AbstractRepository::lockInstallationScript(sub);
            this->executeFile2(exec, d.absolutePath(),
                    d.absolutePath() + "\\" + installationScript,
                    env, printScriptOutput, true);
            if (acquired)
                AbstractRepository::unlockInstallationScript();

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
            ip->updateNpackdCLEnvVar();
        }

        job->setProgress(0.91);
    }
    job->setTitle(initialTitle);

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
        QString err = ip->setPackageVersionPath(
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
        removeDirectory(rjob, d.absolutePath(), programCloseType,
                stoppedServices);
    }

    if (success) {
        qCInfo(npackd).noquote() << QObject::tr(
                "The package %1 was installed successfully in \"%2\"").
                arg(this->toString(true), where);
    } else {
        qCCritical(npackd).noquote() << QObject::tr(
                "The installation of the package %1 in \"%2\" failed: %3").
                arg(this->toString(true), where, job->getErrorMessage());
    }

    if (job->shouldProceed()) {
        job->setProgress(1);
    }

    job->complete();
}

QString PackageVersion::addBasicVars(std::vector<QString>* env)
{
    QString err;
    env->push_back("NPACKD_PACKAGE_NAME");
    env->push_back(this->package);
    env->push_back("NPACKD_PACKAGE_VERSION");
    env->push_back(this->version.getVersionString());
    env->push_back("NPACKD_CL");
    env->push_back(InstalledPackages::getDefault()->
            computeNpackdCLEnvVar_(&err));

    return err;
}

void PackageVersion::addDependencyVars(std::vector<QString>* vars)
{
    InstalledPackages* ip = InstalledPackages::getDefault();
    for (auto d: this->dependencies) {
        if (!d->var.isEmpty()) {
            vars->push_back(d->var);
            InstalledPackageVersion* ipv = ip->findHighestInstalledMatch(*d);
            if (ipv) {
                vars->push_back(ipv->getDirectory());
                delete ipv;
            } else {
                // this could happen if a package was un-installed manually
                // without Npackd or the repository has changed after this
                // package was installed
                vars->push_back("");
            }
        }
    }
}


QString PackageVersion::saveFiles(const QDir& d)
{
    QString res;
    for (auto f: this->files) {
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

QString PackageVersion::getStatus(DBRepository* r) const
{
    QString status;
    bool installed = this->installed();
    QString err;
    PackageVersion* newest = r->findNewestInstallablePackageVersion_(
            this->package, &err);
    if (installed) {
        status = QObject::tr("installed");
    }
    if (installed && newest != nullptr && version.compare(newest->version) < 0) {
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

void PackageVersion::build(Job* job, const QString& outputPackage,
        const QString& outputDir,
    bool printScriptOutput)
{
    // prepare the environment variables
    std::vector<QString> env;
    QString err = addBasicVars(&env);
    if (!err.isEmpty())
        job->setErrorMessage(err);
    env.push_back("NPACKD_PACKAGE_DIR");
    env.push_back(this->getPath());
    env.push_back("NPACKD_OUTPUT_PACKAGE");
    env.push_back(outputPackage);
    addDependencyVars(&env);

    QString filename = ".Npackd\\Build.bat";
    QFileInfo d(this->getPath() + "\\" + filename);
    if (!d.exists() || !d.isFile()) {
        job->setErrorMessage(QObject::tr(
                "%1 is missing or is not a directory").arg(d.absoluteFilePath()));
        job->complete();
    } else {
        executeFile2(job, outputDir, this->getPath() + "\\" + filename,
                env, printScriptOutput, false);
    }
}

void PackageVersion::executeFile2(Job* job, const QString& where,
        const QString& path,
        const std::vector<QString>& env, bool printScriptOutput, bool unicode)
{
    QString outputFile = WPMUtils::getMessagesLog();
    WPMUtils::executeBatchFile(
            job, where, path, outputFile, env, printScriptOutput, unicode);

    if (!job->getErrorMessage().isEmpty()) {
        QString text, error;
        std::tie(text, error) = WPMUtils::readLastLines(outputFile);
        if (error.isEmpty()) {
            job->setErrorMessage((QObject::tr("%1. Full output was saved in %2") +
                    "\r\n" +
                    QObject::tr("The last lines of the output:") +
                    "\r\n...\r\n%3").arg(
                    job->getErrorMessage(), outputFile, text));
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
    for (auto f: this->files) {
        r->files.push_back(f->clone());
    }
    for (auto d: this->dependencies) {
        r->dependencies.push_back(d->clone());
    }

    r->type = this->type;
    r->sha1 = this->sha1;
    r->hashSumType = this->hashSumType;
    r->download = this->download;

    return r;
}

PackageVersion *PackageVersion::parse(QByteArray &xml, QString *err,
        bool /*validate*/)
{
    PackageVersion* r = nullptr;

    QBuffer buf(&xml);
    buf.open(QIODevice::ReadOnly);

    Repository rep;
    QXmlStreamReader reader(&buf);
    RepositoryXMLHandler handler(&rep, QUrl(), &reader);
    *err = handler.parseTopLevelVersion();
    if (err->isEmpty()) {
        if (rep.packageVersions.size() == 1) {
            r = rep.packageVersions.at(0);
            rep.packageVersions.erase(rep.packageVersions.begin());
            *err = "";
        } else {
            *err = QObject::tr("Expected one package version");
        }
    }
    return r;
}

bool PackageVersion::contains(const std::vector<PackageVersion *> &list,
        PackageVersion *pv)
{
    return indexOf(list, pv) >= 0;
}

void PackageVersion::toXML(QXmlStreamWriter *w) const
{
    w->writeStartElement("version");
    w->writeAttribute("name", this->version.getVersionString());
    w->writeAttribute("package", this->package);
    if (this->type == PackageVersion::Type::ONE_FILE)
        w->writeAttribute("type", "one-file");
    else if (this->type == PackageVersion::Type::INNO_SETUP)
        w->writeAttribute("type", "inno-setup");
    else if (this->type == PackageVersion::Type::NSIS)
        w->writeAttribute("type", "nsis");
    for (int i = 0; i < static_cast<int>(this->importantFiles.size()); i++) {
        w->writeStartElement("important-file");
        w->writeAttribute("path", this->importantFiles.at(i));
        w->writeAttribute("title", this->importantFilesTitles.at(i));
        w->writeEndElement();
    }
    for (auto& f: this->cmdFiles) {
        w->writeStartElement("cmd-file");
        //qCDebug(npackd) << this->package << this->version.getVersionString() <<
        //    this->cmdFiles.at(i) << "!";
        w->writeAttribute("path", f);
        w->writeEndElement();
    }
    for (auto f: this->files) {
        w->writeStartElement("file");
        w->writeAttribute("path", f->path);
        w->writeCharacters(f->content);
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
    for (auto d: this->dependencies) {
        w->writeStartElement("dependency");
        w->writeAttribute("package", d->package);
        w->writeAttribute("versions", d->versionsToString());
        if (!d->var.isEmpty())
            w->writeTextElement("variable", d->var);
        w->writeEndElement();
    }
    /*
    XML is never shown on the console
    if (!this->getPath().isEmpty()) {
        w->writeTextElement("installed", this->getPath());
    }
    */
    w->writeEndElement();
}

void PackageVersion::toJSON(QJsonObject& w) const
{
    w["name"] = this->version.getVersionString();
    w["package"] = this->package;
    if (this->type == PackageVersion::Type::ONE_FILE)
        w["type"] = "one-file";
    else if (this->type == PackageVersion::Type::INNO_SETUP)
        w["type"] = "inno-setup";
    else if (this->type == PackageVersion::Type::NSIS)
        w["type"] = "nsis";

    if (importantFiles.size() != 0) {
        QJsonArray a;
        for (int i = 0; i < static_cast<int>(this->importantFiles.size()); i++) {
            QJsonObject obj;
            obj["path"] = this->importantFiles.at(i);
            obj["title"] = this->importantFilesTitles.at(i);
            a.append(obj);
        }
        w["importantFiles"] = a;
    }

    if (cmdFiles.size() != 0) {
        QJsonArray a;
        for (auto& f: this->cmdFiles) {
            QJsonObject obj;
            obj["path"] = f;
            a.append(obj);
        }
        w["cmdFiles"] = a;
    }

    if (files.size() > 0) {
        QJsonArray path;
        for (auto f: this->files) {
            QJsonObject obj;
            obj["path"] = f->path;
            obj["content"] = f->content;
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

    if (dependencies.size() > 0) {
        QJsonArray dependency;
        for (auto d: this->dependencies) {
            QJsonObject obj;
            obj["package"] = d->package;
            obj["versions"] = d->versionsToString();
            if (!d->var.isEmpty())
                obj["variable"] = d->var;
            dependency.append(obj);
        }
        w["dependencies"] = dependency;
    }

    if (!this->getPath().isEmpty()) {
        w["installed"] = this->getPath();
    }
}

PackageVersionFile* PackageVersion::findFile(const QString& path) const
{
    PackageVersionFile* r = nullptr;
    QString lowerPath = path.toLower();
    for (auto pvf: this->files) {
        if (pvf->path.toLower() == lowerPath) {
            r = pvf;
            break;
        }
    }
    return r;
}

void PackageVersion::stop(Job* job, DWORD programCloseType,
        bool printScriptOutput, std::vector<QString>* stoppedServices)
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
        Job* exec = job->newSubJob(0.49,
                QObject::tr("Executing the stop script"), true, true);
        if (!d.exists(".Npackd"))
            d.mkdir(".Npackd");

        std::vector<QString> env;
        QString err = addBasicVars(&env);
        if (!err.isEmpty())
            job->setErrorMessage(err);

        addDependencyVars(&env);

        Job* sub = job->newSubJob(0.01, QObject::tr("Acquire the installation script lock"), true, true);
        bool acquired = AbstractRepository::lockInstallationScript(sub);
        if (job->shouldProceed())
            this->executeFile2(exec, d.absolutePath(),
                    d.absolutePath() + "\\.Npackd\\Stop.bat",
                    env,
                    printScriptOutput, true);
        if (acquired)
            AbstractRepository::unlockInstallationScript();
    } else {
        WPMUtils::closeProcessesThatUseDirectory(getPath(), programCloseType,
                stoppedServices);

        job->setProgress(0.5);
    }

    if (job->getErrorMessage().isEmpty())
        job->setProgress(1);

    job->complete();
}
