#include <math.h>

#include <QList>
#include <QTreeWidgetItem>
#include <QStringList>
#include <QString>
#include <QProgressBar>
#include <QPushButton>

#include "progresstree2.h"
#include "job.h"
#include "visiblejobs.h"

ProgressTree2::ProgressTree2(QWidget *parent) :
    QTreeWidget(parent), started(0)
{
    setColumnCount(5);

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timerTimeout()));
    timer->start(1000);

    this->monitoredJobLastChanged = 0;
}

void ProgressTree2::addJob(Job* job, const QString& title, QThread* thread)
{
    this->title = title;
    this->thread = thread;

    QTreeWidgetItem * item;
    item = new QTreeWidgetItem((QTreeWidget*)0, QStringList(title));

    addTopLevelItem(item);

    QProgressBar* pb = new QProgressBar(this);
    pb->setMaximum(10000);
    setItemWidget(item, 3, pb);

    QPushButton* cancel = new QPushButton(this);
    cancel->setText(QObject::tr("Cancel"));
    connect(cancel, SIGNAL(clicked()), this,
            SLOT(cancelClicked()));
    setItemWidget(item, 4, cancel);

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
    // TODO: ui->pushButtonCancel->setEnabled(false);
    // TODO: job->cancel();
}

void ProgressTree2::monitoredJobChanged(const JobState& state)
{
    time_t now;
    time(&now);

    // TODO: the index of the Job may be wrong here

    if (now != monitoredJobLastChanged) {
        monitoredJobLastChanged = now;

        int index = VisibleJobs::getDefault()->runningJobs.indexOf(state.job);

        QTreeWidgetItem * item = this->topLevelItem(index);
        updateItem(item, state);
    }

    if (state.completed) {
        int index = VisibleJobs::getDefault()->runningJobs.indexOf(state.job);
        QTreeWidgetItem * item = this->topLevelItem(index);
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

        item->setText(0, this->title + " / " + s.hint);

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
        /*
         * TODO:
        ui->pushButtonCancel->setEnabled(!s.cancelRequested);
        */
    }
}

void ProgressTree2::threadFinished()
{
    /* TODO:
    if (!job->getErrorMessage().isEmpty()) {
        QString title = QObject::tr("Error") + ": " + this->title +
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
    */

    delete this->thread;

    /* TODO:
    VisibleJobs::getDefault()->unregisterJob(this->job);

    delete this->job;
    delete ui;
    */
}

