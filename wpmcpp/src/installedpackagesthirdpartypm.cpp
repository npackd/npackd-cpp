#include <QList>
#include <QSet>
#include <QString>
#include <QDebug>

#include "installedpackagesthirdpartypm.h"
#include "installedpackages.h"

InstalledPackagesThirdPartyPM::InstalledPackagesThirdPartyPM()
{
}

void InstalledPackagesThirdPartyPM::scan(Job* job,
        QList<InstalledPackageVersion *> *installed, Repository *rep) const
{
    InstalledPackages* ip = InstalledPackages::getDefault();
    QList<InstalledPackageVersion*> ipvs = ip->getAllNoCopy();
    QSet<QString> used;
    for (int i = 0; i < ipvs.count(); ++i) {
        InstalledPackageVersion* ipv = ipvs.at(i);
        if (!used.contains(ipv->package)) {
            QString title = ipv->package;
            int pos = title.lastIndexOf('.');
            if (pos > 1)
                title = title.right(title.count() - pos - 1);
            Package* p = new Package(ipv->package, title);
            p->description = "[" +
                    QObject::tr("Npackd list of installed packages") +
                    "] " + p->title;
            // Crystal Icons
            p->icon = "data:image/png;base64,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";

            rep->packages.append(p);
            used.insert(ipv->package);
        }
        PackageVersion* pv = new PackageVersion(ipv->package, ipv->version);
        rep->packageVersions.append(pv);
        rep->package2versions.insert(ipv->package, pv);

        if (ipv->installed()) {
            installed->append(ipv->clone());
        }
    }

    job->setProgress(1);
    job->complete();
}
