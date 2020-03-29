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
#include "packageutils.h"
#include "repositoriesitemmodel.h"

SettingsFrame::SettingsFrame(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::SettingsFrame)
{
    ui->setupUi(this);

    WindowsRegistry wr;
    QString err = wr.open(
        PackageUtils::globalMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            "Software\\Npackd\\Npackd\\InstallationDirs", false, KEY_READ);
    QStringList dirs;
    if (err.isEmpty()) {
        dirs = wr.loadStringList(&err);
    }
	dirs.append(WPMUtils::getShellDir(CSIDL_APPDATA) +
		QStringLiteral("\\Npackd\\Installation"));

    dirs.append(PackageUtils::getInstallationDirectory());
    if (PackageUtils::globalMode) {
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
            this->ui->tableViewReps->setEnabled(false);
		}
	}

    ui->checkBoxCheckForUpdates->setChecked(WPMUtils::isTaskEnabled(&err));

    // table with URLs
    QTableView* t = this->ui->tableViewReps;
    QItemSelectionModel *sm = t->selectionModel();
    QAbstractItemModel* m = t->model();
    t->setModel(new RepositoriesItemModel());
    delete sm;
    delete m;

    t->setTabKeyNavigation(false);
    t->setTextElideMode(Qt::ElideRight);
    t->setSortingEnabled(false);
    t->horizontalHeader()->setSectionsMovable(true);

    t->setColumnWidth(0, 80);
    t->setColumnWidth(1, 350);
    t->setColumnWidth(2, 300);
}

SettingsFrame::~SettingsFrame()
{
    delete ui;
}

QString SettingsFrame::getInstallationDirectory()
{
    return this->ui->comboBoxDir->currentText();
}

void SettingsFrame::setInstallationDirectory(const QString& dir)
{
    this->ui->comboBoxDir->setEditText(dir);
}

void SettingsFrame::setRepositoryURLs(const QStringList &urls, const QStringList& comments)
{
    QString err;
    QTableView* t = this->ui->tableViewReps;
    RepositoriesItemModel* m = static_cast<RepositoriesItemModel*>(t->model());
    QList<RepositoriesItemModel::Entry*> entries;
    for (int i = 0; i < urls.size(); i++) {
        RepositoriesItemModel::Entry* e = new RepositoriesItemModel::Entry();
        e->enabled = true;
        e->url = urls.at(i);
        e->comment = comments.at(i);
        entries.append(e);
    }
    m->setURLs(entries);
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

void SettingsFrame::on_buttonBox_clicked(QAbstractButton* /*button*/)
{
    MainWindow* mw = MainWindow::getInstance();
    DBRepository* dbr = DBRepository::getDefault();

    QString err;

    QString lockedPackage;
    Version lockedVersion;
    PackageVersion::findLockedPackageVersion(&lockedPackage, &lockedVersion);
    if (!lockedPackage.isEmpty()) {
        PackageVersion* locked = dbr->findPackageVersion_(lockedPackage, lockedVersion, &err);
        QString name;
        if (locked) {
            name = locked->toString();
            delete locked;
        } else {
            name = lockedPackage + " " + lockedVersion.getVersionString();
        }

        QString msg = QObject::tr("Cannot change settings now. The package %1 is locked by a currently running installation/removal.").
                arg(name);
        mw->addErrorMessage(msg, msg, true, QMessageBox::Critical);
        return;
    }

    QTableView* t = this->ui->tableViewReps;
    RepositoriesItemModel* m = static_cast<RepositoriesItemModel*>(t->model());
    QList<RepositoriesItemModel::Entry*> entries = m->getEntries();

    QStringList uiReps;
    QStringList uiComments;
    for (int i = 0; i < entries.count(); i++) {
        RepositoriesItemModel::Entry* e = entries.at(i);
        if (e->enabled) {
            uiReps.append(e->url.trimmed());
            uiComments.append(e->comment.trimmed());
        }
    }

    if (err.isEmpty() && uiReps.count() == 0)
        err = QObject::tr("No repositories defined");

    if (err.isEmpty()) {
        err = dbr->checkInstallationDirectory(getInstallationDirectory());
    }

    if (err.isEmpty()) {
        for (int i = 0; i < uiReps.count(); i++) {
            QUrl url(uiReps.at(i));
            if (!url.isValid()) {
                err = QString(QObject::tr("%1 is not a valid repository address")).arg(
                        uiReps.at(i));
                break;
            }
        }
    }

    if (err.isEmpty()) {
        PackageUtils::setInstallationDirectory(getInstallationDirectory());
        PackageUtils::setCloseProcessType(getCloseProcessType());
    }

    bool repsChanged = false;

    if (err.isEmpty()) {
        QStringList reps, comments;
        std::tie(reps, comments, err) = PackageUtils::getRepositoryURLsAndComments();
        repsChanged = (reps != uiReps) || (comments != uiComments);
    }

    if (err.isEmpty()) {
        if (repsChanged) {
            if (mw->reloadRepositoriesThreadRunning) {
                err = QObject::tr("Cannot change settings now. The repositories download is running.");
            } else {
                err = PackageUtils::setRepositoryURLsAndComments(uiReps, uiComments);
                if (err.isEmpty()) {
                    mw->closeDetailTabs();
                    mw->recognizeAndLoadRepositories(true);
                }
            }
        }
    }

    if (err.isEmpty()) {
        WindowsRegistry m(
                PackageUtils::globalMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
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

    // test: scheduling a task
    //CoInitialize(NULL);
    if (err.isEmpty()) {
        err = WPMUtils::createMSTask(ui->checkBoxCheckForUpdates->isChecked());
    }

    if (!err.isEmpty())
        mw->addErrorMessage(err, err, true, QMessageBox::Critical);
}

void SettingsFrame::on_pushButton_clicked()
{
     QString dir = QFileDialog::getExistingDirectory(
            this, QObject::tr("Choose installation directory"),
            getInstallationDirectory(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
     if (!dir.isEmpty())
         setInstallationDirectory(WPMUtils::normalizePath(dir));
}

void SettingsFrame::on_pushButtonProxySettings_clicked()
{
    Job* job = new Job();
    QString d = WPMUtils::getWindowsDir();

    WPMUtils::executeFile(job, d, d + "\\System32\\rundll32.exe",
            "inetcpl.cpl,LaunchConnectionDialog", nullptr,
            QStringList(), false, true, false);
    if (!job->getErrorMessage().isEmpty())
        MainWindow::getInstance()->addErrorMessage(job->getErrorMessage());
    delete job;
}

void SettingsFrame::on_pushButtonAddRep_clicked()
{
    QTableView* t = this->ui->tableViewReps;
    RepositoriesItemModel* m = static_cast<RepositoriesItemModel*>(t->model());
    m->add();
}

void SettingsFrame::on_pushButtonRemoveRep_clicked()
{
    QTableView* t = this->ui->tableViewReps;
    QModelIndexList sel = t->selectionModel()->selectedIndexes();
    if (sel.size() > 0) {
        RepositoriesItemModel* m = static_cast<RepositoriesItemModel*>(t->model());
        m->remove(sel.at(0).row());
    }
}
