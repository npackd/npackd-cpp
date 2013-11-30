#include "settingsframe.h"
#include "ui_settingsframe.h"

#include <QApplication>

#include "repository.h"
#include "mainwindow.h"
#include "wpmutils.h"
#include "installedpackages.h"

SettingsFrame::SettingsFrame(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::SettingsFrame)
{
    ui->setupUi(this);
}

SettingsFrame::~SettingsFrame()
{
    delete ui;
}

QStringList SettingsFrame::getRepositoryURLs()
{
    QString txt = this->ui->plainTextEditReps->toPlainText().trimmed();
    QStringList sl = txt.split("\n", QString::SkipEmptyParts);
    return sl;
}

QString SettingsFrame::getInstallationDirectory()
{
    return this->ui->lineEditDir->text();
}

void SettingsFrame::setInstallationDirectory(const QString& dir)
{
    this->ui->lineEditDir->setText(dir);
}

void SettingsFrame::setRepositoryURLs(const QStringList &urls)
{
    this->ui->plainTextEditReps->setPlainText(urls.join("\r\n"));
}

void SettingsFrame::on_buttonBox_accepted()
{
}

void SettingsFrame::setCloseProcessType(DWORD v)
{
    this->ui->checkBoxCloseWindows->setChecked(v &
            WPMUtils::CLOSE_WINDOW);
    this->ui->checkBoxKillProcesses->setChecked(v &
            WPMUtils::KILL_PROCESS);
}

DWORD SettingsFrame::getCloseProcessType()
{
    DWORD cpt = 0;
    if (this->ui->checkBoxCloseWindows->isChecked())
        cpt |= WPMUtils::CLOSE_WINDOW;
    if (this->ui->checkBoxKillProcesses->isChecked())
        cpt |= WPMUtils::KILL_PROCESS;
    return cpt;
}

void SettingsFrame::on_buttonBox_clicked(QAbstractButton *button)
{
    MainWindow* mw = MainWindow::getInstance();

    if (mw->hardDriveScanRunning) {
        mw->addErrorMessage(QObject::tr("Cannot change settings now. The hard drive scan is running."));
        return;
    }

    if (mw->reloadRepositoriesThreadRunning) {
        mw->addErrorMessage(QObject::tr("Cannot change settings now. The repositories download is running."));
        return;
    }

    QString err;
    PackageVersion* locked = PackageVersion::findLockedPackageVersion(&err);
    if (locked) {
        err = QObject::tr("Cannot find locked package versions: %1").
                arg(err);
        mw->addErrorMessage(err);
        delete locked;
        return;
    }

    if (locked) {
        QString msg(QObject::tr("Cannot change settings now. The package %1 is locked by a currently running installation/removal."));
        mw->addErrorMessage(msg.arg(locked->toString()));
        delete locked;
        return;
    }

    QStringList list = getRepositoryURLs();

    if (err.isEmpty() && list.count() == 0)
        err = QObject::tr("No repositories defined");

    if (err.isEmpty() && getInstallationDirectory().isEmpty())
        err = QObject::tr("The installation directory cannot be empty");

    if (err.isEmpty() && !QDir(getInstallationDirectory()).exists())
        err = QObject::tr("The installation directory does not exist");

    if (err.isEmpty()) {
        InstalledPackages* ip = InstalledPackages::getDefault();
        InstalledPackageVersion* ipv = ip->findOwner(
                getInstallationDirectory());
        if (ipv) {
            AbstractRepository* r = AbstractRepository::getDefault_();
            err = QObject::tr("Cannot change the installation directory to %1. %2 %3 is installed there").arg(
                    getInstallationDirectory()).
                    arg(r->getPackageTitleAndName(ipv->package)).
                    arg(ipv->version.getVersionString());
            delete ipv;
        }
    }

    if (err.isEmpty()) {
        QList<QUrl*> urls;
        for (int i = 0; i < list.count(); i++) {
            QUrl* url = new QUrl(list.at(i));
            urls.append(url);
            if (!url->isValid()) {
                err = QString(QObject::tr("%1 is not a valid repository address")).arg(
                        list.at(i));
                break;
            }
        }

        if (err.isEmpty()) {
            WPMUtils::setInstallationDirectory(getInstallationDirectory());
            Repository::setRepositoryURLs(urls, &err);
            WPMUtils::setCloseProcessType(getCloseProcessType());
            if (err.isEmpty()) {
                mw->closeDetailTabs();
                mw->recognizeAndLoadRepositories(false);
            }
        }
        qDeleteAll(urls);
        urls.clear();
    }

    if (!err.isEmpty())
        mw->addErrorMessage(err, err, true, QMessageBox::Critical);
}
