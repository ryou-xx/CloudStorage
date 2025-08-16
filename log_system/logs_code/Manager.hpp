#pragma once
#include <unordered_map>
#include "AsyncLogger.hpp"

namespace mylog{
    class LoggerManager{
    public:
        // 确保只有一个Manager
        static LoggerManager &GetInstance()
        {
            static LoggerManager eton;
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

        void AddLogger(const AsyncLogger::ptr &AsyncLogger)
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

        void StopAll()
        {
            std::unique_lock<std::mutex> lock(mtx_);
            for (auto& pair : loggers_) {
                pair.second->Stop(); // 调用每个日志器的Stop方法
            }
        }

        AsyncLogger::ptr DefaultLogger() { return default_logger_;}

        // 确保单例
        LoggerManager(const LoggerManager&) = delete;
        LoggerManager(const LoggerManager&&) = delete;
        LoggerManager& operator=(LoggerManager&) = delete;
        LoggerManager& operator=(LoggerManager&&) = delete;
    private:
        // 新建一个Manager并初始化一个默认日志器
        LoggerManager()
        {
            std::unique_ptr<LoggerBuilder> builder(new LoggerBuilder());
            builder->BuildLoggerName("default");
            default_logger_ = builder->Build(); // 默认日志器，仅使用标准输出
            loggers_["default"] = default_logger_;
        }
        ~LoggerManager() = default;
    private:
        std::mutex mtx_;
        AsyncLogger::ptr default_logger_;
        std::unordered_map<std::string, AsyncLogger::ptr> loggers_;
    };
}