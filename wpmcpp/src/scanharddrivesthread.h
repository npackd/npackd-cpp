#ifndef SCANHARDDRIVESTHREAD_H
#define SCANHARDDRIVESTHREAD_H

#include <QThread>
#include <QMap>
#include <QString>
#include <QList>

#include "job.h"
#include "packageversion.h"

/**
 * Scan hard drives.
 */
class ScanHardDrivesThread: public QThread
{
    Job* job;
public:
    QMap<QString, int> words;

    QList<PackageVersion*> detected;

    ScanHardDrivesThread(Job* job);

    void run();
};

#endif // SCANHARDDRIVESTHREAD_H
