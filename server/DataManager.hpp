#pragma once
#include <pthread.h>
#include <unordered_map>
#include <unordered_set>
#include <ctime>
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
#ifdef DEBUG_LOG
            mylog::GetLogger("asynclogger")->DEBUG("data information insert start");
#endif
            pthread_rwlock_wrlock(&rwlock_);
            table_[info.url_] = info;
            pthread_rwlock_unlock(&rwlock_);
            
            // 在初始化阶段need_persist为false，此时不需要将table写入文件
            if (need_persist_ && !Storage())
            {
                mylog::GetLogger("asynclogger")->Error("data information Storage after Insert error");
                return false;
            }
#ifdef DEBUG_LOG
            mylog::GetLogger("asynclogger")->DEBUG("data information insert completed");
#endif
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

        // 删除云端文件后要从table_中删除对应的文件信息并更新storage文件
        void Remove(const string &url)
        {
            pthread_rwlock_wrlock(&rwlock_);
            table_.erase(url);
            pthread_rwlock_unlock(&rwlock_);
            if (!Storage()) // 更新失败，程序能够正常运行，但是用户在浏览器中看到的文件列表可能过期
                mylog::GetLogger("asynclogger")->Warn("Update %s failed, files list may expired", Config::GetConfigData().GetStorageInfoFile());
        }

        // 更新文件信息，同时查看文件是否真实存在，若不存在需要删除该文件在table_中的信息并更新storage文件
        void Update()
        {
            pthread_rwlock_wrlock(&rwlock_);
            size_t old_size = table_.size();
            for (auto it = table_.begin(); it != table_.end(); ) 
            {
                FileUtil fu(it->second.storage_path_);
                
                if (!fu.Exists()) 
                {
                    it = table_.erase(it);
                }
                else
                {
                    it->second.mtime_ = fu.LastMidifyTime();
                    it->second.atime_ = fu.LastAccessTime();
                    it->second.fsize_ = fu.FileSize();
                    ++it;
                }
            }   
            pthread_rwlock_unlock(&rwlock_); 
            if (!Storage()) // 更新失败，程序能够正常运行，但是用户在浏览器中看到的文件列表可能过期
                mylog::GetLogger("asynclogger")->Warn("Update %s failed, files list may expired", Config::GetConfigData().GetStorageInfoFile());
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
            // 更新文件信息
            Update();
 
            mylog::GetLogger("asynclogger")->Info("init data manager completed");
            return true;
        }

    private:
        string storage_info_file_;
        pthread_rwlock_t rwlock_;
        std::unordered_map<string, StorageInfo> table_;
        bool need_persist_;                                 // 用于避免在初始化的使用调用Insert重复将刚读取到的信息写入文件中
    }; // class DataManager

    class LoginManager{
    public:
        static LoginManager& GetLoginManager()
        {
            static LoginManager login_manager;
            return login_manager;
        }

        const std::unordered_map<string, time_t> GetAllIp() 
        { 
            pthread_rwlock_rdlock(&rwlock_);
            auto ret = ip_register_;
            pthread_rwlock_unlock(&rwlock_);
            return ip_register_;
        }

        bool CheckLoggedIn(const string &ip) 
        { 
            pthread_rwlock_rdlock(&rwlock_);
            bool ret = ip_register_.find(ip) != ip_register_.end();
            pthread_rwlock_unlock(&rwlock_);
            return ret;
        }

        void Login(const string &ip)
        {
            pthread_rwlock_wrlock(&rwlock_);
            ip_register_[ip] = time(nullptr);
            pthread_rwlock_unlock(&rwlock_);
        }

        void LogOut(const string &ip)
        {
            pthread_rwlock_wrlock(&rwlock_);
            ip_register_.erase(ip);
            pthread_rwlock_unlock(&rwlock_);
        }

        void UpdateLoginTime(const string ip)
        {
            if (ip_register_.find(ip) == ip_register_.end()) return;
            pthread_rwlock_wrlock(&rwlock_);
            ip_register_[ip] = time(nullptr);
            pthread_rwlock_unlock(&rwlock_);
        }

        // 清理掉最后登录时间超过一天的IP信息
        void UpdateRegister()
        {
            time_t now = time(nullptr);
            for (auto it = ip_register_.begin(); it != ip_register_.end();)
            {
                if ((now - it->second) > 60 * 60 *24) ip_register_.erase(it->first);
                else it++;
            }
        }
    private:
        LoginManager(){ pthread_rwlock_init(&rwlock_, nullptr); }
        ~LoginManager() { pthread_rwlock_destroy(&rwlock_); }
    private:
        std::unordered_map<string, time_t> ip_register_;    // 用于记录服务器启动以来登录过的IP地址及其登录时间
        pthread_rwlock_t rwlock_;
    };
}