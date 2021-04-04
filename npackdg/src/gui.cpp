#include "gui.h"

#include <commctrl.h>
#include <windowsx.h>

#include "wpmutils.h"

MainWindow_t mainWindow;

// current instance
HINSTANCE hInst;

const WCHAR* mainWindowClass = L"Npackdg";

int guiRun(int nCmdShow)
{
    // Initialize common controls.
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    // create main window
    mainWindow.window = createMainWindow(nCmdShow);

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    destroyMainWindow(mainWindow.window);

    UnregisterClassW(mainWindowClass, hInst);

    return (int) msg.wParam;
}

void destroyMainWindow(HWND hWnd)
{
    HMENU mainMenu = GetMenu(hWnd);
    DestroyWindow(hWnd);
    DestroyMenu(mainMenu);
}


HWND createMainWindow(int nCmdShow)
{
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = mainWindowProc;
    wcex.hInstance      = hInst;
    wcex.hIcon          = LoadIcon(hInst, L"IDI_ICON1");
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszClassName  = mainWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, L"IDI_ICON1");
    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(mainWindowClass, L"Npackd", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, hInst, NULL);

    SetMenu(hWnd, createMainMenu());

    mainWindow.tabs = createTab(hWnd);

    mainWindow.packagesPanel = createPackagesPanel(mainWindow.tabs);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return hWnd;
}

HWND createButton(HWND hParent, const TCHAR *szCaption)
{
    return CreateWindow(WC_BUTTON, szCaption,
        WS_VISIBLE | WS_CHILD | BS_RADIOBUTTON,
        0, 0, CW_USEDEFAULT, CW_USEDEFAULT,
        hParent, NULL, hInst, NULL);
}

HWND createEdit(HWND hParent)
{
    return CreateWindow(WC_EDIT, NULL,
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
        0, 0, 100, CW_USEDEFAULT,
        hParent, NULL, hInst, NULL);
}

void layoutPackagesPanel()
{
    RECT r;
    GetClientRect(mainWindow.packagesPanel, &r);

    r.left = 200;
    MoveWindow(mainWindow.table, r.left, r.top,
        r.right - r.left, r.bottom - r.top, FALSE);
}

SIZE getPreferredSize(HWND window)
{
    SIZE r = {.cx = 100, .cy = 100};

    WCHAR className[256];
    WCHAR text[256];
    if (GetClassName(window, className,
        sizeof(className) / sizeof(className[0]))) {
        if (wcsicmp(WC_BUTTON, className) == 0) {
            Button_GetIdealSize(window, &r);
        } else if (wcsicmp(WC_STATIC, className) == 0) {
            HDC hDC = GetDC(window);
            RECT rect = { 0, 0, 0, 0 };

            int textLen = GetWindowText(window, text,
                sizeof(text) / sizeof(text[0]));
            if (textLen) {
                DrawText(hDC, text, textLen, &rect, DT_CALCRECT);
                r.cx = rect.right - rect.left;
                r.cy = rect.bottom - rect.top;
            }
        } else if (wcsicmp(WC_EDIT, className) == 0) {
            HDC hDC = GetDC(window);
            GetTextExtentPoint32W(hDC, L"W", 1, &r);
            r.cx = 200;
        } else if (wcsicmp(WC_COMBOBOX, className) == 0) {
            HDC hDC = GetDC(window);
            GetTextExtentPoint32W(hDC, L"W", 1, &r);
            r.cx = 200;
        }
    }

    return r;
}

HWND createPackagesPanel(HWND parent)
{
    HWND result = createPanel(parent);
    SetWindowSubclass(result, &packagesPanelSubClassProc, 1, 0);

    HWND label = createStatic(result, L"S&earch:");
    SIZE sz = getPreferredSize(label);
    MoveWindow(label, 0, 0, sz.cx, sz.cy, FALSE);
    int y = sz.cy;

    HWND edit = createEdit(result);
    sz = getPreferredSize(edit);
    MoveWindow(edit, 0, y, sz.cx, sz.cy, FALSE);
    y += sz.cy;

    HWND btn = createButton(result, L"&All");
    sz = getPreferredSize(btn);
    MoveWindow(btn, 0, y, sz.cx, sz.cy, FALSE);
    y += sz.cy;

    btn = createButton(result, L"&Installed");
    sz = getPreferredSize(btn);
    MoveWindow(btn, 0, y, sz.cx, sz.cy, FALSE);
    y += sz.cy;

    btn = createButton(result, L"&Updateable");
    sz = getPreferredSize(btn);
    MoveWindow(btn, 0, y, sz.cx, sz.cy, FALSE);
    y += sz.cy;

    label = createStatic(result, L"Category:");
    sz = getPreferredSize(label);
    MoveWindow(label, 0, y, sz.cx, sz.cy, FALSE);
    y += sz.cy;

    HWND combobox = createComboBox(result);
    sz = getPreferredSize(combobox);
    MoveWindow(combobox, 0, y, sz.cx, sz.cy, FALSE);
    y += sz.cy;

    label = createStatic(result, L"Sub-category:");
    sz = getPreferredSize(label);
    MoveWindow(label, 0, y, sz.cx, sz.cy, FALSE);
    y += sz.cy;

    combobox = createComboBox(result);
    sz = getPreferredSize(combobox);
    MoveWindow(combobox, 0, y, sz.cx, sz.cy, FALSE);
    y += sz.cy;

    mainWindow.table = createTable(result);
    LVCOLUMN col = {};
    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    col.fmt = LVCFMT_LEFT;
    col.cx = 100;
    col.pszText = const_cast<LPWSTR>(L"ColumnHeader");
    ListView_InsertColumn(mainWindow.table, 0, &col);
    ListView_SetItemCountEx(mainWindow.table, 1000,
        LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);

    return result;
}

LRESULT CALLBACK packagesPanelSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/)
{
    switch(uMsg)
    {
        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code) {
                case LVN_GETDISPINFO:
                {
                    // provide the data for the packages table
                    NMLVDISPINFO* pnmv = (NMLVDISPINFO*) lParam;
                    if (pnmv->item.iItem < 0 || // typo fixed 11am
                            pnmv->item.iItem >= 1000) {
                        return E_FAIL;         // requesting invalid item
                    }

                    LPWSTR pszResult;
                    if (pnmv->item.mask & LVIF_TEXT) {
                        switch (pnmv->item.iSubItem) {
                        case 0:
                            pszResult = const_cast<LPWSTR>(L"First");
                            break;
                        case 1:
                            pszResult = const_cast<LPWSTR>(L"Second");
                            break;
                        default:
                            pszResult = const_cast<LPWSTR>(L"Other");
                            break;
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
        case WM_SIZE:
        {
            layoutPackagesPanel();
            break;
        }
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK tabSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/)
{
    switch(uMsg)
    {
        case WM_SIZE:
        {
            RECT r = {.left = 0, .top = 0,
                .right = GET_X_LPARAM(lParam),
                .bottom = GET_Y_LPARAM(lParam)};

            TabCtrl_AdjustRect(hWnd, FALSE, &r);

            // Resize the tab control to fit the client are of main window.
            MoveWindow(mainWindow.packagesPanel, r.left, r.top,
                r.right - r.left, r.bottom - r.top, FALSE);

            break;
        }
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK mainWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
        {
            PostQuitMessage(0);
            break;
        }

        case WM_SIZE:
        {
            if (mainWindow.tabs == 0)
                return E_INVALIDARG;

            // Resize the tab control to fit the client are of main window.
            if (!SetWindowPos(mainWindow.tabs, HWND_TOP, 0, 0, GET_X_LPARAM(lParam),
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

HWND createStatic(HWND parent, const LPCWSTR title)
{
    return CreateWindow(WC_STATIC, title,
        WS_CHILD | WS_VISIBLE,
        0, 0, 100, 50,
        parent, NULL, hInst, NULL);
}

HWND createPanel(HWND parent)
{
    return CreateWindow(WC_STATIC, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        100, 100, 100, 100,
        parent, NULL, hInst, NULL);
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

HWND createTab(HWND hwndParent)
{
    RECT rcClient;

    // Get the dimensions of the parent window's client area, and
    // create a tab control child window of that size.
    GetClientRect(hwndParent, &rcClient);

    HWND hwndTab = CreateWindow(WC_TABCONTROL, L"",
        WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
        0, 0, rcClient.right, rcClient.bottom,
        hwndParent, NULL, hInst, NULL);

    // Add tabs for each day of the week.
    TCITEM tie;
    tie.mask = TCIF_TEXT | TCIF_IMAGE;
    tie.iImage = -1;

    tie.pszText = const_cast<LPWSTR>(L"Packages");
    TabCtrl_InsertItem(hwndTab, 0, &tie);

    tie.pszText = const_cast<LPWSTR>(L"Jobs");
    TabCtrl_InsertItem(hwndTab, 1, &tie);

    SetWindowSubclass(hwndTab, &tabSubClassProc, 1, 0);

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


HWND createComboBox(HWND hParent)
{
    return CreateWindow(WC_COMBOBOX, NULL,
        WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
        0, 0, 100, CW_USEDEFAULT,
        hParent, NULL, hInst, NULL);
}


void gui_critical(HWND hwnd, const QString &title, const QString &message)
{
    MessageBox(hwnd, WPMUtils::toLPWSTR(title), WPMUtils::toLPWSTR(message),
        MB_OK | MB_ICONERROR);
}
