#ifndef MAIN_H
#define MAIN_H

#include <windows.h>

#include <QString>

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
 * @param hInstance application instance
 * @param nCmdShow SW_*
 * @return main window handle
 */
HWND InitInstance(HINSTANCE, int);

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
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

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
HWND createTabControl(HWND hwndParent);

#endif // MAIN_H
