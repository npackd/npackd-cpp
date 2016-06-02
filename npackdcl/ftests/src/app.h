#ifndef APP_H
#define APP_H

#include <QObject>
#include <QtTest/QtTest>

/**
 * @brief functional tests for NpackdCL. This tests should also work for
 * non-admins. Test that cannot be run for non-admins will be skipped.
 */
class App: public QObject
{
    Q_OBJECT
private:
    bool admin;

    QString captureNpackdCLOutput(const QString &params);
    QString captureOutput(const QString &program, const QString &params,
            const QString &where);
public:
    App();

private slots:
    void init();

    /**
     * @brief "add"/"remove"
     */
    void addRemove();

    /**
     * @brief "add"/"remove" with a file share
     */
    void addRemoveShare();

    /**
     * @brief "add" --file=<installation directory>
     */
    void addToDir();

    /**
     * @brief "add" --file=<installation directory> for an existing dir
     */
    void addToExistingDir();

    /**
     * @brief "update" -f
     */
    void updateToDir();

    /**
     * @brief "update" --keep-directories
     */
    void updateKeepDirectories();

    /**
     * @brief "place"
     */
    void place();

    /**
     * @brief "add" --versions
     */
    void addVersions();

    /**
     * @brief "update" --install
     */
    void updateInstall();

    /**
     * @brief "add"/"remove" for a running program
     */
    void addRemoveRunning();

    /**
     * @brief "add" should not create another installed version for a detected
     *     package in the same directory. For example, installing
     *     net.poedit.POEdit in "C:\ProgramFiles" should not result in another
     *     version being detected also in "C:\ProgramFiles\POEdit"
     */
    void addDoesntProduceDetected();

    /**
     * @brief "add" without the version number
     */
    void addWithoutVersion();

    /**
     * @brief "npackdcl path" must be fast
     */
    void pathIsFast();

    /**
     * @brief "npackdcl path --version"
     */
    void pathVersion();

    /**
     * @brief "check"
     */
    void check();

    /**
     * @brief "list-repos"
     */
    void listRepos();

    /**
     * @brief "info"
     */
    void info();

    /**
     * @brief "search"
     */
    void search();

    /**
     * @brief "where"
     */
    void where();

    /**
     * @brief "set-repo"
     */
    void setRepo();

    /**
     * @brief "add-repo"/"remove-repo"
     */
    void addRemoveRepo();

    /**
     * @brief "detect"
     */
    void detect();

    /**
     * @brief "list"
     */
    void list();

    /**
     * @brief "help"
     */
    void help();

    /**
     * @brief "which"
     */
    void which();

    /**
     * @brief "install-dir/set-install-dir"
     */
    void installDir();
};

#endif // APP_H
