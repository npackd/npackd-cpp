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

void SettingsFrame::updateActions()
{
    QTableView* t = this->ui->tableViewReps;
    QModelIndexList sel = t->selectionModel()->selectedIndexes();
    RepositoriesItemModel* m = static_cast<RepositoriesItemModel*>(t->model());

    int row = -1;
    if (sel.size() > 0)
        row = sel.at(0).row();

    this->ui->pushButtonRemoveRep->setEnabled(row >= 0);
    this->ui->pushButtonMoveUp->setEnabled(row > 0);
    this->ui->pushButtonMoveDown->setEnabled(row < m->rowCount(QModelIndex()) - 1);
}

SettingsFrame::SettingsFrame(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::SettingsFrame)
{
    ui->setupUi(this);

    WindowsRegistry wr;
    QString err = wr.open(
        PackageUtils::globalMode ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            "Software\\Npackd\\Npackd\\InstallationDirs", false, KEY_READ);
    std::vector<QString> dirs;
    if (err.isEmpty()) {
        dirs = wr.loadStringList(&err);
    }
    dirs.push_back(WPMUtils::getShellDir(FOLDERID_RoamingAppData) +
		QStringLiteral("\\Npackd\\Installation"));

    dirs.push_back(PackageUtils::getInstallationDirectory());
    if (PackageUtils::globalMode) {
        dirs.push_back(WPMUtils::getProgramFilesDir());
		if (WPMUtils::is64BitWindows())
            dirs.push_back(WPMUtils::getShellDir(FOLDERID_ProgramFilesX86));
	}

    // remove duplicates
    for (int i = 0; i < static_cast<int>(dirs.size()); ) {
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
            dirs.erase(dirs.begin() + i);
        else
            i++;
    }

    this->ui->comboBoxDir->addItems(WPMUtils::toQStringList(dirs));

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
    t->setColumnWidth(2, 600);

    connect(t->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this,
            SLOT(tableViewReps_selectionChanged()));
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

void SettingsFrame::fillRepositories()
{
    QTableView* t = this->ui->tableViewReps;
    RepositoriesItemModel* m = static_cast<RepositoriesItemModel*>(t->model());
    std::vector<RepositoriesItemModel::Entry*> entries;

    QString err;
    std::vector<QString> urls, comments;

    std::tie(urls, comments, err) = PackageUtils::getRepositoryURLsAndComments(false);
    for (int i = 0; i < static_cast<int>(urls.size()); i++) {
        RepositoriesItemModel::Entry* e = new RepositoriesItemModel::Entry();
        e->enabled = false;
        e->url = urls.at(i);
        e->comment = comments.at(i);
        entries.push_back(e);
    }

    std::tie(urls, comments, err) = PackageUtils::getRepositoryURLsAndComments();
    for (int i = 0; i < static_cast<int>(urls.size()); i++) {
        RepositoriesItemModel::Entry* e = new RepositoriesItemModel::Entry();
        e->enabled = true;
        e->url = urls.at(i);
        e->comment = comments.at(i);
        entries.push_back(e);
    }

    m->setURLs(entries);

    updateActions();
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
    std::vector<RepositoriesItemModel::Entry*> entries = m->getEntries();

    std::vector<QString> uiReps;
    std::vector<QString> uiComments;
    std::vector<QString> uiUnusedReps;
    std::vector<QString> uiUnusedComments;
    for (auto e: entries) {
        if (e->enabled) {
            uiReps.push_back(e->url.trimmed());
            uiComments.push_back(e->comment.trimmed());
        } else {
            uiUnusedReps.push_back(e->url.trimmed());
            uiUnusedComments.push_back(e->comment.trimmed());
        }
    }

    if (err.isEmpty() && uiReps.size() == 0)
        err = QObject::tr("No repositories defined");

    if (err.isEmpty()) {
        err = dbr->checkInstallationDirectory(getInstallationDirectory());
    }

    if (err.isEmpty()) {
        for (auto& uiRep: uiReps) {
            QUrl url(uiRep);
            if (!url.isValid()) {
                err = QString(QObject::tr("%1 is not a valid repository address")).arg(
                        uiRep);
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
        std::vector<QString> reps, comments;
        std::vector<QString> unusedReps, unusedComments;
        std::tie(reps, comments, err) = PackageUtils::getRepositoryURLsAndComments();
        std::tie(unusedReps, unusedComments, err) = PackageUtils::getRepositoryURLsAndComments(false);
        repsChanged = (reps != uiReps) || (comments != uiComments) ||
                (unusedReps != uiUnusedReps) || (unusedComments != uiUnusedComments);
    }

    if (err.isEmpty()) {
        if (repsChanged) {
            if (mw->reloadRepositoriesThreadRunning) {
                err = QObject::tr("Cannot change settings now. The repositories download is running.");
            } else {
                err = PackageUtils::setRepositoryURLsAndComments(uiReps, uiComments);
                if (err.isEmpty())
                    err = PackageUtils::setRepositoryURLsAndComments(uiUnusedReps, uiUnusedComments, false);
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

        std::vector<QString> dirs;
        for (int i = 0; i < this->ui->comboBoxDir->count(); i++)
            dirs.push_back(this->ui->comboBoxDir->itemText(i));
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

    // for Windows 10 "start ms-settings:network-proxy" shows the newer dialog
    WPMUtils::executeFile(job, d, d + "\\System32\\rundll32.exe",
            "inetcpl.cpl,LaunchConnectionDialog", nullptr,
            std::vector<QString>(), false, true, false);
    if (!job->getErrorMessage().isEmpty())
        MainWindow::getInstance()->addErrorMessage(job->getErrorMessage());
    delete job;
}

void SettingsFrame::on_pushButtonAddRep_clicked()
{
    QTableView* t = this->ui->tableViewReps;
    RepositoriesItemModel* m = static_cast<RepositoriesItemModel*>(t->model());
    m->add();
    updateActions();
}

void SettingsFrame::on_pushButtonRemoveRep_clicked()
{
    QTableView* t = this->ui->tableViewReps;
    QModelIndexList sel = t->selectionModel()->selectedIndexes();
    if (sel.size() > 0) {
        RepositoriesItemModel* m = static_cast<RepositoriesItemModel*>(t->model());
        m->remove(sel.at(0).row());
        updateActions();
    }
}

void SettingsFrame::on_pushButtonMoveUp_clicked()
{
    QTableView* t = this->ui->tableViewReps;
    QModelIndexList sel = t->selectionModel()->selectedIndexes();
    if (sel.size() > 0) {
        RepositoriesItemModel* m = static_cast<RepositoriesItemModel*>(t->model());
        int row = sel.at(0).row();
        if (row > 0) {
            m->moveRow(QModelIndex(), row, QModelIndex(), row - 1);
            updateActions();
        }
    }
}

void SettingsFrame::on_pushButtonMoveDown_clicked()
{
    QTableView* t = this->ui->tableViewReps;
    QModelIndexList sel = t->selectionModel()->selectedIndexes();
    if (sel.size() > 0) {
        RepositoriesItemModel* m = static_cast<RepositoriesItemModel*>(t->model());
        int row = sel.at(0).row();
        if (row < m->rowCount(QModelIndex()) - 1) {
            m->moveRow(QModelIndex(), row, QModelIndex(), row + 2);
            updateActions();
        }
    }
}

void SettingsFrame::tableViewReps_selectionChanged()
{
    updateActions();
}
