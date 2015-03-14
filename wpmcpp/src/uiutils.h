#ifndef UIUTILS_H
#define UIUTILS_H

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

    static QString createPackageVersionsHTML(const QStringList &names);
public:
    /**
     * @brief processes the specified operations by copying this executable to a
     *     temporary location, closing this instance of the UI, and starting
     *     the copied executable
     * @param job job
     * @param install operations
     * @param programCloseType how to close processes
     */
    static void processWithSelfUpdate(Job *job,
            QList<InstallOperation *> &install, int programCloseType);

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
     * @param err error message will be stored here
     * @return true = confirmed
     */
    static bool confirmInstallOperations(QWidget *parent,
            QList<InstallOperation *> &install, QString *err);
};

#endif // UIUTILS_H
