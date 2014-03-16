#include <math.h>

#include <qabstractitemview.h>
#include <qmessagebox.h>
#include <qvariant.h>
#include <qprogressdialog.h>
#include <qwaitcondition.h>
#include <qthread.h>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>

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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "repository.h"
#include "job.h"
#include "wpmutils.h"
#include "installoperation.h"
#include "downloader.h"
#include "packageversionform.h"
#include "uiutils.h"
#include "progressframe.h"
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

extern HWND defaultPasswordWindow;

QMap<QString, QIcon> MainWindow::icons;
QIcon MainWindow::genericAppIcon;
QIcon MainWindow::waitAppIcon;
MainWindow* MainWindow::instance = 0;

class InstallThread: public QThread
{
    PackageVersion* pv;

    // 0, 1, 2 = install/uninstall
    // 3, 4 = recognize installed applications + load repositories
    int type;

    Job* job;
public:
    // name of the log file for type=6
    // directory for type=7
    QString logFile;

    QList<InstallOperation*> install;

    /**
     * type = 3 or 4
     * true (default value) = the HTTP cache will be used
     */
    bool useCache;

    InstallThread(PackageVersion* pv, int type, Job* job);

    void run();
};

InstallThread::InstallThread(PackageVersion *pv, int type, Job* job)
{
    this->pv = pv;
    this->type = type;
    this->job = job;
    this->useCache = false;
}

void InstallThread::run()
{
    CoInitialize(NULL);

    // qDebug() << "InstallThread::run.1";
    switch (this->type) {
        case 0:
        case 1:
        case 2:
            AbstractRepository::getDefault_()->process(job, install,
                    WPMUtils::getCloseProcessType());
            break;
        case 3:
        case 4: {
            DBRepository* dbr = DBRepository::getDefault();
            dbr->updateF5(job);
            break;
        }
    }

    CoUninitialize();

    // qDebug() << "InstallThread::run.2";
}

/**
 * Scan hard drives.
 */
class ScanHardDrivesThread: public QThread
{
    Job* job;
public:
    QMap<QString, int> words;

    QList<PackageVersion*> detected;

    ScanHardDrivesThread(Job* job);

    void run();
};

ScanHardDrivesThread::ScanHardDrivesThread(Job *job)
{
    this->job = job;
}

void ScanHardDrivesThread::run()
{
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

    QString err;
    AbstractRepository* r = AbstractRepository::getDefault_();
    QList<PackageVersion*> s1 = r->getInstalled_(&err);
    if (!err.isEmpty())
        job->setErrorMessage(err);


    ScanDiskThirdPartyPM* sd = new ScanDiskThirdPartyPM();
    err = InstalledPackages::getDefault()->detect3rdParty(sd, false);
    delete sd;
    if (!err.isEmpty())
        job->setErrorMessage(err);

    QList<PackageVersion*> s2 = r->getInstalled_(&err);
    if (!err.isEmpty())
        job->setErrorMessage(err);

    for (int i = 0; i < s2.count(); i++) {
        PackageVersion* pv = s2.at(i);
        if (!PackageVersion::contains(s1, pv)) {
            detected.append(pv);
        }
    }

    qDeleteAll(s1);
    qDeleteAll(s2);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    instance = this;

    ui->setupUi(this);

    commandLineProcessed = false;

    this->setMenuAccelerators();
    this->setActionAccelerators(this);

    this->taskbarMessageId = 0;

    this->monitoredJobLastChanged = 0;
    this->progressContent = 0;
    this->jobsTab = 0;
    this->taskbarInterface = 0;

    this->hardDriveScanRunning = false;
    this->reloadRepositoriesThreadRunning = false;

    setWindowTitle("Npackd");

    this->genericAppIcon = QIcon(":/images/app.png");
    this->waitAppIcon = QIcon(":/images/wait.png");

    this->mainFrame = new MainFrame(this);

    updateActions();

    QTableView* t = this->mainFrame->getTableWidget();
    t->addAction(this->ui->actionInstall);
    t->addAction(this->ui->actionUninstall);
    t->addAction(this->ui->actionUpdate);
    t->addAction(this->ui->actionShow_Details);
    t->addAction(this->ui->actionGotoPackageURL);
    t->addAction(this->ui->actionTest_Download_Site);
    t->addAction(this->ui->actionOpen_folder);

    connect(&this->fileLoader, SIGNAL(downloaded(const FileLoaderItem&)), this,
            SLOT(iconDownloaded(const FileLoaderItem&)),
            Qt::QueuedConnection);
    this->fileLoader.start(QThread::LowestPriority);

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
    if (lpfChangeWindowMessageFilterEx)
        lpfChangeWindowMessageFilterEx((HWND) winId(), taskbarMessageId, 1, 0);
    FreeLibrary(hInstLib);
}

void MainWindow::applicationFocusChanged(QWidget* old, QWidget* now)
{
    updateActions();
}

bool MainWindow::winEvent(MSG* message, long* result)
{
    if (message->message == taskbarMessageId) {
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
            pf->updateIcons();
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
    AbstractRepository* r = AbstractRepository::getDefault_();
    Package* p = r->findPackage_(package);

    QIcon icon = MainWindow::genericAppIcon;
    if (p) {
        if (!p->icon.isEmpty()) {
            if (MainWindow::icons.contains(p->icon))
                icon = MainWindow::icons[p->icon];
            else
                icon = MainWindow::waitAppIcon;
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
    int n = this->runningJobs.count();

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

void MainWindow::iconDownloaded(const FileLoaderItem& it)
{
    if (it.f) {
        QPixmap pm(it.f->fileName());

        /* gray
        QStyleOption opt(0);
        opt.palette = QApplication::palette();
        pm = QApplication::style()->generatedIconPixmap(QIcon::Disabled, pm, &opt);
        */

        delete it.f;
        if (!pm.isNull()) {
            QIcon icon(pm);
            icon.detach();
            this->icons.insert(it.url, icon);
            updateIcon(it.url);
        }
    }
}

void MainWindow::prepare()
{
    QTimer *pTimer = new QTimer(this);
    pTimer->setSingleShot(true);
    connect(pTimer, SIGNAL(timeout()), this, SLOT(onShow()));

    pTimer->start(0);

    cl.add("package", 'p',
            "internal package name (e.g. com.example.Editor or just Editor)",
            "package", true);
    cl.add("versions", 'r', "versions range (e.g. [1.5,2))",
            "range", false);
    cl.add("version", 'v', "version number (e.g. 1.5.12)",
            "version", false);
    cl.add("url", 'u', "repository URL (e.g. https://www.example.com/Rep.xml)",
            "repository", false);
    cl.add("status", 's', "filters package versions by status",
            "status", false);
    cl.add("bare-format", 'b', "bare format (no heading or summary)",
            "", false);
    cl.add("query", 'q', "search terms (e.g. editor)",
            "search terms", false);
    cl.add("debug", 'd', "turn on the debug output", "", false);
    cl.add("file", 'f', "file or directory", "file", false);
    cl.add("end-process", 'e',
        "comma separated list of ways to close running applications (windows, kill)",
        "list", false);

    commandLineParsingError = cl.parse();

    // cl.dump();

    /* TODO
    if (cl.isPresent("debug")) {
        clp.setUpdateRate(0);
    }
    */

    QStringList fr = cl.getFreeArguments();

    if (fr.count() == 1) {
        commandLineCommand = fr.at(0);
    } else if (fr.count() > 1) {
        commandLineParsingError = QObject::tr("Unexpected argument: %1").
                arg(fr.at(1));
    }
}

void MainWindow::updateProgressTabTitle()
{
    int n = this->runningJobStates.count();
    time_t max = 0;
    double maxProgress = 0;
    for (int i = 0; i < n; i++) {
        JobState state = this->runningJobStates.at(i);

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
    this->fileLoader.terminated = 1;
    if (!this->fileLoader.wait(1000))
        this->fileLoader.terminate();
    delete ui;
}

void MainWindow::monitoredJobChanged(const JobState& state)
{
    time_t now;
    time(&now);

    if (now != this->monitoredJobLastChanged) {
        this->monitoredJobLastChanged = now;

        int index = this->runningJobs.indexOf(state.job);
        if (index >= 0) {
            this->runningJobStates.replace(index, state);
        }

        updateProgressTabTitle();
    }
}

void MainWindow::monitor(Job* job, const QString& title, QThread* thread)
{
    connect(job, SIGNAL(changed(const JobState&)), this,
            SLOT(monitoredJobChanged(const JobState&)),
            Qt::QueuedConnection);

    this->runningJobs.append(job);
    this->runningJobStates.append(JobState());

    updateProgressTabTitle();

    ProgressFrame* pf = new ProgressFrame(progressContent, job, title,
            thread);
    pf->resize(100, 100);
    QVBoxLayout* layout = (QVBoxLayout*) progressContent->layout();
    layout->insertWidget(0, pf);

    progressContent->resize(500, 500);
}

void MainWindow::onShow()
{
    DBRepository* dbr = DBRepository::getDefault();
    QString err = dbr->open();
    if (err.isEmpty())
        recognizeAndLoadRepositories(true);
    else
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

void MainWindow::fillList()
{
    // Columns and data types:
    // 0: icon QString
    // 1: package name Package*
    // 2: package description Package*
    // 3: available version Package*
    // 4: installed versions Package*
    // 5: license Package*

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

    DBRepository* dbr = DBRepository::getDefault();
    QString err;
    QList<Package*> found = dbr->findPackages(status, statusInclude, query,
            cat0, cat1, &err);

    QList<QStringList> cats;
    if (err.isEmpty()) {
        cats = dbr->findCategories(status, statusInclude, query, 0, -1, -1,
                &err);
    }
    if (err.isEmpty())
        this->mainFrame->setCategories(0, cats);

    this->mainFrame->setCategoryFilter(0, cat0);

    if (err.isEmpty()) {
        QList<QStringList> cats1;
        if (cat0 >= 0) {
            cats1 = dbr->findCategories(status, statusInclude, query, 1,
                    cat0, -1, &err);
        }
        this->mainFrame->setCategories(1, cats1);
    }

    this->mainFrame->setCategoryFilter(1, cat1);

    if (!err.isEmpty())
        addErrorMessage(err, err, true, QMessageBox::Critical);

    PackageItemModel* m = (PackageItemModel*) t->model();
    m->setPackages(found);
    t->setUpdatesEnabled(true);
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

void MainWindow::process(QList<InstallOperation*> &install)
{
    QString err;

    QList<PackageVersion*> pvs;

    // fetch package versions
    for (int j = 0; j < install.size(); j++) {
        InstallOperation* op = install.at(j);
        PackageVersion* pv = op->findPackageVersion(&err);
        if (!err.isEmpty()) {
            err = QObject::tr("Cannot find the package version %1: %2").arg(
                    pv->toString()).arg(err);
            break;
        }
        pvs.append(pv);
    }

    // check for locked package versions
    if (err.isEmpty()) {
        for (int j = 0; j < pvs.size(); j++) {
            PackageVersion* pv = pvs.at(j);
            if (pv->isLocked()) {
                err = QObject::tr("The package %1 is locked by a currently running installation/removal.").
                        arg(pv->toString());
                break;
            }
        }
    }

    // list of used package versions
    QList<bool> used;
    used.reserve(install.count());
    for (int i = 0; i < install.count(); i++) {
        used.append(false);
    }

    // find updates
    QStringList updateNames;
    if (err.isEmpty()) {
        for (int i = 0; i < install.count(); i++) {
            if (used[i])
                continue;

            InstallOperation* op = install.at(i);
            if (!op->install) {
                for (int j = 0; j < install.count(); j++) {
                    if (used[j] || j == i)
                        continue;

                    InstallOperation* op2 = install.at(j);
                    if (op2->install && op->package == op2->package &&
                            op->version < op2->version) {
                        used[i] = used[j] = true;

                        PackageVersion* pv = pvs.at(i);
                        updateNames.append(pv->toString() + " -> " +
                                op2->version.getVersionString());
                    }
                }
            }
        }
    }

    // find uninstalls
    QStringList uninstallNames;
    if (err.isEmpty()) {
        for (int i = 0; i < install.count(); i++) {
            if (used[i])
                continue;

            InstallOperation* op = install.at(i);
            PackageVersion* pv = pvs.at(i);
            if (!op->install) {
                uninstallNames.append(pv->toString());
            }
        }
    }

    // find installs
    QStringList installNames;
    if (err.isEmpty()) {
        for (int i = 0; i < install.count(); i++) {
            if (used[i])
                continue;

            InstallOperation* op = install.at(i);
            PackageVersion* pv = pvs.at(i);
            if (op->install) {
                installNames.append(pv->toString());
            }
        }
    }

    bool b = false;
    QString msg;
    QString title;
    QString dialogTitle;
    QString detailedMessage;
    if (installNames.count() == 1 && uninstallNames.count() == 0 &&
            updateNames.count() == 0) {
        b = true;
        title = QObject::tr("Installing");
    } else if (installNames.count() == 0 && uninstallNames.count() == 1 &&
            updateNames.count() == 0) {
        title = QObject::tr("Uninstalling");

        PackageVersion* pv = pvs.at(0);

        dialogTitle = QObject::tr("Uninstall");
        msg = QString("<html><head/><body><h2>") +
                QObject::tr("The package %1 will be uninstalled.").
                arg(pv->toString()).toHtmlEscaped() +
                "</h2>" +
                QObject::tr("The corresponding directory %1 will be completely deleted. There is no way to restore the files. The processes locking the files will be closed.").
                arg(pv->getPath()).toHtmlEscaped() +
                "</body>";
        detailedMessage = QObject::tr("The package %1 will be uninstalled.").
                arg(pv->toString());
    } else if (installNames.count() > 0 && uninstallNames.count() == 0 &&
            updateNames.count() == 0) {
        title = QString(QObject::tr("Installing %1 packages")).arg(
                installNames.count());
        dialogTitle = QObject::tr("Install");
        msg = QString("<html><head/><body><h2>") +
                QObject::tr("%1 package(s) will be installed:").
                arg(installNames.count()).toHtmlEscaped() +
                "</h2><br><b>" +
                createPackageVersionsHTML(installNames) +
                "</b></body>";
        detailedMessage = QObject::tr("%1 package(s) will be installed:").
                arg(installNames.count()) + "\n" +
                installNames.join("\n");
    } else if (installNames.count() == 0 && uninstallNames.count() > 0 &&
            updateNames.count() == 0) {
        title = QString(QObject::tr("Uninstalling %1 packages")).arg(
                uninstallNames.count());
        dialogTitle = QObject::tr("Uninstall");
        msg = QString("<html><head/><body><h2>") +
                QObject::tr("%1 package(s) will be uninstalled:").
                arg(uninstallNames.count()).toHtmlEscaped() +
                "</h2><br><b>" +
                createPackageVersionsHTML(uninstallNames) +
                "</b>.<br><br>" +
                QObject::tr("The corresponding directories will be completely deleted. There is no way to restore the files. The processes locking the files will be closed.").
                toHtmlEscaped() +
                "</body>";
        detailedMessage = QObject::tr("%1 package(s) will be uninstalled:").
                arg(uninstallNames.count()) + "\n" +
                uninstallNames.join("\n");
    } else {
        title = QObject::tr("Installing %1 packages, uninstalling %2 packages, updating %3 packages").
                arg(installNames.count()).
                arg(uninstallNames.count()).
                arg(updateNames.count());
        dialogTitle = QObject::tr("Install/Uninstall");
        msg = "<html><head/><body>";
        if (updateNames.count() > 0) {
            msg += "<h2>" +
                    QObject::tr("%3 package(s) will be updated:").
                    arg(updateNames.count()).toHtmlEscaped() +
                    "</h2><br><b>" +
                    createPackageVersionsHTML(updateNames) +
                    "</b>";
            if (!detailedMessage.isEmpty())
                detailedMessage += "\n\n";
            detailedMessage += QObject::tr("%3 package(s) will be updated:").
                    arg(updateNames.count()) + "\n" +
                    updateNames.join("\n");
        }
        if (uninstallNames.count() > 0) {
            msg += "<h2>" +
                    QObject::tr("%1 package(s) will be uninstalled:").
                    arg(uninstallNames.count()).toHtmlEscaped() +
                    "</h2><br><b>" +
                    createPackageVersionsHTML(uninstallNames) +
                    "</b>";
            if (!detailedMessage.isEmpty())
                detailedMessage += "\n\n";
            detailedMessage += QObject::tr("%1 package(s) will be uninstalled:").
                    arg(uninstallNames.count()) + "\n" +
                    uninstallNames.join("\n");
        }
        if (installNames.count() > 0) {
            msg += "<h2>" +
                    QObject::tr("%3 package(s) will be installed:").
                    arg(installNames.count()).toHtmlEscaped() +
                    "</h2><br><b>" +
                    createPackageVersionsHTML(installNames) +
                    "</b>";
            if (!detailedMessage.isEmpty())
                detailedMessage += "\n\n";
            detailedMessage += QObject::tr("%3 package(s) will be installed:").
                    arg(installNames.count()) + "\n" +
                    installNames.join("\n");
        }
        msg += "<br><br>" +
                QObject::tr("The corresponding directories will be completely deleted. There is no way to restore the files. The processes locking the files will be closed.").
                toHtmlEscaped();
        msg += "</body>";
    }

    if (err.isEmpty()) {
        if (!b)
            b = UIUtils::confirm(this, dialogTitle, msg, detailedMessage);
    }

    if (err.isEmpty()) {
        if (b) {
            Job* job = new Job();
            InstallThread* it = new InstallThread(0, 1, job);
            it->install = install;
            install.clear();

            connect(it, SIGNAL(finished()), this,
                    SLOT(processThreadFinished()),
                    Qt::QueuedConnection);

            monitor(job, title, it);
        }
    }

    if (!err.isEmpty()) {
        addErrorMessage(err, err, true, QMessageBox::Critical);
    }

    qDeleteAll(install);
    install.clear();

    qDeleteAll(pvs);
    pvs.clear();
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
    int n = this->runningJobs.count();

    if (n > 0)
        addErrorMessage(QObject::tr("Cannot exit while jobs are running"));
    else
        this->close();
}

void MainWindow::unregisterJob(Job *job)
{
    int index = this->runningJobs.indexOf(job);
    if (index >= 0) {
        this->runningJobs.removeAt(index);
        this->runningJobStates.removeAt(index);
    }

    this->updateProgressTabTitle();
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
            enabled = selected.count() > 0 &&
                    !hardDriveScanRunning && !reloadRepositoriesThreadRunning;
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
            enabled = selected.count() > 0 &&
                    !hardDriveScanRunning && !reloadRepositoriesThreadRunning;
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
            enabled = selected.count() > 0 &&
                    !hardDriveScanRunning && !reloadRepositoriesThreadRunning;
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
            enabled = selected.count() > 0 &&
                    !hardDriveScanRunning && !reloadRepositoriesThreadRunning;
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
            enabled = selected.count() > 0 &&
                    !hardDriveScanRunning && !reloadRepositoriesThreadRunning;
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
            enabled = selected.count() > 0 &&
                    !hardDriveScanRunning && !reloadRepositoriesThreadRunning;
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
            enabled =
                    !hardDriveScanRunning && !reloadRepositoriesThreadRunning;
            for (int i = 0; i < selected.count(); i++) {
                if (!enabled)
                    break;

                Package* p = (Package*) selected.at(i);

                enabled = enabled && isUpdateEnabled(p->name);
            }
        } else {
            selected = selection->getSelected("PackageVersion");
            enabled = selected.count() >= 1 &&
                    !hardDriveScanRunning && !reloadRepositoriesThreadRunning;
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
        if (selected.count() > 0)
            enabled = !reloadRepositoriesThreadRunning;
        else {
            selected = selection->getSelected("Package");
            enabled = !reloadRepositoriesThreadRunning && selected.count() > 0;
        }
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
            if (!reloadRepositoriesThreadRunning) {
                for (int i = 0; i < selected.count(); i++) {
                    PackageVersion* pv = (PackageVersion*) selected.at(i);
                    if (pv->download.isValid()) {
                        enabled = true;
                        break;
                    }
                }
            }
        } else {
            AbstractRepository* r = AbstractRepository::getDefault_();
            selected = selection->getSelected("Package");
            if (!reloadRepositoriesThreadRunning) {
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
    }

    this->ui->actionTest_Download_Site->setEnabled(enabled);
}

void MainWindow::updateGotoPackageURLAction()
{
    Selection* selection = Selection::findCurrent();
    bool enabled = false;
    if (selection) {
        QList<void*> selected = selection->getSelected("PackageVersion");
        if (selected.count() > 0) {
            AbstractRepository* r = Repository::getDefault_();
            if (!reloadRepositoriesThreadRunning) {
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
            }
        } else {
            selected = selection->getSelected("Package");
            if (!reloadRepositoriesThreadRunning) {
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
    QTableView* t = this->mainFrame->getTableWidget();
    PackageItemModel* m = (PackageItemModel*) t->model();
    m->setPackages(QList<Package*>());
    m->clearCache();

    Job* job = new Job();
    InstallThread* it = new InstallThread(0, 3, job);
    it->useCache = useCache;

    connect(it, SIGNAL(finished()), this,
            SLOT(recognizeAndLoadRepositoriesThreadFinished()),
            Qt::QueuedConnection);

    this->reloadRepositoriesThreadRunning = true;
    updateActions();

    monitor(job, QObject::tr("Initializing"), it);
}

void MainWindow::setMenuAccelerators(){
    QMenuBar* mb = this->menuBar();
    QList<QChar> used;
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

void MainWindow::recognizeAndLoadRepositoriesThreadFinished()
{
    fillList();

    this->reloadRepositoriesThreadRunning = false;
    updateActions();


    if (!commandLineProcessed) {
        commandLineProcessed = true;

        if (commandLineParsingError.isEmpty()) {
            if (commandLineCommand == "remove" || commandLineCommand == "rm") {
                remove();
            }

            /* TODO
            const QString cmd = fr.at(0);
            QString err;
            if (cmd == "remove") {
                QMessageBox::critical(this, "Error", "Removing!");
            * TODO:
            if (cmd == "help") {
                usage();
            } else if (cmd == "path") {
                err = path();
            } else if (cmd == "remove" || cmd == "rm") {
                err = remove();
            } else if (cmd == "add") {
                err = add();
            } else if (cmd == "add-repo") {
                err = addRepo();
            } else if (cmd == "remove-repo") {
                err = removeRepo();
            } else if (cmd == "list-repos") {
                err = listRepos();
            } else if (cmd == "search") {
                err = search();
            } else if (cmd == "check") {
                err = check();
            } else if (cmd == "which") {
                err = which();
            } else if (cmd == "list") {
                err = list();
            } else if (cmd == "info") {
                err = info();
            } else if (cmd == "update") {
                err = update();
            } else if (cmd == "detect") {
                err = detect();
            } else {
                err = "Wrong command: " + cmd + ". Try npackdcl help";
            }

            if (!err.isEmpty())
                QMessageBox::critical(this, "Error", err);
                */
        } else {
            QString msg = QObject::tr("Error parsing the command line: %1").
                    arg(commandLineParsingError);
            addErrorMessage(msg, msg, true, QMessageBox::Critical);
        }
    }
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
    Selection* sel = Selection::findCurrent();
    QList<void*> selected;
    if (sel)
        selected = sel->getSelected("PackageVersion");

    AbstractRepository* r = AbstractRepository::getDefault_();
    QList<Package*> packages;
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

    if (packages.count() > 0) {
        QList<InstallOperation*> ops;
        QString err = r->planUpdates(packages, ops);

        if (err.isEmpty()) {
            process(ops);
        } else
            addErrorMessage(err, err, true, QMessageBox::Critical);
    }

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
            QObject::tr("<html><body>Npackd %1 - software package manager for Windows (R)<br><a href='http://code.google.com/p/windows-package-manager'>http://code.google.com/p/windows-package-manager</a></body></html>")).
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
    QScrollArea* jobsScrollArea = new QScrollArea(this->ui->tabWidget);
    jobsScrollArea->setFrameStyle(0);

    progressContent = new QFrame(jobsScrollArea);
    QVBoxLayout* layout = new QVBoxLayout();

    QSizePolicy sp;
    sp.setVerticalPolicy(QSizePolicy::Preferred);
    sp.setHorizontalPolicy(QSizePolicy::Ignored);
    sp.setHorizontalStretch(100);
    progressContent->setSizePolicy(sp);

    layout->addStretch(100);
    progressContent->setLayout(layout);
    jobsScrollArea->setWidget(progressContent);
    jobsScrollArea->setWidgetResizable(true);

    int index = this->ui->tabWidget->addTab(jobsScrollArea,
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

    Job* job = new Job();
    ScanHardDrivesThread* it = new ScanHardDrivesThread(job);

    connect(it, SIGNAL(finished()), this,
            SLOT(hardDriveScanThreadFinished()),
            Qt::QueuedConnection);

    this->hardDriveScanRunning = true;
    this->updateActions();

    monitor(job, QObject::tr("Install/Uninstall"), it);
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
        closeDetailTabs();
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
        process(ops);
    else
        addErrorMessage(err, err, true, QMessageBox::Critical);

    qDeleteAll(installed);
    qDeleteAll(pvs);
}

void MainWindow::on_actionUninstall_triggered()
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

    QString err;
    QList<InstallOperation*> ops;
    QList<PackageVersion*> installed = AbstractRepository::getDefault_()->
            getInstalled_(&err);

    if (err.isEmpty()) {
        for (int i = 0; i < pvs.count(); i++) {
            PackageVersion* pv = pvs.at(i);
            err = pv->planUninstallation(installed, ops);
            if (!err.isEmpty())
                break;
        }
    }

    if (err.isEmpty())
        process(ops);
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

QString MainWindow::remove()
{
    QString err;

    /* TODO:
    int programCloseType = WPMUtils::CLOSE_WINDOW;

    programCloseType = WPMUtils::getProgramCloseType(cl, &err);
    */

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

    if (err.isEmpty())
        process(ops);
    else
        addErrorMessage(err, err, true, QMessageBox::Critical);

    qDeleteAll(installed);
    qDeleteAll(toRemove);

    /* TODO
     *confirmation. This is not yet enabled in order to maintain the
     * compatibility.
    if (job->shouldProceed()) {
        QString title;
        QString err;
        if (!confirm(ops, &title, &err))
            job->cancel();
        if (!err.isEmpty())
            job->setErrorMessage(err);
    }
    */


/* TODO: command line removing
    if (job->shouldProceed("Removing")) {
        Job* removeJob = job->newSubJob(0.9);
        rep->process(removeJob, ops, programCloseType);
        if (!removeJob->getErrorMessage().isEmpty())
            job->setErrorMessage(QString("Error removing: %1\n").
                    arg(removeJob->getErrorMessage()));
        delete removeJob;
    }
*/

    return err;
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
