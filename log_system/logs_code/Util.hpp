#pragma once
#include <jsoncpp/json/json.h>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

using std::cout;
using std::endl;

namespace mylog{
    namespace Util{
        class Date{
        public:
            static time_t Now() {return time(nullptr);}
        };

        class File{
        public:
            File() = delete;
            // 检查文件是否存在，存在返回1，否则返回0
            static bool Exists(const std::string& filename)
            {
                struct stat st;
                return (0 == stat(filename.c_str(), &st));
            }
            // 返回文件所在路径
            static std::string Path(const std::string &filename)
            {
                if (filename.empty()) return "";
                int pos = filename.find_last_of("/\\"); // 找到最后一个"/"或"\"
                if (pos != std::string::npos) return filename.substr(0, pos + 1);
                return "";
            }

            static void CreateDirectory(const std::string &pathname)
            {
                if (pathname.empty()) perror("所给文件路径为空：");

                if (!Exists(pathname))
                {
                    size_t pos, index = 0;
                    size_t size = pathname.size();
                    while (index < size)
                    {
                        pos = pathname.find_first_of("/\\", index);
                        // 直接在当前目录下创建目录
                        if (pos == std::string::npos) 
                        {
                            mkdir(pathname.c_str(), 0755);
                            return;
                        }

                        if (pos == index)
                        {
                            index = pos + 1;
                            continue;
                        }

                        std::string sub_path = pathname.substr(0, pos);

                        if (Exists(sub_path))
                        {
                            index = pos + 1;
                            continue;
                        }

                        mkdir(sub_path.c_str(), 0755);
                        index = pos + 1;
                    }
                }
            }
            
           static int64_t FileSize(const std::string &filename)
           {
                struct stat st;
                int ret = stat(filename.c_str(), &st);
                if (ret == -1)
                {
                    perror("[stat error]Get file size failed");
                    return -1;
                }
                return st.st_size;
           }
           // 获取文件内容
           static bool GetContent(std::string *content, const std::string &filename)
           {
                std::ifstream ifs;
                ifs.open(filename, std::ios::binary);
                if (!ifs.is_open())
                {
                    std::cout << "file open error" << std::endl;
                    return false;
                }

                ifs.seekg(0, std::ios::beg); // 将读指针定位到文件开始位置
                size_t len = FileSize(filename);
                content->resize(len);
                ifs.read(&(*content)[0], len); // 将最大为指定数量字节的文件内容读取到content中。
                if (!ifs.good())
                {
                    std::cerr << __FILE__ << " " << __LINE__ << " -"
                              << " read file content error" << std::endl;
                    ifs.close();
                    return false;
                }
                ifs.close();
                return true;
           }
        };// class File

        class JsonUtil{
        public:
            JsonUtil() = delete;
            // 将json数据结构序列化为字符串
            static bool Serialize(const Json::Value &val, std::string *str)
            {
                Json::StreamWriterBuilder swb;
                std::unique_ptr<Json::StreamWriter> usw(swb.newStreamWriter());
                std::stringstream ss;
                if (usw->write(val, &ss) != 0)
                {
                    std::cerr << "serialize error" << std::endl;
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
                if (ucr->parse(str.c_str(), str.c_str() + str.size(), val, &err) == false)
                {
                    std::cerr << __FILE__ << " " << __LINE__ << " parse error " << err << std::endl;
                    return false;
                }
                return true;
            }
        }; // class JsonUtil

        // 读取config.conf中的配置信息并存储在JsonData实例中
        struct JsonData{
            // 只能有一个JsonData实例，用户无法从类的外部构建实例
            static JsonData& GetJsonData()
            {
                static JsonData json_data;
                return json_data;
            }

            // 确保单例
            JsonData(const JsonData&) = delete;
            JsonData(const JsonData&&) = delete;
            JsonData& operator=(JsonData&) = delete;
            JsonData& operator=(JsonData&&) = delete;
        public:
            size_t buffer_size; // 缓冲区基础容量
            size_t threadhold; // 倍数扩容阈值
            size_t linear_growth; // 线性增长容量
            size_t flush_log; // 控制日志同步到磁盘的时机, 1调用fflush, 2调用fsync
            std::string backup_addr;
            uint16_t backup_port;
            size_t thread_count;
        private:
            JsonData()
            {
                std::string content;

#ifdef LOG_DEBUG
                if (File::Exists("./config.conf") == false)
                {
                    std::cerr << "config.conf not exists" << std::endl;
                    return;
                }
                if (File::GetContent(&content, "./config.conf") == false)
                {
                    std::cerr << __FILE__ << " " << __LINE__ << " open config.conf failed: ";
                    perror(nullptr);
                    return;
                }
#else
                if (File::Exists("../log_system/logs_code/config.conf") == false)
                {
                    std::cerr << "config.conf not exists" << std::endl;
                    return;
                }
                if (File::GetContent(&content, "../log_system/logs_code/config.conf") == false)
                {
                    std::cerr << __FILE__ << " " << __LINE__ << " open config.conf failed: ";
                    perror(nullptr);
                    return;
                }
#endif
                Json::Value root;
                JsonUtil::UnSerialize(content, &root);
                buffer_size = root["buffer_size"].asInt64();
                threadhold = root["threadhold"].asInt64();
                linear_growth = root["linear_growth"].asInt64();
                flush_log = root["flush_log"].asInt64();
                backup_addr = root["backup_addr"].asString();
                backup_port = root["backup_port"].asInt();
                thread_count = root["thread_count"].asInt();
            }
            ~JsonData() = default;
        };// class JsonData
    }// Util
}// mylog