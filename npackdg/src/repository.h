#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <windows.h>
#include <unordered_map>

#include "qfile.h"
#include "qurl.h"
#include "qtemporaryfile.h"
#include "qdom.h"

#include "package.h"
#include "packageversion.h"
#include "license.h"
#include "windowsregistry.h"
#include "abstractrepository.h"

/**
 * A repository is a list of packages and package versions.
 */
class Repository: public AbstractRepository
{
private:
    static Repository def;

    void addWindowsPackage();

    /**
     * Finds all package versions.
     *
     * @param package full package name
     * @return the list of package versions (the objects should not
     *     be freed) sorted by the version number. The first returned object
     *     has the highest version number.
     */
    std::vector<PackageVersion*> getPackageVersions(const QString& package) const;

    /**
     * Find the newest installable package version.
     *
     * @param package name of the package like "org.server.Word"
     * @return found package version or 0
     */
    PackageVersion* findNewestInstallablePackageVersion(const QString& package);

    /**
     * Searches for a license by name.
     *
     * @param name name of the license like "org.gnu.GPLv3"
     * @return found license or 0
     */
    License* findLicense(const QString& name);

    /**
     * Searches for a package by name.
     *
     * @param name name of the package like "org.server.Word"
     * @return found package or 0
     */
    Package* findPackage(const QString& name) const;

    /**
     * Find the newest available package version.
     *
     * @param package name of the package like "org.server.Word"
     * @param version package version
     * @return found package version or 0
     */
    PackageVersion* findPackageVersion(const QString& package,
            const Version& version) const;
public:
    /** full package name -> all defined package versions */
    std::unordered_multimap<QString, PackageVersion*> package2versions;

    /**
     * Checks the specified value in <spec-version></spec-version> to be a valid
     * and compatible version number
     *
     * @param specVersion value in <spec-version></spec-version>
     * @return error message or ""
     */
    static QString checkSpecVersion(const QString& specVersion);

    /**
     * Checks the content of the tag <category>
     *
     * @param category content
     * @param err error message is stored here
     * @return improved value of the category, if there were no errors
     */
    static QString checkCategory(const QString& category, QString* err);

    /**
     * Package versions. All version numbers should be normalized.
     */
    std::vector<PackageVersion*> packageVersions;

    /**
     * Packages.
     */
    std::vector<Package*> packages;

    /**
     * Licenses.
     */
    std::vector<License*> licenses;

    /**
     * Creates an empty repository.
     */
    Repository();

    virtual ~Repository();

    Package* findPackage_(const QString& name) const override;

    /**
     * Writes this repository to an XML file.
     *
     * @param filename output file name
     * @return error message or ""
     */
    //QString writeTo(const QString& filename) const;

    /**
     * Stores this object as XML <repository>.
     *
     * @param w output
     */
    void toXML(QXmlStreamWriter& w) const;

    std::vector<PackageVersion*> getPackageVersions_(const QString& package,
            QString *err) const override;

    QString savePackage(Package* p, bool replace) override;

    QString savePackageVersion(PackageVersion* p, bool replace) override;

    QString saveLicense(License *p, bool replace) override;

    PackageVersion* findPackageVersion_(const QString& package,
            const Version& version, QString* err) const override;

    License* findLicense_(const QString& name, QString *err) override;

    QString clear() override;

    std::vector<Package*> findPackagesByShortName(const QString& name) const override;
};

#endif // REPOSITORY_H
