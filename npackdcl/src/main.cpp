#include <windows.h>
#include <qstringlist.h>
#include <qstring.h>
#include <QTimer>
#include <QLoggingCategory>

#include "repository.h"
#include "commandline.h"
#include "wpmutils.h"
#include "abstractrepository.h"
#include "dbrepository.h"
#include "version.h"
#include "installedpackages.h"

#include "app.h"

int main(int argc, char *argv[])
{
    HMODULE m = LoadLibrary(L"exchndl.dll");

    QLoggingCategory::setFilterRules(QStringLiteral("npackd=false"));

    QCoreApplication ca(argc, argv);

    CoInitializeEx(0, COINIT_MULTITHREADED);

    qRegisterMetaType<Version>("Version");

    InstalledPackages::packageName =
            "com.googlecode.windows-package-manager.NpackdCL";

    App app;

    QTimer::singleShot(0, &app, SLOT(process()));

    int r = ca.exec();

    FreeLibrary(m);

    return r;
}

