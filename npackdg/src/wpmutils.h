#ifndef WPMUTILS_H
#define WPMUTILS_H

#include <windows.h>
#include <time.h>
#include <taskschd.h>

#include <QString>
#include <QDir>
#include <QTime>
#include <QCryptographicHash>
#include <QThreadPool>
#include <QLoggingCategory>

#include "job.h"
#include "version.h"
#include "commandline.h"
#include "package.h"
#include "hrtimer.h"

// Logging category for the whole application.
Q_DECLARE_LOGGING_CATEGORY(npackd)

// This category can be used for important messages.
// Currently the only difference is that qInfo() for this category
// on the console uses green as the background color.
Q_DECLARE_LOGGING_CATEGORY(npackdImportant)

/**
 * Some utility methods. This class is called WPMUtils because of the original
 * name of Npackd "Windows Package Manager".
 *
 * All package related utility methods should be placed in PackageUtils.
 */
class WPMUtils
{
private:
    static HANDLE hEventLog;

    static QAtomicInt nextNamePipeId;

    static QString taskName;

    WPMUtils();

    /**
     * @brief checks whether a process is still running
     * @param process process handle
     * @return true = the process is still running
     */
    static bool isProcessRunning(HANDLE process);

    /**
     * @brief closes the specified process top windows
     * @param process process handle
     * @param processWindows all top windows that belong to the same process
     */
    static void closeProcessWindows(HANDLE process,
            const QList<HWND> &processWindows);

    /**
     * @brief searches for the processes that somehow lock the specified
     *     directory. This function enumerates the loaded modules (.dll, etc.)
     * @param dir a directory
     * @return list of handles with PROCESS_ALL_ACCESS permissions. These
     *     handles should be closed later using CloseHandle()
     */
    static QList<HANDLE> getProcessHandlesLockingDirectory(const QString &dir);

    /**
     * @brief searches for the processes that somehow lock the specified
     *     directory. This function enumerates all files opened by the process.
     * @param dir a directory
     * @return list of handles with PROCESS_QUERY_LIMITED_INFORMATION
     *     permissions. These handles should be closed later using CloseHandle()
     */
    static QList<HANDLE> getProcessHandlesLockingDirectory2(const QString &dir);

    static QList<HWND> findProcessTopWindows(DWORD processID);

    static QString disconnectFrom(LPWSTR netname);
    static QString StopDependentServices(SC_HANDLE schSCManager, SC_HANDLE schService,
            QStringList *stoppedServices);
    static QString DoStopSvc(SC_HANDLE schSCManager, const QString &serviceName,
                             QStringList *stoppedServices);
    static QString waitForServiceStatus(SC_HANDLE schService,
            const QString &operation, DWORD status);

    static QList<HANDLE> getAllProcessHandlesLockingDirectory(const QString &dir);

    static void closeHandles(const QList<HANDLE> handles);

    static QString getTaskName();
public:
    /**
     * @brief how to close a process
     */
    static const int CLOSE_WINDOW = 1;
    static const int KILL_PROCESS = 2;
    static const int DISABLE_SHARES = 4;
    static const int STOP_SERVICES = 8;
    static const int CTRL_C = 16;

    static const char* UCS2LE_BOM;

    static const char* CRLF;

    static HRTimer timer;

    /**
     * @return location of the Npackd log file
     */
    static QString getMessagesLog();

    /**
     * Converts the value returned by SHFileOperation to an error message.
     *
     * @param res value returned by SHFileOperation
     * @return error message or "", if OK
     */
    static QString getShellFileOperationErrorMessage(DWORD res);

    /**
     * Moves a directory to a recycle bin.
     *
     * @param dir directory
     * @return error message or ""
     */
    static QString moveToRecycleBin(QString dir);

    /**
     * @return true if this program is running on a 64-bit Windows
     */
    static bool is64BitWindows();

    /**
     * @brief read last lines from a text file
     * @param filename text file
     * @return (lines, error message)
     */
    static std::tuple<QString, QString> readLastLines(const QString filename);

    /**
     * Deletes a directory
     *
     * @param job progress for this task
     * @param aDir_ this directory will be deleted
     * @param firstLevel true = first level will be deleted
     */
    static void removeDirectory(Job* job, const QString &aDir_,
            bool firstLevel=true);

    /**
     * Uses the Shell's IShellLink and IPersistFile interfaces
     * to create and store a shortcut to the specified object.
     *
     * @return error message
     * @param lpszPathObj - address of a buffer containing the path of the object.
     * @param lpszPathLink - address of a buffer containing the path where the
     *      Shell link is to be stored.
     * @param lpszDesc - address of a buffer containing the description of the
     *      Shell link.
     * @param workingDir working directory
     */
    static QString createLink(LPCWSTR lpszPathObj, LPCWSTR lpszPathLink,
            LPCWSTR lpszDesc,
            LPCWSTR workingDir);

    /**
     * Finds the parent directory for a path.
     *
     * @param path a directory
     * @return parent directory without an ending \\
     */
    static QString parentDirectory(const QString& path);

    /**
     * @brief converts the value for how to show programs into the command line
     *     parameter
     * @param programCloseType how to close programs
     * @return e.g. "windows,kill"
     */
    static QString programCloseType2String(DWORD programCloseType);

    /**
     * @return directory like "C:\Program Files"
     */
    static QString getProgramFilesDir();

    /**
     * - lower case
     * - replaces / by \
     * - removes \ at the end
     * - replace multiple occurences of \
     *
     * @param path a file/directory path
     * @param lowerCase true = call .toLower()
     * @return normalized path
     * @threadsafe
     */
    static QString normalizePath(const QString& path, bool lowerCase=true);

    /**
     * - lower case
     * - replaces / by \
     * - removes \ at the end
     * - replace multiple occurences of \
     *
     * @param path a file/directory path
     * @param lowerCase true = call .toLower()
     * @threadsafe
     */
    static void normalizePath2(QString* path, bool lowerCase=true);

    /**
     * @param commandLine command line
     * @param err error message will be stored here or ""
     * @return parts of the command line
     */
    static QStringList parseCommandLine(const QString& commandLine,
        QString* err);

    /**
     * Checks if a file or a directory is an ancestoer of at least one of
     * the specified directories or equals to one of this directories.
     *
     * @param file file or directory.
     * @param dirs directories.
     */
    static bool isOverOrEquals(const QString& file, const QStringList& dirs);

    /**
     * Checks if a file or a directory is under the specified directory or
     * equals to it.
     *
     * @param file file or directory.
     * @param dir directory.
     */
    static bool isUnderOrEquals(const QString &file, const QString &dir);

    /**
     * @brief extracts an icon from a file using the Windows API ExtractIcon()
     * @param iconFile .ico, .exe, etc. Additionally to what is supported by
     *     ExtractIcon() also ",0" and similar at the end of the path name is
     *     supported to load different icons from the same file
     * @return "data:..." or an empty string if the icon cannot be extracted
     */
    static QString extractIconURL(const QString &iconFile);

    /**
     * Validates a GUID
     *
     * @param guid a GUID
     * @return error message or ""
     */
    static QString validateGUID(const QString& guid);

    /**
     * @param type a CSIDL constant like CSIDL_COMMON_PROGRAMS
     * @return directory like
     *     "C:\Documents and Settings\All Users\Start Menu\Programs"
     */
    static QString getShellDir(int type);

    /**
     * Validates an SHA1.
     *
     * @param sha1 a SHA1 value
     * @return an error message or an empty string if SHA1 is a valid SHA1
     */
    static QString validateSHA1(const QString& sha1);

    /**
     * Validates an SHA-256.
     *
     * @param sha256 a SHA-256 value
     * @return an error message or an empty string
     */
    static QString validateSHA256(const QString& sha256);

    /**
     * Formats a Windows error message.
     *
     * @param err see GetLastError() or HRESULT
     * @param errMsg the message will be stored here
     */
    static void formatMessage(DWORD err, QString* errMsg);

    /**
     * Formats a Windows error message.
     *
     * @param err see GetLastError() or HRESULT
     * @param errMsg the message will be stored here
     */
    inline static void formatMessage(HRESULT err, QString* errMsg)
    {
        return formatMessage(static_cast<DWORD>(err), errMsg);
    }

    /**
     * Checks whether a file is somewhere in a directory (at any level).
     *
     * @param file the file
     * @param dir the directory
     */
    static bool isUnder(const QString& file, const QString& dir);

    /**
     * @return full paths to files locked because of running processes
     */
    static QStringList getProcessFiles();

    /**
     * Scans all files in a directory and deletes all links (.lnk) to files
     * in another directory.
     *
     * @param dir links to items in this directory (or any subdirectory) will
     *     be deleted
     * @param d this directory will be scanned for .lnk files
     */
    static void deleteShortcuts(const QString& dir, QDir& d);

    /**
     * @param name file name possible containing not allowed characters (like /)
     * @param rep replacement character for invalid characters
     * @return file name with invalid characters replaced by rep
     */
    static QString makeValidFilename(const QString& name, const QChar rep);

    /**
     * Input text from the console.
     *
     * @return entered text
     */
    static QString inputTextConsole();

    /**
     * Output text to the console.
     *
     * @param txt the text
     * @param stdout_ true = stdout, false = stderr
     */
    static void outputTextConsole(const QString& txt, bool stdout_=true,
            bool useColor=false, WORD attrs=FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    /**
     * Output text to the console. \r\n will be appended automatically.
     *
     * @param txt the text
     * @param stdout_ true = stdout, false = stderr
     */
    static void writeln(const QString& txt, bool stdout_=true);

    /**
     * Is the output redirected?
     *
     * @return true if the output is redirected (e.g. into a file)
     * @param stdout_ true = stdout, false = stderr
     */
    static bool isOutputRedirected(bool stdout_);

    /**
     * @param diff duration
     * @return time
     */
    static QTime durationToTime(time_t diff);

    /**
     * Input a password from the console.
     *
     * @return entered password
     */
    static QString inputPasswordConsole();

    /**
     * Creates a path for a non-existing file/directory based on the start
     * value.
     *
     * @param start base file or directory name
     *     (e.g. C:\Program Files\Prog 1.0). If the file or directory already
     *     exists, "_2", "_3", etc. will be used
     * @param ext file extension (e.g. ".txt")
     * @return non-existing path based on start
     *     (e.g. C:\Program Files\Prog 1.0_2.txt)
     */
    static QString findNonExistingFile(const QString& start, const QString ext);

    /**
     * @return directory with the .exe
     */
    static QString getExeDir();

    /**
     * @return full .exe path and file name
     */
    static QString getExeFile();

    /**
     * This function returns the Windows system directory even on Terminal Services
     * where ::GetWindowsDirectory() returns different value for each user.
     * @return C:\Windows
     * @threadsafe
     */
    static QString getWindowsDir();

    /**
     * Finds the path to cmd.exe. Returns 64-bit cmd.exe on 64-bit systems.
     *
     * @return path to cmd.exe
     */
    static QString findCmdExe();

    /**
     * @return GUIDs for installed products (MSI) in lower case
     */
    static QStringList findInstalledMSIProducts();

    /**
     * @return GUIDs for installed components (MSI) in lower case
     */
    static QStringList findInstalledMSIComponents();

    /**
     * Finds location of an installed MSI product.
     *
     * @param guid product GUID
     * @param err error message will be stored here
     * @return installation location (C:\Program Files\MyProg)
     */
    static QString getMSIProductLocation(const QString& guid, QString* err);

    /**
     * Returns the value of the specified attribute for the specified MSI
     * product.
     *
     * @param guid product GUID
     * @param attribute INSTALLPROPERTY_PRODUCTNAME for the name etc
     * @param err error message will be stored here
     * @return installation location (C:\Program Files\MyProg)
     */
    static QString getMSIProductAttribute(const QString& guid,
            LPCWSTR attribute, QString* err);

    /**
     * Finds the name of an installed MSI product.
     *
     * @param guid product GUID
     * @param err error message will be stored here
     * @return product name
     */
    static QString getMSIProductName(const QString& guid, QString* err);

    /**
     * Replaces the variable references ${{Var}} in the given text.
     *
     * @param txt text
     * @param vars variables and their values
     */
    static QString format(const QString& txt,
            const QMap<QString, QString>& vars);

    /**
     * Compares 2 file paths.
     *
     * @param patha absolute file path
     * @param pathb absolute file path
     * @return true if paths are equal
     */
    static bool pathEquals(const QString& patha, const QString& pathb);

    /**
     * @return Names and GUIDs for installed products (MSI)
     */
    static QStringList findInstalledMSIProductNames();

    /**
     * @param path .DLL file path
     * @return version number or 0.0 it it cannot be determined
     */
    static Version getDLLVersion(const QString& path);

    /**
     * Changes a system environment variable.
     *
     * @param name name of the variable
     * @param value value of the variable
     * @param expandVars true if REG_EXPAND_SZ should be used instead of REG_SZ
     * @param system true = system-wide, false = user environment
     * @return error message or ""
     */
    static QString setSystemEnvVar(const QString& name, const QString& value,
            bool expandVars, bool system);

    /**
     * Reads a system environment variable.
     *
     * @param name name of the variable
     * @param err error message will be stored here
     * @return value value of the variable
     */
    static QString getSystemEnvVar(const QString& name, QString* err);

    /**
     * @param text multiline text
     * @return first non-empty line from text
     */
    static QString getFirstLine(const QString& text);

    /**
     * Notifies other Windows applications that an environment entry like "PATH"
     * was changed.
     */
    static void fireEnvChanged();

    /**
     * @brief creates the task "Npackd\CheckForUpdates" in the Windows scheduler.
     * The task search for updates and shows a notification if any were found.
     * @param enabled is the task enabled?
     * @return error message
     */
    static QString createMSTask(bool enabled);

    /**
     * Asks the user to confirm an operation
     *
     * @return true if y was entered
     */
    static bool confirmConsole(const QString &msg);

    /**
     * @brief running processes
     * @return the list of process identifiers.
     */
    static QVector<DWORD> getProcessIDs();

    /**
     * @brief returns process executable
     * @param hProcess process handle
     * @return process executable path and file name or ""
     */
    static QString getProcessFile(HANDLE hProcess);

    /**
     * @return handles for the top level windows on the desktop
     */
    static QList<HWND> findTopWindows();

    /**
     * @brief closes all processes that lock the specified directory. This
     *     function ignores the current process.
     * @param dir a directory
     * @param stoppedServices the names of stopped Windows services will be
     *     stored here
     */
    static void closeProcessesThatUseDirectory(const QString& dir,
            DWORD cpt, QStringList *stoppedServices);

    /**
     * @brief disconnects all users from all shares that include the specified
     *     directory
     * @param dir a directory
     */
    static void disconnectShareUsersFrom(const QString& dir);

    /**
     * @param dir the directory
     * @return full file path to the executable locking the specified directory
     *     or ""
     */
    static QString findFirstExeLockingDirectory(const QString& dir);

    /**
     * @brief parses the command line and returns the chosen program close type
     * @param cl command line
     * @param err error message will be stored here
     * @return program close type
     */
    static DWORD getProgramCloseType(const CommandLine &cl, QString *err);

    /**
     * @brief maps MSI components to the corresponding products
     * @param components MSI GUIDs of the components in lower case
     * @return Product GUID -> Component GUID. All GUIDs are in lower case
     */
    static QMultiMap<QString, QString> mapMSIComponentsToProducts(
            const QStringList &components);

    /**
     * @brief determines the location of an installed MSI component on disk
     * @param product product GUID
     * @param guid component GUID
     * @param err error message or "" will be stored here
     * @return component path on disk or in the Windows registry. See
     *     MsiGetComponentPath documentation for more details
     */
    static QString getMSIComponentPath(const QString &product,
                                       const QString &guid, QString *err);

    /**
     * @brief computes a check sum for a file
     * @param job job
     * @param file a file
     * @param alg algorithm
     * @return checksum
     */
    static QString fileCheckSum(Job *job, QFile *file,
            QCryptographicHash::Algorithm alg);

    /**
     * @brief appends the contents of one file to another
     * @param from source file name
     * @param to target file name
     * @return error message or ""
     */
    static QString appendFile(const QString from, const QString to);

    /**
     * @brief creates an empty file in the temporary directory
     * @param pattern e.g. "NpackdXXXXXX.log"
     * @return full path to the created file
     */
    static QString createEmptyTempFile(const QString pattern);

    /**
     * @brief unzips a file
     * @param job job
     * @param zipfile .zip file
     * @param outputdir output directory
     */
    static void unzip(Job* job, const QString zipfile, const QString outputdir);

    /**
     * @param job job to monitor the progress. The error message will be set
     *     to a non-empty string if the exit code of the process is not 0.
     * @param where working directory
     * @param path executable
     * @param nativeArguments all native arguments
     * @param outputFile the output will be saved here. Can be 0.
     * @param env additional environemnt variables
     * @param printScriptOutput true = redirect the output to the default output
     *     stream
     * @param unicode true = UTF-16, false = 1 byte system encoding
     * @param wait true = wait for the process to end
     */
    static void executeFile(Job* job, const QString& where,
            const QString& path, const QString &nativeArguments,
            QIODevice *outputFile,
            const QStringList& env,
            bool printScriptOutput=false, bool unicode=true, bool wait=true);

    /**
     * @param job job to monitor the progress. The error message will be set
     *     to a non-empty string if the exit code of the process is not 0.
     * @param where working directory
     * @param path executable
     * @param nativeArguments all native arguments
     * @param outputFile the output will be saved here. The content will be
     *     appended. If this value is "", the output will not be saved anywhere.
     * @param env additional environemnt variables
     * @param write writeUTF16LEBOM write UTF-16 LE BOM mark at the beginning of
     *     the output file?
     * @param printScriptOutput true = redirect the output to the default output
     *     stream
     * @param unicode true = UTF-16, false = 1 byte system encoding
     */
    static void executeFile(Job* job, const QString& where,
            const QString& path, const QString &nativeArguments,
            const QString& outputFile,
            const QStringList& env, bool writeUTF16LEBOM=true,
            bool printScriptOutput=false, bool unicode=true);

    /**
     * @param job job to monitor the progress. The error message will be set
     *     to a non-empty string if the exit code of the process is not 0.
     * @param where working directory
     * @param path relative path to the .bat file
     * @param outputFile the output will be saved here or "" if not available.
     * @param env additional environemnt variables
     * @param printScriptOutput true = redirect the script output to the default
     *     output stream
     * @param unicode true = use the "/U" parameter to cmd.exe
     */
    static void executeBatchFile(Job* job, const QString& where,
            const QString& path, const QString& outputFile,
            const QStringList& env, bool printScriptOutput, bool unicode=true);

    /**
     * @brief reports an event using the Windows Log API
     * @param msg message
     * @param wType information type as in ReportEvent
     *      (e.g. EVENTLOG_INFORMATION_TYPE)
     */
    static void reportEvent(const QString& msg,
            WORD wType=EVENTLOG_INFORMATION_TYPE);

    /**
     * @brief returns the class name of a window or an empty string
     * @param w a window
     * @return class name
     */
    static QString getClassName(HWND w);

    /**
     * @return computer name
     */
    static QString getHostName();

    /**
     * @brief parses the environment strings
     * @param env2 the environment as returned by GetEnvironmentStrings()
     * @return name=value pairs
     */
    static QMap<QString, QString> parseEnv(LPWCH env2);

    /**
     * @brief serializes the specified environment
     * @param env name=value pairs
     * @return serialized environment, e.g. for CreateProcess
     */
    static QByteArray serializeEnv(const QMap<QString, QString> &env);

    /**
     * Checks whether the specified value is a valid URL. The URL cannot be
     * empty. Only http:, https: and file: schemes are supported.
     *
     * @param base URLs will be resolved relative to this one. If this URL is
     *     empty, a relative URL would lead to an error.
     * @param url a string that should be checked. The absolute value will be
     *     stored here if the URL is relative. The value will be trimmed.
     * @param allowEmpty should an empty value be allowed
     * @return error message or an empty string
     */
    static QString checkURL(const QUrl& base, QString *url, bool allowEmpty);

    /**
     * @brief stops a Windows service
     * @param serviceName internal name of the service
     * @param stoppedServices all the stopped services will be stored here
     * @return error message
     */
    static QString stopService(const QString &serviceName,
                               QStringList *stoppedServices);

    /**
     * @brief starts a Windows service
     * @param serviceName internal name of the service
     * @return error message
     */
    static QString startService(const QString &serviceName);

    /**
     * @brief starts a Windows service
     * @param schSCManager Windows services manager
     * @param serviceName internal name of the service
     * @return error message
     */
    static QString startService(SC_HANDLE schSCManager,
            const QString &serviceName);

    /**
     * @brief searches for a service with the specified process ID
     * @param processId ID of a process
     * @param err error message will be stored here
     * @return name of the service or ""
     */
    static QString findService(DWORD processId, QString *err);

    /**
     * @brief checks if the specified directory is shared directly or as a
     *     sub-directory of a shared directory
     * @param dir this directory will be checked
     * @return true = shared
     */
    static bool isDirShared(const QString &dir);

	/**
	* @brief check user for admin privileges
	* @return true = admin privileges
	*/
    static bool hasAdminPrivileges();

    /**
     * @brief returns the content of a QString as LPWSTR = WCHAR* for use in
     *     win32 API functions. The returned data should be considered
     *     read-only.
     *
     * @param s a string
     * @return read-only contents of the string
     */
    inline static LPWSTR toLPWSTR(const QString& s) {
        return reinterpret_cast<LPWSTR>(const_cast<ushort*>(s.utf16()));
    }

    /**
     * @brief returns the content of a QString as LPWSTR = WCHAR* for use in
     *     win32 API functions. The returned data should be considered
     *     read-only.
     *
     * @param s a string
     * @return read-only contents of the string
     */
    inline static LPWSTR toLPWSTR(const wchar_t* s) {
        return reinterpret_cast<LPWSTR>(const_cast<wchar_t*>(s));
    }

    /**
     * Renames a directory. This function makes several attempts and waits
     * between them. Also the directory is copied if it cannot be renamed.
     * If the old directory cannot be removed, it does not lead to an error message
     * in "job".
     *
     * @param job progress for this task
     * @param oldName old directory path
     * @param newName new directory path
     */
    static void renameDirectory(Job *job, const QString &oldName, const QString &newName);

    /**
     * Copies a directory.
     *
     * @param src source path
     * @param dest destination path
     */
    static bool copyDirectory(QString src, QString dest);

    /**
     * @brief search for a task "Npackd\CheckForUpdates" in the Windows scheduler
     * @param err error message will be stored here
     * @return whether the task is enabled
     */
    static bool isTaskEnabled(QString* err);

    /**
     * @brief converts the given SID to a string format.
     * @param pSID
     * @param err error message will be stored here
     * @return SID string
     */
    static QString convertSidToString(PSID pSID, QString* err);

    /**
     * @param err error message will be stored here
     * @return [move] token data or nullptr
     */
    static PTOKEN_USER getUserSID(QString *err);
};

#endif // WPMUTILS_H
