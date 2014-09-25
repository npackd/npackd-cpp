#include <shlobj.h>

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

    WindowsRegistry wr;
    QString err = wr.open(HKEY_LOCAL_MACHINE,
            "Software\\Npackd\\Npackd\\InstallationDirs", false, KEY_READ);
    QStringList dirs;
    if (err.isEmpty()) {
        dirs = wr.loadStringList(&err);
    }

    dirs.append(WPMUtils::getInstallationDirectory());
    dirs.append(WPMUtils::getProgramFilesDir());
    if (WPMUtils::is64BitWindows())
        dirs.append(WPMUtils::getShellDir(CSIDL_PROGRAM_FILESX86));

    dirs.removeDuplicates();
    this->ui->comboBoxDir->addItems(dirs);
}

SettingsFrame::~SettingsFrame()
{
    delete ui;
}

QStringList SettingsFrame::getRepositoryURLs()
{
    QString txt = this->ui->plainTextEditReps->toPlainText().trimmed();
    QStringList sl = txt.split("\n", QString::SkipEmptyParts);
    for (int i = 0; i < sl.count(); i++) {
        sl[i] = sl.at(i).trimmed();
    }
    return sl;
}

QString SettingsFrame::getInstallationDirectory()
{
    return this->ui->comboBoxDir->currentText();
}

void SettingsFrame::setInstallationDirectory(const QString& dir)
{
    this->ui->comboBoxDir->setEditText(dir);
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

    if (err.isEmpty()) {
        err = WPMUtils::checkInstallationDirectory(getInstallationDirectory());
    }

    QList<QUrl*> urls;
    if (err.isEmpty()) {
        for (int i = 0; i < list.count(); i++) {
            QUrl* url = new QUrl(list.at(i));
            urls.append(url);
            if (!url->isValid()) {
                err = QString(QObject::tr("%1 is not a valid repository address")).arg(
                        list.at(i));
                break;
            }
        }
    }

    if (err.isEmpty()) {
        WPMUtils::setInstallationDirectory(getInstallationDirectory());
        WPMUtils::setCloseProcessType(getCloseProcessType());
    }

    bool repsChanged = false;

    if (err.isEmpty()) {
        QList<QUrl*> oldList = Repository::getRepositoryURLs(&err);
        repsChanged = oldList.count() != urls.count();
        if (!repsChanged) {
            for (int i = 0; i < oldList.count(); i++) {
                if ((*oldList.at(i)) != (*urls.at(i))) {
                    repsChanged = true;
                    break;
                }
            }
        }
        qDeleteAll(oldList);
        oldList.clear();
    }

    if (err.isEmpty()) {
        if (repsChanged) {
            if (mw->reloadRepositoriesThreadRunning) {
                err = QObject::tr("Cannot change settings now. The repositories download is running.");
            } else {
                Repository::setRepositoryURLs(urls, &err);
                if (err.isEmpty()) {
                    mw->closeDetailTabs();
                    mw->recognizeAndLoadRepositories(false);
                }
            }
        }
    }

    qDeleteAll(urls);
    urls.clear();

    if (err.isEmpty()) {
        WindowsRegistry m(HKEY_LOCAL_MACHINE, false, KEY_ALL_ACCESS);
        WindowsRegistry wr = m.createSubKey(
                "Software\\Npackd\\Npackd\\InstallationDirs", &err,
                KEY_ALL_ACCESS);

        QStringList dirs;
        for (int i = 0; i < this->ui->comboBoxDir->count(); i++)
            dirs.append(this->ui->comboBoxDir->itemText(i));
        if (err.isEmpty()) {
            wr.saveStringList(dirs);
        }

        // it is not important, whether the list of directories is saved or not
        err = "";
    }

    if (!err.isEmpty())
        mw->addErrorMessage(err, err, true, QMessageBox::Critical);
}
