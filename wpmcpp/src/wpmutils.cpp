// for restartmanager.h
//#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <wininet.h>
#include <stdlib.h>
#include <time.h>
#include "msi.h"
#include <shellapi.h>
#include <string>
#include <initguid.h>
#include <ole2.h>
#include <wchar.h>
#include <limits>
#include <inttypes.h>
#include <lm.h>

//#define CCH_RM_MAX_APP_NAME 255
//#define CCH_RM_MAX_SVC_NAME 63
//#define CCH_RM_SESSION_KEY sizeof(GUID) * 2
//#include <restartmanager.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QString>
#include <QFile>
#include <QCryptographicHash>
#include <QFile>
#include <QVariant>
#include <QBuffer>
#include <QByteArray>
#include <QUrl>

#include <quazip.h>
#include <quazipfile.h>

// reduces the size of NpackdCL by 5 MiB
#ifdef QT_GUI_LIB
#include <QImage>
#endif

#include "wpmutils.h"
#include "version.h"
#include "windowsregistry.h"
#include "mstask.h"

bool WPMUtils::debug = false;

QAtomicInt WPMUtils::nextNamePipeId;

HANDLE WPMUtils::hEventLog = 0;

const char* WPMUtils::UCS2LE_BOM = "\xFF\xFE";

const char* WPMUtils::CRLF = "\r\n";

HRTimer WPMUtils::timer(2);


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

WPMUtils::WPMUtils()
{
}

/*
void WPMUtils::createMSTask()
{
    // this function requires better error handling
    ITaskScheduler *pITS;
    HRESULT hr = CoCreateInstance(CLSID_CTaskScheduler,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_ITaskScheduler,
            (void **) &pITS);

    ITask *pITask;
    LPCWSTR lpcwszTaskName;
    lpcwszTaskName = L"Test Task";
    hr = pITS->NewWorkItem(L"NpackdTest5", CLSID_CTask,
                         IID_ITask,
                         (IUnknown**) &pITask);

    //Release ITaskScheduler interface.
    pITS->Release();

    // !!!!!!!!! if (FAILED(hr))

    IPersistFile *pIPersistFile;

    // save to disk
    hr = pITask->QueryInterface(IID_IPersistFile,
            (void **)&pIPersistFile);


    hr = pIPersistFile->Save(NULL,
                       TRUE);
    pIPersistFile->Release();

    pITask->EditWorkItem(0, 0);
    pITask->Release();

}
*/

QString WPMUtils::parentDirectory(const QString& path)
{
    QString p = path;
    p = p.replace('/', '\\');
    int index = p.lastIndexOf('\\');
    return p.left(index);
}

QString WPMUtils::programCloseType2String(DWORD programCloseType)
{
    QString r;

    if (programCloseType & WPMUtils::CLOSE_WINDOW) {
        r += "c";
    }

    if (programCloseType & WPMUtils::DISABLE_SHARES) {
        r += "s";
    }

    if (programCloseType & WPMUtils::KILL_PROCESS) {
        r += "k";
    }

    return r;
}

QString WPMUtils::getProgramFilesDir()
{
    QString ret;

    if (is64BitWindows()) {
        WindowsRegistry wr;
        QString err = wr.open(HKEY_LOCAL_MACHINE,
                "SOFTWARE\\Microsoft\\Windows\\CurrentVersion", false,
                KEY_READ);

        if (err.isEmpty()) {
            ret = wr.get("ProgramFilesDir", &err);
            if (!err.isEmpty())
                ret = "";
        }
    }

    if (ret.isEmpty()) {
        WCHAR dir[MAX_PATH];
        SHGetFolderPath(0, CSIDL_PROGRAM_FILES, NULL, 0, dir);
        ret = QString::fromUtf16(reinterpret_cast<ushort*>(dir));
    }

    return ret;
}

QString WPMUtils::normalizePath(const QString& path, bool lowerCase)
{
    QString r = path;
    normalizePath2(&r, lowerCase);
    return r;
}

void WPMUtils::normalizePath2(QString* path, bool lowerCase)
{
    if (lowerCase)
        *path = path->toLower();
    path->replace('/', '\\');

    int newlen = path->length();
    int oldlen = newlen + 1;
    while (oldlen != newlen) {
        path->replace("\\\\", "\\");
        oldlen = newlen;
        newlen = path->length();
    }

    if (path->endsWith('\\'))
        path->chop(1);
    if (path->endsWith('.'))
        path->chop(1);
}

QStringList WPMUtils::parseCommandLine(const QString& commandLine,
    QString* err) {
    *err = "";

    QStringList params;

    int nArgs;
    LPWSTR* szArglist = CommandLineToArgvW((WCHAR*) commandLine.utf16(), &nArgs);
    if (NULL == szArglist) {
        *err = QObject::tr("CommandLineToArgvW failed");
    } else {
        for(int i = 0; i < nArgs; i++) {
            QString s;
            s.setUtf16((ushort*) szArglist[i], wcslen(szArglist[i]));
            params.append(s);
        }
        LocalFree(szArglist);
    }

    return params;
}

bool WPMUtils::isOverOrEquals(const QString& file, const QStringList& dirs)
{
    bool r = false;
    for (int j = 0; j < dirs.count(); j++) {
        const QString& dir = dirs.at(j);
        if (WPMUtils::pathEquals(file, dir) ||
                WPMUtils::isUnder(dir, file)) {
            r = true;
            break;
        }
    }

    return r;
}

#ifdef QT_GUI_LIB

// Qt 5.2.1
// qtbase/src/gui/image/qpixmap_win.cpp:qt_pixmapFromWinHICON has a bug.
// It crashes in the following line sometimes:
//     return QPixmap::fromImage(image);
// => this function only uses QImage
QImage my_qt_pixmapFromWinHICON(HICON icon)
{
    // qDebug() << "my_qt_pixmapFromWinHICON 0";

    HDC screenDevice = GetDC(0);
    HDC hdc = CreateCompatibleDC(screenDevice);
    ReleaseDC(0, screenDevice);

    ICONINFO iconinfo;
    bool result = GetIconInfo(icon, &iconinfo);
    if (!result)
        qWarning("QPixmap::fromWinHICON(), failed to GetIconInfo()");

    // qDebug() << "my_qt_pixmapFromWinHICON 1";

    int w = 0;
    int h = 0;
    if (!iconinfo.xHotspot || !iconinfo.yHotspot) {
        // We could not retrieve the icon size via GetIconInfo,
        // so we try again using the icon bitmap.
        BITMAP bm;
        int result = GetObject(iconinfo.hbmColor, sizeof(BITMAP), &bm);
        if (!result) result = GetObject(iconinfo.hbmMask, sizeof(BITMAP), &bm);
        if (!result) {
            qWarning("QPixmap::fromWinHICON(), failed to retrieve icon size");
            return QImage();
        }
        w = bm.bmWidth;
        h = bm.bmHeight;
    } else {
        // x and y Hotspot describes the icon center
        w = iconinfo.xHotspot * 2;
        h = iconinfo.yHotspot * 2;
    }
    const DWORD dwImageSize = w * h * 4;

    // qDebug() << "my_qt_pixmapFromWinHICON 2" << w << h;

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFO);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage   = dwImageSize;

    uchar* bits;

    // qDebug() << "my_qt_pixmapFromWinHICON 3";

    HBITMAP winBitmap = CreateDIBSection(hdc, &bmi,
            DIB_RGB_COLORS, (void**) &bits, 0, 0);
    if (winBitmap )
        memset(bits, 0xff, dwImageSize);
    if (!winBitmap) {
        qWarning("QPixmap::fromWinHICON(), failed to CreateDIBSection()");
        return QImage();
    }

    HGDIOBJ oldhdc = (HBITMAP)SelectObject(hdc, winBitmap);
    if (!DrawIconEx( hdc, 0, 0, icon, w, h, 0, 0, DI_NORMAL))
        qWarning("QPixmap::fromWinHICON(), failed to DrawIcon()");

    uint mask = 0xff000000;
    // Create image and copy data into image.
    QImage image(w, h, QImage::Format_ARGB32);

    if (!image.isNull()) { // failed to alloc?
        int bytes_per_line = w * sizeof(QRgb);
        for (int y=0; y < h; ++y) {
            QRgb *dest = (QRgb *) image.scanLine(y);
            const QRgb *src = (const QRgb *) (bits + y * bytes_per_line);
            for (int x=0; x < w; ++x) {
                dest[x] = src[x];
            }
        }
    }

    // qDebug() << "my_qt_pixmapFromWinHICON 4";

    if (!DrawIconEx( hdc, 0, 0, icon, w, h, 0, 0, DI_MASK))
        qWarning("QPixmap::fromWinHICON(), failed to DrawIcon()");
    if (!image.isNull()) { // failed to alloc?
        int bytes_per_line = w * sizeof(QRgb);
        for (int y=0; y < h; ++y) {
            QRgb *dest = (QRgb *) image.scanLine(y);
            const QRgb *src = (const QRgb *) (bits + y * bytes_per_line);
            for (int x=0; x < w; ++x) {
                if (!src[x])
                    dest[x] = dest[x] | mask;
            }
        }
    }

    SelectObject(hdc, oldhdc); //restore state
    DeleteObject(winBitmap);
    DeleteDC(hdc);

    // qDebug() << "my_qt_pixmapFromWinHICON 5";

    return image;
}

QString WPMUtils::extractIconURL(const QString& iconFile)
{
    // qDebug() << "extractIconURL 0";

    QString res;
    QString icon = iconFile.trimmed();
    if (!icon.isEmpty()) {
        // qDebug() << "extractIconURL 1";

        UINT iconIndex = 0;
        int index = icon.lastIndexOf(',');
        if (index > 0) {
            QString number = icon.mid(index + 1);
            bool ok;
            int v = number.toInt(&ok);
            if (ok) {
                iconIndex = (UINT) v;
                icon = icon.left(index);
            }
        }

        // qDebug() << "extractIconURL 2" << icon << iconIndex;

        HICON ic = ExtractIcon(GetModuleHandle(NULL),
                (LPCWSTR) icon.utf16(), iconIndex);

        // qDebug() << "extractIconURL 2.1" << ((UINT_PTR) ic);

        if (((UINT_PTR) ic) > 1) {
            // qDebug() << "extractIconURL 3" << ii << info.fIcon << info.xHotspot;

            QImage pm = my_qt_pixmapFromWinHICON(ic); // QtWin::fromHICON(ic);

            // qDebug() << "extractIconURL 4";

            if (!pm.isNull() && pm.width() > 0 &&
                    pm.height() > 0) {
                // qDebug() << "extractIconURL 5";

                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                pm.save(&buffer, "PNG");
                res = QString("data:image/png;base64,") +
                        bytes.toBase64();

                // qDebug() << "extractIconURL 6";
            }
            DestroyIcon(ic);

            // qDebug() << "extractIconURL 7";
        }
    }

    return res;
}
#else
QString WPMUtils::extractIconURL(const QString& iconFile)
{
    return "";
}
#endif

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

bool WPMUtils::isUnderOrEquals(const QString& file, const QString& dir)
{
    return WPMUtils::pathEquals(file, dir) ||
                WPMUtils::isUnder(file, dir);
}

QString WPMUtils::validateGUID(const QString& guid)
{
    QString err;
    if (guid.length() != 38) {
        err = QObject::tr("A GUID must be 38 characters long");
    } else {
        for (int i = 0; i < guid.length(); i++) {
            QChar c = guid.at(i);
            bool valid;
            if (i == 9 || i == 14 || i == 19 || i == 24) {
                valid = c == '-';
            } else if (i == 0) {
                valid = c == '{';
            } else if (i == 37) {
                valid = c == '}';
            } else {
                valid = (c >= '0' && c <= '9') ||
                        (c >= 'a' && c <= 'f') ||
                        (c >= 'A' && c <= 'F');
            }

            if (!valid) {
                err = QString(QObject::tr("Wrong character at position %1")).
                        arg(i + 1);
                break;
            }
        }
    }
    return err;
}

bool WPMUtils::isUnder(const QString &file, const QString &dir)
{
    QString f = file;
    QString d = dir;

    normalizePath2(&f);
    normalizePath2(&d);

    if (!d.endsWith('\\'))
        d.append('\\');

    return f.startsWith(d, Qt::CaseInsensitive);
}

void WPMUtils::formatMessage(DWORD err, QString* errMsg)
{
    HLOCAL pBuffer;
    DWORD n;

    if (err >= INTERNET_ERROR_BASE && err <= INTERNET_ERROR_LAST) {
        // wininet.dll-errors
        n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_HMODULE,
                       GetModuleHandle(L"wininet.dll"),
                       err, 0, (LPTSTR)&pBuffer, 0, 0);
    } else {
        n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_SYSTEM,
                       0, err, 0, (LPTSTR)&pBuffer, 0, 0);
    }
    if (n == 0)
        errMsg->append(QString(QObject::tr("Error %1")).arg(err));
    else {
        QString msg;
        msg.setUtf16((ushort*) pBuffer, n);
        errMsg->append(QString(QObject::tr("Error %1: %2")).arg(err).arg(msg));
        LocalFree(pBuffer);
    }
}

QString WPMUtils::getInstallationDirectory()
{
    QString v;

    WindowsRegistry npackd;
    QString err = npackd.open(
            HKEY_LOCAL_MACHINE, "Software\\Npackd\\Npackd", false, KEY_READ);
    if (err.isEmpty()) {
        v = npackd.get("path", &err);
    }

    if (v.isEmpty()) {
        err = npackd.open(HKEY_LOCAL_MACHINE,
                "Software\\WPM\\Windows Package Manager", false,
                KEY_READ);
        if (err.isEmpty()) {
            v = npackd.get("path", &err);
        }
    }

    if (v.isEmpty())
        v = WPMUtils::getProgramFilesDir();

    return v;
}

QString WPMUtils::setInstallationDirectory(const QString& dir)
{
    WindowsRegistry m(HKEY_LOCAL_MACHINE, false, KEY_ALL_ACCESS);
    QString err;
    WindowsRegistry npackd = m.createSubKey("Software\\Npackd\\Npackd", &err,
            KEY_ALL_ACCESS);
    if (err.isEmpty()) {
        npackd.set("path", dir);
    }

    return err;
}

void WPMUtils::setCloseProcessType(DWORD cpt)
{
    WindowsRegistry m(HKEY_LOCAL_MACHINE, false, KEY_ALL_ACCESS);
    QString err;
    WindowsRegistry npackd = m.createSubKey("Software\\Npackd\\Npackd", &err,
            KEY_ALL_ACCESS);
    if (err.isEmpty()) {
        npackd.setDWORD("closeProcessType", cpt);
    }
}

DWORD WPMUtils::getCloseProcessType()
{
    DWORD cpt = CLOSE_WINDOW;

    WindowsRegistry npackd;
    QString err = npackd.open(
            HKEY_LOCAL_MACHINE, "Software\\Npackd\\Npackd", false, KEY_READ);
    if (err.isEmpty()) {
        DWORD v = npackd.getDWORD("closeProcessType", &err);
        if (err.isEmpty())
            cpt = v;
    }

    return cpt;
}

BOOL CALLBACK myEnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    QList<HWND>* p = (QList<HWND>*) lParam;
    p->append(hwnd);
    return TRUE;
}

QList<HWND> WPMUtils::findTopWindows()
{
    QList<HWND> r;
    EnumWindows(myEnumWindowsProc, (LPARAM) &r);
    return r;
}

QList<HWND> WPMUtils::findProcessTopWindows(DWORD processID)
{
    QList<HWND> r;

    QList<HWND> tws = findTopWindows();

    for (int i = 0; i < tws.size(); i++) {
        DWORD pid;
        if (GetWindowThreadProcessId(tws[i], &pid) && pid == processID) {
            r.append(tws.at(i));
        }
    }

    return r;
}

bool WPMUtils::isProcessRunning(HANDLE process)
{
    bool r = false;
    DWORD ec;
    if (GetExitCodeProcess(process, &ec)) {
        // qDebug() << "exit code" << ec << STILL_ACTIVE;
        if (ec == STILL_ACTIVE) {
            r = true;
        }
    }
    return r;
}

QString WPMUtils::disconnectFrom(LMSTR netname)
{
    QString err;

    CONNECTION_INFO_1* ci;
    DWORD entriesRead, totalEntries, resumeHandle;
    resumeHandle = 0;
    if (NetConnectionEnum(0, netname, 1,
            (LPBYTE*) &ci, MAX_PREFERRED_LENGTH,
            &entriesRead, &totalEntries, &resumeHandle) == NERR_Success) {
        for (int i = 0; i < (int) entriesRead; i++) {
            NetSessionDel(0, ci[i].coni1_netname, ci[i].coni1_username);
        }
    } else {
        formatMessage(GetLastError(), &err);
    }
    NetApiBufferFree(ci);

    return err;
}

void WPMUtils::disconnectShareUsersFrom(const QString &dir)
{
    SHARE_INFO_502* buf;
    DWORD entriesRead, totalEntries, resumeHandle;
    resumeHandle = 0;
    if (NetShareEnum(0, 502, (LPBYTE*) &buf, MAX_PREFERRED_LENGTH,
            &entriesRead, &totalEntries, &resumeHandle) == NERR_Success) {
        QString dirNormalized = normalizePath(dir);
#ifndef STYPE_MASK
        const DWORD STYPE_MASK = 0xF0000000;
#endif
        // qDebug() << entriesRead;
        for (int i = 0; i < (int) entriesRead; i++) {
            // qDebug() << "share " << buf[i].shi502_type;
            if ((buf[i].shi502_type & STYPE_MASK) == STYPE_DISKTREE) {
                QString path;
                path.setUtf16((const ushort*) buf[i].shi502_path,
                        wcslen(buf[i].shi502_path));
                path = normalizePath(path);
                //qDebug() << "share found" << path;
                if (isUnderOrEquals(path, dirNormalized)) {
                    //qDebug() << "share found" << path;
                    QString netName;
                    netName.setUtf16((const ushort*) buf[i].shi502_netname,
                            wcslen(buf[i].shi502_netname));
                    // NetShareDel(0, buf[i].shi502_netname, 0);
                    // buf[i].shi502_max_uses = 0;
                    //NetShareSetInfo(0, buf[i].shi502_netname, 502,
                    //        (LPBYTE) &(buf[i]), 0);
                    disconnectFrom(buf[i].shi502_netname);
                }
            }
        }
    }
    NetApiBufferFree(buf);
}

void WPMUtils::closeProcessesThatUseDirectory(const QString &dir,
        DWORD cpt, int f)
{
    if (cpt == 0)
        return;

    //QString f = dir + "\\abc.txt";
    //test((PCWSTR) f.utf16());

    QList<HANDLE> ps0;
    if (f & RUNNING_EXE_IN_DIR) {
        ps0 = WPMUtils::getProcessHandlesLockingDirectory(dir);
    }

    //qDebug() << "getProcessHandlesLockingDirectory";

    QList<HANDLE> ps;
    if (f & USED_FILE_IN_DIR) {
        ps = WPMUtils::getProcessHandlesLockingDirectory2(dir);
    }

    ps.append(ps0);

    //qDebug() << "getProcessHandlesLockingDirectory2";

    DWORD me = GetCurrentProcessId();

    if (cpt & CLOSE_WINDOW) {
        for (int i = 0; i < ps.size(); i++) {
            HANDLE p = ps.at(i);
            if (GetProcessId(p) != me) {
                QList<HWND> ws = findProcessTopWindows(GetProcessId(p));
                if (ws.size() > 0) {
                    closeProcessWindows(p, ws);
                }
            }
        }
    }

    //qDebug() << "Windows closed";

    if (cpt & KILL_PROCESS) {
        for (int i = 0; i < ps.size(); i++) {
            HANDLE hProc = ps.at(i);
            DWORD processId = GetProcessId(hProc);

            if (processId != 0 && processId != me &&
                    WPMUtils::isProcessRunning(hProc)) {
                // TerminateProcess is asynchronous
                if (TerminateProcess(hProc, 1000))
                    WaitForSingleObject(hProc, 30000);
            }
        }
    }

    // qDebug() << "Processes killed";

    for (int i = 0; i < ps.size(); i++) {
        CloseHandle(ps[i]);
    }
}

QString WPMUtils::findFirstExeLockingDirectory(const QString &dir)
{
    QString r;

    QList<HANDLE> ps = WPMUtils::getProcessHandlesLockingDirectory(dir);

    for (int i = 0; i < ps.size(); i++) {
        r = WPMUtils::getProcessFile(ps.at(i));
        if (!r.isEmpty())
            break;
    }

    for (int i = 0; i < ps.size(); i++) {
        CloseHandle(ps[i]);
    }

    return r;
}

QString WPMUtils::getClassName(HWND w)
{
    WCHAR cn[257];
    int n = GetClassName(w, cn, 257);
    QString r;
    r.setUtf16(reinterpret_cast<const ushort*>(cn), n);
    return r;
}

QString WPMUtils::getHostName()
{
    char hostname[1024];
    hostname[1023] = '\0';
    if (gethostname(hostname, 1023) == 0) {
        return QString::fromLatin1(hostname);
    } else {
        return "localhost";
    }
}

#if defined(max) && defined(_MSC_VER)
#undef max
#endif

void WPMUtils::closeProcessWindows(HANDLE process,
        const QList<HWND>& processWindows)
{
    // start flashing
    for (int i = 0; i < processWindows.size(); i++) {
        HWND w = processWindows.at(i);
        if (w != 0 && IsWindow(w) &&
                GetAncestor(w, GA_PARENT) == GetDesktopWindow() &&
                IsWindowVisible(w) &&
                getClassName(w) != "Shell_TrayWnd") {
            FLASHWINFO fwi = {};
            fwi.cbSize = sizeof(fwi);
            fwi.hwnd = w;
            fwi.dwFlags = FLASHW_ALL | FLASHW_TIMER;
            fwi.uCount = std::numeric_limits<UINT>::max();
            // qDebug() << "flash!";
            FlashWindowEx(&fwi);
        }
    }

    QList<HWND> ws = processWindows;
    int old = ws.size();
    while (true) {
        int c = 0;
        for (int i = 0; i < ws.size(); i++) {
            HWND w = ws.at(i);
            if (w) {
                if (!IsWindow(w) ||
                        GetAncestor(w, GA_PARENT) != GetDesktopWindow() ||
                        !IsWindowVisible(w) ||
                        getClassName(w) == "Shell_TrayWnd") {
                    ws[i] = 0;
                } else {
                    c++;
                    if ((GetWindowLong(w, GWL_STYLE) & WS_DISABLED) == 0) {
                        //qDebug() << "WM_CLOSE to " <<
                        //        GetProcessId(process) << getClassName(w);
                        PostMessage(w, WM_CLOSE, 0, 0);
                    }
                }
            }
        }

        if (old == c)
            break;

        old = c;
    }

    if (WPMUtils::isProcessRunning(process)) {
        WaitForSingleObject(process, 30000);
    }

    // stop flashing
    for (int i = 0; i < processWindows.size(); i++) {
        HWND w = processWindows.at(i);
        if (w != 0 && IsWindow(w) &&
                GetAncestor(w, GA_PARENT) == GetDesktopWindow() &&
                IsWindowVisible(w) &&
                getClassName(w) != "Shell_TrayWnd") {
            FLASHWINFO fwi = {};
            fwi.cbSize = sizeof(fwi);
            fwi.hwnd = w;
            fwi.dwFlags = FLASHW_STOP;
            fwi.uCount = std::numeric_limits<UINT>::max();
            // qDebug() << "flash!";
            FlashWindowEx(&fwi);
        }
    }
}

QString WPMUtils::getProcessFile(HANDLE hProcess)
{
    QString res;
    res.resize(MAX_PATH + 1);
    DWORD r = GetModuleFileNameEx(hProcess, 0, (LPWSTR) res.data(),
            res.length());
    if (r != 0) {
        res.resize(r);
    }

    return res;
}

// see also http://msdn.microsoft.com/en-us/library/ms683217(v=VS.85).aspx
QVector<DWORD> WPMUtils::getProcessIDs()
{
    QVector<DWORD> r;

    r.resize(100);
    DWORD cb = r.size() * sizeof(DWORD);
    DWORD cbneeded;
    BOOL ok;
    ok = EnumProcesses(r.data(), cb, &cbneeded);
    if (!ok && cb < cbneeded) {
        r.resize(cbneeded / sizeof(DWORD) + 1);
        cb = r.size() * sizeof(DWORD);
        ok = EnumProcesses(r.data(), cb, &cbneeded);
    }
    if (ok) {
        r.resize(cbneeded / sizeof(DWORD));
    }

    return r;
}

QList<HANDLE> WPMUtils::getProcessHandlesLockingDirectory(const QString& dir)
{
    QList<HANDLE> r;

    OSVERSIONINFO osvi;
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);

    // >= Windows Vista
    if (osvi.dwMajorVersion >= 6) {
        BOOL WINAPI (*lpfQueryFullProcessImageName)(
                HANDLE, DWORD, LPTSTR, PDWORD);

        HINSTANCE hInstLib = LoadLibraryA("KERNEL32.DLL");
        lpfQueryFullProcessImageName =
                (BOOL (WINAPI*) (HANDLE, DWORD, LPTSTR, PDWORD))
                GetProcAddress(hInstLib, "QueryFullProcessImageNameW");

        DWORD aiPID[1000], iCb = 1000;
        DWORD iCbneeded;
        if (!EnumProcesses(aiPID, iCb, &iCbneeded)) {
            FreeLibrary(hInstLib);
            return r;
        }

        // How many processes are there?
        int iNumProc = iCbneeded / sizeof(DWORD);

        // Get and match the name of each process
        for (int i = 0; i < iNumProc; i++) {
            // First, get a handle to the process
            HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, aiPID[i]);

            // Now, get the process name
            if (hProc) {
                HMODULE hMod;
                if (EnumProcessModules(hProc, &hMod, sizeof(hMod), &iCbneeded)) {
                    if (iCbneeded != 0) {
                        HMODULE* modules = new HMODULE[iCbneeded / sizeof(HMODULE)];
                        if (EnumProcessModules(hProc, modules, iCbneeded,
                                &iCbneeded)) {
                            DWORD len = MAX_PATH;
                            WCHAR szName[MAX_PATH];
                            if (lpfQueryFullProcessImageName(hProc, 0, szName,
                                    &len)) {
                                QString s;
                                s.setUtf16((ushort*) szName, len);
                                if (WPMUtils::pathEquals(s, dir) ||
                                        WPMUtils::isUnder(s, dir)) {
                                    r.append(hProc);
                                    hProc = 0;
                                }
                            }
                        }
                        delete[] modules;
                    }
                }
                if (hProc)
                    CloseHandle(hProc);
            }
        }
        FreeLibrary(hInstLib);
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
QMap<QString, QString> mapDevices2Drives() {
    QMap<QString, QString> devices2drives;

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
                logicalDrive.setUtf16((ushort*) drv, wcslen(drv));

                if (logicalDrive.length() >= 2) {
                    logicalDrive.chop(logicalDrive.length() - 2);

                    WCHAR* devices = new WCHAR[2000];

                    // may return multiple strings!
                    // returns very weird strings for network shares
                    if (QueryDosDevice((LPCWSTR) logicalDrive.utf16(), devices,
                            2000)) {
                        WCHAR* device = devices;
                        while (*device) {
                            QString logicalDevice;
                            logicalDevice.setUtf16((ushort*) device,
                                    wcslen(device));
                            devices2drives.insert(
                                    logicalDevice + "\\",
                                    logicalDrive + "\\");

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

class MyThread : public QThread {
public:
    HANDLE h;
    bool ok;
    QString name;

    MyThread(HANDLE h) {
        this->h = h;
        ok = false;
    }

    void run() {
        HMODULE module = GetModuleHandleA("ntdll.dll");
        _NtQueryObject NtQueryObject;
        NtQueryObject = (_NtQueryObject) GetProcAddress(module, "NtQueryObject");

        PVOID objectNameInfo = 0;
        ULONG returnLength = 0;

        objectNameInfo = malloc(0x1000);
        ok = NT_SUCCESS(NtQueryObject(h, ObjectNameInformation,
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

            /* Print the information! */
            if (objectName.Length) {
                name.setUtf16((const ushort*) objectName.Buffer,
                        objectName.Length / 2);
            } else {
                ok = false;
            }
        }

        free(objectNameInfo);
    }
};


QList<HANDLE> WPMUtils::getProcessHandlesLockingDirectory2(const QString &dir) {
    QMap<QString, QString> devices2drives = mapDevices2Drives();

    QList<HANDLE> result;

    return result;

    HMODULE module = GetModuleHandleA("ntdll.dll");
    if (module == 0) {
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
            NULL)) == STATUS_INFO_LENGTH_MISMATCH) {
        handleInfo = (PSYSTEM_HANDLE_INFORMATION) realloc(handleInfo,
                handleInfoSize *= 2);
    }

    // NtQuerySystemInformation stopped giving us STATUS_INFO_LENGTH_MISMATCH.
    if (!NT_SUCCESS(status)) {
        handleInfo->HandleCount = 0;
    }

    QSet<ULONG> usedProcessIds;
    for (i = 0; i < handleInfo->HandleCount; i++) {
        bool ok = true;

        SYSTEM_HANDLE handle = handleInfo->Handles[i];
        HANDLE dupHandle = INVALID_HANDLE_VALUE;
        POBJECT_TYPE_INFORMATION objectTypeInfo = 0;

        HANDLE processHandle = INVALID_HANDLE_VALUE;

        if (usedProcessIds.contains(handle.ProcessId)) {
            ok = false;
        }

        if (ok) {
            if (!(processHandle = OpenProcess(PROCESS_DUP_HANDLE |
                    PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                    handle.ProcessId))) {
                // Could not open PID %d! (Don't try to open a system process.)
                ok = false;
                processHandle = INVALID_HANDLE_VALUE;

                // qDebug() << "OpenProcess ok";
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

                // qDebug() << "NtDuplicateObject ok";
            }
        }

        QString type;

        if (ok) {
            /* Query the object type. */
            objectTypeInfo = (POBJECT_TYPE_INFORMATION)malloc(0x1000);
            if (!NT_SUCCESS(NtQueryObject(
                dupHandle,
                ObjectTypeInformation,
                objectTypeInfo,
                0x1000,
                NULL))) {
                ok = false;
            } else {
                // qDebug() << "NtQueryObject ok";
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
            qDebug() << "NtQueryObject start" << handle.Flags
                    << handle.GrantedAccess << handle.ProcessId
                    << handle.ObjectTypeNumber << handle.Handle;
                    */

            // retrieving the name of a handle may hang for pipes
            MyThread t(dupHandle);
            t.start();
            if (t.wait(1000) ) {
                // Finished
                ok = t.ok;
                name = t.name;
            } else {
                // Not finished
                t.terminate();
                ok = false;
            }

            // qDebug() << "NtQueryObject end";
        }

        if (ok) {
            QMapIterator<QString, QString> i(devices2drives);
            while (i.hasNext()) {
                i.next();
                if (name.startsWith(i.key())) {
                    name = i.value() + name.mid(i.key().length());
                    if (QFileInfo(name).isFile() &&
                            WPMUtils::isUnder(name, dir)) {
                        result.append(processHandle);
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

// see also http://msdn.microsoft.com/en-us/library/ms683217(v=VS.85).aspx
QStringList WPMUtils::getProcessFiles()
{
    QStringList r;

    OSVERSIONINFO osvi;
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);

    // >= Windows Vista
    if (osvi.dwMajorVersion >= 6) {
        BOOL WINAPI (*lpfQueryFullProcessImageName)(
                HANDLE, DWORD, LPTSTR, PDWORD);

        HINSTANCE hInstLib = LoadLibraryA("KERNEL32.DLL");
        lpfQueryFullProcessImageName =
                (BOOL (WINAPI*) (HANDLE, DWORD, LPTSTR, PDWORD))
                GetProcAddress(hInstLib, "QueryFullProcessImageNameW");

        DWORD aiPID[1000], iCb = 1000;
        DWORD iCbneeded;
        if (!EnumProcesses(aiPID, iCb, &iCbneeded)) {
            FreeLibrary(hInstLib);
            return r;
        }

        // How many processes are there?
        int iNumProc = iCbneeded / sizeof(DWORD);

        // Get and match the name of each process
        for (int i = 0; i < iNumProc; i++) {
            // First, get a handle to the process
            HANDLE hProc = OpenProcess(
                    PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                    FALSE, aiPID[i]);

            // Now, get the process name
            if (hProc) {
                HMODULE hMod;
                if (EnumProcessModules(hProc, &hMod, sizeof(hMod), &iCbneeded)) {
                    if (iCbneeded != 0) {
                        HMODULE* modules = new HMODULE[iCbneeded / sizeof(HMODULE)];
                        if (EnumProcessModules(hProc, modules, iCbneeded,
                                &iCbneeded)) {
                            DWORD len = MAX_PATH;
                            WCHAR szName[MAX_PATH];
                            if (lpfQueryFullProcessImageName(hProc, 0, szName,
                                    &len)) {
                                QString s;
                                s.setUtf16((ushort*) szName, len);
                                r.append(s);
                            }
                        }
                        delete[] modules;
                    }
                }
                CloseHandle(hProc);
            }
        }
        FreeLibrary(hInstLib);
    }
    return r;
}

QString WPMUtils::getShellDir(int type)
{
    /* since Vista
        HRESULT WINAPI (*lpfSHGetKnownFolderPath) (
          _In_      REFKNOWNFOLDERID rfid,
          _In_      DWORD dwFlags,
          _In_opt_  HANDLE hToken,
          _Out_     PWSTR *ppszPath
        );

        HINSTANCE hInstLib = LoadLibraryA("SHELL32.DLL");
        if (hInstLib) {
            lpfSHGetKnownFolderPath =
                    (HRESULT (WINAPI*) (
                      _In_      REFKNOWNFOLDERID rfid,
                      _In_      DWORD dwFlags,
                      _In_opt_  HANDLE hToken,
                      _Out_     PWSTR *ppszPath
                    ))
                    GetProcAddress(hInstLib, "SHGetKnownFolderPath");

            if (lpfSHGetKnownFolderPath) {
                outputTextConsole("not null\r\n");
                PWSTR s;
                const DWORD KF_FLAG_DONT_VERIFY = 0x00004000;
                if (SUCCEEDED(lpfSHGetKnownFolderPath(FOLDERID_ProgramFilesX64,
                        KF_FLAG_DONT_VERIFY,
                        0, &s))) {
                    outputTextConsole("S_OK\r\n");
                    ret = QString::fromUtf16(reinterpret_cast<ushort*>(s));
                    CoTaskMemFree(s);
                }
            }

            FreeLibrary(hInstLib);
        }
     */
    WCHAR dir[MAX_PATH];
    SHGetFolderPath(0, type, NULL, 0, dir);
    return QString::fromUtf16(reinterpret_cast<ushort*>(dir));
}

QString WPMUtils::validateFullPackageName(const QString& n)
{
    if (n.length() == 0) {
        return QObject::tr("Empty package name");
    } else {
        int pos = n.indexOf("..");
        if (pos >= 0)
            return QString(QObject::tr("Empty segment at position %1 in %2")).
                    arg(pos + 1).arg(n);

        pos = n.indexOf("--");
        if (pos >= 0)
            return QString(QObject::tr("-- at position %1 in %2")).
                    arg(pos + 1).arg(n);

        QStringList parts = n.split('.', QString::SkipEmptyParts);
        for (int j = 0; j < parts.count(); j++) {
            QString part = parts.at(j);

            int pos = part.indexOf("--");
            if (pos >= 0)
                return QString(QObject::tr("-- at position %1 in %2")).
                        arg(pos + 1).arg(part);

            if (!part.isEmpty()) {
                QChar c = part.at(0);
                if (!((c >= '0' && c <= '9') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c == '_') ||
                        (c >= 'a' && c <= 'z') ||
                        c.isLetter()))
                    return QString(QObject::tr("Wrong character at position 1 in %1")).
                            arg(part);
            }

            for (int i = 1; i < part.length() - 1; i++) {
                QChar c = part.at(i);
                if (!((c >= '0' && c <= '9') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c == '_') ||
                        (c == '-') ||
                        (c >= 'a' && c <= 'z') ||
                        c.isLetter()))
                    return QString(QObject::tr("Wrong character at position %1 in %2")).
                            arg(i + 1).arg(part);
            }

            if (!part.isEmpty()) {
                QChar c = part.at(part.length() - 1);
                if (!((c >= '0' && c <= '9') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c == '_') ||
                        (c >= 'a' && c <= 'z') ||
                        c.isLetter()))
                    return QString(QObject::tr("Wrong character at position %1 in %2")).
                            arg(part.length()).arg(part);
            }
        }
    }

    return "";
}

QString WPMUtils::makeValidFullPackageName(const QString& name)
{
    QString r(name);
    QStringList parts = r.split('.', QString::SkipEmptyParts);
    for (int j = 0; j < parts.count(); ) {
        QString part = parts.at(j);

        if (!part.isEmpty()) {
            QChar c = part.at(0);
            if (!((c >= '0' && c <= '9') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c == '_') ||
                    (c >= 'a' && c <= 'z') ||
                    c.isLetter()))
                part[0] = '_';
        }

        for (int i = 1; i < part.length() - 1; i++) {
            QChar c = part.at(i);
            if (!((c >= '0' && c <= '9') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c == '_') ||
                    (c == '-') ||
                    (c >= 'a' && c <= 'z') ||
                    c.isLetter()))
                part[i] = '_';
        }

        if (!part.isEmpty()) {
            QChar c = part.at(part.length() - 1);
            if (!((c >= '0' && c <= '9') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c == '_') ||
                    (c >= 'a' && c <= 'z') ||
                    c.isLetter()))
                part[part.length() - 1] = '_';
        }

        if (part.isEmpty())
            parts.removeAt(j);
        else {
            parts.replace(j, part);
            j++;
        }
    }
    r = parts.join(".");
    if (r.isEmpty())
        r = '_';
    return r;
}

QString WPMUtils::validateSHA1(const QString& sha1)
{
    if (sha1.length() != 40) {
        return QString(QObject::tr("Wrong length: %1")).arg(sha1);
    } else {
        for (int i = 0; i < sha1.length(); i++) {
            QChar c = sha1.at(i);
            if (!((c >= '0' && c <= '9') ||
                (c >= 'a' && c <= 'f') ||
                (c >= 'A' && c <= 'F'))) {
                return QString(QObject::tr("Wrong character at position %1 in %2")).
                        arg(i + 1).arg(sha1);
            }
        }
    }

    return "";
}

QString WPMUtils::validateSHA256(const QString &sha256)
{
    if (sha256.length() != 64) {
        return QString(QObject::tr("Wrong length: %1")).arg(sha256);
    } else {
        for (int i = 0; i < sha256.length(); i++) {
            QChar c = sha256.at(i);
            if (!((c >= '0' && c <= '9') ||
                (c >= 'a' && c <= 'f') ||
                (c >= 'A' && c <= 'F'))) {
                return QString(QObject::tr("Wrong character at position %1 in %2")).
                        arg(i + 1).arg(sha256);
            }
        }
    }

    return "";
}

QString WPMUtils::setSystemEnvVar(const QString& name, const QString& value,
        bool expandVars)
{
    WindowsRegistry wr;
    QString err = wr.open(HKEY_LOCAL_MACHINE,
            "System\\CurrentControlSet\\Control\\Session Manager\\Environment",
            false);
    if (!err.isEmpty())
        return err;

    if (expandVars)
        err = wr.setExpand(name, value);
    else
        err = wr.set(name, value);

    if (!err.isEmpty())
        return err;

    return "";
}

QString WPMUtils::getFirstLine(const QString& text)
{
    QStringList sl = text.trimmed().split("\n");
    if (sl.count() > 0)
        return sl.at(0).trimmed();
    else
        return "";
}

void WPMUtils::fireEnvChanged()
{
    SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
            (LPARAM) L"Environment",
            SMTO_ABORTIFHUNG, 5000, 0);
}

QString WPMUtils::getSystemEnvVar(const QString& name, QString* err)
{
    err->clear();

    WindowsRegistry wr;
    QString e = wr.open(HKEY_LOCAL_MACHINE,
            "System\\CurrentControlSet\\Control\\Session Manager\\Environment",
            false);
    if (!e.isEmpty()) {
        *err = e;
        return "";
    }

    return wr.get(name, err);
}

Version WPMUtils::getDLLVersion(const QString &path)
{
    Version res(0, 0);

    DWORD dwVerHnd;
    DWORD size;
    size = GetFileVersionInfoSize((LPWSTR) path.utf16(), &dwVerHnd);
    if (size != 0) {
        void* mem = malloc(size);
        if (GetFileVersionInfo((LPWSTR) path.utf16(), 0, size, mem)) {
            VS_FIXEDFILEINFO *pFileInfo;
            unsigned int bufLen;
            if (VerQueryValue(mem, (WCHAR*) L"\\", (LPVOID *) &pFileInfo,
                    &bufLen)) {
                res.setVersion(HIWORD(pFileInfo->dwFileVersionMS),
                        LOWORD(pFileInfo->dwFileVersionMS),
                        HIWORD(pFileInfo->dwFileVersionLS),
                        LOWORD(pFileInfo->dwFileVersionLS));
            }
        }
        free(mem);
    }

    return res;
}

QStringList WPMUtils::findInstalledMSIProductNames()
{
    QStringList result;
    WCHAR buf[39];
    int index = 0;
    while (true) {
        UINT r = MsiEnumProducts(index, buf);
        if (r != ERROR_SUCCESS)
            break;
        QString uuid;
        uuid.setUtf16((ushort*) buf, 38);

        WCHAR value[64];
        DWORD len;

        len = sizeof(value) / sizeof(value[0]);
        r = MsiGetProductInfo(buf, INSTALLPROPERTY_INSTALLEDPRODUCTNAME,
                value, &len);
        QString title;
        if (r == ERROR_SUCCESS) {
            title.setUtf16((ushort*) value, len);
        }

        len = sizeof(value) / sizeof(value[0]);
        r = MsiGetProductInfo(buf, INSTALLPROPERTY_VERSIONSTRING,
                value, &len);
        QString version;
        if (r == ERROR_SUCCESS) {
            version.setUtf16((ushort*) value, len);
        }

        result.append(title + " " + version);
        result.append("    " + uuid);

        QString err;
        QString path = WPMUtils::getMSIProductLocation(uuid, &err);
        if (!err.isEmpty())
            result.append("    err" + err);
        else
            result.append("    " + path);

        index++;
    }
    return result;
}

QString WPMUtils::getMSIProductLocation(const QString& guid, QString* err)
{
    return getMSIProductAttribute(guid, INSTALLPROPERTY_INSTALLLOCATION, err);
}

QString WPMUtils::getMSIProductAttribute(const QString &guid,
        LPCWSTR attribute, QString *err)
{
    WCHAR value[MAX_PATH];
    DWORD len;

    len = sizeof(value) / sizeof(value[0]);
    UINT r = MsiGetProductInfo(
            (WCHAR*) guid.utf16(),
            attribute,
            value, &len);
    QString p;
    if (r == ERROR_SUCCESS) {
        p.setUtf16((ushort*) value, len);
        err->clear();
    } else {
        *err = QObject::tr("Cannot determine MSI product location for GUID %1").
                arg(guid);
    }
    return p;
}

QString WPMUtils::getMSIComponentPath(const QString& product,
        const QString &guid,
        QString *err)
{
    WCHAR value[MAX_PATH];
    DWORD len;

    len = sizeof(value) / sizeof(value[0]);
    UINT r = MsiGetComponentPath(
            (WCHAR*) product.utf16(),
            (WCHAR*) guid.utf16(),
            value, &len);
    QString p;
    if (r == INSTALLSTATE_LOCAL) {
        p.setUtf16((ushort*) value, len);
        err->clear();
    } else {
        *err = QObject::tr("Cannot determine MSI component location for GUID %1").
                arg(guid);
    }
    return p;
}

bool WPMUtils::pathEquals(const QString& patha, const QString& pathb)
{
    QString a = patha;
    QString b = pathb;
    a.replace('/', '\\');
    b.replace('/', '\\');
    return QString::compare(a, b, Qt::CaseInsensitive) == 0;
}

QString WPMUtils::getMSIProductName(const QString& guid, QString* err)
{
    return getMSIProductAttribute(guid, INSTALLPROPERTY_PRODUCTNAME, err);
}

QString WPMUtils::format(const QString& txt, const QMap<QString, QString>& vars)
{
    QString res;

    int from = 0;
    while (true) {
        int p = txt.indexOf("${{", from);
        if (p >= 0) {
            res.append(txt.mid(from, p - from));

            int p2 = txt.indexOf("}}", p + 3);
            if (p2 < 0) {
                res.append(txt.mid(p));
                break;
            }
            QString var = txt.mid(p + 3, p2 - p - 3).trimmed();
            if (var.isEmpty()) {
                res.append(txt.mid(p, p2 + 2 - p));
            } else {
                if (vars.contains(var)) {
                    res.append(vars[var]);
                }
            }
            from = p2 + 2;
        } else {
            res.append(txt.mid(from));
            break;
        }
    }

    return res;
}

QStringList WPMUtils::findInstalledMSIProducts()
{
    QStringList result;
    WCHAR buf[39];
    int index = 0;
    while (true) {
        UINT r = MsiEnumProducts(index, buf);
        if (r != ERROR_SUCCESS)
            break;
        QString v;
        v.setUtf16((ushort*) buf, 38);
        result.append(v.toLower());
        index++;
    }
    return result;
}

QStringList WPMUtils::findInstalledMSIComponents()
{
    QStringList result;
    WCHAR buf[39];
    int index = 0;
    while (true) {
        UINT r = MsiEnumComponents(index, buf);
        if (r != ERROR_SUCCESS)
            break;
        QString v;
        v.setUtf16((ushort*) buf, 38);
        result.append(v.toLower());
        index++;
    }
    return result;
}

QMultiMap<QString, QString> WPMUtils::mapMSIComponentsToProducts(
        const QStringList& components)
{
    QMultiMap<QString, QString> map;
    WCHAR buf[39];
    for (int i = 0; i < components.count(); i++) {
        QString c = components.at(i);
        if (MsiGetProductCode((LPCWSTR) c.utf16(), buf) == ERROR_SUCCESS) {
            QString v;
            v.setUtf16((ushort*) buf, 38);
            map.insert(v.toLower(), c);
        }
    }
    return map;
}

QString WPMUtils::getWindowsDir()
{
    WCHAR dir[MAX_PATH];
    // this sometimes returns wrong value (Terminal Services?): SHGetFolderPath(0, CSIDL_WINDOWS, NULL, 0, dir);
    GetSystemWindowsDirectory(dir, MAX_PATH);
    return QString::fromUtf16(reinterpret_cast<ushort*>(dir));
}

QString WPMUtils::findCmdExe()
{
    QString r = getWindowsDir() + "\\Sysnative\\cmd.exe";
    if (!QFileInfo(r).exists()) {
        r = getWindowsDir() + "\\system32\\cmd.exe";
    }
    return r;
}

QString WPMUtils::getExeFile()
{
    TCHAR path[MAX_PATH];
    GetModuleFileName(0, path, sizeof(path) / sizeof(path[0]));
    QString r;
    r.setUtf16((ushort*) path, wcslen(path));

    return r.replace('/', '\\');
}

QString WPMUtils::getExeDir()
{
    TCHAR path[MAX_PATH];
    GetModuleFileName(0, path, sizeof(path) / sizeof(path[0]));
    QString r;
    r.setUtf16((ushort*) path, wcslen(path));

    QDir d(r);
    d.cdUp();
    return d.absolutePath().replace('/', '\\');
}

QString WPMUtils::regQueryValue(HKEY hk, const QString &var)
{
    QString value_;
    char value[255];
    DWORD valueSize = sizeof(value);
    if (RegQueryValueEx(hk, (WCHAR*) var.utf16(), 0, 0, (BYTE*) value,
            &valueSize) == ERROR_SUCCESS) {
        // the next line is important
        // valueSize is sometimes == 0 and the expression (valueSize /2 - 1)
        // below leads to an AV
        if (valueSize != 0)
            value_.setUtf16((ushort*) value, valueSize / 2 - 1);
    }
    return value_;
}

QString WPMUtils::sha1(const QString& filename)
{
    return WPMUtils::hashSum(filename, QCryptographicHash::Sha1);
}

QString WPMUtils::hashSum(const QString& filename,
        QCryptographicHash::Algorithm alg)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
         return "";

    QCryptographicHash hash(alg);

    const int SIZE = 512 * 1024;
    char* buffer = new char[SIZE];

    bool err = false;
    while (!file.atEnd()) {
        qint64 r = file.read(buffer, SIZE);
        if (r < 0) {
            err = true;
            break;
        }
        hash.addData(buffer, r);
    }
    file.close();

    delete[] buffer;

    if (err)
        return "";
    else
        return hash.result().toHex().toLower();
}

QString WPMUtils::getShellFileOperationErrorMessage(int res)
{
    QString r;
    switch (res) {
        case 0:
            r = "";
            break;
        case 0x71:
            r = QObject::tr("The source and destination files are the same file.");
            break;
        case 0x72:
            r = QObject::tr("Multiple file paths were specified in the source buffer, but only one destination file path.");
            break;
        case 0x73:
            r = QObject::tr("Rename operation was specified but the destination path is a different directory. Use the move operation instead.");
            break;
        case 0x74:
            r = QObject::tr("The source is a root directory, which cannot be moved or renamed.");
            break;
        case 0x75:
            r = QObject::tr("The operation was canceled by the user, or silently canceled if the appropriate flags were supplied to SHFileOperation.");
            break;
        case 0x76:
            r = QObject::tr("The destination is a subtree of the source.");
            break;
        case 0x78:
            r = QObject::tr("Security settings denied access to the source.");
            break;
        case 0x79:
            r = QObject::tr("The source or destination path exceeded or would exceed MAX_PATH.");
            break;
        case 0x7A:
            r = QObject::tr("The operation involved multiple destination paths, which can fail in the case of a move operation.");
            break;
        case 0x7C:
            r = QObject::tr("The path in the source or destination or both was invalid.");
            break;
        case 0x7D:
            r = QObject::tr("The source and destination have the same parent folder.");
            break;
        case 0x7E:
            r = QObject::tr("The destination path is an existing file.");
            break;
        case 0x80:
            r = QObject::tr("The destination path is an existing folder.");
            break;
        case 0x81:
            r = QObject::tr("The name of the file exceeds MAX_PATH.");
            break;
        case 0x82:
            r = QObject::tr("The destination is a read-only CD-ROM, possibly unformatted.");
            break;
        case 0x83:
            r = QObject::tr("The destination is a read-only DVD, possibly unformatted.");
            break;
        case 0x84:
            r = QObject::tr("The destination is a writable CD-ROM, possibly unformatted.");
            break;
        case 0x85:
            r = QObject::tr("The file involved in the operation is too large for the destination media or file system.");
            break;
        case 0x86:
            r = QObject::tr("The source is a read-only CD-ROM, possibly unformatted.");
            break;
        case 0x87:
            r = QObject::tr("The source is a read-only DVD, possibly unformatted.");
            break;
        case 0x88:
            r = QObject::tr("The source is a writable CD-ROM, possibly unformatted.");
            break;
        case 0xB7:
            r = QObject::tr("MAX_PATH was exceeded during the operation.");
            break;
        case 0x402:
            r = QObject::tr("An unknown error occurred. This is typically due to an invalid path in the source or destination. This error does not occur on Windows Vista and later.");
            break;
        case 0x10000:
            r = QObject::tr("An unspecified error occurred on the destination.");
            break;
        case 0x10074:
            r = QObject::tr("Destination is a root directory and cannot be renamed.");
            break;
        default:
            WPMUtils::formatMessage(res, &r);
            break;
    }
    return r;
}

QString WPMUtils::moveToRecycleBin(QString dir)
{
    WPMUtils::reportEvent(QObject::tr(
            "Moving %1 to the recycle bin").
            arg(dir.replace('/', '\\')));

    SHFILEOPSTRUCTW f;
    memset(&f, 0, sizeof(f));
    WCHAR* from = new WCHAR[dir.length() + 2];
    wcscpy(from, (WCHAR*) dir.utf16());
    from[wcslen(from) + 1] = 0;
    f.wFunc = FO_DELETE;
    f.pFrom = from;
    f.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI |
            FOF_SILENT | FOF_NOCONFIRMMKDIR;

    int r = SHFileOperationW(&f);
    delete[] from;

    if (r == 0)
        return "";
    else {
        return QString(QObject::tr("Error deleting %1: %2")).
                arg(dir).arg(
                WPMUtils::getShellFileOperationErrorMessage(r));
    }
}

bool WPMUtils::is64BitWindows()
{
#if defined(__x86_64__) || defined(_WIN64)
    return true;
#else
    // 32-bit programs run on both 32-bit and 64-bit Windows
    // so must sniff
    BOOL WINAPI (* lpfIsWow64Process_) (HANDLE,PBOOL);

    HINSTANCE hInstLib = LoadLibraryA("KERNEL32.DLL");
    lpfIsWow64Process_ =
            (BOOL (WINAPI *) (HANDLE,PBOOL))
            GetProcAddress(hInstLib, "IsWow64Process");
    bool ret;
    if (lpfIsWow64Process_) {
        BOOL f64 = FALSE;
        ret = (*lpfIsWow64Process_)(GetCurrentProcess(), &f64) && f64;
    } else {
        ret = false;
    }
    FreeLibrary(hInstLib);
    return ret;
#endif
}

QString WPMUtils::createLink(LPCWSTR lpszPathObj, LPCWSTR lpszPathLink,
        LPCWSTR lpszDesc,
        LPCWSTR workingDir)
{
    QString r;
    HRESULT hres;
    IShellLink* psl;

    // Get a pointer to the IShellLink interface.
    hres = CoCreateInstance(CLSID_ShellLink, NULL,
            CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID *) &psl);

    if (SUCCEEDED(hres)) {
        IPersistFile* ppf;

        // Set the path to the shortcut target and add the
        // description.
        psl->SetPath(lpszPathObj);
        psl->SetDescription(lpszDesc);
        psl->SetWorkingDirectory(workingDir);

        // Query IShellLink for the IPersistFile interface for saving the
        // shortcut in persistent storage.
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

        if (SUCCEEDED(hres)) {
            // Save the link by calling IPersistFile::Save.
            hres = ppf->Save(lpszPathLink, TRUE);
            ppf->Release();
        }
        psl->Release();
    }

    if (!SUCCEEDED(hres)) {
        formatMessage(hres, &r);
    }

    return r;
}

void WPMUtils::removeDirectory(Job* job, QDir &aDir, bool firstLevel)
{
    if (firstLevel) {
        WPMUtils::reportEvent(QObject::tr(
                "Deleting %1").
                arg(aDir.absolutePath().replace('/', '\\')));
    }

    if (aDir.exists()) {
        QFileInfoList entries = aDir.entryInfoList(
                QDir::NoDotAndDotDot |
                QDir::AllEntries | QDir::System);
        int count = entries.size();
        for (int idx = 0; idx < count; idx++) {
            QFileInfo entryInfo = entries[idx];
            QString path = entryInfo.absoluteFilePath();
            if (entryInfo.isDir()) {
                QDir dd(path);
                Job* sub = job->newSubJob(1 / ((double) count + 1));
                removeDirectory(sub, dd, false);
                if (!sub->getErrorMessage().isEmpty())
                    job->setErrorMessage(sub->getErrorMessage());
                // if (!ok)
                //    qDebug() << "WPMUtils::removeDirectory.3" << *errMsg;
            } else {
                QFile file(path);
                if (!file.remove() && file.exists()) {
                    job->setErrorMessage(QString(QObject::tr("Cannot delete the file: %1")).
                            arg(path));
                    // qDebug() << "WPMUtils::removeDirectory.1" << *errMsg;
                } else {
                    job->setProgress(idx / ((double) count + 1));
                }
            }
            if (!job->getErrorMessage().isEmpty())
                break;
        }

        if (job->getErrorMessage().isEmpty()) {
            if (!aDir.rmdir(aDir.absolutePath()))
                // qDebug() << "WPMUtils::removeDirectory.2";
                job->setErrorMessage(QString(
                        QObject::tr("Cannot delete the directory: %1")).
                        arg(aDir.absolutePath()));
            else
                job->setProgress(1);
        }
    } else {
        job->setProgress(1);
    }

    job->complete();
}

QString WPMUtils::makeValidFilename(const QString &name, QChar rep)
{
    // http://msdn.microsoft.com/en-us/library/aa365247(v=vs.85).aspx
    QString invalid("<>:\"/\\|?* ");

    QString r(name);
    for (int i = 0; i < invalid.length(); i++)
        r.replace(invalid.at(i), rep);
    return r;
}

void WPMUtils::writeln(const QString& txt, bool stdout_)
{
    outputTextConsole(txt + "\r\n", stdout_);
}

void WPMUtils::outputTextConsole(const QString& txt, bool stdout_)
{
    HANDLE hStdout;
    if (stdout_)
        hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    else {
        hStdout = GetStdHandle(STD_ERROR_HANDLE);
        if (hStdout == INVALID_HANDLE_VALUE)
            hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    if (hStdout != INVALID_HANDLE_VALUE) {
        // we do not check GetLastError here as it sometimes returns
        // 2=The system cannot find the file specified.
        // GetFileType returns 0 if an error occures so that the == check below
        // is sufficient
        DWORD ft = GetFileType(hStdout);
        bool consoleOutput = (ft & ~(FILE_TYPE_REMOTE)) ==
                FILE_TYPE_CHAR;

        DWORD consoleMode;
        if (consoleOutput) {
            if (!GetConsoleMode(hStdout, &consoleMode))
                consoleOutput = false;
        }

        DWORD written;
        if (consoleOutput) {
            // WriteConsole automatically converts UTF-16 to the code page used
            // by the console
            WriteConsoleW(hStdout, txt.utf16(), txt.length(), &written, 0);
        } else {
            // we always write UTF-8 to the output file
            QByteArray arr = txt.toUtf8();
            WriteFile(hStdout, arr.constData(), arr.length(), &written, NULL);
        }
    }
}

bool WPMUtils::isOutputRedirected(bool stdout_)
{
    bool r = false;
    HANDLE hStdout = GetStdHandle(stdout_ ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
    if (hStdout != INVALID_HANDLE_VALUE) {
        DWORD consoleMode;
        r = !((GetFileType(hStdout) & ~(FILE_TYPE_REMOTE)) ==
                FILE_TYPE_CHAR &&
                (GetLastError() == 0) &&
                GetConsoleMode(hStdout, &consoleMode) &&
                (GetLastError() == 0));
    }
    return r;
}

QTime WPMUtils::durationToTime(time_t diff)
{
    int sec = diff % 60;
    diff /= 60;
    int min = diff % 60;
    diff /= 60;
    int h = diff;

    return QTime(h, min, sec);
}

bool WPMUtils::confirmConsole(const QString& msg)
{
   outputTextConsole(msg);
   return inputTextConsole().trimmed().toLower() == "y";
}

QString WPMUtils::inputTextConsole()
{
    // http://msdn.microsoft.com/en-us/library/ms686974(v=VS.85).aspx

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE)
        return "";

    WCHAR buffer[255];
    DWORD read;
    if (!ReadConsoleW(hStdin, &buffer, sizeof(buffer) / sizeof(buffer[0]), &read, 0))
        return "";

    QString r;
    r.setUtf16((ushort*) buffer, read);

    while (r.endsWith('\n') || r.endsWith('\r'))
        r.chop(1);

    return r;
}

QString WPMUtils::inputPasswordConsole()
{
    // http://www.cplusplus.com/forum/general/3570/

    QString result;

    // Set the console mode to no-echo, not-line-buffered input
    DWORD mode;
    HANDLE ih = GetStdHandle(STD_INPUT_HANDLE);
    if (ih == INVALID_HANDLE_VALUE)
        return "";

    HANDLE oh = GetStdHandle(STD_OUTPUT_HANDLE);
    if (oh == INVALID_HANDLE_VALUE)
        return "";

    if (!GetConsoleMode(ih, &mode))
        return result;

    SetConsoleMode(ih, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));

    // Get the password string
    DWORD count;
    WCHAR c;
    while (true) {
        if (!ReadConsoleW(ih, &c, 1, &count, NULL) ||
                (c == '\r') || (c == '\n'))
            break;

        if (c == '\b') {
            if (result.length()) {
                WriteConsoleW(oh, L"\b \b", 3, &count, NULL);
                result.chop(1);
            }
        } else {
            WriteConsoleW(oh, L"*", 1, &count, NULL);
            result.push_back(c);
        }
    }

    // Restore the console mode
    SetConsoleMode(ih, mode);

    return result;
}

QString WPMUtils::findNonExistingFile(const QString& start,
        const QString ext)
{
    QString result = start + ext;
    if (!QFileInfo(result).exists())
        return result;

    for (int i = 2; i < 100; i++) {
        result = start + "_" + QString::number(i) + ext;
        if (!QFileInfo(result).exists())
            return result;
    }

    return start + ext;
}

void WPMUtils::deleteShortcuts(const QString& dir, QDir& d)
{
    // Get a pointer to the IShellLink interface.
    IShellLink* psl;
    HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL,
            CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID *) &psl);

    if (SUCCEEDED(hres)) {
        QDir instDir(dir);
        QString instPath = instDir.absolutePath();
        QFileInfoList entries = d.entryInfoList(
                QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
        int count = entries.size();
        for (int idx = 0; idx < count; idx++) {
            QFileInfo entryInfo = entries[idx];
            QString path = entryInfo.absoluteFilePath();
            // qDebug() << "PackageVersion::deleteShortcuts " << path;
            if (entryInfo.isDir()) {
                QDir dd(path);
                deleteShortcuts(dir, dd);
            } else {
                if (path.toLower().endsWith(".lnk")) {
                    // qDebug() << "deleteShortcuts " << path;
                    IPersistFile* ppf;

                    hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

                    if (SUCCEEDED(hres)) {
                        //qDebug() << "Loading " << path;

                        hres = ppf->Load((WCHAR*) path.utf16(), STGM_READ);

                        if (SUCCEEDED(hres)) {
                            WCHAR info[MAX_PATH + 1];
                            hres = psl->GetPath(info, MAX_PATH,
                                    (WIN32_FIND_DATAW*) 0, 0);
                            if (SUCCEEDED(hres)) {
                                QString targetPath;
                                targetPath.setUtf16((ushort*) info, wcslen(info));
                                // qDebug() << "deleteShortcuts " << targetPath << " " <<
                                //        instPath;
                                if (WPMUtils::isUnder(targetPath,
                                                      instPath)) {
                                    QFile::remove(path);
                                    // qDebug() << "deleteShortcuts removed";
                                }
                            }
                        }
                        ppf->Release();
                    }
                }
            }
        }
        psl->Release();
    } else {
        qDebug() << "failed";
    }
}

int WPMUtils::getProgramCloseType(const CommandLine& cl, QString* err)
{
    int r = WPMUtils::CLOSE_WINDOW;
    QString v = cl.get("end-process");
    if (!v.isNull()) {
        r = 0;
        if (v.length() == 0) {
            *err = QObject::tr("Empty list of program close types");
        } else {
            for (int i = 0; i < v.length(); i++) {
                QChar t = v.at(i);
                if (t == 'c')
                    r |= WPMUtils::CLOSE_WINDOW;
                else if (t == 's')
                    r |= WPMUtils::DISABLE_SHARES;
                else if (t == 'k')
                    r |= WPMUtils::KILL_PROCESS;
                else
                    *err = QObject::tr(
                            "Invalid program close type: %1").arg(t);
            }
        }
    }
    return r;
}

QString WPMUtils::fileCheckSum(Job* job,
        QFile* file, QCryptographicHash::Algorithm alg)
{
    QString initialTitle = job->getTitle();

    // download/compute SHA1 loop
    QCryptographicHash hash(alg);
    const int bufferSize = 512 * 1024;
    char* buffer = new char[bufferSize];

    QString sha1;

    qint64 alreadyRead = 0;
    qint64 bufferLength;
    do {
        bufferLength = file->read(buffer, bufferSize);

        if (bufferLength == 0)
            break;

        if (bufferLength < 0) {
            job->setErrorMessage(file->errorString());
            break;
        }

        // update SHA1 if necessary
        hash.addData((char*) buffer, bufferLength);

        alreadyRead += bufferLength;
        job->setProgress(0.5);
        job->setTitle(initialTitle + " / " + QObject::tr("%L0 bytes").
                arg(alreadyRead));
    } while (bufferLength != 0 && !job->isCancelled());

    if (job->shouldProceed()) {
        sha1 = hash.result().toHex().toLower();
        job->setProgress(1);
    }

    delete[] buffer;

    job->complete();

    return sha1;
}

void WPMUtils::unzip(Job* job, const QString zipfile, const QString outputdir)
{
    QString initialTitle = job->getTitle();

    QuaZip zip(zipfile);
    if (!zip.open(QuaZip::mdUnzip)) {
        job->setErrorMessage(QString(QObject::tr("Cannot open the ZIP file %1: %2")).
                       arg(zipfile).arg(zip.getZipError()));
    } else {
        job->setProgress(0.01);
    }

    if (job->shouldProceed()) {
        QString odir = outputdir;
        if (!odir.endsWith("\\") && !odir.endsWith("/"))
            odir.append("\\");

        job->setTitle(initialTitle + " / " + QObject::tr("Extracting"));
        QuaZipFile file(&zip);
        int n = zip.getEntriesCount();
        int blockSize = 1024 * 1024;
        char* block = new char[blockSize];
        int i = 0;
        for (bool more = zip.goToFirstFile(); more; more = zip.goToNextFile()) {
            QString name = zip.getCurrentFileName();
            if (!file.open(QIODevice::ReadOnly)) {
                job->setErrorMessage(QString(
                        QObject::tr("Error unzipping the file %1: Error %2 in %3")).
                        arg(zipfile).arg(file.getZipError()).
                        arg(name));
                break;
            }
            name.prepend(odir);
            QFile meminfo(name);
            QFileInfo infofile(meminfo);
            QDir dira(infofile.absolutePath());
            if (dira.mkpath(infofile.absolutePath())) {
                if (meminfo.open(QIODevice::ReadWrite)) {
                    while (true) {
                        qint64 read = file.read(block, blockSize);
                        if (read <= 0)
                            break;
                        meminfo.write(block, read);
                    }
                    meminfo.close();
                }
            } else {
                job->setErrorMessage(QString(QObject::tr("Cannot create directory %1")).arg(
                        infofile.absolutePath()));
                file.close();
            }
            file.close(); // do not forget to close!
            i++;
            job->setProgress(0.01 + 0.99 * i / n);
            if (i % 100 == 0)
                job->setTitle(initialTitle + " / " +
                        QString(QObject::tr("%L1 files")).arg(i));

            if (!job->shouldProceed())
                break;
        }
        zip.close();

        delete[] block;
    }

    job->complete();
}

void WPMUtils::executeBatchFile(Job* job, const QString& where,
        const QString& path,
        const QString& outputFile, const QStringList& env,
        bool printScriptOutput)
{
    QDir d(where);

    QString exe = WPMUtils::findCmdExe();
    QString file = d.absolutePath() + "\\" + path;
    file.replace('/', '\\');

    QString outputPath = outputFile;
    if (!outputPath.isEmpty())
        outputPath = d.absolutePath() + "\\" + outputPath;

    executeFile(job, d.absolutePath().replace('/', '\\'), exe,
            "/U /E:ON /V:OFF /C \"\"" + file + "\"\"",
            outputPath, env, true, printScriptOutput);
}

void WPMUtils::reportEvent(const QString &msg, WORD wType)
{
    if (hEventLog == 0) {
        hEventLog = RegisterEventSource(NULL, L"Npackd");
    }

    if (hEventLog) {
        // qDebug() << "ReportEvent";
        LPCWSTR strings[1];
        strings[0] = (LPCWSTR) msg.utf16();

        ReportEvent(hEventLog, wType, 0, 1, NULL,
                1, 0, strings,
                NULL);
    }
}

void WPMUtils::executeFile(Job* job, const QString& where,
        const QString& path, const QString& nativeArguments,
        const QString& outputFile, const QStringList& env,
        bool writeUTF16LEBOM, bool printScriptOutput)
{
    if (!outputFile.isEmpty()) {
        QFile f(outputFile);
        if (job->shouldProceed()) {
            if (!f.open(QIODevice::WriteOnly | QIODevice::Append))
                job->setErrorMessage(f.errorString());
        }

        if (job->shouldProceed() && writeUTF16LEBOM) {
            if (f.pos() == 0) {
                if (f.write("\xff\xfe") == -1)
                    job->setErrorMessage(f.errorString());
            }
        }

        executeFile(job, where, path, nativeArguments,
                &f, env, printScriptOutput);

        // ignore possible errors here
        f.close();
    } else {
        executeFile(job, where, path, nativeArguments,
                0, env, printScriptOutput);
    }
}

QMap<QString, QString> WPMUtils::parseEnv(LPWCH env2)
{
    QMap<QString, QString> env_;
    LPWCH e = env2;
    while (true) {
        int len = wcslen(e);
        if (!len)
            break;

        QString s;
        s.setUtf16((const ushort*) e, len);
        int p = s.indexOf('=');

        QString name, value;
        if (p >= 0) {
            name = s.left(p);
            value = s.mid(p + 1);
        } else {
            name = s;
        }
        env_.insert(name, value);

        e += len + 1;

        // qDebug() << name << value;
    }

    return env_;
}

QByteArray WPMUtils::serializeEnv(const QMap<QString, QString>& env)
{
    QByteArray ba;
    QMapIterator<QString, QString> i(env);
    while (i.hasNext()) {
        i.next();
        QString name = i.key();
        QString value = i.value();
        ba.append((char*) name.utf16(), name.length() * 2);
        ba.append('=');
        ba.append('\0');
        ba.append((char*) value.utf16(), (value.length() + 1) * 2);
    }
    ba.append('\0');
    ba.append('\0');

    return ba;
}

QString WPMUtils::checkURL(const QUrl &base, QString *url, bool allowEmpty)
{
    *url = url->trimmed();

    QString r;

    if (url->isEmpty()) {
        if (!allowEmpty)
            r = QObject::tr("The URL cannot be empty");
    } else {
        QUrl u(*url);
        if (!u.isValid()) {
            r = QObject::tr("The URL is invalid");
        } else {
            if (u.isRelative()) {
                if (base.isEmpty())
                    r = QObject::tr("The URL cannot be relative");
                else {
                    if (base.scheme() == "file")
                        u = QUrl::fromLocalFile(*url);

                    //qDebug() << base << u;

                    *url = base.resolved(u).toString(QUrl::FullyEncoded);

                    // qDebug() << *url;
                }
            } else if (u.scheme() != "http" && u.scheme() != "https" &&
                    u.scheme() != "file") {
                r = QObject::tr("Unsupported URL scheme");
            }
        }
    }

    return r;
}

void WPMUtils::executeFile(Job* job, const QString& where,
        const QString& path, const QString& nativeArguments,
        QIODevice* outputFile, const QStringList& env,
        bool printScriptOutput)

{
    if (debug)
        WPMUtils::writeln(where + " " + path + " " + nativeArguments);

    QString initialTitle = job->getTitle();

    time_t start = time(NULL);

    LPWCH env2 = GetEnvironmentStrings();
    QMap<QString, QString> env_ = parseEnv(env2);
    FreeEnvironmentStrings(env2);

    for (int i = 0; i + 1 < env.size(); i += 2) {
        env_.insert(env.at(i), env.at(i + 1));
    }

    QByteArray ba = serializeEnv(env_);

    QString err;

    // qDebug() << ba;

    PROCESS_INFORMATION pinfo;

    SECURITY_ATTRIBUTES saAttr = {0};
    saAttr.nLength = sizeof(saAttr);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE g_hChildStd_IN_Rd = NULL;
    HANDLE g_hChildStd_IN_Wr = NULL;
    HANDLE g_hChildStd_OUT_Rd = INVALID_HANDLE_VALUE;
    HANDLE g_hChildStd_OUT_Wr = NULL;

    QString name = QString("\\\\.\\Pipe\\NpackdExecute.%1.%2").arg(
            GetCurrentProcessId()).arg(
            nextNamePipeId.fetchAndAddOrdered(1));

    if (job->shouldProceed()) {
        g_hChildStd_OUT_Rd = CreateNamedPipe(
                reinterpret_cast<LPCWSTR>(name.utf16()),
                PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_WAIT, 1,
                128, 128, 1000, &saAttr);
        job->checkOSCall(g_hChildStd_OUT_Rd != INVALID_HANDLE_VALUE);
    }

    if (job->shouldProceed()) {
        g_hChildStd_OUT_Wr = CreateFileW(
                reinterpret_cast<LPCWSTR>(name.utf16()),
                GENERIC_WRITE,
                0,
                &saAttr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
        job->checkOSCall(g_hChildStd_OUT_Wr != INVALID_HANDLE_VALUE);
    }

    if (job->shouldProceed()) {
        job->checkOSCall(
                SetHandleInformation(g_hChildStd_OUT_Rd,
                HANDLE_FLAG_INHERIT, 0));
    }

    if (job->shouldProceed()) {
        job->checkOSCall(
                CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0));
    }

    if (job->shouldProceed()) {
        job->checkOSCall(
                SetHandleInformation(g_hChildStd_IN_Wr,
                HANDLE_FLAG_INHERIT, 0));
    }

    bool success = false;

    // create the process
    if (job->shouldProceed()) {
        STARTUPINFOW startupInfo = {
            sizeof(STARTUPINFO), 0, 0, 0,
            (ulong) CW_USEDEFAULT, (ulong) CW_USEDEFAULT,
            (ulong) CW_USEDEFAULT, (ulong) CW_USEDEFAULT,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };

        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
        startupInfo.hStdInput = g_hChildStd_IN_Rd;
        startupInfo.hStdOutput = g_hChildStd_OUT_Wr;
        startupInfo.hStdError = g_hChildStd_OUT_Wr;

        QString args = "\"" + path + "\"";
        if (!nativeArguments.isEmpty())
            args = args + ' ' + nativeArguments;
        success = CreateProcess(
                (wchar_t*) path.utf16(),
                (wchar_t*) args.utf16(),
                0, &saAttr, TRUE,
                CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                ba.data(),
                (wchar_t*) where.utf16(),
                &startupInfo, &pinfo);

        job->checkOSCall(success);

        // ignore the possible errors here
        CloseHandle(g_hChildStd_OUT_Wr);
    }

    // try to assign the process to a job
    HANDLE job_ = INVALID_HANDLE_VALUE;
    if (job->shouldProceed()) {
        BOOL r;
        if (IsProcessInJob(pinfo.hProcess, 0, &r) && !r) {
            job_ = CreateJobObject(NULL, NULL);
            if (job_) {
                if (!AssignProcessToJobObject(job_, pinfo.hProcess)) {
                    CloseHandle(job_);
                    job_ = INVALID_HANDLE_VALUE;
                }
            }
        }
    }

    // determine the output channel
    HANDLE hStdout = INVALID_HANDLE_VALUE;
    bool consoleOutput = false;
    if (job->shouldProceed()) {
        if (printScriptOutput) {
            hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

            // we do not check GetLastError here as it sometimes returns
            // 2=The system cannot find the file specified.
            // GetFileType returns 0 if an error occures so that the == check below
            // is sufficient
            DWORD ft = GetFileType(hStdout);
            consoleOutput = (ft & ~(FILE_TYPE_REMOTE)) ==
                    FILE_TYPE_CHAR;

            DWORD consoleMode;
            if (consoleOutput) {
                if (!GetConsoleMode(hStdout, &consoleMode))
                    consoleOutput = false;
            }
        } else {
            hStdout = INVALID_HANDLE_VALUE;
            consoleOutput = false;
        }
    }

    // read the output
    if (job->shouldProceed()) {
        OVERLAPPED stOverlapped = {0};
        HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        stOverlapped.hEvent = hEvent;

        int64_t outputLen = 0;
        DWORD ec = 0;
        const int BUFSIZE = 1024;
        char* chBuf = new char[BUFSIZE];
        while (true) {
            job->checkTimeout();

            if (job->isCancelled()) {
                break;
            }

            DWORD dwRead;
            bool bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, BUFSIZE,
                    &dwRead, &stOverlapped);

            bool eof = false;

            if (!bSuccess) {
                DWORD e = GetLastError();

                /*
                WPMUtils::writeln(QString("!success %1 %2").arg(e).arg(dwRead));
                WPMUtils::inputTextConsole();
                */

                switch (e) {
                    case ERROR_HANDLE_EOF:
                    case ERROR_BROKEN_PIPE:
                        eof = true;
                        break;
                    case ERROR_IO_PENDING:
                    {
                        bool pending = true;
                        while (pending && job->shouldProceed()) {
                            job->checkTimeout();

                            if (!GetOverlappedResult(g_hChildStd_OUT_Rd,
                                &stOverlapped, &dwRead,
                                FALSE)) {
                                DWORD e2 = GetLastError();

                                /*
                                WPMUtils::writeln(QString("err2: %1 %2").
                                        arg(e2).arg(dwRead));
                                WPMUtils::inputTextConsole();
                                */

                                switch (e2) {
                                    case ERROR_HANDLE_EOF:
                                    case ERROR_BROKEN_PIPE:
                                        eof = true;
                                        pending = false;
                                        break;
                                    case ERROR_IO_INCOMPLETE:
                                        pending = true;
                                        Sleep(50);
                                        continue;
                                    default:
                                        formatMessage(e, &err);
                                        job->setErrorMessage(err);
                                        pending = false;
                                }
                            } else {
                                ResetEvent(stOverlapped.hEvent);
                                pending = false;
                            }
                        }
                        break;
                    }
                    default:
                        formatMessage(e, &err);
                        job->setErrorMessage(err);
                }
            } else {
                eof = dwRead == 0;
            }

            if (eof)
                break;

            if (!job->shouldProceed())
                break;

            if (dwRead == 0)
                continue;

            outputLen += dwRead;

            if (hStdout != INVALID_HANDLE_VALUE) {
                DWORD dwWritten;
                if (consoleOutput)
                    WriteConsoleW(hStdout, chBuf, dwRead / 2, &dwWritten, 0);
                else {
                    // convert to UTF-8
                    QString s;
                    s.setUtf16((const ushort*) chBuf, dwRead / 2);
                    QByteArray ba = s.toUtf8();

                    WriteFile(hStdout, ba.constData(), ba.length(),
                            &dwWritten, 0);
                }
            }

            if (outputFile) {
                if (outputFile->write(chBuf, dwRead) == -1) {
                    job->setErrorMessage(outputFile->errorString());
                    break;
                }
            }

            time_t seconds = time(NULL) - start;
            double percents = ((double) seconds) / 300; // 5 Minutes
            if (percents > 0.9)
                percents = 0.9;
            job->setProgress(percents);
            job->setTitle(initialTitle + " / " +
                    QString(QObject::tr("%1 minutes")).
                    arg(seconds / 60));
        }
        delete[] chBuf;

        if (hEvent != 0)
            CloseHandle(hEvent);

        // ignore possible errors here
        CloseHandle(g_hChildStd_OUT_Rd);

        if (GetExitCodeProcess(pinfo.hProcess, &ec) && ec == STILL_ACTIVE) {
            if (job->isCancelled()) {
                if (job_ != INVALID_HANDLE_VALUE) {
                    TerminateJobObject(job_, 0xFFFFFFFF);
                } else {
                    TerminateProcess(pinfo.hProcess, 0xFFFFFFFF);
                }
            }
            WaitForSingleObject(pinfo.hProcess, INFINITE);
        }

        if (job->shouldProceed()) {
            job->checkOSCall(GetExitCodeProcess(pinfo.hProcess, &ec));
        }

        if (ec != 0) {
            job->setErrorMessage(
                    QString(QObject::tr("Process %1 exited with the code %2")).
                    arg(path).arg(ec));
        }
    }
    job->setTitle(initialTitle);

    if (job_ != INVALID_HANDLE_VALUE) {
        CloseHandle(job_);
    }

    if (success) {
        // ignore possible errors here
        CloseHandle(pinfo.hThread);
        CloseHandle(pinfo.hProcess);
    }

    job->complete();
}

