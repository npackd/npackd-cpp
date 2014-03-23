#ifndef UPDATEREPOSITORYTHREAD_H
#define UPDATEREPOSITORYTHREAD_H

#include <QThread>
#include <QString>
#include <QList>

#include "packageversion.h"
#include "job.h"
#include "installoperation.h"

class UpdateRepositoryThread: public QThread
{
    Job* job;
public:
    UpdateRepositoryThread(Job* job);

    void run();
};

#endif // UPDATEREPOSITORYTHREAD_H
