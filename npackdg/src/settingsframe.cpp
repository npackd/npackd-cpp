#include <shlobj.h>
#include <taskschd.h>

#include "settingsframe.h"
#include "ui_settingsframe.h"

#include <QApplication>
#include <QPushButton>
#include <QFileDialog>

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
        WPMUtils::adminMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            "Software\\Npackd\\Npackd\\InstallationDirs", false, KEY_READ);
    QStringList dirs;
    if (err.isEmpty()) {
        dirs = wr.loadStringList(&err);
    }
	dirs.append(WPMUtils::getShellDir(CSIDL_APPDATA) +
		QStringLiteral("\\Npackd\\Installation"));

    dirs.append(WPMUtils::getInstallationDirectory());
    if (WPMUtils::adminMode) {
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

    wr.close();

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
		}
	}

    IRegisteredTask* t = WPMUtils::findTask(&err);
    if (t) {
        VARIANT_BOOL enabled;
        HRESULT hr = t->get_Enabled(&enabled);
        if (SUCCEEDED(hr)) {
            ui->checkBoxAutomaticUpdates->setChecked(enabled);
        }
        t->Release();
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

    for (int i = 0; i < sl.size(); ) {
        if (sl.at(i).trimmed().startsWith("#"))
            sl.removeAt(i);
        else
            i++;
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
    QStringList texts;

    texts = urls;

    QStringList comments;
    QString err;
    WindowsRegistry wr;
    err = wr.open(WPMUtils::adminMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            "Software\\Npackd\\Npackd\\UsedReps", false, KEY_READ);
    if (err.isEmpty())
        comments = wr.loadStringList(&err);

    for (int i = 0; i < comments.size(); i++) {
        texts.append("# " + comments.at(i));
    }

    this->ui->plainTextEditReps->setPlainText(texts.join("\r\n"));
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
    this->ui->checkBoxCtrlC->setChecked(v &
            WPMUtils::CTRL_C);
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
    if (this->ui->checkBoxCtrlC->isChecked())
        cpt |= WPMUtils::CTRL_C;
    return cpt;
}

void SettingsFrame::on_buttonBox_clicked(QAbstractButton */*button*/)
{
    MainWindow* mw = MainWindow::getInstance();

    QString err;
    PackageVersion* locked = PackageVersion::findLockedPackageVersion(&err);
    if (locked) {
        QString msg = QObject::tr("Cannot change settings now. The package %1 is locked by a currently running installation/removal.").
                arg(locked->toString());
        mw->addErrorMessage(msg, msg, true, QMessageBox::Critical);
        delete locked;
        return;
    }

    QStringList list = getRepositoryURLs();

    if (err.isEmpty() && list.count() == 0)
        err = QObject::tr("No repositories defined");

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
                    mw->recognizeAndLoadRepositories(true);
                }
            }
        }
    }

    if (err.isEmpty()) {
        WindowsRegistry m(
                WPMUtils::adminMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
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
                WPMUtils::adminMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
				false, KEY_ALL_ACCESS);
        WindowsRegistry wr = m.createSubKey(
                "Software\\Npackd\\Npackd\\UsedReps", &err,
                KEY_ALL_ACCESS);

        QStringList comments = this->ui->plainTextEditReps->toPlainText().
                trimmed().split("\n", QString::SkipEmptyParts);
        for (int i = 0; i < comments.size(); ) {
            QString c = comments.at(i).trimmed();
            if (!c.startsWith("#"))
                comments.removeAt(i);
            else {
                comments[i] = c.mid(1);
                i++;
            }
        }
        wr.saveStringList(comments);

        // it is not important, whether the list of used repositories
        // is saved or not
        err = "";
    }

    // test: scheduling a task
    //CoInitialize(NULL);
    if (err.isEmpty()) {
        err = WPMUtils::createMSTask(ui->checkBoxAutomaticUpdates->isChecked());
    }

    if (!err.isEmpty())
        mw->addErrorMessage(err, err, true, QMessageBox::Critical);

    qDeleteAll(urls);
    urls.clear();
}

void SettingsFrame::on_pushButton_clicked()
{
     QString dir = QFileDialog::getExistingDirectory(
            this, QObject::tr("Choose installation directory"),
            getInstallationDirectory(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    setInstallationDirectory(WPMUtils::normalizePath(dir));
}
