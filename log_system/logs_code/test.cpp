// --- START OF FILE test.cpp ---
#define LOG_DEBUG 1

#include "MyLog.hpp"
#include "ThreadPool.hpp"
#include "Util.hpp"
#include "Manager.hpp"
#include <iostream>
#include <chrono>
#include <filesystem> 
#include <string>     

namespace fs = std::filesystem;

ThreadPool* tp = nullptr;
const long long TOTAL_LOG = 200000;

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

    const std::string log_dir = "../logfile";
    if (fs::exists(log_dir)) {
        fs::remove_all(log_dir);
    }
    fs::create_directories(log_dir);

    std::shared_ptr<mylog::LoggerBuilder> Glb(new mylog::LoggerBuilder());
    Glb->BuildLoggerName("asynclogger");
    const std::string file_flush_path_1 = log_dir + "/FileFlush_1.log";
    const std::string file_flush_path_2= log_dir + "/FileFlush_2.log";
    const std::string file_flush_path_3= log_dir + "/FileFlush_3.log";
    const std::string file_flush_path_4= log_dir + "/FileFlush_4.log";
    const std::string file_flush_path_5= log_dir + "/FileFlush_5.log";
    const std::string file_flush_path_6= log_dir + "/FileFlush_6.log";
    const std::string file_flush_path_7= log_dir + "/FileFlush_7.log";
    const std::string file_flush_path_8= log_dir + "/FileFlush_8.log";
    const std::string file_flush_path_9= log_dir + "/FileFlush_9.log";
    const std::string file_flush_path_10= log_dir + "/FileFlush_10.log";
    const std::string file_flush_path_11= log_dir + "/FileFlush_11.log";
    const std::string file_flush_path_12= log_dir + "/FileFlush_12.log";
    const std::string file_flush_path_13= log_dir + "/FileFlush_13.log";
    const std::string file_flush_path_14= log_dir + "/FileFlush_14.log";
    const std::string file_flush_path_15= log_dir + "/FileFlush_15.log";
    const std::string file_flush_path_16= log_dir + "/FileFlush_16.log";
    const std::string file_flush_path_17= log_dir + "/FileFlush_17.log";
    const std::string file_flush_path_18= log_dir + "/FileFlush_18.log";
    const std::string file_flush_path_19= log_dir + "/FileFlush_19.log";
    const std::string file_flush_path_20= log_dir + "/FileFlush_20.log";
    const std::string roll_file_base_path = log_dir + "/RollFile_log";
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_1);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_2);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_3);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_4);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_5);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_6);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_7);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_8);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_9);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_10);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_11);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_12);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_13);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_14);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_15);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_16);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_17);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_18);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_19);
    Glb->BuildLoggerFlush<mylog::FileFlush>(file_flush_path_20);
    Glb->BuildLoggerFlush<mylog::RollFileFlush>(roll_file_base_path, 1024 * 1024);
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

    long long total_bytes_written = 0;
    try {
        // 遍历 ../logfile 目录下的所有文件
        for (const auto& entry : fs::directory_iterator(log_dir)) {
            if (entry.is_regular_file()) {
                total_bytes_written += entry.file_size();
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error accessing log directory: " << e.what() << std::endl;
    }
    
    elapsed = flush_end - start_time;
    long long total_logical_logs = TOTAL_LOG * 3; // 3个日志调用
    double lps = total_logical_logs / elapsed.count();
    double write_rate_mbps = (double)total_bytes_written / (1024 * 1024) / elapsed.count();

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Benchmark Results:" << std::endl;
    std::cout << "Total logical logs: " << total_logical_logs << std::endl;
    std::cout << "Total time elapsed: " << elapsed.count() << " seconds." << std::endl;
    std::cout << "Logs per second (LPS): " << static_cast<long long>(lps) << std::endl;
    std::cout << "Total data written: " << (double)total_bytes_written / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Write throughput: " << write_rate_mbps << " MB/s" << std::endl;
    std::cout << "----------------------------------------" << std::endl;


    delete(tp);
    std::cout << "Test finished." << std::endl;
    return 0;
}