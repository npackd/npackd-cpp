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


