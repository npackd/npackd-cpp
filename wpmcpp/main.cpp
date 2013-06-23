#define _WIN32_IE 0x0500

#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <QApplication>
#include <QMetaType>
#include <QDebug>
#include <QTranslator>

#include "version.h"
#include "mainwindow.h"
#include "repository.h"
#include "wpmutils.h"
#include "job.h"
#include "fileloader.h"
#include "abstractrepository.h"
#include "dbrepository.h"
#include "installedpackages.h"

int main(int argc, char *argv[])
{
    // test: scheduling a task
    //CoInitialize(NULL);
    //WPMUtils::createMSTask();

#if !defined(__x86_64__)
    LoadLibrary(L"exchndl.dll");
#endif

    AbstractRepository::setDefault_(DBRepository::getDefault());

    QApplication a(argc, argv);

/*
 * this should translate the Qt messages
    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(),
            QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qtTranslator);
*/

    QString packageName;
#if !defined(__x86_64__)
    packageName = "com.googlecode.windows-package-manager.Npackd";
#else
    packageName = "com.googlecode.windows-package-manager.Npackd64";
#endif
    InstalledPackages::getDefault()->packageName = packageName;

    // to use a resource: ":/resources/translations"
    QTranslator myappTranslator;
    bool r = myappTranslator.load(//"wpmcpp_fr",  // for testing
            "npackdg_" + QLocale::system().name(),
            a.applicationDirPath(),
            "_.", ".qm");
    if (r)
        a.installTranslator(&myappTranslator);

    qRegisterMetaType<JobState>("JobState");
    qRegisterMetaType<FileLoaderItem>("FileLoaderItem");
    qRegisterMetaType<Version>("Version");

#if !defined(__x86_64__)
    if (WPMUtils::is64BitWindows()) {
        QMessageBox::critical(0, "Error",
                QObject::tr("The 32 bit version of Npackd requires a 32 bit operating system.") + "\n" +
                QObject::tr("Please download the 64 bit version from http://code.google.com/p/windows-package-manager/"));
        return 1;
    }
#endif

    MainWindow w;

    w.prepare();
    w.showMaximized();
    return QApplication::exec();
}
