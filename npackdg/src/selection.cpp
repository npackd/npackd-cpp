#include <windows.h>

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
    HWND w = GetFocus();
    Selection* ret = nullptr;
    while (w) {
        LONG_PTR p = GetWindowLongPtr(w, GWLP_USERDATA);
        ret = dynamic_cast<Selection*>(reinterpret_cast<QObject*>(p));
        if (ret)
            break;
        w = GetParent(w);
    }
    return ret;
}

