#ifndef INSTALLEDPACKAGES_H
#define INSTALLEDPACKAGES_H

#include <windows.h>

#include <QMap>
#include <QObject>

#include "installedpackageversion.h"
#include "version.h"
#include "windowsregistry.h"
#include "job.h"
#include "abstractthirdpartypm.h"
#include "dbrepository.h"

/**
 * @brief information about installed packages
 * @threadsafe
 */
class InstalledPackages: public QObject
{
    Q_OBJECT
private:
    static InstalledPackages def;

    mutable QMutex mutex;

    /** please use the mutex to access the data */
    QMap<QString, InstalledPackageVersion*> data;

    InstalledPackages();

    void detectOneDotNet(const WindowsRegistry& wr, const QString& keyName);

    /**
     * @return error message
     */
    QString clearPackagesInNestedDirectories();

    void detectMicrosoftInstaller();
    void detectMSXML();
    void detectJRE(bool w64bit);
    void detectJDK(bool w64bit);
    void detectWindows();

    /**
     * @brief processOneInstalled3rdParty
     * @param r
     * @param ipv
     * @threadsafe
     */
    void processOneInstalled3rdParty(DBRepository *r,
            InstalledPackageVersion *ipv);

    /**
     * @param exact if true, only exact matches to packages from current
     *     repositories recognized as existing software (e.g. something like
     *     com.mysoftware.MySoftware-2.2.3). This setting should help in rare
     *     cases when Npackd 1.14 and 1.15 are used in parallel for some time
     *     If the value is false, also
     *     packages not known in current repositories are recognized as
     *     installed.
     */
    void scanPre1_15Dir(bool exact);

    /**
     * Adds unknown in the repository, but installed packages.
     */
    void detectPre_1_15_Packages();

    /**
     * THIS METHOD IS NOT THREAD-SAFE
     *
     * @brief finds the specified installed package version
     * @param package full package name
     * @param version package version
     * @param err error message will be stored here
     * @return found or newly created information
     */
    InstalledPackageVersion* findOrCreate(const QString& package,
            const Version& version, QString* err);

    /**
     * @brief saves the information in the Windows registry
     * @param ipv information about an installed package version
     * @return error message
     */
    static QString saveToRegistry(InstalledPackageVersion* ipv);

    /**
     * THIS METHOD IS NOT THREAD-SAFE
     *
     * @brief finds the specified installed package version
     * @param package full package name
     * @param version package version
     * @return found information or 0 if the specified package version is not
     *     installed. The returned object may still represent a not installed
     *     package version. Please check InstalledPackageVersion::getDirectory()
     */
    InstalledPackageVersion* findNoCopy(const QString& package,
            const Version& version) const;
public:
    /** package name for the current application */
    QString packageName;

    /**
     * @return default instance
     */
    static InstalledPackages* getDefault();

    /**
     * Reads the package statuses from the registry.
     *
     * @return error message
     */
    QString readRegistryDatabase();

    /**
     * @brief finds the specified installed package version
     * @param package full package name
     * @param version package version
     * @return [owner:caller]
     *     found information or 0 if the specified package version is not
     *     installed. The returned object may still represent a not installed
     *     package version. Please check InstalledPackageVersion::getDirectory()
     */
    InstalledPackageVersion* find(const QString& package,
            const Version& version) const;

    /**
     * @brief detects packages, package versions etc. from another package
     *     manager
     *
     * If the directory of an installed package resides under an existing
     * package, it will be ignored.
     *
     * These 5 cases exist for installed package versions:
     *     case 1: directory == "", "Uninstall.bat" is not available.
     *         A directory under "NpackdDetected" will be created and a simple
     *         "Uninstall.bat" that generates an error will be stored there.
     *     case 2: directory == "", "Uninstall.bat" is available.
     *         A directory under "NpackdDetected" will be created and the
     *         "Uninstall.bat" will be placed there
     *     case 3: directory != "", but is belongs to another package.
     *         This entry will be ignored.
     *     case 4: directory != "", "Uninstall.bat" is not available.
     *         The package removal would just delete the directory.
     *     case 5: directory != "", "Uninstall.bat" is available. The
     *         "Uninstall.bat" will be stored in the package directory, if
     *         it does not already exist.
     * Note: a non-existing directory is handled as ""
     *
     * @param r [ownership:caller] repository where all the data will be stored
     * @param pm [ownership:caller] a 3rd party package manager
     * @param replace should the existing entries be replaced?
     * @param detectionInfoPrefix prefix for all detection info values
     *     generated by this
     *     package manager like "control-panel:" or an empty string if there is
     *     none. If
     *     the value is not empty, the package manager should return all
     *     installed packages and the packages not returned are considered to
     *     be not installed anymore.
     * @return error message
     */
    QString detect3rdParty(DBRepository *r,
            AbstractThirdPartyPM* pm, bool replace=false,
            const QString &detectionInfoPrefix="");

    /**
     * @brief searches for a dependency in the list of installed packages. This
     *     function uses the Windows registry directly and should be only used
     *     from "npackdcl path". It should be fast.
     * @param dep dependency
     */
    QString findPath_npackdcl(const Dependency& dep);

    /**
     * @brief registers an installed package version
     * @param package full package name
     * @param version package version
     * @param directory installation directory. This value cannot be empty.
     * @return error message
     */
    QString setPackageVersionPath(const QString& package, const Version& version,
            const QString& directory);

    /**
     * @param filePath full file or directory path
     * @return [ownership:caller] installed package version that "owns" the
     *     specified file or directory or 0
     */
    InstalledPackageVersion* findOwner(const QString& filePath) const;

    /**
     * @return [ownership:caller] installed packages
     */
    QList<InstalledPackageVersion*> getAll() const;

    /**
     * Searches for installed versions of a package.
     *
     * @return [ownership:caller] installed packages
     */
    QList<InstalledPackageVersion*> getByPackage(const QString& package) const;

    /**
     * @brief paths to all installed package versions
     * @return list of directories
     */
    QStringList getAllInstalledPackagePaths() const;

    /**
     * Reloads the database about installed packages from the
     * registry and performs a quick detection of packages from the MSI database
     * and "Software" control panel. Checks also that the package versions
     * directories are still present.
     *
     * @param rep repository that should be used
     * @param job job for this method
     */
    void refresh(DBRepository *rep, Job* job);

    /**
     * @brief returns the path of an installed package version
     * @param package full package name
     * @param version package version
     * @return installation path or "" if the package version is not installed
     */
    QString getPath(const QString& package, const Version& version) const;

    /**
     * @brief checks whether a package version is installed
     * @param package full package name
     * @param version version number
     * @return true = installed
     */
    bool isInstalled(const QString& package, const Version& version) const;

    /**
     * @brief fires the statusChanged() event
     * @param package full package name
     * @param version package version number
     */
    void fireStatusChanged(const QString& package, const Version& version);

    /**
     * @brief returns the newest installed version for a package
     * @param package full package name
     * @return [owner:caller] found installed version or 0. This is a copy.
     */
    InstalledPackageVersion *getNewestInstalled(const QString &package) const;
signals:
    /**
     * @brief fired if a package version was installed or uninstalled
     * @param package full package version
     * @param version version number
     */
    void statusChanged(const QString& package, const Version& version);
};

#endif // INSTALLEDPACKAGES_H
