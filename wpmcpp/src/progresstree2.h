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
     * @brief registers a new job. The thread should not be started yet.
     * @param job the job
     */
    void addJob(Job* job);
private slots:
    void cancelClicked();
    void monitoredJobChanged(const JobState &state);
    void timerTimeout();
    void subJobCreated(Job *sub);
};

#endif // PROGRESSTREE2_H
