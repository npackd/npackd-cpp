#include <math.h>

#include <QList>
#include <QTreeWidgetItem>
#include <QStringList>
#include <QString>
#include <QProgressBar>
#include <QPushButton>
#include <QMessageBox>
#include <QHeaderView>
#include <QDebug>

#include "progresstree2.h"
#include "job.h"
#include "visiblejobs.h"
#include "wpmutils.h"
#include "mainwindow.h"

class CancelPushButton: public QPushButton
{
public:
    QTreeWidgetItem* item;

    CancelPushButton(QWidget *parent): QPushButton(parent) {}
};

ProgressTree2::ProgressTree2(QWidget *parent) :
    QTreeWidget(parent)
{
    setColumnCount(5);

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timerTimeout()));
    timer->start(1000);

    this->monitoredJobLastChanged = 0;

    setColumnWidth(0, 500);
    setColumnWidth(1, 100);
    setColumnWidth(2, 100);
    setColumnWidth(3, 200);
    setColumnWidth(4, 100);
    header()->setStretchLastSection(false);

    QStringList hls;
    hls.append(QObject::tr("Task / Step"));
    hls.append(QObject::tr("Elapsed time"));
    hls.append(QObject::tr("Estimated time"));
    hls.append(QObject::tr("Progress"));
    hls.append(QObject::tr(""));
    setHeaderLabels(hls);
}

void ProgressTree2::timerTimeout()
{

}

QTreeWidgetItem* ProgressTree2::findItem(Job* job)
{
    QList<Job*> path;
    Job* v = job;
    while (v) {
        path.insert(0, v);
        v = v->parentJob;
    }

    QTreeWidgetItem* c = 0;
    for (int i = 0; i < path.count(); i++) {
        Job* toFind = path.at(i);
        QTreeWidgetItem* found = 0;
        if (i == 0) {
            for (int j = 0; j < this->topLevelItemCount(); j++) {
                QTreeWidgetItem* item = this->topLevelItem(j);
                Job* v = getJob(*item);
                if (v == toFind) {
                    found = item;
                    break;
                }
            }
        } else {
            for (int j = 0; j < c->childCount(); j++) {
                QTreeWidgetItem* item = c->child(j);
                Job* v = getJob(*item);
                if (v == toFind) {
                    found = item;
                    break;
                }
            }
        }
        if (found)
            c = found;
        else {
            c = 0;
            break;
        }
    }

    return c;
}

void ProgressTree2::fillItem(QTreeWidgetItem* item,
        Job* job)
{
    item->setText(0, job->getTitle().isEmpty() ?
            "-" : job->getTitle());

    QProgressBar* pb = new QProgressBar(this);
    pb->setMaximum(10000);
    setItemWidget(item, 3, pb);

    CancelPushButton* cancel = new CancelPushButton(this);
    cancel->setText(QObject::tr("Cancel"));
    cancel->item = item;
    connect(cancel, SIGNAL(clicked()), this,
            SLOT(cancelClicked()));
    setItemWidget(item, 4, cancel);

    item->setData(0, Qt::UserRole, qVariantFromValue((void*) job));
}

void ProgressTree2::addJob(Job* job, QThread* thread)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(this);
    fillItem(item, job);

    connect(job, SIGNAL(subJobCreated(Job*)), this,
            SLOT(subJobCreated(Job*)),
            Qt::QueuedConnection);

    connect(job, SIGNAL(changed(const JobState&)), this,
            SLOT(monitoredJobChanged(const JobState&)),
            Qt::QueuedConnection);

    addTopLevelItem(item);

    connect(thread, SIGNAL(finished()), this, SLOT(threadFinished()),
            Qt::QueuedConnection);

    thread->start(QThread::LowestPriority);
}

void ProgressTree2::subJobCreated(Job* sub)
{
    Job* job = sub->parentJob;
    QTreeWidgetItem* item = findItem(job);
    if (item) {
        QTreeWidgetItem* subItem = new QTreeWidgetItem(item);
        fillItem(subItem, sub);
        item->setExpanded(true);
    }
}

void ProgressTree2::cancelClicked()
{
    CancelPushButton* pb = (CancelPushButton*) QObject::sender();
    pb->setEnabled(false);
    Job* job = getJob(*pb->item);
    job->cancel();
}

void ProgressTree2::monitoredJobChanged(const JobState& state)
{
    time_t now;
    time(&now);

    // TODO: the index of the Job may be wrong here

    //if (now != monitoredJobLastChanged) {
        monitoredJobLastChanged = now;

        QTreeWidgetItem* item = findItem(state.job);
        if (item)
            updateItem(item, state);
    //}

    if (state.completed) {
        QTreeWidgetItem* item = findItem(state.job);

        if (item) {
            setItemWidget(item, 4, 0);
        }

        Job* job = state.job;

        if (!job->getErrorMessage().isEmpty()) {
            QString jobTitle = job->getTitle();
            QString title = QObject::tr("Error") + ": " + jobTitle +
                        " / " + job->getHint() +
                        ": " + WPMUtils::getFirstLine(job->getErrorMessage());
            QString msg = job->getHint() + "\n" + job->getErrorMessage();
            if (MainWindow::getInstance())
                MainWindow::getInstance()->addErrorMessage(
                        title, msg);
            else {
                QMessageBox mb;
                mb.setWindowTitle(title);
                mb.setText(msg);
                mb.setIcon(QMessageBox::Warning);
                mb.setStandardButtons(QMessageBox::Ok);
                mb.setDefaultButton(QMessageBox::Ok);
                mb.setDetailedText(msg);
                mb.exec();
            }
        }

        if (!job->parentJob) {
            VisibleJobs::getDefault()->unregisterJob(job);

            // TODO: should be probably a QSharedPointer job->deleteLater();

            delete item;
        }
    }
}

void ProgressTree2::updateItem(QTreeWidgetItem* item, const JobState& s)
{
    if (s.completed) {
        // nothing
    } else {
        time_t now;
        time(&now);

        if (item->childCount() == 0)
            item->setText(0, s.job->getTitle() + " / " + s.job->getHint());
        else
            item->setText(0, s.job->getTitle());

        if (s.started != 0) {
            time_t diff = difftime(now, s.started);

            int sec = diff % 60;
            diff /= 60;
            int min = diff % 60;
            diff /= 60;
            int h = diff;

            QTime e(h, min, sec);
            item->setText(1, e.toString());

            diff = difftime(now, s.started);
            diff = lround(diff * (1 / s.progress - 1));
            sec = diff % 60;
            diff /= 60;
            min = diff % 60;
            diff /= 60;
            h = diff;

            QTime r(h, min, sec);
            item->setText(2, r.toString());
        } else {
            item->setText(1, "-");
        }

        QProgressBar* pb = (QProgressBar*) itemWidget(item, 3);
        pb->setValue(lround(s.progress * 10000));

        QPushButton* b = (QPushButton*) itemWidget(item, 4);
        b->setEnabled(!s.cancelRequested);
    }
}

void ProgressTree2::threadFinished()
{
    QThread* t = (QThread*) QObject::sender();
    t->deleteLater();
}

Job* ProgressTree2::getJob(const QTreeWidgetItem& item)
{
    const QVariant v = item.data(0, Qt::UserRole);
    Job* f = (Job*) v.value<void*>();
    return f;
}
