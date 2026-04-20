#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <QDebug>

using std::vector;
using std::mutex;
using std::thread;
using std::queue;
using std::condition_variable;
using std::atomic;

class ThreadPool
{
private:

    vector<thread> workers;
    queue<std::function<void()>> tasks;
    mutable mutex queue_mutex;
    condition_variable condition;
    atomic<bool> stop;
    atomic<size_t> active_tasks;

public:

    explicit ThreadPool(size_t threads = thread::hardware_concurrency());

    ~ThreadPool();

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;

    size_t pendingTasks() const;

    bool idle() const;

    void waitForAll();
};

template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>{

    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();

    {
        std::unique_lock<mutex> lock(queue_mutex);

        if(stop){

            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        tasks.emplace([task](){

            (*task)();
        });

    }

    condition.notify_one();

    return result;
}

#endif // THREADPOOL_H
