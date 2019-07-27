#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <stdint.h>

#include <QApplication>
#include <QMetaType>
#include <QTranslator>
#include <QList>
#include <QVBoxLayout>
#include <QImageReader>
#include <QtPlugin>
#include <QStyleFactory>

#include "version.h"
#include "mainwindow.h"
#include "repository.h"
#include "wpmutils.h"
#include "job.h"
#include "fileloader.h"
#include "abstractrepository.h"
#include "dbrepository.h"
#include "installedpackages.h"
#include "commandline.h"
#include "packageversion.h"
#include "installoperation.h"
#include "uiutils.h"
#include "clprocessor.h"

// Modern and efficient C++ Thread Pool Library
// https://github.com/vit-vit/CTPL

int main(int argc, char *argv[])
{
    //qCDebug(npackd) << QUrl("file:///C:/test").resolved(QUrl::fromLocalFile("abc.txt"));

    // test: scheduling a task
    //CoInitialize(NULL);
    //WPMUtils::createMSTask();

    HMODULE m = LoadLibrary(L"exchndl.dll");

    QLoggingCategory::setFilterRules("npackd=false");

#if NPACKED_ADMIN != 1
    WPMUtils::hasAdminPrivileges();
#endif

    QApplication a(argc, argv);

    QString packageName;
#if !defined(__x86_64__) && !defined(_WIN64)
    packageName = "com.googlecode.windows-package-manager.Npackd";
#else
    packageName = "com.googlecode.windows-package-manager.Npackd64";
#endif
    InstalledPackages::packageName = packageName;

    QTranslator myappTranslator;
    bool r = myappTranslator.load(
            "npackdg_" + QLocale::system().name(),
            ":/translations");
    if (r)
        a.installTranslator(&myappTranslator);

    qRegisterMetaType<Version>("Version");
    qRegisterMetaType<int64_t>("int64_t");

#if !defined(__x86_64__) && !defined(_WIN64)
    if (WPMUtils::is64BitWindows()) {
        QMessageBox::critical(0, "Error",
                QObject::tr("The 32 bit version of Npackd requires a 32 bit operating system.") + "\r\n" +
                QObject::tr("Please download the 64 bit version from https://github.com/tim-lebedkov/npackd/wiki/Downloads"));
        return 1;
    }
#endif


    CLProcessor clp;

    // July, 25 2018:
    // "bmp", "cur", "gif", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "xbm", "xpm"
    // December, 25 2018:
    // "bmp", "cur", "gif", "icns", "ico", "jp2", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
    // July, 27 2019:
    // "bmp", "cur", "gif", "icns", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
    qCDebug(npackd) << QImageReader::supportedImageFormats();

    // July, 25 2018: "windowsvista", "Windows", "Fusion"
    qCDebug(npackd) << QStyleFactory::keys();

    int errorCode;
    if (!clp.process(&errorCode)){
        MainWindow w;

        w.prepare();
        w.show();
        errorCode = QApplication::exec();
    }

    //WPMUtils::timer.dump();

    FreeLibrary(m);

    return errorCode;
}
