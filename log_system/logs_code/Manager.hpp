#pragma once
#include <unordered_map>
#include "AsyncLogger.hpp"

namespace mylog{
    class LoggerManeger{
    public:
        // 确保只有一个Manager
        static LoggerManeger &GetInstance()
        {
            static LoggerManeger eton;
            return eton;
        }
    
        // 查看名称为name的日志器是否存在
        bool LoggerExist(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(mtx_);
            auto it = loggers_.find(name);
            if (it == loggers_.end()) return false;
            return true;
        }

        void AddLogger(const AsyncLogger::ptr &&AsyncLogger)
        {
            if (LoggerExist(AsyncLogger->Name())) return;

            std::unique_lock<std::mutex> lock(mtx_);
            loggers_[AsyncLogger->Name()] = AsyncLogger;
        }

        AsyncLogger::ptr GetLogger(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(mtx_);
            auto it = loggers_.find(name);
            if (it == loggers_.end()) return nullptr;
            return it->second;
        }

        AsyncLogger::ptr DefaultLogger() { return default_logger_;}

    private:
        // 新建一个Manager并初始化一个默认日志器
        LoggerManeger()
        {
            std::unique_ptr<LoggerBuilder> builder(new LoggerBuilder());
            builder->BuildLoggerName("default");
            default_logger_ = builder->Build(); // 默认日志器，仅使用标准输出
            loggers_["default"] = default_logger_;
        }
    private:
        std::mutex mtx_;
        AsyncLogger::ptr default_logger_;
        std::unordered_map<std::string, AsyncLogger::ptr> loggers_;
    };
}