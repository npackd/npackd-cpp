#include <math.h>

#include <QList>
#include <QTreeWidgetItem>
#include <QStringList>
#include <QString>
#include <QProgressBar>
#include <QPushButton>
#include <QMessageBox>
#include <QHeaderView>

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
    QTreeWidget(parent), started(0)
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

void ProgressTree2::addJob(Job* job, const QString& title, QThread* thread)
{
    QTreeWidgetItem* item;
    item = new QTreeWidgetItem((QTreeWidget*)0, QStringList(title));

    addTopLevelItem(item);

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
    item->setData(1, Qt::UserRole, QVariant(title));

    connect(job, SIGNAL(changed(const JobState&)), this,
            SLOT(monitoredJobChanged(const JobState&)),
            Qt::QueuedConnection);

    connect(thread, SIGNAL(finished()), this,
            SLOT(threadFinished()),
            Qt::QueuedConnection);

    thread->start(QThread::LowestPriority);
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

    if (now != monitoredJobLastChanged) {
        monitoredJobLastChanged = now;

        int index = VisibleJobs::getDefault()->runningJobs.indexOf(state.job);

        QTreeWidgetItem* item = this->topLevelItem(index);
        updateItem(item, state);
    }

    if (state.completed) {
        int index = VisibleJobs::getDefault()->runningJobs.indexOf(state.job);
        QTreeWidgetItem* item = this->topLevelItem(index);

        Job* job = state.job;

        if (!job->getErrorMessage().isEmpty()) {
            QString jobTitle = item->data(1, Qt::UserRole).toString();
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
        VisibleJobs::getDefault()->unregisterJob(job);

        job->deleteLater();

        delete item;
    }
}

void ProgressTree2::updateItem(QTreeWidgetItem* item, const JobState& s)
{
    if (s.completed) {
        // nothing
    } else {
        time_t now;
        time(&now);

        QString jobTitle = item->data(1, Qt::UserRole).toString();
        item->setText(0, jobTitle + " / " + s.hint);

        if (started != 0) {
            time_t diff = difftime(now, started);

            int sec = diff % 60;
            diff /= 60;
            int min = diff % 60;
            diff /= 60;
            int h = diff;

            QTime e(h, min, sec);
            item->setText(1, e.toString());

            diff = difftime(now, started);
            diff = lround(diff * (1 / s.progress - 1));
            sec = diff % 60;
            diff /= 60;
            min = diff % 60;
            diff /= 60;
            h = diff;

            QTime r(h, min, sec);
            item->setText(2, r.toString());
        } else {
            time(&this->started);
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
