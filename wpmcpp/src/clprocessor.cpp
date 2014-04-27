#include "clprocessor.h"

#include <QDialog>
#include <QVBoxLayout>

#include "progressframe.h"
#include "installedpackages.h"
#include "wpmutils.h"
#include "packageversion.h"
#include "abstractrepository.h"
#include "installoperation.h"
#include "uiutils.h"
#include "installthread.h"

CLProcessor::CLProcessor()
{
}

void CLProcessor::monitor(Job* job, const QString& title, QThread* thread)
{
    QDialog d;
    QVBoxLayout* layout = new QVBoxLayout();

    ProgressFrame* pf = new ProgressFrame(&d, job, title, thread);
    pf->resize(400, 100);
    layout->insertWidget(0, pf);

    d.setLayout(layout);

    d.exec();
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
        confirmed = UIUtils::confirmInstalOperations(0, ops, &err);

    if (err.isEmpty() && confirmed) {
        Job* job = new Job();
        InstallThread* it = new InstallThread(0, 1, job);
        it->install = ops;
        it->programCloseType = programCloseType;
        ops.clear();

        /* TODO
        connect(it, SIGNAL(finished()), this,
                SLOT(processThreadFinished()),
                Qt::QueuedConnection);
                */

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


