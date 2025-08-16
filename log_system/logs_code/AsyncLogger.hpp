#pragma once
#include "Level.hpp"
#include "LogFlush.hpp"
#include "Message.hpp"
#include "backlog/CliBackupLog.hpp"
#include "ThreadPool.hpp"
#include "AsyncWorker.hpp"
#include <cassert>
#include <memory>
#include <atomic>
#include <cstdarg>

extern ThreadPool *tp;

namespace mylog{
    class AsyncLogger{
    public:
        using ptr = std::shared_ptr<AsyncLogger>;
        AsyncLogger(const std::string &logger_name, std::vector<LogFlush::ptr>&flushs, AsyncType type)
            : logger_name_(logger_name), flushs_(flushs.begin(), flushs.end())
        {
            asyncworker = {std::make_shared<AsyncWorker>(
                std::bind(&AsyncLogger::RealFlush, this, std::placeholders::_1),
            type)}; // 就地初始化
        }

        virtual ~AsyncLogger() {};

        std::string Name() {return logger_name_;}

        void Debug(const std::string &file, size_t line, const std::string format, ...)
        {
            va_list va; // 获取可变参数列表中的格式
            va_start(va, format); // 获取format之后的参数列表
            char *ret;
            int r = vasprintf(&ret, format.c_str(), va); // 将参数按照format中的格式转化为字符串
            if (r == -1) perror("vasprintf error: ");
            va_end(va); // 将va指针置空

            serialize(LogLevel::value::DEBUG, file, line, ret);

            free(ret); // 释放vasprintf申请的动态内存
            ret = nullptr;
        }

        void Info(const std::string &file, size_t line, const std::string format, ...)
        {
            va_list va; // 获取可变参数列表中的格式
            va_start(va, format); // 获取format之后的参数列表
            char *ret;
            int r = vasprintf(&ret, format.c_str(), va); // 将参数按照format中的格式转化为字符串
            if (r == -1) perror("vasprintf error: ");
            va_end(va); // 将va指针置空

            serialize(LogLevel::value::INFO, file, line, ret);

            free(ret); // 释放vasprintf申请的动态内存
            ret = nullptr;
        }

        void Warn(const std::string &file, size_t line, const std::string format, ...)
        {
            va_list va; // 获取可变参数列表中的格式
            va_start(va, format); // 获取format之后的参数列表
            char *ret;
            int r = vasprintf(&ret, format.c_str(), va); // 将参数按照format中的格式转化为字符串
            if (r == -1) perror("vasprintf error: ");
            va_end(va); // 将va指针置空

            serialize(LogLevel::value::WARN, file, line, ret);

            free(ret); // 释放vasprintf申请的动态内存
            ret = nullptr;
        }

        void Error(const std::string &file, size_t line, const std::string format, ...)
        {
            va_list va; // 获取可变参数列表中的格式
            va_start(va, format); // 获取format之后的参数列表
            char *ret;
            int r = vasprintf(&ret, format.c_str(), va); // 将参数按照format中的格式转化为字符串
            if (r == -1) perror("vasprintf error: ");
            va_end(va); // 将va指针置空

            serialize(LogLevel::value::ERROR, file, line, ret);

            free(ret); // 释放vasprintf申请的动态内存
            ret = nullptr;
        }

        void Fatal(const std::string &file, size_t line, const std::string format, ...)
        {
            va_list va; // 获取可变参数列表中的格式
            va_start(va, format); // 获取format之后的参数列表
            char *ret;
            int r = vasprintf(&ret, format.c_str(), va); // 将参数按照format中的格式转化为字符串
            if (r == -1) perror("vasprintf error: ");
            va_end(va); // 将va指针置空

            serialize(LogLevel::value::FATAL, file, line, ret);

            free(ret); // 释放vasprintf申请的动态内存
            ret = nullptr;
        }

        // 停止AsyncLogger的工作，释放线程池资源
        void Stop() {
            asyncworker->Stop();
        }
        
    private:
        void serialize(LogLevel::value level, const std::string &file, size_t line, char *ret)
        {
            LogMessage msg(level, file, line, logger_name_, ret); // 初始化日志信息实例
            std::string data = msg.format();    // 将实例信息格式化为字符串
            if (level == LogLevel::value::FATAL || level == LogLevel::value::ERROR)
            {
                try
                {
                    auto ret = tp->enqueue(start_backup, data); // 创建一个线程任务将FATAL和ERROR级别的日志发送到备份服务器
                    ret.get(); // 异步获取任务的返回值
                }
                catch(const std::runtime_error& e) // 捕获线程任务可能抛出的异常, 即在创建任务的过程中线程池关闭了
                {
                    std::cerr << __FILE__ << " " << __LINE__ << " thread pool closed" << std::endl;
                }
            }
            Flush(data.c_str(), data.size()); // 将数据输出到缓冲区
        }

        void Flush(const char* data, size_t len)
        {
            asyncworker->Push(data, len);
        }

        // 异步线程写文件, 作为AsyncWorker的回调函数，每次AsyncWorker线程检测到缓冲区有数据会自动调用该函数
        void RealFlush(Buffer &buffer)
        {
            if (flushs_.empty()) return;

            for (auto &e : flushs_)
            {
                // 将缓冲区中的数据写到e中
                e->Flush(buffer.Begin(), buffer.ReadableSize());
            }
        }

    private:
            std::mutex mtx_;
            std::string logger_name_;
            std::vector<LogFlush::ptr> flushs_; // 存放所有LogFlush的实例指针
            AsyncWorker::ptr asyncworker;
        };// AsyncLogger

    // 用于创建AsyncLogger的工厂
    class LoggerBuilder{
    public:
        using ptr = std::shared_ptr<LoggerBuilder>;
        void BuildLoggerName(const std::string &name) {logger_name_ = name;}
        void BuildLoggerType(AsyncType type) {async_type_ = type;}

        // 各个类型的Flush的构造函数参数列表不同，所以这里要用模板的FlushType指定指针的类型
        template <typename FlushType, typename... Args>
        void BuildLoggerFlush(Args &&...args)
        {
            flushs_.emplace_back(LogFlushFactory::CreateLog<FlushType>(std::forward<Args>(args)...));
        }

        AsyncLogger::ptr Build()
        {
            assert(logger_name_.empty() == false); // 必须有日志器名称

            // 如果没有指定LogFlush，则默认使用标准输出Flush
            if (flushs_.empty())
                flushs_.emplace_back(std::make_shared<StdoutFlush>());
            return std::make_shared<AsyncLogger>(logger_name_, flushs_, async_type_);
        }
    private:
        std::string logger_name_ = "async_logger";
        std::vector<LogFlush::ptr> flushs_;
        AsyncType async_type_ = AsyncType::ASYNC_SAFE;
    };
}// mylog