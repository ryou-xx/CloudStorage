#pragma once
#include "Level.hpp"
#include "LogFlush.hpp"
#include "Message.hpp"
#include "backlog/CliBackupLog.hpp"
#include "ThreadPoll.hpp"
#include <cassert>
#include <memory>

extern ThreadPoll *tp;

namespace mylog{
    class AsyncLogger{
    public:
        using ptr = std::shared_ptr<AsyncLogger>;
        AsyncLogger(const std::string &logger_name, std::vector<LogFlush::ptr>&flushs,){}
    protected:
        std::mutex mtx_;
        std::string logger_name_;
        std::vector<LogFlush::ptr> flushs_;
        mylog::AsyncWorker::ptr asyncworker;
    };// AsyncLogger
}// mylog