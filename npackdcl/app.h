#ifndef APP_H
#define APP_H

#include <time.h>

#include <QtCore/QCoreApplication>
#include <qdebug.h>
#include <qstringlist.h>
#include <qstring.h>

#include "..\wpmcpp\repository.h"
#include "..\wpmcpp\commandline.h"
#include "..\wpmcpp\job.h"
#include "..\wpmcpp\clprogress.h"

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

    bool confirm(const QList<InstallOperation *> ops, QString *title,
            QString *err);
    QString printDependencies(bool onlyInstalled,
            const QString parentPrefix, int level, PackageVersion *pv);

    /**
     * @param package full or short package name
     * @param err error message will be stored here
     * @return [ownership:caller] found package or 0. The returned value is
     *     only 0 if the error is not empty
     */
    Package *findOnePackage(const QString &package, QString *err);
public:
    /**
     * Process the command line.
     *
     * @return exit code
     */
    int process();
};

#endif // APP_H
