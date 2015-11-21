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
public:
    App();
private slots:
    /**
     * @brief "add"/"remove"
     */
    void addRemove();

    /**
     * @brief "add" --file=<installation directory>
     */
    void addToDir();

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
};

#endif // APP_H
