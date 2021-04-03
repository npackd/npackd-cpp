#include <windows.h>
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

#define IDM_ABOUT 0x0010
#define IDM_EXIT 0x0011

// Global Variables:
HINSTANCE hInst;                                // current instance
const WCHAR* szWindowClass = L"Npackdg";            // the main window class name

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

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow)) {
        return FALSE;
    }

    //HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_HELLOWINDOWS));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

/**
 * @brief Registers the window class.
 * @param hInstance application instance
 * @return result from the call to RegisterClassExW
 */
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

/**
 * @brief saves instance handle and creates main window. In this function,
 * we save the instance handle in a global variable and
 * create and display the main program window.
 * @param hInstance application instance
 * @param nCmdShow SW_*
 * @return TRUE if the application was successfully initialized
 */
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, L"Npackd", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, hInstance, NULL);

   if (!hWnd) {
      return FALSE;
   }

   SetMenu(hWnd, createMainMenu());

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

/**
 * @brief Processes messages for the main window.
 * WM_COMMAND  - process the application menu
 * WM_PAINT    - Paint the main window
 * WM_DESTROY  - post a quit message and return
 *
 * @param hWnd windows handle
 * @param message message
 * @param wParam first parameter
 * @param lParam second parameter
 * @return result
 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId) {
            case IDM_ABOUT:
                // DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            RECT rect;
            HDC hdc = BeginPaint(hWnd, &ps);

            GetClientRect(hWnd, &rect);
            DrawText(hdc, TEXT("Hello, Windows!"), -1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            EndPaint(hWnd, &ps);
        }
        break;

        case WM_DESTROY:
            PostQuitMessage(0);
        break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

/**
 * @brief Message handler for about box.
 * @param hDlg dialog
 * @param message message
 * @param wParam first parameter
 * @param lParam second parameter
 * @return result
 */
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

HMENU createMainMenu()
{
    HMENU packageMenu = CreateMenu();
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Install");
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Uninstall");
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Update");
    AppendMenu(packageMenu, MF_SEPARATOR, (UINT_PTR) nullptr, nullptr);
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Show details");
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Show changelog");
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Run");
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Open folder");
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Open web site");
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Test download site");
    AppendMenu(packageMenu, MF_SEPARATOR, (UINT_PTR) nullptr, nullptr);
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Check dependencies");
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Reload repositories");
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Add package...");
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Export...");
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Settings");
    AppendMenu(packageMenu, MF_SEPARATOR, (UINT_PTR) nullptr, nullptr);
    AppendMenu(packageMenu, MF_STRING, (UINT_PTR) nullptr,
        WPMUtils::toLPWSTR(QObject::tr("Exit")));

    HMENU viewMenu = CreateMenu();
    AppendMenu(viewMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Close tab");
    AppendMenu(viewMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Choose columns...");
    AppendMenu(viewMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Toggle toolbar");

    HMENU helpMenu = CreateMenu();
    AppendMenu(helpMenu, MF_STRING, (UINT_PTR) nullptr,
        L"Feedback");
    AppendMenu(helpMenu, MF_STRING, (UINT_PTR) nullptr,
        L"About...");

    HMENU hmenuMain = CreateMenu();
    AppendMenu(hmenuMain, MF_STRING | MF_POPUP, (UINT_PTR) packageMenu,
        L"Package");
    AppendMenu(hmenuMain, MF_STRING | MF_POPUP, (UINT_PTR) viewMenu,
        L"View");
    AppendMenu(hmenuMain, MF_STRING | MF_POPUP, (UINT_PTR) helpMenu,
        L"Help");

    return hmenuMain;
}

