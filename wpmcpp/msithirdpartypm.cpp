#include <windows.h>
#include <msi.h>

#include <QApplication>

#include "msithirdpartypm.h"
#include "wpmutils.h"

QString MSIThirdPartyPM::scan(QList<InstalledPackageVersion *> *installed,
        Repository *rep) const
{
    QString err;

    QStringList all = WPMUtils::findInstalledMSIProducts();
    // qDebug() << all.at(0);

    for (int i = 0; i < all.count(); i++) {
        QString guid = all.at(i);

        QString package = "msi." + guid.mid(1, 36);

        // TODO: rep->findPackageVersionByMSIGUID_(guid)

        QString err;

        // create package version
        QScopedPointer<PackageVersion> pv(new PackageVersion(package));
        QString version_ = WPMUtils::getMSIProductAttribute(guid,
                INSTALLPROPERTY_VERSIONSTRING, &err);
        Version version;
        if (err.isEmpty()) {
            if (!version.setVersion(version_))
                version.setVersion(1, 0);
            else
                version.normalize();
        }
        pv->version = version;
        // Uninstall.bat
        PackageVersionFile* pvf = new PackageVersionFile(
                ".Npackd\\Uninstall.bat",
                "msiexec.exe /qn /norestart /Lime "
                            ".Npackd\\UninstallMSI.log /x" + guid + "\r\n" +
                            "set err=%errorlevel%" + "\r\n" +
                            "type .Npackd\\UninstallMSI.log" + "\r\n" +
                            "rem 3010=restart required" + "\r\n" +
                            "if %err% equ 3010 exit 0" + "\r\n" +
                            "if %err% neq 0 exit %err%" + "\r\n");
        pv->files.append(pvf);
        rep->savePackageVersion(pv.data());

        // create package
        QString title = WPMUtils::getMSIProductName(guid, &err);
        if (!err.isEmpty())
            title = guid;
        QScopedPointer<Package> p(new Package(pv->package, title));
        p->description = "[" + QApplication::tr("MSI database") +
                "] " + p->title + " GUID: " + guid;
        QString url = WPMUtils::getMSIProductAttribute(guid,
                INSTALLPROPERTY_URLINFOABOUT, &err);
        if (err.isEmpty() && QUrl(url).isValid())
            p->url = url;
        if (p->url.isEmpty()) {
            QString err;
            QString url = WPMUtils::getMSIProductAttribute(guid,
                    INSTALLPROPERTY_HELPLINK, &err);
            if (err.isEmpty() && QUrl(url).isValid())
                p->url = url;
        }
        rep->savePackage(p.data());

        // create InstalledPackageVersion
        QString dir = WPMUtils::getMSIProductLocation(guid, &err);
        if (!err.isEmpty())
            dir = "";
        InstalledPackageVersion* ipv = new InstalledPackageVersion(pv->package,
                pv->version, dir);
        ipv->detectionInfo = "msi:" + guid;
        installed->append(ipv);
    }

    /* TODO
    // remove uninstalled MSI packages
    QMapIterator<QString, InstalledPackageVersion*> i(this->data);
    while (i.hasNext()) {
        i.next();
        InstalledPackageVersion* ipv = i.value();
        if (ipv->detectionInfo.length() == 4 + 38 &&
                ipv->detectionInfo.left(4) == "msi:" &&
                ipv->installed() &&
                !all.contains(ipv->detectionInfo.right(38))) {
            // DEBUG qDebug() << "uninstall " << pv->package << " " <<
            // DEBUG         pv->version.getVersionString();
            ipv->setPath("");
        }
    }
    */

    return err;
}
