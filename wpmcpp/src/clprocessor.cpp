#include "clprocessor.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QMessageBox>

#include "progressframe.h"
#include "installedpackages.h"
#include "wpmutils.h"
#include "packageversion.h"
#include "abstractrepository.h"
#include "installoperation.h"
#include "uiutils.h"
#include "installthread.h"

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

QString CLProcessor::remove(const CommandLine& cl)
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

QString CLProcessor::add(const CommandLine& cl)
{
    QString err;

    err = DBRepository::getDefault()->openDefault();

    if (err.isEmpty()) {
        err = InstalledPackages::getDefault()->readRegistryDatabase();
    }

    QList<PackageVersion*> toInstall =
            WPMUtils::getPackageVersionOptions(cl, &err, true);

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
        // TODO: it->programCloseType = programCloseType;
        ops.clear();

        monitor(job, QObject::tr("Install"), it);
    }

    qDeleteAll(ops);
    qDeleteAll(toInstall);

    return err;
}

QString CLProcessor::update(const CommandLine& cl)
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

    /* TODO
    int programCloseType; // TODO = WPMUtils::CLOSE_WINDOW;
    if (job->shouldProceed()) {
        QString err;
        programCloseType = WPMUtils::getProgramCloseType(cl, &err);
        if (!err.isEmpty()) {
            job->setErrorMessage(err);
        }
    }
    */

    QStringList packages_ = cl.getAll("package");

    if (job->shouldProceed()) {
        if (packages_.size() == 0) {
            job->setErrorMessage("Missing option: --package");
        }
    }

    if (job->shouldProceed()) {
        for (int i = 0; i < packages_.size(); i++) {
            QString package = packages_.at(i);
            if (!Package::isValidName(package)) {
                job->setErrorMessage("Invalid package name: " + package);
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
                    job->setErrorMessage("Unknown package: " + package);
                } else if (packages.count() > 1) {
                    job->setErrorMessage("Ambiguous package name");
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
    if (job->shouldProceed("Planning")) {
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

    if (job->shouldProceed("Updating") && !up2date) {
        Job* sjob = new Job();
        InstallThread* it = new InstallThread(0, 1, sjob);
        it->install = ops;
        // TODO: it->programCloseType = programCloseType;
        ops.clear();

        monitor(sjob, QObject::tr("Install"), it);
        if (sjob->getErrorMessage().isEmpty())
            job->setErrorMessage(sjob->getErrorMessage());
    }

    /*
    if (job->shouldProceed("Updating") && !up2date) {
        Job* ijob = job->newSubJob(0.86);
        rep->process(ijob, ops, programCloseType);
        if (!ijob->getErrorMessage().isEmpty()) {
            job->setErrorMessage(QString("Error updating: %1").
                    arg(ijob->getErrorMessage()));
        }
        delete ijob;
    }
    */

    qDeleteAll(ops);

    job->complete();

    /* TODO
    QString r = job->getErrorMessage();
    if (job->isCancelled()) {
        r = "The package update was cancelled";
    } else if (up2date) {
        WPMUtils::outputTextConsole("The packages are already up-to-date\n");
    } else if (r.isEmpty()) {
        WPMUtils::outputTextConsole("The packages were updated successfully\n");
    }
    */

    QString ret = job->getErrorMessage();

    delete job;

    qDeleteAll(toUpdate);

    return ret;
}

void CLProcessor::usage()
{
    // TODO: i18n

    const char* lines[] = {
        "Usage:",
        "    npackdcl help",
        "        prints this help",
        "    npackdcl add (--package=<package> [--version=<version>])+",
        "        installs packages. The newest available version will be installed, ",
        "        if none is specified.",
        "        Short package names can be used here",
        "        (e.g. App instead of com.example.App)",
        "    npackdcl remove|rm (--package=<package> [--version=<version>])+",
        "           [--end-process=<types>]",
        "        removes packages. The version number may be omitted, ",
        "        if only one is installed.",
        "        Short package names can be used here",
        "        (e.g. App instead of com.example.App)",
        "    npackdcl update (--package=<package>)+ [--end-process=<types>]",
        "        updates packages by uninstalling the currently installed",
        "        and installing the newest version. ",
        "        Short package names can be used here",
        "        (e.g. App instead of com.example.App)",
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
        "Options:",
    };
    QStringList sl;
    for (int i = 0; i < (int) (sizeof(lines) / sizeof(lines[0])); i++) {
        sl.append(QString(lines[i]));
    }
    // TODO: this->cl.printOptions();
    const char* lines2[] = {
        "",
        "The process exits with the code unequal to 0 if an error occures."
        //"",
        // TODO:"See https://code.google.com/p/windows-package-manager/wiki/CommandLine for more details.",
    };
    for (int i = 0; i < (int) (sizeof(lines2) / sizeof(lines2[0])); i++) {
        sl.append(QString(lines2[i]));
    }

    QMessageBox mb(0);
    mb.setWindowTitle(QString(
            "Npackd %1\n").arg(NPACKD_VERSION));
    mb.setText("npackdg [command]"); // TODO
    mb.setIcon(QMessageBox::Warning);
    mb.setStandardButtons(QMessageBox::Ok);
    mb.setDefaultButton(QMessageBox::Ok);
    mb.setDetailedText(sl.join("\r\n"));
    mb.exec();
}
