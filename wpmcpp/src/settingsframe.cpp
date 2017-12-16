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
    QString err = wr.open(
		WPMUtils::hasAdminPrivileges() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            "Software\\Npackd\\Npackd\\InstallationDirs", false, KEY_READ);
    QStringList dirs;
    if (err.isEmpty()) {
        dirs = wr.loadStringList(&err);
    }
	dirs.append(WPMUtils::getShellDir(CSIDL_APPDATA) +
		QStringLiteral("\\Npackd\\Installation"));

    dirs.append(WPMUtils::getInstallationDirectory());
	if (WPMUtils::hasAdminPrivileges()) {
		dirs.append(WPMUtils::getProgramFilesDir());
		if (WPMUtils::is64BitWindows())
			dirs.append(WPMUtils::getShellDir(CSIDL_PROGRAM_FILESX86));
	}

    // remove duplicates
    for (int i = 0; i < dirs.size(); ) {
        bool f = false;
        QString dirsi = WPMUtils::normalizePath(dirs.at(i));
        for (int j = 0; j < i; j++) {
            QString dirsj = WPMUtils::normalizePath(dirs.at(j));
            if (dirsi == dirsj) {
                f = true;
                break;
            }
        }

        if (f)
            dirs.removeAt(i);
        else
            i++;
    }

    this->ui->comboBoxDir->addItems(dirs);

    // used repositories
    wr.close();
    err = wr.open(
		WPMUtils::hasAdminPrivileges() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            "Software\\Npackd\\Npackd\\UsedReps", false, KEY_READ);
    QStringList usedReps;
    if (err.isEmpty()) {
        usedReps = wr.loadStringList(&err);
    }
    this->ui->comboBoxRep->addItems(usedReps);
    addUsedRepository("https://npackd.appspot.com/rep/xml?tag=stable");
    addUsedRepository("https://npackd.appspot.com/rep/xml?tag=stable64");
    addUsedRepository("https://npackd.appspot.com/rep/xml?tag=libs");
    addUsedRepository("https://npackd.appspot.com/rep/xml?tag=unstable");

	// translation bugfix
	ui->buttonBox->button(QDialogButtonBox::Apply)->setText(tr("Apply"));

	// detect group policy
	err = wr.open(HKEY_LOCAL_MACHINE,
		QStringLiteral("SOFTWARE\\Policies\\Npackd"), false, KEY_READ);
	if (err.isEmpty()) {
		wr.get(QStringLiteral("path"), &err);
		if (err.isEmpty()) this->ui->comboBoxDir->setEnabled(false);
		err.clear();
		wr.getDWORD("closeProcessType", &err);
		if (err.isEmpty()) this->ui->groupBox->setEnabled(false);
	}
	err = wr.open(HKEY_LOCAL_MACHINE,
		QStringLiteral("SOFTWARE\\Policies\\Npackd\\Reps"), false, KEY_READ);
	if (err.isEmpty()) {
		DWORD repCount = wr.getDWORD("size", &err);
		if (err.isEmpty() && repCount > 0) {
			this->ui->plainTextEditReps->setEnabled(false);
			this->ui->comboBoxRep->setEnabled(false);
			this->ui->pushButtonAddRep->setEnabled(false);
		}
	}
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

void SettingsFrame::addUsedRepository(const QString& repository)
{
    if (this->ui->comboBoxRep->findText(repository) < 0)
        this->ui->comboBoxRep->addItem(repository);
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

    for (int i = 0; i < urls.size(); i++) {
        addUsedRepository(urls.at(i));
    }
}

void SettingsFrame::on_buttonBox_accepted()
{
}

void SettingsFrame::setCloseProcessType(DWORD v)
{
    this->ui->checkBoxCloseWindows->setChecked(v &
            WPMUtils::CLOSE_WINDOW);
    this->ui->checkBoxDeleteFileShares->setChecked(v &
            WPMUtils::DISABLE_SHARES);
    this->ui->checkBoxKillProcesses->setChecked(v &
            WPMUtils::KILL_PROCESS);
    this->ui->checkBoxStopServices->setChecked(v &
            WPMUtils::STOP_SERVICES);
}

DWORD SettingsFrame::getCloseProcessType()
{
    DWORD cpt = 0;
    if (this->ui->checkBoxCloseWindows->isChecked())
        cpt |= WPMUtils::CLOSE_WINDOW;
    if (this->ui->checkBoxDeleteFileShares->isChecked())
        cpt |= WPMUtils::DISABLE_SHARES;
    if (this->ui->checkBoxKillProcesses->isChecked())
        cpt |= WPMUtils::KILL_PROCESS;
    if (this->ui->checkBoxStopServices->isChecked())
        cpt |= WPMUtils::STOP_SERVICES;
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
        QString msg(QObject::tr("Cannot change settings now. The package %1 is locked by a currently running installation/removal."));
        mw->addErrorMessage(msg.arg(locked->toString()));
        delete locked;
        return;
    }

    QStringList list = getRepositoryURLs();

    if (err.isEmpty() && list.count() == 0)
        err = QObject::tr("No repositories defined");

    if (err.isEmpty()) {
        for (int i = 0; i < list.size(); i++) {
            addUsedRepository(list.at(i));
        }
    }

    if (err.isEmpty()) {
        err = AbstractRepository::checkInstallationDirectory(getInstallationDirectory());
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
        WindowsRegistry m(
			WPMUtils::hasAdminPrivileges() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
				false, KEY_ALL_ACCESS);
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

    if (err.isEmpty()) {
        WindowsRegistry m(
			WPMUtils::hasAdminPrivileges() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
				false, KEY_ALL_ACCESS);
        WindowsRegistry wr = m.createSubKey(
                "Software\\Npackd\\Npackd\\UsedReps", &err,
                KEY_ALL_ACCESS);

        QStringList usedReps;
        for (int i = 0; i < this->ui->comboBoxRep->count(); i++)
            usedReps.append(this->ui->comboBoxRep->itemText(i));
        if (err.isEmpty()) {
            wr.saveStringList(usedReps);
        }

        // it is not important, whether the list of used repositories
        // is saved or not
        err = "";
    }

    if (!err.isEmpty())
        mw->addErrorMessage(err, err, true, QMessageBox::Critical);
}

void SettingsFrame::on_pushButtonAddRep_clicked()
{
    int index = this->ui->comboBoxRep->currentIndex();
    if (index >= 0) {
        QStringList urls = getRepositoryURLs();
        QString url = this->ui->comboBoxRep->itemText(index);
        if (urls.indexOf(url) < 0)
            this->ui->plainTextEditReps->appendPlainText(url);
    }
}
