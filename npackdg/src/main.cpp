#include <windows.h>
#include <windowsx.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <QApplication>
#include <QMetaType>
#include <QTranslator>
#include <QVBoxLayout>
#include <QImageReader>
#include <QtPlugin>
#include <QStyleFactory>

#include "version.h"
#include "mainwindow.h"
#include "repository.h"
#include "wpmutils.h"
#include "job.h"
#include "fileloader.h"
#include "abstractrepository.h"
#include "dbrepository.h"
#include "installedpackages.h"
#include "commandline.h"
#include "packageversion.h"
#include "installoperation.h"
#include "uiutils.h"
#include "clprocessor.h"
#include "uimessagehandler.h"
#include "main.h"

#define IDM_ABOUT 16
#define IDM_EXIT 17
#define IDM_SETTINGS 18
#define IDM_FEEDBACK 19
#define IDM_CLOSE_TAB 20
#define IDM_CHOOSE_COLUMNS 21
#define IDM_TOGGLE_TOOLBAR 22
#define IDM_INSTALL 23
#define IDM_UNINSTALL 24
#define IDM_UPDATE 25
#define IDM_SHOW_DETAILS 26
#define IDM_SHOW_CHANGELOG 27
#define IDM_RUN 28
#define IDM_OPEN_FOLDER 29
#define IDM_OPEN_WEB_SITE 30
#define IDM_TEST_DOWNLOAD_SITE 31
#define IDM_CHECK_DEPENDENCIES 32
#define IDM_RELOAD_REPOSITORIES 33
#define IDM_ADD_PACKAGE 34
#define IDM_EXPORT 35

#define DAYS_IN_WEEK 7

// Global Variables:
HINSTANCE hInst;                                // current instance
const WCHAR* szWindowClass = L"Npackdg";            // the main window class name
HWND hwndTab = 0;

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    //qCDebug(npackd) << QUrl("file:///C:/test").resolved(QUrl::fromLocalFile("abc.txt"));

    qInstallMessageHandler(eventLogMessageHandler);

    HMODULE m = LoadLibrary(L"exchndl.dll");

    QLoggingCategory::setFilterRules("npackd=true\nnpackd.debug=false");

#if NPACKD_ADMIN != 1
    WPMUtils::hasAdminPrivileges();
#endif

    // this does not work unfortunately. The only way to ensure the maximum width of a tooltip is to
    // use HTML
    // a.setStyleSheet("QToolTip { height: auto; max-width: 400px; color: #ffffff; background-color: #2a82da; border: 1px solid white; }");

    QString packageName;
#if !defined(__x86_64__) && !defined(_WIN64)
    packageName = "com.googlecode.windows-package-manager.Npackd";
#else
    packageName = "com.googlecode.windows-package-manager.Npackd64";
#endif
    InstalledPackages::packageName = packageName;

    qRegisterMetaType<Version>("Version");
    qRegisterMetaType<int64_t>("int64_t");
    qRegisterMetaType<QMessageBox::Icon>("QMessageBox::Icon");

#if !defined(__x86_64__) && !defined(_WIN64)
    // TODO: use Windows MessageDlg
    if (WPMUtils::is64BitWindows()) {
        QMessageBox::critical(0, "Error",
                QObject::tr("The 32 bit version of Npackd requires a 32 bit operating system.") + "\r\n" +
                QObject::tr("Please download the 64 bit version from https://github.com/tim-lebedkov/npackd/wiki/Downloads"));
        return 1;
    }
#endif


    // July, 25 2018:
    // "bmp", "cur", "gif", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "xbm", "xpm"
    // December, 25 2018:
    // "bmp", "cur", "gif", "icns", "ico", "jp2", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
    // July, 27 2019:
    // "bmp", "cur", "gif", "icns", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
    // September, 26 2020:
    // "bmp", "cur", "gif", "icns", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "svg", "svgz", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
    qCDebug(npackd) << QImageReader::supportedImageFormats();

    // July, 25 2018: "windowsvista", "Windows", "Fusion"
    qCDebug(npackd) << QStyleFactory::keys();

    CLProcessor clp;

    // int errorCode = 0; // TODO


    // TODO: clp.process(argc, argv, &errorCode);

    //WPMUtils::timer.dump();

    FreeLibrary(m);

    // TODO: return errorCode;

    // TODO: Place code here.

    // Initialize global strings
    MyRegisterClass(hInstance);

    // Initialize common controls.
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    hInst = hInstance; // Store instance handle in our global variable

    // Perform application initialization:
    HWND hWnd = createMainWindow(nCmdShow);

    //HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_HELLOWINDOWS));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    destroyMainWindow(hWnd);

    UnregisterClassW(szWindowClass, hInstance);

    return (int) msg.wParam;
}

void destroyMainWindow(HWND hWnd)
{
    HMENU mainMenu = GetMenu(hWnd);
    DestroyWindow(hWnd);
    DestroyMenu(mainMenu);
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = {};

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, L"IDI_ICON1");
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, L"IDI_ICON1");

    return RegisterClassExW(&wcex);
}

HWND createMainWindow(int nCmdShow)
{
   HWND hWnd = CreateWindowW(szWindowClass, L"Npackd", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, hInst, NULL);

   if (!hWnd) {
      return FALSE;
   }

   SetMenu(hWnd, createMainMenu());

   hwndTab = createTabControl(hWnd);

   HWND table = createTable(hwndTab);
   LVCOLUMN col = {};
   col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
   col.fmt = LVCFMT_LEFT;
   col.cx = 100;
   col.pszText = const_cast<LPWSTR>(L"ColumnHeader");
   ListView_InsertColumn(table, 0, &col);
   ListView_SetItemCountEx(table, 1000, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return hWnd;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId) {
            case IDM_ABOUT:
                MessageBox(hWnd, L"hello", L"Caption", MB_OK);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            RECT rect;
            HDC hdc = BeginPaint(hWnd, &ps);

            GetClientRect(hWnd, &rect);
            DrawText(hdc, TEXT("Hello, Windows!"), -1, &rect, DT_SINGLELINE |
                DT_CENTER | DT_VCENTER);
            EndPaint(hWnd, &ps);
        }
        break;

        case WM_DESTROY:
            PostQuitMessage(0);
        break;

        case WM_SIZE:
        {
            if (hwndTab == 0)
                return E_INVALIDARG;

            // Resize the tab control to fit the client are of main window.
            if (!SetWindowPos(hwndTab, HWND_TOP, 0, 0, GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam), SWP_SHOWWINDOW))
                return E_FAIL;

            break;
        }

        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code) {
                case TCN_SELCHANGING:
                {
                    // Return FALSE to allow the selection to change.
                    return FALSE;
                }

                case TCN_SELCHANGE:
                {
                    // int iPage = TabCtrl_GetCurSel(hwndTab);

                    // Note that g_hInst is the global instance handle.
                    MessageBox(hWnd, L"changed", L"Caption", MB_OK);
                    /*SendMessage(hwndDisplay, WM_SETTEXT, 0,
                        (LPARAM) L"test!!!");*/
                    break;
                }

                case LVN_GETDISPINFO:
                {
                    NMLVDISPINFO* pnmv = (NMLVDISPINFO*) lParam;
                    if (pnmv->item.iItem < 0 || // typo fixed 11am
                            pnmv->item.iItem >= 1000) {
                        return E_FAIL;         // requesting invalid item
                    }

                    LPWSTR pszResult;
                    if (pnmv->item.mask & LVIF_TEXT) {
                        switch (pnmv->item.iSubItem) {
                        case 0:    pszResult = L"First";    break;
                        case 1:  pszResult = L"Second";  break;
                        default: pszResult = L"Other"; break;
                        }
                        pnmv->item.pszText = const_cast<LPWSTR>(pszResult);
                    }
                    if (pnmv->item.mask & LVIF_IMAGE) {
                        pnmv->item.iImage = -1;
                    }
                    if (pnmv->item.mask & LVIF_STATE) {
                        pnmv->item.state = 0;
                    }
                    break;
                }
            }
            break;
        }

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}

void appendMenuItem(HMENU menu, UINT_PTR id, const QString& title)
{
    AppendMenu(menu, MF_STRING, id, WPMUtils::toLPWSTR(title));
}

HWND createStatic(HWND parent)
{
    HWND hwndStatic = CreateWindow(WC_STATIC, L"Text!",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        100, 100, 100, 100,        // Position and dimensions; example only.
        parent, NULL, hInst,    // hInst is the global instance handle
        NULL);
    return hwndStatic;
}

HMENU createMainMenu()
{
    HMENU packageMenu = CreateMenu();
    appendMenuItem(packageMenu, IDM_INSTALL, QObject::tr("&Install"));
    appendMenuItem(packageMenu, IDM_UNINSTALL, QObject::tr("U&ninstall"));
    appendMenuItem(packageMenu, IDM_UPDATE, QObject::tr("&Update"));
    AppendMenu(packageMenu, MF_SEPARATOR, (UINT_PTR) nullptr, nullptr);
    appendMenuItem(packageMenu, IDM_SHOW_DETAILS, QObject::tr("Show details"));
    appendMenuItem(packageMenu, IDM_SHOW_CHANGELOG,
        QObject::tr("Show changelog"));
    appendMenuItem(packageMenu, IDM_RUN, QObject::tr("Run"));
    appendMenuItem(packageMenu, IDM_OPEN_FOLDER, QObject::tr("Open folder"));
    appendMenuItem(packageMenu, IDM_OPEN_WEB_SITE, QObject::tr("&Open web site"));
    appendMenuItem(packageMenu, IDM_TEST_DOWNLOAD_SITE,
        QObject::tr("&Test download site"));
    AppendMenu(packageMenu, MF_SEPARATOR, (UINT_PTR) nullptr, nullptr);
    appendMenuItem(packageMenu, IDM_CHECK_DEPENDENCIES,
        QObject::tr("Check dependencies"));
    appendMenuItem(packageMenu, IDM_RELOAD_REPOSITORIES,
        QObject::tr("Reload repositories"));
    appendMenuItem(packageMenu, IDM_ADD_PACKAGE, QObject::tr("Add package..."));
    appendMenuItem(packageMenu, IDM_EXPORT, QObject::tr("Export..."));
    appendMenuItem(packageMenu, IDM_SETTINGS, QObject::tr("&Settings"));
    AppendMenu(packageMenu, MF_SEPARATOR, (UINT_PTR) nullptr, nullptr);
    appendMenuItem(packageMenu, IDM_EXIT, QObject::tr("&Exit"));

    HMENU viewMenu = CreateMenu();
    appendMenuItem(viewMenu, IDM_CLOSE_TAB, QObject::tr("Close tab"));
    appendMenuItem(viewMenu, IDM_CHOOSE_COLUMNS,
        QObject::tr("Choose columns..."));
    appendMenuItem(viewMenu, IDM_TOGGLE_TOOLBAR, QObject::tr("Toggle toolbar"));

    HMENU helpMenu = CreateMenu();
    AppendMenu(helpMenu, MF_STRING, IDM_FEEDBACK, L"Feedback");
    AppendMenu(helpMenu, MF_STRING, IDM_ABOUT, L"About");

    HMENU hmenuMain = CreateMenu();
    AppendMenu(hmenuMain, MF_STRING | MF_POPUP, (UINT_PTR) packageMenu,
        WPMUtils::toLPWSTR(QObject::tr("Package")));
    AppendMenu(hmenuMain, MF_STRING | MF_POPUP, (UINT_PTR) viewMenu,
        WPMUtils::toLPWSTR(QObject::tr("View")));
    AppendMenu(hmenuMain, MF_STRING | MF_POPUP, (UINT_PTR) helpMenu,
        WPMUtils::toLPWSTR(QObject::tr("Help")));

    return hmenuMain;
}

HWND createTabControl(HWND hwndParent)
{
    RECT rcClient;
    HWND hwndTab;
    TCITEM tie;

    // Get the dimensions of the parent window's client area, and
    // create a tab control child window of that size. Note that g_hInst
    // is the global instance handle.
    GetClientRect(hwndParent, &rcClient);
    hwndTab = CreateWindow(WC_TABCONTROL, L"",
        WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
        0, 0, rcClient.right, rcClient.bottom,
        hwndParent, NULL, hInst, NULL);
    if (hwndTab == NULL) {
        return NULL;
    }

    // Add tabs for each day of the week.
    tie.mask = TCIF_TEXT | TCIF_IMAGE;
    tie.iImage = -1;

    tie.pszText = const_cast<LPWSTR>(L"Packages");
    TabCtrl_InsertItem(hwndTab, 0, &tie);

    tie.pszText = const_cast<LPWSTR>(L"Jobs");
    TabCtrl_InsertItem(hwndTab, 1, &tie);

    return hwndTab;
}

HWND createTable(HWND parent)
{
    return CreateWindow(WC_LISTVIEW, NULL,
                  WS_VISIBLE | WS_CHILD | WS_TABSTOP |
                  LVS_NOSORTHEADER | LVS_OWNERDATA |
                  LVS_SINGLESEL | LVS_REPORT,
                  200, 25, 200, 200,
                  parent,
                  0,
                  hInst,
                  NULL);
}
