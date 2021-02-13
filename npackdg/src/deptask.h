#ifndef DEPTASK_H
#define DEPTASK_H

#include <vector>
#include <functional>
#include <condition_variable>
#include <mutex>

#include "dag.h"
#include "threadpool.h"
#include "job.h"

/**
 * @brief a task that executes a list of tasks with dependencies between them
 */
class DepTask
{
    Job* job;
    const std::vector<std::function<void(Job*)>> tasks;
    DAG dependencies;
    ThreadPool& threadPool;
    std::condition_variable cv;
    std::mutex mutex;
public:
    DepTask(const DepTask& other);

    /**
     * @brief creates a super task
     * @param job job
     * @param tasks tasks
     * @param dependencies dependencies between installation operations. The
     *     nodes of the graph are indexes in the "tasks" vector. An edge from "u"
     *     to "v" means that "u" depends on "v".
     * @param threadPool the tasks will be executed here
     */
    DepTask(Job* job, const std::vector<std::function<void(Job*)>>& tasks,
            const DAG& dependencies, ThreadPool& threadPool);

    void operator()();
};

#endif // DEPTASK_H
