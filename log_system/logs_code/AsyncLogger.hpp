#pragma once
#include "Level.hpp"
#include "LogFlush.hpp"
#include "Message.hpp"
#include <cassert>
#include <memory>

namespace mylog{
    class AsyncLogger{
    public:
        AsyncLogger(LogLevel level) : level_(level), formatter_(std::make_unique<Formatter>())
        {
            console_Flush_ = std::make_unique<ConsoleFlush>();
            file_Flush_ = std::make_unique<FileFlush>("app.log");
        }

        // 将日志输出到终端和日志文件
        void log(LogLevel level, const std::string& message)
        {
            if (level >= level_)
            {
                std::string formatted_log = formatter_->format(level, message);
                console_Flush_->flush(formatted_log);
                file_Flush_->flush(formatted_log);
            }
        }

        void debug(const std::string& message)
        {
            log(LogLevel::DEBUG, message);
        }

        void info(const std::string& message)
        {
            log(LogLevel::INFO, message);
        }
        
        void warn(const std::string& message)
        {
            log(LogLevel::WARN, message);
        }

        void error(const std::string& message)
        {
            log(LogLevel::ERROR, message);
        }

        void fatal(const std::string& message)
        {
            log(LogLevel::FATAL, message);
        }
    private:
        LogLevel level_; // 需要记录的日志的最低等级
        std::unique_ptr<Formatter> formatter_;
        std::unique_ptr<Flush> console_Flush_;
        std::unique_ptr<Flush> file_Flush_;
    };// AsyncLogger
}// mylog