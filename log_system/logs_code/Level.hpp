#pragma once

namespace mylog{
    class LogLevel{
    public:
        enum class value{
            DEBUG,  // 用于开发调试阶段， 记录详细的调试信息
            INFO,   // 记录程序正常运行时的关键信息
            WARN,   // 表示程序出现了潜在问题， 但不影响正常运行
            ERROR,  //  记录程序发生的错误，导致部分功能无法正常运行
            FATAL   //  表示发生了严重错误，程序无法继续运行
        };

        static const char* ToString(value level)
        {
            switch(level)
            {
                case value::DEBUG:
                    return "DEBUG";
                case value::INFO:
                    return "INFO";
                case value::WARN:
                    return "WARN";
                case value::ERROR:
                    return "ERROR";
                case value::FATAL:
                    return "FATAL";
                default:
                    return "UNKNOW";
            }
        }
    }; // LogLevel
} // mylog