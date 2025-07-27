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
        static Config& GetConfigData()
        {
            static Config config_data;
            return config_data;
        }
        bool ReadConfig()
        {
            FileUtil fu(Config_File);
            string content;
            if (!fu.GetContent(&content)) return false;

            Json::Value val;
            if(!JsonUtil::UnSerialize(content, &val)) return false;

            server_port_ = val["server_port"].asInt();
            server_ip_ = val["server_ip"].asString();
            download_prefix_ = val["download_prefix"].asString();
            deep_storage_dir_ = val["deep_storage_dir"].asString();
            low_storage_dir_ = val["low_storage_dir"].asString();
            storage_info_file_ = val["storage_info_file"].asString();
            bundle_format_ = val["bundle_format_"].asInt();
            return true;
        }

        int GetServerPort(){ return server_port_; }

        string GetServerIP(){ return server_ip_; }

        string GetDownLoadPrefix() { return download_prefix_; }

        string GetDeepStorageDir() { return deep_storage_dir_; }

        string GetLowStorageDirt() { return low_storage_dir_; }

        string GetStorageInfoFile() { return storage_info_file_; }

        int GetBundleFormat() { return bundle_format_; }

        // 确保单例
        Config(const Config&) = delete;
        Config(const Config&&) = delete;
        Config& operator=(Config&) = delete;
        Config& operator=(Config&&) = delete;

    private:
        Config()
        {
            if (ReadConfig())
            {
                mylog::GetLogger("asynclogger")->Fatal("ReadConfig failed");
            }
            mylog::GetLogger("asynclogger")->Info("Get configure information successfully");
        }
        ~Config() = default;
        
    private:
        int server_port_;
        string server_ip_;
        string download_prefix_;
        string deep_storage_dir_;
        string low_storage_dir_;
        string storage_info_file_;       // 记录已存储文件信息的文件的路径
        int bundle_format_;
    }; // class Config
}
