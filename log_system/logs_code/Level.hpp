#pragma once

namespace mylog{
    enum class LogLevel{
        DEBUG,  // 用于开发调试阶段， 记录详细的调试信息
        INFO,   // 记录程序正常运行时的关键信息
        WARN,   // 表示程序出现了潜在问题， 但不影响正常运行
        ERROR,  //  记录程序发生的错误，导致部分功能无法正常运行
        FATAL   //  表示发生了严重错误，程序无法继续运行
    }; // LogLevel
} // mylog