/* threadpool.h
 * Probably not optimal but it's prescise and fast enough
*/

#ifndef THREADPOOL_GUARD
#define THREADPOOL_GUARD

#include <iostream>
#include <vector>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <chrono>
#include <list>
#include <functional>

class nofetch_threadpool {
private:
    std::atomic<bool> thread_flag;
    std::vector<std::thread> threads;
    std::list<std::function<void()>> tasks;

    std::condition_variable cv;
    std::mutex cv_m;

public:
    nofetch_threadpool(size_t thread_count) {
        thread_flag.store(true);

        while (thread_count) {
            threads.push_back(std::thread(&nofetch_threadpool::worker, this));
            thread_count--;
        }
    }

    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(cv_m);
            thread_flag.store(false);
        }
        
        cv.notify_all();

        for (auto& thread : threads) {
            if (thread.joinable())
                thread.join();
        }
    }

    template <typename Func,typename... Args>
    void exec(Func func, Args... args) {
        std::function<void()> task = [this, args..., func]() { func(args...); };
        {
            std::unique_lock<std::mutex> lock(cv_m);
            tasks.push_back(task);
        }
        cv.notify_one();
    }

private:
    void worker() {
        std::unique_lock<std::mutex> lock(cv_m);
        while (thread_flag.load()) {
            std::function<void()> task;
            cv.wait(lock, [this]() { return !tasks.empty() || !thread_flag.load(); });
            if (!thread_flag.load() && tasks.empty()) { return ;}

            {
                if (!tasks.empty()) {
                    task = tasks.front();
                    tasks.pop_front();
                }
            }

            task();
        }
    }
};

#endif