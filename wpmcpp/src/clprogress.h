#ifndef CLPROGRESS_H
#define CLPROGRESS_H

#include <time.h>
#include <windows.h>

#include <QObject>
#include <QString>

#include "..\wpmcpp\job.h"

/**
 * Command line job progress monitor.
 */
class CLProgress : public QObject
{
    Q_OBJECT
private slots:
    void jobChanged(const JobState& s);
    void jobChangedSimple(const JobState& s);
private:
    int updateRate;
    CONSOLE_SCREEN_BUFFER_INFO progressPos;
    time_t lastJobChange;
    QString lastHint;
public:
    explicit CLProgress(QObject *parent = 0);

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
