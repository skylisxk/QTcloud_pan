#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>

using std::vector;
using std::thread;
using std::mutex;
using std::queue;
using std::unique_lock;
using std::condition_variable;
using std::future;
using std::function;

class ThreadPool
{

private:

    //线程列表
    vector<thread> workers;

    //任务队列
    queue<function<void()>> tasks;

    //互斥锁
    mutable mutex queue_mutex;

    //条件变量用来同步
    condition_variable condition;

    //停止标志
    std::atomic<bool> stop;

    //当前活动的任务数
    std::atomic<size_t> active_tasks;

public:

    //创建指定数量的线程数,默认是电脑的线程数
    explicit ThreadPool(size_t threads = thread::hardware_concurrency());

    // 析构函数：等待所有任务完成并清理线程
    ~ThreadPool();

    // 获取互斥锁（用于外部同步）
    mutex& getMutex();

    // 添加任务到线程池
    // 可以接受任何可调用对象和参数，返回一个future用于获取结果
    //这个箭头后面是返回值类型，表示函数返回一个 future 对象。
    //std::result_of 是 C++11 的类型萃取工具，用于获取函数/可调用对象的返回值类型。
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> future<typename std::invoke_result<F, Args...>::type>;

    // 获取当前任务队列大小tasks
    size_t pendingTasks() const;

    //是否空闲
    bool idle() const;

    //等待所有任务完成
    void waitForAll();
};

// 模板函数需要在头文件中实现
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> future<typename std::invoke_result<F, Args...>::type>{

    //using 定义的是类型别名（类型名）
    using return_type = typename std::invoke_result<F, Args...>::type;

    //创建任务包装器,bind作用：把函数和它的参数"粘"在一起，变成一个无参数的函数对象。
    //packaged_task<return_type()> - 打包任务
    //makeshare作用：把任务放到智能指针里，让多个地方可以共享同一个任务
    //std::forward 保持参数的"左右值属性"，实现完美转发。
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args...>(args)...)
        );

    // 调用者以后可以用 result.get() 等待结果
    future<return_type> result = task->get_future();

    {
        unique_lock<mutex> lock(queue_mutex);

        if(stop){

            throw std::runtime_error("error");
        }

        //任务添加到对列
        // 为什么要多包一层？
        // 因为 tasks 队列里存的是 std::function<void()>
        // 所有任务必须是无参数无返回值的函数
        // 所以把 packaged_task 包进一个 lambda 里
        tasks.emplace([task](){
            (*task)();
        });
    }

    // 通知一个等待的线程
    condition.notify_one();

    return result;

}

#endif // THREADPOOL_H
