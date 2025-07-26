#pragma once
#include <memory>
#include <mutex>
#include "Util.hpp"

using std::string;

namespace storage{
    const char* Config_File = "Storage.conf";
    
    // 用于读取云存储系统的配置文件信息
    class Config{
    public:
        bool ReadConfig(){}


    private:
        Config()
        {
            if (ReadConfig())
            {
                mylog::GetLogger("asynclogger")->Fatal("ReadConfig failed");
            }
        }

    private:
        int server_port_;
        string server_ip_;
        string download_prefix_;
        string deep_storage_dir_;
        string low_storage_dir_;
        string storage_info_;
        int bundle_format_;

        static std::mutex mtx_;
        static Config *instance_;
    }; // class Config
}
