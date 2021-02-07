#include "deptask.h"

DepTask::DepTask(const std::vector<std::function<void ()> > &tasks,
        const DAG &dependencies, ThreadPool &threadPool):
    tasks(tasks), dependencies(dependencies), threadPool(threadPool)
{
}

void DepTask::operator ()()
{
    std::mutex doneMutex;
    std::vector<bool> done;
    this->dependencies.resize(tasks.size());
    std::vector<int> order = dependencies.topologicalSort();
    done.resize(tasks.size());

    int scheduled = 0;
    while (true) {
        int found = -1;
        {
            std::unique_lock<std::mutex> lock{doneMutex};
            for (int i = 0; i < static_cast<int>(order.size()); i++) {
                if (!done[order[i]]) {
                    bool depsOK = true;
                    auto edges = dependencies.getEdges(order[i]);
                    for (auto edge: edges) {
                        if (!done[edge]) {
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
            threadPool.addTask([this, found, &done, &doneMutex](){
                (tasks[found])();
                cv.notify_one();

                {
                    std::unique_lock<std::mutex> lock{doneMutex};
                    done[found] = true;
                }
            });
            scheduled++;
        }

        if (scheduled == static_cast<int>(order.size()))
            break;

        std::unique_lock<std::mutex> lock{mutex};
        cv.wait_for(lock, std::chrono::seconds(1));
    }
}
