#include "selection.h"

#include <QWidget>
#include <QApplication>
#include <QDebug>

Selection::Selection()
{
}

Selection* Selection::findCurrent()
{
    QWidget* w = QApplication::focusWidget();
    Selection* ret = 0;
    while (w) {
        ret = dynamic_cast<Selection*>(w);
        if (ret)
            break;
        w = w->parentWidget();
    }
    return ret;
}

