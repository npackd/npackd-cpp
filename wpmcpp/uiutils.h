#ifndef UIUTILS_H
#define UIUTILS_H

#include "qwidget.h"
#include "qstring.h"

/**
 * Some UI related utility methods.
 */
class UIUtils
{
private:
    UIUtils();
public:
    /**
     * Shows a confirmation dialog.
     *
     * @param parent parent widget
     * @param title dialog title
     * @param text message (may contain multiple lines separated by \n)
     */
    static bool confirm(QWidget* parent, QString title, QString text);
};

#endif // UIUTILS_H
