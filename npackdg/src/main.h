#ifndef MAIN_H
#define MAIN_H

#include <windows.h>

#include <QString>

/**
 * Main window.
 */
typedef struct MainWindow_t {
    /** handle */
    HWND window;

    /** tab control */
    HWND tabs;

    /** panel for packages */
    HWND packagesPanel;

    /** table with packages */
    HWND table;
} MainWindow_t;

/**
 * @brief Registers the window class.
 * @param hInstance application instance
 * @return result from the call to RegisterClassExW
 */
ATOM MyRegisterClass(HINSTANCE hInstance);

/**
 * @brief saves instance handle and creates main window. In this function,
 * we save the instance handle in a global variable and
 * create and display the main program window.
 *
 * @param nCmdShow SW_*
 * @return main window handle
 */
HWND createMainWindow(int);

/**
 * @brief creates a button
 *
 * @param hParent parent
 * @param szCaption title
 * @return handle
 */
HWND createButton(HWND hParent, const TCHAR *szCaption);

/**
 * @brief creates a one-line edit field
 *
 * @param hParent parent window
 * @return handle
 */
HWND createEdit(HWND hParent);

/**
 * @brief creates a combobox
 *
 * @param hParent parent window
 * @return handle
 */
HWND createComboBox(HWND hParent);

/**
 * @brief layout the controls on the packages page
 */
void layoutPackagesPanel();

/**
 * @brief determines the preferred size of a control. This works for
 * WC_BUTTON and WC_STATIC
 *
 * @param window a window
 * @return preferred size
 */
SIZE getPreferredSize(HWND window);

/**
 * @brief create the panel with packages table and all the fiters
 *
 * @param parent parent window
 * @return handle
 */
HWND createPackagesPanel(HWND parent);

/**
 * @brief processes messages in the packages panel
 *
 * @param hWnd window
 * @param uMsg message
 * @param wParam first parameter
 * @param lParam second parameter
 * @param uIdSubclass Id of the Subclass procedure
 * @param dwRefData reference data
 * @return result
 */
LRESULT CALLBACK packagesPanelSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

/**
 * @brief processes messages in the tab control
 *
 * @param hWnd window
 * @param uMsg message
 * @param wParam first parameter
 * @param lParam second parameter
 * @param uIdSubclass Id of the Subclass procedure
 * @param dwRefData reference data
 * @return result
 */
LRESULT CALLBACK tabSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

/**
 * @brief Processes messages for the main window.
 *
 * @param hWnd windows handle
 * @param message message
 * @param wParam first parameter
 * @param lParam second parameter
 * @return result
 */
LRESULT CALLBACK mainWindowProc(HWND, UINT, WPARAM, LPARAM);

/**
 * @brief Message handler for about box.
 * @param hDlg dialog
 * @param message message
 * @param wParam first parameter
 * @param lParam second parameter
 * @return result
 */
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

/**
 * @brief appends a new menu item
 * @param menu menu handle
 * @param id command Id
 * @param title title
 */
void appendMenuItem(HMENU menu, UINT_PTR id, const QString& title);

/**
 * @brief creates a label.
 *
 * @param parent parent control
 * @param title caption
 * @return the handle to the static control
 */
HWND createStatic(HWND parent, const LPCWSTR title);

/**
 * @brief creates a static control that can be used as a
 * panel to group other controls.
 *
 * @param parent parent control
 * @param title caption
 * @return the handle to the static control
 */
HWND createPanel(HWND parent);

/**
 * @brief create the main menu
 * @return menu handle
 */
HMENU createMainMenu();

/**
 * @brief Destroy the main window.
 * @param hWnd main window handle
 */
void destroyMainWindow(HWND hWnd);

/**
 * @brief Creates a tab control, sized to fit the specified parent window's client
 * area, and adds some tabs.
 * @param hwndParent parent window (the application's main window)
 * @return handle to the tab control
 */
HWND createTab(HWND hwndParent);

/**
 * @brief creates the table with packages
 * @param parent parent window
 * @return window handle
 */
HWND createTable(HWND parent);

#endif // MAIN_H
