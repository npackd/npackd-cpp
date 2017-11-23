#define _WIN32_IE 0x0500

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
            "wpmcpp_" + QLocale::system().name(),
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

	HANDLE hMutex = NULL;
	bool is_running = false;
	WindowsRegistry wr;
	QString err = wr.open(HKEY_LOCAL_MACHINE,
		"Software\\Npackd\\Npackd", false, KEY_READ);
	if (err.isEmpty()) {
		DWORD dwSingleInstance = wr.getDWORD("SingleInstance", &err);
		if (err.isEmpty() && dwSingleInstance == 1) {
			hMutex = CreateMutexA(NULL, FALSE, packageName.toStdString().c_str());
			is_running = (GetLastError() == ERROR_ALREADY_EXISTS);

			if (is_running) {
				QMessageBox msgBox;
				msgBox.setIcon(QMessageBox::Warning);
				msgBox.setText(a.tr("npacked is already running!"));
				msgBox.exec();
			}
		}
	}

    CLProcessor clp;

    // qCDebug(npackd) << QImageReader::supportedImageFormats();

    int errorCode;
    if (!clp.process(&errorCode) && !is_running){
        MainWindow w;

        w.prepare();
        w.show();
        errorCode = QApplication::exec();
    }

    //WPMUtils::timer.dump();

    FreeLibrary(m);

	if (hMutex) CloseHandle(hMutex);

    return errorCode;
}
