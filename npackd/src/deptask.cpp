#include "deptask.h"

DepTask::DepTask(const DepTask &other): job(other.job),
    tasks(other.tasks),
    dependencies(other.dependencies),
    threadPool(other.threadPool)
{
}

DepTask::DepTask(Job* job, const std::vector<std::function<void(Job *)>> &tasks,
        const DAG &dependencies, ThreadPool &threadPool):
    job(job), tasks(tasks), dependencies(dependencies), threadPool(threadPool)
{
}

void DepTask::operator ()()
{
    std::mutex statusMutex;

    // 0 - not started, 1 - scheduled, 2 - done
    std::vector<int> status(tasks.size());

    int doneCount = 0;

    this->dependencies.resize(tasks.size());
    std::vector<int> order = dependencies.topologicalSort();

    while (job->shouldProceed()) {
        int found = -1;

        {
            std::unique_lock<std::mutex> lock{statusMutex};
            for (int i = 0; i < static_cast<int>(order.size()); i++) {
                if (status[order[i]] == 0) {
                    bool depsOK = true;
                    auto edges = dependencies.getEdges(order[i]);
                    for (auto edge: edges) {
                        if (status[edge] != 2) {
                            depsOK = false;
                            break;
                        }
                    }

                    if (depsOK) {
                        found = order[i];
                        break;
                    }
                }
            }
        }

        if (found >= 0) {
            {
                std::unique_lock<std::mutex> lock{statusMutex};
                status[found] = 1;
            }

            threadPool.addTask([this, found, &status, &statusMutex, &doneCount](){
                Job* sub = this->job->newSubJob(1.0 / this->tasks.size(),
                        QObject::tr("Installation operation"), true, true);
                (tasks[found])(sub);
                cv.notify_one();

                {
                    std::unique_lock<std::mutex> lock{statusMutex};
                    status[found] = 2;
                    doneCount++;
                }
            });
        }

        {
            std::unique_lock<std::mutex> lock{statusMutex};

            if (doneCount == static_cast<int>(order.size()))
                break;
        }

        std::unique_lock<std::mutex> lock{mutex};
        cv.wait_for(lock, std::chrono::seconds(1));
    }

    if (job->shouldProceed())
        job->setProgress(1);

    job->complete();
}
