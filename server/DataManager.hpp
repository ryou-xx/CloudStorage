#pragma once
#include <pthread.h>
#include <unordered_map>
#include "Config.hpp"

namespace storage{
    // 存储文件的属性信息
    class StorageInfo{
    public:
        bool NewStorageInfo(const string &storage_path)
        {
            mylog::GetLogger("asynclogger")->Info("NewStorageInfo start");
            FileUtil fu(storage_path);
            if (!fu.Exists())
            {
                mylog::GetLogger("asynclogger")->Info("file not exists");
                return false;
            }

            mtime_ = fu.LastMidifyTime();
            atime_ = fu.LastAccessTime();
            fsize_ = fu.FileSize();
            storage_path_ = storage_path;
            url_ = Config::GetConfigData().GetDownLoadPrefix() + fu.FileName();
            mylog::GetLogger("asynclogger")->Info(
                "download_url: %s, mtime: %s, atime: %s, fsize: %d",
                 url_.c_str(), ctime(&mtime_), ctime(&atime_), fsize_);
            mylog::GetLogger("asynclogger")->Info("NewStorageInfo end");
            return true;
        }
    public:
        time_t mtime_;          // 文件修改时间
        time_t atime_;          // 文件访问时间
        size_t fsize_;          // 文件大小
        string storage_path_;   // 文件存储路径
        string url_;            // 请求URL中的资源路径
    }; // class StorageInfo

    class DataManager{
    public:
        static DataManager& GetDataManager()
        {
            static DataManager data_manager;
            return data_manager;
        }
    
        // 每次有信息改变需要持久化存储一次，即把table_中的数据转成json格式存入文件中
        bool Storage()
        {
            mylog::GetLogger("asynclogger")->Info("file information storage start");
            std::vector<StorageInfo> vec;
            GetAll(vec);

            Json::Value root;
            for (auto e : vec)
            {
                Json::Value item;
                item["mtime_"] = (Json::Int64)e.mtime_;
                item["atime_"] = (Json::Int64)e.atime_;
                item["fsize_"] = (Json::Int64)e.fsize_;
                item["storage_path_"] = e.storage_path_.c_str();
                item["url_"] = e.url_.c_str();
                root.append(item);
            }

            string body;
            if (!JsonUtil::Serialize(root, &body))
            { 
                mylog::GetLogger("asynclogger")->Error("file information storage error");
                return false;
            }
            mylog::GetLogger("asynclogger")->Info("all file information: %s", body.c_str());

            FileUtil fu(storage_info_file_);
            if (!fu.SetContent(body.c_str(), body.size()))
            {
                mylog::GetLogger("asynclogger")->Error("file information storage error");
                return false;
            }

            mylog::GetLogger("asynclogger")->Info("file information storage end");
            return true;
        }
        
        bool Insert(const StorageInfo &info)
        {
            mylog::GetLogger("asynclogger")->Info("data information insert start");
            pthread_rwlock_wrlock(&rwlock_);
            table_[info.url_] = info;
            pthread_rwlock_unlock(&rwlock_);
            
            // 在初始化阶段need_persist为false，此时不需要将table写入文件
            if (need_persist_ && !Storage())
            {
                mylog::GetLogger("asynclogger")->Error("data information Storage after Insert error");
                return false;
            }

            mylog::GetLogger("asynclogger")->Info("data information insert completed");
            return true;
        }

        void SetPersist(bool flag)
        {
            pthread_rwlock_wrlock(&rwlock_);
            need_persist_ = flag;
            pthread_rwlock_unlock(&rwlock_);
            mylog::GetLogger("asynclogger")->Info("set need_persist to %s", (flag ? "true" : "false"));
        }

        bool GetOneByURL(const string &url, StorageInfo *info)
        {
            pthread_rwlock_rdlock(&rwlock_);
            if (table_.find(url) == table_.end())
            {
                pthread_rwlock_unlock(&rwlock_);
                mylog::GetLogger("asynclogger")->Error("can not find URL: %s", url.c_str());
                return false;
            }
            *info = table_[url];
            pthread_rwlock_unlock(&rwlock_);
            mylog::GetLogger("asynclogger")->Info("URL: %s, get information successfully", url.c_str());
            return true;
        }

        bool GetOneByStoragePath(const string &path, StorageInfo *info)
        {
            pthread_rwlock_rdlock(&rwlock_);
            for (auto e : table_)
            {
                if (e.second.storage_path_ == path)
                {
                    *info = e.second;
                    pthread_rwlock_unlock(&rwlock_);
                    mylog::GetLogger("asynclogger")->Info("Path: %s, get information successfully", path.c_str());
                    return true; 
                }
            }
            pthread_rwlock_unlock(&rwlock_);
            mylog::GetLogger("asynclogger")->Error("can not find path: %s", path.c_str());
            return false;
        }

        bool GetAll(std::vector<StorageInfo> &vec)
        {
            pthread_rwlock_rdlock(&rwlock_);
            for (auto e : table_)
            {
                vec.push_back(e.second);
            }
            pthread_rwlock_unlock(&rwlock_);
            return true;
        }

        // 确保单例
        DataManager(const DataManager&) = delete;
        DataManager(const DataManager&&) = delete;
        DataManager& operator=(DataManager&) = delete;
        DataManager& operator=(DataManager&&) = delete;
    private:
        DataManager()
        {
            mylog::GetLogger("asynclogger")->Info("DataManager construct start");
            // 获取已存储的文件的信息
            storage_info_file_ = Config::GetConfigData().GetStorageInfoFile();
            pthread_rwlock_init(&rwlock_, nullptr);
            need_persist_ = false;
            InitLoad();
            need_persist_ = true;
            mylog::GetLogger("asynclogger")->Info("DataManager construct end");
        }
        ~DataManager() { pthread_rwlock_destroy(&rwlock_); }

        bool InitLoad()
        {
            mylog::GetLogger("asynclogger")->Info("init data manager start");
            FileUtil fu(storage_info_file_);
            if (!fu.Exists())
            {
                mylog::GetLogger("asynclogger")->Info("there is no storage file info need to load,\
                     init data manager completed");
                return true;
            }

            string body;
            if (!fu.GetContent(&body)) 
            {
                mylog::GetLogger("asynclogger")->Warn("init data manager failed");
                return false;
            }

            Json::Value val;
            if (!JsonUtil::UnSerialize(body, &val)) 
            {
                mylog::GetLogger("asynclogger")->Warn("init data manager failed");
                return false;
            }

            for (int i = 0; i < val.size(); i++)
            {
                StorageInfo info;
                info.atime_ = val[i]["atime_"].asInt();
                info.mtime_ = val[i]["mtime_"].asInt();
                info.fsize_ = val[i]["fsize_"].asInt();
                info.storage_path_ = val[i]["storage_path_"].asString();
                info.url_ = val[i]["url_"].asString();
                Insert(info);
            }
            mylog::GetLogger("asynclogger")->Info("init data manager completed");
            return true;
        }

    private:
        string storage_info_file_;
        pthread_rwlock_t rwlock_;
        std::unordered_map<string, StorageInfo> table_;
        bool need_persist_;                                 // 用于避免在初始化的使用调用Insert重复将刚读取到的信息写入文件中
    }; // class DataManager
}