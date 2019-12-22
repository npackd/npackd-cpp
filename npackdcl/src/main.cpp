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
#include "commandlinemessagehandler.h"

#include "app.h"

App app;

BOOL WINAPI ctrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (app.currentJob) {
                app.currentJob->cancel();
                WPMUtils::outputTextConsole("Cancelled\n");
            }
            break;
    }

    return TRUE;
}

int main(int argc, char *argv[])
{
    oldMessageHandler = qInstallMessageHandler(eventLogMessageHandler);

    HMODULE m = LoadLibrary(L"exchndl.dll");

    QLoggingCategory::setFilterRules("npackd=true\nnpackd.debug=false");

    // WPMUtils::writeln(QString("npackd.isDebugEnabled %1").arg(npackd().isDebugEnabled()));
    // WPMUtils::writeln(QString("npackd.isInfoEnabled %1").arg(npackd().isInfoEnabled()));

#if NPACKD_ADMIN != 1
	WPMUtils::hasAdminPrivileges();
#endif

    QCoreApplication ca(argc, argv);

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    qRegisterMetaType<Version>("Version");

    InstalledPackages::packageName =
            "com.googlecode.windows-package-manager.NpackdCL";

    SetConsoleCtrlHandler(ctrlHandler, TRUE);

    QTimer::singleShot(0, &app, SLOT(process()));

    int r = ca.exec();

    FreeLibrary(m);

    return r;
}

