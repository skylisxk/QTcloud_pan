#include "threadpool.h"
#include <QDebug>



ThreadPool::ThreadPool(size_t threads) : stop(false), active_tasks(0)
{
    // 如果线程数为0，使用CPU核心数
    size_t actual_threads = threads;
    if(actual_threads == 0) {
        actual_threads = std::thread::hardware_concurrency();
        if(actual_threads == 0) {
            actual_threads = 2;  // 保底值
        }
    }

    for(size_t i = 0; i < actual_threads; i++){

        workers.emplace_back([this]{

            while(1){

                std::function<void()> task;

                {
                    std::unique_lock<mutex> lock(this->queue_mutex);

                    this->condition.wait(lock, [this]{

                        return this->stop.load() || !this->tasks.empty();
                    });

                    if(this->stop.load() && this->tasks.empty()){

                        return;
                    }

                    task = std::move(this->tasks.front());
                    tasks.pop();
                }

                active_tasks++;
                task();
                active_tasks--;
            }
        });
    }
}

ThreadPool::~ThreadPool(){

    waitForAll();

    stop = true;

    condition.notify_all();

    for(thread& worker : workers){

        if(worker.joinable()){

            worker.join();
        }
    }
}



size_t ThreadPool::pendingTasks() const
{
    std::lock_guard<mutex> lock(queue_mutex);
    return tasks.size();
}



bool ThreadPool::idle() const
{
    std::lock_guard<mutex> lock(queue_mutex);
    return tasks.empty() && active_tasks.load() == 0;
}

void ThreadPool::waitForAll()
{
    while(!idle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
