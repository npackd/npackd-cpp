#include <windows.h>

#include "threadpool.h"
#include "wpmutils.h"

ThreadPool::ThreadPool(const int n, const int priority):
    maxThreads(n), priority(priority)
{
}

ThreadPool::~ThreadPool()
{
    clearThreads();
}

void ThreadPool::clearThreads()
{
    {
        std::lock_guard<std::mutex> lock{mutex};
        done = true;
    }
    cond.notify_all();

    try {
        for (auto& thr: threads) {
            thr.join();
        }
    } catch (const std::system_error& e) {
        qCWarning(npackd) << "ThreadPool::clearThreads" << e.what();
    }

    {
        std::lock_guard<std::mutex> lock{mutex};
        threads.clear();
        done = false;
    }
}

void ThreadPool::addTask(std::function<void()> &&task)
{
    {
        std::lock_guard<std::mutex> lock{mutex};

        tasks.push(task);

        if (runningTasks + static_cast<int>(tasks.size()) >
            static_cast<int>(threads.size())) {
            if (static_cast<int>(threads.size()) < maxThreads) {
                this->threads.emplace_back(&ThreadPool::process, this);
            }
        }
    }
    cond.notify_one();
}

void ThreadPool::process() {
    SetThreadPriority(GetCurrentThread(), priority);

    while (true) {
        std::function<void()> f;
        {
            std::unique_lock<std::mutex> lock{mutex};

            cond.wait(lock, [this] {
                return done || !tasks.empty();
            });

            if (done)
                break;

            f = tasks.front();
            tasks.pop();

            runningTasks++;
        }

        f();

        {
            std::unique_lock<std::mutex> lock{mutex};
            runningTasks--;
        }
    }
}
