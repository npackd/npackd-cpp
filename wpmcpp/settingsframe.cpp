#include "settingsframe.h"
#include "ui_settingsframe.h"

#include <QApplication>

#include "repository.h"
#include "mainwindow.h"
#include "wpmutils.h"

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
    if (list.count() == 0) {
        QString msg(QObject::tr("No repositories defined"));
        mw->addErrorMessage(msg, msg, true, QMessageBox::Critical);
    } else if (getInstallationDirectory().isEmpty()) {
        QString msg(QObject::tr("The installation directory cannot be empty"));
        mw->addErrorMessage(msg, msg, true, QMessageBox::Critical);
    } else if (!QDir(getInstallationDirectory()).exists()) {
        QString msg(QObject::tr("The installation directory does not exist"));
        mw->addErrorMessage(msg, msg, true, QMessageBox::Critical);
    } else {
        QString err;
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
            if (err.isEmpty()) {
                mw->closeDetailTabs();
                mw->recognizeAndLoadRepositories(false);
            } else {
                mw->addErrorMessage(err, err, true, QMessageBox::Critical);
            }
        } else {
            mw->addErrorMessage(err, err, true, QMessageBox::Critical);
        }
        qDeleteAll(urls);
        urls.clear();
    }
}
