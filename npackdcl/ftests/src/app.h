#ifndef APP_H
#define APP_H

#include <QObject>
#include <QtTest/QtTest>

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
};

#endif // APP_H
