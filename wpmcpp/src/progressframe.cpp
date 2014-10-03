#include <time.h>
#include <math.h>

#include <QTime>
#include <QTimer>
#include <QApplication>

#include "progressframe.h"
#include "ui_progressframe.h"
#include "mainwindow.h"
#include "wpmutils.h"
#include "visiblejobs.h"

ProgressFrame::ProgressFrame(QWidget *parent, Job* job, const QString& title,
        QThread* thread) :
    QFrame(parent),
    ui(new Ui::ProgressFrame)
{
    ui->setupUi(this);
    ui->progressBar->setMaximum(10000);

    this->title = title;
    this->job = job;
    this->started = 0;
    this->modified = 0;
    this->thread = thread;

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timerTimeout()));
    timer->start(1000);

    connect(job, SIGNAL(changed(const JobState&)), this,
            SLOT(jobChanged(const JobState&)),
            Qt::QueuedConnection);
    connect(thread, SIGNAL(finished()), this,
            SLOT(threadFinished()),
            Qt::QueuedConnection);

    thread->start(QThread::LowestPriority);
}

ProgressFrame::~ProgressFrame()
{
    if (!job->getErrorMessage().isEmpty()) {
        QString title = QObject::tr("Error") + ": " + this->title +
                    ": " + WPMUtils::getFirstLine(job->getErrorMessage());
        QString msg = job->getFullTitle() + "\n" + job->getErrorMessage();
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
    delete this->thread;

    VisibleJobs::getDefault()->unregisterJob(this->job);

    delete this->job;
    delete ui;
}

void ProgressFrame::threadFinished()
{
    this->deleteLater();
}

void ProgressFrame::timerTimeout()
{
}

void ProgressFrame::jobChanged(const JobState& s)
{
    if (s.completed) {
        // nothing
    } else {
        time_t now;
        time(&now);
        if (now != this->modified) {
            ui->labelStep->setText(this->title + " / " + s.hint);
            this->modified = now;

            if (started != 0) {
                time_t diff = difftime(now, started);

                int sec = diff % 60;
                diff /= 60;
                int min = diff % 60;
                diff /= 60;
                int h = diff;

                QTime e(h, min, sec);
                ui->labelElapsed->setText(e.toString());

                diff = difftime(now, started);
                diff = lround(diff * (1 / s.progress - 1));
                sec = diff % 60;
                diff /= 60;
                min = diff % 60;
                diff /= 60;
                h = diff;

                QTime r(h, min, sec);
                ui->label_remainingTime->setText(r.toString());
            } else {
                time(&this->started);
                ui->labelElapsed->setText("-");
            }
            ui->progressBar->setValue(lround(s.progress * 10000));
            ui->pushButtonCancel->setEnabled(!s.cancelRequested);
        }
    }
}

void ProgressFrame::on_pushButtonCancel_clicked()
{
    ui->pushButtonCancel->setEnabled(false);
    job->cancel();
}
