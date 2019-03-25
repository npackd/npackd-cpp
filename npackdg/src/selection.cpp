#include "selection.h"

#include <QWidget>
#include <QApplication>

Selection::Selection()
{
}

Selection::~Selection()
{
}

Selection* Selection::findCurrent()
{
    QWidget* w = QApplication::focusWidget();
    Selection* ret = nullptr;
    while (w) {
        ret = dynamic_cast<Selection*>(w);
        if (ret)
            break;
        w = w->parentWidget();
    }
    return ret;
}

