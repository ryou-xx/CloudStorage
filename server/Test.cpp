// #define DEBUG_LOG
#include "Server.hpp"
#include <thread>
using namespace std;

ThreadPool* tp=nullptr;
void service_module()
{
    storage::Server s;
    mylog::GetLogger("asynclogger")->Info("service step in RunModule");
    s.RunModule();
}

void log_system_module_init()
{
    tp = new ThreadPool(mylog::Util::JsonData::GetJsonData().thread_count);
    std::shared_ptr<mylog::LoggerBuilder> Glb(new mylog::LoggerBuilder());
    Glb->BuildLoggerName("asynclogger");
    Glb->BuildLoggerFlush<mylog::RollFileFlush>("./logfile/RollFile_log",
                                              1024 * 1024);
    mylog::LoggerManager::GetInstance().AddLogger(Glb->Build());
}
int main()
{
    log_system_module_init();
    // thread t1(service_module);
    service_module();
    // t1.join();
    delete(tp);
    return 0;
}