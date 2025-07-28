#pragma once
#include <cassert>
#include <sstream>
#include <memory>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include "bundle.h"
#include "Config.hpp"
#include "jsoncpp/json/json.h"
#include "../log_system/logs_code/MyLog.hpp"

namespace storage{
    namespace fs = std::filesystem;

    static unsigned char ToHex(unsigned char x)
    {
        return x > 9 ? x + 55 : x + 48;
    }

    static unsigned char FromHex(unsigned char x)
    {
        unsigned char y;
        if (x >= 'A' && x <= 'F')
            y = x - 'A' + 10;
        else if (x >= 'a' && x <= 'f')
            y = x - 'a' + 10;
        else if (x >= '0' && x <= '9')
            y = x - '0';
        else
            std::cerr << "Hex string format error" << std::endl;
            assert(0);
        return y; 
    }

    static std::string UrlDecode(const std::string &str)
    {
        std::string strTemp = "";
        size_t length = str.size();
        for (size_t i = 0; i < length; i++)
        {
            if (str[i] == '+') strTemp += " ";
            if (str[i] == '%')
            {
                assert(i + 2 < length);
                unsigned char hight = FromHex((unsigned char)str[++i]);
                unsigned char low = FromHex((unsigned char)str[++i]);
                strTemp += hight * 16 + low;
            }
            else
                strTemp += str[i];
        }
        return strTemp;
    }

    class FileUtil{
    private:
        std::string filename_;

    public:
        FileUtil(const std::string filename) : filename_(filename){}

        // 获取文件大小
        int64_t FileSize()
        {
            struct stat st;
            if (-1 == stat(filename_.c_str(), &st))
            {
                mylog::GetLogger("asynclogger")->Info("%s, Get file size failed: %s", filename_.c_str(), strerror(errno));
                return -1;
            }
            return st.st_size;
        }

        // 获取文件最近访问时间
        time_t LastAccessTime()
        {
            struct stat st;
            if (-1 == stat(filename_.c_str(), &st))
            {
                mylog::GetLogger("asynclogger")->Info("%s, Get file size failed: %s", filename_.c_str(), strerror(errno));
                return -1;
            }
            return st.st_atime;
        }

        // 获取文件最近的修改时间
        time_t LastMidifyTime()
        {
            struct stat st;
            if (-1 == stat(filename_.c_str(), &st))
            {
                mylog::GetLogger("asynclogger")->Info("%s, Get file size failed: %s", filename_.c_str(), strerror(errno));
                return -1;
            }
            return st.st_mtime;
        }

        // 获取文件名
        std::string FileName()
        {
            auto pos = filename_.find_last_of('/');
            if (pos == std::string::npos) return filename_;
            return filename_.substr(pos + 1);
        }

        // 从文件POS开始获取len长度字符串给content
        bool GetPosLen(std::string *content, size_t pos, size_t len)
        {
            if (pos + len > FileSize())
            {
                mylog::GetLogger("asynclogger")->Info("%s, The data required exceeds the file size",filename_.c_str());
                return false;
            }

            std::ifstream ifs;
            ifs.open(filename_, std::ios::binary);
            if (ifs.is_open() == false)
            {
                mylog::GetLogger("asynclogger")->Info("%s, file open error", filename_.c_str());
                return false;
            }

            ifs.seekg(pos, std::ios::beg); // 将文件读指针调整到文件开始位置+pos
            content->resize(len);
            ifs.read(&(*content)[0], len);
            if (!ifs.good())
            {
                mylog::GetLogger("asynclogger")->Info("%s, read file content error", filename_.c_str());
                return false;
            }
            ifs.close();
            return true;
        }

        // 获取文件全部内容
        bool GetContent(std::string *content)
        {
            return GetPosLen(content, 0, FileSize());
        }

        // 写文件
        bool SetContent(const char *content, size_t len)
        {
            std::ofstream ofs;
            ofs.open(filename_, std::ios::binary | std::ios::trunc);
            if (!ofs.is_open())
            {
                mylog::GetLogger("asynclogger")->Info("%s open error: %s", filename_.c_str(), strerror(errno));
                return false;
            }

            ofs.write(content, len);
            if (!ofs.good())
            {
                mylog::GetLogger("asynclogger")->Info("%s, file set content error", filename_.c_str());
                return false;
            }
            ofs.close();
            return true;
        }

        // 压缩文件
        bool Compress(const std::string &content, int format)
        {
            std::string packed = bundle::pack(format, content);
            if (packed.size() == 0)
            {
                mylog::GetLogger("asynclogger")->Info("filename: %s, Compress SetContent error", filename_.c_str());
                return false;
            }

            FileUtil f(filename_);
            if (f.SetContent(packed.c_str(), packed.size()) == false)
            {
                mylog::GetLogger("asynclogger")->Info("filename: %s, Compress SetContent error", filename_.c_str());
                return false;
            }
            return true;
        }

        // 解压文件
        bool UnCompress(std::string &uncompress_path)
        {
            std::string body;
            if (!GetContent(&body))
            {
                mylog::GetLogger("asynclogger")->Info("filename: %s, uncompress get file content failed", filename_.c_str());
                return false;
            }
            auto unpack = bundle::unpack(body);

            FileUtil f(uncompress_path);
            if (f.SetContent(unpack.c_str(), unpack.size()) == false)
            {
                mylog::GetLogger("asynclogger")->Info("filename: %s, UnCompress write unpacked data failed error", filename_.c_str());
                return false;
            }
            return true;
        }

        bool Exists()
        {
            return fs::exists(filename_);
        }

        bool CreateDirectory()
        {
            // 若filename_指定的路径存在，使用create_directories会返回false，
            // 所以这里需要手动检查并返回true
            if (Exists()) return true; 

            return fs::create_directories(filename_);
        }

        // 扫描指定目录中的普通文件并返回其相对路径
        bool ScanDirectory(std::vector<std::string> *arry)
        {
            for (auto &p : fs::directory_iterator(filename_))
            {
                if (p.is_directory()) continue;
                arry->push_back(p.path().relative_path().string());
            }
            return true;
        }
    }; // class FileUtil

    class JsonUtil{
        public:
            static bool Serialize(const Json::Value &val, std::string *str)
            {
                Json::StreamWriterBuilder swb;
                swb["emitUTF8"] = true; // 直接输出UTF-8字符，不进行转义
                std::unique_ptr<Json::StreamWriter> usw(swb.newStreamWriter());
                std::stringstream ss;
                if (usw->write(val, &ss) != 0)
                {
                    mylog::GetLogger("asynclogger")->Info("json value serialize error");
                    return false;
                }
                *str = ss.str();
                return true;
            }

            static bool UnSerialize(const std::string &str, Json::Value *val)
            {
                Json::CharReaderBuilder crb;
                std::unique_ptr<Json::CharReader> ucr(crb.newCharReader());
                std::string err;
                if (ucr->parse(str.data(), str.data() + str.size(), val, &err) == false)
                {
                    mylog::GetLogger("asynclogger")->Info("json string unserialize error");
                    return false;
                }
                return true;
            }
    }; // class JsonUtil
}