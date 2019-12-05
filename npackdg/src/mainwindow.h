#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <windows.h>
#include <shobjidl.h>
#include <stdint.h>

#include <QMainWindow>
#include <QProgressDialog>
#include <QTimer>
#include <QMap>
#include <QModelIndex>
#include <QFrame>
#include <QScrollArea>
#include <QMessageBox>
#include <QStringList>
#include <QString>
#include <QCache>
#include <QList>

#include "packageversion.h"
#include "package.h"
#include "job.h"
#include "fileloader.h"
#include "selection.h"
#include "mainframe.h"
#include "progresstree2.h"
#include "downloadsizefinder.h"

namespace Ui {
    class MainWindow;
}

const UINT WM_ICONTRAY = WM_USER + 1;

/**
 * @brief search result
 */
class _SearchResult {
public:
    QStringList found;
    QList<QStringList> cats, cats1;
};

/**
 * Main window.
 */
class MainWindow : public QMainWindow, public Selection {
    Q_OBJECT
private:
    static MainWindow* instance;

    QIcon brokenIcon;

    ProgressTree2* pt;

    Ui::MainWindow *ui;

    QWidget* jobsTab;
    MainFrame* mainFrame;

    UINT taskbarMessageId;
    ITaskbarList3* taskbarInterface;

    QMap<QString, QIcon> screenshots;

    /**
     * @brief URL -> download size
     */
    QMap<QString, int64_t> downloadSizes;

    int findPackageTab(const QString& package) const;
    int findPackageVersionTab(const QString& package,
            const Version& version) const;
    int findLicenseTab(const QString& name) const;

    void addJobsTab();
    void showDetails();
    void updateIcon(const QString &url);
    bool isUpdateEnabled(const QString& package);
    void setMenuAccelerators();
    void setActionAccelerators(QWidget* w);

    void updateInstallAction();
    void updateUninstallAction();
    void updateUpdateAction();
    void updateTestDownloadSiteAction();
    void updateGotoPackageURLAction();
    void updateActionShowDetailsAction();
    void updateCloseTabAction();
    void updateReloadRepositoriesAction();
    void updateScanHardDrivesAction();
    void updateShowFolderAction();
    void updateShowChangelogAction();
    void updateRunAction();
    void updateExportAction();

    /**
     * @param ps selected packages
     */
    void selectPackages(QList<Package*> ps);

    void updateStatusInDetailTabs();
    void updateProgressTabTitle();
    void saveUISettings();
    void loadUISettings();

    virtual void closeEvent(QCloseEvent *event);
    void reloadTabs();

    /** URL -> icon */
    QCache<QString, QIcon> icons;

    void updateDownloadSize(const QString &url);
    _SearchResult search(Package::Status minStatus, Package::Status maxStatus,
                         const QString &query, int cat0, int cat1, QString *err);
public:
    /** URL -> full path to the file or "" in case of an error */
    QMap<QString, QString> downloadCache;

    /**
     * Adds an entry in the "Progress" tab and monitors a task. The thread
     * itself should be started after the call to this method.
     *
     * @param job this job will be monitored. The object will be destroyed after
     *     the thread completion
     */
    void monitor(Job* job);

    void updateActions();

    /**
     * @param package full package name
     * @return icon for the specified package
     */
    static QIcon getPackageIcon(const QString& package);

    /**
     * @return the only instance of this class
     */
    static MainWindow* getInstance();

    /**
     * This icon is used if a package does not define an icon.
     */
    static QIcon genericAppIcon;

    /**
     * This icon is used if a package icon is being downloaded
     */
    static QIcon waitAppIcon;

    /** true if the repositories are being reloaded */
    bool reloadRepositoriesThreadRunning;

    /** file loader thread */
    FileLoader fileLoader;

    /** finds download sizes */
    DownloadSizeFinder downloadSizeFinder;

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /**
     * @brief This functions returns the image downloaded from the specified
     *     URL. The code inserts the provided URL in the work queue if it was
     *     not yet downloaded. The background thread will download the image.
     * @param url URL of the image
     * @return the image if it was already downloaded or a "please wait" image
     *     if the image is not yet available.
     */
    QIcon downloadIcon(const QString& url);

    /**
     * Fills the table with known package versions.
     */
    void fillList();

    /**
     * Adds an error message panel.
     *
     * @param msg short error message
     * @param details error details
     * @param autoHide true = automatically hide the message after some time
     * @param icon message icon. This determines the background color of the
     *     message
     */
    void addErrorMessage(const QString& msg, const QString& details="",
            bool autoHide=true, QMessageBox::Icon icon=QMessageBox::Critical);

    /**
     * Adds a new tab with the specified text
     *
     * @param title tab title
     * @param text the text
     * @param html true = HTML, false = plain text
     */
    void addTextTab(const QString& title, const QString& text, bool html=false);

    bool nativeEvent(const QByteArray & eventType, void * message, long * result);

    /**
     * Prepares the UI after the constructor was called.
     */
    void prepare();

    /**
     * Close all detail tabs (versions and licenses).
     */
    void closeDetailTabs();

    /**
     * Reloads the content of all repositories and recognizes all installed
     * packages again.
     *
     * @param useCache true = use HTTP cache
     */
    void recognizeAndLoadRepositories(bool useCache);

    /**
     * Adds a new tab. The new tab will be automatically selected.
     *
     * @param w content of the new tab
     * @param icon tab icon
     * @param title tab title
     */
    void addTab(QWidget* w, const QIcon& icon, const QString& title);

    /**
     * @brief opens a new tab for the specified package version. A new tab will
     *     not be created if there is already a tab for the package version. The
     *     package version should exist.
     * @param package full package name
     * @param version version
     * @param select true = select the newly created tab
     */
    void openPackageVersion(const QString& package,
            const Version& version, bool select);

    /**
     * @brief opens a new tab for the specified package. A new tab will
     *     not be created if there is already a tab for the package. The
     *     package should exist.
     * @param package full package name
     * @param select true = select the newly created tab
     */
    void openPackage(const QString& package, bool select);

    /**
     * @brief opens a new tab for the specified license. A new tab will
     *     not be created if there is already a tab for the license. The
     *     license should exist.
     * @param name full internal name
     * @param select true = select the newly created tab
     */
    void openLicense(const QString& name, bool select);

    QList<void*> getSelected(const QString& type) const;

    void openURL(const QUrl &url);
    static QString createPackageVersionsHTML(const QStringList &names);

    /**
     * @brief This functions returns the image downloaded from the specified
     *     URL. The code inserts the provided URL in the work queue if it was
     *     not yet downloaded. The background thread will download the image.
     * @param url URL of the image
     * @return the image if it was already downloaded or a "please wait" image
     *     if the image is not yet available. The image will have the maximum
     *     size of 200x200
     */
    QIcon downloadScreenshot(const QString &url);

    /**
     * @brief start filling the list asnchronously
     */
    void fillListInBackground();
protected:
    void changeEvent(QEvent *e);

    /**
     * @param install the objects will be destroyed
     * @param programCloseType how the programs should be closed
     */
    void process(QList<InstallOperation*>& install, DWORD programCloseType);
public slots:
    void on_errorMessage(const QString& msg, const QString& details="",
            bool autoHide=true, QMessageBox::Icon icon=QMessageBox::Critical);
private slots:
    void processThreadFinished();
    void recognizeAndLoadRepositoriesThreadFinished();
    void on_actionShow_Details_triggered();
    void on_tabWidget_currentChanged(int index);
    void on_tabWidget_tabCloseRequested(int index);
    void on_actionAbout_triggered();
    void on_actionTest_Download_Site_triggered();
    void on_actionUpdate_triggered();
    void on_actionSettings_triggered();
    void on_actionGotoPackageURL_triggered();
    void onShow();
    void on_actionExit_triggered();
    void on_actionReload_Repositories_triggered();
    void on_actionClose_Tab_triggered();
    void repositoryStatusChanged(const QString &, const Version &);
    void monitoredJobChanged(Job *state);
    void on_actionFile_an_Issue_triggered();
    void updateActionsSlot();
    void applicationFocusChanged(QWidget* old, QWidget* now);
    void on_actionInstall_triggered();
    void on_actionUninstall_triggered();
    void on_actionAdd_package_triggered();
    void on_actionOpen_folder_triggered();
    void visibleJobsChanged();
    void on_actionChoose_columns_triggered();
    void downloadCompleted(const QString &url,
            const QString &filename, const QString &error);
    void downloadSizeCompleted(const QString &url,
            int64_t size);
    void on_actionShow_changelog_triggered();
    void on_actionToggle_toolbar_triggered(bool checked);
    void on_mainToolBar_visibilityChanged(bool visible);
    void monitoredJobCompleted();
    void on_actionRun_triggered();
    void on_actionExport_triggered();
    void on_actionCheck_dependencies_triggered();

};

#endif // MAINWINDOW_H
