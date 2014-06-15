#include "clprocessor.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QString>

#include "progressframe.h"
#include "installedpackages.h"
#include "wpmutils.h"
#include "packageversion.h"
#include "abstractrepository.h"
#include "installoperation.h"
#include "uiutils.h"
#include "installthread.h"
#include "commandline.h"

// TODO: i18n

class ProgressDialog: public QDialog {
public:
    Job* job;

    void reject();
};

void ProgressDialog::reject() {
    job->cancel();
}

CLProcessor::CLProcessor()
{
}

void CLProcessor::monitor(Job* job, const QString& title, QThread* thread)
{
    ProgressDialog* d = new ProgressDialog();
    d->setWindowTitle(title);
    d->job = job;
    QVBoxLayout* layout = new QVBoxLayout();

    ProgressFrame* pf = new ProgressFrame(d, job, title, thread);
    layout->insertWidget(0, pf);

    d->setLayout(layout);

    QObject::connect(thread, SIGNAL(finished()), d,
            SLOT(accept()), Qt::QueuedConnection);

    d->resize(400, 200);
    d->exec();
    d->deleteLater();
}

QString CLProcessor::remove()
{
    QString err;

    err = DBRepository::getDefault()->openDefault();

    if (err.isEmpty()) {
        err = InstalledPackages::getDefault()->readRegistryDatabase();
    }

    int programCloseType = WPMUtils::CLOSE_WINDOW;
    if (err.isEmpty())
        programCloseType = WPMUtils::getProgramCloseType(cl, &err);

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

    bool confirmed = false;
    if (err.isEmpty())
        confirmed = UIUtils::confirmInstallOperations(0, ops, &err);

    if (err.isEmpty() && confirmed) {
        Job* job = new Job();
        InstallThread* it = new InstallThread(0, 1, job);
        it->install = ops;
        it->programCloseType = programCloseType;
        ops.clear();

        monitor(job, QObject::tr("Uninstall"), it);
    }

    qDeleteAll(installed);
    qDeleteAll(toRemove);

    qDeleteAll(ops);
    ops.clear();

    return err;
}

QString CLProcessor::startNewestNpackdg()
{
    QString err;

    InstalledPackages* ip = InstalledPackages::getDefault();
    if (err.isEmpty()) {
        err = ip->readRegistryDatabase();
    }

    QString exe;
    if (err.isEmpty()) {
        InstalledPackageVersion* ipv = ip->getNewestInstalled(ip->packageName);

        if (!ipv || ipv->version <= Version(NPACKD_VERSION))
            err = QObject::tr("Newer Npackd GUI was not found");
        else
            exe = ipv->directory + "\\npackdg.exe";

        delete ipv;
    }

    if (err.isEmpty()) {
        QString args;
        bool success = false;
        PROCESS_INFORMATION pinfo;

        STARTUPINFOW startupInfo = {
            sizeof(STARTUPINFO), 0, 0, 0,
            (ulong) CW_USEDEFAULT, (ulong) CW_USEDEFAULT,
            (ulong) CW_USEDEFAULT, (ulong) CW_USEDEFAULT,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        startupInfo.dwFlags = STARTF_USESHOWWINDOW;
        startupInfo.wShowWindow = SW_HIDE;
        success = CreateProcess(
                (wchar_t*) exe.utf16(),
                (wchar_t*) args.utf16(),
                0, 0, TRUE,
                CREATE_UNICODE_ENVIRONMENT, 0,
                0, &startupInfo, &pinfo);

        if (success) {
            CloseHandle(pinfo.hThread);
            CloseHandle(pinfo.hProcess);
        } else {
            WPMUtils::formatMessage(GetLastError(), &err);
            err = QObject::tr("Error starting %1: %2").arg(exe).arg(err);
        }
    }

    return err;
}

QString CLProcessor::add()
{
    QString err;

    err = DBRepository::getDefault()->openDefault();

    if (err.isEmpty()) {
        err = InstalledPackages::getDefault()->readRegistryDatabase();
    }

    QList<PackageVersion*> toInstall =
            WPMUtils::getPackageVersionOptions(cl, &err, true);

    DWORD pct = WPMUtils::CLOSE_WINDOW;
    if (err.isEmpty()) {
        pct = WPMUtils::getProgramCloseType(cl, &err);
    }

    // debug: WPMUtils::outputTextConsole << "Versions: " << d.toString()) << std::endl;
    QList<InstallOperation*> ops;

    QList<PackageVersion*> installed;
    if (err.isEmpty()) {
        installed = AbstractRepository::getDefault_()->getInstalled_(&err);
    }

    if (err.isEmpty()) {
        QList<PackageVersion*> avoid;
        for (int i = 0; i < toInstall.size(); i++) {
            PackageVersion* pv = toInstall.at(i);
            err = pv->planInstallation(installed, ops, avoid);
            if (!err.isEmpty())
                break;
        }
    }

    qDeleteAll(installed);

    // debug: WPMUtils::outputTextConsole(QString("%1\n").arg(ops.size()));

    if (err.isEmpty()) {
        Job* job = new Job();
        InstallThread* it = new InstallThread(0, 1, job);
        it->install = ops;
        it->programCloseType = pct;
        ops.clear();

        monitor(job, QObject::tr("Install"), it);
    }

    qDeleteAll(ops);
    qDeleteAll(toInstall);

    return err;
}

QString CLProcessor::update()
{
    Job* job = new Job();

    QString err = DBRepository::getDefault()->openDefault();
    if (!err.isEmpty())
        job->setErrorMessage(err);

    if (job->shouldProceed()) {
        err = InstalledPackages::getDefault()->readRegistryDatabase();
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    DBRepository* rep = DBRepository::getDefault();

    int programCloseType = WPMUtils::CLOSE_WINDOW;
    if (job->shouldProceed()) {
        QString err;
        programCloseType = WPMUtils::getProgramCloseType(cl, &err);
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        }
    }

    QStringList packages_ = cl.getAll("package");

    if (job->shouldProceed()) {
        if (packages_.size() == 0) {
            job->setErrorMessage(QObject::tr("Missing option: --package"));
        }
    }

    if (job->shouldProceed()) {
        for (int i = 0; i < packages_.size(); i++) {
            QString package = packages_.at(i);
            if (!Package::isValidName(package)) {
                job->setErrorMessage(QObject::tr("Invalid package name: %1").
                        arg(package));
            }
        }
    }

    QList<Package*> toUpdate;

    if (job->shouldProceed()) {
        for (int i = 0; i < packages_.size(); i++) {
            QString package = packages_.at(i);
            QList<Package*> packages;
            if (package.contains('.')) {
                Package* p = rep->findPackage_(package);
                if (p)
                    packages.append(p);
            } else {
                packages = rep->findPackagesByShortName(package);
            }

            if (job->shouldProceed()) {
                if (packages.count() == 0) {
                    job->setErrorMessage(QObject::tr("Unknown package: %1").
                            arg(package));
                } else if (packages.count() > 1) {
                    job->setErrorMessage(QObject::tr("Ambiguous package name"));
                } else {
                    toUpdate.append(packages.at(0)->clone());
                }
            }

            qDeleteAll(packages);

            if (!job->shouldProceed())
                break;
        }
    }

    QList<InstallOperation*> ops;
    bool up2date = false;
    if (job->shouldProceed(QObject::tr("Planning"))) {
        QString err = rep->planUpdates(toUpdate, ops);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            job->setProgress(0.12);
            up2date = ops.size() == 0;
        }
    }

    /**
     *confirmation. This is not yet enabled in order to maintain the
     * compatibility.
    if (job->shouldProceed() && !up2date) {
        QString title;
        QString err;
        if (!confirm(ops, &title, &err))
            job->cancel();
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }
    */

    if (job->shouldProceed(QObject::tr("Updating")) && !up2date) {
        Job* sjob = new Job();
        InstallThread* it = new InstallThread(0, 1, sjob);
        it->install = ops;
        it->programCloseType = programCloseType;
        ops.clear();

        monitor(sjob, QObject::tr("Install"), it);
        if (sjob->getErrorMessage().isEmpty())
            job->setErrorMessage(sjob->getErrorMessage());
    }

    /*
    if (job->shouldProceed(QObject::tr("Updating")) && !up2date) {
        Job* ijob = job->newSubJob(0.86);
        rep->process(ijob, ops, programCloseType);
        if (!ijob->getErrorMessage().isEmpty()) {
            job->setErrorMessage(QObject::tr("Error updating: %1").
                    arg(ijob->getErrorMessage()));
        }
        delete ijob;
    }
    */

    qDeleteAll(ops);

    job->complete();

    /*
    QString r = job->getErrorMessage();
    if (job->isCancelled()) {
        r = QObject::tr("The package update was cancelled");
    } else if (up2date) {
        WPMUtils::outputTextConsole(
                QObject::tr("The packages are already up-to-date\n"));
    } else if (r.isEmpty()) {
        WPMUtils::outputTextConsole(
                QObject::tr("The packages were updated successfully\n"));
    }
    */

    QString ret = job->getErrorMessage();

    delete job;

    qDeleteAll(toUpdate);

    return ret;
}

void CLProcessor::usage()
{
    const char* lines[] = {
        "Usage:",
        "    npackdg help",
        "        prints this help",
        "    npackdg add (--package=<package> [--version=<version>])+",
        "        installs packages. The newest available version will be installed, ",
        "        if none is specified.",
        "        Short package names can be used here",
        "        (e.g. App instead of com.example.App)",
        "    npackdg remove|rm (--package=<package> [--version=<version>])+",
        "           [--end-process=<types>]",
        "        removes packages. The version number may be omitted, ",
        "        if only one is installed.",
        "        Short package names can be used here",
        "        (e.g. App instead of com.example.App)",
        /*
        "    npackdg update (--package=<package>)+ [--end-process=<types>]",
        "        updates packages by uninstalling the currently installed",
        "        and installing the newest version. ",
        "        Short package names can be used here",
        "        (e.g. App instead of com.example.App)",
        */
        "    npackdg start-newest",
        "        searches for the newest installed version ",
        "        of Npackd GUI and starts it",
        /*
        "    npackdcl list [--status=installed | all] [--bare-format]",
        "        lists package versions sorted by package name and version.",
        "        Please note that since 1.18 only installed package versions",
        "        are listed regardless of the --status switch.",
        "    npackdcl search [--query=<search terms>] [--status=installed | all]",
        "            [--bare-format]",
        "        full text search. Lists found packages sorted by package name.",
        "        All packages are shown by default.",
        "    npackdcl info --package=<package> [--version=<version>]",
        "        shows information about the specified package or package version",
        "    npackdcl path --package=<package> [--versions=<versions>]",
        "        searches for an installed package and prints its location",
        "    npackdcl add-repo --url=<repository>",
        "        appends a repository to the list",
        "    npackdcl remove-repo --url=<repository>",
        "        removes a repository from the list",
        "    npackdcl list-repos",
        "        list currently defined repositories",
        "    npackdcl detect",
        "        detect packages from the MSI database and software control panel",
        "    npackdcl check",
        "        checks the installed packages for missing dependencies",
        "    npackdcl which --file=<file>",
        "        finds the package that owns the specified file or directory",
        "    npackdcl set-install-dir --file=<directory>",
        "        changes the directory where packages will be installed",
        "    npackdcl get-install-dir",
        "        prints the directory where packages will be installed",
        */
        "Options:",
    };
    QStringList sl;
    for (int i = 0; i < (int) (sizeof(lines) / sizeof(lines[0])); i++) {
        sl.append(QString(lines[i]));
    }

    QStringList opts = this->cl.printOptions();
    sl.append(opts);

    const char* lines2[] = {
        "",
        "The process exits with the code unequal to 0 if an error occures."
        //"",
        // "See https://code.google.com/p/windows-package-manager/wiki/CommandLine for more details.",
    };
    for (int i = 0; i < (int) (sizeof(lines2) / sizeof(lines2[0])); i++) {
        sl.append(QString(lines2[i]));
    }

    QMessageBox mb(0);
    mb.setWindowTitle(QString(
            "Npackd %1\n").arg(NPACKD_VERSION));
    mb.setText("npackdg [command] [options]\n"
            "Here is [command] one of add, remove, rm or help");
    mb.setIcon(QMessageBox::Information);
    mb.setStandardButtons(QMessageBox::Ok);
    mb.setDefaultButton(QMessageBox::Ok);
    mb.setDetailedText(sl.join("\r\n"));
    mb.exec();
}

bool CLProcessor::process(int* errorCode)
{
    *errorCode = 0;

    bool ret = true;

    cl.add("package", 'p',
            QObject::tr("internal package name (e.g. com.example.Editor or just Editor)"),
            QObject::tr("package"), true);
    cl.add("versions", 'r', QObject::tr("versions range (e.g. [1.5,2))"),
            QObject::tr("range"), false);
    cl.add("version", 'v', QObject::tr("version number (e.g. 1.5.12)"),
            QObject::tr("version"), false);
            /*
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
    */
    cl.add("end-process", 'e',
            QObject::tr("list of ways to close running applications (c=close, k=kill). The default value is 'c'."),
            QObject::tr("[c][k]"), false);

    QString commandLineParsingError = cl.parse();

    // cl.dump();

    /*
    if (cl.isPresent("debug")) {
        clp.setUpdateRate(0);
    }
    */

    QStringList fr = cl.getFreeArguments();

    if (!commandLineParsingError.isEmpty()) {
        QString msg = QObject::tr("Error parsing the command line: %1").
                arg(commandLineParsingError);
        QMessageBox::critical(0, QObject::tr("Error"), msg);
        *errorCode = 1;
    } else if (fr.count() == 1) {
        QString cmd = fr.at(0);

        QString err;
        if (cmd == "help") {
            usage();
        }/* else if (cmd == "path") {
            err = path();
        }*/ else if (cmd == "remove" || cmd == "rm") {
            err = remove();
        } else if (cmd == "add") {
            err = add();
        } else if (cmd == "start-newest") {
            err = startNewestNpackdg();
            if (!err.isEmpty())
                ret = false;
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
            err = QObject::tr("Wrong command: %1. Try npackdg help").arg(cmd);
        }
        if (!err.isEmpty()) {
            QMessageBox::critical(0, QObject::tr("Error"), err);
            *errorCode = 1;
        }
    } else if (fr.count() > 1) {
        QString err = QObject::tr("Unexpected argument: %1").
                arg(fr.at(1));
        QMessageBox::critical(0, QObject::tr("Error"), err);
        *errorCode = 1;
    } else {
        ret = false;
    }

    return ret;
}
