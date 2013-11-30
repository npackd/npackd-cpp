#ifndef NPACKDLIB_H
#define NPACKDLIB_H

#include "npackdlib_global.h"

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A primitive installation/uninstallation operation.
 */
typedef struct _INSTALL_OPERATION {
    /** TRUE = install, FALSE = uninstall */
    BOOL install;

    /** full package name */
    LPCWSTR package;

    /** version number like "1.1" */
    LPCWSTR version;
} INSTALL_OPERATION, *LPINSTALL_OPERATION;


/**
 * A repository.
 */
typedef struct _REPOSITORY {
    /** http:/https: URL for the repository */
    LPCWSTR url;
} REPOSITORY, *LPREPOSITORY;

/**
 * A package
 */
typedef struct _PACKAGE {
    /** full package name */
    LPCWSTR name;

    /** title */
    LPCWSTR title;

    /** multi-line description of this package */
    LPCWSTR description;
} PACKAGE, *LPPACKAGE;

/**
 * A package version
 */
typedef struct _PACKAGE_VERSION {
    /** full package name */
    LPCWSTR package;

    /** version number like "1.1" */
    LPCWSTR version;
} PACKAGE_VERSION, *LPPACKAGE_VERSION;

/**
 * Information about an installed package version
 */
typedef struct _INSTALLED_PACKAGE_VERSION {
    /** full package name */
    LPCWSTR package;

    /** version number like "1.1" */
    LPCWSTR version;

    /** version number like "1.1" */
    LPCWSTR path;
} INSTALLED_PACKAGE_VERSION, *LPINSTALLED_PACKAGE_VERSION;

/**
 * A callback function used to progress.
 *
 * @param progress progress of the operation 0...1
 * @param hint one-line description of the current operation
 * @return FALSE, if the whole operation should be cancelled, TRUE otherwise
 */
typedef BOOL _stdcall *PROGRESSCHANGEPROC(double progress, LPCWSTR hint);

/**
 * Plans installation/uninstallation.
 *
 * @param ninfo number of installed packages will be stored here
 * @param info information about installed packages will be stored here
 * @param pf this function will be called to notify about the progress of this
 *     call.
 * @param err error message will be stored here or 0 if none
 * @return TRUE if the call succeeds
 */
BOOL _stdcall NPACKDLIBSHARED_EXPORT GetInstalledInfo(PDWORD ninfo,
        LPINSTALLED_PACKAGE_VERSION* info,
        PROGRESSCHANGEPROC pf,
        LPCWSTR* err);

/**
 * Plans installation/uninstallation.
 *
 * @param install TRUE = plan installatioon, FALSE = plan uninstallation
 * @package full package name. This package should be installed or uninstalled.
 * @param version version number like "1.1"
 * @param nops number of necessary operations will be stored here. The number
 *     can be also 0.
 * @param ops a pointer to an array of InstallOperationR structs
 *     will be stored here
 * @param pf this function will be called to notify about the progress of this
 *     call.
 * @param err error message will be stored here or 0 if none
 * @return TRUE if the call succeeds
 */
BOOL _stdcall NPACKDLIBSHARED_EXPORT Plan(BOOL install, LPCWSTR package,
        LPCWSTR version, PDWORD nops, LPINSTALL_OPERATION* ops,
        PROGRESSCHANGEPROC pf, LPCWSTR* err);

/**
 * Plans installation/uninstallation.
 *
 * @param nops number of operations
 * @param ops a pointer to an array of installation operations will be stored
 *     here.
 * @param pf this function will be called to notify about the progress of this
 *     call.
 * @param err error message will be stored here or 0 if none
 * @return TRUE if the call succeeds
 */
BOOL _stdcall NPACKDLIBSHARED_EXPORT Process(PDWORD nops,
        LPINSTALL_OPERATION* ops, PROGRESSCHANGEPROC pf, LPCWSTR* err);

/**
 * Searches for packages.
 *
 * @param text a piece of text to search in package description and title
 * @param finstalled TRUE = search only in installed packages, FALSE = search
 *     in all available packages
 * @param offset offset for the first result in the result set (0, 1, ...)
 * @param max maximum number of returned results
 * @param nops number of found packages will be stored here
 * @param ops a pointer to an array of found packages will be stored here
 * @param pf this function will be called to notify about the progress of this
 *     call.
 * @param err error message will be stored here or 0 if none
 * @return TRUE if the call succeeds
 */
BOOL _stdcall NPACKDLIBSHARED_EXPORT Find(LPCWSTR text, BOOL finstalled,
        DWORD offset, DWORD max,
        PDWORD npackages,
        LPPACKAGE* packages, PROGRESSCHANGEPROC pf, LPCWSTR* err);

/**
 * Returns all versions for the specified package.
 *
 * @param package full package name
 * @param nops number of found versions will be stored here
 * @param ops a pointer to an array of found versions will be stored here
 * @param pf this function will be called to notify about the progress of this
 *     call.
 * @param err error message will be stored here or 0 if none
 * @return TRUE if the call succeeds
 */
BOOL _stdcall NPACKDLIBSHARED_EXPORT GetVersions(LPCWSTR package,
        PDWORD nversions,
        LPPACKAGE_VERSION* versions, PROGRESSCHANGEPROC pf, LPCWSTR* err);

/**
 * Returns all defined repositories.
 *
 * @param nreps number of repositories will be stored here
 * @param reps a pointer to an array of found repositories will be stored here
 * @param err error message will be stored here or 0 if none
 * @return TRUE if the call succeeds
 */
BOOL _stdcall NPACKDLIBSHARED_EXPORT GetRepositories(
        PDWORD nreps,
        LPREPOSITORY* reps, LPCWSTR* err);

/**
 * Changes the list of repositories.
 *
 * @param nreps number of repositories
 * @param reps a pointer to an array of repositories
 * @param err error message will be stored here or 0 if none
 * @return TRUE if the call succeeds
 */
BOOL _stdcall NPACKDLIBSHARED_EXPORT SetRepositories(
        DWORD nreps,
        LPREPOSITORY reps, LPCWSTR* err);

#ifdef __cplusplus
}
#endif

#endif // NPACKDLIB_H
