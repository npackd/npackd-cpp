#include <memory>

#include <windows.h>
#include <msi.h>
#include <map>

#include <QBuffer>
#include <QByteArray>

#include "msithirdpartypm.h"
#include "wpmutils.h"

MSIThirdPartyPM::MSIThirdPartyPM()
{
    detectionPrefix = "msi:";
}

void MSIThirdPartyPM::scan(Job* job,
        std::vector<InstalledPackageVersion *> *installed,
        Repository *rep) const
{
    // qCDebug(npackd) << "MSIThirdPartyPM::scan 0";

    std::vector<QString> all = WPMUtils::findInstalledMSIProducts();

    std::vector<QString> components = WPMUtils::findInstalledMSIComponents();
    std::unordered_multimap<QString, QString> p2c =
            WPMUtils::mapMSIComponentsToProducts(components);

    QString windowsDir = WPMUtils::normalizePath(WPMUtils::getWindowsDir());

    // qCDebug(npackd) << all.at(0);

    // qCDebug(npackd) << "MSIThirdPartyPM::scan 1";

    for (auto& guid: all) {
        // qCDebug(npackd) << "MSIThirdPartyPM::scan " << i << "of" << all.count();
        // qCDebug(npackd) << guid;

        QString err;
        QString package;
        Version version;

        // qCDebug(npackd) << "MSIThirdPartyPM::scan loop 1";

        // qCDebug(npackd) << "MSIThirdPartyPM::scan loop 1.1";

        package = "msi." + guid.mid(1, 36);

        // create package version
        std::unique_ptr<PackageVersion> pv(new PackageVersion(package));

        QString version_ = WPMUtils::getMSIProductAttribute(guid,
                INSTALLPROPERTY_VERSIONSTRING, &err);
        if (err.isEmpty()) {
            if (!version.setVersion(version_))
                version.setVersion(1, 0);
            else
                version.normalize();
        }
        pv->version = version;

        // qCDebug(npackd) << "MSIThirdPartyPM::scan loop 1.2";

        // Uninstall.bat
        PackageVersionFile* pvf = new PackageVersionFile(
                ".Npackd\\Uninstall.bat",
                "msiexec.exe /qn /norestart /Lime "
                            ".Npackd\\UninstallMSI.log /x" + guid + "\r\n" +
                            "set err=%errorlevel%" + "\r\n" +
                            "type .Npackd\\UninstallMSI.log" + "\r\n" +
                            "rem 3010=restart required" + "\r\n" +
                            "if %err% equ 3010 exit 0" + "\r\n" +
                            "rem 1605=unknown product" + "\r\n" +
                            "if %err% equ 1605 exit 0" + "\r\n" +
                            "if %err% neq 0 exit %err%" + "\r\n");
        pv->files.push_back(pvf);


        pvf = new PackageVersionFile(
                ".Npackd\\Stop.bat",
                "rem the program should be stopped by the uninstaller\r\n");
        pv->files.push_back(pvf);

        rep->savePackageVersion(pv.get(), true);

        // qCDebug(npackd) << "MSIThirdPartyPM::scan loop 1.3";

        QString title = WPMUtils::getMSIProductName(guid, &err);

        if (title.endsWith(version_)) {
            title.chop(version_.length());
            title = title.trimmed();
        }

        // qCDebug(npackd) << "MSIThirdPartyPM::scan loop 1.4";

        Package* p = new Package(pv->package, title);
        p->description = p->title;

        QString publisher = WPMUtils::getMSIProductAttribute(guid,
                INSTALLPROPERTY_PUBLISHER, &err);
        if (err.isEmpty() && !publisher.isEmpty())
            p->description += " (" + publisher + ")";

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

        QString changelog = WPMUtils::getMSIProductAttribute(guid,
                INSTALLPROPERTY_URLUPDATEINFO, &err);
        if (err.isEmpty() && WPMUtils::checkURL(QUrl(),
                &changelog, false).isEmpty())
            p->setChangeLog(changelog);

        // qCDebug(npackd) << "MSIThirdPartyPM::scan loop 1.5";

        p->categories.push_back(QObject::tr("MSI packages"));

        if (err.isEmpty()) {
            QString icon = WPMUtils::getMSIProductAttribute(guid,
                    INSTALLPROPERTY_PRODUCTICON, &err);
            if (err.isEmpty()) {
                p->setIcon(WPMUtils::extractIconURL(icon));
            } else {
                err = "";
            }
        }
        // p->icon = "data:image/png;base64,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";

        // qCDebug(npackd) << guid << p->title;

        // qCDebug(npackd) << "MSIThirdPartyPM::scan loop 1.6";

        // qCDebug(npackd) << "MSIThirdPartyPM::scan loop 1.7";

        // qCDebug(npackd) << "MSIThirdPartyPM::scan loop 2";

        // create InstalledPackageVersion
        QString dir = WPMUtils::getMSIProductLocation(guid, &err);
        if (!err.isEmpty())
            dir = "";

        if (dir.isEmpty()) {
            // it would be better to search for a common ancestor here
            auto cmps = p2c.equal_range(guid);
            for (auto i = cmps.first; i != cmps.second; ++i) {
                dir = WPMUtils::getMSIComponentPath(guid, i->second, &err);
                if (err.isEmpty() && !dir.isEmpty()) {
                    if (!dir.at(0).isDigit()) {
                        QFileInfo fi(dir);
                        if (fi.isAbsolute() && fi.exists()) {
                            QDir d;
                            if (fi.isFile())
                                d = fi.absoluteDir();
                            else
                                d = QDir(fi.absoluteFilePath());

                            if (!d.isRoot() && !WPMUtils::isUnderOrEquals(
                                    WPMUtils::normalizePath(d.absolutePath()),
                                    windowsDir)) {
                                dir = d.absolutePath();
                                break;
                            }
                        }
                    } else {
                        // qCDebug(npackd) << "cmp reg" << dir;
                    }
                }
                dir = "";
            }
        }

        // use the directory name for the title if there is no title
        if (p->title.trimmed().isEmpty()) {
            if (!dir.isEmpty()) {
                p->title = WPMUtils::normalizePath(dir);
                int pos = p->title.lastIndexOf('\\');
                if (pos > 0)
                    p->title = p->title.right(p->title.length() - pos - 1);
            } else {
                p->title = QObject::tr("MSI package with the GUID %1").
                        arg(guid);
            }
        }

        rep->savePackage(p, true);
        delete p;
        p = nullptr;

        // qCDebug(npackd) << "MSIThirdPartyPM::scan loop 3";

        InstalledPackageVersion* ipv = new InstalledPackageVersion(package,
                version, dir);
        ipv->detectionInfo = "msi:" + guid;
        installed->push_back(ipv);
    }

    // qCDebug(npackd) << "MSIThirdPartyPM::scan 2";

    job->setProgress(1);
    job->complete();
}
