#ifndef PROGRESSTREE2_H
#define PROGRESSTREE2_H

#include <time.h>

#include <QTreeWidget>
#include <QTimer>
#include <QThread>
#include <QString>
#include <QWidget>
#include <QString>

#include "job.h"

/**
 * @brief tree with the running jobs
 */
class ProgressTree2: public QTreeWidget
{
    Q_OBJECT
private:
    QTimer* timer;
    time_t monitoredJobLastChanged;
    void updateItem(QTreeWidgetItem *item, const JobState &s);
    Job *getJob(const QTreeWidgetItem &item);
    void fillItem(QTreeWidgetItem* item, Job *job);
    QTreeWidgetItem *findItem(Job *job);
public:
    /**
     * @param parent parent widget
     */
    ProgressTree2(QWidget* parent);

    /**
     * @brief registers a new job
     * @param job the job
     * @param thread job thread
     */
    void addJob(Job* job, QThread* thread);
private slots:
    void cancelClicked();
    void monitoredJobChanged(const JobState &state);
    void threadFinished();
    void timerTimeout();
    void subJobCreated(Job *sub);
};

#endif // PROGRESSTREE2_H
