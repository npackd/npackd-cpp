#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <QApplication>
#include <QMetaType>
#include <QTranslator>
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
#include "uimessagehandler.h"
#include "packageutils.h"

// These lines are necessary for a static build
#ifdef NPACKD_STATIC
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin);
Q_IMPORT_PLUGIN(QSQLiteDriverPlugin);
Q_IMPORT_PLUGIN(QICNSPlugin);
Q_IMPORT_PLUGIN(QICOPlugin);
Q_IMPORT_PLUGIN(QJpegPlugin);
Q_IMPORT_PLUGIN(QGifPlugin);
Q_IMPORT_PLUGINQTiffPlugin);
Q_IMPORT_PLUGIN(QWbmpPlugin);
Q_IMPORT_PLUGIN(QWebpPlugin);
Q_IMPORT_PLUGIN(QSvgPlugin);
Q_IMPORT_PLUGIN(QTgaPlugin);
#endif

// Modern and efficient C++ Thread Pool Library
// https://github.com/vit-vit/CTPL

int main(int argc, char *argv[])
{
    //qCDebug(npackd) << QUrl("file:///C:/test").resolved(QUrl::fromLocalFile("abc.txt"));

    qInstallMessageHandler(eventLogMessageHandler);

    HMODULE m = LoadLibrary(L"exchndl.dll");

    QLoggingCategory::setFilterRules("npackd=true\nnpackd.debug=false");

#if NPACKD_ADMIN != 1
    PackageUtils::globalMode = WPMUtils::hasAdminPrivileges();
#endif

    // this does not work unfortunately. The only way to ensure the maximum width of a tooltip is to
    // use HTML
    // a.setStyleSheet("QToolTip { height: auto; max-width: 400px; color: #ffffff; background-color: #2a82da; border: 1px solid white; }");

    QString packageName;
#if !defined(__x86_64__) && !defined(_WIN64)
    packageName = "com.googlecode.windows-package-manager.Npackd";
#else
    packageName = "com.googlecode.windows-package-manager.Npackd64";
#endif
    InstalledPackages::packageName = packageName;

    qRegisterMetaType<Version>("Version");
    qRegisterMetaType<int64_t>("int64_t");
    qRegisterMetaType<QMessageBox::Icon>("QMessageBox::Icon");

#if !defined(__x86_64__) && !defined(_WIN64)
    if (WPMUtils::is64BitWindows()) {
        QMessageBox::critical(0, "Error",
                QObject::tr("The 32 bit version of Npackd requires a 32 bit operating system.") + "\r\n" +
                QObject::tr("Please download the 64 bit version from https://github.com/tim-lebedkov/npackd/wiki/Downloads"));
        return 1;
    }
#endif


    // July, 25 2018:
    // "bmp", "cur", "gif", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "xbm", "xpm"
    // December, 25 2018:
    // "bmp", "cur", "gif", "icns", "ico", "jp2", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
    // July, 27 2019:
    // "bmp", "cur", "gif", "icns", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
    // September, 26 2020:
    // "bmp", "cur", "gif", "icns", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "svg", "svgz", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
    // January, 28 2024:
    // "bmp", "cur", "gif", "icns", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "svg", "svgz", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm" 
    qCInfo(npackdImportant) << QImageReader::supportedImageFormats();

    //QString::fromLatin1(QImageReader::supportedImageFormats().join(','));
    //QMessageBox::information(0, "Error", "def"); //ba));

    // July, 25 2018: "windowsvista", "Windows", "Fusion"
    qCDebug(npackd) << QStyleFactory::keys();

    CLProcessor clp;

    int errorCode;
    clp.process(argc, argv, &errorCode);

    //WPMUtils::timer.dump();

    FreeLibrary(m);

    return errorCode;
}
