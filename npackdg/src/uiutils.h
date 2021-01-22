#ifndef UIUTILS_H
#define UIUTILS_H

#include <windows.h>

#include <QWidget>
#include <QString>

#include "installoperation.h"

/**
 * Some UI related utility methods.
 */
class UIUtils
{
private:
    UIUtils();

    static QString createPackageVersionsHTML(const std::vector<QString> &names);
public:
    /** size of an icon in the UI */
    static const int ICON_SIZE = 32;

    /**
     * @brief processes the specified operations by copying this executable to a
     *     temporary location, closing this instance of the UI, and starting
     *     the copied executable
     * @param job job
     * @param install operations
     * @param programCloseType how to close processes
     */
    static void processWithSelfUpdate(Job *job,
            std::vector<InstallOperation *> &install, DWORD programCloseType);

    /**
     * Shows a confirmation dialog.
     *
     * @param parent parent widget or 0
     * @param title dialog title
     * @param text message (may contain multiple lines separated by \n or HTML)
     * @param detailedText detailed plain text (may contain multiple lines, but
     *     not HTML)
     */
    static bool confirm(QWidget* parent, QString title, QString text,
            QString detailedText);

    /**
     * @brief confirms the specified operations
     * @param parent parent widget or 0
     * @param install operations
     * @param title the title for the job will be stored here
     * @param err error message will be stored here
     * @return true = confirmed
     */
    static bool confirmInstallOperations(QWidget *parent,
            std::vector<InstallOperation *> &install, QString *title, QString *err);

    /**
     * @brief choose the accelerators automatically
     * @param titles existing titles with or without accelerators (will be modified)
     * @param ignore lower case characters that should be ignored
     */
    static void chooseAccelerators(std::vector<QString>* titles,
            const QString &ignore=QString());

    /**
     * @brief choose the accelerators automatically
     * @param w a widget
     * @param ignore lower case characters that should be ignored
     */
    static void chooseAccelerators(QWidget* w, const QString &ignore=QString());

    /**
     * @brief searches for accelerators
     * @param titles titles
     * @return lower case characters for found accelerators
     */
    static QString extractAccelerators(const std::vector<QString> &titles);
};

#endif // UIUTILS_H
