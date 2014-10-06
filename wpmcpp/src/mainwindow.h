#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <windows.h>
#include <shobjidl.h>

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

#include "packageversion.h"
#include "package.h"
#include "job.h"
#include "fileloader.h"
#include "selection.h"
#include "mainframe.h"
#include "progresstree2.h"

namespace Ui {
    class MainWindow;
}

const UINT WM_ICONTRAY = WM_USER + 1;

/**
 * Main window.
 */
class MainWindow : public QMainWindow, public Selection {
    Q_OBJECT
private:
    static MainWindow* instance;

    ProgressTree2* pt;

    Ui::MainWindow *ui;

    QWidget* jobsTab;
    MainFrame* mainFrame;

    UINT taskbarMessageId;
    ITaskbarList3* taskbarInterface;

    QMap<QString, QIcon> screenshots;

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
    void chooseAccelerators(QStringList* titles);

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

    /**
     * @param ps selected packages
     */
    void selectPackages(QList<Package*> ps);

    /**
     * Adds an entry in the "Progress" tab and monitors a task. The thread
     * itself should be started after the call to this method.
     *
     * @param job this job will be monitored. The object will be destroyed after
     *     the thread completion
     */
    void monitor(Job* job);

    void updateStatusInDetailTabs();
    void updateProgressTabTitle();
    void saveUISettings();
    void loadUISettings();

    virtual void closeEvent(QCloseEvent *event);
    void processWithSelfUpdate(Job *job, QList<InstallOperation *> &install,
                               int programCloseType);
    void reloadTabs();

    /**
     * @brief tests whether an install/uninstall operation can be performed
     *     right
     * @return error message or ""
     */
    QString mayPerformInstallOperation();

    /** URL -> icon */
    QCache<QString, QIcon> icons;
public:
    /** URL -> full path to the file or "" in case of an error */
    QMap<QString, QString> downloadCache;

    void updateActions();

    /**
     * @param package full package name
     * @return icon for the specified package
     */
    static QIcon getPackageVersionIcon(const QString& package);

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

    /** true if the hard drive scan is runnig */
    bool hardDriveScanRunning;

    /** true if the repositories are being reloaded */
    bool reloadRepositoriesThreadRunning;

    /** file loader thread */
    FileLoader fileLoader;

    MainWindow(QWidget *parent = 0);
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
     * @param icon message icon
     */
    void addErrorMessage(const QString& msg, const QString& details="",
            bool autoHide=true, QMessageBox::Icon icon=QMessageBox::NoIcon);

    /**
     * Adds a new tab with the specified text
     *
     * @param title tab title
     * @param text the text
     * @param html true = HTML, false = plain text
     */
    void addTextTab(const QString& title, const QString& text, bool html=false);

    virtual bool nativeEvent(const QByteArray & eventType, void * message, long * result);

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
protected:
    void changeEvent(QEvent *e);

    /**
     * @param install the objects will be destroyed
     * @param programCloseType how the programs should be closed
     */
    void process(QList<InstallOperation*>& install, int programCloseType);
private slots:
    void processThreadFinished();
    void hardDriveScanThreadFinished();
    void recognizeAndLoadRepositoriesThreadFinished();
    void on_actionScan_Hard_Drives_triggered();
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
    void iconDownloaded(const FileLoaderItem& it);
    void on_actionReload_Repositories_triggered();
    void on_actionClose_Tab_triggered();
    void repositoryStatusChanged(const QString &, const Version &);
    void monitoredJobChanged(const JobState& state);
    void on_actionFile_an_Issue_triggered();
    void updateActionsSlot();
    void applicationFocusChanged(QWidget* old, QWidget* now);
    void on_actionInstall_triggered();
    void on_actionUninstall_triggered();
    void on_actionAdd_package_triggered();
    void on_actionOpen_folder_triggered();
    void visibleJobsChanged();
};

#endif // MAINWINDOW_H
