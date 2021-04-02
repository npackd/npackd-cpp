#ifndef PACKAGEVERSION_H
#define PACKAGEVERSION_H

#include <guiddef.h>
#include <windows.h>
#include <unordered_set>
#include <mutex>

#include <QString>
#include <QMetaType>
#include <QDir>
#include <QUrl>
#include <QSemaphore>
#include <QXmlStreamWriter>
#include <QCryptographicHash>
#include <QJsonObject>

#include "job.h"
#include "packageversionfile.h"
#include "version.h"
#include "dependency.h"
#include "installoperation.h"
#include "commandline.h"
#include "dag.h"

// 30ed381d-59ea-4ca5-bd1d-5ee8ec97b2be
DEFINE_GUID(UUID_ClientID,0x30ed381dL,0x59ea,0x4ca5,0xbd,0x1d,0x5e,0xe8,0xec,0x97,0xb2,0xbe);

class DBRepository;
class AbstractRepository;
class InstallOperation;
class InstalledPackages;

/**
 * One version of a package (installed or not).
 *
 * Adding a new field:
 * - add the variable definition
 * - update toXML
 * - update toJSON
 * - update clone
 */
class PackageVersion
{
private:    
    static QSemaphore httpConnections;

    /**
     * Set of PackageVersion::getStringId() for the locked package versions.
     * A locked package version cannot be installed or uninstalled.
     * Access to this data should be only done under the
     * lockedPackageVersionsMutex
     */
    static std::unordered_set<QString> lockedPackageVersions;

    /** mutex for lockedPackageVersions */
    static std::recursive_mutex lockedPackageVersionsMutex;

    bool createShortcuts(const QString& dir, QString* errMsg);

    /**
     * @brief executes a script like .Npackd\Install.bat
     * @param job job for monitoring the progress
     * @param where directory where to start
     * @param path this is the name of the script like ".Npackd\Install.bat"
     *     relative to "where"
     * @param env additional environment variables
     * @param printScriptOutput true = redirect the script output to the default
     *     output stream
     * @param unicode true = UTF-16, false = 1 byte system encoding
     */
    void executeFile2(Job *job, const QString &where, const QString &path,
            const std::vector<QString> &env,
            bool printScriptOutput, bool unicode=true);

    void deleteShortcuts(const QString& dir,
            Job* job, bool menu, bool desktop, bool quickLaunch);

    /**
     * @brief deletes shortcuts from a new thread
     * @param dir target directory for shortcuts
     * @param job job
     * @param menu true = delete shortcuts in the start menu
     * @param desktop true = delete shortcuts on the desktop
     * @param quickLaunch true = delete shortcuts in the quick launch bar
     */
    void deleteShortcutsRunnable(const QString& dir,
            Job* job, bool menu, bool desktop, bool quickLaunch);

    /**
     * Deletes a directory. If something cannot be deleted, it waits and
     * tries to delete the directory again. Moves the directory to .Trash if
     * it cannot be moved to the recycle bin.
     *
     * @param job progress for this task
     * @param dir this directory will be deleted
     * @param programCloseType how to close running programs. Multiple flags
     *     may be combined here using OR.
     * @param stoppedServices internal names of the Windows services that were
     *     stopped
     */
    void removeDirectory(Job* job, const QString& dir, DWORD programCloseType,
            std::vector<QString> *stoppedServices);

    void emitStatusChanged();

    QString addBasicVars(std::vector<QString> *env);
    void addDependencyVars(std::vector<QString> *vars);

    /**
     * @brief creates executable shims
     * @param dir directory for this package version if it is being installed or
     *     an empty string if it is being removed
     * @param errMsg the error message will be stored here
     * @return true = OK
     */
    bool createExecutableShims(const QString &dir, QString *errMsg);

    void installWith(Job *job);
public:
    /** package version type */
    enum Type {
        /** .zip file */
        ZIP,

        /** any file */
        ONE_FILE,

        /** Inno Setup installer */
        INNO_SETUP,

        /** NSIS installer */
        NSIS
    };

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
    static int indexOf(const std::vector<PackageVersion*>& pvs, PackageVersion* f);

    /**
     * @param package package name will be stored here or "" if none
     * @param version version number will be stored here
     */
    static void findLockedPackageVersion(QString* package, Version* version);

    /**
     * @param xml <version>
     * @param err error message will be stored here
     * @param validate true = perform all available validations
     * @return created object or 0
     */
    static PackageVersion* parse(QByteArray &xml, QString* err,
            bool validate=true);

    /**
     * @brief searches for a package version only using the package name and
     *     version number
     * @param list searching in this list
     * @param pv searching for this package version
     * @return true if the list contains the specified package version
     */
    static bool contains(const std::vector<PackageVersion*>& list,
            PackageVersion* pv);

    /**
     * @brief parses the command line and returns the list of chosen package
     *     versions
     * @param cl command line
     * @param err errors will be stored here
     * @return [move] list of package versions
     */
    static std::vector<PackageVersion *> getRemovePackageVersionOptions(
            const CommandLine &cl, QString *err);

    /**
     * @brief parses the command line and returns the list of chosen package
     *     versions
     * @param cl command line
     * @param err errors will be stored here
     * @return [move] list of installed package versions
     */
    static std::vector<InstalledPackageVersion *> getPathPackageVersionOptions(
            const CommandLine &cl, QString *err);

    /** package version */
    Version version;

    /** complete package name like net.sourceforge.NotepadPlusPlus */
    QString package;

    /** important files (shortcuts for these will be created in the menu) */
    std::vector<QString> importantFiles;

    /** titles for the important files */
    std::vector<QString> importantFilesTitles;

    /** command line tools ("shim" executables for these will be created) */
    std::vector<QString> cmdFiles;

    /**
     * Text files.
     */
    std::vector<PackageVersionFile*> files;

    /**
     * Dependencies.
     */
    std::vector<Dependency*> dependencies;

    /** 0 = zip file, 1 = one file, 2 = Inno Setup */
    Type type;

    /**
     * SHA-1 or SHA-256 hash sum for the installation file or empty if not
     * defined
     */
    QString sha1;

    /** 0 = SHA-1, 1 = SHA-256 */
    QCryptographicHash::Algorithm hashSumType;

    /**
     * .zip file for downloading
     */
    QUrl download;

    /**
     * unknown/1.0
     */
    PackageVersion();

    /**
     * package/1.0
     *
     * @param package
     */
    explicit PackageVersion(const QString& package);

    /**
     * -
     *
     * @param package full internal package name
     * @param version package version
     */
    PackageVersion(const QString& package, const Version& version);

    virtual ~PackageVersion();

    /**
     * @brief saves the text files associated with this package version
     * @param d the files will be saved in this directory. This directory
     *     might not exist.
     * @return error message or ""
     */
    QString saveFiles(const QDir& d);

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
     * Returns the extension of the package file (quessing from the URL).
     *
     * @return e.g. ".exe" or ".zip". Never returns an empty string
     */
    QString getFileExtension();

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
    QString toString(bool includeFullPackageName=false) const;

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
     * @return a maybe existing directory where this package would normally
     *     installed (e.g. C:\Program Files\My_Prog)
     */
    QString getIdealInstallationDirectory() const;

    /**
     * @return a maybe existing directory where this package would normally
     *     installed as secondary location including the version number
     *     (e.g. C:\Program Files\My_Prog-12.2.3)
     */
    QString getSecondaryInstallationDirectory() const;

    /**
     * Installs this package without dependencies. The directory should already
     * exist and be prepared by this->download(...)
     *
     * @param job job for this method
     * @param where target directory
     * @param binary relative file name of the downloaded binary
     *     or "" for packages of type "zip"
     * @param printScriptOutput true = redirect the script output to the default
     *     output stream
     * @param programCloseType how to close running programs. Multiple flags
     *     may be combined here using OR.
     * @param stoppedServices internal names of the Windows services that were
     *     stopped will be stored here
     */
    void install(Job* job, const QString& where, const QString &binary,
            bool printScriptOutput, DWORD programCloseType,
            std::vector<QString> *stoppedServices);

    /**
     * Downloads the package binary, checks its hash sum, checks the binary for
     * viruses, unpacks it in case of a .zip file, stores the text files.
     *
     * @param job job for this method
     * @param where an existing directory for the package
     * @param interactive true = allow the interaction with the user
     * @param user user name for the HTTP authentication or ""
     * @param password password for the HTTP authentication or ""
     * @param proxyUser user name for the HTTP proxy authentication or ""
     * @param proxyPassword password for the HTTP proxy authentication or ""
     * @return the full name of the downloaded file or "" for packages of
     *     type "zip"
     */
    QString download_(Job* job, const QString& where,
            bool interactive, const QString& user,
            const QString& password,
            const QString& proxyUser, const QString& proxyPassword);

    /**
     * Uninstalls this package version.
     *
     * @param job job for this method
     * @param printScriptOutput true = redirect the script output to the default
     *     output stream
     * @param programCloseType how to close running programs. Multiple flags
     *     may be combined here using OR.
     * @param stoppedServices internal names of the Windows services that were
     *     stopped
     */
    void uninstall(Job* job, bool printScriptOutput, DWORD programCloseType,
            std::vector<QString> *stoppedServices);

    /**
     * @param r DB repository
     * @return status like "locked, installed"
     */
    QString getStatus(DBRepository *r) const;

    /**
     * Stores this object as XML <version>.
     *
     * @param w output
     */
    void toXML(QXmlStreamWriter* w) const;

    /**
     * Stores this object as JSON
     *
     * @param w output
     */
    void toJSON(QJsonObject &w) const;

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
     * @brief searches for a definition of a text file
     * @param path file path (case-insensitive)
     * @return found file or 0. The object will still be owned by this PackageVersion
     */
    PackageVersionFile *findFile(const QString &path) const;

    /**
     * @brief stops this package version if it is running. This either executes
     *     .Npackd\Stop.bat or closes the running applications otherwise.
     * @param job
     * @param programCloseType how to close running programs. Multiple flags
     *     may be combined here using OR.
     * @param printScriptOutput true = redirect the script output to the default
     *     output stream
     * @param stoppedServices the names of stopped Windows services will be
     *     stored here
     */
    void stop(Job *job, DWORD programCloseType, bool printScriptOutput,
            std::vector<QString> *stoppedServices);

    /**
     * @brief returns the file name without the path for a command line tool
     *     with the specified index
     * @param index index of the "cmd-file" tag
     * @return the file name without the path
     */
    QString getCmdFileName(int index);

    /**
     * @brief downloads the binary to the specified location validating the
     *    checksum
     * @param job job object
     * @param filename target file name
     * @param interactive true = interactive
     */
    void downloadTo(Job &job, const QString &filename, bool interactive);

    /**
     * Builds another package from this one. This is normally used to build a
     * binary from the sources.
     *
     * @param job job for this method
     * @param outputDir an existing directory where the output package
     *     should be built
     * @param outputPackage the name of the package that should be built
     * @param printScriptOutput true = redirect the script output to the default
     *     output stream
     */
    void build(Job *job, const QString &outputPackage, const QString &outputDir,
               bool printScriptOutput);

    /**
     * @brief checks whether a package version is locked and cannot be installed
     *     or removed
     * @param package package name
     * @param version version number
     * @return true if this package version is locked and cannot be installed
     *     or removed
     */
    static bool isLocked(const QString &package, const Version &version);
};

Q_DECLARE_METATYPE(PackageVersion);

inline QString PackageVersion::getStringId(const QString& package,
    const Version& version)
{
    QString r(package);
    r.append('/');
    Version v = version;
    v.normalize();
    r.append(v.getVersionString());
    return r;
}

#endif // PACKAGEVERSION_H
