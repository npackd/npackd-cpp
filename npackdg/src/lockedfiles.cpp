#include "lockedfiles.h"

#include <psapi.h>
#include <signal.h>

#include <thread>
#include <condition_variable>
#include <unordered_set>

#include "wpmutils.h"

//#define CCH_RM_MAX_APP_NAME 255
//#define CCH_RM_MAX_SVC_NAME 63
//#define CCH_RM_SESSION_KEY sizeof(GUID) * 2
//#include <restartmanager.h>

/*
int __cdecl test(PCWSTR pszFile)
{
    // since Vista!
    DWORD dwSession;
    WCHAR szSessionKey[CCH_RM_SESSION_KEY+1] = { 0 };
    DWORD dwError = RmStartSession(&dwSession, 0, szSessionKey);
    wprintf(L"RmStartSession returned %d\r\n", dwError);
    if (dwError == ERROR_SUCCESS) {
        dwError = RmRegisterResources(dwSession, 1, &pszFile,
                                      0, NULL, 0, NULL);
        wprintf(L"RmRegisterResources(%ls) returned %d\r\n",
                pszFile, dwError);
        if (dwError == ERROR_SUCCESS) {
            DWORD dwReason = RmRebootReasonNone;
            UINT i;
            UINT nProcInfoNeeded = 1;
            UINT nProcInfo = 10;
            RM_PROCESS_INFO rgpi[10];
            dwError = RmGetList(dwSession, &nProcInfoNeeded,
                                &nProcInfo, rgpi, &dwReason);
            wprintf(L"RmGetList returned %d\r\n", dwError);
            if (dwError == ERROR_SUCCESS) {
                wprintf(L"RmGetList returned %d infos (%d needed)\r\n",
                        nProcInfo, nProcInfoNeeded);
                for (i = 0; i < nProcInfo; i++) {
                    wprintf(L"%d.ApplicationType = %d\r\n", i,
                            rgpi[i].ApplicationType);
                    wprintf(L"%d.strAppName = %ls\r\n", i,
                            rgpi[i].strAppName);
                    wprintf(L"%d.Process.dwProcessId = %d\r\n", i,
                            rgpi[i].Process.dwProcessId);
                    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                                  FALSE, rgpi[i].Process.dwProcessId);
                    if (hProcess) {
                        FILETIME ftCreate, ftExit, ftKernel, ftUser;
                        if (GetProcessTimes(hProcess, &ftCreate, &ftExit,
                                            &ftKernel, &ftUser) &&
                                CompareFileTime(&rgpi[i].Process.ProcessStartTime,
                                                &ftCreate) == 0) {
                            WCHAR sz[MAX_PATH];
                            DWORD cch = MAX_PATH;
                            if (QueryFullProcessImageNameW(hProcess, 0, sz, &cch) &&
                                    cch <= MAX_PATH) {
                                wprintf(L"  = %ls\r\n", sz);
                            }
                        }
                        CloseHandle(hProcess);
                    }
                }
            }
        }
        RmEndSession(dwSession);
    }
    return 0;
}
*/

// definitions for .getProcessHandlesLockingDirectory2
#define NT_SUCCESS(x) ((x) >= 0)
const NTSTATUS STATUS_INFO_LENGTH_MISMATCH = 0xc0000004;

#define SystemHandleInformation 16
#define ObjectBasicInformation 0
#define ObjectNameInformation 1
#define ObjectTypeInformation 2

typedef NTSTATUS (NTAPI *_NtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );
typedef NTSTATUS (NTAPI *_NtDuplicateObject)(
    HANDLE SourceProcessHandle,
    HANDLE SourceHandle,
    HANDLE TargetProcessHandle,
    PHANDLE TargetHandle,
    ACCESS_MASK DesiredAccess,
    ULONG Attributes,
    ULONG Options
    );
typedef NTSTATUS (NTAPI *_NtQueryObject)(
    HANDLE ObjectHandle,
    ULONG ObjectInformationClass,
    PVOID ObjectInformation,
    ULONG ObjectInformationLength,
    PULONG ReturnLength
    );

typedef struct _UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _SYSTEM_HANDLE
{
    ULONG ProcessId;
    BYTE ObjectTypeNumber;
    BYTE Flags;
    USHORT Handle;
    PVOID Object;
    ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE, *PSYSTEM_HANDLE;

typedef struct _SYSTEM_HANDLE_INFORMATION
{
    ULONG HandleCount;
    SYSTEM_HANDLE Handles[1];
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

typedef enum _POOL_TYPE
{
    NonPagedPool,
    PagedPool,
    NonPagedPoolMustSucceed,
    DontUseThisType,
    NonPagedPoolCacheAligned,
    PagedPoolCacheAligned,
    NonPagedPoolCacheAlignedMustS
} POOL_TYPE, *PPOOL_TYPE;

typedef struct _OBJECT_TYPE_INFORMATION
{
    UNICODE_STRING Name;
    ULONG TotalNumberOfObjects;
    ULONG TotalNumberOfHandles;
    ULONG TotalPagedPoolUsage;
    ULONG TotalNonPagedPoolUsage;
    ULONG TotalNamePoolUsage;
    ULONG TotalHandleTableUsage;
    ULONG HighWaterNumberOfObjects;
    ULONG HighWaterNumberOfHandles;
    ULONG HighWaterPagedPoolUsage;
    ULONG HighWaterNonPagedPoolUsage;
    ULONG HighWaterNamePoolUsage;
    ULONG HighWaterHandleTableUsage;
    ULONG InvalidAttributes;
    GENERIC_MAPPING GenericMapping;
    ULONG ValidAccess;
    BOOLEAN SecurityRequired;
    BOOLEAN MaintainHandleCount;
    USHORT MaintainTypeList;
    POOL_TYPE PoolType;
    ULONG PagedPoolUsage;
    ULONG NonPagedPoolUsage;
} OBJECT_TYPE_INFORMATION, *POBJECT_TYPE_INFORMATION;

// end of definitions for .getProcessHandlesLockingDirectory2

std::vector<HANDLE> LockedFiles::getProcessHandlesLockingDirectory(const QString& dir)
{
    std::vector<HANDLE> r;

    std::vector<DWORD> aiPID = WPMUtils::getProcessIDs();

    // How many processes are there?
    int iNumProc = aiPID.size();

    // Get and match the name of each process
    for (int i = 0; i < iNumProc; i++) {
        // First, get a handle to the process
        HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, aiPID[i]);

        // Now, get the process name
        if (hProc) {
            bool found = false;

            // .exe
            DWORD len = MAX_PATH;
            WCHAR szName[MAX_PATH];
            if (QueryFullProcessImageName(hProc, 0, szName, &len)) {
                QString s = QString::fromUtf16(
                        reinterpret_cast<ushort*>(szName),
                        static_cast<int>(len));
                if (WPMUtils::pathEquals(s, dir) ||
                        WPMUtils::isUnder(s, dir)) {
                    found = true;
                }
            }

            // .dll
            if (!found) {
                HMODULE hMod;
                DWORD iCbneeded;
                if (EnumProcessModules(hProc, &hMod, sizeof(hMod), &iCbneeded)) {
                    if (iCbneeded != 0) {
                        HMODULE* modules = new HMODULE[iCbneeded / sizeof(HMODULE)];
                        if (EnumProcessModules(hProc, modules, iCbneeded,
                                &iCbneeded)) {

                            for (DWORD i = 0; i < iCbneeded / sizeof(HMODULE); i++) {
                                if (GetModuleFileNameEx(hProc, modules[i], szName, len)) {
                                    QString s = QString::fromUtf16(
                                            reinterpret_cast<ushort*>(szName),
                                            static_cast<int>(len));
                                    if (WPMUtils::pathEquals(s, dir) ||
                                            WPMUtils::isUnder(s, dir)) {
                                        found = true;
                                        break;
                                    }
                                }
                            }
                        }
                        delete[] modules;
                    }
                }
            }

            if (found) {
                r.push_back(hProc);
                hProc = nullptr;
            } else {
                CloseHandle(hProc);
            }
        }
    }

    return r;
}

// converts
// "\Device\HarddiskVolume3"                                -> "E:"
// "\Device\HarddiskVolume3\Temp"                           -> "E:\Temp"
// "\Device\HarddiskVolume3\Temp\transparent.jpeg"          -> "E:\Temp\transparent.jpeg"
// "\Device\Harddisk1\DP(1)0-0+6\foto.jpg"                  -> "I:\foto.jpg"
// "\Device\TrueCryptVolumeP\Data\Passwords.txt"            -> "P:\Data\Passwords.txt"
// "\Device\Floppy0\Autoexec.bat"                           -> "A:\Autoexec.bat"
// "\Device\CdRom1\VIDEO_TS\VTS_01_0.VOB"                   -> "H:\VIDEO_TS\VTS_01_0.VOB"
// "\Device\Serial1"                                        -> "COM1"
// "\Device\USBSER000"                                      -> "COM4"
// "\Device\Mup\ComputerName\C$\Boot.ini"                   -> "\\ComputerName\C$\Boot.ini"
// "\Device\LanmanRedirector\ComputerName\C$\Boot.ini"      -> "\\ComputerName\C$\Boot.ini"
// "\Device\LanmanRedirector\ComputerName\Shares\Dance.m3u" -> "\\ComputerName\Shares\Dance.m3u"
// returns an error for any other device type
std::unordered_map<QString, QString> mapDevices2Drives() {
    std::unordered_map<QString, QString> devices2drives;

    //DWORD u32_Error;

    /*
    if (wcsnicmp(u16_NTPath, L"\\Device\\Serial", 14) == 0 || // e.g. "Serial1"
        wcsnicmp(u16_NTPath, L"\\Device\\UsbSer", 14) == 0)   // e.g. "USBSER000"
    {
        HKEY h_Key;
        if (u32_Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Hardware\\DeviceMap\\SerialComm", 0, KEY_QUERY_VALUE, &h_Key))
            return u32_Error;

        WCHAR u16_ComPort[50];

        DWORD u32_Type;
        DWORD u32_Size = sizeof(u16_ComPort);
        if (u32_Error = RegQueryValueEx(h_Key, u16_NTPath, 0, &u32_Type, (BYTE*)u16_ComPort, &u32_Size))
        {
            RegCloseKey(h_Key);
            return ERROR_UNKNOWN_PORT;
        }

        *ps_DosPath = u16_ComPort;
        RegCloseKey(h_Key);
        return 0;
    }

    if (wcsnicmp(u16_NTPath, L"\\Device\\LanmanRedirector\\", 25) == 0) // Win XP
    {
        *ps_DosPath  = L"\\\\";
        *ps_DosPath += (u16_NTPath + 25);
        return 0;
    }

    if (wcsnicmp(u16_NTPath, L"\\Device\\Mup\\", 12) == 0) // Win 7
    {
        *ps_DosPath  = L"\\\\";
        *ps_DosPath += (u16_NTPath + 12);
        return 0;
    }
    */

    const size_t INITIAL_SIZE = 300;
    WCHAR* drives = new WCHAR[INITIAL_SIZE + 1];
    DWORD len = GetLogicalDriveStrings(INITIAL_SIZE, drives);
    if (len > 0) {
        if (len > INITIAL_SIZE) {
            delete[] drives;
            drives = new WCHAR[len + 1];
            len = GetLogicalDriveStrings(INITIAL_SIZE, drives);
        }
        if (len > 0 && len <= INITIAL_SIZE) {
            WCHAR* drv = drives;
            while (*drv) {
                QString logicalDrive;
                logicalDrive.setUtf16((ushort*) drv, (int)wcslen(drv));

                if (logicalDrive.length() >= 2) {
                    logicalDrive.chop(logicalDrive.length() - 2);

                    WCHAR* devices = new WCHAR[2000];

                    // may return multiple strings!
                    // returns very weird strings for network shares
                    if (QueryDosDevice(WPMUtils::toLPWSTR(logicalDrive), devices,
                            2000)) {
                        WCHAR* device = devices;
                        while (*device) {
                            QString logicalDevice;
                            logicalDevice.setUtf16((ushort*) device,
                                (int)wcslen(device));
                            devices2drives.insert(
                                    {logicalDevice + '\\',
                                    logicalDrive + '\\'});

                            device += wcslen(device) + 1;
                        }
                    }

                    delete[] devices;
                }

                drv += wcslen(drv) + 1;
            }
        }
    }

    delete[] drives;

    return devices2drives;
}

class GetObjectNameThread {
    // this should be initialized before std::thread
    HANDLE h;

    std::thread thr;

    // access under mutex_
    QString name;

    // access under mutex_
    bool done{false};

    std::mutex mutex_;
    std::condition_variable cv;

    void run0();
public:
    GetObjectNameThread(HANDLE h): h(h), thr(&GetObjectNameThread::run, this) {
    }

    void run();

    QString getName();
};

#if defined(__MINGW32__) || defined(__MINGW64__)
extern "C" {
EXCEPTION_DISPOSITION _catch1( _EXCEPTION_RECORD* exception_record,
    void* err, _CONTEXT* context, void* par)
{
    throw exception_record;
}
}

void GetObjectNameThread::run()
{
    try {
        __try1(_catch1)
        run0();
        __except1
    } catch(_EXCEPTION_RECORD* seh) {
        qCWarning(npackd) << "getting an object name via NtQueryObject failed";
    };
}
#else
// Ref: https://docs.microsoft.com/de-de/cpp/cpp/try-except-statement?view=msvc-170
EXCEPTION_DISPOSITION __catch(struct _EXCEPTION_POINTERS* exception_data)
{
    qCWarning(npackd) << "getting an object name via NtQueryObject failed";
    return ExceptionContinueExecution;
}

void GetObjectNameThread::run()
{
    __try
    {
        run0();
    }
    __except (__catch(GetExceptionInformation()))
    {
    }
}
#endif

void GetObjectNameThread::run0()
{
    HMODULE module = GetModuleHandleA("ntdll.dll");
    _NtQueryObject NtQueryObject;
    NtQueryObject = (_NtQueryObject) GetProcAddress(module, "NtQueryObject");

    PVOID objectNameInfo = nullptr;
    ULONG returnLength = 0;

    objectNameInfo = malloc(0x1000);
    bool ok = NT_SUCCESS(NtQueryObject(h, ObjectNameInformation,
            objectNameInfo, 0x1000, &returnLength));
    if (!ok) {
        /* Reallocate the buffer and try again. */
        objectNameInfo = realloc(objectNameInfo, returnLength);
        ok = NT_SUCCESS(NtQueryObject(h, ObjectNameInformation,
                objectNameInfo, returnLength, NULL));
    }

    if (ok) {
        UNICODE_STRING objectName = {};

        /* Cast our buffer into an UNICODE_STRING. */
        objectName = *(PUNICODE_STRING) objectNameInfo;

        if (objectName.Length && objectName.Buffer) {
            {
                std::lock_guard<std::mutex> lck(mutex_);
                name.setUtf16((const ushort*) objectName.Buffer,
                        objectName.Length / 2);
                done = true;
            }
        }
    }

    free(objectNameInfo);

    cv.notify_one();
}

QString GetObjectNameThread::getName() {
    bool d;
    QString r;
    {
        std::unique_lock<std::mutex> lck(mutex_);
        cv.wait_for(lck, std::chrono::seconds(1),
                [this]{ return done; });
        d = done;
        r = name;
    }

    try {
        if (d) {
            // Finished
            thr.join();
        } else {
            thr.detach();

            // Not finished
            TerminateThread(reinterpret_cast<HANDLE>(
                    thr.native_handle()), 255);
        }
    } catch (const std::system_error& e) {
        qCWarning(npackd) << "GetObjectNameThread::getName" << e.what();
    }

    return r;
}

std::vector<HANDLE> LockedFiles::getProcessHandlesLockingDirectory2(const QString &dir) {
    std::unordered_map<QString, QString> devices2drives = mapDevices2Drives();

    std::vector<HANDLE> result;

    HMODULE module = GetModuleHandleA("ntdll.dll");
    if (module == nullptr) {
        return result;
    }

    _NtQuerySystemInformation NtQuerySystemInformation;
    _NtDuplicateObject NtDuplicateObject;
    _NtQueryObject NtQueryObject;
    NtQuerySystemInformation = (_NtQuerySystemInformation)
            GetProcAddress(module, "NtQuerySystemInformation");
    NtDuplicateObject = (_NtDuplicateObject) GetProcAddress(module,
            "NtDuplicateObject");
    NtQueryObject = (_NtQueryObject) GetProcAddress(module, "NtQueryObject");

    PSYSTEM_HANDLE_INFORMATION handleInfo;
    ULONG handleInfoSize = 0x10000;
    ULONG i;

    handleInfo = (PSYSTEM_HANDLE_INFORMATION) malloc(handleInfoSize);

    // NtQuerySystemInformation won't give us the correct buffer size,
    // so we guess by doubling the buffer size.
    NTSTATUS status;
    while ((status = NtQuerySystemInformation(
            SystemHandleInformation, handleInfo, handleInfoSize,
            nullptr)) == STATUS_INFO_LENGTH_MISMATCH) {
        handleInfo = (PSYSTEM_HANDLE_INFORMATION) realloc(handleInfo,
                handleInfoSize *= 2);
    }

    // NtQuerySystemInformation stopped giving us STATUS_INFO_LENGTH_MISMATCH.
    if (!NT_SUCCESS(status)) {
        handleInfo->HandleCount = 0;
    }

    std::unordered_set<ULONG> usedProcessIds;
    for (i = 0; i < handleInfo->HandleCount; i++) {
        bool ok = true;

        SYSTEM_HANDLE handle = handleInfo->Handles[i];
        HANDLE dupHandle = INVALID_HANDLE_VALUE;
        POBJECT_TYPE_INFORMATION objectTypeInfo = nullptr;

        HANDLE processHandle = INVALID_HANDLE_VALUE;

        if (usedProcessIds.count(handle.ProcessId) > 0) {
            ok = false;
        }

        if (ok) {
            if (!(processHandle = OpenProcess(PROCESS_DUP_HANDLE |
                    PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                    handle.ProcessId))) {
                // Could not open PID %d! (Don't try to open a system process.)
                ok = false;
                processHandle = INVALID_HANDLE_VALUE;

                // qCDebug(npackd) << "OpenProcess ok";
            }
        }

        if (ok) {
            /* Duplicate the handle so we can query it. */
            if (!NT_SUCCESS(NtDuplicateObject(
                processHandle,
                (HANDLE) (uintptr_t) handle.Handle,
                GetCurrentProcess(),
                &dupHandle,
                0,
                0,
                0))) {
                ok = false;
                dupHandle = INVALID_HANDLE_VALUE;

                // qCDebug(npackd) << "NtDuplicateObject ok";
            }
        }

        QString type;

        if (ok) {
            /* Query the object type. */
            const size_t SZ = sizeof(_OBJECT_TYPE_INFORMATION) * 200;
            objectTypeInfo = (POBJECT_TYPE_INFORMATION) malloc(SZ);
            if (!NT_SUCCESS(NtQueryObject(
                dupHandle,
                ObjectTypeInformation,
                objectTypeInfo,
                SZ,
                NULL))) {
                ok = false;
            } else {
                // qCDebug(npackd) << "NtQueryObject ok";
                type.setUtf16((const ushort*) objectTypeInfo->Name.Buffer,
                        objectTypeInfo->Name.Length / 2);
                if (type != "File")
                    ok = false;
            }
        }

        if (ok) {
            // Query the object name (unless it has an access of
            // 0x0012019f, on which NtQueryObject could hang.
            if (handle.GrantedAccess == 0x0012019f) {
                ok = false;
            }
        }

        QString name;

        if (ok) {
            /*
            qCDebug(npackd) << "NtQueryObject start" << handle.Flags
                    << handle.GrantedAccess << handle.ProcessId
                    << handle.ObjectTypeNumber << handle.Handle;
                    */

            // retrieving the name of a handle may hang for pipes
            GetObjectNameThread t(dupHandle);
            name = t.getName();
            ok = !name.isEmpty();

            // qCDebug(npackd) << "NtQueryObject end";
        }

        if (ok) {
            for (auto i = devices2drives.begin(); i != devices2drives.end(); ++i) {
                if (name.startsWith(i->first)) {
                    name = i->second + name.mid(i->first.length());

                    qCDebug(npackd) << "File object converted" << name;

                    if (QFileInfo::exists(name) &&
                            WPMUtils::isUnderOrEquals(name, dir)) {
                        result.push_back(processHandle);
                        processHandle = INVALID_HANDLE_VALUE;
                        usedProcessIds.insert(handle.ProcessId);
                    }
                    break;
                }
            }
        }

        free(objectTypeInfo);

        if (dupHandle != INVALID_HANDLE_VALUE)
            CloseHandle(dupHandle);

        if (processHandle != INVALID_HANDLE_VALUE)
            CloseHandle(processHandle);
    }

    free(handleInfo);

    return result;
}

std::vector<HANDLE> LockedFiles::getAllProcessHandlesLockingDirectory(const QString& dir)
{
    std::vector<HANDLE> ps0 = LockedFiles::getProcessHandlesLockingDirectory(dir);

    //qCDebug(npackd) << "getProcessHandlesLockingDirectory";

    std::vector<HANDLE> ps = LockedFiles::getProcessHandlesLockingDirectory2(dir);

    ps.insert(ps.end(), ps0.begin(), ps0.end());

    return ps;
}

QString LockedFiles::findFirstExeLockingDirectory(const QString &dir)
{
    QString r;

    std::vector<HANDLE> ps = LockedFiles::getProcessHandlesLockingDirectory(dir);

    for (auto p: ps) {
        r = WPMUtils::getProcessFile(p);
        if (!r.isEmpty())
            break;
    }

    for (auto p: ps) {
        CloseHandle(p);
    }

    return r;
}

LockedFiles::LockedFiles()
{

}
