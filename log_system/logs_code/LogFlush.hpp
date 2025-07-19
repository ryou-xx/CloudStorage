#pragma once
#include <string>
#include <iostream>
#include <fstream>

namespace mylog{
    // 日志输出器
    class Flush{
    public:
        virtual void flush(const std::string& formatted_log) = 0;
        virtual ~Flush() = default;
    }; // Flush

    // 控制台输出器
    class ConsoleFlush : public Flush{
    public:
        void flush(const std::string& formatted_log) override
        {
            std::cout << formatted_log << std::endl;
        }
    }; // ConsoleFlush

    // 文件输出器
    class FileFlush : public Flush{
    public:
        FileFlush(const std::string& filename) : file_(filename, std::ios::app){}
        
        void flush(const std::string& formatted_log) override
        {
            if (file_.is_open()) file_ << formatted_log << std::endl;
        }

        ~FileFlush()
        {
            if (file_.is_open()) file_.close();
        }
    private:
        std::ofstream file_;
    }; // FileFulsh
} // mylog