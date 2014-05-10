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
     * Shows a confirmation dialog.
     *
     * @param parent parent widget
     * @param title dialog title
     * @param text message (may contain multiple lines separated by \n or HTML)
     * @param detailedText detailed plain text (may contain multiple lines, but
     *     not HTML)
     */
    static bool confirm(QWidget* parent, QString title, QString text,
            QString detailedText);

    static bool confirmInstallOperations(QWidget *parent,
            QList<InstallOperation *> &install, QString *err);
};

#endif // UIUTILS_H
