#include "threadpool.h"

using std::move;

ThreadPool::ThreadPool(size_t threads) : stop(false), active_tasks(0)
{
    // 如果线程数为0，使用CPU核心数
    if(threads == 0){

        threads = thread::hardware_concurrency();
        if(threads == 0) threads = 2; // 保底值
    }

    // 创建工作线程
    for(size_t i = 0; i < threads; i++){

        workers.emplace_back([this]{

            while(1){

                function<void()> task;

                {
                    unique_lock<mutex> lock(this->queue_mutex);

                    //等待任务队列非空,醒来条件：要么关门，要么有活干
                    this->condition.wait(lock, [this]{

                        return this->stop || !this->tasks.empty();
                    });

                    // 如果停止且没有任务，退出线程
                    if(this->stop && this->tasks.empty()) {

                        return;
                    }

                    // 取出一个任务
                    task = move(this->tasks.front());
                    this->tasks.pop();
                }

                //执行任务
                active_tasks++;
                task();
                active_tasks--;

            }
        });
    }
}

ThreadPool::~ThreadPool()
{
    // 等待所有任务完成
    waitForAll();

    // 设置停止标志
    stop = true;

    // 唤醒所有等待的线程
    condition.notify_all();

    //等待所有线程结束
    for(thread& worker : workers){

        if(worker.joinable()){

            worker.join();
        }
    }

}

mutex &ThreadPool::getMutex()
{
    return queue_mutex;
}

size_t ThreadPool::pendingTasks() const{

    std::lock_guard<mutex> lock(queue_mutex);
    return tasks.size();
}

bool ThreadPool::idle() const{

    std::lock_guard<mutex> lock(queue_mutex);
    return tasks.empty() && active_tasks == 0;
}

void ThreadPool::waitForAll(){

    while(!idle()){

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
