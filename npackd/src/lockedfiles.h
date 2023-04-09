#ifndef LOCKEDFILES_H
#define LOCKEDFILES_H

#include <windows.h>

#include <vector>

#include "QString"

extern "C" {
EXCEPTION_DISPOSITION _catch1( _EXCEPTION_RECORD* exception_record,
    void* err, _CONTEXT* context, void* par);
}

class LockedFiles
{
    /**
     * @brief searches for the processes that somehow lock the specified
     *     directory. This function enumerates the loaded modules (.dll, etc.)
     * @param dir a directory
     * @return list of handles with PROCESS_ALL_ACCESS permissions. These
     *     handles should be closed later using CloseHandle()
     */
    static std::vector<HANDLE> getProcessHandlesLockingDirectory(const QString &dir);

    /**
     * @brief searches for the processes that somehow lock the specified
     *     directory. This function enumerates all files opened by the process.
     * @param dir a directory
     * @return list of handles with PROCESS_QUERY_LIMITED_INFORMATION
     *     permissions. These handles should be closed later using CloseHandle()
     */
    static std::vector<HANDLE> getProcessHandlesLockingDirectory2(const QString &dir);
public:
    LockedFiles();

    static std::vector<HANDLE> getAllProcessHandlesLockingDirectory(const QString &dir);

    /**
     * @param dir the directory
     * @return full file path to the executable locking the specified directory
     *     or ""
     */
    static QString findFirstExeLockingDirectory(const QString& dir);
};

#endif // LOCKEDFILES_H
