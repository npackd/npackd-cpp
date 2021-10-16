#ifndef GUI_H
#define GUI_H

#include <windows.h>
#include <commctrl.h>

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
 * @brief appends a new menu item
 * @param menu menu handle
 * @param id command Id
 * @param title title
 * @param bitmap the bitmap that should be used or NULL
 */
void t_gui_menu_append_item(HMENU menu, UINT_PTR id, const QString& title, HBITMAP bitmap);

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

/**
 * @brief creates a static control that can be used as a
 * panel to group other controls.
 *
 * @param parent parent control
 * @param title caption
 * @return the handle to the static control
 */
HWND t_gui_create_panel(HWND parent);

/**
 * @brief load a .png file from a resource of type RCDATA
 * @param pName
 * @return the loaded bitmap or NULL
 */
HBITMAP t_gui_load_png_resource(LPCTSTR pName);

/**
 * @brief creates a rebar with one toolbar
 * @param hwndOwner parent of the rebar
 * @param hwndToolbar toolbar
 * @return the handle
 */
HWND t_gui_create_rebar(HWND hwndOwner, HWND hwndToolbar);

/**
 * @brief creates a toolbar
 * @param hWndParent parent window
 * @return the handle
 */
HWND t_gui_create_toolbar(HWND hWndParent);

/**
 * @brief creates an image list from .png files
 * @param cx width of an image
 * @param cy height of an image
 * @param pngs resource names
 * @param count number of images
 * @return handle
 */
HIMAGELIST t_gui_create_image_list(int cx, int cy, LPCWSTR* pngs, int count);

#endif // GUI_H
