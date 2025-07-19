#pragma once
#include <string>
#include "Level.hpp"
#include <ctime>

namespace mylog{

class Formatter{
public:
    // 返回"[当前时间] + [日志级别] + messsage"的字符串
    std::string format(LogLevel level, const std::string &message)
    {
        std::time_t now = std::time(nullptr); // 返回当前时间
        std::string time_str = std::ctime(&now); // 将秒数转换为世界时间字符串
        time_str.pop_back(); // 去掉换行符

        std::string level_str;
        switch(level)
        {
            case LogLevel::DEBUG: level_str = "DEBUG"; break;
            case LogLevel::INFO: level_str = "INFO"; break;
            case LogLevel::WARN: level_str = "WARN"; break;
            case LogLevel::ERROR: level_str = "ERROR"; break;
            case LogLevel::FATAL: level_str = "FATAL"; break;
        }

        return "[" + time_str + "] [" + level_str + "] " + message;
    }
}; // Formatter

} // mylog
