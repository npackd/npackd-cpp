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
    Job* job;
public:
    QList<InstallOperation*> install;

    /**
     * how the programs should be closed. The value is automatically
     * initialized to the current defaults.
     */
    DWORD programCloseType;

    InstallThread(Job* job);

    void run();
};

#endif // INSTALLTHREAD_H
