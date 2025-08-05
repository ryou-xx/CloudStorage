#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <memory>
#include <unistd.h>
#include <cassert>
#include "Util.hpp"

namespace mylog{
    // 日志输出器
    class LogFlush{
    public:
        using ptr = std::shared_ptr<LogFlush>;
        virtual void Flush(const char *data, size_t len) = 0;
        virtual ~LogFlush() = default;
    }; // Flush

    // 控制台输出器
    class StdoutFlush : public LogFlush{
    public:
        using ptr = std::shared_ptr<StdoutFlush>;
        void Flush(const char *data, size_t len) override
        {
            cout.write(data, len);
        }
    }; // ConsoleFlush

    // 文件输出器
    class FileFlush : public LogFlush{
    public:
        using ptr = std::shared_ptr<FileFlush>;
        FileFlush(const std::string& filename) : filename_(filename)
        {
            Util::File::CreateDirectory(Util::File::Path(filename)); // 如果指定文件路径不存在会先创建文件路径
            fs_ = fopen(filename_.c_str(), "ab");
            if (fs_ == nullptr)
            {
                std::cerr << __FILE__ << " " <<  __LINE__ << " open log file failed: ";
                perror(nullptr);
            }
        }
        
        void Flush(const char* data, size_t len) override
        {
            fwrite(data, 1, len, fs_);
            if (ferror(fs_))
            {
                std::cerr << __FILE__ << " " << __LINE__ << " write log file failed: ";
                perror(nullptr);
            }
            if (Util::JsonData::GetJsonData().flush_log == 1)
            {
                if (fflush(fs_) == EOF)
                {
                    std::cerr << __FILE__ << " " << __LINE__ << " fflush file failed: ";
                    perror(nullptr);
                }
            }
            else if (Util::JsonData::GetJsonData().flush_log == 2)
            {
                fflush(fs_);
                fsync(fileno(fs_));
            }
        }
    private:
        std::string filename_;
        FILE* fs_ = nullptr;
    }; // FileFulsh

    class RollFileFlush : public LogFlush{
    public:
        using ptr = std::shared_ptr<RollFileFlush>;
        RollFileFlush(const std::string &filename, size_t max_size)
            : max_size_(max_size), basename_(filename)
        {
            Util::File::CreateDirectory(Util::File::Path(filename));
        }

        void Flush(const char *data, size_t len) override
        {
            InitLogFile();

            fwrite(data, 1, len, fs_);
            if (ferror(fs_))
            {
                std::cerr << __FILE__ << " " << __LINE__ << " write log file failed: ";
                perror(nullptr);
            }
            cur_size_ += len;
            if(fflush(fs_) == EOF)
            {
                std::cerr << __FILE__ << " " << __LINE__ << " fflush error: ";
                perror(nullptr);
            }
            if (Util::JsonData::GetJsonData().flush_log == 2)
            {
                if (fsync(fileno(fs_)) != 0)
                {
                    std::cerr << __FILE__ << " " << __LINE__ << " fsync error: ";
                    perror(nullptr);
                }
            }
        }
    
    private:
        void InitLogFile()
        {
            if (fs_ == nullptr || cur_size_ >= max_size_)
            {
                if(fs_ != nullptr)
                {
                    fclose(fs_);
                    fs_ = nullptr;
                }
                std::string filename = CreateFilename();
                fs_=fopen(filename.c_str(), "ab");
                if (fs_ == nullptr)
                {
                    std::cerr << __FILE__ << " " <<__LINE__ << " open file failed: ";
                    perror(nullptr);
                }
                cur_size_ = 0;
            }
        }
        // 构建落地的滚动日志文件名称
        std::string CreateFilename()
        {
            time_t time_ = Util::Date::Now();
            struct tm t;
            localtime_r(&time_, &t);
            std::string filename = basename_;
            filename += "_" + std::to_string(t.tm_year + 1900);
            filename += std::to_string(t.tm_mon + 1);
            filename += std::to_string(t.tm_mday);
            filename += std::to_string(t.tm_hour);
            filename += std::to_string(t.tm_min);
            filename += std::to_string(t.tm_sec);
            filename += '-' + std::to_string(++cnt_) + ".log";
            return filename;
        }
    
    private:
        size_t cnt_ = 0;
        size_t cur_size_ = 0;
        size_t max_size_;
        std::string basename_;
        FILE *fs_ = nullptr;
    }; // RollFileFulsh

    class LogFlushFactory
    {
    public:
        using ptr = std::shared_ptr<LogFlushFactory>;
        template <typename FlushType, typename... Args>
        static std::shared_ptr<LogFlush> CreateLog(Args &&...args)
        {
            return std::make_shared<FlushType>(std::forward<Args>(args)...);
        }
    };
} // mylog