#include <QList>
#include <QSet>
#include <QString>

#include "installedpackagesthirdpartypm.h"
#include "installedpackages.h"

InstalledPackagesThirdPartyPM::InstalledPackagesThirdPartyPM()
{
}

void InstalledPackagesThirdPartyPM::scan(Job* job,
        QList<InstalledPackageVersion *> *installed, Repository *rep) const
{
    InstalledPackages* ip = InstalledPackages::getDefault();
    QList<InstalledPackageVersion*> ipvs = ip->getAll();
    QSet<QString> used;
    for (int i = 0; i < ipvs.count(); ++i) {
        InstalledPackageVersion* ipv = ipvs.at(i);
        if (!used.contains(ipv->package)) {
            QString title = ipv->package;
            int pos = title.lastIndexOf('.');
            if (pos > 1)
                title = title.right(title.count() - pos - 1);
            Package* p = new Package(ipv->package, title);
            rep->packages.append(p);
            used.insert(ipv->package);
        }
        PackageVersion* pv = new PackageVersion(ipv->package, ipv->version);
        rep->packageVersions.append(pv);
        rep->package2versions.insert(ipv->package, pv);
    }
    qDeleteAll(ipvs);

    job->setProgress(1);
    job->complete();
}
