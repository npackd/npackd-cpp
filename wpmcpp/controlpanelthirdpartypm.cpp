#include <QUrl>
#include <QDebug>

#include "controlpanelthirdpartypm.h"
#include "windowsregistry.h"
#include "wpmutils.h"

ControlPanelThirdPartyPM::ControlPanelThirdPartyPM()
{
}

void ControlPanelThirdPartyPM::scan(Job* job,
        QList<InstalledPackageVersion*>* installed,
        Repository *rep) const
{
    detectControlPanelProgramsFrom(installed, rep, HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
            false
    );
    if (WPMUtils::is64BitWindows()) {
        detectControlPanelProgramsFrom(installed, rep, HKEY_LOCAL_MACHINE,
                "SOFTWARE\\WoW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                false
        );
    }
    detectControlPanelProgramsFrom(installed, rep, HKEY_CURRENT_USER,
            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
            false
    );
    if (WPMUtils::is64BitWindows()) {
        detectControlPanelProgramsFrom(installed, rep, HKEY_CURRENT_USER,
                "SOFTWARE\\WoW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                false
        );
    }

    job->setProgress(1);
    job->complete();
}

void ControlPanelThirdPartyPM::
        detectControlPanelProgramsFrom(QList<InstalledPackageVersion*>* installed,
        Repository* rep, HKEY root,
        const QString& path, bool useWoWNode) const {
    WindowsRegistry wr;
    QString err;
    err = wr.open(root, path, useWoWNode, KEY_READ);
    if (err.isEmpty()) {
        QString fullPath;
        if (root == HKEY_CLASSES_ROOT)
            fullPath = "HKEY_CLASSES_ROOT";
        else if (root == HKEY_CURRENT_USER)
            fullPath = "HKEY_CURRENT_USER";
        else if (root == HKEY_LOCAL_MACHINE)
            fullPath = "HKEY_LOCAL_MACHINE";
        else if (root == HKEY_USERS)
            fullPath = "HKEY_USERS";
        else if (root == HKEY_PERFORMANCE_DATA)
            fullPath = "HKEY_PERFORMANCE_DATA";
        else if (root == HKEY_CURRENT_CONFIG)
            fullPath = "HKEY_CURRENT_CONFIG";
        else if (root == HKEY_DYN_DATA)
            fullPath = "HKEY_DYN_DATA";
        else
            fullPath = QString("%1").arg((uintptr_t) root);
        fullPath += "\\" + path;

        QStringList entries = wr.list(&err);
        for (int i = 0; i < entries.count(); i++) {
            WindowsRegistry k;
            err = k.open(wr, entries.at(i), KEY_READ);
            if (err.isEmpty()) {
                detectOneControlPanelProgram(installed, rep,
                        fullPath + "\\" + entries.at(i),
                        k, entries.at(i));
            }
        }
    }
}

void ControlPanelThirdPartyPM::detectOneControlPanelProgram(
        QList<InstalledPackageVersion*>* installed,
        Repository *rep,
        const QString& registryPath,
        WindowsRegistry& k,
        const QString& keyName) const
{
    // find the package name
    QString package = keyName;
    package.replace('.', '_');
    package = WPMUtils::makeValidFullPackageName(
            "control-panel." + package);

    // find the version number
    bool versionFound = false;
    Version version;
    QString err;
    QString version_ = k.get("DisplayVersion", &err);
    if (err.isEmpty()) {
        version.setVersion(version_);
        version.normalize();
        versionFound = true;
    }
    if (!versionFound) {
        DWORD major = k.getDWORD("VersionMajor", &err);
        if (err.isEmpty()) {
            DWORD minor = k.getDWORD("VersionMinor", &err);
            if (err.isEmpty())
                version.setVersion(major, minor);
            else
                version.setVersion(major, 0);
            version.normalize();
            versionFound = true;
        }
    }
    if (!versionFound) {
        QString major = k.get("VersionMajor", &err);
        if (err.isEmpty()) {
            QString minor = k.get("VersionMinor", &err);
            if (err.isEmpty()) {
                if (version.setVersion(major)) {
                    versionFound = true;
                    version.normalize();
                }
            } else {
                if (version.setVersion(major + "." + minor)) {
                    versionFound = true;
                    version.normalize();
                }
            }
        }
    }
    if (!versionFound) {
        QString displayName = k.get("DisplayName", &err);
        if (err.isEmpty()) {
            QStringList parts = displayName.split(' ');
            if (parts.count() > 1 && parts.last().contains('.')) {
                version.setVersion(parts.last());
                version.normalize();
                versionFound = true;
            }
        }
    }

    //qDebug() << "InstalledPackages::detectOneControlPanelProgram.0";

    QScopedPointer<Package> p(new Package(package, package));

    QString title = k.get("DisplayName", &err);
    if (!err.isEmpty() || title.isEmpty())
        title = keyName;
    p->title = title;
    p->description = "[Control Panel] " + p->title;

    QString url = k.get("URLInfoAbout", &err);
    if (!err.isEmpty() || url.isEmpty() || !QUrl(url).isValid())
        url = "";
    if (url.isEmpty())
        url = k.get("URLUpdateInfo", &err);
    if (!err.isEmpty() || url.isEmpty() || !QUrl(url).isValid())
        url = "";
    p->url = url;

    p->categories.append(QObject::tr("Control panel software"));

    // qDebug() << "adding package " << p.data()->name;
    rep->savePackage(p.data());

    QDir d;

    bool useThisEntry = true;

    QString uninstall;
    if (useThisEntry) {
        uninstall = k.get("QuietUninstallString", &err);
        if (!err.isEmpty())
            uninstall = "";
        if (uninstall.isEmpty())
            uninstall = k.get("UninstallString", &err);
        if (!err.isEmpty())
            uninstall = "";

        // some programs store in UninstallString the complete path to
        // the uninstallation program with spaces
        if (!uninstall.isEmpty() && uninstall.contains(" ") &&
                !uninstall.contains("\"") &&
                d.exists(uninstall))
            uninstall = "\"" + uninstall + "\"";

        if (uninstall.trimmed().isEmpty())
            useThisEntry = false;

        // qDebug() << uninstall;
    }

    // already detected as an MSI package
    if (uninstall.length() == 14 + 38 &&
            (uninstall.indexOf("MsiExec.exe /X", 0, Qt::CaseInsensitive) == 0 ||
            uninstall.indexOf("MsiExec.exe /I", 0, Qt::CaseInsensitive) == 0) &&
            WPMUtils::validateGUID(uninstall.right(38)) == "") {
        useThisEntry = false;
    }

    QString dir;
    if (useThisEntry) {
        dir = k.get("InstallLocation", &err);
        if (!err.isEmpty())
            dir = "";

        if (dir.isEmpty() && !uninstall.isEmpty()) {
            QStringList params = WPMUtils::parseCommandLine(uninstall, &err);
            if (err.isEmpty() && params.count() > 0 && d.exists(params[0])) {
                dir = WPMUtils::parentDirectory(params[0]);
            } /* DEBUG  else {
                qDebug() << "cannot parse " << uninstall << " " << err <<
                        " " << params.count();
                if (params.count() > 0)
                    qDebug() << "cannot parse2 " << params[0] << " " <<
                            d.exists(params[0]);
            } */
        }
    }

    if (useThisEntry) {
        if (!dir.isEmpty()) {
            dir = WPMUtils::normalizePath(dir);
        }
    }

    if (useThisEntry) {
        // qDebug() << package << version.getVersionString() << dir;
        InstalledPackageVersion* ipv = new InstalledPackageVersion(package,
                version, dir);
        ipv->detectionInfo = "control-panel:" + registryPath;
        installed->append(ipv);

        QScopedPointer<PackageVersion> pv(new PackageVersion(package));
        pv->version = version;
        PackageVersionFile* pvf = new PackageVersionFile(
                ".Npackd\\Uninstall.bat", uninstall + "\r\n");
        pv->files.append(pvf);
        rep->savePackageVersion(pv.data());
    }
}
