#define _WIN32_IE 0x0500

#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <QApplication>
#include <QMetaType>
#include <QDebug>
#include <QTranslator>
#include <QList>

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

QString remove(const CommandLine& cl)
{
    QString err;

    // TODO: int programCloseType = WPMUtils::getProgramCloseType(cl, &err);

    QList<PackageVersion*> toRemove;
    if (err.isEmpty()) {
        toRemove =
                WPMUtils::getPackageVersionOptions(cl, &err, false);
    }

    QList<PackageVersion*> installed;
    if (err.isEmpty()) {
        installed = AbstractRepository::getDefault_()->getInstalled_(&err);
    }

    QList<InstallOperation*> ops;
    if (err.isEmpty()) {
        for (int i = 0; i < toRemove.count(); i++) {
            PackageVersion* pv = toRemove.at(i);
            err = pv->planUninstallation(installed, ops);
            if (!err.isEmpty())
                break;
        }
    }

    // TODO: if (err.isEmpty())
    //    process(ops, programCloseType);

    qDeleteAll(installed);
    qDeleteAll(toRemove);

    return err;
}

QString add(const CommandLine& cl)
{
    return ""; // TODO
    /*
    Job* job = clp.createJob();

    if (job->shouldProceed("Detecting installed software")) {
        Job* rjob = job->newSubJob(0.1);
        InstalledPackages::getDefault()->refresh(DBRepository::getDefault(),
                rjob);
        if (!rjob->getErrorMessage().isEmpty()) {
            job->setErrorMessage(rjob->getErrorMessage());
        }
        delete rjob;
    }

    QString err;
    QList<PackageVersion*> toInstall =
            WPMUtils::getPackageVersionOptions(cl, &err, true);
    if (!err.isEmpty())
        job->setErrorMessage(err);

    // debug: WPMUtils::outputTextConsole << "Versions: " << d.toString()) << std::endl;
    QList<InstallOperation*> ops;
    if (job->shouldProceed()) {
        QString err;
        QList<PackageVersion*> installed =
                AbstractRepository::getDefault_()->getInstalled_(&err);
        if (!err.isEmpty())
            job->setErrorMessage(err);

        QList<PackageVersion*> avoid;
        for (int i = 0; i < toInstall.size(); i++) {
            PackageVersion* pv = toInstall.at(i);
            if (job->shouldProceed())
                err = pv->planInstallation(installed, ops, avoid);
            if (!err.isEmpty()) {
                job->setErrorMessage(err);
            }
        }
        qDeleteAll(installed);
    }

    // debug: WPMUtils::outputTextConsole(QString("%1\n").arg(ops.size()));

    AbstractRepository* rep = AbstractRepository::getDefault_();
    if (job->shouldProceed("Installing") && ops.size() > 0) {
        Job* ijob = job->newSubJob(0.9);
        rep->process(ijob, ops, WPMUtils::CLOSE_WINDOW);
        if (!ijob->getErrorMessage().isEmpty())
            job->setErrorMessage(QString("Error installing: %1").
                    arg(ijob->getErrorMessage()));

        delete ijob;
    }

    job->complete();

    QString r = job->getErrorMessage();
    if (r.isEmpty()) {
        for (int i = 0; i < toInstall.size(); i++) {
            PackageVersion* pv = toInstall.at(i);
            WPMUtils::outputTextConsole(QString(
                    "The package %1 was installed successfully in %2\n").arg(
                    pv->toString()).arg(pv->getPath()));
        }
    }

    delete job;

    qDeleteAll(ops);
    qDeleteAll(toInstall);

    return r;
    */
}


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

    CommandLine cl;

    cl.add("package", 'p',
            "internal package name (e.g. com.example.Editor or just Editor)",
            "package", true);
    cl.add("versions", 'r', "versions range (e.g. [1.5,2))",
            "range", false);
    cl.add("version", 'v', "version number (e.g. 1.5.12)",
            "version", false);
    cl.add("url", 'u', "repository URL (e.g. https://www.example.com/Rep.xml)",
            "repository", false);
    cl.add("status", 's', "filters package versions by status",
            "status", false);
    cl.add("bare-format", 'b', "bare format (no heading or summary)",
            "", false);
    cl.add("query", 'q', "search terms (e.g. editor)",
            "search terms", false);
    cl.add("debug", 'd', "turn on the debug output", "", false);
    cl.add("file", 'f', "file or directory", "file", false);
    cl.add("end-process", 'e',
        "comma separated list of ways to close running applications (windows, kill)",
        "list", false);

    QString commandLineParsingError = cl.parse();

    // cl.dump();

    /* TODO
    if (cl.isPresent("debug")) {
        clp.setUpdateRate(0);
    }
    */

    QStringList fr = cl.getFreeArguments();

    if (!commandLineParsingError.isEmpty()) {
        QString msg = QObject::tr("Error parsing the command line: %1").
                arg(commandLineParsingError);
        QMessageBox::critical(0, QObject::tr("Error"), msg);
        return 1;
    } else if (fr.count() == 1) {
        QString cmd = fr.at(0);

        QString err;
        /*if (cmd == "help") {
            usage();
        } else if (cmd == "path") {
            err = path();
        } else */if (cmd == "remove" || cmd == "rm") {
            err = remove(cl);
        } else if (cmd == "add") {
            err = add(cl);
        } /* else if (cmd == "add-repo") {
            err = addRepo();
        } else if (cmd == "remove-repo") {
            err = removeRepo();
        } else if (cmd == "list-repos") {
            err = listRepos();
        } else if (cmd == "search") {
            err = search();
        } else if (cmd == "check") {
            err = check();
        } else if (cmd == "which") {
            err = which();
        } else if (cmd == "list") {
            err = list();
        } else if (cmd == "info") {
            err = info();
        } else if (cmd == "update") {
            err = update();
        } else if (cmd == "detect") {
            err = detect();
        }*/ else {
            err = "Wrong command: " + cmd + ". Try npackdg help";
        }
        if (!err.isEmpty()) {
            QMessageBox::critical(0, QObject::tr("Error"), err);
            return 1;
        }
        return 0;
    } else if (fr.count() > 1) {
        QString err = QObject::tr("Unexpected argument: %1").
                arg(fr.at(1));
        QMessageBox::critical(0, QObject::tr("Error"), err);
        return 1;
    } else {
        MainWindow w;

        w.prepare();
        w.show();
        return QApplication::exec();
    }
}
