#include "gui.h"

#include <commctrl.h>
#include <windowsx.h>

#include "wpmutils.h"

HINSTANCE hInst;

HFONT defaultFont;

void t_gui_init()
{
    // Initialize common controls.
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    defaultFont = CreateFont(13, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
        ANSI_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, TEXT("Tahoma"));
}

HWND t_gui_create_radio_button(HWND hParent, const TCHAR *szCaption)
{
    HWND w = CreateWindow(WC_BUTTON, szCaption,
        WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        0, 0, CW_USEDEFAULT, CW_USEDEFAULT,
        hParent, NULL, hInst, NULL);
    SendMessage(w, WM_SETFONT, (LPARAM)defaultFont, TRUE);
    return w;
}

HWND t_gui_create_edit(HWND hParent, int id)
{
    return CreateWindow(WC_EDIT, NULL,
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
        0, 0, 100, CW_USEDEFAULT,
        hParent, reinterpret_cast<HMENU>(id), hInst, NULL);
}

SIZE t_gui_get_preferred_size(HWND window)
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
            // see
            // https://docs.microsoft.com/de-de/windows/win32/uxguide/vis-layout?redirectedfrom=MSDN#sizingandspacing
            // https://stackoverflow.com/questions/43892043/win32-how-to-calculate-control-sizes-for-a-consistent-look-across-windows-versi
            r.cy = 20;
            r.cx = 200;
        } else if (wcsicmp(WC_COMBOBOX, className) == 0) {
            r.cx = 200;
            r.cy = 25;
        }
    }

    return r;
}

void t_gui_menu_append_item(HMENU menu, UINT_PTR id, const QString& title)
{
    AppendMenu(menu, MF_STRING, id, WPMUtils::toLPWSTR(title));
}

HWND t_gui_create_label(HWND parent, const LPCWSTR title)
{
    HWND w = CreateWindow(WC_STATIC, title,
        WS_CHILD | WS_VISIBLE,
        0, 0, 100, 50,
        parent, NULL, hInst, NULL);
    SendMessage(w, WM_SETFONT, (LPARAM)defaultFont, TRUE);

    return w;
}

HWND t_gui_create_panel(HWND parent)
{
    return CreateWindow(WC_STATIC, L"",
        WS_CHILD | WS_VISIBLE,
        100, 100, 100, 100,
        parent, NULL, hInst, NULL);
}

HWND t_gui_create_combobox(HWND hParent)
{
    HWND w = CreateWindow(WC_COMBOBOX, NULL,
        WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
        0, 0, 100, CW_USEDEFAULT,
        hParent, NULL, hInst, NULL);
    return w;
}


void t_gui_critical_error(HWND hwnd, const QString &title, const QString &message)
{
    MessageBox(hwnd, WPMUtils::toLPWSTR(title), WPMUtils::toLPWSTR(message),
        MB_OK | MB_ICONERROR);
}

QString t_gui_get_window_text(HWND wnd)
{
    QString r;

    int len = GetWindowTextLengthW(wnd);
    if (len) {
        r.resize(len);
        if (!GetWindowTextW(wnd, WPMUtils::toLPWSTR(r), len + 1)) {
            r.clear();
        }
    }

    return r;
}
