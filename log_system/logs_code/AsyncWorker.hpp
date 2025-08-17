#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <memory>
#include "AsyncBuffer.hpp"


namespace mylog{
enum class AsyncType{ ASYNC_SAFE, ASYNC_UNSAFE};

using functior = std::function<void(Buffer&)>;

class AsyncWorker{
public:
    using ptr = std::shared_ptr<AsyncWorker>;
    AsyncWorker(const functior & cb, AsyncType async_type = AsyncType::ASYNC_SAFE)
        : async_type_(async_type), callback_(cb), stop_(false), thread_(std::thread(&AsyncWorker::ThreadEntry, this))
    {}
    ~AsyncWorker() { Stop();}

    void Push(const char* data, size_t len)
    {
        // 安全模式下，缓冲区大小固定，如果生产者队列空间不足，则会阻塞等待
        std::unique_lock<std::mutex> lock(mtx_);
        if (stop_) return;
        if (AsyncType::ASYNC_SAFE == async_type_)
        {
            cond_productor_.wait(lock, [&](){return len <= buffer_productor_.WriteableSize();});
        }
        buffer_productor_.Push(data, len); // 这里的Push是Buffer的Push，不是AsyncWorker的Push
        cond_consumer_.notify_one();
    }

    void Stop()
    {
        stop_ = true;
        cond_consumer_.notify_all();
        if(thread_.joinable()) thread_.join(); // 等待后台线程执行完毕
    }
private:
    void ThreadEntry()
    {
        while(true)
        {
            // 仅在缓冲区交换的时候锁住缓冲区，避免在处理缓冲区数据的时候锁住生产者缓冲区导致生产者无法正常生产
            {
                std::unique_lock<std::mutex> lock(mtx_);

                //阻塞等待productor产生数据，该语句在等待条件变量时会先判断谓词的返回值，所以不需要if或while进行手动判断
                cond_consumer_.wait(lock, [&](){return stop_ || !buffer_productor_.IsEmpty();});

                // 只要生产者缓冲区有数据，就进行交换处理
                if (!buffer_productor_.IsEmpty()) {
                    buffer_productor_.Swap(buffer_consumer_);
                    if (async_type_ == AsyncType::ASYNC_SAFE) cond_productor_.notify_one();
                }
                
                // 退出条件：收到停止信号，并且生产者缓冲区已经空了
                // stop_被设置后，不允许再有生产者数据进入
                if (stop_ && buffer_productor_.IsEmpty()) {
                    // 确保消费者缓冲区最后的数据也被处理
                    if (!buffer_consumer_.IsEmpty()){
                        callback_(buffer_consumer_);
                        buffer_consumer_.Reset();
                    }
                    return; 
                }
            }

            callback_(buffer_consumer_); // 处理读缓冲区中的数据
            buffer_consumer_.Reset();
        }
    }

private:
    AsyncType async_type_;
    std::atomic<bool> stop_; // 用于控制异步工作器的启动
    std::mutex mtx_;
    mylog::Buffer buffer_productor_;
    mylog::Buffer buffer_consumer_;
    std::condition_variable cond_productor_;
    std::condition_variable cond_consumer_;
    std::thread thread_;
    functior callback_;
};// AsyncWorker

}