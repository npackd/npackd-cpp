#include <windows.h>
#include <msi.h>
#include <QDebug>

#include "msithirdpartypm.h"
#include "wpmutils.h"
#include "dbrepository.h"

void MSIThirdPartyPM::scan(Job* job,
        QList<InstalledPackageVersion *> *installed,
        Repository *rep) const
{
    QStringList all = WPMUtils::findInstalledMSIProducts();

    DBRepository* dbr = DBRepository::getDefault();
    // qDebug() << all.at(0);

    for (int i = 0; i < all.count(); i++) {
        QString guid = all.at(i);

        QString err;
        QString package;
        Version version;

        PackageVersion* pv_ = dbr->findPackageVersionByMSIGUID_(guid, &err);
        if (pv_ != 0) {
            package = pv_->package;
            version = pv_->version;
            delete pv_;
        } else {
            package = "msi." + guid.mid(1, 36);

            // create package version
            QScopedPointer<PackageVersion> pv(new PackageVersion(package));

            QString version_ = WPMUtils::getMSIProductAttribute(guid,
                    INSTALLPROPERTY_VERSIONSTRING, &err);
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

            QString title = WPMUtils::getMSIProductName(guid, &err);

            // use the directory name for the title if there is no title
            if (!err.isEmpty() || title.trimmed().isEmpty()) {
                title = WPMUtils::getMSIProductLocation(guid, &err);
                title = WPMUtils::normalizePath(title);
                int pos = title.lastIndexOf('\\');
                if (pos > 0)
                    title = title.right(title.length() - pos - 1);
            }

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

            p->categories.append(QObject::tr("MSI packages"));

            // qDebug() << guid << p->title;

            rep->savePackage(p.data());
        }

        // create InstalledPackageVersion
        QString dir = WPMUtils::getMSIProductLocation(guid, &err);
        if (!err.isEmpty())
            dir = "";

        InstalledPackageVersion* ipv = new InstalledPackageVersion(package,
                version, dir);
        ipv->detectionInfo = "msi:" + guid;
        installed->append(ipv);
    }

    job->setProgress(1);
    job->complete();
}
