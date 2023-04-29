#ifndef CLPROGRESS_H
#define CLPROGRESS_H

#include <time.h>
#include <windows.h>

#include <QObject>
#include <QString>

#include "job.h"

/**
 * Command line job progress monitor.
 */
class CLProgress : public QObject
{
    Q_OBJECT
private slots:
    void jobChanged(Job *s);
    void jobChangedSimple(Job *s);
private:
    int updateRate;
    CONSOLE_SCREEN_BUFFER_INFO progressPos;
    time_t lastJobChange;
    QString lastHint;
public:
    explicit CLProgress(QObject *parent = nullptr);

    /**
     * @return a new job object connected to the console output
     */
    Job* createJob();

    /**
     * Changes the update rate
     *
     * @param r new update rate in seconds. 0 means "output everything"
     */
    void setUpdateRate(int r);
signals:

public slots:

};

#endif // CLPROGRESS_H
