#pragma once
#include "Manager.hpp"

namespace mylog{
AsyncLogger::ptr GetLogger(const std::string &name)
{
    return LoggerManager::GetInstance().GetLogger(name);
}

AsyncLogger::ptr DefaultLogger()
{
    return LoggerManager::GetInstance().DefaultLogger();
}

// 简化函数使用，在使用时需要指定日志器
#define Debug(fmt, ...) Debug(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...) Info(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warn(fmt, ...) Warn(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Error(fmt, ...) Error(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Fatal(fmt, ...) Fatal(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

// 使用默认日志器输出
#define LOGDEBUGDEFAULT(fmt, ...) mylog::DefaultLogger()->Debug(fmt, ##__VA_ARGS__)
#define LOGINFODEFAULT(fmt, ...) mylog::DefaultLogger()->Info(fmt, ##__VA_ARGS__)
#define LOGWARNDEFAULT(fmt, ...) mylog::DefaultLogger()->Warn(fmt, ##__VA_ARGS__)
#define LOGERRORDEFAULT(fmt, ...) mylog::DefaultLogger()->Error(fmt, ##__VA_ARGS__)
#define LOGFATALDEFAULT(fmt, ...) mylog::DefaultLogger()->Fatal(fmt, ##__VA_ARGS__)

// 停止所有日志器
void StopAllLoggers()
{
    LoggerManager::GetInstance().StopAll();
}
}