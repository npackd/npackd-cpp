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

    QString icon = k.get("DisplayIcon", &err);
    if (err.isEmpty())
        p->icon = WPMUtils::extractIconURL(icon);

    // Crystal Icons
    // p->icon = "data:image/png;base64,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";

    //qDebug() << "CP: adding package " << p.data()->name << p.data()->title;

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
