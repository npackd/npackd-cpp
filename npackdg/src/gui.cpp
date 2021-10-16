#include "gui.h"

#include <commctrl.h>
#include <windowsx.h>
#include <gdiplus.h>

#include "wpmutils.h"

HINSTANCE hInst;

HFONT defaultFont;

void t_gui_init()
{
    // Initialize common controls.
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES | ICC_COOL_CLASSES | ICC_BAR_CLASSES;
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
    if (GetClassName(window, className, T_GUI_COUNT_OF(className))) {
        if (wcsicmp(WC_BUTTON, className) == 0) {
            Button_GetIdealSize(window, &r);
        } else if (wcsicmp(WC_STATIC, className) == 0) {
            HDC hDC = GetDC(window);
            RECT rect = { 0, 0, 0, 0 };

            int textLen = GetWindowText(window, text, T_GUI_COUNT_OF(text));
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

void t_gui_menu_append_item(HMENU menu, UINT_PTR id, const QString& title, HBITMAP bitmap)
{
    AppendMenu(menu, MF_STRING, id, WPMUtils::toLPWSTR(title));
    SetMenuItemBitmaps(menu, id, MF_BYCOMMAND, bitmap, bitmap);
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

HBITMAP t_gui_load_png_resource(LPCTSTR pName) {
    // https://www.codeproject.com/Articles/3537/Loading-JPG-PNG-resources-using-GDI
    HBITMAP ret = NULL;

    HRSRC h_resource = FindResource(hInst, pName, RT_RCDATA);
    if (h_resource != NULL) {
        DWORD image_size = SizeofResource(hInst, h_resource);
        if (image_size > 0) {
            HGLOBAL res = LoadResource(hInst, h_resource);
            if (res != NULL) {
                const void* p_resource_data = LockResource(res);

                HGLOBAL global_buffer  = GlobalAlloc(GMEM_MOVEABLE, image_size);
                void* buffer = GlobalLock(global_buffer);
                if (buffer) {
                    CopyMemory(buffer, p_resource_data, image_size);

                    IStream* stream = NULL;
                    if (CreateStreamOnHGlobal(global_buffer, FALSE, &stream) == S_OK) {
                        Gdiplus::Bitmap* m_pBitmap = Gdiplus::Bitmap::FromStream(stream);
                        stream->Release();
                        if (m_pBitmap) {
                            if (m_pBitmap->GetLastStatus() == Gdiplus::Ok)
                            m_pBitmap->GetHBITMAP(Gdiplus::Color(), &ret);
                            delete m_pBitmap;
                        }
                    }
                    GlobalUnlock(global_buffer);
                }
                GlobalFree(global_buffer);
            }
        }
    }

    return ret;
}

HWND t_gui_create_toolbar(HWND hWndParent)
{
    // Create the toolbar.
    HWND hWndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
                                      WS_CHILD | TBSTYLE_WRAPABLE |TBSTYLE_FLAT, 0, 0, 0, 0,
                                      hWndParent, NULL, hInst, NULL);

    if (hWndToolbar == NULL)
        return NULL;

    return hWndToolbar;
}

#define NUMBUTTONS 3

HWND t_gui_create_rebar(HWND hwndOwner, HWND hwndToolbar)
{
    // Create the rebar.
    HWND hwndRebar = CreateWindowEx(WS_EX_TOOLWINDOW,
        REBARCLASSNAME,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
        WS_CLIPCHILDREN | RBS_VARHEIGHT |
        CCS_NODIVIDER | RBS_BANDBORDERS,
        0,0,0,0,
        hwndOwner,
        NULL,
        hInst, // global instance handle
        NULL);

    if(!hwndRebar) {
        return NULL;
    }

    // Initialize band info used by both bands.
    REBARBANDINFO rbBand = {  };
    rbBand.cbSize = sizeof(REBARBANDINFO);
    rbBand.fMask  =
          RBBIM_STYLE       // fStyle is valid.
        | RBBIM_TEXT        // lpText is valid.
        | RBBIM_CHILD       // hwndChild is valid.
        | RBBIM_CHILDSIZE   // child size members are valid.
        | RBBIM_SIZE;       // cx is valid
    rbBand.fStyle = RBBS_CHILDEDGE | RBBS_GRIPPERALWAYS;

    // Get the height of the toolbar.
    DWORD dwBtnSize = (DWORD)SendMessage(hwndToolbar, TB_GETBUTTONSIZE, 0,0);

    // Set values unique to the band with the toolbar.
    rbBand.lpText = (LPWSTR) L"";
    rbBand.hwndChild = hwndToolbar;
    rbBand.cyChild = LOWORD(dwBtnSize);
    rbBand.cxMinChild = NUMBUTTONS * HIWORD(dwBtnSize);
    rbBand.cyMinChild = LOWORD(dwBtnSize);
    // The default width is the width of the buttons.
    rbBand.cx = 0;

    // Add the band that has the toolbar.
    SendMessage(hwndRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    return (hwndRebar);
}

HIMAGELIST t_gui_create_image_list(int cx, int cy, LPCWSTR* pngs, int count)
{
    HIMAGELIST images = ImageList_Create(cx, cy, ILC_COLOR32, 0, 0);

    for (int i = 0; i < count; i++) {
        HBITMAP b = t_gui_load_png_resource(pngs[i]);
        ImageList_Add(images, b, NULL);
        DeleteObject(b);
    }

    return images;
}
