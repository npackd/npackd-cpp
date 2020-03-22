#include "packageutils.h"

#include "installedpackages.h"

PackageUtils::PackageUtils()
{

}

QList<PackageVersion*> PackageUtils::getAddPackageVersionOptions(
        const DBRepository& dbr, const CommandLine& cl,
        QString* err)
{
    QList<PackageVersion*> ret;
    QList<CommandLine::ParsedOption *> pos = cl.getParsedOptions();

    InstalledPackages* ip = InstalledPackages::getDefault();

    for (int i = 0; i < pos.size(); i++) {
        if (!err->isEmpty())
            break;

        CommandLine::ParsedOption* po = pos.at(i);
        if (po->opt->nameMathes("package")) {
            CommandLine::ParsedOption* ponext = nullptr;
            if (i + 1 < pos.size())
                ponext = pos.at(i + 1);

            QString package = po->value;
            if (!Package::isValidName(package)) {
                *err = QObject::tr("Invalid package name: %1").arg(package);
            }

            Package* p = nullptr;
            if (err->isEmpty()) {
                p = dbr.findOnePackage(package, err);
                if (err->isEmpty()) {
                    if (!p)
                        *err = QObject::tr("Unknown package: %1").arg(package);
                }
            }

            PackageVersion* pv = nullptr;
            if (err->isEmpty()) {
                QString version;
                if (ponext != nullptr && ponext->opt->nameMathes("version"))
                    version = ponext->value;

                QString versions;
                if (ponext != nullptr && ponext->opt->nameMathes("versions"))
                    versions = ponext->value;

                if (!versions.isNull()) {
                    i++;
                    Dependency v;
                    v.package = p->name;
                    if (!v.setVersions(versions)) {
                        *err = QObject::tr("Cannot parse a version range: %1").
                                arg(versions);
                    } else {
                        InstalledPackageVersion* ipv =
                                ip->findHighestInstalledMatch(v);
                        if (ipv) {
                            pv = dbr.findPackageVersion_(ipv->package,
                                    ipv->version, err);
                            if (err->isEmpty()) {
                                if (!pv) {
                                    *err = QObject::tr("Package version not found: %1 (%2) %3").
                                            arg(p->title).arg(p->name).
                                            arg(ipv->version.getVersionString());
                                }
                            }
                            delete ipv;
                        } else {
                            pv = dbr.findBestMatchToInstall(v,
                                    QList<PackageVersion*>(), err);
                            if (err->isEmpty()) {
                                if (!pv) {
                                    *err = QObject::tr("Package version not found: %1 (%2) %3").
                                            arg(p->title).arg(p->name).
                                            arg(versions);
                                }
                            }
                        }
                    }
                } else if (version.isNull()) {
                    pv = dbr.findNewestInstallablePackageVersion_(
                            p->name, err);
                    if (err->isEmpty()) {
                        if (!pv) {
                            *err = QObject::tr("No installable version was found for the package %1 (%2)").
                                    arg(p->title).arg(p->name);
                        }
                    }
                } else {
                    i++;
                    Version v;
                    if (!v.setVersion(version)) {
                        *err = QObject::tr("Cannot parse version: %1").
                                arg(version);
                    } else {
                        pv = dbr.findPackageVersion_(p->name, v,
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
                ret.append(pv);

            delete p;
        }
    }

    return ret;
}

QList<Dependency *> PackageUtils::getPackageVersionOptions(const CommandLine &cl, QString *err)
{
    DBRepository* dbr = DBRepository::getDefault();

    QList<Dependency*> ret;
    QList<CommandLine::ParsedOption *> pos = cl.getParsedOptions();

    for (int i = 0; i < pos.size(); i++) {
        if (!err->isEmpty())
            break;

        CommandLine::ParsedOption* po = pos.at(i);
        if (po->opt->nameMathes("package")) {
            CommandLine::ParsedOption* ponext = nullptr;
            if (i + 1 < pos.size())
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

            if (err->isEmpty()) {
                QString version;
                if (ponext != nullptr && ponext->opt->nameMathes("version"))
                    version = ponext->value;

                QString versions;
                if (ponext != nullptr && ponext->opt->nameMathes("versions"))
                    versions = ponext->value;

                Dependency* dep = nullptr;
                if (!versions.isNull()) {
                    i++;
                    dep = new Dependency();
                    dep->package = p->name;
                    if (!dep->setVersions(versions)) {
                        *err = QObject::tr("Cannot parse a version range: %1").
                                arg(versions);
                        delete dep;
                        dep = nullptr;
                    }
                } else if (version.isNull()) {
                    dep = new Dependency();
                    dep->package = p->name;
                    dep->setUnboundedVersions();
                } else {
                    i++;
                    Version v;
                    if (!v.setVersion(version)) {
                        *err = QObject::tr("Cannot parse version: %1").
                                arg(version);
                    } else {
                        dep = new Dependency();
                        dep->package = p->name;
                        dep->setExactVersion(v);
                    }
                }
                if (dep)
                    ret.append(dep);
            }

            delete p;
        }
    }

    return ret;
}


