// --- START OF FILE test.cpp ---
#define LOG_DEBUG 1

#include "MyLog.hpp"
#include "ThreadPool.hpp"
#include "Util.hpp"
#include "Manager.hpp"
#include <iostream>
#include <chrono>

ThreadPool* tp = nullptr;
const long long TOTAL_LOG = 100000;

void test() {
    int cur_size = 0;
    int cnt = 1;
    while (cur_size++ < TOTAL_LOG) {
        mylog::GetLogger("asynclogger")->Info("测试日志-%d", cnt++);
        mylog::GetLogger("asynclogger")->Warn("测试日志-%d", cnt++);
        mylog::GetLogger("asynclogger")->Debug("测试日志-%d", cnt++);
        // mylog::GetLogger("asynclogger")->Error("测试日志-%d", cnt++);
        // mylog::GetLogger("asynclogger")->Fatal("测试日志-%d", cnt++);
    }
}

void init_thread_pool() {
    tp = new ThreadPool(mylog::Util::JsonData::GetJsonData().thread_count);
}

int main() {
    init_thread_pool();
    std::shared_ptr<mylog::LoggerBuilder> Glb(new mylog::LoggerBuilder());
    Glb->BuildLoggerName("asynclogger");
    Glb->BuildLoggerFlush<mylog::FileFlush>("../logfile/FileFlush.log");
    Glb->BuildLoggerFlush<mylog::RollFileFlush>("../logfile/RollFile_log", 1024 * 1024);
    mylog::LoggerManager::GetInstance().AddLogger(Glb->Build());
    std::cout << "Starting test..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();

    test();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << "Log calls finished in " << elapsed.count() << " seconds." << std::endl;

    std::cout << "Flushing all logs..." << std::endl;
    auto flush_start = std::chrono::high_resolution_clock::now();
    
    mylog::StopAllLoggers(); // 等待所有日志被写入磁盘

    auto flush_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> flush_elapsed = flush_end - flush_start;
    std::cout << "All logs flushed in " << flush_elapsed.count() << " seconds." << std::endl;

    elapsed = flush_end - start_time;
    double lps = TOTAL_LOG * 6 / elapsed.count();
    std::cout << "Total logs: " << TOTAL_LOG << std::endl;
    std::cout << "Logs per second (LPS): " << lps << std::endl;

    delete(tp);
    std::cout << "Test finished." << std::endl;
    return 0;
}