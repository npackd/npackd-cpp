#include <windows.h>
#include <msi.h>

#include "msithirdpartypm.h"
#include "wpmutils.h"

void MSIThirdPartyPM::scan(Job* job,
        QList<InstalledPackageVersion *> *installed,
        Repository *rep) const
{
    QStringList all = WPMUtils::findInstalledMSIProducts();
    // qDebug() << all.at(0);

    for (int i = 0; i < all.count(); i++) {
        QString guid = all.at(i);

        QString package = "msi." + guid.mid(1, 36);

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
        p->description = "[" + QObject::tr("MSI database") +
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

    job->setProgress(1);
    job->complete();
}
