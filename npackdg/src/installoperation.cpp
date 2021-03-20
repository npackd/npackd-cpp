#include "installoperation.h"
#include "dbrepository.h"
#include "abstractrepository.h"

InstallOperation::InstallOperation(
        const QString &package, const Version &version, bool install):
        install(install), package(package), version(version)
{
}

PackageVersion *InstallOperation::findPackageVersion(QString* err) const
{
    return DBRepository::getDefault()->findPackageVersion_(
            this->package, this->version, err);
}

QString InstallOperation::toString() const
{
    return package + " " + version.getVersionString() + " " +
            (install ? "install" : "uninstall");
}

void InstallOperation::simplify(std::vector<InstallOperation*> ops)
{
    for (int i = 0; i < static_cast<int>(ops.size()); ) {
        InstallOperation* op = ops.at(i);

        int found = -1;
        for (int j = i + 1; j < static_cast<int>(ops.size()); j++) {
            InstallOperation* op2 = ops.at(j);
            if (op->package == op2->package &&
                    op->version == op2->version &&
                    !op->install && op2->install) {
                found = j;
                break;
            }
        }

        if (found >= 0) {
            delete ops.at(i);
            ops.erase(ops.begin() + i);
            delete ops.at(found);
            ops.erase(ops.begin() + found);
        } else {
            i++;
        }
    }
}
