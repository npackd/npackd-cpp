#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <windows.h>
#include <shobjidl.h>

#include <QMainWindow>
#include <QProgressDialog>
#include <QTimer>
#include <QModelIndex>
#include <QFrame>
#include <QScrollArea>
#include <QMessageBox>
#include <QString>
#include <QCache>

#include "packageversion.h"
#include "package.h"
#include "job.h"
#include "fileloader.h"
#include "selection.h"
#include "mainframe.h"
#include "progresstree2.h"

#define IDM_ABOUT 16
#define IDM_EXIT 17
#define IDM_SETTINGS 18
#define IDM_FEEDBACK 19
#define IDM_CLOSE_TAB 20
#define IDM_CHOOSE_COLUMNS 21
#define IDM_TOGGLE_TOOLBAR 22
#define IDM_INSTALL 23
#define IDM_UNINSTALL 24
#define IDM_UPDATE 25
#define IDM_SHOW_DETAILS 26
#define IDM_SHOW_CHANGELOG 27
#define IDM_RUN 28
#define IDM_OPEN_FOLDER 29
#define IDM_OPEN_WEB_SITE 30
#define IDM_TEST_DOWNLOAD_SITE 31
#define IDM_CHECK_DEPENDENCIES 32
#define IDM_RELOAD_REPOSITORIES 33
#define IDM_ADD_PACKAGE 34
#define IDM_EXPORT 35


const UINT WM_ICONTRAY = WM_USER + 1;

/**
 * @brief search result
 */
class _SearchResult {
public:
    std::vector<QString> found;
    std::vector<std::vector<QString>> cats, cats1;
};

/**
 * @brief runs the GUI
 *
 * @param nCmdShow SW_*
 * @return error code
 */
int runGUI(int nCmdShow);

/**
 * Main window.
 */
class MainWindow : public QObject, public Selection {
    Q_OBJECT
private:
    static MainWindow* instance;

    QIcon brokenIcon;

    ProgressTree2* pt;

    QWidget* jobsTab;
    MainFrame* mainFrame;

    UINT taskbarMessageId;
    ITaskbarList3* taskbarInterface;

    std::unordered_map<QString, QIcon> screenshots;

    /** Icon on the tray or zeros */
    NOTIFYICONDATAW nid;

    /** tab control */
    HWND tabs;

    HWND toolbar;

    std::vector<HWND> tabPanels;

    /** panel for packages */
    HWND packagesPanel;

    /** panel for progress */
    HWND progressPanel;

    HMENU mainMenu;

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
    void selectPackages(std::vector<Package *> ps);

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

    /**
     * @brief saves instance handle and creates main window. In this function,
     * we save the instance handle in a global variable and
     * create and display the main program window.
     *
     * @param nCmdShow SW_*
     */
    void createMainWindow(int);

    /**
     * @brief create the main menu
     * @return menu handle
     */
    void createMainMenu();

    /**
     * @brief Creates a tab control, sized to fit the specified parent window's client
     * area, and adds some tabs.
     * @param hwndParent parent window (the application's main window)
     * @return handle to the tab control
     */
    HWND createTabs(HWND hwndParent);

    HWND createToolbar(HWND parent);
    void addRichTextTab(const QString &title, const QString &rtf);
    void on_actionToggle_toolbar_triggered();
public:
    /** handle */
    HWND window;

    /**
     * @brief layout the main window
     */
    void layoutMainWindow();

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
     * @brief shows an icon in the system tray
     *
     * @param nupdates number of found package updates
     */
    void showIconInSystemTray(int nupdates);

    /**
     * @brief hides the system tray icon
     */
    void hideIconInSystemTray();

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
     */
    void addTextTab(const QString& title, const QString& text);

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
     * Adds a new tab. The new tab will *not* be automatically selected.
     *
     * @param w content of the new tab
     * @param icon tab icon
     * @param title tab title
     */
    void addTab(HWND w, const QIcon& icon, const QString& title);

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

    std::vector<void *> getSelected(const QString& type) const;

    void openURL(const QUrl &url);

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

    void onShow();

    /**
     * @brief runs the GUI
     * @param nCmdShow how the window should be shown (see WinMain)
     * @return process exit code
     */
    int runGUI(int nCmdShow);

    /**
     * @brief process messages
     *
     * @param hWnd window handle
     * @param message message
     * @param wParam first parameter
     * @param lParam second parameter
     * @return result
     */
    LRESULT windowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    /**
     * @return number of tabs
     */
    int getTabCount() const;

    /**
     * @brief selects a tab
     * @param index tab index
     */
    void selectTab(int index);
protected:
    void changeEvent(QEvent *e);

    /**
     * @param install the objects will be destroyed
     * @param opsDependencies dependencies between installation operations. The
     *     nodes of the graph are indexes in the "ops" vector. An edge from "u"
     *     to "v" means that "u" depends on "v".
     * @param programCloseType how the programs should be closed
     */
    void process(std::vector<InstallOperation *> &install,
            const DAG &opsDependencies, DWORD programCloseType);

public slots:
    void on_errorMessage(const QString& msg, const QString& details="",
            bool autoHide=true, QMessageBox::Icon icon=QMessageBox::Critical);
private slots:
    void on_actionGotoPackageURL_triggered();
    void processThreadFinished();
    void recognizeAndLoadRepositoriesThreadFinished();
    void on_actionShow_Details_triggered();
    void on_tabWidget_currentChanged(int index);
    void on_tabWidget_tabCloseRequested(int index);
    void on_actionAbout_triggered();
    void on_actionTest_Download_Site_triggered();
    void on_actionUpdate_triggered();
    void on_actionSettings_triggered();
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
    void monitoredJobCompleted();
    void on_actionRun_triggered();
    void on_actionExport_triggered();
    void on_actionCheck_dependencies_triggered();

};

#endif // MAINWINDOW_H
