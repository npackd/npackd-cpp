#ifndef GUI_H
#define GUI_H

#include <windows.h>

#include <QString>

/** application instance */
extern HINSTANCE hInst;

/** default font */
extern HFONT defaultFont;

/**
 * @brief initializes the GUI
 */
void t_gui_init();

/**
 * @brief retrives the window text
 *
 * @param wnd a window
 * @return text
 */
QString t_gui_get_window_text(HWND wnd);

/**
 * @brief creates a button
 *
 * @param hParent parent
 * @param szCaption title
 * @return handle
 */
HWND t_gui_create_radio_button(HWND hParent, const TCHAR *szCaption);

/**
 * @brief creates a one-line edit field
 *
 * @param hParent parent window
 * @param id Id of the control
 * @return handle
 */
HWND t_gui_create_edit(HWND hParent, int id=0);

/**
 * @brief creates a combobox
 *
 * @param hParent parent window
 * @return handle
 */
HWND t_gui_create_combobox(HWND hParent);

/**
 * @brief determines the preferred size of a control. This works for
 * WC_BUTTON and WC_STATIC
 *
 * @param window a window
 * @return preferred size
 */
SIZE t_gui_get_preferred_size(HWND window);

/**
 * @brief Message handler for about box.
 * @param hDlg dialog
 * @param message message
 * @param wParam first parameter
 * @param lParam second parameter
 * @return result
 */
INT_PTR CALLBACK about(HWND, UINT, WPARAM, LPARAM);

/**
 * @brief appends a new menu item
 * @param menu menu handle
 * @param id command Id
 * @param title title
 */
void t_gui_menu_append_item(HMENU menu, UINT_PTR id, const QString& title);

/**
 * @brief creates a label.
 *
 * @param parent parent control
 * @param title caption
 * @return the handle to the static control
 */
HWND t_gui_create_label(HWND parent, const LPCWSTR title);

/**
 * @brief shows a critical error message
 *
 * @param hwnd parent window or 0
 * @param title window title
 * @param message message
 */
void t_gui_critical_error(HWND hwnd, const QString& title, const QString& message);

#endif // GUI_H
