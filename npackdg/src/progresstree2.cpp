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
#include "wpmutils.h"
#include "mainwindow.h"

class CancelPushButton: public QPushButton
{
public:
    QTreeWidgetItem* item;

    CancelPushButton(QWidget *parent): QPushButton(parent), item(nullptr) {}
    virtual ~CancelPushButton();
};

CancelPushButton::~CancelPushButton()
{
    // this virtual destructor is necessary to remove the Clang warning
    // 'CancelPushButton' has no out-of-line virtual method definitions;
    // its vtable will be emitted in every translation unit [-Wweak-vtables]
}

ProgressTree2::ProgressTree2(QWidget *parent) :
    QTreeWidget(parent)
{
    this->autoExpandNodes = false;
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
    hls.append(QObject::tr("Remaining time"));
    hls.append(QObject::tr("Progress"));
    hls.append(QObject::tr(""));
    setHeaderLabels(hls);
}

void ProgressTree2::timerTimeout()
{

}

QTreeWidgetItem* ProgressTree2::findItem(Job* job, bool create)
{
    QList<Job*> path;
    Job* v = job;
    while (v) {
        path.insert(0, v);
        v = v->parentJob;
    }

    QTreeWidgetItem* c = nullptr;
    for (int i = 0; i < path.count(); i++) {
        Job* toFind = path.at(i);
        QTreeWidgetItem* found = nullptr;
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
            if (c) {
                for (int j = 0; j < c->childCount(); j++) {
                    QTreeWidgetItem* item = c->child(j);
                    Job* v = getJob(*item);
                    if (v == toFind) {
                        found = item;
                        break;
                    }
                }
            }
        }
        if (found)
            c = found;
        else {
            if (create) {
                if (!toFind->parentJob)
                    ; //c = addJob(toFind);
                else {
                    if (c) {
                        QTreeWidgetItem* subItem = new QTreeWidgetItem(c);
                        fillItem(subItem, toFind);
                        if (autoExpandNodes)
                            c->setExpanded(true);
                        c = subItem;
                    }
                }
            } else {
                c = nullptr;
                break;
            }
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

    item->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(job)));
}

QTreeWidgetItem* ProgressTree2::addJob(Job* job)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(this);
    fillItem(item, job);

    connect(job, SIGNAL(subJobCreated(Job*)), this,
            SLOT(subJobCreated(Job*)),
            Qt::QueuedConnection);

    connect(job, SIGNAL(changed(Job*)), this,
            SLOT(monitoredJobChanged(Job*)),
            Qt::QueuedConnection);

    addTopLevelItem(item);

    return item;
}

void ProgressTree2::removeJob(Job *job)
{
    if (!job->parentJob) {
        QTreeWidgetItem* item = findItem(job);
        if (item) {
            takeTopLevelItem(this->indexOfTopLevelItem(item));
            delete item;
        }
    }
}

void ProgressTree2::subJobCreated(Job* sub)
{
    findItem(sub, true);
}

void ProgressTree2::cancelClicked()
{
    CancelPushButton* pb = static_cast<CancelPushButton*>(QObject::sender());
    pb->setEnabled(false);
    Job* job = getJob(*pb->item);
    job->cancel();
}

void ProgressTree2::monitoredJobChanged(Job* state)
{
    time_t now;
    time(&now);

    monitoredJobLastChanged = now;

    QTreeWidgetItem* item = findItem(state, true);
    if (item)
        updateItem(item, state);

    if (state->isCompleted()) {
        if (item) {
            setItemWidget(item, 4, nullptr);
        }
    }
}

void ProgressTree2::updateItem(QTreeWidgetItem* item, Job* s)
{
    item->setText(0, s->getTitle());

    time_t now;
    time(&now);

    if (s->getStarted() != 0) {
        time_t diff = static_cast<time_t>(difftime(now, s->getStarted()));

        QTime e = WPMUtils::durationToTime(diff);
        item->setText(1, e.toString());

        if (!s->isCompleted()) {
            QTime r = WPMUtils::durationToTime(lround(
                    diff * (1 / s->getProgress() - 1)));
            item->setText(2, r.toString());
        } else {
            item->setText(2, "");
        }
    } else {
        item->setText(1, "");
        item->setText(2, "");
    }

    QProgressBar* pb = reinterpret_cast<QProgressBar*>(itemWidget(item, 3));
    QPushButton* b = reinterpret_cast<QPushButton*>(itemWidget(item, 4));
    if (s->isCompleted()) {
        pb->setValue(10000);
        if (b)
            b->setEnabled(false);
    } else {
        pb->setValue(lround(s->getProgress() * 10000));
        if (b)
            b->setEnabled(!s->isCancelled());
    }
}

Job* ProgressTree2::getJob(const QTreeWidgetItem& item)
{
    const QVariant v = item.data(0, Qt::UserRole);
    Job* f = static_cast<Job*>(v.value<void*>());
    return f;
}
