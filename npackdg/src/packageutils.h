#ifndef PACKAGEUTILS_H
#define PACKAGEUTILS_H

#include "QList"
#include "QString"

#include "packageversion.h"
#include "dbrepository.h"
#include "commandline.h"
#include "dependency.h"

class PackageUtils
{
private:
    PackageUtils();
public:
    /**
     * @brief parses the command line and returns the list of chosen package
     *     versions
     * @param dbr DB-Repository
     * @param cl command line
     * @param err errors will be stored here
     * @return [owner:caller] list of package versions
     */
    static QList<PackageVersion *> getAddPackageVersionOptions(const DBRepository& dbr, const CommandLine &cl, QString *err);

    /**
     * @brief parses the command line and returns the list of chosen package
     *     versions
     * @param cl command line
     * @param err errors will be stored here
     * @return [owner:caller] list of package versions ranges
     */
    static QList<Dependency *> getPackageVersionOptions(const CommandLine &cl,
            QString *err);
};

#endif // PACKAGEUTILS_H
