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

    /**
     * @brief extracts an icon from a file using the Windows API ExtractIcon()
     * @param iconFile .ico, .exe, etc. Additionally to what is supported by
     *     ExtractIcon() also ",0" and similar at the end of the path name is
     *     supported to load different icons from the same file
     * @return "data:..." or an empty string if the icon cannot be extracted
     */
    static QString extractIconURL(const QString &iconFile);

    static bool confirmInstallOperations(QWidget *parent,
            QList<InstallOperation *> &install, QString *err);
};

#endif // UIUTILS_H
