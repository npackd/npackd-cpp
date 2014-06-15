#ifndef APP_H
#define APP_H

#include <time.h>

#include <QtCore/QCoreApplication>
#include <qdebug.h>
#include <qstringlist.h>
#include <qstring.h>

#include "repository.h"
#include "commandline.h"
#include "job.h"
#include "clprogress.h"

/**
 * NpackdCL
 */
class App: public QObject
{
    Q_OBJECT
private:
    CommandLine cl;
    CLProgress clp;

    /**
     * @brief defines the NPACKD_CL variable and adds the NpackdCL package to
     *     the local repository
     * @return error message
     */
    QString addNpackdCL();

    void usage();
    QString path();
    QString add();
    QString remove();
    QString addRepo();
    QString removeRepo();
    QString search();
    QString list();
    QString info();
    QString update();
    QString detect();
    QString listRepos();
    QString which();
    QString check();
    QString getInstallPath();
    QString setInstallPath();

    bool confirm(const QList<InstallOperation *> ops, QString *title,
            QString *err);
    QString printDependencies(bool onlyInstalled,
                              const QString parentPrefix, int level, PackageVersion *pv);
    void processInstallOperations(Job *job,
            const QList<InstallOperation *> &ops, DWORD programCloseType);
public slots:
    /**
     * Process the command line.
     *
     * @return exit code
     */
    int process();

    /**
     * Tests
     *
     * @return exit code
     */
#ifdef TEST
    int test();
#endif
};

#endif // APP_H
