#ifndef APP_H
#define APP_H

#include <time.h>

#include <QJsonObject>
#include <QtCore/QCoreApplication>

#include "repository.h"
#include "commandline.h"
#include "job.h"
#include "clprogress.h"
#include "dbrepository.h"

/**
 * NpackdCL
 */
class App: public QObject
{
    Q_OBJECT
private:
    CommandLine cl;
    CLProgress clp;

    bool debug;
    bool interactive;

    static void printJSON(const QJsonObject & obj);

    /**
     * @brief defines the NPACKD_CL variable and adds the NpackdCL package to
     *     the local repository
     *
     * @param r DB repository
     * @return error message
     */
    QString addNpackdCL(DBRepository *r);

    void usage(Job *job);
    void path(Job* job);
    void place(Job *job);
    void add(Job *job);
    void remove(Job *job);
    void addRepo(Job *job);
    void setRepo(Job *job);
    void removeRepo(Job *job);
    void search(Job *job);
    void list(Job *job);
    void info(Job *job);
    void update(Job *job);
    void detect(Job *job);
    void listRepos(Job *job);
    void which(Job *job);
    void where(Job *job);
    void check(Job *job);
    void getInstallPath(Job *job);
    void setInstallPath(Job *job);
    void removeSCP(Job *job);
    void build(Job *job);

    bool confirm(const QList<InstallOperation *> ops, QString *title,
            QString *err);
    QString printDependencies(bool onlyInstalled,
            const QString parentPrefix, int level, PackageVersion *pv);
    void processInstallOperations(Job *job, DBRepository *rep,
            const QList<InstallOperation *> &ops, DWORD programCloseType,
            bool interactive, const QString user, const QString password,
            const QString proxyUser, const QString proxyPassword);
    QStringList sortPackageVersionsByPackageTitle(
            QList<PackageVersion *> *list);
public:
    App();
    Job* currentJob;
public slots:
    /**
     * Process the command line.
     *
     * @return exit code
     */
    int process();
};

#endif // APP_H
