#include <math.h>
#include <stdint.h>

#include <qabstractitemview.h>
#include <qmessagebox.h>
#include <qvariant.h>
#include <qprogressdialog.h>
#include <qwaitcondition.h>
#include <qthread.h>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdint.h>

#include <QApplication>
#include <QTimer>
#include <QDebug>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QRegExp>
#include <QDateTime>
#include <QDesktopServices>
#include <qinputdialog.h>
#include <qfiledialog.h>
#include <QTextStream>
#include <QIODevice>
#include <QMenu>
#include <QTextEdit>
#include <QScrollArea>
#include <QPushButton>
#include <QCloseEvent>
#include <QTextBrowser>
#include <QTableWidget>
#include <QDebug>
#include <QLabel>
#include <QDockWidget>
#include <QTreeWidget>
#include <QtConcurrent/QtConcurrentRun>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "repository.h"
#include "job.h"
#include "wpmutils.h"
#include "installoperation.h"
#include "downloader.h"
#include "packageversionform.h"
#include "uiutils.h"
#include "progresstree2.h"
#include "messageframe.h"
#include "settingsframe.h"
#include "licenseform.h"
#include "packageframe.h"
#include "hrtimer.h"
#include "dbrepository.h"
#include "packageitemmodel.h"
#include "installedpackages.h"
#include "flowlayout.h"
#include "scandiskthirdpartypm.h"
#include "installthread.h"
#include "scanharddrivesthread.h"
#include "visiblejobs.h"
#include "progresstree2.h"

extern HWND defaultPasswordWindow;

QIcon MainWindow::genericAppIcon;
QIcon MainWindow::waitAppIcon;
MainWindow* MainWindow::instance = 0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    instance = this;

    ui->setupUi(this);

    this->setMenuAccelerators();
    this->setActionAccelerators(this);

    this->taskbarMessageId = 0;

    this->pt = 0;
    this->jobsTab = 0;
    this->taskbarInterface = 0;

    this->hardDriveScanRunning = false;
    this->reloadRepositoriesThreadRunning = false;

    setWindowTitle("Npackd");

    this->genericAppIcon = QIcon(":/images/app.png");
    this->waitAppIcon = QIcon(":/images/wait.png");
    this->brokenIcon = QIcon(":/images/broken.png");

    this->mainFrame = new MainFrame(this);

    updateActions();

    QTableView* t = this->mainFrame->getTableWidget();
    t->addAction(this->ui->actionInstall);
    t->addAction(this->ui->actionUninstall);
    t->addAction(this->ui->actionUpdate);
    t->addAction(this->ui->actionShow_Details);
    t->addAction(this->ui->actionShow_changelog);
    t->addAction(this->ui->actionOpen_folder);
    t->addAction(this->ui->actionGotoPackageURL);
    t->addAction(this->ui->actionTest_Download_Site);

    connect(&this->fileLoader, SIGNAL(downloadCompleted(QString,QString,QString)), this,
            SLOT(downloadCompleted(QString,QString,QString)),
            Qt::QueuedConnection);
    connect(&this->downloadSizeFinder, SIGNAL(downloadCompleted(QString,int64_t,QString)), this,
            SLOT(downloadSizeCompleted(QString,qlonglong,QString)),
            Qt::QueuedConnection);

    // copy toolTip to statusTip for all actions
    for (int i = 0; i < this->children().count(); i++) {
        QObject* ch = this->children().at(i);
        QAction* a = dynamic_cast<QAction*>(ch);
        if (a) {
            a->setStatusTip(a->toolTip());
        }
    }

    connect(QApplication::instance(),
            SIGNAL(focusChanged(QWidget*, QWidget*)), this,
            SLOT(applicationFocusChanged(QWidget*, QWidget*)));

    this->ui->tabWidget->addTab(mainFrame, QObject::tr("Packages"));
    this->loadUISettings();

    this->addJobsTab();
    connect(VisibleJobs::getDefault(), SIGNAL(changed()),
            this, SLOT(visibleJobsChanged()));

    this->mainFrame->getFilterLineEdit()->setFocus();

    InstalledPackages* ip = InstalledPackages::getDefault();
    connect(ip, SIGNAL(statusChanged(const QString&, const Version&)), this,
            SLOT(repositoryStatusChanged(const QString&, const Version&)),
            Qt::QueuedConnection);

    defaultPasswordWindow = (HWND) this->winId();

    this->taskbarMessageId = RegisterWindowMessage(L"TaskbarButtonCreated");
    // qDebug() << "id " << taskbarMessageId;

    // Npackd runs elevated and the taskbar does not. We have to allow the
    // taskbar event here.
    HINSTANCE hInstLib = LoadLibraryA("USER32.DLL");
    BOOL WINAPI (*lpfChangeWindowMessageFilterEx)
            (HWND, UINT, DWORD, void*) =
            (BOOL (WINAPI*) (HWND, UINT, DWORD, void*))
            GetProcAddress(hInstLib, "ChangeWindowMessageFilterEx");
    if (lpfChangeWindowMessageFilterEx) {
        lpfChangeWindowMessageFilterEx((HWND) winId(), taskbarMessageId, 1, 0);
        // qDebug() << "allow taskbar event " << taskbarMessageId;
    }
    FreeLibrary(hInstLib);
}

void MainWindow::applicationFocusChanged(QWidget* old, QWidget* now)
{
    updateActions();
}

bool MainWindow::nativeEvent(const QByteArray & eventType, void * message,
        long * result)
{
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == taskbarMessageId) {
        // qDebug() << "taskbarmessageid";
        HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL,
                CLSCTX_INPROC_SERVER, IID_ITaskbarList3,
                reinterpret_cast<void**> (&(taskbarInterface)));

        if (SUCCEEDED(hr)) {
            hr = taskbarInterface->HrInit();

            if (FAILED(hr)) {
                taskbarInterface->Release();
                taskbarInterface = 0;
            }
        }
        return true;
    }
    return false;
}

void MainWindow::showDetails()
{
    Selection* sel = Selection::findCurrent();
    if (sel) {
        QList<void*> selected = sel->getSelected("PackageVersion");
        if (selected.count() > 0) {
            for (int i = 0; i < selected.count(); i++) {
                PackageVersion* pv = (PackageVersion*) selected.at(i);

                openPackageVersion(pv->package, pv->version, true);
            }
        } else {
            selected = sel->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = (Package*) selected.at(i);

                int index = this->findPackageTab(p->name);
                if (index < 0) {
                    PackageFrame* pf = new PackageFrame(this->ui->tabWidget);
                    Package* p_ = DBRepository::getDefault()->
                            findPackage_(p->name);
                    if (p_) {
                        pf->fillForm(p_);
                        QIcon icon = getPackageVersionIcon(p->name);
                        QString t = p->title;
                        if (t.isEmpty())
                            t = p->name;
                        this->ui->tabWidget->addTab(pf, icon, t);
                        index = this->ui->tabWidget->count() - 1;
                    }
                }

                if (i == selected.count() - 1)
                    this->ui->tabWidget->setCurrentIndex(index);
            }
        }
    }
}

void MainWindow::updateDownloadSize(const QString& url)
{
    QTableView* t = this->mainFrame->getTableWidget();
    PackageItemModel* m = (PackageItemModel*) t->model();
    m->downloadSizeUpdated(url);
}

void MainWindow::updateIcon(const QString& url)
{
    QTableView* t = this->mainFrame->getTableWidget();
    PackageItemModel* m = (PackageItemModel*) t->model();
    m->iconUpdated(url);

    for (int i = 0; i < this->ui->tabWidget->count(); i++) {
        QWidget* w = this->ui->tabWidget->widget(i);
        PackageVersionForm* pvf = dynamic_cast<PackageVersionForm*>(w);
        if (pvf) {
            pvf->updateIcons();
            QIcon icon = getPackageVersionIcon(pvf->pv->package);
            this->ui->tabWidget->setTabIcon(i, icon);
        }
        PackageFrame* pf = dynamic_cast<PackageFrame*>(w);
        if (pf) {
            pf->updateIcons(url);
            QIcon icon = getPackageVersionIcon(pf->p->name);
            this->ui->tabWidget->setTabIcon(i, icon);
        }
    }
}

int MainWindow::findPackageTab(const QString& package) const
{
    int r = -1;
    for (int i = 0; i < this->ui->tabWidget->count(); i++) {
        QWidget* w = this->ui->tabWidget->widget(i);
        PackageFrame* pf = dynamic_cast<PackageFrame*>(w);
        if (pf) {
            if (pf->p->name == package) {
                r = i;
                break;
            }
        }
    }
    return r;
}

int MainWindow::findPackageVersionTab(const QString& package,
        const Version& version) const
{
    int r = -1;
    for (int i = 0; i < this->ui->tabWidget->count(); i++) {
        QWidget* w = this->ui->tabWidget->widget(i);
        PackageVersionForm* pvf = dynamic_cast<PackageVersionForm*>(w);
        if (pvf) {
            //qDebug() << pvf->pv.data()->toString() << "---" <<
            //        package << version.getVersionString();
            if (pvf->pv->package == package && pvf->pv->version == version) {
                r = i;
                break;
            }
        }
    }
    return r;
}

int MainWindow::findLicenseTab(const QString& name) const
{
    int r = -1;
    for (int i = 0; i < this->ui->tabWidget->count(); i++) {
        QWidget* w = this->ui->tabWidget->widget(i);
        LicenseForm* pvf = dynamic_cast<LicenseForm*>(w);
        if (pvf) {
            //qDebug() << pvf->pv.data()->toString() << "---" <<
            //        package << version.getVersionString();
            if (pvf->license->name == name) {
                r = i;
                break;
            }
        }
    }
    return r;
}

void MainWindow::updateStatusInDetailTabs()
{
    for (int i = 0; i < this->ui->tabWidget->count(); i++) {
        QWidget* w = this->ui->tabWidget->widget(i);
        PackageVersionForm* pvf = dynamic_cast<PackageVersionForm*>(w);
        if (pvf) {
            pvf->updateStatus();
        } else {
            PackageFrame* pf = dynamic_cast<PackageFrame*>(w);
            if (pf)
                pf->updateStatus();
        }
    }
}

QIcon MainWindow::getPackageVersionIcon(const QString& package)
{
    MainWindow* mw = MainWindow::getInstance();
    AbstractRepository* r = AbstractRepository::getDefault_();
    Package* p = r->findPackage_(package);

    QIcon icon = MainWindow::genericAppIcon;
    if (p) {
        if (!p->getIcon().isEmpty()) {
            icon = mw->downloadIcon(p->getIcon());
        }
    }
    delete p;

    return icon;
}

MainWindow* MainWindow::getInstance()
{
    return instance;
}

void MainWindow::saveUISettings()
{
    this->mainFrame->saveColumns();
    WindowsRegistry r(HKEY_CURRENT_USER, false, KEY_ALL_ACCESS);
    QString err;
    WindowsRegistry n = r.createSubKey("SOFTWARE\\Npackd\\Npackd", &err,
            KEY_ALL_ACCESS);
    if (err.isEmpty()) {
        n.setBytes("MainWindowState", this->saveState());
        n.setBytes("MainWindowGeometry", this->saveGeometry());
    }
}

void MainWindow::loadUISettings()
{
    mainFrame->loadColumns();
    WindowsRegistry r(HKEY_CURRENT_USER, false, KEY_ALL_ACCESS);
    QString err;
    WindowsRegistry n = r.createSubKey("SOFTWARE\\Npackd\\Npackd", &err,
            KEY_ALL_ACCESS);
    // qDebug() << "MainWindow::loadUISettings" << err;
    if (err.isEmpty()) {
        QByteArray ba = n.getBytes("MainWindowState", &err);
        // qDebug() << "MainWindow::loadUISettings error: " << err;
        if (err.isEmpty()) {
            // qDebug() << "MainWindow::loadUISettings" << ba.length();
            this->restoreState(ba);
        }

        ba = n.getBytes("MainWindowGeometry", &err);
        // qDebug() << "MainWindow::loadUISettings error: " << err;
        if (err.isEmpty()) {
            // qDebug() << "MainWindow::loadUISettings" << ba.length();
            this->restoreGeometry(ba);
        } else {
            this->setWindowState(Qt::WindowMaximized);
        }
    } else {
        this->setWindowState(Qt::WindowMaximized);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    int n = VisibleJobs::getDefault()->runningJobs.count();

    if (n == 0) {
        this->saveUISettings();
        event->accept();
    } else {
        addErrorMessage(QObject::tr("Cannot exit while jobs are running"));
        event->ignore();
    }
}

void MainWindow::repositoryStatusChanged(const QString& package,
        const Version& version)
{
    // qDebug() << "MainWindow::repositoryStatusChanged" << pv->toString();

    QTableView* t = this->mainFrame->getTableWidget();
    PackageItemModel* m = (PackageItemModel*) t->model();
    m->installedStatusChanged(package, version);
    this->updateStatusInDetailTabs();
    this->updateActions();
}

/*static QImage toGray(const QImage& img)
{
    QImage img_gray(img.width(), img.height(), QImage::Format_Indexed8);

    QVector<QRgb> grayscales;
    for (int i=0; i<256; ++i)
        grayscales.push_back(qRgb(i,i,i));
    img_gray.setColorTable(grayscales);

    // farben übertragen
    for (int y=0; y<img.height(); ++y) {
        for (int x=0; x<img.width(); ++x) {
            // farbwert holen
            QRgb rgb = img.pixel(x,y);
            // umrechnen in graustufe
            unsigned char gray = 0.299*qRed(rgb) + 0.587*qGreen(rgb) +
                    0.114*qBlue(rgb);
            // dem graustufen bild den wert zuweisen
            img_gray.setPixel(x,y, gray);
        }
    }
    return img_gray;
}*/

void MainWindow::downloadCompleted(const QString& url,
        const QString& filename, const QString& error)
{
    updateIcon(url);
}

void MainWindow::downloadSizeCompleted(const QString& url,
        int64_t size, const QString& error)
{
    QTableView* t = this->mainFrame->getTableWidget();
    PackageItemModel* m = (PackageItemModel*) t->model();
    m->downloadSizeUpdated(url);
}

void MainWindow::prepare()
{
    QTimer *pTimer = new QTimer(this);
    pTimer->setSingleShot(true);
    connect(pTimer, SIGNAL(timeout()), this, SLOT(onShow()));

    pTimer->start(0);

}

void MainWindow::updateProgressTabTitle()
{
    int n = VisibleJobs::getDefault()->runningJobStates.count();
    time_t max = 0;
    double maxProgress = 0;
    for (int i = 0; i < n; i++) {
        JobState state = VisibleJobs::getDefault()->runningJobStates.at(i);

        // state.job may be null if the corresponding task was just started
        if (state.job) {
            time_t t = state.remainingTime();
            if (t > max) {
                max = t;
                maxProgress = state.progress;
            }
        }
    }
    int maxProgress_ = lround(maxProgress * 100);
    QTime rest = WPMUtils::durationToTime(max);

    QString title;
    if (n == 0)
        title = QString(QObject::tr("0 Jobs"));
    else if (n == 1)
        title = QString(QObject::tr("1 Job (%1%, %2)")).arg(maxProgress_).
                arg(rest.toString());
    else
        title = QString(QObject::tr("%1 Jobs (%2%, %3)")).
                arg(n).arg(maxProgress_).
                arg(rest.toString());

    int index = this->ui->tabWidget->indexOf(this->jobsTab);
    this->ui->tabWidget->setTabText(index, title);

    if (this->taskbarInterface) {
        if (n == 0)
            taskbarInterface->SetProgressState((HWND) winId(), TBPF_NOPROGRESS);
        else {
            taskbarInterface->SetProgressState((HWND) winId(), TBPF_NORMAL);
            taskbarInterface->SetProgressValue((HWND) winId(),
                    lround(maxProgress * 10000), 10000);
        }
    }
}

MainWindow::~MainWindow()
{
    QThreadPool::globalInstance()->clear();
    QThreadPool::globalInstance()->waitForDone(5000);

    delete ui;
}

QIcon MainWindow::downloadIcon(const QString &url)
{
    QIcon r;
    QIcon* inCache = icons.object(url);
    if (inCache) {
        r = *inCache;
    } else {
        QString err;
        QString file = fileLoader.downloadOrQueue(url, &err);
        if (!err.isEmpty()) {
            r = MainWindow::genericAppIcon;
        } else if (!file.isEmpty()) {
            QPixmap pm(file);

            /* gray
            QStyleOption opt(0);
            opt.palette = QApplication::palette();
            pm = QApplication::style()->generatedIconPixmap(QIcon::Disabled, pm, &opt);
            */

            if (!pm.isNull()) {
                pm = pm.scaled(32, 32, Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);

                inCache = new QIcon(pm);
                inCache->detach();

                icons.insert(url, inCache);

                r = *inCache;
            } else {
                r = MainWindow::genericAppIcon;
                inCache = new QIcon(r);
                inCache->detach();

                icons.insert(url, inCache);
            }
        } else {
            r = MainWindow::waitAppIcon;
        }
    }
    return r;
}

QIcon MainWindow::downloadScreenshot(const QString &url)
{
    QIcon r;
    if (screenshots.contains(url)) {
        r = screenshots[url];
    } else {
        QString err;
        QString filename = fileLoader.downloadOrQueue(url, &err);

        if (!err.isEmpty()) {
            r = MainWindow::brokenIcon;
        } else if (!filename.isEmpty()) {
            QPixmap pm(filename);

            /* gray
            QStyleOption opt(0);
            opt.palette = QApplication::palette();
            pm = QApplication::style()->generatedIconPixmap(QIcon::Disabled, pm, &opt);
            */

            if (!pm.isNull()) {
                pm = pm.scaled(200, 200, Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
                r.addPixmap(pm);
                r.detach();

                screenshots.insert(url, r);
            } else {
                r = MainWindow::genericAppIcon;
                screenshots.insert(url, r);
            }
        } else {
            r = MainWindow::waitAppIcon;
        }
    }
    return r;
}

void MainWindow::monitoredJobChanged(const JobState& state)
{
    time_t now;
    time(&now);

    if (now != VisibleJobs::getDefault()->monitoredJobLastChanged &&
            !state.job->parentJob) {
        VisibleJobs::getDefault()->monitoredJobLastChanged = now;

        int index = VisibleJobs::getDefault()->runningJobs.indexOf(state.job);
        if (index >= 0) {
            VisibleJobs::getDefault()->runningJobStates.replace(index, state);
        }

        updateProgressTabTitle();
    }

    if (state.completed && !state.job->parentJob) {
        if (!state.job->getErrorMessage().isEmpty())
            addErrorMessage(state.job->getTitle() + ": " +
                    state.errorMessage, state.errorMessage);
        VisibleJobs::getDefault()->unregisterJob(state.job);
        updateProgressTabTitle();
        state.job->deleteLater();
    }
}

void MainWindow::monitor(Job* job)
{
    connect(job, SIGNAL(changed(const JobState&)), this,
            SLOT(monitoredJobChanged(const JobState&)),
            Qt::QueuedConnection);

    VisibleJobs::getDefault()->runningJobs.append(job);
    VisibleJobs::getDefault()->runningJobStates.append(JobState());

    updateProgressTabTitle();

    pt->addJob(job);
}

void MainWindow::onShow()
{
    DBRepository* dbr = DBRepository::getDefault();

    QString dir = WPMUtils::getShellDir(CSIDL_COMMON_APPDATA) + "\\Npackd";
    QDir d;
    if (!d.exists(dir))
        d.mkpath(dir);

    QString path = dir + "\\Data.db";

    path = QDir::toNativeSeparators(path);

    QString err = dbr->open("default", path);

    if (err.isEmpty()) {
        err = InstalledPackages::getDefault()->readRegistryDatabase();
    }

    if (err.isEmpty()) {
        fillList();

        // this seems to improve the rendering of the main table before
        // the background repository loading starts
        this->mainFrame->getTableWidget()->repaint();

        recognizeAndLoadRepositories(true);
    } else
        this->addErrorMessage(err, err, true, QMessageBox::Critical);
}

void MainWindow::selectPackages(QList<Package*> ps)
{
    QTableView* t = this->mainFrame->getTableWidget();
    t->clearSelection();
    QAbstractItemModel* m = t->model();
    for (int i = 0; i < m->rowCount(); i++) {
        const QVariant v = m->data(m->index(i, 1), Qt::UserRole);
        Package* f = (Package*) v.value<void*>();
        if (Package::contains(ps, f)) {
            //topLeft = t->selectionModel()->selection().
            QModelIndex topLeft = t->model()->index(i, 0);
            // QModelIndex bottomRight = t->model()->index(i, t->columnCount() - 1);

            t->selectionModel()->select(topLeft, QItemSelectionModel::Rows |
                    QItemSelectionModel::Select);
        }
    }
}

class QCITableWidgetItem: public QTableWidgetItem {
public:
    explicit QCITableWidgetItem(const QString &text, int type = Type);
    virtual bool operator<(const QTableWidgetItem &other) const;
};

QCITableWidgetItem::QCITableWidgetItem(const QString &text, int type)
    : QTableWidgetItem(text, type)
{
}

bool QCITableWidgetItem::operator<(const QTableWidgetItem &other) const
{
    QString a = this->text();
    QString b = other.text();

    return a.compare(b, Qt::CaseInsensitive) <= 0;
}

_SearchResult MainWindow::search(Package::Status status, boolean statusInclude,
        const QString& query, int cat0, int cat1, QString* err)
{
    //DWORD start = GetTickCount();

    _SearchResult r;
    *err = "";

    DBRepository* dbr = DBRepository::getDefault();
    r.found = dbr->findPackages(status, statusInclude, query,
            cat0, cat1, err);

    //DWORD search = GetTickCount();
    //qDebug() << "Only search" << (search - start) << query;

    if (err->isEmpty()) {
        r.cats = dbr->findCategories(status, statusInclude, query, 0, -1, -1,
                err);
    }

    if (err->isEmpty()) {
        if (cat0 >= 0) {
            r.cats1 = dbr->findCategories(status, statusInclude, query, 1,
                    cat0, -1, err);
        }
    }

    //qDebug() << "Only categories" << (GetTickCount() - search);

    return r;
}

void MainWindow::fillListInBackground()
{
    fillList();
}

void MainWindow::fillList()
{
    DWORD start = GetTickCount();

    // qDebug() << "MainWindow::fillList";
    QTableView* t = this->mainFrame->getTableWidget();

    t->setUpdatesEnabled(false);

    QString query = this->mainFrame->getFilterLineEdit()->text();

    //QSet<QString> requestedIcons;
    int statusFilter = this->mainFrame->getStatusFilter();
    Package::Status status = Package::NOT_INSTALLED;
    bool statusInclude = false;
    switch (statusFilter) {
        case 1:
            status = Package::INSTALLED;
            statusInclude = true;
            break;
        case 2:
            status = Package::UPDATEABLE;
            statusInclude = true;
            break;
    }

    int cat0 = this->mainFrame->getCategoryFilter(0);
    int cat1 = this->mainFrame->getCategoryFilter(1);

    QString err;
    _SearchResult sr = search(status, statusInclude, query, cat0, cat1, &err);

    if (err.isEmpty()) {
        this->mainFrame->setCategories(0, sr.cats);
        this->mainFrame->setCategoryFilter(0, cat0);
        this->mainFrame->setCategories(1, sr.cats1);
        this->mainFrame->setCategoryFilter(1, cat1);
    } else {
        addErrorMessage(err, err, true, QMessageBox::Critical);
    }

    PackageItemModel* m = (PackageItemModel*) t->model();
    m->setPackages(sr.found);
    t->setUpdatesEnabled(true);
    t->horizontalHeader()->setSectionsMovable(true);

    DWORD dur = GetTickCount() - start;

    this->mainFrame->setDuration(dur);
}

QString MainWindow::createPackageVersionsHTML(const QStringList& names)
{
    QStringList allNames;
    if (names.count() > 10) {
        allNames = names.mid(0, 10);
        allNames.append("...");
    } else {
        allNames = names;
    }
    for (int i = 0; i < allNames.count(); i++) {
        allNames[i] = allNames[i].toHtmlEscaped();
    }
    return allNames.join("<br>");
}

void MainWindow::processWithSelfUpdate(Job* job,
        QList<InstallOperation*> &ops, int programCloseType)
{
    QString newExe;

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.8, "Copying the executable");

        QString thisExe = WPMUtils::getExeFile();

        // 1. copy .exe to the temporary directory
        QTemporaryFile of(QDir::tempPath() + "\\npackdgXXXXXX.exe");
        of.setAutoRemove(false);
        if (!of.open())
            job->setErrorMessage(of.errorString());
        else {
            newExe = of.fileName();
            if (!of.remove()) {
                job->setErrorMessage(of.errorString());
            } else {
                of.close();

                // qDebug() << "self-update 1";

                if (!QFile::copy(thisExe, newExe))
                    job->setErrorMessage("Error copying the binary");
                else
                    sub->completeWithProgress();

                // qDebug() << "self-update 2";
            }
        }
    }

    QString batchFileName;
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.1, "Creating the .bat file");

        QString pct = WPMUtils::programCloseType2String(programCloseType);
        QStringList batch;
        for (int i = 0; i < ops.count(); i++) {
            InstallOperation* op = ops.at(i);
            QString oneCmd = "\"" + newExe + "\" ";

            // ping 1.1.1.1 always fails so we use || instead of &&
            if (op->install) {
                oneCmd += "add -p " + op->package + " -v " +
                        op->version.getVersionString() +
                        " || ping 1.1.1.1 -n 1 -w 10000 > nul || exit /b %errorlevel%";
            } else {
                oneCmd += "remove -p " + op->package + " -v " +
                        op->version.getVersionString() +
                        " -e " + pct +
                        " || ping 1.1.1.1 -n 1 -w 10000 > nul || exit /b %errorlevel%";
            }
            batch.append(oneCmd);
        }

        batch.append("\"" + newExe + "\" start-newest");

        // qDebug() << "self-update 3";

        QTemporaryFile file(QDir::tempPath() +
                          "\\npackdgXXXXXX.bat");
        file.setAutoRemove(false);
        if (!file.open())
            job->setErrorMessage(file.errorString());
        else {
            batchFileName = file.fileName();

            // qDebug() << "batch" << file.fileName();

            QTextStream stream(&file);
            stream.setCodec("UTF-8");
            stream << batch.join("\r\n");
            if (stream.status() != QTextStream::Ok)
                job->setErrorMessage("Error writing the .bat file");
            file.close();

            // qDebug() << "self-update 4";

            sub->completeWithProgress();
        }
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.1, "Starting the copied binary");

        QString file_ = batchFileName;
        file_.replace('/', '\\');
        QString args = "/U /E:ON /V:OFF /C \"\"" + file_ + "\"\"";
        QString prg = WPMUtils::findCmdExe();

        bool success = false;
        PROCESS_INFORMATION pinfo;

        STARTUPINFOW startupInfo = {
            sizeof(STARTUPINFO), 0, 0, 0,
            (ulong) CW_USEDEFAULT, (ulong) CW_USEDEFAULT,
            (ulong) CW_USEDEFAULT, (ulong) CW_USEDEFAULT,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        success = CreateProcess(
                (wchar_t*) prg.utf16(),
                (wchar_t*) args.utf16(),
                0, 0, TRUE,
                CREATE_UNICODE_ENVIRONMENT, 0,
                0, &startupInfo, &pinfo);

        if (success) {
            CloseHandle(pinfo.hThread);
            CloseHandle(pinfo.hProcess);
            // qDebug() << "success!222";
        }

        // qDebug() << "self-update 5";

        sub->completeWithProgress();
        job->setProgress(1);
    }

    job->complete();
}

void MainWindow::process(QList<InstallOperation*> &install,
        int programCloseType)
{
    QString err;

    bool confirmed = false;
    if (err.isEmpty())
        confirmed = UIUtils::confirmInstallOperations(this, install, &err);

    if (err.isEmpty()) {
        if (confirmed) {
            AbstractRepository* rep = AbstractRepository::getDefault_();

            if (rep->includesRemoveItself(install)) {
                QString txt = QObject::tr("Chosen changes require an update of this Npackd instance. Are you sure?");
                if (UIUtils::confirm(this, QObject::tr("Warning"), txt, txt)) {
                    Job* job = new Job();
                    processWithSelfUpdate(job, install, programCloseType);
                    delete job;
                }
            } else {
                Job* job = new Job(QObject::tr("Install/Uninstall"));

                connect(job, SIGNAL(jobCompleted()), this,
                        SLOT(processThreadFinished()),
                        Qt::QueuedConnection);

                monitor(job);

                QtConcurrent::run(AbstractRepository::getDefault_(),
                        &AbstractRepository::processWithCoInitialize,
                        job, install,
                        WPMUtils::getCloseProcessType());

                install.clear();
            }
        }
    }

    if (!err.isEmpty()) {
        addErrorMessage(err, err, true, QMessageBox::Critical);
    }

    qDeleteAll(install);
    install.clear();
}

void MainWindow::processThreadFinished()
{
    QTableView* t = this->mainFrame->getTableWidget();
    QItemSelectionModel* sm = t->selectionModel();
    QList<Package*> sel = mainFrame->getSelectedPackagesInTable();
    for (int i = 0; i < sel.count(); i++) {
        sel[i] = sel[i]->clone();
    }
    QModelIndex index = sm->currentIndex();
    fillList();
    updateStatusInDetailTabs();
    sm->setCurrentIndex(index, QItemSelectionModel::Current);
    selectPackages(sel);
    qDeleteAll(sel);
    updateActions();
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    case QEvent::ActivationChange:
        // qDebug() << "QEvent::ActivationChange";
        QTimer::singleShot(0, this, SLOT(updateActionsSlot()));
        break;
    default:
        break;
    }
}

void MainWindow::on_actionExit_triggered()
{
    int n = VisibleJobs::getDefault()->runningJobs.count();

    if (n > 0)
        addErrorMessage(QObject::tr("Cannot exit while jobs are running"));
    else
        this->close();
}

bool MainWindow::isUpdateEnabled(const QString& package)
{
    QString err;

    bool res = false;
    AbstractRepository* r = AbstractRepository::getDefault_();
    PackageVersion* newest = r->findNewestInstallablePackageVersion_(
            package, &err);
    PackageVersion* newesti = r->findNewestInstalledPackageVersion_(
            package, &err);
    if (newest != 0 && newesti != 0) {
        // qDebug() << newest->version.getVersionString() << " " <<
                newesti->version.getVersionString();
        bool canInstall = !newest->isLocked() && !newest->installed() &&
                newest->download.isValid();
        bool canUninstall = !newesti->isLocked() &&
                !newesti->isInWindowsDir();

        // qDebug() << canInstall << " " << canUninstall;

        res = canInstall && canUninstall &&
                newest->version.compare(newesti->version) > 0;
    }
    delete newest;
    delete newesti;

    return res;
}

void MainWindow::updateActions()
{
    updateInstallAction();
    updateUninstallAction();
    updateUpdateAction();
    updateGotoPackageURLAction();
    updateShowChangelogAction();
    updateTestDownloadSiteAction();
    updateActionShowDetailsAction();
    updateCloseTabAction();
    updateReloadRepositoriesAction();
    updateScanHardDrivesAction();
    updateShowFolderAction();
}

void MainWindow::updateInstallAction()
{
    Selection* selection = Selection::findCurrent();
    bool enabled = false;
    if (selection) {
        QList<void*> selected = selection->getSelected("PackageVersion");
        if (selected.count() > 0) {
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                PackageVersion* pv = (PackageVersion*) selected.at(i);

                enabled = enabled &&
                        pv && !pv->isLocked() &&
                        !pv->installed() &&
                        pv->download.isValid();
            }
        } else {
            AbstractRepository* r = AbstractRepository::getDefault_();
            selected = selection->getSelected("Package");
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                Package* p = (Package*) selected.at(i);
                QString err;
                PackageVersion* pv  = r->findNewestInstallablePackageVersion_(
                        p->name, &err);

                enabled = enabled &&
                        pv && !pv->isLocked() &&
                        !pv->installed() &&
                        pv->download.isValid();

                delete pv;
            }
        }
    }

    this->ui->actionInstall->setEnabled(enabled);
}

void MainWindow::updateShowFolderAction()
{
    // qDebug() << "MainWindow::updateUninstallAction start";

    Selection* selection = Selection::findCurrent();

    bool enabled = false;
    if (selection) {
        QList<void*> selected = selection->getSelected("PackageVersion");
        if (selected.count() > 0) {
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                PackageVersion* pv = (PackageVersion*) selected.at(i);

                enabled = enabled &&
                        pv && !pv->isLocked() &&
                        pv->installed() && !pv->isInWindowsDir();
            }
            // qDebug() << "MainWindow::updateUninstallAction 2:" << selected.count();
        } else {
            AbstractRepository* r = AbstractRepository::getDefault_();
            QList<void*> selected = selection->getSelected("Package");
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                Package* p = (Package*) selected.at(i);

                QString err;
                PackageVersion* pv = r->findNewestInstalledPackageVersion_(
                        p->name, &err);
                if (!err.isEmpty()) {
                    err = QObject::tr("Error finding the newest installed version for %1: %2").
                            arg(p->title).arg(err);
                    addErrorMessage(err, err, true, QMessageBox::Critical);
                }

                enabled = enabled &&
                        pv && !pv->isLocked() &&
                        pv->installed() && !pv->isInWindowsDir();

                delete pv;
            }
        }
    }
    this->ui->actionOpen_folder->setEnabled(enabled);
    // qDebug() << "MainWindow::updateUninstallAction end " << enabled;
}

void MainWindow::updateUninstallAction()
{
    // qDebug() << "MainWindow::updateUninstallAction start";

    Selection* selection = Selection::findCurrent();

    bool enabled = false;
    if (selection) {
        QList<void*> selected = selection->getSelected("PackageVersion");
        if (selected.count() > 0) {
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                PackageVersion* pv = (PackageVersion*) selected.at(i);

                enabled = enabled &&
                        pv && !pv->isLocked() &&
                        pv->installed() && !pv->isInWindowsDir();
            }
            // qDebug() << "MainWindow::updateUninstallAction 2:" << selected.count();
        } else {
            AbstractRepository* r = AbstractRepository::getDefault_();
            QList<void*> selected = selection->getSelected("Package");
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                Package* p = (Package*) selected.at(i);

                QString err;
                PackageVersion* pv = r->findNewestInstalledPackageVersion_(
                        p->name, &err);
                if (!err.isEmpty()) {
                    err = QObject::tr("Error finding the newest installed version for %1: %2").
                            arg(p->title).arg(err);
                    addErrorMessage(err, err, true, QMessageBox::Critical);
                }

                enabled = enabled &&
                        pv && !pv->isLocked() &&
                        pv->installed() && !pv->isInWindowsDir();

                delete pv;
            }
        }
    }
    this->ui->actionUninstall->setEnabled(enabled);
    // qDebug() << "MainWindow::updateUninstallAction end " << enabled;
}

void MainWindow::updateUpdateAction()
{
    Selection* selection = Selection::findCurrent();

    bool enabled = false;
    if (selection) {
        QList<void*> selected = selection->getSelected("Package");
        if (selected.count() > 0) {
            enabled = true;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                Package* p = (Package*) selected.at(i);

                enabled = enabled && isUpdateEnabled(p->name);
            }
        } else {
            selected = selection->getSelected("PackageVersion");
            enabled = selected.count() >= 1;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                PackageVersion* pv = (PackageVersion*) selected.at(i);

                enabled = enabled && isUpdateEnabled(pv->package);
            }
        }
    }
    this->ui->actionUpdate->setEnabled(enabled);
}

void MainWindow::updateScanHardDrivesAction()
{
    this->ui->actionScan_Hard_Drives->setEnabled(
            !hardDriveScanRunning && !reloadRepositoriesThreadRunning);
}

void MainWindow::updateReloadRepositoriesAction()
{
    this->ui->actionReload_Repositories->setEnabled(
            !hardDriveScanRunning && !reloadRepositoriesThreadRunning);
}

void MainWindow::updateCloseTabAction()
{
    QWidget* w = this->ui->tabWidget->currentWidget();
    this->ui->actionClose_Tab->setEnabled(
            w != this->mainFrame && w != this->jobsTab);
}

void MainWindow::updateActionShowDetailsAction()
{
    Selection* selection = Selection::findCurrent();
    bool enabled = false;
    if (selection) {
        QList<void*> selected = selection->getSelected("PackageVersion");
        if (selected.count() == 0)
            selected = selection->getSelected("Package");

        enabled = selected.count() > 0;
    }

    this->ui->actionShow_Details->setEnabled(enabled);
}

void MainWindow::updateTestDownloadSiteAction()
{
    Selection* selection = Selection::findCurrent();

    bool enabled = false;
    if (selection) {
        QList<void*> selected = selection->getSelected("PackageVersion");
        if (selected.count() > 0) {
            for (int i = 0; i < selected.count(); i++) {
                PackageVersion* pv = (PackageVersion*) selected.at(i);
                if (pv->download.isValid()) {
                    enabled = true;
                    break;
                }
            }
        } else {
            AbstractRepository* r = AbstractRepository::getDefault_();
            selected = selection->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = (Package*) selected.at(i);
                QString err;
                PackageVersion* pv = r->findNewestInstallablePackageVersion_(
                        p->name, &err);
                if (pv && pv->download.isValid()) {
                    enabled = true;
                    delete pv;
                    break;
                }
                delete pv;
            }
        }
    }

    this->ui->actionTest_Download_Site->setEnabled(enabled);
}

void MainWindow::updateShowChangelogAction()
{
    Selection* selection = Selection::findCurrent();
    bool enabled = false;
    if (selection) {
        QList<void*> selected = selection->getSelected("PackageVersion");
        if (selected.count() > 0) {
            AbstractRepository* r = Repository::getDefault_();
            for (int i = 0; i < selected.count(); i++) {
                PackageVersion* pv = (PackageVersion*) selected.at(i);

                Package* p = r->findPackage_(pv->package);
                if (p) {
                    QUrl url(p->getChangeLog());
                    delete p;

                    if (url.isValid()) {
                        enabled = true;
                        break;
                    }
                }
            }
        } else {
            selected = selection->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = (Package*) selected.at(i);

                if (p) {
                    QUrl url(p->getChangeLog());
                    if (url.isValid()) {
                        enabled = true;
                        break;
                    }
                }
            }
        }
    }

    this->ui->actionShow_changelog->setEnabled(enabled);
}

void MainWindow::updateGotoPackageURLAction()
{
    Selection* selection = Selection::findCurrent();
    bool enabled = false;
    if (selection) {
        QList<void*> selected = selection->getSelected("PackageVersion");
        if (selected.count() > 0) {
            AbstractRepository* r = Repository::getDefault_();
            for (int i = 0; i < selected.count(); i++) {
                PackageVersion* pv = (PackageVersion*) selected.at(i);

                Package* p = r->findPackage_(pv->package);
                if (p) {
                    QUrl url(p->url);
                    delete p;

                    if (url.isValid()) {
                        enabled = true;
                        break;
                    }
                }
            }
        } else {
            selected = selection->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = (Package*) selected.at(i);

                if (p) {
                    QUrl url(p->url);
                    if (url.isValid()) {
                        enabled = true;
                        break;
                    }
                }
            }
        }
    }

    this->ui->actionGotoPackageURL->setEnabled(enabled);
}

void MainWindow::closeDetailTabs()
{
    for (int i = 0; i < this->ui->tabWidget->count(); ) {
        QWidget* w = this->ui->tabWidget->widget(i);
        PackageVersionForm* pvf = dynamic_cast<PackageVersionForm*>(w);
        PackageFrame* pf = dynamic_cast<PackageFrame*>(w);
        LicenseForm* lf = dynamic_cast<LicenseForm*>(w);
        if (pvf != 0 || lf != 0 || pf != 0) {
            this->ui->tabWidget->removeTab(i);
        } else {
            i++;
        }
    }
}

void MainWindow::recognizeAndLoadRepositories(bool useCache)
{
    Job* job = new Job(QObject::tr("Initializing"));

    connect(job, SIGNAL(jobCompleted()), this,
            SLOT(recognizeAndLoadRepositoriesThreadFinished()),
            Qt::QueuedConnection);

    this->reloadRepositoriesThreadRunning = true;
    updateActions();

    monitor(job);

    QtConcurrent::run(DBRepository::getDefault(),
            &DBRepository::updateF5Runnable,
            job);
}

void MainWindow::setMenuAccelerators(){
    QMenuBar* mb = this->menuBar();

    QStringList titles;
    for (int i = 0; i < mb->children().count(); i++) {
        QMenu* m = dynamic_cast<QMenu*>(mb->children().at(i));
        if (m) {
            titles.append(m->title());
        }
    }
    chooseAccelerators(&titles);
    for (int i = 0, j = 0; i < mb->children().count(); i++) {
        QMenu* m = dynamic_cast<QMenu*>(mb->children().at(i));
        if (m) {
            m->setTitle(titles.at(j));
            j++;
        }
    }
}

void MainWindow::chooseAccelerators(QStringList* titles)
{
    QList<QChar> used;
    for (int i = 0; i < titles->count(); i++) {
        QString title = titles->at(i);

        if (title.contains('&')) {
            int pos = title.indexOf('&');
            if (pos + 1 < title.length()) {
                QChar c = title.at(pos + 1);
                if (c.isLetter()) {
                    if (!used.contains(c))
                        used.append(c);
                    else
                        title.remove(pos, 1);
                }
            }
        }

        if (!title.contains('&')) {
            QString s = title.toUpper();
            int pos = -1;
            for (int j = 0; j < s.length(); j++) {
                QChar c = s.at(j);
                if (c.isLetter() && !used.contains(c)) {
                    pos = j;
                    break;
                }
            }

            if (pos >= 0) {
                QChar c = s.at(pos);
                used.append(c);
                title.insert(pos, '&');
            }
        }

        titles->replace(i, title);
    }
}

void MainWindow::setActionAccelerators(QWidget* w) {
    QStringList titles;
    for (int i = 0; i < w->children().count(); i++) {
        QAction* m = dynamic_cast<QAction*>(w->children().at(i));
        if (m) {
            titles.append(m->text());
        }
    }
    chooseAccelerators(&titles);
    for (int i = 0, j = 0; i < w->children().count(); i++) {
        QAction* m = dynamic_cast<QAction*>(w->children().at(i));
        if (m) {
            m->setText(titles.at(j));
            j++;
        }
    }
}

void MainWindow::reloadTabs()
{
    for (int i = 0; i < this->ui->tabWidget->count(); i++) {
        QWidget* w = this->ui->tabWidget->widget(i);

        PackageFrame* pf = dynamic_cast<PackageFrame*>(w);
        if (pf) {
            pf->reload();
        }

        PackageVersionForm* pvf = dynamic_cast<PackageVersionForm*>(w);
        if (pvf) {
            pvf->reload();
        }

        LicenseForm* lf = dynamic_cast<LicenseForm*>(w);
        if (lf) {
            lf->reload();
        }
    }
}

void MainWindow::recognizeAndLoadRepositoriesThreadFinished()
{
    QTableView* t = this->mainFrame->getTableWidget();
    QItemSelectionModel* sm = t->selectionModel();
    QList<Package*> sel = mainFrame->getSelectedPackagesInTable();
    for (int i = 0; i < sel.count(); i++) {
        sel[i] = sel[i]->clone();
    }
    QModelIndex index = sm->currentIndex();

    PackageItemModel* m = (PackageItemModel*) t->model();
    m->setPackages(QList<Package*>());
    m->clearCache();
    fillList();

    sm->setCurrentIndex(index, QItemSelectionModel::Current);
    selectPackages(sel);
    qDeleteAll(sel);
    reloadTabs();

    this->reloadRepositoriesThreadRunning = false;
    updateActions();
}

QList<void*> MainWindow::getSelected(const QString& type) const
{
    QWidget* w = this->ui->tabWidget->currentWidget();
    QList<void*> r;
    if (w) {
        Selection* sel = dynamic_cast<Selection*>(w);
        if (sel)
            r = sel->getSelected(type);
    }
    return r;
}

void MainWindow::openLicense(const QString& name, bool select)
{
    int index = this->findLicenseTab(name);
    if (index < 0) {
        LicenseForm* f = new LicenseForm(this->ui->tabWidget);
        QString err;
        License* lic =
                DBRepository::getDefault()->
                findLicense_(name, &err);
        if (err.isEmpty()) {
            f->fillForm(lic);
            this->ui->tabWidget->addTab(f, lic->title);
        } else {
            MainWindow::getInstance()->addErrorMessage(err, err, true,
                    QMessageBox::Critical);
        }
        index = this->ui->tabWidget->count() - 1;
    }
    if (select)
        this->ui->tabWidget->setCurrentIndex(index);
}

void MainWindow::openPackageVersion(const QString& package,
        const Version& version, bool select)
{
    int index = this->findPackageVersionTab(package, version);
    if (index < 0) {
        PackageVersionForm* pvf = new PackageVersionForm(
                this->ui->tabWidget);
        QString err;
        PackageVersion* pv_ =
                DBRepository::getDefault()->
                findPackageVersion_(package, version, &err);
        if (!err.isEmpty())
            addErrorMessage(err, err, true, QMessageBox::Critical);
        if (pv_) {
            pvf->fillForm(pv_);
            QIcon icon = getPackageVersionIcon(package);
            this->ui->tabWidget->addTab(pvf, icon, pv_->toString());
            index = this->ui->tabWidget->count() - 1;
        }
    }
    if (select)
        this->ui->tabWidget->setCurrentIndex(index);
}

void MainWindow::addTab(QWidget* w, const QIcon& icon, const QString& title)
{
    this->ui->tabWidget->addTab(w, icon, title);
    this->ui->tabWidget->setCurrentIndex(this->ui->tabWidget->count() - 1);
}

void MainWindow::on_actionGotoPackageURL_triggered()
{
    QSet<QUrl> urls;

    Selection* selection = Selection::findCurrent();
    QList<void*> selected;
    if (selection) {
        selected = selection->getSelected("Package");
        if (selected.count() != 0) {
            for (int i = 0; i < selected.count(); i++) {
                Package* p = (Package*) selected.at(i);
                QUrl url(p->url);
                if (url.isValid())
                    urls.insert(url);
            }
        } else {
            AbstractRepository* r = AbstractRepository::getDefault_();
            selected = selection->getSelected("PackageVersion");
            for (int i = 0; i < selected.count(); i++) {
                PackageVersion* pv = (PackageVersion*) selected.at(i);
                QScopedPointer<Package> p(r->findPackage_(pv->package));
                if (p) {
                    QUrl url(p->url);
                    if (url.isValid())
                        urls.insert(url);
                }
            }
        }
    }

    for (QSet<QUrl>::const_iterator it = urls.begin();
            it != urls.end(); it++) {
        openURL(*it);
    }
}

void MainWindow::on_actionSettings_triggered()
{
    SettingsFrame* d = 0;
    for (int i = 0; i < this->ui->tabWidget->count(); i++) {
        QWidget* w = this->ui->tabWidget->widget(i);
        d = dynamic_cast<SettingsFrame*>(w);
        if (d) {
            break;
        }
    }
    if (d) {
        this->ui->tabWidget->setCurrentWidget(d);
    } else {
        d = new SettingsFrame(this->ui->tabWidget);

        QString err;
        QList<QUrl*> urls = Repository::getRepositoryURLs(&err);
        QStringList list;
        for (int i = 0; i < urls.count(); i++) {
            list.append(urls.at(i)->toString());
        }
        d->setRepositoryURLs(list);
        qDeleteAll(urls);
        urls.clear();

        d->setInstallationDirectory(WPMUtils::getInstallationDirectory());

        d->setCloseProcessType(WPMUtils::getCloseProcessType());

        this->ui->tabWidget->addTab(d, QObject::tr("Settings"));
        this->ui->tabWidget->setCurrentIndex(this->ui->tabWidget->count() - 1);
    }
}

void MainWindow::on_actionUpdate_triggered()
{
    QString err;

    QList<Package*> packages;
    AbstractRepository* r = AbstractRepository::getDefault_();
    if (err.isEmpty()) {
        Selection* sel = Selection::findCurrent();
        QList<void*> selected;
        if (sel)
            selected = sel->getSelected("PackageVersion");

        if (selected.count() > 0) {
            // multiple versions of the same package could be selected in the table,
            // but only one should be updated
            QSet<QString> used;

            for (int i = 0; i < selected.count(); i++) {
                PackageVersion* pv = (PackageVersion*) selected.at(i);
                if (!used.contains(pv->package)) {
                    Package* p = r->findPackage_(pv->package);

                    if (p != 0) {
                        packages.append(p);
                        used.insert(pv->package);
                    }
                }
            }
        } else {
            if (sel) {
                selected = sel->getSelected("Package");
                for (int i = 0; i < selected.count(); i++) {
                    Package* p = (Package*) selected.at(i);
                    packages.append(p->clone());
                }
            }
        }
    }

    QList<InstallOperation*> ops;
    if (err.isEmpty() && packages.count() > 0) {
        err = r->planUpdates(packages, ops);
    }

    if (err.isEmpty()) {
        if (ops.count() > 0)
            process(ops, WPMUtils::getCloseProcessType());
    } else
        addErrorMessage(err, err, true, QMessageBox::Critical);

    qDeleteAll(packages);
}

void MainWindow::on_actionTest_Download_Site_triggered()
{
    QString err;
    Selection* sel = Selection::findCurrent();
    if (sel) {
        QSet<QString> urls;

        QList<void*> selected = sel->getSelected("PackageVersion");
        if (selected.count() > 0) {
            for (int i = 0; i < selected.count(); i++) {
                PackageVersion* pv = (PackageVersion*) selected.at(i);
                urls.insert(pv->download.host());
            }
        } else {
            AbstractRepository* r = AbstractRepository::getDefault_();
            selected = sel->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = (Package*) selected.at(i);
                QScopedPointer<PackageVersion> pv(
                        r->findNewestInstallablePackageVersion_(p->name,
                        &err));
                if (!err.isEmpty())
                    break;
                if (pv) {
                    urls.insert(pv->download.host());
                }
            }
        }

        if (err.isEmpty()) {
            for (QSet<QString>::const_iterator it = urls.begin();
                    it != urls.end(); it++) {
                QString s = "http://www.urlvoid.com/scan/" + *it;
                QUrl url(s);
                if (url.isValid())
                    openURL(url);
            }
        }
    }

    if (!err.isEmpty())
        addErrorMessage(err, err, true, QMessageBox::Critical);
}

void MainWindow::on_actionAbout_triggered()
{
    addTextTab(QObject::tr("About"), QString(
            QObject::tr("<html><body>Npackd %1 - software package manager for Windows (R)<br><a href='https://npackd.appspot.com/'>https://npackd.appspot.com/</a></body></html>")).
            arg(NPACKD_VERSION), true);
}

void MainWindow::on_tabWidget_tabCloseRequested(int index)
{
    QWidget* w = this->ui->tabWidget->widget(index);
    if (w != this->mainFrame && w != this->jobsTab) {
        this->ui->tabWidget->removeTab(index);
    }
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
    updateActions();
}

void MainWindow::addTextTab(const QString& title, const QString& text,
        bool html)
{
    QWidget* w;
    if (html) {
        QTextBrowser* te = new QTextBrowser(this->ui->tabWidget);
        te->setReadOnly(true);
        te->setHtml(text);
        te->setOpenExternalLinks(true);
        w = te;
    } else {
        QTextEdit* te = new QTextEdit(this->ui->tabWidget);
        te->setReadOnly(true);
        te->setText(text);
        w = te;
    }
    this->ui->tabWidget->addTab(w, title);
    this->ui->tabWidget->setCurrentIndex(this->ui->tabWidget->count() - 1);
}

void MainWindow::addJobsTab()
{
    QWidget* w = new QWidget(this->ui->tabWidget);
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 10, 0, 0);
    w->setLayout(layout);

    pt = new ProgressTree2(w);
    layout->addWidget(pt);

    int index = this->ui->tabWidget->addTab(w,
            QObject::tr("Jobs"));

    this->jobsTab = this->ui->tabWidget->widget(index);
    updateProgressTabTitle();
}

void MainWindow::on_actionShow_Details_triggered()
{
    showDetails();
}

void MainWindow::on_actionScan_Hard_Drives_triggered()
{
    QString err;
    PackageVersion* locked = PackageVersion::findLockedPackageVersion(&err);
    if (!err.isEmpty()) {
        this->addErrorMessage(err, err, true, QMessageBox::Critical);
        delete locked;
        return;
    }
    if (locked) {
        QString msg(QObject::tr("Cannot start the scan now. The package %1 is locked by a currently running installation/removal."));
        this->addErrorMessage(msg.arg(locked->toString()));
        delete locked;
        return;
    }

    Job* job = new Job(QObject::tr("Install/Uninstall"));
    ScanHardDrivesThread* it = new ScanHardDrivesThread(job);

    connect(it, SIGNAL(finished()), this,
            SLOT(hardDriveScanThreadFinished()),
            Qt::QueuedConnection);

    this->hardDriveScanRunning = true;
    this->updateActions();

    monitor(job);
    it->start(QThread::LowestPriority);
}

bool comparesi(const QPair<QString, int>& a, const QPair<QString, int>& b)
{
    return a.second > b.second;
}

void MainWindow::hardDriveScanThreadFinished()
{
    /* tag cloud
    QScrollArea* jobsScrollArea = new QScrollArea(this->ui->tabWidget);
    jobsScrollArea->setFrameStyle(0);

    ScanHardDrivesThread* it = (ScanHardDrivesThread*) this->sender();
    QList<QPair<QString, int> > entries;
    QList<QString> words = it->words.keys();
    for (int i = 0; i < words.count(); i++) {
        QString w = words.at(i);
        entries.append(QPair<QString, int>(w, it->words.value(w)));
    }
    qSort(entries.begin(), entries.end(), comparesi);

    QWidget *window = new QWidget(jobsScrollArea);
    FlowLayout *layout = new FlowLayout();
    window->setLayout(layout);

    QSizePolicy sp;
    sp.setVerticalPolicy(QSizePolicy::Preferred);
    sp.setHorizontalPolicy(QSizePolicy::Ignored);
    sp.setHorizontalStretch(100);
    window->setSizePolicy(sp);

    for (int i = 0; i < entries.count(); i++) {
        QString w = entries.at(i).first;
        int n = entries.at(i).second;

        QLabel *b = new QLabel("<a href=\"http://www.test.de\">" +
                w + " (" + QString::number(n) + ")</a>");
        b->setMouseTracking(true);
        b->setFocusPolicy(Qt::StrongFocus);
        b->setTextInteractionFlags(Qt::TextSelectableByMouse |
                Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse);
        layout->addWidget(b);
    }
    jobsScrollArea->setWidget(window);
    jobsScrollArea->setWidgetResizable(true);
    addTab(jobsScrollArea, genericAppIcon, "Tags");
    */

    QStringList detected;
    ScanHardDrivesThread* it = (ScanHardDrivesThread*) this->sender();
    for (int i = 0; i < it->detected.count(); i++) {
        PackageVersion* pv = it->detected.at(i);
        detected.append(pv->toString());
    }

    detected.append("____________________");
    detected.append(QString(QObject::tr("%1 package(s) detected")).
            arg(it->detected.count()));

    fillList();

    addTextTab(QObject::tr("Package detection status"),
            detected.join("\n"));

    this->hardDriveScanRunning = false;
    this->updateActions();
}

void MainWindow::addErrorMessage(const QString& msg, const QString& details,
        bool autoHide, QMessageBox::Icon icon)
{
    MessageFrame* label = new MessageFrame(this->centralWidget(), msg,
            details, autoHide ? 30 : 0, icon);
    QVBoxLayout* layout = (QVBoxLayout*) this->centralWidget()->layout();
    layout->insertWidget(0, label);
}

void MainWindow::on_actionReload_Repositories_triggered()
{
    QString err;
    PackageVersion* locked = PackageVersion::findLockedPackageVersion(&err);
    if (!err.isEmpty())
        addErrorMessage(err, err, true, QMessageBox::Critical);
    if (locked) {
        QString msg(QObject::tr("Cannot reload the repositories now. The package %1 is locked by a currently running installation/removal."));
        this->addErrorMessage(msg.arg(locked->toString()));
        delete locked;
    } else {
        recognizeAndLoadRepositories(true);
    }
}

void MainWindow::on_actionClose_Tab_triggered()
{
    QWidget* w = this->ui->tabWidget->currentWidget();
    if (w != this->mainFrame && w != this->jobsTab) {
        this->ui->tabWidget->removeTab(this->ui->tabWidget->currentIndex());
    }
}

void MainWindow::updateActionsSlot()
{
    updateActions();
}

void MainWindow::on_actionFile_an_Issue_triggered()
{
    openURL(QUrl(
            "http://code.google.com/p/windows-package-manager/issues/entry?template=Defect%20report%20from%20user"));
}

void MainWindow::on_actionInstall_triggered()
{
    QString err;

    QList<PackageVersion*> pvs;

    if (err.isEmpty()) {
        Selection* selection = Selection::findCurrent();
        QList<void*> selected;
        if (selection)
            selected = selection->getSelected("PackageVersion");

        if (selected.count() == 0) {
            AbstractRepository* r = AbstractRepository::getDefault_();
            if (selection)
                selected = selection->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = (Package*) selected.at(i);
                PackageVersion* pv = r->findNewestInstallablePackageVersion_(
                        p->name, &err);
                if (!err.isEmpty())
                    break;

                if (pv)
                    pvs.append(pv);
            }
        } else {
            for (int i = 0; i < selected.count(); i++) {
                pvs.append(((PackageVersion*) selected.at(i))->clone());
            }
        }
    }

    QList<InstallOperation*> ops;
    QList<PackageVersion*> installed;
    QList<PackageVersion*> avoid;

    if (err.isEmpty()) {
        installed = AbstractRepository::getDefault_()->getInstalled_(&err);
    }

    if (err.isEmpty()) {
        for (int i = 0; i < pvs.count(); i++) {
            PackageVersion* pv = pvs.at(i);

            avoid.clear();
            err = pv->planInstallation(installed, ops, avoid);
            if (!err.isEmpty())
                break;
        }
    }

    if (err.isEmpty())
        process(ops, WPMUtils::getCloseProcessType());
    else
        addErrorMessage(err, err, true, QMessageBox::Critical);

    qDeleteAll(installed);
    qDeleteAll(pvs);
}

void MainWindow::on_actionUninstall_triggered()
{
    QString err;

    QList<PackageVersion*> pvs;

    if (err.isEmpty()) {
        Selection* selection = Selection::findCurrent();
        QList<void*> selected;
        if (selection)
            selected = selection->getSelected("PackageVersion");

        if (selected.count() == 0) {
            AbstractRepository* r = AbstractRepository::getDefault_();
            if (selection)
                selected = selection->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = (Package*) selected.at(i);

                QString err;
                PackageVersion* pv = r->findNewestInstalledPackageVersion_(
                        p->name, &err);
                if (!err.isEmpty())
                    addErrorMessage(err, err, true, QMessageBox::Critical);
                if (pv) {
                    pvs.append(pv);
                }
            }
        } else {
            for (int i = 0; i < selected.count(); i++) {
                pvs.append(((PackageVersion*) selected.at(i))->clone());
            }
        }
    }

    QList<InstallOperation*> ops;
    QList<PackageVersion*> installed;

    if (err.isEmpty()) {
        installed = AbstractRepository::getDefault_()->
                getInstalled_(&err);
    }

    if (err.isEmpty()) {
        for (int i = 0; i < pvs.count(); i++) {
            PackageVersion* pv = pvs.at(i);
            err = pv->planUninstallation(installed, ops);
            if (!err.isEmpty())
                break;
        }
    }

    if (err.isEmpty())
        process(ops, WPMUtils::getCloseProcessType());
    else
        addErrorMessage(err, err, true, QMessageBox::Critical);

    qDeleteAll(installed);
    qDeleteAll(pvs);
}

void MainWindow::on_actionAdd_package_triggered()
{
    openURL(QUrl(
            "https://npackd.appspot.com/package/new"));
}

void MainWindow::openURL(const QUrl& url) {
    if (!QDesktopServices::openUrl(url))
        this->addErrorMessage(QObject::tr("Cannot open the URL %1").
                arg(url.toString()));
}

void MainWindow::on_actionOpen_folder_triggered()
{
    Selection* selection = Selection::findCurrent();
    QList<void*> selected;
    if (selection)
        selected = selection->getSelected("PackageVersion");

    QList<PackageVersion*> pvs;
    if (selected.count() == 0) {
        AbstractRepository* r = AbstractRepository::getDefault_();
        if (selection)
            selected = selection->getSelected("Package");
        for (int i = 0; i < selected.count(); i++) {
            Package* p = (Package*) selected.at(i);

            QString err;
            PackageVersion* pv = r->findNewestInstalledPackageVersion_(
                    p->name, &err);
            if (!err.isEmpty())
                addErrorMessage(err, err, true, QMessageBox::Critical);
            if (pv) {
                pvs.append(pv);
            }
        }
    } else {
        for (int i = 0; i < selected.count(); i++) {
            pvs.append(((PackageVersion*) selected.at(i))->clone());
        }
    }

    for (int i = 0; i < pvs.count(); i++) {
        PackageVersion* pv = pvs.at(i);

        QString p = pv->getPath();
        if (!p.isEmpty())
            openURL(QUrl("file:///" + p));
    }

    qDeleteAll(pvs);
}

void MainWindow::visibleJobsChanged()
{
    this->updateProgressTabTitle();
}

void MainWindow::on_actionChoose_columns_triggered()
{
    this->mainFrame->chooseColumns();
}

void MainWindow::on_actionShow_changelog_triggered()
{
    QSet<QUrl> urls;

    Selection* selection = Selection::findCurrent();
    QList<void*> selected;
    if (selection) {
        selected = selection->getSelected("Package");
        if (selected.count() != 0) {
            for (int i = 0; i < selected.count(); i++) {
                Package* p = (Package*) selected.at(i);
                QUrl url(p->getChangeLog());
                if (url.isValid())
                    urls.insert(url);
            }
        } else {
            AbstractRepository* r = AbstractRepository::getDefault_();
            selected = selection->getSelected("PackageVersion");
            for (int i = 0; i < selected.count(); i++) {
                PackageVersion* pv = (PackageVersion*) selected.at(i);
                QScopedPointer<Package> p(r->findPackage_(pv->package));
                if (p) {
                    QUrl url(p->getChangeLog());
                    if (url.isValid())
                        urls.insert(url);
                }
            }
        }
    }

    for (QSet<QUrl>::const_iterator it = urls.begin();
            it != urls.end(); it++) {
        openURL(*it);
    }
}
