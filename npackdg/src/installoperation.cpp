#include "installoperation.h"
#include "dbrepository.h"
#include "abstractrepository.h"

InstallOperation::InstallOperation()
{
    this->install = true;
}

PackageVersion *InstallOperation::findPackageVersion(QString* err) const
{
    return DBRepository::getDefault()->findPackageVersion_(
            this->package, this->version, err);
}

InstallOperation *InstallOperation::clone() const
{
    InstallOperation* r = new InstallOperation();
    *r = *this;
    return r;
}

QString InstallOperation::toString() const
{
    return package + " " + version.getVersionString() + " " +
            (install ? "install" : "uninstall");
}

void InstallOperation::simplify(QList<InstallOperation*> ops)
{
    for (int i = 0; i < ops.size(); ) {
        InstallOperation* op = ops.at(i);

        int found = -1;
        for (int j = i + 1; j < ops.size(); j++) {
            InstallOperation* op2 = ops.at(j);
            if (op->package == op2->package &&
                    op->version == op2->version &&
                    !op->install && op2->install) {
                found = j;
                break;
            }
        }

        if (found >= 0) {
            delete ops.takeAt(i);
            delete ops.takeAt(found);
        } else {
            i++;
        }
    }
}
