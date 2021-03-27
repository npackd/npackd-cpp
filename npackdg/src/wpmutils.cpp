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
#include <memory>
#include <taskschd.h>
#include <comdef.h>
#include <sddl.h>
#include <objbase.h>
#include <unordered_set>

#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QFile>
#include <QCryptographicHash>
#include <QFile>
#include <QVariant>
#include <QBuffer>
#include <QByteArray>
#include <QUrl>
#include <QLoggingCategory>
#include <QDirIterator>
#include <QThread>

#include <quazip.h>
#include <quazipfile.h>

// reduces the size of NpackdCL by 5 MiB
#ifdef QT_GUI_LIB
#include <QImage>
#endif

#include "wpmutils.h"
#include "packageutils.h"
#include "version.h"
#include "windowsregistry.h"
#include "lockedfiles.h"
#include "comobject.h"

QAtomicInt WPMUtils::nextNamePipeId;

HANDLE WPMUtils::hEventLog = nullptr;

const char* WPMUtils::UCS2LE_BOM = "\xFF\xFE";

const char* WPMUtils::CRLF = "\r\n";

QString WPMUtils::taskName;

HRTimer WPMUtils::timer(2);

Q_LOGGING_CATEGORY(npackd, "npackd")
Q_LOGGING_CATEGORY(npackdImportant, "npackd.important")

WPMUtils::WPMUtils()
{
}

void WPMUtils::FindDesktopFolderView(REFIID riid, void **ppv, QString* err)
{
    *err = "";

    COMObject<IShellWindows> spShellWindows;

    HRESULT hr = CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL,
                    IID_IShellWindows,
                     reinterpret_cast<void**>(&spShellWindows.ptr));
    if (FAILED(hr)) {
        formatMessage(hr, err);
    }

    _variant_t vtLoc(CSIDL_DESKTOP);
    _variant_t vtEmpty;

    long lhwnd;
    COMObject<IDispatch> spdisp;
    if (err->isEmpty()) {
        HRESULT hr = spShellWindows->FindWindowSW(&vtLoc, &vtEmpty,
            SWC_DESKTOP, &lhwnd, SWFO_NEEDDISPATCH, &spdisp.ptr);
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }

    COMObject<IShellBrowser> spBrowser;

    COMObject<IServiceProvider> serviceProvider;
    if (err->isEmpty()) {
        HRESULT hr = spdisp->QueryInterface(IID_IServiceProvider,
            reinterpret_cast<void**>(&serviceProvider.ptr));
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }

    if (err->isEmpty()) {
        HRESULT hr = serviceProvider->QueryService(SID_STopLevelBrowser,
                      IID_PPV_ARGS(&spBrowser.ptr));
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }

    COMObject<IShellView> spView;
    if (err->isEmpty()) {
        HRESULT hr = spBrowser->QueryActiveShellView(&spView.ptr);
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }

    if (err->isEmpty()) {
        HRESULT hr = spView->QueryInterface(riid, ppv);
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }
}

void WPMUtils::GetDesktopAutomationObject(REFIID riid, void **ppv, QString* err)
{
    *err = "";

    COMObject<IShellView> spsv;
    FindDesktopFolderView(IID_PPV_ARGS(&spsv.ptr), err);

    COMObject<IDispatch> spdispView;
    if (err->isEmpty()) {
        HRESULT hr = spsv->GetItemObject(SVGIO_BACKGROUND,
            IID_PPV_ARGS(&spdispView.ptr));
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }

    if (err->isEmpty()) {
        HRESULT hr = spdispView->QueryInterface(riid, ppv);
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }
}

void WPMUtils::ShellExecuteFromExplorer(
    const QString& pszFile,
    const QString& pszParameters,
    const QString& pszDirectory,
    const QString& pszOperation,
    int nShowCmd, QString* err)
{
    *err = "";

    COMObject<IShellFolderViewDual> spFolderView;
    GetDesktopAutomationObject(IID_PPV_ARGS(&spFolderView.ptr), err);

    COMObject<IDispatch> spdispShell;
    if (err->isEmpty()) {
        HRESULT hr = spFolderView->get_Application(&spdispShell.ptr);
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }

    COMObject<IShellDispatch2> shellDispatch2;
    if (err->isEmpty()) {
        HRESULT hr = spdispShell->QueryInterface(IID_IShellDispatch2,
            reinterpret_cast<void**>(&shellDispatch2.ptr));
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }

    if (err->isEmpty()) {
        HRESULT hr = shellDispatch2->ShellExecute(
            _bstr_t(WPMUtils::toLPWSTR(pszFile)),
            _variant_t(WPMUtils::toLPWSTR(pszParameters)),
            _variant_t(WPMUtils::toLPWSTR(pszDirectory)),
            _variant_t(WPMUtils::toLPWSTR(pszOperation)),
            _variant_t(nShowCmd));
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }
}

QString WPMUtils::getMessagesLog()
{
    static QString log = WPMUtils::getShellDir(PackageUtils::globalMode ?
            FOLDERID_ProgramData : FOLDERID_RoamingAppData) +
            "\\Npackd\\Messages.log";

    return log;
}

bool WPMUtils::isTaskEnabled(QString* err)
{
    bool result = false;

    err->clear();

    IRegisteredTask* res = nullptr;

    // this function requires better error handling
    ITaskService *pITS = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TaskScheduler,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_ITaskService,
            reinterpret_cast<void **>(&pITS));
    if (FAILED(hr)) {
        formatMessage(hr, err);
    }

    if (err->isEmpty()) {
        hr = pITS->Connect(_variant_t(), _variant_t(),
                _variant_t(), _variant_t());
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }

    ITaskFolder *rootFolder = nullptr;
    if (err->isEmpty()) {
        hr = pITS->GetFolder(_bstr_t(L"\\"), &rootFolder);
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }

    ITaskFolderCollection* folders = nullptr;
    if (err->isEmpty()) {
        hr = rootFolder->GetFolders(0, &folders);
        if (FAILED(hr)) {
            formatMessage(hr, err);
        }
    }

    if (taskName.isEmpty()) {
        taskName = getTaskName();
    }

    ITaskFolder* npackdFolder = nullptr;
    const _bstr_t npackdFolderName(L"Npackd");
    if (err->isEmpty()) {
        LONG count;
        hr = folders->get_Count(&count);
        if (FAILED(hr)) {
            formatMessage(hr, err);
        } else {
            for (int i = 1; i <= count; i++) {
                ITaskFolder* ff;
                hr = folders->get_Item(_variant_t(static_cast<long>(i)), &ff);
                if (SUCCEEDED(hr)) {
                    BSTR n;
                    hr = ff->get_Name(&n);
                    if (SUCCEEDED(hr)) {
                        _bstr_t folderName(n, false);
                        if (folderName == npackdFolderName) {
                            npackdFolder = ff;
                        }
                    }

                    if (npackdFolder)
                        break;

                    ff->Release();
                }
            }
        }
    }

    if (err->isEmpty()) {
        if (npackdFolder) {
            hr = npackdFolder->GetTask(_bstr_t(toLPWSTR(taskName)), &res);
            if (FAILED(hr)) {
                formatMessage(hr, err);
            }
        }
    }

    if (err->isEmpty()) {
        if (res) {
            VARIANT_BOOL b;
            hr = res->get_Enabled(&b);
            if (FAILED(hr)) {
                formatMessage(hr, err);
            } else {
                result = b;
            }
        }
    }

    if (res) {
        res->Release();
    }
    if (npackdFolder) {
        npackdFolder->Release();
    }
    if (folders) {
        folders->Release();
    }
    if (rootFolder) {
        rootFolder->Release();
    }
    if (pITS) {
        pITS->Release();
    }

    return result;
}

PTOKEN_USER WPMUtils::getUserSID(QString* err)
{
    HANDLE hToken = nullptr;

    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &hToken)) {
        DWORD e = GetLastError();
        if (e == ERROR_NO_TOKEN) {
            if (!OpenProcessToken(GetCurrentProcess(),
                    TOKEN_QUERY, &hToken)) {
                WPMUtils::formatMessage(GetLastError(), err);
            }
        } else {
            WPMUtils::formatMessage(e, err);
        }
        qCDebug(npackd) << "hasAdminPrivileges.1";
    }

    // Get the size of the memory buffer needed for the SID
    DWORD dwBufferSize = 0;
    if (err->isEmpty()) {
        if (!GetTokenInformation(hToken, TokenUser, nullptr, 0, &dwBufferSize)) {
            DWORD e = GetLastError();
            if (e != ERROR_INSUFFICIENT_BUFFER)   {
                WPMUtils::formatMessage(e, err);
            }
        }
    }

    PTOKEN_USER pTokenUser = nullptr;
    if (err->isEmpty()) {
        pTokenUser = reinterpret_cast<PTOKEN_USER>(new char[dwBufferSize]);

        // Retrieve the token information in a TOKEN_USER structure
        if (!GetTokenInformation(hToken, TokenUser, pTokenUser,
                dwBufferSize, &dwBufferSize)) {
            WPMUtils::formatMessage(GetLastError(), err);
        }
    }

    // Check if SID is valid
    if (err->isEmpty()) {
        if (!IsValidSid(pTokenUser->User.Sid)) {
            *err = "Invalid user SID";
        }
    }

    if (hToken)
        CloseHandle(hToken);

    if (!err->isEmpty()) {
        delete[] pTokenUser;
        pTokenUser = nullptr;
    }

    return pTokenUser;
}

QString WPMUtils::convertSidToString(PSID pSID, QString *err)
{
    QString r;

    err->clear();

    // Get string corresponding to SID
    LPTSTR pszSID = nullptr;
    if (!ConvertSidToStringSid(pSID, &pszSID)) {
        formatMessage(GetLastError(), err);
    } else {
        r = QString::fromWCharArray(pszSID);
    }

    // Release buffer allocated by ConvertSidToStringSid API
    LocalFree(pszSID);

    // Return string representation of the SID
    return r;
}

QString WPMUtils::createMSTask(bool enabled)
{
    QString err;

    qCDebug(npackd) << "createMSTask";


    // this function requires better error handling
    ITaskService *pITS = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TaskScheduler,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_ITaskService,
            reinterpret_cast<void **>(&pITS));
    if (FAILED(hr)) {
        formatMessage(hr, &err);
    }

    if (err.isEmpty()) {
        hr = pITS->Connect(_variant_t(), _variant_t(),
                _variant_t(), _variant_t());
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    ITaskFolder *rootFolder = nullptr;
    if (err.isEmpty()) {
        hr = pITS->GetFolder(_bstr_t(L"\\"), &rootFolder);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    qCDebug(npackd) << "createMSTask.1";

    ITaskFolderCollection* folders = nullptr;
    if (err.isEmpty()) {
        hr = rootFolder->GetFolders(0, &folders);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    if (taskName.isEmpty()) {
        taskName = getTaskName();
    }

    ITaskFolder* npackdFolder = nullptr;
    const _bstr_t npackdFolderName(L"Npackd");
    if (err.isEmpty()) {
        LONG count;
        hr = folders->get_Count(&count);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        } else {
            for (int i = 1; i <= count; i++) {
                ITaskFolder* ff;
                hr = folders->get_Item(_variant_t(static_cast<long>(i)), &ff);
                if (SUCCEEDED(hr)) {
                    BSTR n;
                    hr = ff->get_Name(&n);
                    if (SUCCEEDED(hr)) {
                        _bstr_t folderName(n, false);
                        if (folderName == npackdFolderName) {
                            npackdFolder = ff;
                        }
                    }

                    if (npackdFolder)
                        break;

                    ff->Release();
                }
            }
        }
    }

    qCDebug(npackd) << "createMSTask.2";

    if (err.isEmpty()) {
        if (!npackdFolder) {
            // G:BA - SDDL_BUILTIN_ADMINISTRATORS
            // D:(A;OICI;GRGW;;;BA) -
            //     A = access allowed
            //     OI = SDDL_OBJECT_INHERIT
            //     CI = SDDL_CONTAINER_INHERIT
            //     GA = SDDL_GENERIC_ALL
            //     BA = SDDL_BUILTIN_ADMINISTRATORS
            hr = rootFolder->CreateFolder(npackdFolderName, _variant_t(L"G:BAD:(A;OICI;GA;;;BA)"), &npackdFolder);
            if (FAILED(hr)) {
                formatMessage(hr, &err);
            }
        }
    }

    if (err.isEmpty()) {
        npackdFolder->DeleteTask(_bstr_t(toLPWSTR(taskName)), 0);
    }

    ITaskDefinition *pTask = nullptr;
    if (err.isEmpty()) {
        hr = pITS->NewTask(0, &pTask);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    qCDebug(npackd) << "createMSTask.3";

    IRegistrationInfo *pRegInfo = nullptr;
    if (err.isEmpty()) {
        hr = pTask->get_RegistrationInfo(&pRegInfo);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    if (err.isEmpty()) {
        hr = pRegInfo->put_Author(_bstr_t(L"Npackd"));
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    qCDebug(npackd) << "createMSTask.4";

    if (err.isEmpty()) {
        hr = pRegInfo->put_Description(_bstr_t(L"WARNING: do not change this task. It is automatically re-created. "
                "Checks for package updates and shows a notification if any were found."));
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    IPrincipal *pPrincipal = nullptr;
    if (err.isEmpty()) {
        hr = pTask->get_Principal(&pPrincipal);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    if (err.isEmpty()) {
        hr = pPrincipal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    if (err.isEmpty()) {
        hr = pPrincipal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    ITaskSettings *pSettings = nullptr;
    if (err.isEmpty()) {
        hr = pTask->get_Settings(&pSettings);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    qCDebug(npackd) << "createMSTask.5";

    if (err.isEmpty()) {
        hr = pSettings->put_StartWhenAvailable(VARIANT_TRUE);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    IIdleSettings *pIdleSettings = nullptr;
    if (err.isEmpty()) {
        hr = pSettings->get_IdleSettings(&pIdleSettings);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    if (err.isEmpty()) {
        hr = pIdleSettings->put_WaitTimeout(_bstr_t(L"PT5M"));
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    ITriggerCollection *pTriggerCollection = nullptr;
    if (err.isEmpty()) {
        hr = pTask->get_Triggers( &pTriggerCollection );
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    qCDebug(npackd) << "createMSTask.6";

    ITrigger *pTrigger = nullptr;
    if (err.isEmpty()) {
        hr = pTriggerCollection->Create(TASK_TRIGGER_DAILY, &pTrigger);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    IDailyTrigger *pTimeTrigger = nullptr;
    if (err.isEmpty()) {
        hr = pTrigger->QueryInterface(
                IID_IDailyTrigger, reinterpret_cast<void**>(&pTimeTrigger));
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    if (err.isEmpty()) {
        hr = pTimeTrigger->put_Id(_bstr_t(L"Trigger1"));
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    if (err.isEmpty()) {
        hr = pTimeTrigger->put_StartBoundary(_bstr_t(L"2019-01-01T12:00:00"));
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    if (err.isEmpty()) {
        hr = pTimeTrigger->put_RandomDelay(_bstr_t(L"P1D") );
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    if (err.isEmpty()) {
        hr = pTimeTrigger->put_ExecutionTimeLimit(_bstr_t(L"PT30M") );
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    IActionCollection *pActionCollection = nullptr;
    if (err.isEmpty()) {
        hr = pTask->get_Actions(&pActionCollection);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    qCDebug(npackd) << "createMSTask.7";

    IAction *pAction = nullptr;
    if (err.isEmpty()) {
        hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    IExecAction *pExecAction = nullptr;
    if (err.isEmpty()) {
        hr = pAction->QueryInterface(
                IID_IExecAction, reinterpret_cast<void**>(&pExecAction));
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    if (err.isEmpty()) {
        QString exePath = getExeFile();
        _bstr_t path(toLPWSTR(exePath));
        hr = pExecAction->put_Path(path);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    if (err.isEmpty()) {
        _bstr_t args(L"check-for-updates");
        hr = pExecAction->put_Arguments(args);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    qCDebug(npackd) << "createMSTask.8";

    IRegisteredTask *pRegisteredTask = nullptr;
    if (err.isEmpty()) {
        hr = npackdFolder->RegisterTaskDefinition(
                _bstr_t(toLPWSTR(taskName)),
                pTask,
                TASK_CREATE_OR_UPDATE,
                _variant_t(),
                _variant_t(),
                TASK_LOGON_INTERACTIVE_TOKEN,
                _variant_t(L""),
                &pRegisteredTask);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    if (err.isEmpty()) {
        hr = pRegisteredTask->put_Enabled(enabled);
        if (FAILED(hr)) {
            formatMessage(hr, &err);
        }
    }

    qCDebug(npackd) << "createMSTask.9";

    if (pRegisteredTask) {
        pRegisteredTask->Release();
    }
    if (pExecAction) {
        pExecAction->Release();
    }
    if (pAction) {
        pAction->Release();
    }

    qCDebug(npackd) << "createMSTask.9.25";

    if (pActionCollection) {
        pActionCollection->Release();
    }
    if (pTimeTrigger) {
        pTimeTrigger->Release();
    }
    if (pTrigger) {
        pTrigger->Release();
    }

    qCDebug(npackd) << "createMSTask9.5";

    if (pTriggerCollection) {
        pTriggerCollection->Release();
    }
    if (pIdleSettings) {
        pIdleSettings->Release();
    }
    if (pSettings) {
        pSettings->Release();
    }
    if (pPrincipal) {
        pPrincipal->Release();
    }
    if (pRegInfo) {
        pRegInfo->Release();
    }

    qCDebug(npackd) << "createMSTask9.75";

    if (pTask) {
        pTask->Release();
    }
    if (npackdFolder) {
        npackdFolder->Release();
    }
    if (folders) {
        folders->Release();
    }
    if (rootFolder) {
        rootFolder->Release();
    }
    if (pITS) {
        pITS->Release();
    }

    qCDebug(npackd) << "createMSTask.10";

    return err;
}

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

    if (programCloseType & WPMUtils::STOP_SERVICES) {
        r += "d";
    }

    if (programCloseType & WPMUtils::CTRL_C) {
        r += "t";
    }

    return r;
}

QString WPMUtils::getProgramFilesDir()
{
    QString ret;

    if (is64BitWindows()) {
        WindowsRegistry wr;
        QString err = wr.open(HKEY_LOCAL_MACHINE,
                QStringLiteral(
                "SOFTWARE\\Microsoft\\Windows\\CurrentVersion"), false,
                KEY_READ);

        if (err.isEmpty()) {
            ret = wr.get(QStringLiteral("ProgramFilesDir"), &err);
            if (!err.isEmpty())
                ret = "";
        }
    }

    if (ret.isEmpty()) {
        ret = getShellDir(FOLDERID_ProgramFiles);
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
    const QString dbs = QStringLiteral("\\\\");
    const QString sbs = QStringLiteral("\\");
    while (oldlen != newlen) {
        path->replace(dbs, sbs);
        oldlen = newlen;
        newlen = path->length();
    }

    if (path->endsWith('\\'))
        path->chop(1);
    if (path != QStringLiteral("..") && path->endsWith('.'))
        path->chop(1);
}

std::vector<QString> WPMUtils::parseCommandLine(const QString& commandLine,
    QString* err) {
    *err = "";

    std::vector<QString> params;

    int nArgs;
    LPWSTR* szArglist = CommandLineToArgvW(WPMUtils::toLPWSTR(commandLine), &nArgs);
    if (nullptr == szArglist) {
        *err = QObject::tr("CommandLineToArgvW failed");
    } else {
        for(int i = 0; i < nArgs; i++) {
            params.push_back(QString::fromUtf16(reinterpret_cast<ushort*>(szArglist[i])));
        }
        LocalFree(szArglist);
    }

    return params;
}

#ifdef QT_GUI_LIB

// Qt 5.2.1
// qtbase/src/gui/image/qpixmap_win.cpp:qt_pixmapFromWinHICON has a bug.
// It crashes in the following line sometimes:
//     return QPixmap::fromImage(image);
// => this function only uses QImage
QImage my_qt_pixmapFromWinHICON(HICON icon)
{
    // qCDebug(npackd) << "my_qt_pixmapFromWinHICON 0";

    HDC screenDevice = GetDC(0);
    HDC hdc = CreateCompatibleDC(screenDevice);
    ReleaseDC(0, screenDevice);

    ICONINFO iconinfo;
    bool result = GetIconInfo(icon, &iconinfo);
    if (!result)
        qWarning("QPixmap::fromWinHICON(), failed to GetIconInfo()");

    // qCDebug(npackd) << "my_qt_pixmapFromWinHICON 1";

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

    // qCDebug(npackd) << "my_qt_pixmapFromWinHICON 2" << w << h;

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

    // qCDebug(npackd) << "my_qt_pixmapFromWinHICON 3";

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

    // qCDebug(npackd) << "my_qt_pixmapFromWinHICON 4";

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

    // qCDebug(npackd) << "my_qt_pixmapFromWinHICON 5";

    return image;
}

QString WPMUtils::extractIconURL(const QString& iconFile)
{
    // qCDebug(npackd) << "extractIconURL 0";

    QString res;
    QString icon = iconFile.trimmed();
    if (!icon.isEmpty()) {
        // qCDebug(npackd) << "extractIconURL 1";

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

        // qCDebug(npackd) << "extractIconURL 2" << icon << iconIndex;

        HICON ic = ExtractIcon(GetModuleHandle(NULL),
                WPMUtils::toLPWSTR(icon), iconIndex);

        // qCDebug(npackd) << "extractIconURL 2.1" << ((UINT_PTR) ic);

        if (((UINT_PTR) ic) > 1) {
            // qCDebug(npackd) << "extractIconURL 3" << ii << info.fIcon << info.xHotspot;

            QImage pm = my_qt_pixmapFromWinHICON(ic); // QtWin::fromHICON(ic);

            // qCDebug(npackd) << "extractIconURL 4";

            if (!pm.isNull() && pm.width() > 0 &&
                    pm.height() > 0) {
                // qCDebug(npackd) << "extractIconURL 5";

                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                pm.save(&buffer, "PNG");
                res = QStringLiteral("data:image/png;base64,") +
                        bytes.toBase64();

                // qCDebug(npackd) << "extractIconURL 6";
            }
            DestroyIcon(ic);

            // qCDebug(npackd) << "extractIconURL 7";
        }
    }

    return res;
}
#else
QString WPMUtils::extractIconURL(const QString& /*iconFile*/)
{
    return "";
}
#endif

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
                       err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPTSTR)&pBuffer, 0, nullptr);
    } else {
        n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_SYSTEM,
                       nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPTSTR)&pBuffer, 0, nullptr);
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

BOOL CALLBACK myEnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    std::vector<HWND>* p = (std::vector<HWND>*) lParam;
    p->push_back(hwnd);
    return TRUE;
}

std::vector<HWND> WPMUtils::findTopWindows()
{
    std::vector<HWND> r;
    EnumWindows(myEnumWindowsProc, (LPARAM) &r);
    return r;
}

std::vector<HWND> WPMUtils::findProcessTopWindows(DWORD processID)
{
    std::vector<HWND> r;

    std::vector<HWND> tws = findTopWindows();

    for (auto w: tws) {
        DWORD pid;
        if (GetWindowThreadProcessId(w, &pid) && pid == processID) {
            r.push_back(w);
        }
    }

    return r;
}

bool WPMUtils::isProcessRunning(HANDLE process)
{
    bool r = false;
    DWORD ec;
    if (GetExitCodeProcess(process, &ec)) {
        // qCDebug(npackd) << "exit code" << ec << STILL_ACTIVE;
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
    if (NetConnectionEnum(nullptr, netname, 1,
            (LPBYTE*) &ci, MAX_PREFERRED_LENGTH,
            &entriesRead, &totalEntries, &resumeHandle) == NERR_Success) {
        for (int i = 0; i < (int) entriesRead; i++) {
            NetSessionDel(nullptr, ci[i].coni1_netname, ci[i].coni1_username);
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
    if (NetShareEnum(nullptr, 502, (LPBYTE*) &buf, MAX_PREFERRED_LENGTH,
            &entriesRead, &totalEntries, &resumeHandle) == NERR_Success) {
        QString dirNormalized = normalizePath(dir);
#ifndef STYPE_MASK
        const DWORD STYPE_MASK = 0xF0000000;
#endif
        // qCDebug(npackd) << entriesRead;
        for (int i = 0; i < (int) entriesRead; i++) {
            // qCDebug(npackd) << "share " << buf[i].shi502_type;
            if ((buf[i].shi502_type & STYPE_MASK) == STYPE_DISKTREE) {
                QString path = QString::fromUtf16(
                        (const ushort*) buf[i].shi502_path);
                path = normalizePath(path);
                //qCDebug(npackd) << "share found" << path;
                if (isUnderOrEquals(path, dirNormalized)) {
                    //qCDebug(npackd) << "share found" << path;
                    QString netName;
                    netName.setUtf16((const ushort*) buf[i].shi502_netname,
                            (int)wcslen(buf[i].shi502_netname));
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

bool WPMUtils::isDirShared(const QString &dir)
{
    bool result = false;

    SHARE_INFO_502* buf;
    DWORD entriesRead, totalEntries, resumeHandle;
    resumeHandle = 0;
    if (NetShareEnum(nullptr, 502, (LPBYTE*) &buf, MAX_PREFERRED_LENGTH,
            &entriesRead, &totalEntries, &resumeHandle) == NERR_Success) {
        QString dirNormalized = normalizePath(dir);
#ifndef STYPE_MASK
        const DWORD STYPE_MASK = 0xF0000000;
#endif
        // qCDebug(npackd) << entriesRead;
        for (int i = 0; i < (int) entriesRead; i++) {
            // qCDebug(npackd) << "share " << buf[i].shi502_type;
            if ((buf[i].shi502_type & STYPE_MASK) == STYPE_DISKTREE) {
                QString path;
                path.setUtf16((const ushort*) buf[i].shi502_path,
					(int)wcslen(buf[i].shi502_path));
                path = normalizePath(path);
                //qCDebug(npackd) << "share found" << path;
                if (isUnderOrEquals(path, dirNormalized)) {
                    result = true;
                    break;
                }
            }
        }
    }
    NetApiBufferFree(buf);

    return result;
}

QString WPMUtils::findService(DWORD processId, QString* err)
{
    *err = "";

    QString r;

    DWORD pcbBytesNeeded;
    DWORD lpServicesReturned;
    DWORD lpResumeHandle = 0;
    ENUM_SERVICE_STATUS_PROCESS* lpServices = nullptr;
    DWORD bufSize;

    // Get a handle to the SCM database.
    SC_HANDLE schSCManager = OpenSCManager(
            nullptr,                    // local computer
            nullptr,                    // ServicesActive database
            SC_MANAGER_ALL_ACCESS);  // full access rights

    if (nullptr == schSCManager) {
        formatMessage(GetLastError(), err);
        *err = QObject::tr("OpenSCManager failed: %0").arg(*err);
    }

    if (err->isEmpty()) {
        if (EnumServicesStatusEx(schSCManager, SC_ENUM_PROCESS_INFO,
                SERVICE_WIN32, SERVICE_ACTIVE, nullptr, 0, &pcbBytesNeeded,
                &lpServicesReturned, &lpResumeHandle, nullptr)) {
            *err = QObject::tr("EnumServicesStatusEx: no services found");
        } else {
            DWORD e = GetLastError();
            if (e != ERROR_MORE_DATA) {
                formatMessage(e, err);
                *err = QObject::tr("EnumServicesStatusEx failed: %0").arg(*err);
            }
        }
    }

    if (err->isEmpty()) {
        bufSize = pcbBytesNeeded;
        lpServices = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESS*>(
                malloc(bufSize));

        if (!EnumServicesStatusEx(schSCManager, SC_ENUM_PROCESS_INFO,
                SERVICE_WIN32, SERVICE_ACTIVE,
                reinterpret_cast<LPBYTE>(lpServices), bufSize,
                &pcbBytesNeeded,
                &lpServicesReturned, &lpResumeHandle, nullptr)) {
            formatMessage(GetLastError(), err);
            *err = QObject::tr("EnumServicesStatusEx failed: %0").arg(*err);
        }
    }

    if (err->isEmpty()) {
        for (DWORD i = 0; i < lpServicesReturned; i++) {
            if ((lpServices + i)->ServiceStatusProcess.dwProcessId == processId) {
                r = QString::fromWCharArray((lpServices + i)->lpServiceName);
                break;
            }
        }
    }

    free(lpServices);

    if (schSCManager)
        CloseServiceHandle(schSCManager);

    return r;
}

void WPMUtils::closeHandles(const std::vector<HANDLE> &handles)
{
    for (auto h: handles) {
        CloseHandle(h);
    }
}

QString WPMUtils::getTaskName()
{
    QString r = "CheckForUpdates";

    QString err;
    PTOKEN_USER t = getUserSID(&err);
    if (err.isEmpty()) {
        QString sid = convertSidToString(t->User.Sid, &err);
        if (err.isEmpty()) {
            r.append("-");
            r.append(sid);
        }
    }

    delete[] t;

    return r;
}

void WPMUtils::closeProcessesThatUseDirectory(const QString &dir,
        DWORD cpt, std::vector<QString>* stoppedServices)
{
    //QString f = dir + "\\abc.txt";
    //test((PCWSTR) f.utf16());

    std::vector<HANDLE> ps = LockedFiles::getAllProcessHandlesLockingDirectory(dir);

    qCDebug(npackd) << "Closing processes locking " <<
            dir << " with " << cpt << ": " << ps.size() << " processes";

    //qCDebug(npackd) << "getProcessHandlesLockingDirectory2";

    DWORD me = GetCurrentProcessId();

    // Even if a directory is open in Windows Explorer, it should not be closed.
    // Closing Windows Explorer starts the logoff.
    QString explorer = getWindowsDir() + "\\explorer.exe";

    if (cpt & CLOSE_WINDOW) {
        bool changed = false;
        for (auto p: ps) {
            if (GetProcessId(p) != me && !pathEquals(explorer, getProcessFile(p))) {
                std::vector<HWND> ws = findProcessTopWindows(GetProcessId(p));
                if (ws.size() > 0) {
                    closeProcessWindows(p, ws);
                    changed = true;
                }
            }
        }

        if (changed) {
            closeHandles(ps);
            ps = LockedFiles::getAllProcessHandlesLockingDirectory(dir);
        }
    }

    //qCDebug(npackd) << "Windows closed";

    if (cpt & CTRL_C) {
        bool changed = false;
        for (auto hProc: ps) {
            DWORD processId = GetProcessId(hProc);

            if (processId != 0 && processId != me &&
                    WPMUtils::isProcessRunning(hProc) &&
                    !pathEquals(explorer, getProcessFile(hProc))) {
                // 0 = GR_GDIOBJECTS, 1 = GR_USEROBJECTS
                if (GetGuiResources(hProc, 0) == 0 &&
                        GetGuiResources(hProc, 1) == 0) {
                    qCInfo(npackd).noquote() << QObject::tr("Sending Ctrl+C to \"%1\"").
                            arg(getProcessFile(hProc));
                    if (GenerateConsoleCtrlEvent(CTRL_C_EVENT, processId)) {
                        changed = true;
                    }
                }
            }
        }

        if (changed) {
            closeHandles(ps);
            ps = LockedFiles::getAllProcessHandlesLockingDirectory(dir);
        }
    }

    if (cpt & KILL_PROCESS) {
        bool changed = false;
        for (auto hProc: ps) {
            DWORD processId = GetProcessId(hProc);

            if (processId != 0 && processId != me &&
                    WPMUtils::isProcessRunning(hProc) &&
                    !pathEquals(explorer, getProcessFile(hProc))) {

                qCInfo(npackd).noquote() << QObject::tr("Killing the process \"%1\"").
                        arg(getProcessFile(hProc));

                // TerminateProcess is asynchronous
                if (TerminateProcess(hProc, 1000))
                    WaitForSingleObject(hProc, 30000);

                changed = true;
            }
        }

        if (changed) {
            closeHandles(ps);
            ps = LockedFiles::getAllProcessHandlesLockingDirectory(dir);
        }
    }

    bool shared = isDirShared(dir);

    if (cpt & DISABLE_SHARES) {
        if (shared) {
            WPMUtils::disconnectShareUsersFrom(dir);
            closeHandles(ps);
            ps = LockedFiles::getAllProcessHandlesLockingDirectory(dir);
        }
    }

    if (cpt & DISABLE_SHARES) {
        if (shared) {
            WPMUtils::stopService("lanmanserver", stoppedServices);

            closeHandles(ps);
            ps = LockedFiles::getAllProcessHandlesLockingDirectory(dir);
        }
    }

    if (cpt & STOP_SERVICES) {
        for (auto hProc: ps) {
            DWORD processId = GetProcessId(hProc);

            if (processId != 0 && processId != me &&
                    WPMUtils::isProcessRunning(hProc)) {
                QString err;
                QString service = findService(processId, &err);

                if (!service.isEmpty()) {
                    qCInfo(npackd).noquote() << QObject::tr("Stopping the service \"%1\"").
                            arg(getProcessFile(hProc));

                    WPMUtils::stopService(service, stoppedServices);
                }
            }
        }
    }

    // qCDebug(npackd) << "Processes killed";

    closeHandles(ps);
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
        return QStringLiteral("localhost");
    }
}

void WPMUtils::closeProcessWindows(HANDLE process,
        const std::vector<HWND>& processWindows)
{
    // start flashing
    for (auto w: processWindows) {
        if (w != nullptr && IsWindow(w) &&
                GetAncestor(w, GA_PARENT) == GetDesktopWindow() &&
                IsWindowVisible(w) &&
                getClassName(w) != QStringLiteral("Shell_TrayWnd")) {
            FLASHWINFO fwi = {};
            fwi.cbSize = sizeof(fwi);
            fwi.hwnd = w;
            fwi.dwFlags = FLASHW_ALL | FLASHW_TIMER;
            fwi.uCount = std::numeric_limits<UINT>::max();
            // qCDebug(npackd) << "flash!";
            FlashWindowEx(&fwi);
        }
    }

    std::vector<HWND> ws = processWindows;
    int old = ws.size();
    while (true) {
        int c = 0;
        for (int i = 0; i < static_cast<int>(ws.size()); i++) {
            HWND w = ws.at(i);
            if (w) {
                if (!IsWindow(w) ||
                        GetAncestor(w, GA_PARENT) != GetDesktopWindow() ||
                        !IsWindowVisible(w) ||
                        getClassName(w) == QStringLiteral("Shell_TrayWnd")) {
                    ws[i] = nullptr;
                } else {
                    c++;
                    if ((GetWindowLong(w, GWL_STYLE) & WS_DISABLED) == 0) {
                        //qCDebug(npackd) << "WM_CLOSE to " <<
                        //        GetProcessId(process) << getClassName(w);
                        qCInfo(npackd).noquote() << QObject::tr("Sending a WM_CLOSE event to \"%1\"").
                                arg(getProcessFile(process));
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
    for (auto w: processWindows) {
        if (w != nullptr && IsWindow(w) &&
                GetAncestor(w, GA_PARENT) == GetDesktopWindow() &&
                IsWindowVisible(w) &&
                getClassName(w) != QStringLiteral("Shell_TrayWnd")) {
            FLASHWINFO fwi = {};
            fwi.cbSize = sizeof(fwi);
            fwi.hwnd = w;
            fwi.dwFlags = FLASHW_STOP;
            fwi.uCount = std::numeric_limits<UINT>::max();
            // qCDebug(npackd) << "flash!";
            FlashWindowEx(&fwi);
        }
    }
}

QString WPMUtils::getProcessFile(HANDLE hProcess)
{
    QString res;
    res.resize(MAX_PATH + 1);
    DWORD r = GetModuleFileNameEx(hProcess, nullptr, (LPWSTR) res.data(),
            res.length());
    if (r != 0) {
        res.resize(r);
    }

    return res;
}

// see also http://msdn.microsoft.com/en-us/library/ms683217(v=VS.85).aspx
std::vector<DWORD> WPMUtils::getProcessIDs()
{
    std::vector<DWORD> r;

    int n = 200;

    while (true) {
        r.resize(n);
        DWORD cb = r.size() * sizeof(DWORD);
        DWORD cbneeded;
        if (!EnumProcesses(r.data(), cb, &cbneeded)) {
            r.resize(0);
            break;
        }

        if (cbneeded < cb) {
            r.resize(cbneeded / sizeof(DWORD));
            break;
        } else {
            n *= 2;
        }
    }

    return r;
}

QString WPMUtils::getShellDir(REFKNOWNFOLDERID type)
{
    QString ret;

    PWSTR s;
    if (SUCCEEDED(SHGetKnownFolderPath(type, KF_FLAG_DONT_VERIFY, 0, &s))) {
        ret = QString::fromUtf16(reinterpret_cast<ushort*>(s));
    }
    CoTaskMemFree(s);

    return ret;
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

    return QStringLiteral("");
}

QString WPMUtils::setSystemEnvVar(const QString& name, const QString& value,
        bool expandVars, bool system)
{
	QString err;
    WindowsRegistry wr;
    if (system)
		err = wr.open(HKEY_LOCAL_MACHINE,
			QStringLiteral("System\\CurrentControlSet\\Control\\Session Manager\\Environment"),
			false);
	else
		err = wr.open(HKEY_CURRENT_USER,
			QStringLiteral("Environment"),
			false);

    if (!err.isEmpty())
        return err;

    if (expandVars)
        err = wr.setExpand(name, value);
    else
        err = wr.set(name, value);

    if (!err.isEmpty())
        return err;

    return QStringLiteral("");
}

QString WPMUtils::getFirstLine(const QString& text)
{
    std::vector<QString> sl = WPMUtils::split(text.trimmed(), '\n');
    if (sl.size() > 0)
        return sl.at(0).trimmed();
    else
        return QStringLiteral("");
}

void WPMUtils::fireEnvChanged()
{
    SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
            (LPARAM) L"Environment",
            SMTO_ABORTIFHUNG, 5000, nullptr);
}

QString WPMUtils::getSystemEnvVar(const QString& name, QString* err)
{
    WindowsRegistry wr;
	// Global environment
    err->clear();
    QString e = wr.open(HKEY_LOCAL_MACHINE,
            QStringLiteral("System\\CurrentControlSet\\Control\\Session Manager\\Environment"),
            false);
    if (!e.isEmpty()) {
        *err = e;
        return QStringLiteral("");
    }
	e = wr.get(name, err);
	if (err->isEmpty()) return e;
	// Local user environment
	err->clear();
	e = wr.open(HKEY_CURRENT_USER,
		QStringLiteral("Environment"),
		false);
	if (!e.isEmpty()) {
		*err = e;
		return QStringLiteral("");
	}
	return wr.get(name, err);
}

Version WPMUtils::getDLLVersion(const QString &path)
{
    Version res(0, 0);

    DWORD dwVerHnd;
    DWORD size;
    size = GetFileVersionInfoSize(WPMUtils::toLPWSTR(path), &dwVerHnd);
    if (size != 0) {
        void* mem = malloc(size);
        if (GetFileVersionInfo(WPMUtils::toLPWSTR(path), 0, size, mem)) {
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
            WPMUtils::toLPWSTR(guid),
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
            WPMUtils::toLPWSTR(product),
            WPMUtils::toLPWSTR(guid),
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
    normalizePath2(&a, true);
    normalizePath2(&b, true);
    return QString::compare(a, b, Qt::CaseInsensitive) == 0;
}

QString WPMUtils::join(const std::vector<QString> &items, const QString &delimiter)
{
    QStringList list(items.begin(), items.end());
    return list.join(delimiter);
}

QString WPMUtils::join(const std::vector<QString> &items,
        QChar delimiter)
{
    QStringList list(items.begin(), items.end());
    return list.join(delimiter);
}

QString WPMUtils::getMSIProductName(const QString& guid, QString* err)
{
    return getMSIProductAttribute(guid, INSTALLPROPERTY_PRODUCTNAME, err);
}

std::vector<QString> WPMUtils::findInstalledMSIProducts()
{
    std::vector<QString> result;
    WCHAR buf[39];
    int index = 0;
    while (true) {
        UINT r = MsiEnumProducts(index, buf);
        if (r != ERROR_SUCCESS)
            break;
        QString v;
        v.setUtf16((ushort*) buf, 38);
        result.push_back(v.toLower());
        index++;
    }
    return result;
}

std::vector<QString> WPMUtils::findInstalledMSIComponents()
{
    std::vector<QString> result;
    WCHAR buf[39];
    int index = 0;
    while (true) {
        UINT r = MsiEnumComponents(index, buf);
        if (r != ERROR_SUCCESS)
            break;
        QString v;
        v.setUtf16((ushort*) buf, 38);
        result.push_back(v.toLower());
        index++;
    }
    return result;
}

std::unordered_multimap<QString, QString> WPMUtils::mapMSIComponentsToProducts(
        const std::vector<QString>& components)
{
    std::unordered_multimap<QString, QString> map;
    WCHAR buf[39];
    for (auto& c: components) {
        if (MsiGetProductCode(WPMUtils::toLPWSTR(c), buf) == ERROR_SUCCESS) {
            QString v;
            v.setUtf16((ushort*) buf, 38);
            map.insert({v.toLower(), c});
        }
    }
    return map;
}

QString WPMUtils::getWindowsDir()
{
    WCHAR dir[MAX_PATH];
    // this sometimes returns wrong value (Terminal Services?): SHGetFolderPath(0, CSIDL_WINDOWS, NULL, 0, dir);
    GetSystemWindowsDirectory(dir, MAX_PATH);
    return QString::fromWCharArray(dir);
}

QString WPMUtils::findCmdExe()
{
    QString r = getWindowsDir() + QStringLiteral("\\Sysnative\\cmd.exe");
    if (!QFileInfo::exists(r)) {
        r = getWindowsDir() + QStringLiteral("\\system32\\cmd.exe");
    }
    return r;
}

QString WPMUtils::getExeFile()
{
    TCHAR path[MAX_PATH];
    GetModuleFileName(nullptr, path, sizeof(path) / sizeof(path[0]));
    QString r;
    r.setUtf16((ushort*) path, (int)wcslen(path));

    return r.replace('/', '\\');
}

QString WPMUtils::getExeDir()
{
    TCHAR path[MAX_PATH];
    GetModuleFileName(nullptr, path, sizeof(path) / sizeof(path[0]));
    QString r;
    r.setUtf16((ushort*) path, (int)wcslen(path));

    QDir d(r);
    d.cdUp();
    return d.absolutePath().replace('/', '\\');
}

QString WPMUtils::getShellFileOperationErrorMessage(DWORD res)
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
    qCInfo(npackd).noquote() << QObject::tr(
            "Moving \"%1\" to the recycle bin").
            arg(dir.replace('/', '\\'));

    SHFILEOPSTRUCTW f;
    memset(&f, 0, sizeof(f));
    WCHAR* from = new WCHAR[dir.length() + 2];
    wcscpy(from, WPMUtils::toLPWSTR(dir));
    from[wcslen(from) + 1] = 0;
    f.wFunc = FO_DELETE;
    f.pFrom = from;
    f.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI |
            FOF_SILENT | FOF_NOCONFIRMMKDIR;

    int r = SHFileOperationW(&f);
    delete[] from;

    if (r == 0)
        return QStringLiteral("");
    else {
        return QString(QObject::tr("Error deleting %1: %2")).
                arg(dir, WPMUtils::getShellFileOperationErrorMessage(r));
    }
}

bool WPMUtils::is64BitWindows()
{
#if defined(__x86_64__) || defined(_WIN64)
    return true;
#else
    // 32-bit programs run on both 32-bit and 64-bit Windows
    // so must sniff
	typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

    HINSTANCE hInstLib = LoadLibraryA("KERNEL32.DLL");
    LPFN_ISWOW64PROCESS lpfIsWow64Process_ =
            (LPFN_ISWOW64PROCESS)
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

std::tuple<QString, QString> WPMUtils::readLastLines(const QString &filename)
{
    QString text;
    QString error;

    bool unicode = false;

    QFile f(filename);
    if (f.open(QFile::ReadOnly)) {
        qint64 sz = f.size();
        qint64 p;

        if (unicode) {
            if (sz > 2048) {
                // UTF-16: 2 bytes per character, 2040 to not get the BOM
                p = sz - 2040 - sz % 1;
            } else if (sz >= 2) {
                // BOM
                p = 2;
            } else {
                p = 0;
            }
        } else {
            if (sz > 1024) {
                p = sz - 1024;
            } else {
                p = 0;
            }
        }

        f.seek(p);

        QByteArray ba = f.read(sz - p);

        if (unicode) {
            text = QString::fromUtf16(
                    reinterpret_cast<const ushort*>(ba.constData()),
                    ba.length() / 2);
        } else {
            text = QString::fromLocal8Bit(
                    ba.constData(),
                    ba.length());
        }

        f.close();
    } else {
        error = f.errorString();
    }

    return std::make_tuple(text, error);
}

QString WPMUtils::createLink(LPCWSTR lpszPathObj, LPCWSTR lpszPathLink,
        LPCWSTR lpszDesc,
        LPCWSTR workingDir)
{
    QString r;
    HRESULT hres;
    IShellLink* psl;

    // Get a pointer to the IShellLink interface.
    hres = CoCreateInstance(CLSID_ShellLink, nullptr,
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

bool WPMUtils::copyDirectory(QString src, QString dest)
{
    // qCDebug(npackd) << "copyDirectory";

    QDir srcDir(src);
    QDir destDir(dest);

    if (!destDir.mkpath(dest))
        return false;

    QDirIterator it(src, QDir::AllEntries | QDir::PermissionMask | QDir::AccessMask |
            QDir::CaseSensitive | QDir::NoDotAndDotDot | QDir::NoSymLinks,
            QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString relPath = srcDir.relativeFilePath(it.next());

        // qCDebug(npackd) << "next file" << relPath;

        QFileInfo fi = it.fileInfo();
        if (fi.isDir()) {
            // qCDebug(npackd) << "dir!";
            relPath = WPMUtils::normalizePath(relPath, false);
            // qCDebug(npackd) << "normalized" << relPath;
            if (!relPath.isEmpty() && relPath != QStringLiteral(".") && relPath != QStringLiteral("..")) {
                // qCDebug(npackd) << "mkdir!";
                if (!destDir.mkdir(relPath)) {
                    // qCDebug(npackd) << "mkdir failed";
                    return false;
                }
            }
        } else {
            // qCDebug(npackd) << "file!";
            if (!QFile::copy(srcDir.filePath(relPath), destDir.filePath(relPath)))
                return false;
        }
    }

    return true;
}

void WPMUtils::renameDirectory(Job* job, const QString &oldName, const QString &newName)
{
    bool done = false;

    QDir dir;

    if (job->shouldProceed() && !done) {
        done = dir.rename(oldName, newName);
        if (done)
            job->setProgress(1);
    }

    if (job->shouldProceed() && !done) {
        // 5 seconds
        Sleep(5000);
        done = dir.rename(oldName, newName);
        if (done)
            job->setProgress(1);
    }

    if (job->shouldProceed() && !done) {
        // 10 seconds
        Sleep(10000);
        done = dir.rename(oldName, newName);
        if (done)
            job->setProgress(1);
    }

    if (job->shouldProceed() && !done) {
        if (WPMUtils::copyDirectory(oldName, newName)) {
            job->setProgress(0.5);

            Job* sub = job->newSubJob(0.5, QObject::tr("Deleting %1").arg(oldName), true, false);
            WPMUtils::removeDirectory(sub, oldName, true);
            job->setProgress(1);
        } else {
            job->setErrorMessage(QObject::tr("Error copying %1 to %2").
                arg(oldName, newName));
        }
    }

    job->complete();
}

void WPMUtils::removeDirectory(Job* job, const QString &aDir_, bool firstLevel)
{
    QDir aDir(aDir_);
    if (firstLevel) {
        qCInfo(npackd).noquote() << QObject::tr("Deleting \"%1\"").
                arg(aDir.absolutePath().replace('/', '\\'));
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
                Job* sub = job->newSubJob(1 / ((double) count + 1),
                        QObject::tr("Deleting the directory %1").arg(
                        path.replace('/', '\\')));
                removeDirectory(sub, path, false);
                if (!sub->getErrorMessage().isEmpty())
                    job->setErrorMessage(sub->getErrorMessage());
                // if (!ok)
                //    qCDebug(npackd) << "WPMUtils::removeDirectory.3" << *errMsg;
            } else {
                QFile file(path);
                if (!file.remove() && file.exists()) {
                    job->setErrorMessage(QString(QObject::tr("Cannot delete the file: %1")).
                            arg(path));
                    // qCDebug(npackd) << "WPMUtils::removeDirectory.1" << *errMsg;
                } else {
                    job->setProgress(idx / ((double) count + 1));
                }
            }
            if (!job->getErrorMessage().isEmpty())
                break;
        }

        if (job->getErrorMessage().isEmpty()) {
            if (!aDir.rmdir(aDir.absolutePath()))
                // qCDebug(npackd) << "WPMUtils::removeDirectory.2";
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

QStringList WPMUtils::toQStringList(const std::vector<QString> &v)
{
    QStringList r;
    r.reserve(v.size());
    for (auto& s: v) {
        r.append(s);
    }
    return r;
}

QString WPMUtils::makeValidFilename(const QString &name, QChar rep)
{
    // http://msdn.microsoft.com/en-us/library/aa365247(v=vs.85).aspx
    QString invalid = QStringLiteral("<>:\"/\\|?* ");

    QString r(name);
    for (int i = 0; i < invalid.length(); i++)
        r.replace(invalid.at(i), rep);
    return r;
}

void WPMUtils::writeln(const QString& txt, bool stdout_)
{
    outputTextConsole(txt + QStringLiteral("\r\n"), stdout_);
}

void WPMUtils::outputTextConsole(const QString& txt, bool stdout_, bool useColor, WORD attrs)
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
            CONSOLE_SCREEN_BUFFER_INFO info;
            if (useColor) {
                GetConsoleScreenBufferInfo(hStdout, &info);
                SetConsoleTextAttribute(hStdout, attrs);
            }

            // WriteConsole automatically converts UTF-16 to the code page used
            // by the console
            WriteConsoleW(hStdout, txt.utf16(), txt.length(), &written, nullptr);

            if (useColor) {
                SetConsoleTextAttribute(hStdout, info.wAttributes);
            }
        } else {
            // we always write UTF-8 to the output file
            QByteArray arr = txt.toUtf8();
            WriteFile(hStdout, arr.constData(), arr.length(), &written, nullptr);
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
   return inputTextConsole().trimmed().toLower() == QStringLiteral("y");
}

QString WPMUtils::inputTextConsole()
{
    // http://msdn.microsoft.com/en-us/library/ms686974(v=VS.85).aspx

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE)
        return QStringLiteral("");

    WCHAR buffer[255];
    DWORD read;
    if (!ReadConsoleW(hStdin, &buffer, sizeof(buffer) / sizeof(buffer[0]), &read, nullptr))
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
        return QStringLiteral("");

    HANDLE oh = GetStdHandle(STD_OUTPUT_HANDLE);
    if (oh == INVALID_HANDLE_VALUE)
        return QStringLiteral("");

    if (!GetConsoleMode(ih, &mode))
        return result;

    SetConsoleMode(ih, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));

    // Get the password string
    DWORD count;
    WCHAR c;
    while (true) {
        if (!ReadConsoleW(ih, &c, 1, &count, nullptr) ||
                (c == '\r') || (c == '\n'))
            break;

        if (c == '\b') {
            if (result.length()) {
                WriteConsoleW(oh, L"\b \b", 3, &count, nullptr);
                result.chop(1);
            }
        } else {
            WriteConsoleW(oh, L"*", 1, &count, nullptr);
            result.push_back(c);
        }
    }

    // Restore the console mode
    SetConsoleMode(ih, mode);

    return result;
}

QString WPMUtils::findNonExistingFile(const QString& start,
        const QString &ext)
{
    QString result = start + ext;
    if (!QFileInfo::exists(result))
        return result;

    for (int i = 2; i < 100; i++) {
        result = start + QStringLiteral("_") + QString::number(i) + ext;
        if (!QFileInfo::exists(result))
            return result;
    }

    return start + ext;
}

void WPMUtils::deleteShortcuts(const QString& dir, QDir& d)
{
    // Get a pointer to the IShellLink interface.
    IShellLink* psl;
    HRESULT hres = CoCreateInstance(CLSID_ShellLink, nullptr,
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
            // qCDebug(npackd) << "PackageVersion::deleteShortcuts " << path;
            if (entryInfo.isDir()) {
                QDir dd(path);
                deleteShortcuts(dir, dd);
            } else {
                if (path.toLower().endsWith(".lnk")) {
                    // qCDebug(npackd) << "deleteShortcuts " << path;
                    IPersistFile* ppf;

                    hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

                    if (SUCCEEDED(hres)) {
                        //qCDebug(npackd) << "Loading " << path;

                        hres = ppf->Load(WPMUtils::toLPWSTR(path), STGM_READ);

                        if (SUCCEEDED(hres)) {
                            WCHAR info[MAX_PATH + 1];
                            hres = psl->GetPath(info, MAX_PATH,
                                    (WIN32_FIND_DATAW*) nullptr, 0);
                            if (SUCCEEDED(hres)) {
                                QString targetPath;
                                targetPath.setUtf16((ushort*) info, (int)wcslen(info));
                                // qCDebug(npackd) << "deleteShortcuts " << targetPath << " " <<
                                //        instPath;
                                if (WPMUtils::isUnder(targetPath,
                                                      instPath)) {
                                    QFile::remove(path);
                                    // qCDebug(npackd) << "deleteShortcuts removed";
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
        qCDebug(npackd) << "failed";
    }
}

DWORD WPMUtils::getProgramCloseType(const CommandLine& cl, QString* err)
{
    int r = WPMUtils::CLOSE_WINDOW;
    QString v = cl.get(QStringLiteral("end-process"));
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
                else if (t == 'd')
                    r |= WPMUtils::STOP_SERVICES;
                else if (t == 't')
                    r |= WPMUtils::CTRL_C;
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
        job->setTitle(initialTitle + QStringLiteral(" / ") +
                QObject::tr("%L0 bytes").
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

QString WPMUtils::appendFile(const QString &from, const QString &to)
{
    QString result;

    QFile fromFile(from);
    QFile toFile(to);

    // ignore the errors here
    if (fromFile.open(QIODevice::ReadOnly)) {
        if (toFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
            char* buffer = new char[1024 * 1024];
            while (true) {
                qint64 n = fromFile.read(buffer, 1024 * 1024);

                if (n < 0) {
                    result = fromFile.errorString();
                    break;
                }

                if (n == 0)
                    break;

                if (toFile.write(buffer, n) != n) {
                    result = toFile.errorString();
                    break;
                }
            }
            delete[] buffer;
        } else {
            result = toFile.errorString();
        }
    } else {
        result = fromFile.errorString();
    }

    return result;
}

QString WPMUtils::createEmptyTempFile(const QString &pattern)
{
    QString result;

    // QTemporaryFile::close() does not do anything on Windows
    // Only QTemporaryFile::~QTemporaryFile() really closes the file.
    QTemporaryFile of;
    of.setFileTemplate(QDir::tempPath() + "\\" + pattern);
    of.setAutoRemove(false);
    if (of.open()) {
        result = WPMUtils::normalizePath(of.fileName(), false);
    }

    return result;
}

void WPMUtils::unzip(Job* job, const QString &zipfile, const QString &outputdir)
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
        if (!odir.endsWith('\\') && !odir.endsWith('/'))
            odir.append('\\');

        job->setTitle(initialTitle + QStringLiteral(" / ") +
                QObject::tr("Extracting"));
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
                job->setTitle(initialTitle + QStringLiteral(" / ") +
                        QString(QObject::tr("%L1 files")).arg(i));

            if (!job->shouldProceed())
                break;
        }
        zip.close();

        delete[] block;
    }

    job->complete();
}

void WPMUtils::startProcess(const QString &prg, const QString &args)
{
    PROCESS_INFORMATION pinfo;

    STARTUPINFOW startupInfo = {
        sizeof(STARTUPINFO), nullptr, nullptr, nullptr,
        static_cast<DWORD>(CW_USEDEFAULT),
        static_cast<DWORD>(CW_USEDEFAULT),
        static_cast<DWORD>(CW_USEDEFAULT),
        static_cast<DWORD>(CW_USEDEFAULT),
        0, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr
    };

    bool success = CreateProcess(
            WPMUtils::toLPWSTR(prg),
            WPMUtils::toLPWSTR(args),
            nullptr, nullptr, TRUE,
            0 /*CREATE_UNICODE_ENVIRONMENT*/, nullptr,
            nullptr, &startupInfo, &pinfo);

    if (success) {
        CloseHandle(pinfo.hThread);
        CloseHandle(pinfo.hProcess);
    }
}

std::function<void ()> WPMUtils::wrapCoInitialize(const std::function<void ()> f)
{
    return [f]() {
        CoInitialize(nullptr);
        f();
        CoUninitialize();
    };
}

QString WPMUtils::toQString(const std::vector<int> &v)
{
    QString r;
    for (auto item: v) {
        if (r.length() > 0)
            r.append(", ");
        r.append(QString::number(item));
    }

    return r;
}

void WPMUtils::executeBatchFile(Job* job, const QString& where,
        const QString& path,
        const QString& outputFile, const std::vector<QString>& env,
        bool printScriptOutput, bool unicode)
{
    QString exe = WPMUtils::findCmdExe();
    QString params = "/E:ON /V:OFF /C \"\"" +
            normalizePath(path, false) +
            "\"\"";
    if (unicode)
        params = "/U " + params;

    executeFile(job, normalizePath(where), exe,
            params,
            outputFile, env, unicode, printScriptOutput, unicode);
}

void WPMUtils::reportEvent(const QString &msg, WORD wType)
{
    if (hEventLog == nullptr) {
        hEventLog = RegisterEventSource(nullptr, L"Npackd");
    }

    if (hEventLog) {
        // qCDebug(npackd) << "ReportEvent";
        LPCWSTR strings[1];
        strings[0] = WPMUtils::toLPWSTR(msg);

        ReportEvent(hEventLog, wType, 0, 1, nullptr,
                1, 0, strings,
                nullptr);
    }
}

void WPMUtils::executeFile(Job* job, const QString& where,
        const QString& path, const QString& nativeArguments,
        const QString& outputFile, const std::vector<QString>& env,
        bool writeUTF16LEBOM, bool printScriptOutput, bool unicode)
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
                &f, env, printScriptOutput, unicode);

        // ignore possible errors here
        f.close();
    } else {
        executeFile(job, where, path, nativeArguments,
                0, env, printScriptOutput, unicode);
    }
}

std::unordered_map<QString, QString> WPMUtils::parseEnv(LPWCH env2)
{
    std::unordered_map<QString, QString> env_;
    LPWCH e = env2;
    while (true) {
        int len = (int)wcslen(e);
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
        env_[name] = value;

        e += len + 1;

        // qCDebug(npackd) << name << value;
    }

    return env_;
}

QByteArray WPMUtils::serializeEnv(const std::unordered_map<QString, QString>& env)
{
    QByteArray ba;

    for (auto i = env.begin(); i != env.end(); ++i) {
        QString name = i->first;
        QString value = i->second;
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

                    //qCDebug(npackd) << base << u;

                    *url = base.resolved(u).toString(QUrl::FullyEncoded);

                    // qCDebug(npackd) << *url;
                }
            } else if (u.scheme() != "http" && u.scheme() != "https" &&
                    u.scheme() != "file") {
                r = QObject::tr("Unsupported URL scheme");
            }
        }
    }

    return r;
}

std::vector<QString> WPMUtils::split(const QString& s, const QString &sep,
    Qt::SplitBehavior behavior)
{
    QStringList sl = s.split(sep, behavior);
    std::vector<QString> r;
    r.reserve(sl.size());
    r.insert(r.end(), sl.begin(), sl.end());
    return r;
}

std::vector<QString> WPMUtils::split(const QString& s, const QChar sep,
    Qt::SplitBehavior behavior)
{
    QStringList sl = s.split(sep, behavior);
    std::vector<QString> r;
    r.reserve(sl.size());
    r.insert(r.end(), sl.begin(), sl.end());
    return r;
}

void WPMUtils::executeFile(Job* job, const QString& where,
        const QString& path, const QString& nativeArguments,
        QIODevice* outputFile, const std::vector<QString>& env,
        bool printScriptOutput, bool unicode, bool wait)

{
    qCDebug(npackd) << where << path << nativeArguments;

    QString initialTitle = job->getTitle();

    time_t start = time(nullptr);

    LPWCH env2 = GetEnvironmentStrings();
    std::unordered_map<QString, QString> env_ = parseEnv(env2);
    FreeEnvironmentStrings(env2);

    for (int i = 0; i + 1 < static_cast<int>(env.size()); i += 2) {
        env_[env.at(i)] = env.at(i + 1);
    }

    QByteArray ba = serializeEnv(env_);

    QString err;

    // qCDebug(npackd) << ba;

    PROCESS_INFORMATION pinfo;

    SECURITY_ATTRIBUTES saAttr = {};
    saAttr.nLength = sizeof(saAttr);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE g_hChildStd_IN_Rd = nullptr;
    HANDLE g_hChildStd_IN_Wr = nullptr;
    HANDLE g_hChildStd_OUT_Rd = INVALID_HANDLE_VALUE;
    HANDLE g_hChildStd_OUT_Wr = nullptr;

    QString name = QStringLiteral("\\\\.\\Pipe\\NpackdExecute.%1.%2").arg(
            GetCurrentProcessId()).arg(
            nextNamePipeId.fetchAndAddOrdered(1));

    if (job->shouldProceed()) {
        if (wait) {
            // the output buffer is much bigger so that processes that
            // output a lot of data do not slow down the execution
            g_hChildStd_OUT_Rd = CreateNamedPipe(
                    WPMUtils::toLPWSTR(name),
                    PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                    PIPE_TYPE_BYTE | PIPE_WAIT, 1,
                    128, 512 * 1024, 1000, &saAttr);
            job->checkOSCall(g_hChildStd_OUT_Rd != INVALID_HANDLE_VALUE);
        }
    }

    if (job->shouldProceed()) {
        if (wait) {
            g_hChildStd_OUT_Wr = CreateFileW(
                    WPMUtils::toLPWSTR(name),
                    GENERIC_WRITE,
                    0,
                    &saAttr,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    nullptr);
            job->checkOSCall(g_hChildStd_OUT_Wr != INVALID_HANDLE_VALUE);
        }
    }

    if (job->shouldProceed()) {
        if (wait) {
            job->checkOSCall(
                    SetHandleInformation(g_hChildStd_OUT_Rd,
                    HANDLE_FLAG_INHERIT, 0));
        }
    }

    if (job->shouldProceed()) {
        if (wait) {
            job->checkOSCall(
                    CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0));
        }
    }

    if (job->shouldProceed()) {
        if (wait) {
            job->checkOSCall(
                    SetHandleInformation(g_hChildStd_IN_Wr,
                    HANDLE_FLAG_INHERIT, 0));
        }
    }

    bool success = false;

    // create the process
    if (job->shouldProceed()) {
        STARTUPINFOW startupInfo = {
            sizeof(STARTUPINFO), nullptr, nullptr, nullptr,
            (ulong) CW_USEDEFAULT, (ulong) CW_USEDEFAULT,
            (ulong) CW_USEDEFAULT, (ulong) CW_USEDEFAULT,
            0, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr
        };

        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
        startupInfo.hStdInput = g_hChildStd_IN_Rd;
        startupInfo.hStdOutput = g_hChildStd_OUT_Wr;
        startupInfo.hStdError = g_hChildStd_OUT_Wr;

        QString args = '\"' + path + '\"';
        if (!nativeArguments.isEmpty())
            args = args + ' ' + nativeArguments;
        success = CreateProcess(
                WPMUtils::toLPWSTR(path),
                WPMUtils::toLPWSTR(args),
                nullptr, &saAttr, TRUE,
                CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                ba.data(),
                WPMUtils::toLPWSTR(where),
                &startupInfo, &pinfo);

        job->checkOSCall(success);

        // ignore the possible errors here
        CloseHandle(g_hChildStd_OUT_Wr);
    }

    // try to assign the process to a job
    HANDLE job_ = INVALID_HANDLE_VALUE;
    if (job->shouldProceed()) {
        BOOL r;
        if (IsProcessInJob(pinfo.hProcess, nullptr, &r) && !r) {
            job_ = CreateJobObject(nullptr, nullptr);
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
    if (job->shouldProceed() && wait) {
        OVERLAPPED stOverlapped = {};
        HANDLE hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        stOverlapped.hEvent = hEvent;

        int64_t outputLen = 0;
        DWORD ec = 0;
        const int BUFSIZE = 512 * 1024;
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
                if (consoleOutput) {
                    if (unicode)
                        WriteConsoleW(hStdout, chBuf, dwRead / 2, &dwWritten, nullptr);
                    else
                        WriteConsoleA(hStdout, chBuf, dwRead, &dwWritten, nullptr);
                } else {
                    // convert to UTF-8
                    QString s;

                    if (unicode)
                        s.setUtf16((const ushort*) chBuf, dwRead / 2);
                    else
                        s = QString::fromLocal8Bit(chBuf, dwRead);

                    QByteArray ba = s.toUtf8();

                    WriteFile(hStdout, ba.constData(),
                            static_cast<DWORD>(ba.length()),
                            &dwWritten, nullptr);
                }
            }

            if (outputFile) {
                if (outputFile->write(chBuf, dwRead) == -1) {
                    job->setErrorMessage(outputFile->errorString());
                    break;
                }
            }

            time_t seconds = time(nullptr) - start;
            double percents = ((double) seconds) / 300; // 5 Minutes
            if (percents > 0.9)
                percents = 0.9;
            job->setProgress(percents);
            job->setTitle(initialTitle + " / " +
                    QString(QObject::tr("%1 minutes")).
                    arg(seconds / 60));
        }
        delete[] chBuf;

        if (hEvent != nullptr)
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

QString WPMUtils::stopService(const QString& serviceName,
        std::vector<QString>* stoppedServices)
{
    QString err;

    // Get a handle to the SCM database.
    SC_HANDLE schSCManager = OpenSCManager(
            nullptr,                    // local computer
            nullptr,                    // ServicesActive database
            SC_MANAGER_ALL_ACCESS);  // full access rights

    if (nullptr == schSCManager) {
        formatMessage(GetLastError(), &err);
        err = QObject::tr("OpenSCManager failed: %0").arg(err);
    }

    if (err.isEmpty()) {
        err = DoStopSvc(schSCManager, serviceName, stoppedServices);
    }

    if (schSCManager)
        CloseServiceHandle(schSCManager);

    return err;
}

QString WPMUtils::DoStopSvc(SC_HANDLE schSCManager, const QString& serviceName,
        std::vector<QString>* stoppedServices)
{
    QString err;

    SERVICE_STATUS_PROCESS ssp;
    DWORD dwBytesNeeded;

    SC_HANDLE schService = nullptr;
    if (err.isEmpty()) {
        // Get a handle to the service.

        schService = OpenService(schSCManager,         // SCM database
                // name of service
                WPMUtils::toLPWSTR(serviceName),
                SERVICE_ALL_ACCESS);
        if (schService == nullptr) {
            formatMessage(GetLastError(), &err);
            err = QObject::tr("OpenService failed: %1").arg(err);
        }
    }

    // If a stop is pending, wait for it.
    if (err.isEmpty()) {
        err = waitForServiceStatus(schService, "!=", SERVICE_STOP_PENDING);
        err = waitForServiceStatus(schService, "!=", SERVICE_START_PENDING);
        err = waitForServiceStatus(schService, "!=", SERVICE_CONTINUE_PENDING);
        err = waitForServiceStatus(schService, "!=", SERVICE_PAUSE_PENDING);
    }

    // Make sure the service is not already stopped.
    if (err.isEmpty()) {
        if (!QueryServiceStatusEx(schService,
                SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp,
                sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded)) {
            formatMessage(GetLastError(), &err);
            err = QObject::tr("QueryServiceStatusEx failed: %1").arg(err);
        }
    }

    // If the service is running, dependencies must be stopped first.
    if (err.isEmpty() && ssp.dwCurrentState != SERVICE_STOPPED) {
        StopDependentServices(schSCManager, schService, stoppedServices);
    }

    // Send a stop code to the service.
    if (err.isEmpty() && ssp.dwCurrentState != SERVICE_STOPPED) {
        qCInfo(npackd).noquote() << QObject::tr(
                "Sending stop signal to the service %1").arg(serviceName);
        if (ControlService(schService, SERVICE_CONTROL_STOP,
                (LPSERVICE_STATUS) &ssp) == 0) {
            formatMessage(GetLastError(), &err);
            err = QObject::tr("ControlService failed: %1").arg(err);
        } else {
            stoppedServices->push_back(serviceName);
        }
    }

    // Wait for the service to stop.
    if (err.isEmpty()) {
        err = waitForServiceStatus(schService, "==", SERVICE_STOPPED);
    }

    if (schService)
        CloseServiceHandle(schService);

    return err;
}

QString WPMUtils::StopDependentServices(SC_HANDLE schSCManager,
        SC_HANDLE schService,
        std::vector<QString>* stoppedServices)
{
    QString err;

    DWORD dwBytesNeeded;
    DWORD dwCount;

    LPENUM_SERVICE_STATUS   lpDependencies = nullptr;

    // Pass a zero-length buffer to get the required buffer size.
    if (EnumDependentServices(schService, SERVICE_ACTIVE,
         lpDependencies, 0, &dwBytesNeeded, &dwCount)) {
         // If the Enum call succeeds, then there are no dependent
         // services, so do nothing.
    } else {
        DWORD e = GetLastError();

        if (GetLastError() != ERROR_MORE_DATA) {
            formatMessage(e, &err);
        }

        // Allocate a buffer for the dependencies.
        lpDependencies = (LPENUM_SERVICE_STATUS) HeapAlloc(
                GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytesNeeded);

        // Enumerate the dependencies.
        if (!EnumDependentServices(schService, SERVICE_ACTIVE,
                lpDependencies, dwBytesNeeded, &dwBytesNeeded,
                &dwCount)) {
            formatMessage(GetLastError(), &err);
        }
    }

    if (err.isEmpty() && lpDependencies) {
        for (DWORD i = 0; i < dwCount; i++ ) {
            ENUM_SERVICE_STATUS ess = *(lpDependencies + i);

            QString name = QString::fromWCharArray(ess.lpServiceName);

            // ignore the error
            DoStopSvc(schSCManager, name, stoppedServices);
        }
    }

    // Always free the enumeration buffer.
    HeapFree(GetProcessHeap(), 0, lpDependencies);

    return err;
}

QString WPMUtils::waitForServiceStatus(SC_HANDLE schService,
        const QString& operation,
        DWORD status)
{
    QString err;

    SERVICE_STATUS_PROCESS ssStatus;
    DWORD dwWaitTime;
    DWORD dwBytesNeeded;

    if (err.isEmpty()) {
        if (!QueryServiceStatusEx(
                schService,                     // handle to service
                SC_STATUS_PROCESS_INFO,         // information level
                (LPBYTE) &ssStatus,             // address of structure
                sizeof(SERVICE_STATUS_PROCESS), // size of structure
                &dwBytesNeeded)) {              // size needed if buffer is too small
            formatMessage(GetLastError(), &err);
            err = QObject::tr("QueryServiceStatusEx failed: %1").arg(err);
        }
    }

    if (err.isEmpty()) {
        DWORD dwStartTickCount = GetTickCount();
        DWORD dwOldCheckPoint = ssStatus.dwCheckPoint;

        while (true) {
            if (operation == "==") {
                if (ssStatus.dwCurrentState == status)
                    break;
            } else {
                if (ssStatus.dwCurrentState != status)
                    break;
            }

            // Do not wait longer than the wait hint. A good interval is
            // one-tenth of the wait hint but not less than 1 second
            // and not more than 10 seconds.

            dwWaitTime = ssStatus.dwWaitHint / 10;

            if (dwWaitTime < 1000)
                dwWaitTime = 1000;
            else if (dwWaitTime > 10000)
                dwWaitTime = 10000;

            Sleep(dwWaitTime);

            // Check the status until the service is no longer stop pending.
            if (!QueryServiceStatusEx(
                    schService,                     // handle to service
                    SC_STATUS_PROCESS_INFO,         // information level
                    (LPBYTE) &ssStatus,             // address of structure
                    sizeof(SERVICE_STATUS_PROCESS), // size of structure
                    &dwBytesNeeded)) {              // size needed if buffer is too small
                formatMessage(GetLastError(), &err);
                err = QObject::tr("QueryServiceStatusEx failed: %1").arg(err);
                break;
            }

            if (ssStatus.dwCheckPoint > dwOldCheckPoint) {
                // Continue to wait and check.
                dwStartTickCount = GetTickCount();
                dwOldCheckPoint = ssStatus.dwCheckPoint;
            } else {
                if (GetTickCount() - dwStartTickCount > 30000) {
                    err = QObject::tr("Timeout waiting for service");
                    break;
                }
            }
        }
    }

    return err;
}

QString WPMUtils::startService(const QString& serviceName)
{
    QString err;

    // Get a handle to the SCM database.
    SC_HANDLE schSCManager = OpenSCManager(
            nullptr,                    // local computer
            nullptr,                    // ServicesActive database
            SC_MANAGER_ALL_ACCESS);  // full access rights

    if (nullptr == schSCManager) {
        formatMessage(GetLastError(), &err);
        err = QObject::tr("OpenSCManager failed: %0").arg(err);
    }

    if (err.isEmpty()) {
        err = startService(schSCManager, serviceName);
    }

    if (schSCManager)
        CloseServiceHandle(schSCManager);

    return err;
}

QString WPMUtils::startService(SC_HANDLE schSCManager,
        const QString& serviceName)
{
    QString err;

    SERVICE_STATUS_PROCESS ssStatus;
    DWORD dwBytesNeeded;

    // Get a handle to the service.

    SC_HANDLE schService = OpenService(
            schSCManager,         // SCM database
            WPMUtils::toLPWSTR(serviceName), // name of service
            SERVICE_ALL_ACCESS);  // full access
    if (schService == nullptr) {
        formatMessage(GetLastError(), &err);
        err = QObject::tr("OpenService failed: %1").arg(err);
    }

    // Check the status in case the service is not stopped.
    if (err.isEmpty()) {
        if (!QueryServiceStatusEx(
                schService,                     // handle to service
                SC_STATUS_PROCESS_INFO,         // information level
                (LPBYTE) &ssStatus,             // address of structure
                sizeof(SERVICE_STATUS_PROCESS), // size of structure
                &dwBytesNeeded ) ) {              // size needed if buffer is too small
            formatMessage(GetLastError(), &err);
            err = QObject::tr("QueryServiceStatusEx failed: %1").arg(err);
        }
    }

    // Check if the service is already running. It would be possible
    // to stop the service here, but for simplicity this example just returns.
    if (err.isEmpty()) {
        if (ssStatus.dwCurrentState != SERVICE_STOPPED &&
                ssStatus.dwCurrentState != SERVICE_STOP_PENDING) {
            err = QObject::tr("Cannot start the service because it is already running");
        }
    }

    // Wait for the service to stop before attempting to start it.
    if (err.isEmpty()) {
        err = waitForServiceStatus(schService, "!=", SERVICE_STOP_PENDING);
    }

    // Attempt to start the service.
    if (err.isEmpty()) {
        qCInfo(npackd).noquote() << QObject::tr(
                "Sending start signal to the service %1").arg(serviceName);
        if (!StartService(
                schService,  // handle to service
                0,           // number of arguments
                nullptr)) {      // no arguments
            formatMessage(GetLastError(), &err);
            err = QObject::tr("StartService failed: %1").arg(err);
        }
    }

    if (err.isEmpty()) {
        err = waitForServiceStatus(schService, "!=", SERVICE_START_PENDING);
    }

    if (err.isEmpty()) {
        if (!QueryServiceStatusEx(
                schService,                     // handle to service
                SC_STATUS_PROCESS_INFO,         // info level
                (LPBYTE) &ssStatus,             // address of structure
                sizeof(SERVICE_STATUS_PROCESS), // size of structure
                &dwBytesNeeded)) {              // if buffer too small
            formatMessage(GetLastError(), &err);
            err = QObject::tr("QueryServiceStatusEx failed %1").arg(err);
        }
    }

    // Determine whether the service is running.
    if (err.isEmpty()) {
        if (ssStatus.dwCurrentState != SERVICE_RUNNING) {
            err = QObject::tr("Service not started. Current status is %1").
                    arg(ssStatus.dwCurrentState);
        }
    }

    if (schService)
        CloseServiceHandle(schService);

    return err;
}

bool WPMUtils::hasAdminPrivileges()
{
    bool fReturn = false;
	const DWORD ACCESS_READ_CONST = 1;
	const DWORD ACCESS_WRITE_CONST = 2;
    HANDLE hToken = nullptr;
    HANDLE hImpersonationToken = nullptr;
    PSID psidAdmin = nullptr;
	SID_IDENTIFIER_AUTHORITY SystemSidAuthority = SECURITY_NT_AUTHORITY;
	GENERIC_MAPPING GenericMapping;
	PRIVILEGE_SET ps;
	DWORD dwStatus;
    PACL pACL = nullptr;
    PSECURITY_DESCRIPTOR psdAdmin = nullptr;

    QString err;

    if (!OpenThreadToken(GetCurrentThread(),
            TOKEN_DUPLICATE | TOKEN_QUERY, TRUE, &hToken)) {
        DWORD e = GetLastError();
        WPMUtils::formatMessage(e, &err);
        if (e == ERROR_NO_TOKEN) {
            err.clear();
            if (!OpenProcessToken(GetCurrentProcess(),
                    TOKEN_DUPLICATE | TOKEN_QUERY, &hToken)) {
                WPMUtils::formatMessage(GetLastError(), &err);
            }
        }
        qCDebug(npackd) << "hasAdminPrivileges.1";
    }

    if (err.isEmpty()) {
        if (!DuplicateToken(hToken, SecurityImpersonation,
                &hImpersonationToken)) {
            WPMUtils::formatMessage(GetLastError(), &err);
        }
        qCDebug(npackd) << "hasAdminPrivileges.2";
    }

    if (err.isEmpty()) {
        if (!AllocateAndInitializeSid(&SystemSidAuthority, 2,
                SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                0, 0, 0, 0, 0, 0, &psidAdmin)) {
            WPMUtils::formatMessage(GetLastError(), &err);
        }
        qCDebug(npackd) << "hasAdminPrivileges.3";
    }

    if (err.isEmpty()) {
        psdAdmin = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);

        if (!InitializeSecurityDescriptor(psdAdmin,
                SECURITY_DESCRIPTOR_REVISION)) {
            WPMUtils::formatMessage(GetLastError(), &err);
        }
        qCDebug(npackd) << "hasAdminPrivileges.4";
    }

    if (err.isEmpty()) {
        DWORD dwACLSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) +
                GetLengthSid(psidAdmin) - sizeof(DWORD);

        pACL = (PACL)LocalAlloc(LPTR, dwACLSize);

        if (!InitializeAcl(pACL, dwACLSize, ACL_REVISION2)) {
            WPMUtils::formatMessage(GetLastError(), &err);
        }
        qCDebug(npackd) << "hasAdminPrivileges.5";
    }

    if (err.isEmpty()) {
        DWORD dwAccessMask = ACCESS_READ_CONST | ACCESS_WRITE_CONST;

        if (!AddAccessAllowedAce(pACL, ACL_REVISION2, dwAccessMask,
                psidAdmin)) {
            WPMUtils::formatMessage(GetLastError(), &err);
        }
        qCDebug(npackd) << "hasAdminPrivileges.6";
    }

    if (err.isEmpty()) {
        if (!SetSecurityDescriptorDacl(psdAdmin, TRUE, pACL, FALSE)) {
            WPMUtils::formatMessage(GetLastError(), &err);
        }
        qCDebug(npackd) << "hasAdminPrivileges.7";
    }

    if (err.isEmpty()) {
        SetSecurityDescriptorGroup(psdAdmin, psidAdmin, FALSE);
        SetSecurityDescriptorOwner(psdAdmin, psidAdmin, FALSE);

        if (!IsValidSecurityDescriptor(psdAdmin)) {
            WPMUtils::formatMessage(GetLastError(), &err);
        }
        qCDebug(npackd) << "hasAdminPrivileges.8";
    }

    if (err.isEmpty()) {
        DWORD dwAccessDesired = ACCESS_READ_CONST;
        GenericMapping.GenericRead = ACCESS_READ_CONST;
        GenericMapping.GenericWrite = ACCESS_WRITE_CONST;
        GenericMapping.GenericExecute = 0;
        GenericMapping.GenericAll = ACCESS_READ_CONST | ACCESS_WRITE_CONST;
        DWORD dwStructureSize = sizeof(PRIVILEGE_SET);
        BOOL val;
        if (AccessCheck(psdAdmin, hImpersonationToken, dwAccessDesired,
                &GenericMapping, &ps, &dwStructureSize, &dwStatus, &val)) {
            fReturn = val == TRUE;
        }
        qCDebug(npackd) << "hasAdminPrivileges.9";
    }

    if (pACL)
        LocalFree(pACL);
    if (psdAdmin)
        LocalFree(psdAdmin);
    if (psidAdmin)
        FreeSid(psidAdmin);
    if (hImpersonationToken)
        CloseHandle(hImpersonationToken);
    if (hToken)
        CloseHandle(hToken);

    //adminMode = fReturn;

    qCDebug(npackd) << "hasAdminPrivileges" << fReturn;

    return fReturn;
}

