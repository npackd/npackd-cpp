#ifndef INSTALLTHREAD_H
#define INSTALLTHREAD_H

#include <QThread>
#include <QString>
#include <QList>

#include "packageversion.h"
#include "job.h"
#include "installoperation.h"

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
     * how the programs should be closed. The value is automatically
     * initialized to the current defaults.
     */
    DWORD programCloseType;

    /**
     * type = 3 or 4
     * true (default value) = the HTTP cache will be used
     */
    bool useCache;

    InstallThread(PackageVersion* pv, int type, Job* job);

    void run();
};

#endif // INSTALLTHREAD_H
