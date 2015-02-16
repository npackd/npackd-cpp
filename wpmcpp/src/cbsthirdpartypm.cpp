#include <QUrl>
#include <QDebug>

#include "cbsthirdpartypm.h"
#include "windowsregistry.h"
#include "wpmutils.h"


// the packages are here: C:\Windows\servicing\Packages

void CBSThirdPartyPM::scan(
        Job *job, QList<InstalledPackageVersion *> *installed,
        Repository *rep) const
{
    detectCBSPackagesFrom(installed, rep, HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\Packages",
            false
    );

    job->setProgress(1);
    job->complete();
}

void CBSThirdPartyPM::
        detectCBSPackagesFrom(QList<InstalledPackageVersion*>* installed,
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
                detectOneCBSPackage(installed, rep,
                        fullPath + "\\" + entries.at(i),
                        k, entries.at(i));
            }
        }
    }
}

void CBSThirdPartyPM::detectOneCBSPackage(
        QList<InstalledPackageVersion*>* installed,
        Repository *rep,
        const QString& registryPath,
        WindowsRegistry& k,
        const QString& keyName) const
{
    QString err;

    QStringList keyNameParts = keyName.split('~');

    if (keyNameParts.size() != 5) {
        err = "Wrong number of parts";
    }

    // find the package name
    QString package;

    if (err.isEmpty()) {
        package = keyNameParts.at(0);
        package.replace('-', '.');
        package = WPMUtils::makeValidFullPackageName(
                "cbs." + package);
    }

    if (err.isEmpty()) {
        QString title = keyNameParts.at(0);
        title.replace('-', ' ');
        QScopedPointer<Package> p(new Package(package, title));
        p->categories.append(QObject::tr("Component-Based Servicing"));
        rep->savePackage(p.data());
    }

    // find the version number
    Version version;
    if (err.isEmpty()) {
        if (version.setVersion(keyNameParts.at(4))) {
            version.normalize();
        } else {
            err = "Invalid version number";
        }
    }

    if (err.isEmpty()) {
        QScopedPointer<PackageVersion> pv(new PackageVersion(package));
        pv->version = version;
        PackageVersionFile* pvf = new PackageVersionFile(
                ".Npackd\\Uninstall.bat", "\r\n"); // TODO
        pv->files.append(pvf);
        rep->savePackageVersion(pv.data());
    }

    DWORD state = 0;
    if (err.isEmpty()) {
        state = k.getDWORD("CurrentState", &err);
    }

    bool packageInstalled = false;

    if (err.isEmpty()) {
        if (state & 0x20) {
            InstalledPackageVersion* ipv = new InstalledPackageVersion(package,
                    version, "");
            ipv->detectionInfo = "cbs:" + registryPath;
            installed->append(ipv);

            packageInstalled = true;
        }
    }

    if (!err.isEmpty()) {
        qDebug() << keyName << err;
    }

    if (err.isEmpty()) {
        WindowsRegistry updates;
        err = updates.open(k, "Updates", KEY_READ);
        if (err.isEmpty())
            detectOneCBSPackageUpdate(package, installed, rep,
                    registryPath + "\\Updates",
                    updates, packageInstalled);
    }
}

void CBSThirdPartyPM::detectOneCBSPackageUpdate(
        const QString& superPackage,
        QList<InstalledPackageVersion*>* installed,
        Repository *rep,
        const QString& registryPath,
        WindowsRegistry& k,
        bool superPackageInstalled) const
{
    QString err;
    QStringList values = k.listValues(&err);

    if (err.isEmpty()) {
        for (int i = 0; i < values.size(); i++) {
            QString name = values.at(i);
            DWORD value = k.getDWORD(name, &err);
            if (!err.isEmpty())
                break;

            QString shortPackageName = name;
            shortPackageName.replace('-', '.');
            QString packageName = WPMUtils::makeValidFullPackageName(
                    superPackage + "." + shortPackageName);

            QString title = name;
            title.replace('-', ' ');

            QScopedPointer<Package> p(new Package(packageName, title));
            p->description = title; // TODO
            p->categories.append(QObject::tr("Component-Based Servicing"));
            rep->savePackage(p.data());

            Version version;

            QScopedPointer<PackageVersion> pv(new PackageVersion(packageName));
            pv->version = version;
            PackageVersionFile* pvf = new PackageVersionFile(
                    ".Npackd\\Uninstall.bat", "\r\n"); // TODO
            pv->files.append(pvf);
            rep->savePackageVersion(pv.data());

            if (superPackageInstalled && value == 1) {
                InstalledPackageVersion* ipv = new InstalledPackageVersion(
                        packageName,
                        version, "");
                ipv->detectionInfo = "cbs:" + registryPath + "\\" + name;
                installed->append(ipv);
            }
        }
    }
}
