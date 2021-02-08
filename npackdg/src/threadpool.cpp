#include <windows.h>

#include "threadpool.h"

ThreadPool::ThreadPool(const int n, const int priority): priority(priority)
{
    for (int i = 0; i < n; ++i) {
        this->threads.emplace_back(&ThreadPool::process, this);
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock{mutex};
        done = true;
    }
    cond.notify_all();

    for (auto& thr: threads) {
        thr.join();
    }
}

void ThreadPool::addTask(std::function<void()> &&task)
{
    {
        std::lock_guard<std::mutex> lock{mutex};
        tasks.push(task);
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
        }
        f();
    }
}
