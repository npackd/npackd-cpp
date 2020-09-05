#include "clprocessor.h"

#include <windows.h>

#include <QApplication>
#include <QDialog>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QString>
#include <QtConcurrent/QtConcurrent>

#include "installedpackages.h"
#include "wpmutils.h"
#include "packageversion.h"
#include "abstractrepository.h"
#include "installoperation.h"
#include "uiutils.h"
#include "commandline.h"
#include "progresstree2.h"
#include "mainwindow.h"
#include "packageutils.h"

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

CLProcessor::~CLProcessor()
{
}

void CLProcessor::monitorAndWaitFor(Job* job)
{
    ProgressDialog* d = new ProgressDialog();
    d->setWindowTitle("Npackd: " + job->getTitle());
    d->job = job;
    QVBoxLayout* layout = new QVBoxLayout();

    ProgressTree2* pf = new ProgressTree2(d);
    pf->addJob(job);
    pf->setColumnWidth(0, 270);
    pf->setColumnWidth(1, 60);
    pf->setColumnWidth(2, 60);
    pf->setColumnWidth(3, 100);
    pf->setColumnWidth(4, 70);
    layout->insertWidget(0, pf);

    d->setLayout(layout);

    QObject::connect(job, SIGNAL(jobCompleted()), d,
            SLOT(accept()), Qt::QueuedConnection);

    d->resize(600, 200);

    if (!job->isCompleted()) {
        d->exec();
    }

    d->deleteLater();
}

QString CLProcessor::remove()
{
    QString err;

    err = DBRepository::getDefault()->openDefault();

    if (err.isEmpty()) {
        err = InstalledPackages::getDefault()->readRegistryDatabase();
    }

    DWORD programCloseType = WPMUtils::CLOSE_WINDOW;
    if (err.isEmpty())
        programCloseType = WPMUtils::getProgramCloseType(cl, &err);

    QList<PackageVersion*> toRemove;
    if (err.isEmpty()) {
        toRemove =
                PackageVersion::getRemovePackageVersionOptions(cl, &err);
    }

    QList<InstallOperation*> ops;
    InstalledPackages installed(*InstalledPackages::getDefault());

    if (err.isEmpty()) {
        err = DBRepository::getDefault()->planAddMissingDeps(installed, ops);
    }

    if (err.isEmpty()) {
        for (int i = 0; i < toRemove.count(); i++) {
            PackageVersion* pv = toRemove.at(i);
            err = DBRepository::getDefault()->planUninstallation(installed,
                    pv->package, pv->version, ops);
            if (!err.isEmpty())
                break;
        }
    }

    if (err.isEmpty()) {
        err = process(ops, programCloseType);
    }

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
        InstalledPackageVersion* ipv = ip->getNewestInstalled(
                InstalledPackages::packageName);

        Version myVersion;
        (void) myVersion.setVersion(NPACKD_VERSION);
        if (!ipv || ipv->version <= myVersion)
            err = QObject::tr("Newer Npackd GUI was not found");
        else
            exe = ipv->directory + "\\npackdg.exe";

        delete ipv;
    }

    if (err.isEmpty()) {
        QString args = "\"" + exe + "\"";
        PROCESS_INFORMATION pinfo;

        STARTUPINFOW startupInfo = {
            sizeof(STARTUPINFO), nullptr, nullptr, nullptr,
            static_cast<DWORD>(CW_USEDEFAULT),
            static_cast<DWORD>(CW_USEDEFAULT),
            static_cast<DWORD>(CW_USEDEFAULT),
            static_cast<DWORD>(CW_USEDEFAULT),
            0, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr
        };
        startupInfo.dwFlags = STARTF_USESHOWWINDOW;
        startupInfo.wShowWindow = SW_HIDE;
        bool success = CreateProcess(
                WPMUtils::toLPWSTR(exe),
                WPMUtils::toLPWSTR(args),
                nullptr, nullptr, TRUE,
                CREATE_UNICODE_ENVIRONMENT, nullptr,
                nullptr, &startupInfo, &pinfo);

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

    DBRepository* dbr = DBRepository::getDefault();
    err = dbr->openDefault();

    if (err.isEmpty()) {
        err = InstalledPackages::getDefault()->readRegistryDatabase();
    }

    QList<PackageVersion*> toInstall =
            PackageUtils::getAddPackageVersionOptions(*dbr, cl, &err);

    DWORD pct = WPMUtils::CLOSE_WINDOW;
    if (err.isEmpty()) {
        pct = WPMUtils::getProgramCloseType(cl, &err);
    }

    // debug: WPMUtils::outputTextConsole << "Versions: " << d.toString()) << std::endl;
    QList<InstallOperation*> ops;

    InstalledPackages installed(*InstalledPackages::getDefault());

    if (err.isEmpty()) {
        err = dbr->planAddMissingDeps(installed, ops);
    }

    if (err.isEmpty()) {
        QList<PackageVersion*> avoid;
        for (int i = 0; i < toInstall.size(); i++) {
            PackageVersion* pv = toInstall.at(i);
            err = pv->planInstallation(dbr, installed, ops, avoid);
            if (!err.isEmpty())
                break;
        }
    }

    // debug: WPMUtils::outputTextConsole(QString("%1\r\n").arg(ops.size()));

    if (err.isEmpty()) {
        err = process(ops, pct);
    }

    qDeleteAll(ops);
    qDeleteAll(toInstall);

    return err;
}

QString CLProcessor::checkForUpdates()
{
    DBRepository* r = DBRepository::getDefault();

    QString err = r->openDefault();

    if (err.isEmpty()) {
        err = InstalledPackages::getDefault()->readRegistryDatabase();
    }

    QList<QUrl*> urls;
    if (err.isEmpty()) {
        QString err;
        urls = PackageUtils::getRepositoryURLs(&err);
        if (!err.isEmpty())
            err = QObject::tr("Cannot load the list of repositories: %1").arg(err);
    }

    Job job;
    if (err.isEmpty()) {
        r->clearAndDownloadRepositories(&job, urls, true, "", "", "", "", false);

        QStringList packages = r->findPackages(Package::UPDATEABLE, Package::NOT_INSTALLED_NOT_AVAILABLE, "", -1, -1, &err);
        int nupdates = packages.count();

        if (job.getErrorMessage().isEmpty() && nupdates > 0) {
            QApplication* app = (QApplication*) QApplication::instance();
            app->setQuitOnLastWindowClosed(false);

            MainWindow w;
            w.showIconInSystemTray(nupdates);
            app->exec();
        }
    }

    qDeleteAll(urls);
    urls.clear();

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

    DWORD programCloseType = WPMUtils::CLOSE_WINDOW;
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
            QString err;
            Package* p = rep->findOnePackage(package, &err);
            if (p)
                toUpdate.append(p);
            else
                job->setErrorMessage(err);

            if (!job->shouldProceed())
                break;
        }
    }

    QList<InstallOperation*> ops;
    InstalledPackages installed(*InstalledPackages::getDefault());

    if (job->shouldProceed()) {
        err = DBRepository::getDefault()->planAddMissingDeps(installed, ops);
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }

    bool up2date = false;
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.12, QObject::tr("Planning"));
        QString err = rep->planUpdates(installed,
                toUpdate, QList<Dependency*>(), ops);
        if (!err.isEmpty())
            job->setErrorMessage(err);
        else {
            sub->completeWithProgress();
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

    if (job->shouldProceed() && !up2date) {
        err = process(ops, programCloseType);
    }

    /*
    if (job->shouldProceed(QObject::tr("Updating")) && !up2date) {
        Job* ijob = job->newSubJob(0.86);
        rep->process(ijob, ops, programCloseType);
        if (!ijob->getErrorMessage().isEmpty()) {
            job->setErrorMessage(QObject::tr("Error updating: %1").
                    arg(ijob->getErrorMessage()));
        }
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
                QObject::tr("The packages are already up-to-date\r\n"));
    } else if (r.isEmpty()) {
        WPMUtils::outputTextConsole(
                QObject::tr("The packages were updated successfully\r\n"));
    }
    */

    QString ret = job->getErrorMessage();

    delete job;

    qDeleteAll(toUpdate);

    return ret;
}

QString CLProcessor::process(QList<InstallOperation*> &install,
        DWORD programCloseType)
{
    QString err;

    bool confirmed = false;
    QString title;
    if (err.isEmpty())
        confirmed = UIUtils::confirmInstallOperations(nullptr, install, &title, &err);

    if (err.isEmpty()) {
        if (confirmed) {
            DBRepository* rep = DBRepository::getDefault();

            if (rep->includesRemoveItself(install)) {
                QString txt = QObject::tr("Chosen changes require an update of this Npackd instance. Are you sure?");
                if (UIUtils::confirm(nullptr, QObject::tr("Warning"), txt, txt)) {
                    Job* job = new Job(title);
                    UIUtils::processWithSelfUpdate(job, install,
                            programCloseType);
                    err = job->getErrorMessage();
                    delete job;
                }
            } else {
                Job* job = new Job(title);

                QtConcurrent::run(static_cast<AbstractRepository*>(rep),
                        &AbstractRepository::processWithCoInitializeAndFree,
                        job, install,
                        PackageUtils::getCloseProcessType());

                monitorAndWaitFor(job);
                err = job->getErrorMessage();

                install.clear();
            }
        }
    }

    if (!err.isEmpty()) {
        QMessageBox::critical(nullptr, QObject::tr("Error"), err);
    }

    qDeleteAll(install);
    install.clear();

    return err;
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
    for (int i = 0; i < static_cast<int>(sizeof(lines) / sizeof(lines[0])); i++) {
        sl.append(QString(lines[i]));
    }

    QStringList opts = this->cl.printOptions();
    sl.append(opts);

    const char* lines2[] = {
        "",
        "The process exits with the code unequal to 0 if an error occures."
        //"",
        // "See https://github.com/tim-lebedkov/npackd/wiki/CommandLine for more details.",
    };
    for (int i = 0; i < static_cast<int>(sizeof(lines2) / sizeof(lines2[0])); i++) {
        sl.append(QString(lines2[i]));
    }

    QMessageBox mb(nullptr);
    mb.setWindowTitle(QString(
            "Npackd %1\r\n").arg(NPACKD_VERSION));
    mb.setText("npackdg [command] [options]\r\n"
            "Here is [command] one of add, remove, rm or help");
    mb.setIcon(QMessageBox::Information);
    mb.setStandardButtons(QMessageBox::Ok);
    mb.setDefaultButton(QMessageBox::Ok);
    mb.setDetailedText(sl.join("\r\n"));
    mb.exec();
}

bool CLProcessor::process(int argc, char *argv[], int* errorCode)
{
    *errorCode = 0;

    bool ret = true;

    QApplication app(argc, argv);

    QTranslator myappTranslator;
    bool r = myappTranslator.load(
            "npackdg_" + QLocale::system().name(),
            ":/translations");
    if (r) {
        app.installTranslator(&myappTranslator);
    }

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

    QList<CommandLine::ParsedOption*> options = cl.getParsedOptions();

    QString cmd;
    if (options.size() > 0 && options.at(0)->opt == nullptr) {
        cmd = options.at(0)->value;
    }

    if (!commandLineParsingError.isEmpty()) {
        QString msg = QObject::tr("Error parsing the command line: %1").
                arg(commandLineParsingError);
        QMessageBox::critical(nullptr, QObject::tr("Error"), msg);
        *errorCode = 1;
    } else if (options.count() == 0) {
        MainWindow w;

        w.prepare();
        w.show();
        *errorCode = QApplication::exec();
    } else if (!cmd.isEmpty()) {
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
        } else if (cmd == "check-for-updates") {
            err = checkForUpdates();
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
            QMessageBox::critical(nullptr, QObject::tr("Error"), err);
            *errorCode = 1;
        }
    } else {
        QString err = QObject::tr("Unexpected argument: %1").
                arg(options.at(0)->opt->name);
        QMessageBox::critical(nullptr, QObject::tr("Error"), err);
        *errorCode = 1;
    }

    return ret;
}
