#define _WIN32_IE 0x0500

#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <stdint.h>

#include <QApplication>
#include <QMetaType>
#include <QDebug>
#include <QTranslator>
#include <QList>
#include <QVBoxLayout>
#include <QImageReader>
#include <QtPlugin>

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
#include "installthread.h"
#include "clprocessor.h"

Q_IMPORT_PLUGIN(QICOPlugin)

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

    QString packageName;
#if !defined(__x86_64__)
    packageName = "com.googlecode.windows-package-manager.Npackd";
#else
    packageName = "com.googlecode.windows-package-manager.Npackd64";
#endif
    InstalledPackages::getDefault()->packageName = packageName;

    QTranslator myappTranslator;
    bool r = myappTranslator.load(
            "wpmcpp_" + QLocale::system().name(),
            ":/translations");
    if (r)
        a.installTranslator(&myappTranslator);

    qRegisterMetaType<JobState>("JobState");
    qRegisterMetaType<Version>("Version");
    qRegisterMetaType<int64_t>("int64_t");

#if !defined(__x86_64__)
    if (WPMUtils::is64BitWindows()) {
        QMessageBox::critical(0, "Error",
                QObject::tr("The 32 bit version of Npackd requires a 32 bit operating system.") + "\n" +
                QObject::tr("Please download the 64 bit version from http://code.google.com/p/windows-package-manager/"));
        return 1;
    }
#endif

    CLProcessor clp;

    // qDebug() << QImageReader::supportedImageFormats();

    int errorCode;
    if (!clp.process(&errorCode)){
        MainWindow w;

        w.prepare();
        w.show();
        errorCode = QApplication::exec();
    }

    //WPMUtils::timer.dump();

    return errorCode;
}
