#include <math.h>
#include <stdint.h>
#include <memory>
#include <tuple>

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
#include <QObject>
#include <QString>
#include <QStringList>
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
#include <QLabel>
#include <QDockWidget>
#include <QTreeWidget>
#include <QtConcurrent/QtConcurrentRun>
#include <QDialogButtonBox>
#include <QHeaderView>

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
#include "visiblejobs.h"
#include "progresstree2.h"
#include "exportrepositoryframe.h"
#include "asyncdownloader.h"
#include "uimessagehandler.h"
#include "packageutils.h"

extern HWND defaultPasswordWindow;

QIcon MainWindow::genericAppIcon;
QIcon MainWindow::waitAppIcon;
MainWindow* MainWindow::instance = nullptr;

/* creating a tag cloud
QString err;
QList<Package*> ps = DBRepository::getDefault()->findPackages(
        Package::INSTALLED, false, "", &err);
words.clear();
QRegExp re("\\W+", Qt::CaseInsensitive);
for (int i = 0; i < ps.count(); i++) {
    Package* p = ps.at(i);
    QString txt = p->title + " " + p->description;
    QStringList sl = txt.toLower().split(re, QString::SkipEmptyParts);
    sl.removeDuplicates();
    for (int j = 0; j < sl.count(); j++) {
        QString w = sl.at(j);
        if (w.length() > 3) {
            int n = words.value(w);
            n++;
            words.insert(w, n);
        }
    }
}
qDeleteAll(ps);

QStringList stopWords = QString("a an and are as at be but by for if in "
        "into is it no not of on or such that the their then there these "
        "they this to was will with").split(" ");
for (int i = 0; i < stopWords.count(); i++)
    words.remove(stopWords.at(i));
    */

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
std::sort(entries.begin(), entries.end(), comparesi);

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

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    instance = this;

    ui->setupUi(this);

    this->setMenuAccelerators();
    this->setActionAccelerators(this);

    this->taskbarMessageId = 0;

    memset(&nid, 0, sizeof(nid));

    this->pt = nullptr;
    this->jobsTab = nullptr;
    this->taskbarInterface = nullptr;

    this->reloadRepositoriesThreadRunning = false;

    setWindowTitle("Npackd");

    MainWindow::genericAppIcon = QIcon(":/images/app.png");
    MainWindow::waitAppIcon = QIcon(":/images/wait.png");
    this->brokenIcon = QIcon(":/images/broken.png");

    this->mainFrame = new MainFrame(this);

    updateActions();

    // also update packageframe.cpp if adding a new action here
    QTableView* t = this->mainFrame->getTableWidget();
    t->addAction(this->ui->actionInstall);
    t->addAction(this->ui->actionUninstall);
    t->addAction(this->ui->actionUpdate);
    t->addAction(this->ui->actionShow_Details);
    t->addAction(this->ui->actionShow_changelog);
    t->addAction(this->ui->actionRun);
    t->addAction(this->ui->actionOpen_folder);
    t->addAction(this->ui->actionGotoPackageURL);
    t->addAction(this->ui->actionTest_Download_Site);
    t->addAction(this->ui->actionExport);

    t->horizontalHeader()->setContextMenuPolicy(Qt::ActionsContextMenu);
    t->horizontalHeader()->addAction(this->ui->actionChoose_columns);

    connect(&this->fileLoader, SIGNAL(downloadCompleted(QString,QString,QString)), this,
            SLOT(downloadCompleted(QString,QString,QString)),
            Qt::QueuedConnection);

    connect(&this->downloadSizeFinder,
            SIGNAL(downloadCompleted(QString,int64_t)), this,
            SLOT(downloadSizeCompleted(QString,qlonglong)),
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
	QTabBar *tabBar = this->ui->tabWidget->findChild<QTabBar *>();
    tabBar->setTabButton(0, QTabBar::RightSide, nullptr);
    this->loadUISettings();

    this->addJobsTab();
    tabBar->setTabButton(1, QTabBar::RightSide, nullptr);
    connect(VisibleJobs::getDefault(), SIGNAL(changed()),
            this, SLOT(visibleJobsChanged()));

    this->mainFrame->getFilterLineEdit()->setFocus();

    InstalledPackages* ip = InstalledPackages::getDefault();
    connect(ip, SIGNAL(statusChanged(const QString&, const Version&)), this,
            SLOT(repositoryStatusChanged(const QString&, const Version&)),
            Qt::QueuedConnection);

    defaultPasswordWindow = reinterpret_cast<HWND>(this->winId());

    this->taskbarMessageId = RegisterWindowMessage(L"TaskbarButtonCreated");
    // qCDebug(npackd) << "id " << taskbarMessageId;

    // Npackd runs elevated and the taskbar does not. We have to allow the
    // taskbar event here.
    HINSTANCE hInstLib = LoadLibraryA("USER32.DLL");
	typedef BOOL(WINAPI *LPFCHANGEWINDOWMESSAGEFILTEREX)
		(HWND, UINT, DWORD, void*);
	LPFCHANGEWINDOWMESSAGEFILTEREX lpfChangeWindowMessageFilterEx =
            reinterpret_cast<LPFCHANGEWINDOWMESSAGEFILTEREX>(
            GetProcAddress(hInstLib, "ChangeWindowMessageFilterEx"));
    if (lpfChangeWindowMessageFilterEx) {
        lpfChangeWindowMessageFilterEx(reinterpret_cast<HWND>(winId()),
                taskbarMessageId, 1, nullptr);
        // qCDebug(npackd) << "allow taskbar event " << taskbarMessageId;
    }
    FreeLibrary(hInstLib);
}

void MainWindow::applicationFocusChanged(QWidget* /*old*/, QWidget* /*now*/)
{
    updateActions();
}

bool MainWindow::nativeEvent(const QByteArray & /*eventType*/, void * message,
        long * /*result*/)
{
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == taskbarMessageId) {
        // qCDebug(npackd) << "taskbarmessageid";
        HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr,
                CLSCTX_INPROC_SERVER, IID_ITaskbarList3,
                reinterpret_cast<void**> (&(taskbarInterface)));

        if (SUCCEEDED(hr)) {
            hr = taskbarInterface->HrInit();

            if (FAILED(hr)) {
                taskbarInterface->Release();
                taskbarInterface = nullptr;
            }
        }
        return true;
    } else if (msg->message == WM_ICONTRAY) {
        //QMessageBox::critical(nullptr, QObject::tr("Warn"), "wm_icontray");
        // qCDebug(npackd) << "MainWindow::winEvent " << message->lParam;
        switch (msg->lParam) {
            case (LPARAM) NIN_BALLOONUSERCLICK:
                this->mainFrame->setStatusFilter(2);
                //this->prepare();
                ((QApplication*) QApplication::instance())->
                        setQuitOnLastWindowClosed(true);
                this->showMaximized();
                this->activateWindow();
                this->hideIconInSystemTray();
                break;
            case (LPARAM) NIN_BALLOONTIMEOUT:
                this->hideIconInSystemTray();
                ((QApplication*) QApplication::instance())->quit();
                break;
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
                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));

                openPackageVersion(pv->package, pv->version, true);
            }
        } else {
            selected = sel->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = static_cast<Package*>(selected.at(i));

                int index = this->findPackageTab(p->name);
                if (index < 0) {
                    PackageFrame* pf = new PackageFrame(this->ui->tabWidget);
                    Package* p_ = DBRepository::getDefault()->
                            findPackage_(p->name);
                    if (p_) {
                        pf->fillForm(p_);
                        QIcon icon = getPackageIcon(p->name);
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
    PackageItemModel* m = static_cast<PackageItemModel*>(t->model());
    m->downloadSizeUpdated(url);
}

void MainWindow::updateIcon(const QString& url)
{
    QTableView* t = this->mainFrame->getTableWidget();
    PackageItemModel* m = static_cast<PackageItemModel*>(t->model());

    for (int row = t->rowAt(0); row <= t->rowAt(t->height()); row++) {
        QModelIndex index = m->index(row, 0);
        QString v = m->data(index, Qt::UserRole).toString();
        if (v == url) {
            m->dataChanged(index, index);
        }
    }

    for (int i = 0; i < this->ui->tabWidget->count(); i++) {
        QWidget* w = this->ui->tabWidget->widget(i);
        PackageVersionForm* pvf = dynamic_cast<PackageVersionForm*>(w);
        if (pvf) {
            pvf->updateIcons();
            QIcon icon = getPackageIcon(pvf->pv->package);
            this->ui->tabWidget->setTabIcon(i, icon);
        }
        PackageFrame* pf = dynamic_cast<PackageFrame*>(w);
        if (pf) {
            pf->updateIcons(url);
            QIcon icon = getPackageIcon(pf->p->name);
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
            //qCDebug(npackd) << pvf->pv.data()->toString() << "---" <<
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
            //qCDebug(npackd) << pvf->pv.data()->toString() << "---" <<
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

QIcon MainWindow::getPackageIcon(const QString& package)
{
    MainWindow* mw = MainWindow::getInstance();
    DBRepository* r = DBRepository::getDefault();
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
    // qCDebug(npackd) << "MainWindow::loadUISettings" << err;
    if (err.isEmpty()) {
        QByteArray ba = n.getBytes("MainWindowState", &err);
        // qCDebug(npackd) << "MainWindow::loadUISettings error: " << err;
        if (err.isEmpty()) {
            // qCDebug(npackd) << "MainWindow::loadUISettings" << ba.length();
            this->restoreState(ba);
        }

        ba = n.getBytes("MainWindowGeometry", &err);
        // qCDebug(npackd) << "MainWindow::loadUISettings error: " << err;
        if (err.isEmpty()) {
            // qCDebug(npackd) << "MainWindow::loadUISettings" << ba.length();
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
        QString msg = QObject::tr("Cannot exit while jobs are running");
        addErrorMessage(msg, msg, true, QMessageBox::Critical);
        event->ignore();
    }
}

void MainWindow::repositoryStatusChanged(const QString& package,
        const Version& version)
{
    // qCDebug(npackd) << "MainWindow::repositoryStatusChanged" << pv->toString();

    QTableView* t = this->mainFrame->getTableWidget();
    PackageItemModel* m = static_cast<PackageItemModel*>(t->model());
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
        const QString& /*filename*/, const QString& /*error*/)
{
    updateIcon(url);
}

void MainWindow::downloadSizeCompleted(const QString& url, int64_t /*size*/)
{
    QTableView* t = this->mainFrame->getTableWidget();
    PackageItemModel* m = static_cast<PackageItemModel*>(t->model());
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
    int n = VisibleJobs::getDefault()->runningJobs.count();
    time_t max = -1;
    double maxProgress = 0;
    for (int i = 0; i < n; i++) {
        Job* state = VisibleJobs::getDefault()->runningJobs.at(i);

        // state.job may be null if the corresponding task was just started
        time_t t = state->remainingTime();
        if (t > max) {
            max = t;
            maxProgress = state->getProgress();
        }
    }
    if (max < 0)
        max = 0;

    int maxProgress_ = lround(maxProgress * 100);
    QTime rest = WPMUtils::durationToTime(max);

    QString title;
    if (n == 0)
        title = QObject::tr("0 Jobs");
    else if (n == 1)
        title = QObject::tr("1 Job (%1%, %2)").arg(
                QString::number(maxProgress_),
                rest.toString());
    else
        title = QObject::tr("%1 Jobs (%2%, %3)").
                arg(QString::number(n), QString::number(maxProgress_),
                rest.toString());

    int index = this->ui->tabWidget->indexOf(this->jobsTab);
    if (this->ui->tabWidget->tabText(index) != title)
        this->ui->tabWidget->setTabText(index, title);

    if (this->taskbarInterface) {
        HWND w = reinterpret_cast<HWND>(winId());
        if (n == 0)
            taskbarInterface->SetProgressState(w, TBPF_NOPROGRESS);
        else {
            taskbarInterface->SetProgressState(w, TBPF_NORMAL);
            taskbarInterface->SetProgressValue(w,
                    static_cast<ULONGLONG>(lround(maxProgress * 10000)),
                    10000);
        }
    }
}

MainWindow::~MainWindow()
{
    QThreadPool::globalInstance()->clear();
    QThreadPool::globalInstance()->waitForDone(-1);

    // if a timeout of 5 seconds is used here, there may an access
    // violation during program shutdown
    DownloadSizeFinder::threadPool.clear();
    DownloadSizeFinder::threadPool.waitForDone(-1);

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

                r = *inCache;
                icons.insert(url, inCache);
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

void MainWindow::showIconInSystemTray(int nupdates)
{
    nid.cbSize = sizeof(nid);

    nid.hWnd = (HWND) winId();
    nid.uID = 0;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
    nid.uCallbackMessage = WM_ICONTRAY;
    nid.hIcon = LoadIconW(GetModuleHandle(0),
                    L"IDI_ICON1");
    qDebug() << "main().1 icon" << nid.hIcon;
    QString tip = QString("%1 update(s) found").arg(nupdates);
    QString txt = QString("Npackd found %1 update(s). "
            "Click here to review and install.").arg(nupdates);

    wcsncpy(nid.szTip, (wchar_t*) tip.utf16(),
            sizeof(nid.szTip) / sizeof(nid.szTip[0]) - 1);
    wcsncpy(nid.szInfo, (wchar_t*) txt.utf16(),
            sizeof(nid.szInfo) / sizeof(nid.szInfo[0]) - 1);
    nid.uVersion = 3; // NOTIFYICON_VERSION
    wcsncpy(nid.szInfoTitle, (wchar_t*) tip.utf16(),
            sizeof(nid.szInfoTitle) / sizeof(nid.szInfoTitle[0]) - 1);
    nid.dwInfoFlags = 1; // NIIF_INFO
    nid.uTimeout = 30000;

    if (!Shell_NotifyIconW(NIM_ADD, &nid))
        qDebug() << "Shell_NotifyIconW failed";
}

void MainWindow::hideIconInSystemTray()
{
    if (!Shell_NotifyIconW(NIM_DELETE, &nid))
        qDebug() << "Shell_NotifyIconW failed";
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
            QPixmap pm;
            pm.load(filename);

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
                r = MainWindow::brokenIcon;
                screenshots.insert(url, r);
            }
        } else {
            r = MainWindow::waitAppIcon;
        }
    }
    return r;
}

void MainWindow::monitoredJobCompleted()
{
    Job* job = reinterpret_cast<Job*>(sender());
    if (!job->getErrorMessage().isEmpty()) {
        addErrorMessage(job->getErrorMessage(),
                job->getTitle() + ": " + job->getErrorMessage(),
                true, QMessageBox::Critical);
    }

    /*
    QStringList logMessages = getLogMessages();
    if (!logMessages.isEmpty()) {
        addTextTab("log", logMessages.join('\n'));
    }
    */

    VisibleJobs::getDefault()->unregisterJob(job);
    pt->removeJob(job);
    updateProgressTabTitle();
    job->disconnect();
    job->deleteLater();
}

void MainWindow::monitoredJobChanged(Job* state)
{
    time_t now;
    time(&now);

    if (!state->parentJob) {
        VisibleJobs::getDefault()->monitoredJobLastChanged = now;

        updateProgressTabTitle();
    }
}

void MainWindow::monitor(Job* job)
{
    connect(job, SIGNAL(changed(Job*)), this,
            SLOT(monitoredJobChanged(Job*)),
            Qt::QueuedConnection);

    connect(job, SIGNAL(jobCompleted()), this,
            SLOT(monitoredJobCompleted()),
            Qt::QueuedConnection);

    VisibleJobs::getDefault()->runningJobs.append(job);

    updateProgressTabTitle();

    pt->addJob(job);
}

void MainWindow::onShow()
{
    this->ui->actionToggle_toolbar->setChecked(
            this->ui->mainToolBar->isVisible());

    DBRepository* dbr = DBRepository::getDefault();
    QString err = dbr->openDefault();

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
    QSet<QString> packageNames;
    for (int i = 0; i < ps.count(); i++) {
        packageNames.insert(ps.at(i)->name);
    }

    QTableView* t = this->mainFrame->getTableWidget();
    t->clearSelection();
    QAbstractItemModel* m = t->model();
    for (int i = 0; i < m->rowCount(); i++) {
        const QVariant v = m->data(m->index(i, 1), Qt::UserRole);
        QString name = v.toString();
        if (packageNames.contains(name)) {
            QModelIndex topLeft = t->model()->index(i, 0);

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

_SearchResult MainWindow::search(Package::Status minStatus,
        Package::Status maxStatus,
        const QString& query, int cat0, int cat1, QString* err)
{
    //DWORD start = GetTickCount();

    _SearchResult r;
    *err = "";

    DBRepository* dbr = DBRepository::getDefault();
    r.found = dbr->findPackages(minStatus, maxStatus, query,
            cat0, cat1, err);

    //DWORD search = GetTickCount();
    //qCDebug(npackd) << "Only search" << (search - start) << query;

    if (err->isEmpty()) {
        r.cats = dbr->findCategories(minStatus, maxStatus, query, 0, -1, -1,
                err);
    }

    if (err->isEmpty()) {
        if (cat0 >= 0) {
            r.cats1 = dbr->findCategories(minStatus, maxStatus, query, 1,
                    cat0, -1, err);
        }
    }

    //qCDebug(npackd) << "Only categories" << (GetTickCount() - search);

    return r;
}

void MainWindow::fillListInBackground()
{
    fillList();
}

void MainWindow::fillList()
{
    DWORD start = GetTickCount();

    // qCDebug(npackd) << "MainWindow::fillList";
    QTableView* t = this->mainFrame->getTableWidget();

    t->setUpdatesEnabled(false);

    QString query = this->mainFrame->getFilterLineEdit()->text();

    //QSet<QString> requestedIcons;
    int statusFilter = this->mainFrame->getStatusFilter();
    Package::Status minStatus, maxStatus;
    switch (statusFilter) {
        case 1:
            minStatus = Package::INSTALLED;
            maxStatus = Package::NOT_INSTALLED_NOT_AVAILABLE;
            break;
        case 2:
            minStatus = Package::UPDATEABLE;
            maxStatus = Package::NOT_INSTALLED_NOT_AVAILABLE;
            break;
        default:
            minStatus = Package::NOT_INSTALLED;
            maxStatus = Package::NOT_INSTALLED_NOT_AVAILABLE;
            break;
    }

    int cat0 = this->mainFrame->getCategoryFilter(0);
    int cat1 = this->mainFrame->getCategoryFilter(1);

    QString err;
    _SearchResult sr = search(minStatus, maxStatus, query, cat0, cat1, &err);

    if (err.isEmpty()) {
        this->mainFrame->setCategories(0, sr.cats);
        this->mainFrame->setCategoryFilter(0, cat0);
        this->mainFrame->setCategories(1, sr.cats1);
        this->mainFrame->setCategoryFilter(1, cat1);
    } else {
        addErrorMessage(err, err, true, QMessageBox::Critical);
    }

    PackageItemModel* m = static_cast<PackageItemModel*>(t->model());
    m->setPackages(sr.found);
    t->setUpdatesEnabled(true);
    t->horizontalHeader()->setSectionsMovable(true);

    DWORD dur = GetTickCount() - start;

    this->mainFrame->setDuration(static_cast<int>(dur));
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

void MainWindow::process(QList<InstallOperation*> &install,
        DWORD programCloseType)
{
    QString err;

    bool confirmed = false;
    QString title;
    if (err.isEmpty())
        confirmed = UIUtils::confirmInstallOperations(this, install, &title,
            &err);

    if (err.isEmpty()) {
        if (confirmed) {
            DBRepository* rep = DBRepository::getDefault();

            if (rep->includesRemoveItself(install)) {
                QString txt = QObject::tr("Chosen changes require an update of this Npackd instance. Are you sure?");
                if (UIUtils::confirm(this, QObject::tr("Warning"), txt, txt)) {
                    Job* job = new Job(title);
                    UIUtils::processWithSelfUpdate(job, install,
                            programCloseType);
                    delete job;
                }
            } else {
                Job* job = new Job(title);

                connect(job, SIGNAL(jobCompleted()), this,
                        SLOT(processThreadFinished()),
                        Qt::QueuedConnection);

                monitor(job);

                QtConcurrent::run(reinterpret_cast<AbstractRepository*>(rep),
                        &DBRepository::processWithCoInitializeAndFree,
                        job, install,
                        PackageUtils::getCloseProcessType());

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

void MainWindow::on_errorMessage(const QString &msg,
        const QString &details, bool autoHide, QMessageBox::Icon icon)
{
    addErrorMessage(msg, details, autoHide, icon);
}

void MainWindow::processThreadFinished()
{
    QTableView* t = this->mainFrame->getTableWidget();
    QItemSelectionModel* sm = t->selectionModel();
    QList<Package*> sel = mainFrame->getSelectedPackagesInTable();
    for (int i = 0; i < sel.count(); i++) {
        sel[i] = new Package(*sel[i]);
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
        // qCDebug(npackd) << "QEvent::ActivationChange";
        QTimer::singleShot(0, this, SLOT(updateActionsSlot()));
        break;
    default:
        break;
    }
}

void MainWindow::on_actionExit_triggered()
{
    int n = VisibleJobs::getDefault()->runningJobs.count();

    if (n > 0) {
        QString msg = QObject::tr("Cannot exit while jobs are running");
        addErrorMessage(msg, msg, true, QMessageBox::Critical);
    } else
        this->close();
}

bool MainWindow::isUpdateEnabled(const QString& package)
{
    QString err;

    bool res = false;
    DBRepository* r = DBRepository::getDefault();
    PackageVersion* newest = r->findNewestInstallablePackageVersion_(
            package, &err);
    InstalledPackageVersion* newesti = InstalledPackages::getDefault()->getNewestInstalled(
            package);
    if (newest != nullptr && newesti != nullptr) {
        // qCDebug(npackd) << newest->version.getVersionString() << " " <<
                newesti->version.getVersionString();
        bool canInstall = !newest->isLocked() && !newest->installed() &&
                newest->download.isValid();
        bool canUninstall = !PackageVersion::isLocked(newesti->package, newesti->version) &&
                !newesti->isInWindowsDir();

        // qCDebug(npackd) << canInstall << " " << canUninstall;

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
    updateShowFolderAction();
    updateRunAction();
    updateExportAction();
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

                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));

                enabled = enabled &&
                        pv && !pv->isLocked() &&
                        !pv->installed() &&
                        pv->download.isValid();
            }
        } else {
            DBRepository* r = DBRepository::getDefault();
            selected = selection->getSelected("Package");
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                Package* p = static_cast<Package*>(selected.at(i));
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

void MainWindow::updateExportAction()
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

                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));

                enabled = enabled &&
                        pv && !pv->isLocked() &&
                        pv->download.isValid();
            }
        } else {
            DBRepository* r = DBRepository::getDefault();
            selected = selection->getSelected("Package");
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                Package* p = static_cast<Package*>(selected.at(i));
                QString err;
                PackageVersion* pv  = r->findNewestInstallablePackageVersion_(
                        p->name, &err);

                enabled = enabled &&
                        pv && !pv->isLocked() &&
                        pv->download.isValid();

                delete pv;
            }
        }
    }

    this->ui->actionExport->setEnabled(enabled);
}

void MainWindow::updateShowFolderAction()
{
    // qCDebug(npackd) << "MainWindow::updateUninstallAction start";

    Selection* selection = Selection::findCurrent();

    bool enabled = false;
    if (selection) {
        QList<void*> selected = selection->getSelected("PackageVersion");
        if (selected.count() > 0) {
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));

                enabled = enabled &&
                        pv && !pv->isLocked() &&
                        pv->installed() && !pv->isInWindowsDir();
            }
            // qCDebug(npackd) << "MainWindow::updateUninstallAction 2:" << selected.count();
        } else {
            QList<void*> selected = selection->getSelected("Package");
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                Package* p = static_cast<Package*>(selected.at(i));

                QString err;
                InstalledPackageVersion* pv = InstalledPackages::getDefault()->getNewestInstalled(
                        p->name);

                enabled = enabled &&
                        pv && !PackageVersion::isLocked(pv->package, pv->version) &&
                        pv->installed() && !pv->isInWindowsDir();

                delete pv;
            }
        }
    }
    this->ui->actionOpen_folder->setEnabled(enabled);
    // qCDebug(npackd) << "MainWindow::updateUninstallAction end " << enabled;
}

void MainWindow::updateUninstallAction()
{
    // qCDebug(npackd) << "MainWindow::updateUninstallAction start";

    Selection* selection = Selection::findCurrent();

    bool enabled = false;
    if (selection) {
        QList<void*> selected = selection->getSelected("PackageVersion");
        if (selected.count() > 0) {
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));

                enabled = enabled &&
                        pv && !pv->isLocked() &&
                        pv->installed() && !pv->isInWindowsDir();
            }
            // qCDebug(npackd) << "MainWindow::updateUninstallAction 2:" << selected.count();
        } else {
            QList<void*> selected = selection->getSelected("Package");
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                Package* p = static_cast<Package*>(selected.at(i));

                InstalledPackageVersion* pv = InstalledPackages::getDefault()->getNewestInstalled(
                        p->name);

                enabled = enabled &&
                        pv && !PackageVersion::isLocked(pv->package, pv->version) &&
                        pv->installed() && !pv->isInWindowsDir();

                delete pv;
            }
        }
    }
    this->ui->actionUninstall->setEnabled(enabled);
    // qCDebug(npackd) << "MainWindow::updateUninstallAction end " << enabled;
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

                Package* p = static_cast<Package*>(selected.at(i));

                enabled = enabled && isUpdateEnabled(p->name);
            }
        } else {
            selected = selection->getSelected("PackageVersion");
            enabled = selected.count() >= 1;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));

                enabled = enabled && isUpdateEnabled(pv->package);
            }
        }
    }
    this->ui->actionUpdate->setEnabled(enabled);
}

void MainWindow::updateReloadRepositoriesAction()
{
    this->ui->actionReload_Repositories->setEnabled(
            !reloadRepositoriesThreadRunning);
}

void MainWindow::updateCloseTabAction()
{
    QTabWidget* t = this->ui->tabWidget;
    bool e = this->ui->tabWidget->tabBar()->tabButton(t->currentIndex(), QTabBar::RightSide) != nullptr;
    this->ui->actionClose_Tab->setEnabled(e);
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
                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));
                if (pv->download.isValid()) {
                    enabled = true;
                    break;
                }
            }
        } else {
            DBRepository* r = DBRepository::getDefault();
            selected = selection->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = static_cast<Package*>(selected.at(i));
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
            DBRepository* r = DBRepository::getDefault();
            for (int i = 0; i < selected.count(); i++) {
                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));

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
                Package* p = static_cast<Package*>(selected.at(i));

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

void MainWindow::updateRunAction()
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

                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));

                enabled = enabled &&
                        pv && !pv->isLocked() &&
                        pv->installed() &&
                        pv->importantFiles.size() == 1;
            }
            // qCDebug(npackd) << "MainWindow::updateUninstallAction 2:" << selected.count();
        } else {
            DBRepository* r = DBRepository::getDefault();
            QList<void*> selected = selection->getSelected("Package");
            enabled = selected.count() > 0;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                Package* p = static_cast<Package*>(selected.at(i));

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
                        pv->installed() &&
                        pv->importantFiles.size() == 1;

                delete pv;
            }
        }
    }
    this->ui->actionRun->setEnabled(enabled);
}

void MainWindow::updateGotoPackageURLAction()
{
    Selection* selection = Selection::findCurrent();
    bool enabled = false;
    if (selection) {
        QList<void*> selected = selection->getSelected("PackageVersion");
        if (selected.count() > 0) {
            DBRepository* r = DBRepository::getDefault();
            for (int i = 0; i < selected.count(); i++) {
                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));

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
                Package* p = static_cast<Package*>(selected.at(i));

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
        if (pvf != nullptr || lf != nullptr || pf != nullptr) {
            this->ui->tabWidget->removeTab(i);
        } else {
            i++;
        }
    }
}

void MainWindow::recognizeAndLoadRepositories(bool useCache)
{
    Job* job = new Job(
            QObject::tr("Reloading repositories and detecting installed software"));

    connect(job, SIGNAL(jobCompleted()), this,
            SLOT(recognizeAndLoadRepositoriesThreadFinished()),
            Qt::QueuedConnection);

    this->reloadRepositoriesThreadRunning = true;
    updateActions();

    monitor(job);

    QtConcurrent::run(DBRepository::getDefault(),
            &DBRepository::updateF5Runnable,
            job, useCache);
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

    UIUtils::chooseAccelerators(&titles);
    for (int i = 0, j = 0; i < mb->children().count(); i++) {
        QMenu* m = dynamic_cast<QMenu*>(mb->children().at(i));
        if (m) {
            m->setTitle(titles.at(j));
            j++;
        }
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
    UIUtils::chooseAccelerators(&titles);
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
    DBRepository::getDefault()->clearCache();

    QTableView* t = this->mainFrame->getTableWidget();
    QItemSelectionModel* sm = t->selectionModel();
    QList<Package*> sel = mainFrame->getSelectedPackagesInTable();
    for (int i = 0; i < sel.count(); i++) {
        sel[i] = new Package(*sel[i]);
    }
    QModelIndex index = sm->currentIndex();

    PackageItemModel* m = static_cast<PackageItemModel*>(t->model());
    m->setPackages(QStringList());
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
            QIcon icon = getPackageIcon(package);
            this->ui->tabWidget->addTab(pvf, icon, pv_->toString());
            index = this->ui->tabWidget->count() - 1;
        }
    }
    if (select)
        this->ui->tabWidget->setCurrentIndex(index);
}

void MainWindow::openPackage(const QString& package, bool select)
{
    int index = this->findPackageTab(package);
    if (index < 0) {
        PackageFrame* pf = new PackageFrame(this->ui->tabWidget);
        Package* p_ = DBRepository::getDefault()->findPackage_(package);
        if (p_) {
            pf->fillForm(p_);
            QIcon icon = getPackageIcon(package);
            QString t = p_->title;
            if (t.isEmpty())
                t = package;
            this->ui->tabWidget->addTab(pf, icon, t);
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
                Package* p = static_cast<Package*>(selected.at(i));
                QUrl url(p->url);
                if (url.isValid())
                    urls.insert(url);
            }
        } else {
            DBRepository* r = DBRepository::getDefault();
            selected = selection->getSelected("PackageVersion");
            for (int i = 0; i < selected.count(); i++) {
                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));
                std::unique_ptr<Package> p(r->findPackage_(pv->package));
                if (p) {
                    QUrl url(p->url);
                    if (url.isValid())
                        urls.insert(url);
                }
            }
        }
    }

    for (QSet<QUrl>::const_iterator it = urls.begin();
            it != urls.end(); ++it) {
        openURL(*it);
    }
}

void MainWindow::on_actionSettings_triggered()
{
    SettingsFrame* d = nullptr;
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

        QMenuBar* mb = this->menuBar();

        QStringList titles;
        for (int i = 0; i < mb->children().count(); i++) {
            QMenu* m = dynamic_cast<QMenu*>(mb->children().at(i));
            if (m) {
                titles.append(m->title());
            }
        }

        UIUtils::chooseAccelerators(d, UIUtils::extractAccelerators(titles));

        d->fillRepositories();

        d->setInstallationDirectory(PackageUtils::getInstallationDirectory());

        d->setCloseProcessType(PackageUtils::getCloseProcessType());

        this->ui->tabWidget->addTab(d, QObject::tr("Settings"));
        this->ui->tabWidget->setCurrentIndex(this->ui->tabWidget->count() - 1);
    }
}

void MainWindow::on_actionUpdate_triggered()
{
    QString err;

    QList<Package*> packages;
    DBRepository* r = DBRepository::getDefault();
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
                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));
                if (!used.contains(pv->package)) {
                    Package* p = r->findPackage_(pv->package);

                    if (p != nullptr) {
                        packages.append(p);
                        used.insert(pv->package);
                    }
                }
            }
        } else {
            if (sel) {
                selected = sel->getSelected("Package");
                for (int i = 0; i < selected.count(); i++) {
                    Package* p = static_cast<Package*>(selected.at(i));
                    packages.append(new Package(*p));
                }
            }
        }
    }

    QList<InstallOperation*> ops;
    InstalledPackages installed(*InstalledPackages::getDefault());

    if (err.isEmpty()) {
        err = DBRepository::getDefault()->planAddMissingDeps(installed, ops);
    }

    if (err.isEmpty() && packages.count() > 0) {
        err = r->planUpdates(installed, packages, QList<Dependency*>(), ops,
                true, false, "", false);
    }

    if (err.isEmpty()) {
        if (ops.count() > 0) {
            process(ops, PackageUtils::getCloseProcessType());
        }
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
                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));
                urls.insert(pv->download.host());
            }
        } else {
            DBRepository* r = DBRepository::getDefault();
            selected = sel->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = static_cast<Package*>(selected.at(i));
                std::unique_ptr<PackageVersion> pv(
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
                    it != urls.end(); ++it) {
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
            QObject::tr("<html><body>Npackd %1 - "
            "software package manager for Windows (R)"
            "<ul>"
            "<li><a href='https://www.npackd.org/'>Home page (https://www.npackd.org)</a></li>"
            "<li><a href='https://github.com/tim-lebedkov/npackd/wiki/ChangeLog'>Changelog</a></li>"
            "<li><a href='https://github.com/tim-lebedkov/npackd/wiki'>Documentation</a></li>"
            "<li>Author: <a href='https://github.com/tim-lebedkov'>Tim Lebedkov</a></li>"
            "</ul>"
            "Contributors:"
            "<ul>"
            "<li><a href='https://github.com/OgreTransporter'>OgreTransporter</a>: Visual C++ support, CMake integration, group policy configuration, non-admin installations</li>"
            "</ul>"
            "</body></html>")).
            arg(NPACKD_VERSION), true);
}

void MainWindow::on_tabWidget_tabCloseRequested(int index)
{
    QWidget* w = this->ui->tabWidget->widget(index);
    if (w != this->mainFrame && w != this->jobsTab) {
        this->ui->tabWidget->removeTab(index);
    }
}

void MainWindow::on_tabWidget_currentChanged(int /*index*/)
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

bool comparesi(const QPair<QString, int>& a, const QPair<QString, int>& b)
{
    return a.second > b.second;
}

void MainWindow::addErrorMessage(const QString& msg, const QString& details,
        bool autoHide, QMessageBox::Icon icon)
{
    bool dots = false;
    QString m = msg.trimmed();
    if (m.length() > 200) {
        m = m.left(200).trimmed();
        dots = true;
    }
    QStringList sl = m.split("\r\n");
    if (sl.count() > 2) {
        m = sl.at(0) + "\r\n" + sl.at(1);
        dots = true;
    }
    if (dots)
        m += "...";

    MessageFrame* label = new MessageFrame(this->centralWidget(), m,
            details, autoHide ? 30 : 0, icon);
    QVBoxLayout* layout = static_cast<QVBoxLayout*>(
            this->centralWidget()->layout());
    layout->insertWidget(0, label);
}

void MainWindow::on_actionReload_Repositories_triggered()
{
    QString lockedPackage;
    Version lockedVersion;
    PackageVersion::findLockedPackageVersion(&lockedPackage, &lockedVersion);

    if (!lockedPackage.isEmpty()) {
        QString err;
        PackageVersion* locked = DBRepository::getDefault()->findPackageVersion_(lockedPackage, lockedVersion, &err);
        QString name;
        if (locked) {
            name = locked->toString();
            delete locked;
        } else {
            name = lockedPackage + " " + lockedVersion.getVersionString();
        }

        QString msg = QObject::tr(
                "Cannot reload the repositories now. The package %1 is locked by a currently running installation/removal.").
                arg(name);

        this->addErrorMessage(msg, msg, true, QMessageBox::Critical);
    } else {
        recognizeAndLoadRepositories(false);
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
            "https://github.com/tim-lebedkov/npackd/issues/new"));
}

void MainWindow::on_actionInstall_triggered()
{
    DBRepository* dbr = DBRepository::getDefault();

    QString err;

    QList<PackageVersion*> pvs;

    if (err.isEmpty()) {
        Selection* selection = Selection::findCurrent();
        QList<void*> selected;
        if (selection)
            selected = selection->getSelected("PackageVersion");

        if (selected.count() == 0) {
            if (selection)
                selected = selection->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = static_cast<Package*>(selected.at(i));
                PackageVersion* pv = dbr->findNewestInstallablePackageVersion_(
                        p->name, &err);
                if (!err.isEmpty())
                    break;

                if (pv)
                    pvs.append(pv);
            }
        } else {
            for (int i = 0; i < selected.count(); i++) {
                pvs.append(static_cast<PackageVersion*>(selected.at(i))->clone());
            }
        }
    }

    QList<InstallOperation*> ops;
    InstalledPackages installed(*InstalledPackages::getDefault());
    QList<PackageVersion*> avoid;

    if (err.isEmpty()) {
        err = dbr->planAddMissingDeps(installed, ops);
    }

    if (err.isEmpty()) {
        for (int i = 0; i < pvs.count(); i++) {
            PackageVersion* pv = pvs.at(i);

            qDeleteAll(avoid);
            avoid.clear();
            err = pv->planInstallation(dbr, installed, ops, avoid);
            if (!err.isEmpty())
                break;
        }
    }

    if (err.isEmpty())
        process(ops, PackageUtils::getCloseProcessType());
    else
        addErrorMessage(err, err, true, QMessageBox::Critical);

    qDeleteAll(avoid);
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
            DBRepository* r = DBRepository::getDefault();
            if (selection)
                selected = selection->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = static_cast<Package*>(selected.at(i));

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
                pvs.append(static_cast<PackageVersion*>(selected.at(i))->
                        clone());
            }
        }
    }

    QList<InstallOperation*> ops;
    InstalledPackages installed(*InstalledPackages::getDefault());

    if (err.isEmpty()) {
        err = DBRepository::getDefault()->planAddMissingDeps(installed, ops);
    }

    if (err.isEmpty()) {
        for (int i = 0; i < pvs.count(); i++) {
            PackageVersion* pv = pvs.at(i);
            err = DBRepository::getDefault()->planUninstallation(installed,
                    pv->package, pv->version, ops);
            if (!err.isEmpty())
                break;
        }
    }

    if (err.isEmpty())
        process(ops, PackageUtils::getCloseProcessType());
    else
        addErrorMessage(err, err, true, QMessageBox::Critical);

    qDeleteAll(pvs);
}

void MainWindow::on_actionAdd_package_triggered()
{
    openURL(QUrl(
            "https://www.npackd.org/package/new"));
}

void MainWindow::openURL(const QUrl& url) {
    if (!QDesktopServices::openUrl(url)) {
        QString err = QObject::tr("Cannot open the URL %1").
                arg(url.toString());
        this->addErrorMessage(err, err, true, QMessageBox::Critical);
    }
}

void MainWindow::on_actionOpen_folder_triggered()
{
    Selection* selection = Selection::findCurrent();
    QList<void*> selected;
    if (selection)
        selected = selection->getSelected("PackageVersion");

    QList<PackageVersion*> pvs;
    if (selected.count() == 0) {
        DBRepository* r = DBRepository::getDefault();
        if (selection)
            selected = selection->getSelected("Package");
        for (int i = 0; i < selected.count(); i++) {
            Package* p = static_cast<Package*>(selected.at(i));

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
            pvs.append(static_cast<PackageVersion*>(selected.at(i))->clone());
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
                Package* p = static_cast<Package*>(selected.at(i));
                QUrl url(p->getChangeLog());
                if (url.isValid())
                    urls.insert(url);
            }
        } else {
            DBRepository* r = DBRepository::getDefault();
            selected = selection->getSelected("PackageVersion");
            for (int i = 0; i < selected.count(); i++) {
                PackageVersion* pv = static_cast<PackageVersion*>(
                        selected.at(i));
                std::unique_ptr<Package> p(r->findPackage_(pv->package));
                if (p) {
                    QUrl url(p->getChangeLog());
                    if (url.isValid())
                        urls.insert(url);
                }
            }
        }
    }

    for (QSet<QUrl>::const_iterator it = urls.begin();
            it != urls.end(); ++it) {
        openURL(*it);
    }
}

void MainWindow::on_actionToggle_toolbar_triggered(bool checked)
{
    this->ui->mainToolBar->setVisible(checked);
}

void MainWindow::on_mainToolBar_visibilityChanged(bool visible)
{
    this->ui->actionToggle_toolbar->setChecked(visible);
}

void MainWindow::on_actionRun_triggered()
{
    QString err;

    QList<PackageVersion*> pvs;

    if (err.isEmpty()) {
        Selection* selection = Selection::findCurrent();
        QList<void*> selected;
        if (selection)
            selected = selection->getSelected("PackageVersion");

        if (selected.count() == 0) {
            DBRepository* r = DBRepository::getDefault();
            if (selection)
                selected = selection->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = static_cast<Package*>(selected.at(i));

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
                pvs.append(static_cast<PackageVersion*>(selected.at(i))->
                        clone());
            }
        }
    }

    if (err.isEmpty()) {
        for (int i = 0; i < pvs.count(); i++) {
            PackageVersion* pv = pvs.at(i);
            if (pv->importantFiles.size() == 1) {
                QString impf = pv->importantFiles.at(0);

                QString filename = pv->getPath() + "\\" + impf;

                if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filename))) {
                    QString msg = QObject::tr("Cannot open the file %1").
                            arg(filename);
                    addErrorMessage(msg, msg, true, QMessageBox::Critical);
                }
            }
        }
    }

    qDeleteAll(pvs);
}

void MainWindow::on_actionExport_triggered()
{
    QString err;

    QList<PackageVersion*> pvs;

    if (err.isEmpty()) {
        Selection* selection = Selection::findCurrent();
        QList<void*> selected;
        if (selection)
            selected = selection->getSelected("PackageVersion");

        if (selected.count() == 0) {
            DBRepository* r = DBRepository::getDefault();
            if (selection)
                selected = selection->getSelected("Package");
            for (int i = 0; i < selected.count(); i++) {
                Package* p = static_cast<Package*>(selected.at(i));
                PackageVersion* pv = r->findNewestInstallablePackageVersion_(
                        p->name, &err);
                if (!err.isEmpty())
                    break;

                if (pv)
                    pvs.append(pv);
            }
        } else {
            for (int i = 0; i < selected.count(); i++) {
                pvs.append(static_cast<PackageVersion*>(selected.at(i))->clone());
            }
        }
    }

    if (err.isEmpty()) {
        QDialog* d = new QDialog(this);
        d->setWindowTitle(QObject::tr("Export"));
        QVBoxLayout* layout = new QVBoxLayout();
        d->setLayout(layout);

        ExportRepositoryFrame* list = new ExportRepositoryFrame(d);

        d->layout()->addWidget(list);

        QDialogButtonBox* dbb = new QDialogButtonBox(QDialogButtonBox::Ok |
                QDialogButtonBox::Cancel);
        d->layout()->addWidget(dbb);
        connect(dbb, SIGNAL(accepted()), d, SLOT(accept()));
        connect(dbb, SIGNAL(rejected()), d, SLOT(reject()));

        d->resize(600, 300);

        if (d->exec()) {
            err = list->getError();
            if (err.isEmpty()) {
                Job* job = new Job(QObject::tr("Export packages"));

                monitor(job);

                DBRepository* rep = DBRepository::getDefault();
                QtConcurrent::run(reinterpret_cast<AbstractRepository*>(rep),
                        &AbstractRepository::exportPackagesCoInitializeAndFree,
                        job, pvs, list->getDirectory(),
                        list->getExportDefinitions());

                pvs.clear();
            } else {
                addErrorMessage(err, err, true, QMessageBox::Critical);
            }
        }
        d->deleteLater();
    } else
        addErrorMessage(err, err, true, QMessageBox::Critical);

    qDeleteAll(pvs);
}

void MainWindow::on_actionCheck_dependencies_triggered()
{
    /*
    AsyncDownloader ad;

    Job* job = new Job("TODO");
    Downloader::Request request(QUrl("https://www.microsoft.com"));
    QFile file("abc.htm");
    file.open(QFile::WriteOnly | QFile::Truncate);
    request.file = &file;
    Downloader::Response response;
    AsyncDownloader::downloadWin2(job, request, &response);
    delete job;
    */

    QString err;

    QString msg;

    InstalledPackages* ip = InstalledPackages::getDefault();
    DBRepository* dbr = DBRepository::getDefault();

    QList<InstalledPackageVersion*> all = ip->getAll();
    int n = 0;
    for (int i = 0; i < all.size(); i++) {
        InstalledPackageVersion* ipv = all.at(i);
        if (ipv->installed()) {
            std::unique_ptr<PackageVersion> pv(dbr->findPackageVersion_(
                    ipv->package, ipv->version, &err));
            if (!err.isEmpty())
                break;

            for (int j = 0; j < pv->dependencies.count(); j++) {
                Dependency* d = pv->dependencies.at(j);
                if (!ip->isInstalled(*d)) {
                    msg += "\r\n" + QString(
                            "%1 depends on %2, which is not installed").
                            arg(pv->toString(true)).
                            arg(dbr->toString(*d, true));
                    n++;
                }
            }
        }
    }
    qDeleteAll(all);

    if (n > 0) {
        this->addErrorMessage(msg, msg, true, QMessageBox::Critical);
    } else {
        QString msg = QObject::tr("All dependencies are installed");
        this->addErrorMessage(msg, msg, true, QMessageBox::Information);
    }
}
