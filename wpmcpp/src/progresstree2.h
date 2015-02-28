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
    QTreeWidgetItem *findItem(Job *job, bool create=false);
public:
    /**
     * Should the nodes be automatically expanded if new sub-jobs are created?
     * Default value is true.
     */
    bool autoExpandNodes;

    /**
     * @param parent parent widget
     */
    ProgressTree2(QWidget* parent);

    /**
     * @brief registers a new job. The thread should not be started yet.
     * @param job the job
     * @return the created node
     */
    QTreeWidgetItem *addJob(Job* job);

    /**
     * @brief set the widths of the columns for a small dialog
     */
    void setNarrowColumns();
private slots:
    void cancelClicked();
    void monitoredJobChanged(const JobState &state);
    void timerTimeout();
    void subJobCreated(Job *sub);
};

#endif // PROGRESSTREE2_H
