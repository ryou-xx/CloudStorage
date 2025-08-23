#pragma once
#include <cassert>
#include <sstream>
#include <memory>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <zlib.h>
#include <zconf.h>
#include <fcntl.h>
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
        {
            std::cerr << "Hex string format error" << std::endl;
            assert(0);
        }
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

        bool PreAllocate(size_t size)
        {
            if (Exists()) remove(filename_.c_str()); // 如果已存在，先删除
            // 创建文件
            int fd = open(filename_.c_str(), O_RDWR | O_CREAT, 0644);
            if (fd == -1) {
                mylog::GetLogger("asynclogger")->Error("PreAllocate: Failed to open file %s: %s", filename_.c_str(), strerror(errno));
                return false;
            }

            // 使用fallocate预分配文件大小
            if (fallocate(fd, 0, 0, size) != 0) {
                mylog::GetLogger("asynclogger")->Error("PreAllocate: fallocate failed for %s: %s", filename_.c_str(), strerror(errno));
                close(fd);
                return false;
            }

            close(fd);
            return true;
        }

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

        // 流压缩文件
        bool Compress(const std::string sorce)
        {
            const size_t CHUNK_SIZE_ZLIB = 16384;

            int ret;
            z_stream strm;
            unsigned char in[CHUNK_SIZE_ZLIB];
            unsigned char out[CHUNK_SIZE_ZLIB];

            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
            strm.opaque = Z_NULL;

            // 初始化Zlib流
            ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
            if (ret != Z_OK)
            {
                mylog::GetLogger("asynclogger")->Error("z_stream init error");
                return false;
            }

            std::ofstream final_file(filename_, std::ios::binary);
            if (!final_file.is_open())
            {
                mylog::GetLogger("asynclogger")->Error("open file %s error", filename_.c_str());
                return false;
            }

            std::ifstream sorce_file(sorce, std::ios::binary);
            int flush = Z_NO_FLUSH;
            // 持续从源文件中读取数据送入压缩器
            do{
                sorce_file.read((char*)in, CHUNK_SIZE_ZLIB);
                strm.avail_in = sorce_file.gcount(); // 获取读取的字节数
                strm.next_in = in; // next_in为下一个输入字节的地址
                if (strm.avail_in == 0)  flush = Z_FINISH;

                // 处理当前数据块
                do{
                    strm.avail_out = CHUNK_SIZE_ZLIB;
                    strm.next_out = out;

                    // 执行压缩
                    ret = deflate(&strm, flush);
                    if (ret == Z_STREAM_ERROR)
                    {
                        deflateEnd(&strm);
                        mylog::GetLogger("asynclogger")->Error("deflate error");
                        return false;
                    }

                    int have = CHUNK_SIZE_ZLIB - strm.avail_out; // 压缩的字节数
                    if (final_file.write((char* )out, have).fail())
                    {
                        deflateEnd(&strm); // 释放资源
                        mylog::GetLogger("asynclogger")->Error("final file %s write error", filename_.c_str());
                        return false;
                    }
                }while(strm.avail_out == 0);

            }while(flush != Z_FINISH);  

            deflateEnd(&strm);
            final_file.close();
            return true;
        }

        // 流解压文件
        bool UnCompress(std::string &uncompress_path)
        {
            const size_t CHUNK_SIZE_ZLIB = 16384;
            std::ifstream source_file(filename_, std::ios::binary);
            if (!source_file.is_open())
            {
                mylog::GetLogger("asynclogger")->Error("Uncompress: Can not Open source file %s", filename_);
                return false;
            }

            std::ofstream dest_file(uncompress_path, std::ios::binary);
            if (!dest_file.is_open())
            {
                mylog::GetLogger("asynclogger")->Error("Uncompress: Can not open destination file %s", uncompress_path);
                return false;
            }

            int ret;
            z_stream strm;
            unsigned char in[CHUNK_SIZE_ZLIB];
            unsigned char out[CHUNK_SIZE_ZLIB];

            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
            strm.opaque = Z_NULL;
            strm.avail_in = 0;
            strm.next_in = Z_NULL;

            // 初始化zlib流
            ret = inflateInit(&strm);
            if (ret != Z_OK)
            {
                mylog::GetLogger("asynclogger")->Error("Uncompress: inflateInit failed");
                dest_file.close();
                remove(uncompress_path.c_str());
                return false;
            }

            // 从源文件读取数据并解压
            do{
                source_file.read((char*)in, CHUNK_SIZE_ZLIB);
                strm.avail_in = source_file.gcount();
                if (strm.avail_in == 0) break; // 读取完毕
                strm.next_in = in;

                // 处理当前输入缓冲区的数据
                do{
                    strm.avail_out = CHUNK_SIZE_ZLIB;
                    strm.next_out = out;
                    ret = inflate(&strm, Z_NO_FLUSH);
                    if (ret == Z_STREAM_ERROR)
                    {
                        mylog::GetLogger("asynclogger")->Error("Uncompress: inflate stream error.");
                        inflateEnd(&strm);
                        dest_file.close();
                        remove(uncompress_path.c_str());
                        return false;
                    }

                    switch (ret)
                    {
                        case Z_NEED_DICT:
                        case Z_DATA_ERROR:
                        case Z_MEM_ERROR:
                            mylog::GetLogger("asynclogger")->Error("Uncompress: inflate error %d", ret);
                            inflateEnd(&strm);
                            dest_file.close();
                            remove(uncompress_path.c_str());
                            return false;
                    }

                    int have = CHUNK_SIZE_ZLIB - strm.avail_out;
                    if (dest_file.write((char*)out, have).fail())
                    {
                        inflateEnd(&strm);
                        mylog::GetLogger("asynclogger")->Error("Uncompress: write to destination file failed.");
                        dest_file.close();
                        remove(uncompress_path.c_str());
                        return false;
                    }

                }while(strm.avail_out == 0);

            }while(ret != Z_STREAM_END);

            inflateEnd(&strm);

            return (ret == Z_STREAM_END);
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

        // 删除整个文件夹内容
        bool RemoveDirectory()
        {
            if (!Exists()) return true;

            try{
                fs::remove_all(filename_);
                return true;
            }
            catch(const std::exception& e){
                mylog::GetLogger("asynclogger")->Error("Remove directory: %s error: %s", filename_.c_str(), e.what());
                return false;
            }
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