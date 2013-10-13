#ifndef PACKAGEVERSION_H
#define PACKAGEVERSION_H

#include "qxml.h"
#include "qstring.h"
#include "qmetatype.h"
#include "qdir.h"
#include "qurl.h"
#include "qstringlist.h"
#include <QSemaphore>
#include <QDomElement>
#include <QXmlStreamWriter>

#include "job.h"
#include "packageversionfile.h"
#include "version.h"
#include "dependency.h"
#include "installoperation.h"
#include "detectfile.h"

class InstallOperation;

/**
 * One version of a package (installed or not).
 */
class PackageVersion
{
private:    
    static QSemaphore httpConnections;
    static QSemaphore installationScripts;

    /**
     * Set of PackageVersion::getStringId() for the locked package versions.
     * A locked package version cannot be installed or uninstalled.
     * Access to this data should be only done under the
     * lockedPackageVersionsMutex
     */
    static QSet<QString> lockedPackageVersions;

    /** mutex for lockedPackageVersions */
    static QMutex lockedPackageVersionsMutex;

    static PackageVersionFile* createPackageVersionFile(QDomElement* e,
            QString* err);
    static Dependency* createDependency(QDomElement* e);
    static DetectFile* createDetectFile(QDomElement* e, QString* err);

    void unzip(Job* job, QString zipfile, QString outputdir);
    bool createShortcuts(const QString& dir, QString* errMsg);
    QString saveFiles(const QDir& d);

    /**
     * @return program output
     */
    QByteArray executeFile(Job* job, const QString& where,
            const QString& path, const QString& outputFile,
            const QStringList& env);

    void deleteShortcuts(const QString& dir,
            Job* job, bool menu, bool desktop, bool quickLaunch);
    /**
     * Deletes a directory. If something cannot be deleted, it waits and
     * tries to delete the directory again. Moves the directory to .Trash if
     * it cannot be moved to the recycle bin.
     *
     * @param job progress for this task
     * @param dir this directory will be deleted
     */
    void removeDirectory(Job* job, const QString& dir);

    void emitStatusChanged();
    void addDependencyVars(QStringList* vars);
public:
    /**
     * @brief string ID for the specified package version
     * @param package full package name
     * @param version package version
     * @return package + "/" + version
     */
    static QString getStringId(const QString& package, const Version& version);

    /**
     * @brief searches for the specified object in the specified list. Objects
     *     will be compared only by package and version.
     * @param pvs list of package versions
     * @param f search for this object
     * @return index of the found object or -1
     */
    static int indexOf(const QList<PackageVersion*> pvs, PackageVersion* f);

    /**
     * @param err error message will be stored here
     * @return [ownership:caller] the first found locked PackageVersion or 0
     */
    static PackageVersion* findLockedPackageVersion(QString* err);

    /**
     * @param e <version>
     * @param err error message will be stored here
     * @return created object or 0
     */
    static PackageVersion* parse(QDomElement* e, QString* err);

    /**
     * @brief searches for a package version only using the package name and
     *     version number
     * @param list searching in this list
     * @param pv searching for this package version
     * @return true if the list contains the specified package version
     */
    static bool contains(const QList<PackageVersion*>& list,
            PackageVersion* pv);

    /** package version */
    Version version;

    /** complete package name like net.sourceforge.NotepadPlusPlus */
    QString package;

    /** important files (shortcuts for these will be created in the menu) */
    QStringList importantFiles;

    /** titles for the important files */
    QStringList importantFilesTitles;

    /**
     * Packages.
     */
    QList<PackageVersionFile*> files;

    /**
     * Package detection
     */
    QList<DetectFile*> detectFiles;

    /**
     * Dependencies.
     */
    QList<Dependency*> dependencies;

    /** 0 = zip file, 1 = one file */
    int type;

    /** SHA1 for the installation file or empty if not defined */
    QString sha1;

    /**
     * .zip file for downloading
     */
    QUrl download;

    /**
     * MSI GUID like {1D2C96C3-A3F3-49E7-B839-95279DED837F} or ""
     * if not available. Should be always in lower case
     */
    QString msiGUID;

    PackageVersion();
    PackageVersion(const QString& package);
    PackageVersion(const QString& package, const Version& version);
    virtual ~PackageVersion();

    /**
     * Locks this package version so that it cannot be installed or removed
     * by other processes.
     */
    void lock();

    /**
     * Unlocks this package version so that it can be installed or removed
     * again.
     */
    void unlock();

    /**
     * @return true if this package version is locked and cannot be installed
     *     or removed
     */
    bool isLocked() const;

    /**
     * @return installation path or "" if the package is not installed
     */
    QString getPath() const;

    /**
     * Changes the installation path for this package. This method should only
     * be used if the package was detected.
     *
     * @param path installation path
     * @return error message
     */
    QString setPath(const QString& path);

    /**
     * Renames the directory for this package to a temporary name and then
     * renames it back.
     *
     * @return true if the renaming was OK (the directory is not locked)
     */
    bool isDirectoryLocked();

    /**
     * Downloads the package and computes its SHA1.
     *
     * @return SHA1
     */
    QString downloadAndComputeSHA1(Job* job);

    /**
     * Returns the extension of the package file (quessing from the URL).
     *
     * @return e.g. ".exe" or ".zip". Never returns an empty string
     */
    QString getFileExtension();

    /**
     * Downloads the package.
     *
     * @param filename target file
     */
    void downloadTo(Job* job, QString filename);

    /**
     * Plans installation of this package and all the dependencies recursively.
     *
     * @param installed [ownership:caller] list of installed packages.
     *     This list should be
     *     consulted instead of .installed() and will be updated and contains
     *     all installed package versions after the process
     * @param op [ownership:caller] necessary operations will be appended here.
     *     The existing
     *     elements will not be modified in any way.
     * @param avoid [ownership:caller] list of package versions that cannot be
     *     installed. The list
     *     will be changed by this method. Normally this is an empty list and
     *     objects will be added to it on different recursion levels.
     * @return error message or ""
     */
    QString planInstallation(QList<PackageVersion*>& installed,
            QList<InstallOperation*>& ops, QList<PackageVersion*>& avoid);

    /**
     * Plans un-installation of this package and all the dependent recursively.
     *
     * @param installed list of installed packages. This list should be
     *     consulted instead of .installed() and will be updated and contains
     *     all installed package versions after the process. The list will also
     *     be updated to reflect packages "uninstalled" by this method
     * @param op necessary operations will be added here. The existing
     *     elements will not be modified in any way.
     * @return error message or ""
     */
    QString planUninstallation(QList<PackageVersion*>& installed,
            QList<InstallOperation*>& ops);

    /**
     * @param includeFullPackageName true = full package name will be added
     * @return package title
     */
    QString getPackageTitle(bool includeFullPackageName=false) const;

    /**
     * @return only the last part of the package name (without a dot)
     */
    QString getShortPackageName();

    /**
     * @param includeFullPackageName true = the full package name will be added
     * @return human readable title for this package version
     */
    QString toString(bool includeFullPackageName=false);

    /**
     * @return true if this package version is installed
     */
    bool installed() const;

    /**
     * @return a non-existing directory where this package would normally be
     *     installed (e.g. C:\Program Files\My Prog 2.3.2)
     */
    QString getPreferredInstallationDirectory();

    /**
     * Installs this package (without dependencies).
     *
     * @param job job for this method
     * @param where a non-existing directory
     */
    void install(Job* job, const QString& where);

    /**
     * Uninstalls this package version.
     *
     * @param job job for this method
     */
    void uninstall(Job* job);

    /**
     * @return files currenly locked in this package directory
     */
    QStringList findLockedFiles();

    /**
     * @return status like "locked, installed"
     */
    QString getStatus() const;

    /**
     * @return XML representation of this package version
     */
    QString serialize() const;

    /**
     * Stores this object as XML.
     *
     * @param version <version>
     */
    void toXML(QDomElement* version) const;

    /**
     * Stores this object as XML <version>.
     *
     * @param w output
     */
    void toXML(QXmlStreamWriter* w) const;

    /**
     * @return a copy
     */
    PackageVersion* clone() const;

    /**
     * @return true if this package is in c:\Windows or one of the nested
     *     directories
     */
    bool isInWindowsDir() const;

    /**
     * @brief string that can be used to identify this package and version
     * @return "package/version"
     */
    QString getStringId() const;

    /**
     * @brief transfers all data from another object into this
     * @param pv another package version
     */
    void fillFrom(PackageVersion* pv);

    /**
     * @brief searches for a definition of a text file
     * @param path file path (case-insensitive)
     * @return [ownership:this] found file or 0
     */
    PackageVersionFile *findFile(const QString &path) const;
};

Q_DECLARE_METATYPE(PackageVersion);

#endif // PACKAGEVERSION_H
