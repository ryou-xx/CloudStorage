#pragma once
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool{
public:
    ThreadPool(size_t threads) : stop(false)
    {
        for (size_t i = 0; i < threads; i++)
        {
            // 线程程序：等待任务队列不为空，从任务队列中取出任务并执行
            workers.emplace_back(
                [this]
                {
                    for(;;)
                    {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(queue_mutex); //加锁
                            //等待任务队列不为空或线程池停止
                            condition.wait(lock,
                                            [this]{return stop || !tasks.empty();});
                            if (stop && tasks.empty()) return; // 如果线程池关闭且任务队列为空，直接返回
                            //取出任务
                            task = std::move(tasks.front()); //移动语义优化性能
                            tasks.pop();
                        } // 自动解锁
                        task();
                    }
                }
            );
        }
    }

    template <typename F, typename... Args>
    auto enqueue(F &&f, Args &&...args)
        -> std::future<typename std::invoke_result_t<F,Args...>> // 自动类型推导
    {
        using return_type = typename std::invoke_result_t<F,Args...>;
        // 借助packeaged_task、future和lamda把任务转换为void()函数
        // 使用智能指针确保packaged_task的生命周期
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        
            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex);

                // 异常机制
                if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");

                tasks.emplace([task](){(*task)();});
            }
            condition.notify_one();
            return res; //返回task绑定的future实例，用于异步获取task结果
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &work : workers) work.join(); // 回收线程
    }
private:
    std::vector<std::thread> workers;           // 用于保存线程
    std::queue<std::function<void()>> tasks;    // 任务队列
    std::mutex queue_mutex;                     // 任务队列和stop互斥锁
    std::condition_variable condition;          // 用于任务队列同步
    bool stop;
}; // class ThreadPool